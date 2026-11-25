#!/bin/bash
# E/S por FCB (15h) sobre CON, AUX y NUL: va a su dispositivo (antes, al
# disco de la SFT: la unidad A:, que podía quedar estropeada). AUX, en
# Hispa-DOS, es un dispositivo nulo, como NUL: no sale nada.
# FCB I/O on character devices.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/FCBDEV.COM tests/progs/fcbdev.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/FCBDEV.COM ::
{ sleep 5; printf 'FCBDEV\r'; sleep 3; printf 'ECHO tras-fcbdev\r'; sleep 2; } |
  timeout 20 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
check() { if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then echo "  FAIL (no debía salir) $1"; fail=1; else echo "  OK   sin «$1»"; fi; }
echo "== Hispa-DOS E/S por FCB sobre dispositivos =="
check "FCB-EN-CON bien."
checkn "FCB-EN-AUX"   # (AUX es nulo en Hispa-DOS: no sale nada, y tampoco va al disco)
checkn "FCB-EN-NUL"
checkn "FCBDEV: fallo"
check "FCBDEV: fin"
if /usr/sbin/fsck.fat -n $T/f.img > $T/fsck.txt 2>&1; then echo "  OK   fsck.fat limpio"
else echo "  FAIL fsck.fat"; cat $T/fsck.txt; fail=1; fi
if cmp -s <(mtype -i $T/f.img ::COMMAND.COM) <(mtype -i "$IMG" ::COMMAND.COM); then echo "  OK   el disquete sigue igual"
else echo "  FAIL el disquete ha cambiado"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
