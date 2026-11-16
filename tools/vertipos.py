#!/usr/bin/env python3
"""Enseña los tipos de letra de un TIPOS*.DAT: escribe una imagen PPM con una
línea de muestra de cada uno (para repasarlos sin arrancar el sistema).

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

uso: vertipos.py TIPOS.DAT salida.ppm ["texto de muestra"]
"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
texto = (sys.argv[3] if len(sys.argv) > 3 else 'El veloz murciélago hindú comía feliz. ¿Ñ? 0123456789').encode('cp437')
n, alto = d[8], d[9]
AN = 640 * alto // 12
px = [[1] * AN for _ in range(n * (alto + 2) + 2)]
for k in range(n):
    off, tam = struct.unpack_from('<II', d, 12 + k * 32 + 24)
    if not tam:
        continue
    nom = d[12 + k * 32:12 + k * 32 + 24].split(b'\0')[0]
    x = 2
    for ch in nom + b': ' + texto:
        o = struct.unpack_from('<H', d, off + (ch - 32) * 2)[0]
        if not o:
            x += alto // 2
            continue
        w, bp = d[off + o], d[off + o + 1]
        for y in range(alto):
            for i in range(bp * 8):
                if d[off + o + 2 + y * bp + i // 8] & (0x80 >> (i % 8)) and x + i < AN:
                    px[k * (alto + 2) + 2 + y][x + i] = 0
        x += w
with open(sys.argv[2], 'wb') as f:
    f.write(b'P5 %d %d 255\n' % (AN, len(px)))
    for fila in px:
        f.write(bytes(255 * v for v in fila))
