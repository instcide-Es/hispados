#!/bin/bash
# FAT32: escribir más allá de 4 GB - 1. Un GRANDE.BIN de FFFFFE00h octetos
# (la cadena hecha aquí, sin escribir los datos: el disco es disperso) y
# 1024 octetos escritos desde el final: solo caben 511, el tamaño queda en
# FFFFFFFFh y el principio del archivo no se toca (antes la posición daba
# la vuelta y se escribía al principio).
# Writes past 4 GB - 1 on FAT32.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
nasm -f bin -o $T/F4GB.COM tests/progs/f4gb.asm || exit 1
cp "$IMG" $T/f.img
mcopy -o -i $T/f.img $T/F4GB.COM ::
truncate -s 5G $T/c.img
echo 'start=2048, type=c' | /usr/sbin/sfdisk -q $T/c.img
mkfs.fat -F 32 -s 64 -n GRANDE --offset 2048 $T/c.img $(( (5*1024*2048 - 2048) / 2 )) > /dev/null 2>&1
python3 - $T/c.img <<'PY'
import struct, sys
f = open(sys.argv[1], 'r+b'); P = 2048 * 512
f.seek(P); b = f.read(512)
bps, spc, res, nf = struct.unpack_from('<HBHB', b, 11)
spf, root = struct.unpack_from('<I', b, 0x24)[0], struct.unpack_from('<I', b, 0x2C)[0]
size = 0xFFFFFE00; cb = bps * spc; n = (size + cb - 1) // cb
first = root + 1
fat = bytearray()
for i in range(n):
    fat += struct.pack('<I', first + i + 1 if i < n - 1 else 0x0FFFFFFF)
for k in range(nf):
    f.seek(P + (res + k * spf) * bps + first * 4); f.write(fat)
data = res + nf * spf
f.seek(P + (data + (root - 2) * spc) * bps)
ent = b'GRANDE  BIN' + bytes([0x20]) + bytes(8) + struct.pack('<H', first >> 16) + bytes(4) + struct.pack('<HI', first & 0xFFFF, size)
f.write(ent)
fs = struct.unpack_from('<H', b, 0x30)[0]          # FSInfo: libres y siguiente, sin saber
f.seek(P + fs * bps + 488); f.write(struct.pack('<II', 0xFFFFFFFF, 0xFFFFFFFF))
PY
{ sleep 5; printf 'F4GB\r'; sleep 8; printf 'ECHO fin-f4gb\r'; sleep 2; } |
  timeout 40 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
    -drive file=$T/f.img,format=raw,if=floppy -drive file=$T/c.img,format=raw,if=ide -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
echo "== Hispa-DOS FAT32: escribir más allá de 4 GB - 1 =="
for want in "F4GB: escritos 511 (hasta 4 GB - 1)" "F4GB: el principio sigue a cero" "F4GB: fin" "fin-f4gb"; do
  if grep -qF -- "$want" $T/log.txt; then echo "  OK   $want"; else echo "  FAIL $want"; fail=1; fi
done
sz=$(python3 - $T/c.img <<'PY'
import struct, sys
f = open(sys.argv[1], 'rb'); P = 2048 * 512
f.seek(P); b = f.read(512)
bps, spc, res, nf = struct.unpack_from('<HBHB', b, 11)
spf, root = struct.unpack_from('<I', b, 0x24)[0], struct.unpack_from('<I', b, 0x2C)[0]
f.seek(P + (res + nf * spf + (root - 2) * spc) * bps); d = f.read(spc * bps)
for i in range(0, len(d), 32):
    if d[i:i+11] == b'GRANDE  BIN': print(struct.unpack_from('<I', d, i + 28)[0]); break
PY
)
if [ "$sz" = "4294967295" ]; then echo "  OK   GRANDE.BIN mide 4 GB - 1"; else echo "  FAIL GRANDE.BIN mide «$sz»"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
exit $fail
