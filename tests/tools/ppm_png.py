#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Pasa un PPM (P6) a PNG, sin bibliotecas de fuera, para verlo; con un
número detrás, ampliado ese número de veces.   uso: ppm_png.py X.ppm X.png [N]"""
import re, struct, sys, zlib


def lee_ppm(ruta):
    d = open(ruta, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]


def png(ruta, w, h, rgb):
    def trozo(t, datos):
        c = t + datos
        return struct.pack('>I', len(datos)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    filas = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    with open(ruta, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(trozo(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(trozo(b'IDAT', zlib.compress(filas, 9)))
        f.write(trozo(b'IEND', b''))


if __name__ == '__main__':
    w, h, d = lee_ppm(sys.argv[1])
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    if n > 1:
        out = bytearray()
        for y in range(h):
            fila = b''.join(d[(y * w + x) * 3:(y * w + x) * 3 + 3] * n for x in range(w))
            out += fila * n
        d, w, h = bytes(out), w * n, h * n
    png(sys.argv[2], w, h, d)
