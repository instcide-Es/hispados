#!/bin/bash
# HSPRATON: INT 33h 17h (restaurar el estado) con la proporción mickeys/
# puntos a 0: se vuelve a la de siempre en vez de dividir por cero en la
# siguiente IRQ 12. Con 0Fh, una proporción 0 o negativa no se toma.
# INT 33h 17h with a zero mickey ratio.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/RAT17.COM tests/progs/rat17.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/RAT17.COM ::
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
{ sleep 6; printf 'A:\\HISPADOS\\HSPRATON /DIRECTO\r'; sleep 2; printf 'RAT17\r'; sleep 3
  mon "mouse_move 40 20"; sleep 1; mon "mouse_move 40 20"; sleep 2
  printf 'ECHO fin-rat17\r'; sleep 2
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
echo "== HSPRATON: 17h con la proporción a 0 =="
for want in "RAT17: estado restaurado" "RAT17: el raton se mueve" "fin-rat17"; do
  if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
done
if grep -qF "Error de divisi" $T/log.txt; then echo "  FAIL Error de división"; fail=1; else echo "  OK   sin «Error de división»"; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
