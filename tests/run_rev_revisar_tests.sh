#!/bin/bash
# Revisión de REVISAR (CHKDSK): que /F nunca deje el disco peor de lo que
# estaba. Casos: raíz lleno con cadenas perdidas (no se liberan), una
# «carpeta» que en realidad apunta a los datos de un archivo (no se «corrigen»
# esos datos), una carpeta sin cadena válida (pasa a archivo vacío, no a un
# segundo nombre del raíz), más de 40 niveles de carpetas y más de 65.532
# entradas (no se toca nada), FAT16 de 256 sectores por FAT (128 KB) y
# ERRORLEVEL 255 cuando hay errores.
# CHKDSK review test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
FI="python3 tests/tools/fatimg.py"
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" "$1"; then ko "no debía salir: $2"; else ok "no sale: $2"; fi; }
# línea entera (para no confundir la salida de ECHO con la orden escrita)
checkx() { if grep -qxF -- "$2" "$1"; then ok "$2"; else ko "$2"; fi; }
checknx() { if grep -qxF -- "$2" "$1"; then ko "no debía salir: $2"; else ok "no sale: $2"; fi; }
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
# disco duro de N MB con una partición FAT16 desde el sector 63
mkhdd() {  # IMG MB ETIQUETA [opciones de mformat]
  local img=$1 mb=$2 lab=$3; shift 3
  rm -f $img; truncate -s ${mb}M $img
  python3 - $img $mb <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
n = int(sys.argv[2]) * 2048
f.write(bytes(446) + bytes([0x80, 1, 1, 0, 0x06, 63, 0xE0, 0x3F]) + struct.pack('<II', 63, n - 63)
        + bytes(48) + b'\x55\xAA')
EOF
  mformat -i $img@@32256 -T $(( mb * 2048 - 63 )) -h 64 -s 63 -H 63 -v $lab "$@" ::
}
part_img() { dd if=$1 of=$2 bs=512 skip=63 status=none; }

cp "$IMG" $T/f.img
printf 'uno\r\n' > $T/UNO.TXT
head -c 3000 /dev/urandom > $T/DATOS.BIN
python3 -c "import sys; sys.stdout.buffer.write(b'A' * 2048)" > $T/ZDATA.BIN

echo "== Hispa-DOS REVISAR (revisión) =="
# ---- 1) raíz lleno + cadena perdida; 2) carpetas que no lo son ----
echo "-- raíz lleno con cadenas perdidas; carpetas que no lo son --"
A=$T/ra.img
dd if=/dev/zero of=$A bs=1k count=1440 status=none; mformat -i $A -f 1440 ::
mcopy -i $A $T/UNO.TXT ::UNO.TXT
lostc=$($FI $A lost 2 PERDIDO)
$FI $A fillroot >/dev/null
B=$T/rb.img
dd if=/dev/zero of=$B bs=1k count=1440 status=none; mformat -i $B -f 1440 ::
mmd -i $B ::ADIR ::MALA
mcopy -i $B $T/UNO.TXT ::MALA/DENTRO.TXT
mcopy -i $B $T/ZDATA.BIN ::ZDATA.BIN
zc=$($FI $B chain ZDATA.BIN | cut -d' ' -f1)
$FI $B setclus ADIR $zc          # ADIR «empieza» en los datos de ZDATA.BIN
$FI $B setclus MALA 2000         # MALA empieza en un clúster libre
run $T/log1 $A "" 3 "ECHO =M1=" "REVISAR B: /F" "S" "IF ERRORLEVEL 255 ECHO nivel-255" "ECHO =M2=" \
    "REVISAR A:" "IF ERRORLEVEL 1 ECHO nivel-sano" "ECHO =M3="
part $T/log1 M1 > $T/s1
part $T/log1 M2 > $T/s2
check $T/s1 "Se encontraron 2 unidades de asignación perdidas en 1 cadenas."
check $T/s1 "No queda sitio en el directorio raíz"
ch=""; for c in $lostc; do ch="$ch $($FI $A get $c)"; done
want=" ${lostc##* } 65535"
[ "$ch" = "$want" ] && ok "raíz lleno: la cadena perdida sigue en la FAT ($lostc)" || ko "raíz lleno: la cadena perdida se ha tocado:$ch"
checkx $T/s1 "nivel-255"
checknx $T/s2 "nivel-sano"
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log1
run $T/log2 $B "" 3 "ECHO =M1=" "REVISAR B: /F" "S" "ECHO =M2="
part $T/log2 M1 > $T/s1
check $T/s1 "B:\\ADIR"
check $T/s1 "   No es un directorio válido: no se revisa su contenido."
checkn $T/s1 "B:\\ADIR\\"
if mtype -i $B ::ZDATA.BIN | cmp -s - $T/ZDATA.BIN; then ok "los datos de ZDATA.BIN no se han tocado"; else ko "REVISAR /F ha cambiado los datos de ZDATA.BIN"; fi
check $T/s1 "   Directorio no válido: convertido en un archivo vacío."
if mdir -i $B ::MALA 2>/dev/null | grep -q '^MALA  *0 '; then ok "MALA es un archivo vacío (no una carpeta que apunta al raíz)"; else ko "MALA: $(mdir -i $B ::MALA 2>&1 | grep -i mala)"; fi
if mtype -i $B ::FILE0000.CHK 2>/dev/null | head -c 20 | grep -q '^\.  '; then ok "la carpeta perdida, recuperada en FILE0000.CHK"; else ko "FILE0000.CHK"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log2

# ---- 3) más de 40 niveles de carpetas ----
echo "-- más de 40 niveles de carpetas --"
C=$T/c.img
mkhdd $C 32 HONDO
d=""
for i in $(seq 1 45); do d="$d/N"; mmd -i $C@@32256 ::$d; done
mcopy -i $C@@32256 $T/DATOS.BIN ::$d/FONDO.BIN
cp $C $T/c0.img
run $T/log3 "" $C 4 "ECHO =M1=" "REVISAR C: /F" "N" "ECHO =M2="
part $T/log3 M1 > $T/s1
check $T/s1 "   Hay demasiados niveles de directorios: no se revisa su contenido."
check $T/s1 "que parecen perdidas no se tocan."
checkn $T/s1 "liberados"
if cmp -s $C $T/c0.img; then ok "el disco no se ha tocado"; else ko "el disco ha cambiado"; fi
if mtype -i $C@@32256 ::$d/FONDO.BIN | cmp -s - $T/DATOS.BIN; then ok "FONDO.BIN (nivel 45) intacto"; else ko "FONDO.BIN"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log3

# ---- 4) más entradas que números de propietario (16 bits) ----
echo "-- 65.536 entradas --"
D=$T/d.img
mkhdd $D 32 MUCHOS
$FI $D@32256 manyfiles 65535 >/dev/null
mcopy -i $D@@32256 $T/DATOS.BIN ::ULTIMO.BIN      # la entrada 65.536
cp $D $T/d0.img
run $T/log4 "" $D 10 "ECHO =M1=" "REVISAR C: /F" "N" "ECHO =M2="
part $T/log4 M1 > $T/s1
check $T/s1 "Hay demasiados archivos y carpetas para revisar este disco"
checkn $T/s1 "liberados"
if cmp -s $D $T/d0.img; then ok "el disco no se ha tocado"; else ko "el disco ha cambiado"; fi
part_img $D $T/p.img
if fsck.fat -n $T/p.img >/dev/null 2>&1; then ok "fsck.fat -n: ULTIMO.BIN conserva sus unidades"; else ko "fsck.fat -n"; fsck.fat -n $T/p.img | head; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log4 | head -40

# ---- 5) FAT16 grande (4 KB por unidad, ~250 sectores por FAT) ----
echo "-- FAT16 de 250 MB: /F y escribir después --"
E=$T/e.img
mkhdd $E 250 GRANDE -c 8
mmd -i $E@@32256 ::DOCS
mcopy -i $E@@32256 $T/DATOS.BIN ::DOCS/DATOS.BIN
mcopy -i $E@@32256 $T/ZDATA.BIN ::LARGO.BIN
$FI $E@32256 lost 3 RECUPERA >/dev/null
$FI $E@32256 setsize LARGO.BIN 9000 >/dev/null
spf=$(python3 -c "import struct;f=open('$E','rb');f.seek(32256+22);print(struct.unpack('<H',f.read(2))[0])")
[ "$spf" -gt 128 ] && ok "la FAT ocupa $spf sectores (más de 64 KB)" || ko "FAT de $spf sectores: la prueba no vale"
run $T/log5 "" $E 5 "ECHO =M1=" "REVISAR C: /F" "S" "ECHO =M2=" "COPY A:\\README.TXT C:\\DOCS\\NUEVO.TXT" \
    "REVISAR C:" "IF ERRORLEVEL 1 ECHO nivel-mal" "ECHO =M3="
part $T/log5 M1 > $T/s1
part $T/log5 M2 > $T/s2
check $T/s1 "Se encontraron 3 unidades de asignación perdidas en 1 cadenas."
check $T/s1 "bytes recuperados en 1 archivos."
check $T/s1 "   El tamaño se ha ajustado."
checknx $T/s2 "nivel-mal"
checkn $T/s2 "no es válida"
part_img $E $T/p.img
if fsck.fat -n $T/p.img >/dev/null 2>&1; then ok "fsck.fat -n después de /F y de escribir"; else ko "fsck.fat -n"; fsck.fat -n $T/p.img | head; fi
if mtype -i $T/p.img ::FILE0000.CHK | head -c 8 | grep -q RECUPERA; then ok "FILE0000.CHK tiene la cadena perdida"; else ko "FILE0000.CHK"; fi
if mtype -i $T/p.img ::DOCS/DATOS.BIN | cmp -s - $T/DATOS.BIN; then ok "DATOS.BIN intacto"; else ko "DATOS.BIN"; fi
if mtype -i $T/p.img ::DOCS/NUEVO.TXT | cmp -s - tests/disk/README.TXT; then ok "NUEVO.TXT escrito después, correcto"; else ko "NUEVO.TXT"; fi
[ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ] && sed -n '/=M1=/,$p' $T/log5
rm -rf $T
exit $fail
