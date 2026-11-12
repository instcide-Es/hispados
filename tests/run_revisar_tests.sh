#!/bin/bash
# Prueba de REVISAR (CHKDSK): informe de un disquete (B:) y de un disco duro
# con una partición FAT16 (C:) sanos (los números se comparan con los que
# calcula tests/tools/fatimg.py y con fsck.fat), y de discos estropeados a
# propósito: cadenas perdidas, archivos con la cadena más larga o más corta
# que su tamaño, vínculos cruzados, «..» mal, muchas entradas y varios
# niveles de carpetas. Sin /F no se escribe nada (cmp); con /F, fsck.fat -n
# pasa después y las cadenas perdidas quedan en FILE0000.CHK...
# CHKDSK test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
FI="python3 tests/tools/fatimg.py"
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
# check FICHERO TEXTO / checkn FICHERO TEXTO (no debe estar)
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "no debía salir: $2"; else ok "no sale: $2"; fi; }
# número con puntos de miles
fmt() { printf "%'d" "$1" | tr ',' '.'; }
export LC_NUMERIC=en_US.UTF-8
# el trozo del registro entre «=Mn=» y la siguiente marca
part() { awk -v m="=$2=" '$0==m{p=1;next} /^=M[0-9]+=$/{p=0} p' "$1"; }
# run LOG B.IMG C.IMG [ESPERA] órdenes...
run() {
  local log=$1 b=$2 c=$3 w=$4; shift 4
  local drv=(-drive file=$T/f.img,format=raw,if=floppy,index=0)
  [ -n "$b" ] && drv+=(-drive file=$b,format=raw,if=floppy,index=1)
  [ -n "$c" ] && drv+=(-drive file=$c,format=raw,if=ide,index=0)
  { sleep 6; for x in "$@"; do printf '%s\r' "$x"; sleep "$w"; done; } | \
    timeout $(( 10 + $# * ${w%.*} + 10 )) qemu-system-i386 -display none -serial stdio -monitor none \
      -parallel none "${drv[@]}" -boot a -no-reboot > $log.raw 2>&1
  tr -d '\r' < $log.raw | iconv -f CP437 -t UTF-8 > $log
}
# el informe de un trozo cuadra con fatimg.py info
report_ok() {
  local txt=$1 img=$2 name=$3
  eval "$($FI $img info | awk '{printf "%s=(%s)\n", $1, substr($0, length($1)+2)}')"
  local good=1
  for l in "$(fmt $total) bytes de espacio total en disco" \
           "$(fmt ${user[1]}) bytes en ${user[0]} archivos de usuario" \
           "$(fmt $avail) bytes disponibles en el disco" \
           "$(fmt $bpc) bytes en cada unidad de asignación" \
           "$(fmt $units) unidades de asignación en total en el disco" \
           "$(fmt $freeunits) unidades de asignación disponibles en el disco"; do
    grep -qF -- "$l" $txt || { ko "$name: falta «$l»"; good=0; }
  done
  if [ "${dirs[0]}" -gt 0 ]; then
    grep -qF -- "$(fmt ${dirs[1]}) bytes en ${dirs[0]} directorios" $txt || { ko "$name: directorios"; good=0; }
  fi
  if [ "${hidden[0]}" -gt 0 ]; then
    grep -qF -- "$(fmt ${hidden[1]}) bytes en ${hidden[0]} archivos ocultos" $txt || { ko "$name: ocultos"; good=0; }
  fi
  # y con fsck.fat: «N files, usadas/total clusters»
  local fs; fs=$(fsck.fat -n -v $img 2>/dev/null | grep -E '^.*: [0-9]+ files, [0-9]+/[0-9]+ clusters$')
  local used=${fs##*files, }; used=${used%%/*}; local tot=${fs##*/}; tot=${tot%% *}
  [ "$tot" = "$units" ] || { ko "$name: fsck.fat dice $tot unidades"; good=0; }
  [ $((tot - used)) = "$freeunits" ] || { ko "$name: fsck.fat dice $((tot - used)) libres"; good=0; }
  [ $good = 1 ] && ok "$name: el informe cuadra con fatimg.py y fsck.fat ($units unidades, $freeunits libres)"
}
fsck_ok() {
  if fsck.fat -n $1 >/dev/null 2>&1; then ok "fsck.fat -n $2"; else ko "fsck.fat -n $2"; fsck.fat -n $1 | head -20; fi
}
# disco duro de 32 MB con una partición FAT16 (desde el sector 63)
mkhdd() {
  dd if=/dev/zero of=$1 bs=1M count=32 status=none
  python3 - $1 <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
f.write(bytes(446) + bytes([0x80, 1, 1, 0, 0x06, 63, 0xE0, 0x3F]) + struct.pack('<II', 63, 65536 - 63)
        + bytes(48) + b'\x55\xAA')
EOF
  mformat -i $1@@32256 -T 65473 -h 64 -s 63 -H 63 -N 5A5A0001 -v $2 ::
}
part_img() { dd if=$1 of=$2 bs=512 skip=63 status=none; }

cp "$IMG" $T/f.img
printf 'uno\r\n' > $T/UNO.TXT
head -c 5000 /dev/urandom > $T/DOS.BIN
head -c 3000 /dev/urandom > $T/FRAG.DAT
head -c 700 /dev/urandom > $T/OCULTO.SYS
head -c 3000 /dev/urandom > $T/LARGO.TXT
head -c 1000 /dev/urandom > $T/CORTO.TXT
head -c 20000 /dev/urandom > $T/GRANDE.BIN

# ---- disquete sano ----
B=$T/b.img
dd if=/dev/zero of=$B bs=1k count=1440 status=none
mformat -i $B -f 1440 -N 1234ABCD -v SANO ::
mcopy -i $B $T/UNO.TXT $T/DOS.BIN $T/FRAG.DAT $T/OCULTO.SYS ::
mattrib -i $B +h ::OCULTO.SYS
mmd -i $B ::DIR1 ::DIR1/SUB2 ::DIR1/SUB2/SUB3 ::MUCHOS
mcopy -i $B $T/UNO.TXT ::DIR1/SUB2/SUB3/HONDO.TXT
for i in $(seq -w 1 40); do mcopy -i $B $T/UNO.TXT ::MUCHOS/F$i.TXT; done
$FI $B fragment FRAG.DAT
cp $B $T/b0.img
# ---- disco duro sano ----
C=$T/c.img
mkhdd $C DISCOC
for d in A A/B A/B/C A/B/C/D A/B/C/D/E A/B/C/D/E/F; do mmd -i $C@@32256 ::$d; done
mcopy -i $C@@32256 $T/UNO.TXT ::A/B/C/D/E/F/FONDO.TXT
mcopy -i $C@@32256 $T/GRANDE.BIN $T/DOS.BIN ::
mmd -i $C@@32256 ::LLENA
for i in $(seq -w 1 100); do mcopy -i $C@@32256 $T/UNO.TXT ::LLENA/A$i.TXT; done
cp $C $T/c0.img

echo "== Hispa-DOS REVISAR =="
echo "-- discos sanos --"
run $T/log1 $B $C 3 "ECHO =M1=" "REVISAR B:" "ECHO =M2=" "REVISAR B:\\*.* /V" "ECHO =M3=" \
    "REVISAR B:\\DIR1\\SUB2\\SUB3\\*.TXT" "ECHO =M4=" "CHKDSK C:" "ECHO =M5=" \
    "REVISAR Z:" "IF ERRORLEVEL 1 ECHO nivel-z" "ECHO =M6=" "REVISAR B: /X" "ECHO =M7="
part $T/log1 M1 > $T/s1
part $T/log1 M2 > $T/s2
part $T/log1 M3 > $T/s3
part $T/log1 M4 > $T/s4
part $T/log1 M5 > $T/s5
part $T/log1 M6 > $T/s6
check $T/s1 "El volumen SANO se creó el"
check $T/s1 "El número de serie del volumen es 1234-ABCD"
checkn $T/s1 "No se escribirán las correcciones"
checkn $T/s1 "no es válida"
check $T/s1 "bytes de memoria total"
check $T/s1 "bytes libres"
report_ok $T/s1 $B "disquete"
check $T/s2 "B:\\FRAG.DAT contiene 3 bloques no contiguos"
check $T/s2 "Directorio B:\\DIR1\\SUB2\\SUB3"
check $T/s2 "B:\\DIR1\\SUB2\\SUB3\\HONDO.TXT"
check $T/s2 "B:\\MUCHOS\\F40.TXT"
checkn $T/s2 "B:\\DOS.BIN contiene"
check $T/s3 "Todos los archivos especificados son contiguos"
checkn $T/s3 "FRAG.DAT"
check $T/s4 "El volumen DISCOC se creó el"
check $T/s4 "El número de serie del volumen es 5A5A-0001"
checkn $T/s4 "no es válida"
part_img $C $T/p.img
report_ok $T/s4 $T/p.img "partición FAT16"
check $T/s5 "Especificación de unidad no válida"
check $T/s5 "nivel-z"
check $T/s6 "Parámetro no válido"
if cmp -s $B $T/b0.img; then ok "disquete sin cambios"; else ko "disquete cambiado"; fi
if cmp -s $C $T/c0.img; then ok "disco duro sin cambios"; else ko "disco duro cambiado"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log1

# ---- disquete estropeado: sin /F no se toca ----
echo "-- disquete estropeado, sin /F --"
M=$T/m.img
dd if=/dev/zero of=$M bs=1k count=1440 status=none
mformat -i $M -f 1440 -N 0BAD0001 -v ROTO ::
mcopy -i $M $T/UNO.TXT $T/LARGO.TXT $T/CORTO.TXT $T/DOS.BIN ::
mmd -i $M ::DIR1 ::DIR1/SUB
mcopy -i $M $T/UNO.TXT ::DIR1/SUB/DENTRO.TXT
$FI $M setsize LARGO.TXT 600 >/dev/null           # 6 clústeres para 600 bytes
$FI $M setsize CORTO.TXT 5000 >/dev/null          # 2 clústeres para 5000 bytes
$FI $M dotdot DIR1\\SUB 0 >/dev/null              # «..» al raíz en vez de a DIR1
$FI $M lost 3 PERDIDO1 >/dev/null
$FI $M lost 1 PERDIDO2 >/dev/null
cp $M $T/m0.img
cp $M $T/n.img
# ---- disco duro estropeado: con /F y S ----
D=$T/d.img
mkhdd $D ROTOC
for d in A A/B A/B/C A/B/C/D A/B/C/D/E; do mmd -i $D@@32256 ::$d; done
mcopy -i $D@@32256 $T/LARGO.TXT ::A/B/C/D/E/LARGO.TXT
mcopy -i $D@@32256 $T/GRANDE.BIN $T/CORTO.TXT ::
mmd -i $D@@32256 ::LLENA
for i in $(seq -w 1 100); do mcopy -i $D@@32256 $T/UNO.TXT ::LLENA/A$i.TXT; done
$FI $D@32256 setsize A\\B\\C\\D\\E\\LARGO.TXT 10 >/dev/null
$FI $D@32256 setsize CORTO.TXT 9000 >/dev/null
$FI $D@32256 lost 2 RECUPERA >/dev/null
run $T/log2 $M $D 3 "ECHO =M1=" "REVISAR B:" "S" "ECHO =M2=" "REVISAR B: /V" "N" "ECHO =M3=" \
    "REVISAR C: /F" "S" "ECHO =M4=" "REVISAR C:" "ECHO =M5=" "DIR C:\\*.CHK" "ECHO =M6="
part $T/log2 M1 > $T/s1
part $T/log2 M2 > $T/s2
part $T/log2 M3 > $T/s3
part $T/log2 M4 > $T/s4
part $T/log2 M5 > $T/s5
check $T/s1 "Se encontraron errores y no se ha indicado /F."
check $T/s1 "No se escribirán las correcciones en el disco."
check $T/s1 "B:\\LARGO.TXT"
check $T/s1 "   La cadena de asignación no es válida."
check $T/s1 "B:\\CORTO.TXT"
check $T/s1 "   El tamaño se ha ajustado."
check $T/s1 "B:\\DIR1\\SUB"
check $T/s1 "   La entrada «..» del directorio no es válida."
check $T/s1 "Se encontraron 4 unidades de asignación perdidas en 2 cadenas."
check $T/s1 "¿Convertir las cadenas perdidas en archivos (S/N)?"
check $T/s1 "Se recuperarían 2.048 bytes en 2 archivos."
check $T/s2 "2.048 bytes de espacio en disco se liberarían."
checkn $T/s1 "B:\\UNO.TXT"
checkn $T/s1 "B:\\DOS.BIN"
if cmp -s $M $T/m0.img; then ok "sin /F el disquete no cambia"; else ko "sin /F el disquete ha cambiado"; fi
echo "-- disco duro estropeado, /F y S --"
check $T/s3 "C:\\A\\B\\C\\D\\E\\LARGO.TXT"
check $T/s3 "C:\\CORTO.TXT"
check $T/s3 "Se encontraron 2 unidades de asignación perdidas en 1 cadenas."
check $T/s3 "bytes recuperados en 1 archivos."
checkn $T/s3 "No se escribirán"
checkn $T/s4 "no es válida"
checkn $T/s4 "ajustado"
checkn $T/s4 "perdidas"
check $T/s5 "FILE0000 CHK"
part_img $D $T/p.img
fsck_ok $T/p.img "después de REVISAR C: /F"
report_ok $T/s4 $T/p.img "partición corregida"
if mtype -i $T/p.img ::FILE0000.CHK | head -c 8 | grep -q RECUPERA; then ok "FILE0000.CHK tiene la cadena perdida"; else ko "contenido de FILE0000.CHK"; fi
sz=$(mdir -i $T/p.img ::FILE0000.CHK | awk '/^FILE0000/{print $3}')
bpcd=$($FI $T/p.img info | awk '$1=="bpc"{print $2}')
[ "$sz" = $((2 * bpcd)) ] && ok "FILE0000.CHK mide 2 unidades ($sz bytes)" || ko "tamaño de FILE0000.CHK: $sz"
if cmp -s <(mtype -i $T/p.img ::A/B/C/D/E/LARGO.TXT) <(head -c 10 $T/LARGO.TXT); then ok "LARGO.TXT acortado a 10 bytes"; else ko "LARGO.TXT"; fi
csz=$(mdir -i $T/p.img ::CORTO.TXT | awk '/^CORTO/{print $3}')
[ "$csz" = $(( (1000 + bpcd - 1) / bpcd * bpcd )) ] && ok "CORTO.TXT ajustado a $csz bytes" || ko "tamaño de CORTO.TXT: $csz"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log2

# ---- /F y N en el disquete; vínculos cruzados en el disco duro ----
echo "-- disquete /F y N; vínculos cruzados --"
X=$T/x.img
mkhdd $X CRUCE
mmd -i $X@@32256 ::DOCS
mcopy -i $X@@32256 $T/GRANDE.BIN ::UNO.BIN
mcopy -i $X@@32256 $T/DOS.BIN ::DOCS/DOS.BIN
xc=$($FI $X@32256 crosslink UNO.BIN DOCS\\DOS.BIN)
cp $X $T/x0.img
freeb=$($FI $T/n.img info | awk '$1=="freeunits"{print $2}')
run $T/log3 $T/n.img $X 3 "ECHO =M1=" "REVISAR B: /F" "N" "ECHO =M2=" "REVISAR B:" "ECHO =M3=" \
    "REVISAR C:" "N" "ECHO =M4="
part $T/log3 M1 > $T/s1
part $T/log3 M2 > $T/s2
part $T/log3 M3 > $T/s3
check $T/s1 "Se encontraron 4 unidades de asignación perdidas en 2 cadenas."
check $T/s1 "2.048 bytes de espacio en disco liberados."
checkn $T/s1 "No se escribirán"
checkn $T/s2 "no es válida"
checkn $T/s2 "perdidas"
fsck_ok $T/n.img "después de REVISAR B: /F + N"
report_ok $T/s2 $T/n.img "disquete corregido"
freea=$($FI $T/n.img info | awk '$1=="freeunits"{print $2}')
# las 4 perdidas y las 4 que sobraban en LARGO.TXT, libres
[ "$freea" = $((freeb + 8)) ] && ok "8 unidades más libres ($freeb -> $freea)" || ko "libres: $freeb -> $freea"
if mtype -i $T/n.img ::DIR1/SUB/DENTRO.TXT | grep -q uno; then ok "DENTRO.TXT intacto"; else ko "DENTRO.TXT"; fi
check $T/s3 "C:\\DOCS\\DOS.BIN y C:\\UNO.BIN tienen vínculos cruzados en la unidad de asignación $xc."
check $T/s3 "unidades de asignación perdidas en 1 cadenas."
if cmp -s $X $T/x0.img; then ok "sin /F el disco duro no cambia"; else ko "sin /F el disco duro ha cambiado"; fi
check $T/s3 "   El tamaño se ha ajustado."
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log3

# ---- CD/DVD y disco que no es FAT ----
echo "-- CD/DVD y disco sin FAT --"
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPCD.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
mkdir -p $T/iso
printf 'hola\r\n' > $T/iso/LEEME.TXT
genisoimage -quiet -V DISCO -o $T/disco.iso $T/iso 2>/dev/null
dd if=/dev/zero of=$T/z.img bs=1k count=1440 status=none
{ sleep 6; for x in "REVISAR C:" "IF ERRORLEVEL 1 ECHO nivel-cd" "REVISAR B:" "IF ERRORLEVEL 1 ECHO nivel-nofat"; do
    printf '%s\r' "$x"; sleep 3; done; } | \
  timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/z.img,format=raw,if=floppy,index=1 \
    -drive file=$T/disco.iso,media=cdrom,if=ide,index=2 -boot a -no-reboot > $T/log4.raw 2>&1
tr -d '\r' < $T/log4.raw | iconv -f CP437 -t UTF-8 > $T/log4
check $T/log4 "REVISAR no se puede usar en una unidad de CD/DVD"
check $T/log4 "nivel-cd"
check $T/log4 "El disco no tiene un sistema de archivos FAT12 o FAT16 válido"
check $T/log4 "nivel-nofat"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/>REVISAR/,$p' $T/log4
rm -rf $T
exit $fail
