#!/usr/bin/env python3
# haz_muestras.py - Pintura 2: los PCX e ICO de prueba que ImageMagick no hace (pruebas/formatos.sh).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#   python3 -I haz_muestras.py CARPETA
# PCX de 16 colores (un plano de 4 bits y cuatro planos de 1 bit), de 1 bit,
# de 8 bits sin comprimir, con octetos de sobra por fila; ICO y CUR con un BMP
# de 1, 4, 8 y 24 bits dentro y su máscara AND; y uno con varias imágenes.
import struct
import sys

C = sys.argv[1]
PAL16 = [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170), (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
         (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255), (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255)]


def ind(x, y, n):
    return (x // 3 + y // 2 + (x * y) // 11) % n


def rle(fila):
    s = bytearray()
    i = 0
    while i < len(fila):
        v = fila[i]
        k = 1
        while i + k < len(fila) and fila[i + k] == v and k < 63:
            k += 1
        if k > 1 or v >= 0xC0:
            s.append(0xC0 | k)
        s.append(v)
        i += k
    return s


def pcx(nombre, w, h, bits, planos, bpl, filas, pal16=None, pal256=None, comprime=True):
    c = bytearray(128)
    c[0] = 10
    c[1] = 5
    c[2] = 1 if comprime else 0
    c[3] = bits
    struct.pack_into('<HHHHHH', c, 4, 0, 0, w - 1, h - 1, 72, 72)
    if pal16:
        for i, (r, g, b) in enumerate(pal16[:16]):
            c[16 + 3 * i:19 + 3 * i] = bytes((r, g, b))
    c[65] = planos
    struct.pack_into('<HH', c, 66, bpl, 1)
    d = bytearray(c)
    for f in filas:
        assert len(f) == bpl * planos
        if comprime:
            for p in range(planos):
                d += rle(f[p * bpl:(p + 1) * bpl])
        else:
            d += f
    if pal256:
        d.append(12)
        for r, g, b in pal256:
            d += bytes((r, g, b))
    open(C + '/' + nombre, 'wb').write(d)


def bits_fila(vals, bits, bpl):
    f = bytearray(bpl)
    for x, v in enumerate(vals):
        b = x * bits
        f[b >> 3] |= v << (8 - (b & 7) - bits)
    return f


# PCX
w, h = 37, 23
filas = [bits_fila([ind(x, y, 16) for x in range(w)], 4, 20) for y in range(h)]
pcx('p_4bits.pcx', w, h, 4, 1, 20, filas, pal16=PAL16)
filas = []
for y in range(h):
    f = bytearray()
    for p in range(4):
        f += bits_fila([(ind(x, y, 16) >> p) & 1 for x in range(w)], 1, 6)
    filas.append(f)
pcx('p_4planos.pcx', w, h, 1, 4, 6, filas, pal16=PAL16)
filas = [bits_fila([ind(x, y, 2) for x in range(w)], 1, 8) for y in range(h)]
pcx('p_1bit.pcx', w, h, 1, 1, 8, filas, pal16=[(0, 0, 0), (255, 255, 255)])
pal = [((i * 37) & 255, (i * 91) & 255, i) for i in range(256)]
filas = [bytearray([ind(x, y, 256) for x in range(w)] + [0] * 3) for y in range(h)]
pcx('p_8crudo.pcx', w, h, 8, 1, 40, filas, pal256=pal, comprime=False)
filas = [bytearray([(x * 255 // w) & 255 for x in range(w)] + [0, 0, 0] +
                   [(y * 255 // h) for x in range(w)] + [0, 0, 0] +
                   [((x + y) * 7) & 255 for x in range(w)] + [0, 0, 0]) for y in range(h)]
pcx('p_24sobra.pcx', w, h, 8, 3, 40, filas)


# ICO y CUR con BMP dentro (de abajo arriba, doble alto, máscara AND)
def bmp_ico(w, h, bpp):
    n = 1 << bpp if bpp <= 8 else 0
    pal = [((i * 53) & 255, (i * 101) & 255, (i * 29 + 7) & 255) for i in range(n)]
    fila = ((w * bpp + 31) // 32) * 4
    mfila = ((w + 31) // 32) * 4
    xor = bytearray()
    andm = bytearray()
    for y in range(h - 1, -1, -1):
        if bpp <= 8:
            f = bits_fila([ind(x, y, n) for x in range(w)], bpp, fila)
        else:
            f = bytearray(fila)
            for x in range(w):
                f[3 * x:3 * x + 3] = bytes(((x * 9) & 255, (y * 13) & 255, ((x ^ y) * 5) & 255))
        xor += f
        andm += bits_fila([1 if (x - w // 2) ** 2 + (y - h // 2) ** 2 < (w // 4) ** 2 else 0 for x in range(w)], 1, mfila)
    cab = struct.pack('<IiiHHIIiiII', 40, w, 2 * h, 1, bpp, 0, 0, 0, 0, n, 0)
    p = b''.join(bytes((b, g, r, 0)) for r, g, b in pal)
    return cab + p + xor + andm


def ico(nombre, imgs, tipo=1):
    d = struct.pack('<HHH', 0, tipo, len(imgs))
    off = 6 + 16 * len(imgs)
    cuerpo = b''
    for w, h, bpp, datos in imgs:
        d += struct.pack('<BBBBHHII', w & 255, h & 255, (1 << bpp) & 255 if bpp < 8 else 0, 0,
                         1 if tipo == 1 else 3, bpp if tipo == 1 else 4, len(datos), off + len(cuerpo))
        cuerpo += datos
    open(C + '/' + nombre, 'wb').write(d + cuerpo)


for bpp in (1, 4, 8, 24):
    ico('i_bmp%d.ico' % bpp, [(32, 32, bpp, bmp_ico(32, 32, bpp))])
ico('i_cur8.cur', [(24, 24, 8, bmp_ico(24, 24, 8))], tipo=2)
ico('i_varias.ico', [(16, 16, 4, bmp_ico(16, 16, 4)), (48, 48, 8, bmp_ico(48, 48, 8)), (32, 32, 24, bmp_ico(32, 32, 24))])


# BMP con RLE8 y RLE4 (tiradas, trozos sueltos, saltos, fin de fila y de imagen)
def bmp_rle(nombre, w, h, bpp, cuerpo):
    n = 1 << bpp
    pal = b''.join(bytes((((i * 29 + 7) & 255), (i * 101) & 255, (i * 53) & 255, 0)) for i in range(n))
    off = 14 + 40 + len(pal)
    cab = struct.pack('<IiiHHIIiiII', 40, w, h, 1, bpp, 1 if bpp == 8 else 2, len(cuerpo), 2835, 2835, n, 0)
    d = b'BM' + struct.pack('<IHHI', off + len(cuerpo), 0, 0, off) + cab + pal + cuerpo
    open(C + '/' + nombre, 'wb').write(d)


w, h = 29, 13
c8 = bytearray()
c4 = bytearray()
for y in range(h):
    if y == 5:
        c8 += bytes((0, 2, 3, 1))	# un salto: 3 a la derecha y una fila arriba
        c4 += bytes((0, 2, 3, 1))
        continue
    if y == 6:
        c8 += bytes((5, 7, 0, 0))
        c4 += bytes((5, 0x7A, 0, 0))
        continue
    c8 += bytes((7, y + 1))				# una tirada
    c8 += bytes((0, 5)) + bytes(range(10, 15)) + b'\0'	# 5 sueltos (y el relleno)
    c8 += bytes((17, 200 - y)) + bytes((0, 0))
    c4 += bytes((7, 0x12 + y))
    c4 += bytes((0, 5, 0x34, 0x56, 0x70, 0))
    c4 += bytes((17, 0xCD)) + bytes((0, 0))
c8 += bytes((0, 1))
c4 += bytes((0, 1))
bmp_rle('b_rle8.bmp', w, h, 8, bytes(c8))
bmp_rle('b_rle4.bmp', w, h, 4, bytes(c4))
