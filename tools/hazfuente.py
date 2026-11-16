#!/usr/bin/env python3
"""Hace un tipo de letra de Pluma a partir de una fuente libre de contornos.

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

Dibuja cada carácter de la página 437 (del 32 al 255) en una casilla de 16
filas, en blanco y negro, y escribe un .inc de NASM: por carácter, 17 palabras
(el ancho en puntos y las 16 filas; el punto de la izquierda es el bit 15).
Ancho 0: la fuente no trae ese carácter y Pluma pone el de la letra de Hispa
Ventanas (los de dibujar recuadros, por ejemplo).

Solo hace falta para REHACER los .inc (ya están en programas/pluma/); pide
Pillow (python3 -m pip install pillow).

uso: hazfuente.py fuente.ttf tamaño etiqueta "origen y licencia" salida.inc
"""
import sys
from PIL import Image, ImageDraw, ImageFont

ttf, tam, etiqueta, origen, salida = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
BASE = 12                               # la fila de la línea base
f = ImageFont.truetype(ttf, tam)
falta = f.getmask('￿', mode='1').getbbox()     # cómo dibuja «no lo tengo»
faltab = bytes(f.getmask('￿', mode='1'))
lin = ['; %s: tipo de letra de Pluma, hecho con tools/hazfuente.py' % etiqueta,
       '; Origen: %s' % origen,
       '; Por carácter (del 32 al 255): el ancho y sus 16 filas.', '%s:' % etiqueta]
cortados = []
for c in range(32, 256):
    ch = bytes([c]).decode('cp437')
    ancho, filas = 0, [0] * 16
    m = f.getmask(ch, mode='1')
    if c == 32 or c == 255 or not (m.getbbox() == falta and bytes(m) == faltab):
        ancho = max(1, min(16, round(f.getlength(ch))))
        im = Image.new('1', (24, 24), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((0, BASE), ch, font=f, fill=1, anchor='ls')
        for y in range(16):
            for x in range(16):
                if im.getpixel((x, y)):
                    filas[y] |= 0x8000 >> x
        if any(im.getpixel((x, y)) for x in range(24) for y in range(16, 24)):
            cortados.append(ch)
    lin.append('        dw %2d, %s' % (ancho, ', '.join('0x%04X' % v for v in filas)))
open(salida, 'w').write('\n'.join(lin) + '\n')
if cortados:
    print('%s: cortados por abajo: %s' % (etiqueta, ' '.join(cortados)))
