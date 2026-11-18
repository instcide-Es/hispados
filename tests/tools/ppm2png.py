#!/usr/bin/env python3
"""Pasa una captura PPM de QEMU (P6) a PNG, sin bibliotecas de fuera: para
guardar las capturas de las pruebas (tests/run_navegador2_tests.sh).
uso: ppm2png.py CAPTURA.ppm SALIDA.png
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import struct, sys, zlib

def lee(n):
    d = open(n, 'rb').read()
    toks, i = [], 0
    while len(toks) < 4:
        while d[i:i + 1].isspace(): i += 1
        j = i
        while not d[j:j + 1].isspace(): j += 1
        toks.append(d[i:j]); i = j
    i += 1
    w, h = int(toks[1]), int(toks[2])
    return w, h, d[i:i + w * h * 3]

def png(n, w, h, px):
    raw = b''.join(b'\0' + px[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def ch(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    open(n, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                        + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))

w, h, px = lee(sys.argv[1])
png(sys.argv[2], w, h, px)
