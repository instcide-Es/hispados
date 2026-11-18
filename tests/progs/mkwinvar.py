#!/usr/bin/env python3
# mkwinvar.py - hace los dibujos de los recursos de WINVAR.EXE (fase 3d de
# Hispa-486): winvar.ico (32x32, 16 colores: un círculo rojo con borde azul y
# lo de fuera transparente), winvar.cur (32x32, monocromo: un cuadrado negro
# con borde blanco, punto caliente en su esquina) y winvar.bmp (32x16, 256
# colores: franjas verdes y amarillas). Se guardan al lado (en tests/progs).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import os, struct
AQUI = os.path.dirname(os.path.abspath(__file__))


def bih(an, al, bpp, ncol):
    return struct.pack('<IiiHHIIiiII', 40, an, al, 1, bpp, 0, 0, 0, 0, ncol, 0)


def mascara(filas_transp, an=32):
    """filas de arriba abajo (listas de bool: True = transparente) -> 1 bit,
    de abajo arriba, filas de 4 octetos"""
    d = b''
    for f in reversed(filas_transp):
        v = 0
        for x, t in enumerate(f):
            if t:
                v |= 1 << (31 - x)
        d += struct.pack('>I', v)
    return d


# el icono
col = [(0, 0, 0), (255, 0, 0), (0, 0, 255), (255, 255, 255)] + [(0, 0, 0)] * 12
pix, transp = [], []
for y in range(32):
    fila, tr = [], []
    for x in range(32):
        r2 = (x - 15.5) ** 2 + (y - 15.5) ** 2
        if r2 <= 11 ** 2:
            fila.append(1)
        elif r2 <= 14 ** 2:
            fila.append(2)
        else:
            fila.append(0)
        tr.append(r2 > 14 ** 2)
    pix.append(fila)
    transp.append(tr)
xor = b''
for f in reversed(pix):
    xor += bytes((f[i] << 4) | f[i + 1] for i in range(0, 32, 2))
img = bih(32, 64, 4, 16) + b''.join(bytes((b, g, r, 0)) for r, g, b in col) + xor + mascara(transp)
ico = struct.pack('<HHH', 0, 1, 1) + struct.pack('<BBBBHHII', 32, 32, 16, 0, 1, 4, len(img), 22) + img
open(os.path.join(AQUI, 'winvar.ico'), 'wb').write(ico)

# el cursor: negro (AND 0, XOR 0), blanco (AND 0, XOR 1), nada (AND 1, XOR 0)
xorf, tr = [], []
for y in range(32):
    fx, ft = [], []
    for x in range(32):
        dentro = x < 16 and y < 16
        borde = dentro and (x in (0, 15) or y in (0, 15))
        fx.append(borde)
        ft.append(not dentro)
    xorf.append(fx)
    tr.append(ft)
img = bih(32, 64, 1, 2) + bytes((0, 0, 0, 0, 255, 255, 255, 0)) + mascara(xorf) + mascara(tr)
cur = struct.pack('<HHH', 0, 2, 1) + struct.pack('<BBBBHHII', 32, 32, 0, 0, 0, 0, len(img), 22) + img
open(os.path.join(AQUI, 'winvar.cur'), 'wb').write(cur)

# el mapa: 32 x 16, 8 bits, franjas de 4 (verde, amarillo)
cols = [(0, 255, 0), (255, 255, 0)]
pal = b''.join(bytes((b, g, r, 0)) for r, g, b in cols)
puntos = b''.join(bytes(((x // 4) % 2) for x in range(32)) for y in range(16))
dib = bih(32, 16, 8, 2) + pal + puntos
bmp = b'BM' + struct.pack('<IHHI', 14 + len(dib), 0, 0, 14 + 40 + len(pal)) + dib
open(os.path.join(AQUI, 'winvar.bmp'), 'wb').write(bmp)
