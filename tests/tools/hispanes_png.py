#!/usr/bin/env python3
"""Pasa una imagen de nesnativo (PGM con los colores de la consola, 0-63) a
PPM con la paleta de Hispa-Nes, para verla.   uso: hispanes_png.py X.pgm X.ppm
(sin argumentos de imagen, «-m» escribe un muestrario de los 64 colores)
"""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
from hispanes_paleta import PALETA


def lee_pgm(ruta):
    d = open(ruta, 'rb').read()
    m = re.match(rb'P5\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]


if __name__ == '__main__':
    if sys.argv[1] == '-m':
        w, h, d = 256, 64, bytes((y // 16) * 16 + x // 16 for y in range(64) for x in range(256))
        sal = sys.argv[2]
    else:
        w, h, d = lee_pgm(sys.argv[1])
        sal = sys.argv[2]
    with open(sal, 'wb') as f:
        f.write(b'P6\n%d %d\n255\n' % (w, h))
        f.write(b''.join(bytes(PALETA[c & 63]) for c in d[:w * h]))
