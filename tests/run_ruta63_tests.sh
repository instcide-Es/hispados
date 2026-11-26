#!/bin/bash
# Carpetas de más de 63 letras (sin «C:\»): CD y MD las rechazan, como
# MS-DOS (antes se llegaba a 65 y INT 21h 47h desbordaba en 2 octetos el
# búfer de 64 del programa). Con 63, sí.
# Directory paths longer than 63 characters are refused.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
cp "$IMG" $T/f.img
B=""
for d in R1234567 R2234567 R3234567 R4234567 R5234567 R6234567; do B="$B/$d"; mmd -i $T/f.img "::$B"; done
mmd -i $T/f.img "::$B/ABCDEF.IJ" "::$B/ABCDEFG.IJ"
W=$(echo "$B" | tr / '\\')
{ sleep 5
  for c in "CD $W\\ABCDEF.IJ" "CD" "CD \\" "CD $W\\ABCDEFG.IJ" "CD" "MD $W\\ABCDEFGH.IJ" "ECHO fin-ruta63"; do
    printf '%s\r' "$c"; sleep 1.5
  done; sleep 1
} | timeout 25 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
echo "== Hispa-DOS carpetas de 63 letras como mucho =="
ok63="A:$W\\ABCDEF.IJ"
if grep -qxF -- "$ok63" $T/log.txt; then echo "  OK   CD a una de 63 letras"; else echo "  FAIL CD a una de 63 letras"; fail=1; fi
if grep -qF -- "A:$W\\ABCDEFG.IJ" <(grep -v '>CD' $T/log.txt); then echo "  FAIL CD a una de 64 letras aceptado"; fail=1
else echo "  OK   CD a una de 64 letras rechazado"; fi
if mdir -i $T/f.img "::$B/ABCDEFGH.IJ" >/dev/null 2>&1; then echo "  FAIL MD de una de 65 letras hecho"; fail=1
else echo "  OK   MD de una de 65 letras rechazado"; fi
if grep -qF "fin-ruta63" $T/log.txt; then echo "  OK   fin-ruta63"; else echo "  FAIL fin-ruta63"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
