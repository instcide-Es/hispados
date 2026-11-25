#!/bin/bash
# Dos handles escribiendo el mismo archivo (tests/progs/doshand.asm):
# comparten primer grupo y tamaño; al cerrar no gana el tamaño viejo, y en un
# archivo vacío no se hace cada uno su cadena. fsck.fat limpio.
# Two handles writing the same file.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/DOSHAND.COM tests/progs/doshand.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/DOSHAND.COM ::
{ sleep 5; printf 'DOSHAND\r'; sleep 4; } | timeout 20 qemu-system-i386 -display none -serial stdio -monitor none \
  -parallel none -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
echo "== Hispa-DOS dos handles escribiendo el mismo archivo =="
if grep -qF "DOSHAND: fin" $T/log.txt; then echo "  OK   DOSHAND: fin"; else echo "  FAIL DOSHAND: fin"; fail=1; fi
want1=$(printf 'b%.0s' $(seq 10); printf 'a%.0s' $(seq 190))
v=$(mtype -i $T/f.img ::D1.BIN 2>/dev/null)
if [ "$v" = "$want1" ]; then echo "  OK   D1.BIN: 200 octetos, 10 'b' delante"; else echo "  FAIL D1.BIN: ${#v} octetos «${v:0:20}...»"; fail=1; fi
v=$(mtype -i $T/f.img ::D2.BIN 2>/dev/null)
want2=$(printf 'b%.0s' $(seq 3000))
if [ "$v" = "$want2" ]; then echo "  OK   D2.BIN: 3000 'b'"; else echo "  FAIL D2.BIN: ${#v} octetos «${v:0:20}...»"; fail=1; fi
if /usr/sbin/fsck.fat -n $T/f.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat limpio"
else echo "  FAIL fsck.fat"; cat $T/fsck.txt; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
