#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""compat/hazicono.py SALIDA.png
(COMPAT) El icono del Asistente de compatibilidad (32 x 32, RGBA): una
ventana de programa (marco oscuro, barra azul) con una lupa encima y, en la
lupa, una marca ✓: «mirar si va». Con borde oscuro, para que se vea sobre el
tapiz claro y sobre el oscuro. Se hizo una vez (compat32.png); este guion
queda para rehacerlo."""
import struct, sys, zlib
AN = 32
p = [[(0, 0, 0, 0)] * AN for _ in range(AN)]


def pon(x, y, c):
    if 0 <= x < AN and 0 <= y < AN:
        p[y][x] = c


BORDE, AZUL, BLANCO, GRIS, CRISTAL, VERDE, MANGO = (30, 38, 52, 255), (47, 111, 219, 255), (250, 252, 255, 255), (170, 182, 198, 255), \
    (214, 236, 250, 255), (31, 122, 58, 255), (120, 72, 30, 255)
# la ventana (de 2,3 a 23,22)
for y in range(3, 23):
    for x in range(2, 24):
        borde = x in (2, 23) or y in (3, 22)
        pon(x, y, BORDE if borde else AZUL if y < 8 else BLANCO)
for y in (11, 14, 17):
    for x in range(5, 18):
        pon(x, y, GRIS)
# la lupa: un círculo (centro 20,19; radio 8) y el mango
for y in range(AN):
    for x in range(AN):
        d = (x - 20) ** 2 + (y - 19) ** 2
        if d <= 64:
            pon(x, y, BORDE if d > 42 else CRISTAL)
for k in range(6):
    for g in range(-1, 2):
        pon(26 + k + g, 25 + k, MANGO)
        pon(26 + k, 25 + k + g, MANGO)
# la marca ✓ en la lupa
for x, y in ((16, 19), (17, 20), (18, 21), (19, 20), (20, 19), (21, 18), (22, 17), (23, 16)):
    for g in (0, 1):
        pon(x, y + g, VERDE)
raw = b''.join(b'\0' + b''.join(bytes(c) for c in fila) for fila in p)


def trozo(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)


png = b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', AN, AN, 8, 6, 0, 0, 0)) + trozo(b'IDAT', zlib.compress(raw, 9)) + trozo(b'IEND', b'')
open(sys.argv[1], 'wb').write(png)
