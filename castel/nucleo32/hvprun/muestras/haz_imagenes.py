#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""(OFICINA2) muestras/haz_imagenes.py CARPETA

Las imágenes de muestra de la parte 40 de castel/pruebas.sh (la oficina 1.1
en HVPRUN), hechas aquí sin nada de fuera:
  FOTO.BMP   160 x 100, 24 bits (un degradado con cuadros)
  FOTO.PCX   lo mismo en PCX de 8 bits con su paleta del final (RLE)
  COLOR.PCX  lo mismo en PCX de color verdadero (tres planos de 8 bits)
  EGA.PCX    96 x 64, cuatro planos de un bit (16 colores, la paleta de la
             cabecera): cuatro franjas de colores
Para que Pintura las abra («Editar» en Mi equipo) y el Mirador las vea. Al
lado, documentos hechos por los programas en HVPRUN (la parte 40 con
OFICINA_MUESTRAS): CARTEL.PUB, FIESTA.PRE, DATOS.BDD y .BDJ, PAGINA2.TXT.
"""
import os
import struct
import sys

AN, AL = 160, 100


def punto(x, y):
    return (255 * x // AN, 255 * y // AL, 200 if (x // 20 + y // 20) % 2 else 60)


def bmp(ruta):
    filas = [b''.join(bytes(punto(x, y)[::-1]) for x in range(AN)) for y in range(AL)]
    datos = b''.join(reversed(filas))
    cab = b'BM' + struct.pack('<IHHI', 54 + len(datos), 0, 0, 54) + \
        struct.pack('<IiiHHIIiiII', 40, AN, AL, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
    open(ruta, 'wb').write(cab + datos)


def rle(fila):
    """las tiradas del PCX de una fila (de todos sus planos juntos)"""
    sal = bytearray()
    i = 0
    while i < len(fila):
        v = fila[i]
        n = 1
        while i + n < len(fila) and fila[i + n] == v and n < 63:
            n += 1
        if n > 1 or v >= 0xC0:
            sal += bytes([0xC0 | n, v])
        else:
            sal.append(v)
        i += n
    return bytes(sal)


def cabecera(an, al, bits, planos, opl, paleta16=b''):
    c = struct.pack('<BBBBHHHHHH', 10, 5, 1, bits, 0, 0, an - 1, al - 1, 72, 72)
    c += paleta16.ljust(48, b'\0') + b'\0' + bytes([planos]) + struct.pack('<HH', opl, 1)
    return c.ljust(128, b'\0')


def pcx8(ruta):
    # la paleta: 6 x 6 x 6 (como el cubo de Ventanas) y lo que se acerca
    pal = [(r * 51, g * 51, b * 51) for r in range(6) for g in range(6) for b in range(6)]
    pal += [(0, 0, 0)] * (256 - len(pal))

    def ind(c):
        return min(range(216), key=lambda i: sum((a - b) ** 2 for a, b in zip(pal[i], c)))
    cache = {}
    datos = bytearray()
    for y in range(AL):
        fila = bytearray()
        for x in range(AN):
            c = punto(x, y)
            if c not in cache:
                cache[c] = ind(c)
            fila.append(cache[c])
        datos += rle(bytes(fila))
    open(ruta, 'wb').write(cabecera(AN, AL, 8, 1, AN) + bytes(datos) + b'\x0c' + b''.join(bytes(c) for c in pal))


def pcx24(ruta):
    datos = bytearray()
    for y in range(AL):
        planos = [bytes(punto(x, y)[k] for x in range(AN)) for k in range(3)]
        datos += rle(b''.join(planos))
    open(ruta, 'wb').write(cabecera(AN, AL, 8, 3, AN) + bytes(datos))


def ega(ruta):
    an, al = 96, 64
    colores = [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170), (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
               (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255), (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255)]
    franjas = [12, 10, 9, 14]               # rojo, verde, azul y amarillo claros
    opl = an // 8
    datos = bytearray()
    for y in range(al):
        ind = [franjas[x * 4 // an] for x in range(an)]
        fila = bytearray()
        for p in range(4):
            for b in range(opl):
                v = 0
                for k in range(8):
                    v = (v << 1) | ((ind[b * 8 + k] >> p) & 1)
                fila.append(v)
        datos += rle(bytes(fila))
    open(ruta, 'wb').write(cabecera(an, al, 1, 4, opl, b''.join(bytes(c) for c in colores)) + bytes(datos))


if __name__ == '__main__':
    d = sys.argv[1]
    os.makedirs(d, exist_ok=True)
    bmp(os.path.join(d, 'FOTO.BMP'))
    pcx8(os.path.join(d, 'FOTO.PCX'))
    pcx24(os.path.join(d, 'COLOR.PCX'))
    ega(os.path.join(d, 'EGA.PCX'))
