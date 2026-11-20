#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""tools/hpicono.py ENTRADA.png SALIDA.ICP

Hace el icono de un acceso del menú Inicio de Hispa-Pentium (un .ICP, el que
dice el «icono=» de un .MNU: castel/nucleo32/accesos.cas) de un PNG de 32 x 32
puntos (de 8 bits por color, RGB o RGBA, sin entrelazar). El .ICP es la magia
«HPIC», el ancho y el alto (32 y 32, en dos palabras de 32 bits, la baja
primero) y los 32 x 32 puntos en 0xAARRGGBB, también la palabra baja primero
(A, la opacidad: de 0x80 para arriba se pinta; lo demás es transparente).
Sin bibliotecas de fuera: el PNG se lee con zlib, que viene con Python.
"""
import struct
import sys
import zlib

LADO = 32


def lee_png(nombre):
    d = open(nombre, 'rb').read()
    if d[:8] != b'\x89PNG\r\n\x1a\n':
        sys.exit('hpicono: %s no es un PNG' % nombre)
    i = 8
    datos = b''
    ancho = alto = tipo = None
    while i < len(d):
        n, clase = struct.unpack('>I4s', d[i:i + 8])
        trozo = d[i + 8:i + 8 + n]
        i += 12 + n
        if clase == b'IHDR':
            ancho, alto, bits, tipo, _, _, entrelazado = struct.unpack('>IIBBBBB', trozo)
            if bits != 8 or tipo not in (2, 6) or entrelazado:
                sys.exit('hpicono: el PNG tiene que ser RGB o RGBA de 8 bits y sin entrelazar')
        elif clase == b'IDAT':
            datos += trozo
        elif clase == b'IEND':
            break
    cada = 4 if tipo == 6 else 3
    crudo = zlib.decompress(datos)
    filas = []
    antes = bytearray(ancho * cada)
    p = 0
    for _ in range(alto):
        filtro = crudo[p]
        f = bytearray(crudo[p + 1:p + 1 + ancho * cada])
        p += 1 + ancho * cada
        for k in range(len(f)):
            a = f[k - cada] if k >= cada else 0
            b = antes[k]
            c = antes[k - cada] if k >= cada else 0
            if filtro == 1:
                f[k] = (f[k] + a) & 255
            elif filtro == 2:
                f[k] = (f[k] + b) & 255
            elif filtro == 3:
                f[k] = (f[k] + (a + b) // 2) & 255
            elif filtro == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                f[k] = (f[k] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        filas.append(f)
        antes = f
    return ancho, alto, cada, filas


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ancho, alto, cada, filas = lee_png(sys.argv[1])
    if (ancho, alto) != (LADO, LADO):
        sys.exit('hpicono: el PNG tiene que medir 32 x 32 (mide %d x %d)' % (ancho, alto))
    sal = bytearray(b'HPIC' + struct.pack('<II', LADO, LADO))
    for f in filas:
        for x in range(LADO):
            r, g, b = f[x * cada:x * cada + 3]
            a = f[x * cada + 3] if cada == 4 else 255
            sal += struct.pack('<I', (a << 24) | (r << 16) | (g << 8) | b)
    open(sys.argv[2], 'wb').write(sal)


if __name__ == '__main__':
    main()
