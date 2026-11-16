#!/usr/bin/env python3
"""Enseña los tipos de letra de TIPOS.DAT: escribe una imagen PPM con una
línea de muestra de cada uno (para repasarlos sin arrancar el sistema).

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

uso: vertipos.py TIPOS.DAT salida.ppm ["texto de muestra"]
"""
import struct, sys
d = open(sys.argv[1], 'rb').read()
texto = (sys.argv[3] if len(sys.argv) > 3 else 'El veloz murciélago hindú comía feliz cardillo y kiwi. ¿Ñ? 0123456789').encode('cp437')
n = struct.unpack_from('<I', d, 8)[0]
AN, ESC = 640, 2
px = [[1] * AN for _ in range(n * 18 + 2)]
for k in range(n):
    off = struct.unpack_from('<I', d, 12 + k * 32 + 24)[0]
    nom = d[12 + k * 32:12 + k * 32 + 24].split(b'\0')[0]
    x = 2
    for ch in nom + b': ' + texto:
        o = struct.unpack_from('<H', d, off + (ch - 32) * 2)[0]
        if not o:
            x += 8
            continue
        w, bp = d[off + o], d[off + o + 1]
        for y in range(16):
            v = d[off + o + 2 + y] << 8 if bp == 1 else struct.unpack_from('<H', d, off + o + 2 + y * 2)[0]
            for i in range(16):
                if v & (0x8000 >> i) and x + i < AN:
                    px[k * 18 + 2 + y][x + i] = 0
        x += w
with open(sys.argv[2], 'wb') as f:
    f.write(b'P5 %d %d 255\n' % (AN * ESC, len(px) * ESC))
    for fila in px:
        l = bytes(255 * v for v in fila for _ in range(ESC))
        f.write(l * ESC)
