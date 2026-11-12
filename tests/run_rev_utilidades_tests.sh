#!/bin/bash
# Revisión de las utilidades que escriben en disco:
#  MOVER    un archivo sobre sí mismo no se borra; destino «X:\» y «carpeta\»
#  XCOPIAR  respuesta F (como XCOPY) y fin de la entrada redirigida (no se
#           queda preguntando sin fin)
#  SISTEMA  «SYS origen: destino:» como en MS-DOS (antes escribía en el origen)
#  PARTIR   la partición nueva no hereda el sector de arranque de la anterior;
#           /PRI:n con n enorme; discos sin tabla de particiones
#  FORMATEA el tamaño es el de la partición, no el del BPB que hubiera;
#           disquetes de 360 KB
#  INSTALAR avisa si no cabe todo; «Fecha:» no es la unidad A:
# Review test of the disk-writing utilities.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "no debía salir: $2"; else ok "no sale: $2"; fi; }
checkx() { if grep -qxF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
part() { awk -v m="=$2=" '$0==m{p=1;next} /^=M[0-9]+=$/{p=0} p' "$1"; }
# run LOG B.IMG C.IMG ESPERA órdenes...
run() {
  local log=$1 b=$2 c=$3 w=$4; shift 4
  local drv=(-drive file=$T/f.img,format=raw,if=floppy,index=0)
  [ -n "$b" ] && drv+=(-drive file=$b,format=raw,if=floppy,index=1)
  [ -n "$c" ] && drv+=(-drive file=$c,format=raw,if=ide,index=0)
  { sleep 6; for x in "$@"; do printf '%s\r' "$x"; sleep "$w"; done; } | \
    timeout $(( 8 + $# * ${w%.*} )) qemu-system-i386 -display none -serial stdio -monitor none \
      -parallel none "${drv[@]}" -boot a -no-reboot > $log.raw 2>&1
  tr -d '\r' < $log.raw | iconv -f CP437 -t UTF-8 > $log
}
# tabla de particiones: «n tipo inicio tamaño total_del_BPB» de cada una
ptable() {
  python3 - $1 <<'EOF'
import struct, sys
f = open(sys.argv[1], 'rb'); m = f.read(512)
for i in range(4):
    e = m[446 + 16 * i: 462 + 16 * i]
    st, sz = struct.unpack_from('<II', e, 8)
    if e[4]:
        f.seek(st * 512); b = f.read(512)
        tot = struct.unpack_from('<H', b, 0x13)[0] or struct.unpack_from('<I', b, 0x20)[0]
        if struct.unpack_from('<H', b, 0x0B)[0] != 512: tot = 0
        print(i + 1, '%02X' % e[4], st, sz, tot)
EOF
}
fsck_ok() { if fsck.fat -n $1 >/dev/null 2>&1; then ok "fsck.fat -n $2"; else ko "fsck.fat -n $2"; fsck.fat -n $1 | head; fi; }

cp "$IMG" $T/f.img
printf 'uno\r\n' > $T/UNO.TXT
printf 'dos\r\n' > $T/DOS.TXT
: > $T/VACIO.TXT
head -c 3000 /dev/urandom > $T/DATOS.BIN

echo "== Hispa-DOS: revisión de las utilidades de disco =="
# ------------------------------------------------------------------ MOVER, XCOPIAR, SISTEMA
echo "-- MOVER, XCOPIAR y SISTEMA en un disquete --"
B=$T/b.img
dd if=/dev/zero of=$B bs=1k count=1440 status=none; mformat -i $B -f 1440 ::
mcopy -i $B $T/UNO.TXT $T/DOS.TXT $T/VACIO.TXT ::
mmd -i $B ::DIR1 ::DIR2 ::ORIG
mcopy -i $B $T/DATOS.BIN ::DIR1/X.BIN
mcopy -i $B $T/DATOS.BIN ::DIR1/Y.BIN
mcopy -i $B $T/UNO.TXT ::ORIG/A.TXT
run $T/log1 $B "" 3 "ECHO =M1=" "MOVER /Y B:\\UNO.TXT B:\\UNO.TXT" "IF ERRORLEVEL 1 ECHO nivel-mismo" \
    "MOVER /Y B:\\DIR1\\Y.BIN B:\\DIR1" "B:" "MOVER DOS.TXT DOS.TXT" "S" "A:" \
    "ECHO =M2=" "MOVER B:\\DIR1\\X.BIN B:\\" "MOVER B:\\DOS.TXT B:\\DIR2\\" \
    "ECHO =M3=" "ECHO F| XCOPIAR B:\\ORIG\\A.TXT B:\\NUEVO.TXT" \
    "XCOPIAR B:\\ORIG\\A.TXT B:\\OTRO.TXT < B:\\VACIO.TXT" "IF ERRORLEVEL 2 ECHO nivel-fin" \
    "ECHO =M4=" "SISTEMA A: B:" "ECHO =M5="
part $T/log1 M1 > $T/s1
part $T/log1 M2 > $T/s2
part $T/log1 M3 > $T/s3
part $T/log1 M4 > $T/s4
check $T/s1 "es el mismo archivo: no se mueve"
checkx $T/s1 "nivel-mismo"
if mtype -i $B ::UNO.TXT 2>/dev/null | grep -q uno; then ok "MOVER /Y UNO.TXT UNO.TXT: el archivo sigue ahí"; else ko "MOVER sobre sí mismo ha borrado UNO.TXT"; fi
if mtype -i $B ::DIR1/Y.BIN 2>/dev/null | cmp -s - $T/DATOS.BIN; then ok "MOVER /Y a su propia carpeta: el archivo sigue ahí"; else ko "MOVER a su propia carpeta ha borrado Y.BIN"; fi
check $T/s2 "B:\\DIR1\\X.BIN => B:\\X.BIN [correcto]"
if mtype -i $B ::X.BIN 2>/dev/null | cmp -s - $T/DATOS.BIN; then ok "MOVER al raíz (B:\\)"; else ko "MOVER al raíz (B:\\)"; fi
if mtype -i $B ::DIR2/DOS.TXT 2>/dev/null | grep -q dos; then ok "MOVER a «carpeta\\» (DOS.TXT sobrevivió a moverse sobre sí mismo)"; else ko "MOVER a «carpeta\\»"; fi
if mtype -i $B ::NUEVO.TXT 2>/dev/null | grep -q uno; then ok "XCOPIAR: la respuesta F vale por «archivo»"; else ko "XCOPIAR con ECHO F|"; fi
n=$(grep -c "un archivo o un directorio" $T/s3)
[ "$n" -le 3 ] && ok "XCOPIAR no pregunta sin fin cuando se acaba la entrada ($n veces)" || ko "XCOPIAR preguntó $n veces"
checkx $T/s3 "nivel-fin"
check $T/s4 "Sistema transferido a la unidad B:"
if mtype -i $B ::KERNEL.SYS 2>/dev/null | cmp -s - build/KERNEL.SYS; then ok "SISTEMA A: B: escribe en B: (el destino es el segundo)"; else ko "SISTEMA A: B: no ha copiado KERNEL.SYS a B:"; fi
fsck_ok $B "disquete"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log1 | head -150

# ------------------------------------------------------------------ PARTIR + FORMATEA
echo "-- PARTIR: una partición nueva más pequeña en el sitio de otra --"
C=$T/c.img
dd if=/dev/zero of=$C bs=1M count=64 status=none
run $T/log2 "" $C 3 "PARTIR /PRI:40" "FORMATEA C: /AUTOTEST" "COPY A:\\README.TXT C:\\VIEJO.TXT" \
    "PARTIR /DEL:1" "PARTIR /PRI:10" "ECHO =M1=" "DIR C:" "ECHO =M2=" "FORMATEA C: /AUTOTEST" \
    "PARTIR /PRI:10" "FORMATEA D: /AUTOTEST" "COPY A:\\README.TXT D:\\D.TXT" \
    "COPY A:\\KERNEL.SYS C:\\" "PARTIR /PRI:2097155" "ECHO =M3="
part $T/log2 M1 > $T/s1
checkn $T/s1 "VIEJO"
ptable $C > $T/pt
read -r _ _ st1 sz1 tot1 < <(sed -n 1p $T/pt)
read -r _ _ st2 sz2 tot2 < <(sed -n 2p $T/pt)
read -r _ _ st3 sz3 tot3 < <(sed -n 3p $T/pt)
[ "${sz1:-0}" = 20480 ] && [ "${tot1:-0}" = 20480 ] && ok "C: de 10 MB con un sistema de archivos de 10 MB" || ko "C: partición de ${sz1:-?} sectores, BPB de ${tot1:-?}"
if [ -n "${st2:-}" ] && mtype -i $C@@$((st2 * 512)) ::D.TXT 2>/dev/null | cmp -s - tests/disk/README.TXT; then ok "D: intacta"; else ko "D:"; fi
if [ -n "${st1:-}" ] && mtype -i $C@@$((st1 * 512)) ::KERNEL.SYS 2>/dev/null | cmp -s - build/KERNEL.SYS; then ok "C: se puede usar"; else ko "C:"; fi
[ "${sz3:-0}" -gt 60000 ] && ok "/PRI:2097155 usa el espacio que queda (${sz3} sectores)" || ko "/PRI:2097155 creó una partición de ${sz3:-?} sectores"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && { cat $T/pt; cat $T/log2; }

echo "-- FORMATEA: BPB antiguo mayor que la partición --"
# C: de 10 MB con el arranque de un sistema de archivos de 40 MB, y D: detrás
G=$T/g.img
dd if=/dev/zero of=$G bs=1M count=64 status=none
mformat -i $G@@32256 -T 81920 -h 16 -s 63 -H 63 ::
python3 - $G <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
e1 = bytes([0x80, 1, 1, 0, 0x04, 15, 63, 20]) + struct.pack('<II', 63, 20480)
e2 = bytes([0x00, 0, 1, 21, 0x04, 15, 63, 41]) + struct.pack('<II', 21168, 20480)
f.write(bytes(446) + e1 + e2 + bytes(32) + b'\x55\xAA')
EOF
mformat -i $G@@$((21168 * 512)) -T 20480 -h 16 -s 63 -H 21168 ::
mcopy -i $G@@$((21168 * 512)) $T/DATOS.BIN ::DATOS.BIN
run $T/log3 "" $G 4 "FORMATEA C: /AUTOTEST" "COPY A:\\KERNEL.SYS C:\\"
ptable $G > $T/pt
read -r _ _ st1 sz1 tot1 < <(sed -n 1p $T/pt)
[ "${tot1:-0}" = 20480 ] && ok "FORMATEA usa el tamaño de la partición (20.480 sectores)" || ko "FORMATEA: BPB de ${tot1:-?} sectores en una partición de 20.480"
check $T/log3 "Formateando 10.00M"
dd if=$G of=$T/p.img bs=512 skip=63 count=20480 status=none
fsck_ok $T/p.img "C: recién formateada"
if mtype -i $G@@$((21168 * 512)) ::DATOS.BIN 2>/dev/null | cmp -s - $T/DATOS.BIN; then ok "D: intacta"; else ko "D:"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && { cat $T/pt; cat $T/log3; }

echo "-- PARTIR: disco sin tabla de particiones (todo él una unidad FAT) --"
S=$T/s.img
dd if=/dev/zero of=$S bs=1M count=16 status=none
mformat -i $S -T 32768 -h 16 -s 63 -v SUPER ::
# como lo deja FORMATEA: código de arranque donde iría la tabla
dd if=build/boot16.bin of=$S bs=1 skip=62 seek=62 count=448 conv=notrunc status=none
mcopy -i $S $T/UNO.TXT ::UNO.TXT
cp $S $T/s0.img
run $T/log4 "" $S 3 "ECHO =M1=" "PARTIR /STATUS" "PARTIR /MBR" "PARTIR /ACT:1" "PARTIR /EMPTY" \
    "IF ERRORLEVEL 1 ECHO nivel-ocupado" "DIR C:" "ECHO =M2="
part $T/log4 M1 > $T/s1
check $T/s1 "Este disco no tiene tabla de particiones: todo él es una unidad FAT."
n=$(grep -c "PRI DOS\|No DOS" $T/s1)
[ "$n" = 1 ] && ok "se muestra como una sola partición" || ko "PARTIR /STATUS muestra $n particiones"
check $T/s1 "Para particionarlo hay que eliminar antes esa unidad (partición 1)."
checkx $T/s1 "nivel-ocupado"
check $T/s1 "UNO      TXT"
if cmp -s $S $T/s0.img; then ok "/MBR y /ACT no tocan el disco"; else ko "/MBR o /ACT han escrito en un disco sin tabla de particiones"; fi
# (de paso, en B:, un disquete nuevo de 720 KB en una unidad de 1,44 MB)
K=$T/k.img
dd if=/dev/zero of=$K bs=1k count=720 status=none
run $T/log5 $K $S 3 "PARTIR /DEL:1" "PARTIR /PRI" "FORMATEA C: /AUTOTEST" "COPY A:\\README.TXT C:\\" \
    "FORMATEA B: /F:720 /AUTOTEST" "COPY A:\\COMMAND.COM B:\\"
ptable $S > $T/pt
read -r _ ty1 st1 sz1 tot1 < <(sed -n 1p $T/pt)
[ "${st1:-0}" = 63 ] && [ "${tot1:-0}" = "${sz1:-x}" ] && ok "después de /DEL:1 y /PRI: partición normal desde el sector 63" || ko "tabla: $(cat $T/pt)"
if [ -n "${st1:-}" ] && mtype -i $S@@$((st1 * 512)) ::README.TXT 2>/dev/null | cmp -s - tests/disk/README.TXT; then ok "la partición nueva se puede usar"; else ko "partición nueva"; fi
if mtype -i $K ::COMMAND.COM 2>/dev/null | cmp -s - build/COMMAND.COM; then ok "FORMATEA /F:720: disquete nuevo de 720 KB formateado y escrito"; else ko "disquete de 720 KB"; fi
fsck_ok $K "720 KB"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && { cat $T/log4 $T/log5; }

# ------------------------------------------------------------------ FORMATEA 360 KB, INSTALAR
echo "-- FORMATEA un disquete de 360 KB; INSTALAR --"
cp "$IMG" $T/f.img
printf '@ECHO OFF\r\nPATH A:\\HISPADOS;A:\\\r\nECHO Fecha: hoy\r\nREM Nota: una=A:\\X\r\n' > $T/AUTOEXEC.BAT
mcopy -o -i $T/f.img $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
F=$T/f360.img
dd if=/dev/zero of=$F bs=1k count=360 status=none
H=$T/h.img
dd if=/dev/zero of=$H bs=1M count=32 status=none
python3 - $H <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
f.write(bytes(446) + bytes([0x80, 1, 1, 0, 0x06, 63, 0xE0, 0x3F]) + struct.pack('<II', 63, 65536 - 63)
        + bytes(48) + b'\x55\xAA')
EOF
mformat -i $H@@32256 -T 65473 -h 64 -s 63 -H 63 ::
# nuevo (sin formato) con /F:360 y, ya con formato, sin /F
run $T/log6 $F $H 5 "FORMATEA B: /F:360 /AUTOTEST" "COPY A:\\README.TXT B:\\" "FORMATEA B: /AUTOTEST" \
    "COPY A:\\COMMAND.COM B:\\" "FORMATEA B: /F:1440 /AUTOTEST" "INSTALAR C:" "ECHO =M1="
n=$(grep -c "Formato completado." $T/log6)
[ "$n" = 2 ] && ok "disquete de 360 KB: formateado dos veces (con /F:360 y sin /F)" || ko "disquete de 360 KB: $n formatos completados"
check $T/log6 "Tamaño de disquete no admitido"
if mtype -i $F ::COMMAND.COM 2>/dev/null | cmp -s - build/COMMAND.COM; then ok "disquete de 360 KB escrito"; else ko "disquete de 360 KB"; fi
[ "$(mdir -b -i $F :: 2>/dev/null | wc -l)" = 1 ] && ok "el segundo formato lo dejó vacío" || ko "contenido del disquete de 360 KB"
fsck_ok $F "360 KB"
check $T/log6 "Hispa-DOS instalado en la unidad C:"
mtype -i $H@@32256 ::AUTOEXEC.BAT 2>/dev/null | tr -d '\r' > $T/auto.txt
checkx $T/auto.txt "PATH C:\\HISPADOS;C:\\"
checkx $T/auto.txt "ECHO Fecha: hoy"
checkx $T/auto.txt "REM Nota: una=C:\\X"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && { cat $T/log6; cat $T/auto.txt; }
# disquete casi lleno: cabe el sistema pero no toda la carpeta \HISPADOS
L=$T/l.img
dd if=/dev/zero of=$L bs=1k count=1440 status=none; mformat -i $L -f 1440 ::
head -c $(( 1457664 - 62 * 1024 )) /dev/zero > $T/RELLENO.BIN
mcopy -i $L $T/RELLENO.BIN ::RELLENO.BIN
run $T/log7 $L "" 10 "INSTALAR B:" "IF ERRORLEVEL 4 ECHO nivel-incompleto" "ECHO =M1="
check $T/log7 "La instalación NO está completa"
checkn $T/log7 "Ya puede arrancar desde ella"
checkx $T/log7 "nivel-incompleto"
bad=0
while read -r name size; do
  want=$(mdir -i "$IMG" ::HISPADOS/$name 2>/dev/null | awk -v n="${name%%.*}" '$1==n{print $3}')
  [ "$size" = "$want" ] || { bad=1; echo "       $name: $size bytes (deberían ser $want)"; }
done < <(mdir -b -i $L ::HISPADOS 2>/dev/null | sed 's|.*/||' | while read -r n; do
           [ -n "$n" ] && echo "$n $(mdir -i $L ::HISPADOS/$n | awk -v b="${n%%.*}" '$1==b{print $3}')"; done)
[ $bad = 0 ] && ok "no quedan archivos a medias en B:\\HISPADOS" || ko "archivos a medias en B:\\HISPADOS"
fsck_ok $L "disquete lleno"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && cat $T/log7
rm -rf $T
exit $fail
