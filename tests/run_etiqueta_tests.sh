#!/bin/bash
# Prueba de ETIQUETA (LABEL): poner, cambiar, quitar y preguntar la etiqueta
# de volumen, en un disquete (B:) y en un disco duro con particiones (C:).
# Se comprueba con mtools (etiqueta del directorio raíz y del sector de
# arranque) y con fsck.fat.
# LABEL test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
dd if=/dev/zero of=$T/b.img bs=1k count=1440 status=none
mformat -i $T/b.img -f 1440 -N 1234ABCD ::
printf 'hola\r\n' > $T/H.TXT; mcopy -i $T/b.img $T/H.TXT ::H.TXT
# disco duro de 32 MB con una partición FAT16 (como en las otras pruebas)
dd if=/dev/zero of=$T/c.img bs=1M count=32 status=none
python3 - $T/c.img <<'EOF'
import struct, sys
f = open(sys.argv[1], 'r+b')
f.write(bytes(446) + bytes([0x80, 1, 1, 0, 0x06, 63, 0xE0, 0x3F]) + struct.pack('<II', 63, 65536 - 63)
        + bytes(48) + b'\x55\xAA')
EOF
mformat -i $T/c.img@@32256 -T 65473 -h 64 -s 63 -H 63 -v VIEJA ::
CMDS=("ETIQUETA B:DATOS 2026" "DIR B:" "ETIQUETA B:Otra"
      "ETIQUETA B:" "" "S"
      "ETIQUETA B:" "pregunta"
      "ETIQUETA B:DEMASIADO LARGA" "ETIQUETA B:MAL*NOMBRE"
      "LABEL C:NUEVA" "DIR C:" "ETIQUETA Z:X")
{ sleep 6; for c in "${CMDS[@]}"; do printf '%s\r' "$c"; sleep 2; done; } | \
  timeout 80 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
  -drive file=$T/f.img,format=raw,if=floppy,index=0 -drive file=$T/b.img,format=raw,if=floppy,index=1 \
  -drive file=$T/c.img,format=raw,if=ide,index=0 -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
echo "== Hispa-DOS ETIQUETA =="
check "El volumen de la unidad B es DATOS 2026"
check "El volumen de la unidad B es OTRA"
check "El número de serie del volumen es 1234-ABCD"
check "¿Quitar la etiqueta actual del volumen (S/N)?"
check "El volumen de la unidad B no tiene etiqueta"
check "La etiqueta tiene más de 11 caracteres"
check "Caracteres no válidos en la etiqueta del volumen"
check "El volumen de la unidad C es NUEVA"
check "Especificación de unidad no válida"
lab=$(mlabel -s -i $T/b.img :: 2>/dev/null)
if [ "${lab%"${lab##*[! ]}"}" = " Volume label is PREGUNTA" ]; then ok "etiqueta final del disquete: PREGUNTA"; else ko "etiqueta del disquete: «$lab»"; fi
if dd if=$T/b.img bs=1 skip=43 count=11 status=none | grep -qx 'PREGUNTA   '; then ok "etiqueta en el sector de arranque"; else ko "etiqueta en el sector de arranque"; fi
lab=$(mlabel -s -i $T/c.img@@32256 :: 2>/dev/null)
if [ "${lab%"${lab##*[! ]}"}" = " Volume label is NUEVA" ]; then ok "etiqueta de la partición: NUEVA"; else ko "etiqueta de la partición: «$lab»"; fi
if mtype -i $T/b.img ::H.TXT | grep -q hola; then ok "H.TXT intacto"; else ko "H.TXT"; fi
if fsck.fat -n $T/b.img >/dev/null 2>&1; then ok "fsck.fat disquete"; else ko "fsck.fat disquete"; fi
dd if=$T/c.img of=$T/p.img bs=512 skip=63 status=none
if fsck.fat -n $T/p.img >/dev/null 2>&1; then ok "fsck.fat partición"; else ko "fsck.fat partición"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>ETIQUETA B:DATOS/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
