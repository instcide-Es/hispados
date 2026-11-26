#!/bin/bash
# Lotes con líneas de más de 200 letras: lo que sobra se descarta hasta el
# LF, como en MS-DOS (antes se ejecutaba como otra orden: «REM » + 196
# letras + « DEL C:\DATOS\*.TXT» borraba). También una línea con el CR justo
# al final de lo leído (el LF en la lectura siguiente) y GOTO tras ellas.
# Over-long batch lines: the rest is discarded up to the LF.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
cp "$IMG" $T/f.img
x196=$(printf 'x%.0s' $(seq 196))
y250=$(printf 'y%.0s' $(seq 250))
z194=$(printf 'z%.0s' $(seq 194))
{
  printf '@ECHO OFF\r\n'
  printf 'REM %s ECHO PELIGRO-EJECUTADO\r\n' "$x196"
  printf 'ECHO tras-la-larga\r\n'
  printf 'ECHO %s FINLARGA\r\n' "$y250"
  printf 'ECHO %s\r\n' "$z194"
  printf 'ECHO tras-cr\r\n'
  printf 'REM %s DEL SINO.TXT\n' "$x196"
  printf 'GOTO FIN\r\n'
  printf 'ECHO NO-DEBE-SALIR\r\n'
  printf ':FIN\r\n'
  printf 'ECHO lote-acabado\r\n'
} > $T/LARGO.BAT
printf 'no borrar' > $T/SINO.TXT
mcopy -o -i $T/f.img $T/LARGO.BAT $T/SINO.TXT ::
{ sleep 5; printf 'LARGO\r'; sleep 4; } | timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
  -parallel none -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin «$1»"; fi; }
echo "== Hispa-DOS lotes: líneas de más de 200 letras =="
checkn "PELIGRO-EJECUTADO"
check "tras-la-larga"
checkn "FINLARGA"
check "yyyyyyyyyyyyyyyyyyyy"
check "$z194"
check "tras-cr"
checkn "NO-DEBE-SALIR"
check "lote-acabado"
checkn "incorrect"
if mtype -i $T/f.img ::SINO.TXT 2>/dev/null | grep -q "no borrar"; then echo "  OK   SINO.TXT sigue ahí"
else echo "  FAIL SINO.TXT borrado"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
