#!/bin/bash
# INT 25h/26h en una unidad FAT32: rechazadas (0207h, como MS-DOS 7.1: para
# eso está 7305h) también si nadie la había usado todavía (sin montar, el
# núcleo no sabía que era FAT32 y las dejaba pasar). En una FAT16, sí.
# INT 25h/26h refused on FAT32 even before the drive is mounted.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/ABS25.COM tests/progs/abs25.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/ABS25.COM ::
truncate -s 600M $T/c.img
echo 'start=2048, type=c' | /usr/sbin/sfdisk -q $T/c.img
mkfs.fat -F 32 -n DISCO32 --offset 2048 $T/c.img $(( (600*2048 - 2048) / 2 )) > /dev/null 2>&1
truncate -s 64M $T/d.img
echo 'start=2048, type=6' | /usr/sbin/sfdisk -q $T/d.img
mkfs.fat -F 16 -n DISCO16 --offset 2048 $T/d.img $(( (64*2048 - 2048) / 2 )) > /dev/null 2>&1
{ sleep 5
  for c in 'ABS25 C' 'ABS25 D' 'DIR C:\' 'ABS25 C' 'ECHO fin-abs25'; do printf '%s\r' "$c"; sleep 2; done
  sleep 1
} | timeout 30 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/c.img,format=raw,if=ide,index=0 \
      -drive file=$T/d.img,format=raw,if=ide,index=1 -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
echo "== Hispa-DOS INT 25h/26h y FAT32 sin montar =="
n=$(grep -cF "ABS25 C: rechazado 0207" $T/log.txt)
if [ "$n" = 2 ]; then echo "  OK   C: (FAT32) rechazada antes y después de usarla"; else echo "  FAIL C: (FAT32) rechazada $n veces (2)"; fail=1; fi
for want in "ABS25 D: leido" "DISCO32" "fin-abs25"; do
  if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
