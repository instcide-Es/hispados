#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/juegos/prueba/a_png.py CAPTURA.ppm... (o -o SALIDA.png CAPTURA.ppm)

Pasa las capturas de QEMU (PPM, «screendump») a PNG, al lado (o a SALIDA),
para la documentación (docs/pentium/juegos/). Sin bibliotecas de fuera.
"""
import struct
import sys
import zlib


def a_png(ppm, png):
    d = open(ppm, 'rb').read()
    trozos = d.split(b'\n', 3)
    if trozos[0] != b'P6':
        sys.exit('a_png: %s no es un PPM (P6)' % ppm)
    w, h = map(int, trozos[1].split())
    px = trozos[3]
    crudo = b''.join(b'\x00' + px[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def trozo(t, x):
        return struct.pack('>I', len(x)) + t + x + struct.pack('>I', zlib.crc32(t + x) & 0xffffffff)
    with open(png, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                trozo(b'IDAT', zlib.compress(crudo, 9)) + trozo(b'IEND', b''))


def main():
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == '-o':
        a_png(a[2], a[1])
        return
    for n in a:
        a_png(n, n[:-4] + '.png' if n.endswith('.ppm') else n + '.png')


if __name__ == '__main__':
    main()
