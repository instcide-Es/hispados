#!/bin/bash
# INT 21h 0Dh («restaurar disco») solo vacía los búferes, como en MS-DOS: la
# carpeta actual de C: (y la de A:) se conserva y lo escrito queda en el
# disco. Volver a mirar los discos es la función propia AX=336Ah (FDISK y
# FORMAT): esa sí deja C: en la raíz.
# INT 21h 0Dh only flushes; the drive rescan is AX=336Ah.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
Q="qemu-system-i386 -display none -serial stdio -monitor none -parallel none"
send() { sleep "${2:-0.8}"; printf '%s\r' "$1"; }
conv() { tr -d '\r' < "$1" | iconv -f CP437 -t UTF-8 > "$1.txt"; }
check() { if grep -qF -- "$2" "$1.txt"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi; }
checkn() { if grep -qF -- "$2" "$1.txt"; then echo "  FAIL (no debía salir) $2"; fail=1; else echo "  OK   sin «$2»"; fi; }
nasm -f bin -DSOLO0D -o $T/CERO.COM tests/progs/d0d.asm || exit 1
nasm -f bin -o $T/D0D.COM tests/progs/d0d.asm || exit 1
cp "$IMG" $T/f.img
mmd -i $T/f.img ::SUBA
mcopy -i $T/f.img $T/CERO.COM $T/D0D.COM ::SUBA/
truncate -s 64M $T/c.img
echo 'start=2048, type=6' | /usr/sbin/sfdisk -q $T/c.img
mkfs.fat -F 16 -n DISCOC --offset 2048 $T/c.img $(( (64*2048 - 2048) / 2 )) > /dev/null 2>&1
mmd -i $T/c.img@@1M ::JUEGO
{ sleep 5
  send "CD A:\\SUBA" 1
  send "C:" 1
  send "CD JUEGO" 1
  send "A:\\SUBA\\CERO" 2
  send "CD" 1
  send "ECHO dentro> RELATIVO.TXT" 1
  send "CD A:" 1
  send "A:\\SUBA\\D0D" 3
  send "CD" 1
  send "ECHO marca-final" 1
  sleep 2
} | timeout 40 $Q -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/c.img,format=raw,if=ide -boot a -no-reboot > $T/l 2>&1
conv $T/l
echo "== Hispa-DOS INT 21h 0Dh y AX=336Ah =="
check $T/l "solo-0D-hecha"
# tras 0Dh, CD sigue en C:\JUEGO, y A: en \SUBA
if awk '/solo-0D-hecha/{f=1;next} f && /^C:\\JUEGO$/{ok=1} END{exit !ok}' $T/l.txt; then echo "  OK   tras 0Dh, C: sigue en C:\\JUEGO"
else echo "  FAIL tras 0Dh, C: ya no está en C:\\JUEGO"; fail=1; fi
check $T/l "A:\\SUBA"
check $T/l "funcion-0D-hecha"
checkn $T/l "sin-336A"
# tras 336Ah, C: a la raíz (recorrido de discos nuevo)
if awk '/funcion-0D-hecha/{f=1;next} f && /^C:\\$/{ok=1} END{exit !ok}' $T/l.txt; then echo "  OK   tras 336Ah, C: en la raíz"
else echo "  FAIL tras 336Ah, C: no está en la raíz"; fail=1; fi
check $T/l "marca-final"
if mtype -i $T/c.img@@1M ::JUEGO/RELATIVO.TXT 2>/dev/null | grep -q dentro; then echo "  OK   RELATIVO.TXT creado en C:\\JUEGO"
else echo "  FAIL RELATIVO.TXT no está en C:\\JUEGO"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/l.txt; fi
exit $fail
