#!/bin/bash
# Arranque desde una partición FAT12 de un disco duro (8 MB, tipo 01h, que
# empieza en el sector 63): el sector de arranque FAT12 tiene que sumar los
# sectores ocultos del BPB. Antes solo arrancaba desde disquete.
# Boot from a FAT12 hard disk partition (boot sector must add hidden sectors).
set -u
cd "$(dirname "$0")/.."
T=$(mktemp -d)
python3 - $T/hd.img <<'EOF'
import struct, subprocess, sys, os
out = sys.argv[1]
HEADS, SPT, START = 16, 63, 63
psize = 16 * HEADS * SPT - START          # termina en límite de cilindro
total = 20 * HEADS * SPT
def chs(lba):
    c, rem = divmod(lba, HEADS * SPT); h, s = divmod(rem, SPT)
    return bytes([h, ((s + 1) & 0x3F) | ((c >> 2) & 0xC0), c & 0xFF])
open(out, 'wb').truncate(total * 512)
mbr = bytearray(open('build/mbr.bin', 'rb').read()[:446])
mbr += bytes([0x80]) + chs(START) + bytes([0x01]) + chs(START + psize - 1) + struct.pack('<II', START, psize)
mbr += bytes(48) + b'\x55\xAA'
with open(out, 'r+b') as f: f.write(mbr)
img = '%s@@%d' % (out, START * 512)
env = dict(os.environ, MTOOLS_SKIP_CHECK='1', MTOOLS_NO_VFAT='1')
def run(*a): subprocess.run(a, check=True, env=env, stdout=subprocess.DEVNULL)
run('mformat', '-i', img, '-T', str(psize), '-h', str(HEADS), '-s', str(SPT), '-H', str(START),
    '-v', 'FAT12HD', '-B', 'build/boot.bin', '::')
run('mcopy', '-i', img, 'build/KERNEL.SYS', '::KERNEL.SYS')
run('mcopy', '-i', img, 'build/COMMAND.COM', '::COMMAND.COM')
bs = open(out, 'rb'); bs.seek(START * 512 + 0x36); print('tipo:', bs.read(8).decode())
EOF
{ sleep 6; printf 'VER\r'; sleep 2; } |
  timeout 14 qemu-system-i386 -display none -serial stdio -monitor none \
    -drive file=$T/hd.img,format=raw,if=ide -boot c -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
echo "== Hispa-DOS arranque desde una partición FAT12 de disco duro =="
if grep -qF "KERNEL.SYS?" $T/log.txt; then echo "  FAIL el sector de arranque no encuentra KERNEL.SYS"; fail=1; else echo "  OK   sin \"KERNEL.SYS?\""; fi
for want in "núcleo compatible con MS-DOS" "Intérprete de órdenes de Hispa-DOS"; do
  if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
done
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
