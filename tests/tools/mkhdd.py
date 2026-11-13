#!/usr/bin/env python3
"""Crea una imagen de disco duro con una partición FAT16 (tabla de particiones
incluida) y copia dentro carpetas del anfitrión, para probar programas grandes.

uso: mkhdd.py IMG MEGAS [CARPETA ...]      (cada carpeta va a la raíz de C:)
Necesita mtools.
"""
import os, struct, subprocess, sys

img, megas, dirs = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
H, S = 16, 63
cyls = megas * 1024 * 1024 // (H * S * 512)
total = cyls * H * S
start = S
size = total - start
with open(img, 'wb') as f:
    f.truncate(total * 512)
    mbr = bytearray(512)
    def chs(lba):
        c, r = divmod(lba, H * S)
        h, s = divmod(r, S)
        c = min(c, 1023)
        return bytes([h, (s + 1) | ((c >> 2) & 0xC0), c & 0xFF])
    ent = bytes([0x80]) + chs(start) + bytes([0x06]) + chs(total - 1) + struct.pack('<II', start, size)
    mbr[446:462] = ent
    mbr[510:512] = b'\x55\xAA'
    f.write(mbr)
spec = '%s@@%d' % (img, start * 512)
subprocess.check_call(['mformat', '-i', spec, '-t', str(cyls), '-h', str(H), '-s', str(S),
                       '-H', str(start), '-v', 'PRUEBAS', '::'])
for d in dirs:
    subprocess.check_call(['mcopy', '-s', '-i', spec, d.rstrip('/'), '::'])
