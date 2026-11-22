#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/agenda/icono.py SALIDA.ICP [SALIDA.PNG]

(AGENDA) El icono de la Agenda (32 x 32, HPIC: 0xAARRGGBB), dibujado aquí:
una hoja de calendario con las dos anillas, la franja de arriba (roja),
la cuadrícula de los días y uno marcado con un recuadro grueso (hoy) y un
lápiz: no es el del Reloj (una esfera). Con un segundo argumento, también
en PNG (para mirarlo).
"""
import struct
import sys
import zlib

L = 32


def lienzo():
    return [[0] * L for _ in range(L)]


def rect(p, x, y, an, al, c):
    for j in range(y, y + al):
        for i in range(x, x + an):
            if 0 <= i < L and 0 <= j < L:
                p[j][i] = 0xFF000000 | c


def icono():
    p = lienzo()
    rect(p, 2, 4, 26, 26, 0x374151)          # el borde de la hoja
    rect(p, 3, 5, 24, 24, 0xFFFFFF)
    rect(p, 3, 5, 24, 6, 0xB91C1C)           # la franja de arriba
    for x in (8, 20):                         # las anillas
        rect(p, x, 2, 3, 6, 0x111827)
        rect(p, x + 1, 3, 1, 4, 0x9CA3AF)
    for f in range(4):                        # los días
        for c in range(5):
            rect(p, 5 + c * 4, 13 + f * 4, 3, 3, 0xCBD5E1)
    # hoy: con un recuadro grueso (no solo el color)
    rect(p, 12, 16, 7, 7, 0x111827)
    rect(p, 14, 18, 3, 3, 0x1D4ED8)
    # el lápiz, en diagonal, abajo a la derecha
    for k in range(9):
        rect(p, 22 + k // 2 - 4 + k // 2, 30 - k - 2, 2, 2, 0xD97706)
    rect(p, 18, 28, 2, 2, 0x111827)
    return p


def guarda_icp(nombre, p):
    with open(nombre, 'wb') as f:
        f.write(b'HPIC' + struct.pack('<II', L, L))
        for fila in p:
            f.write(struct.pack('<%dI' % L, *fila))


def guarda_png(nombre, p):
    crudo = b''.join(b'\0' + b''.join(struct.pack('>I', ((v & 0xFFFFFF) << 8) | (v >> 24)) for v in fila) for fila in p)

    def trozo(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(nombre, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', L, L, 8, 6, 0, 0, 0))
                + trozo(b'IDAT', zlib.compress(crudo)) + trozo(b'IEND', b''))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    i = icono()
    guarda_icp(sys.argv[1], i)
    if len(sys.argv) > 2:
        guarda_png(sys.argv[2], i)
