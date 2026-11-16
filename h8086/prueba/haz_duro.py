#!/usr/bin/env python3
"""Hace una imagen de disco duro de prueba para Hispa-8086: un disco de 20
megas (40 cilindros, 16 cabezas, 63 sectores) con una sola partición FAT16
que empieza en el sector 63, como las hacía FDISK.

uso: haz_duro.py IMAGEN.img
"""
import struct, subprocess, sys

img = sys.argv[1]
CIL, CAB, SEC = 40, 16, 63
total = CIL * CAB * SEC
ini = SEC
tam = total - ini
with open(img, 'wb') as f:
    f.truncate(total * 512)
subprocess.run(['mformat', '-i', '%s@@%d' % (img, ini * 512), '-T', str(tam), '-h', str(CAB), '-s', str(SEC),
                '-H', str(ini), '-v', 'DURO', '::'], check=True)


def chs(lba):
    c, r = divmod(lba, CAB * SEC)
    h, s = divmod(r, SEC)
    return bytes([h, (s + 1) | ((c >> 8) << 6), c & 255])


mbr = bytearray(512)
mbr[0x1BE:0x1CE] = bytes([0x80]) + chs(ini) + bytes([0x04]) + chs(total - 1) + struct.pack('<II', ini, tam)
mbr[510:] = b'\x55\xaa'
with open(img, 'r+b') as f:
    f.write(mbr)
