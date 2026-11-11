#!/bin/bash
# Prueba de SYS: disco formateado sin sistema -> SYS C: -> arranca solo.
# SYS test: format without /S, run SYS C:, then boot from the hard disk.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
dd if=/dev/zero of=$T/hdd.img bs=1M count=32 status=none
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 4
  send "FDISK /PRI" 2
  send "FORMAT C: /V:SINSYS /AUTOTEST" 4
  send "SYS C:" 3
  send "SYS A:" 2
  send "DIR C:\\" 2
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy \
      -drive file=$T/hdd.img,format=raw,if=ide -boot a -no-reboot > $T/l1 2>&1
{
  sleep 4
  send "VER"
  send "DIR" 2
} | timeout 15 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/hdd.img,format=raw,if=ide -boot c -no-reboot > $T/l2 2>&1
tr -d '\r' < $T/l1 > $T/l1.txt; tr -d '\r' < $T/l2 > $T/l2.txt
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
echo "== HispaDOS SYS tests =="
check $T/l1.txt "Format complete."
check $T/l1.txt "Sistema transferido a la unidad C:"
check $T/l1.txt "No se puede transferir el sistema a la unidad de arranque."
check $T/l1.txt "COMMAND  COM"
check $T/l2.txt "HispaDOS 0.2 - MS-DOS 6.22 compatible kernel"
check $T/l2.txt " Volume in drive C is SINSYS"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/l1.txt $T/l2.txt; fi
rm -rf $T
exit $fail
