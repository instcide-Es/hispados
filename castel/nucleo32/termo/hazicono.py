#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""termo/hazicono.py SALIDA.png
(TERMO) El icono del Termómetro (32 x 32, RGBA): un termómetro de cristal
con el depósito y la columna en rojo, las marcas de la escala a la derecha y
el borde oscuro (se ve sobre el tapiz claro y sobre el oscuro). Se hizo una
vez (termo32.png); este guion queda para rehacerlo."""
import struct, sys, zlib
AN = 32
p = [[(0, 0, 0, 0)] * AN for _ in range(AN)]
def pon(x, y, c):
    if 0 <= x < AN and 0 <= y < AN:
        p[y][x] = c
BORDE, CRISTAL, ROJO, ROJO2, MARCA = (40, 44, 52, 255), (236, 240, 245, 255), (200, 30, 40, 255), (240, 90, 80, 255), (60, 70, 90, 255)
cx = 13
# el tubo (de y=3 a y=22) y el depósito (círculo en y=25)
for y in range(3, 23):
    for x in range(cx - 3, cx + 4):
        pon(x, y, BORDE if x in (cx - 3, cx + 3) else CRISTAL)
for x in range(cx - 2, cx + 3):
    pon(x, 2, BORDE)
for y in range(18, 32):
    for x in range(AN):
        d = (x - cx) ** 2 + (y - 25) ** 2
        if d <= 36:
            pon(x, y, BORDE if d > 25 else ROJO)
# la columna roja, con su brillo
for y in range(9, 22):
    for x in range(cx - 1, cx + 2):
        pon(x, y, ROJO)
    pon(cx - 1, y, ROJO2)
pon(cx - 2, 24, ROJO2); pon(cx - 2, 23, ROJO2)
# la escala
for i, y in enumerate(range(5, 22, 3)):
    for x in range(cx + 5, cx + (11 if i % 2 == 0 else 9)):
        pon(x, y, MARCA)
raw = b''.join(b'\0' + b''.join(bytes(c) for c in fila) for fila in p)
def trozo(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
png = b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', AN, AN, 8, 6, 0, 0, 0)) + trozo(b'IDAT', zlib.compress(raw, 9)) + trozo(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
