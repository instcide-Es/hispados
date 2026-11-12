#!/usr/bin/env python3
"""Crea una imagen FAT vacía (sin tabla de particiones) con un número exacto
de clústeres, para probar los límites entre FAT12 y FAT16.

uso: mkfat.py IMG CLUSTERES [SECTORES_POR_CLUSTER]
  menos de 4085 clústeres -> FAT12; 4085 o más -> FAT16
"""
import struct, sys

img, nclus = sys.argv[1], int(sys.argv[2])
spc = int(sys.argv[3]) if len(sys.argv) > 3 else 1
fat16 = nclus >= 4085
rootent = 224
rootsecs = rootent * 32 // 512
fatbytes = (nclus + 2) * 2 if fat16 else ((nclus + 2) * 3 + 1) // 2
spf = (fatbytes + 511) // 512
total = 1 + 2 * spf + rootsecs + nclus * spc
boot = bytearray(512)
boot[0:3] = b'\xEB\x3C\x90'
boot[3:11] = b'HISPADOS'
struct.pack_into('<HBHBHHBHHHII', boot, 11, 512, spc, 1, 2, rootent,
                 total if total < 65536 else 0, 0xF8, spf, 63, 16, 0,
                 total if total >= 65536 else 0)
boot[36] = 0x80
boot[38] = 0x29
struct.pack_into('<I', boot, 39, 0x12345678)
boot[43:54] = b'NO NAME    '
boot[54:62] = b'FAT16   ' if fat16 else b'FAT12   '
boot[510:512] = b'\x55\xAA'
fat = bytearray(spf * 512)
fat[0:4 if fat16 else 3] = b'\xF8\xFF\xFF\xFF' if fat16 else b'\xF8\xFF\xFF'
with open(img, 'wb') as f:
    f.write(boot)
    f.write(fat)
    f.write(fat)
    f.write(bytes((rootsecs + nclus * spc) * 512))
    # QEMU redondea la geometría hacia abajo: relleno hasta un cilindro entero
    pad = -f.tell() % (16 * 63 * 512)
    f.write(bytes(pad))
print(f"{img}: {nclus} clústeres, {'FAT16' if fat16 else 'FAT12'}, {total} sectores")
