#!/usr/bin/env python3
"""Pone el sector de arranque de Hispa-8086 en una imagen de disquete ya
formateada, conservando su BPB (las medidas del disco, octetos 11 a 61).

uso: pon_arranque.py ARRANQUE.bin IMAGEN.img

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import sys

arr = bytearray(open(sys.argv[1], 'rb').read())
assert len(arr) == 512, 'el sector de arranque no mide 512 octetos'
assert arr[510:] == b'\x55\xaa', 'el sector de arranque no acaba en la firma 55AAh'
with open(sys.argv[2], 'r+b') as f:
    viejo = f.read(512)
    arr[11:62] = viejo[11:62]
    f.seek(0)
    f.write(arr)
