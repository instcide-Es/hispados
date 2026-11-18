#!/usr/bin/env python3
"""Hace los tipos de letra que comparten los programas de Hispa Ventanas
(Pluma y los demás de «Productividad»), a partir de fuentes libres.

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

Dibuja cada letra de la página 437 (la de Ventanas, con las letras de más de
tools/hispa437.py), en blanco y negro, en casillas de cuatro
altos: 12, 16, 24 y 32 puntos de pantalla (la línea base, a tres cuartos). De
cada alto sale un archivo: TIPOS12.DAT, TIPOS.DAT (el de 16), TIPOS24.DAT y
TIPOS32.DAT. Los lee programas/comun/tipos.inc. El archivo:

  0   «TIPOS2», 26, 0
  8   db cuántos tipos (con el 0), db alto de la casilla, db línea base, db 0
  12  de cada tipo, 32 octetos: su nombre (24, acabado en 0), dd dónde
      empieza en el archivo y dd cuánto ocupa (0: ese tipo no está aquí)
  y de cada tipo: 224 palabras (dónde está cada carácter, del 32 al 255,
  contando desde el principio del tipo; 0: no lo trae y se pone el de la letra
  de Hispa Ventanas) y, de cada carácter: su ancho, cuántos octetos lleva cada
  fila, y sus filas (tantas como el alto; el punto de la izquierda es el bit
  más alto del primer octeto).

El tipo 0 es «Hispa», la letra de ancho fijo de Hispa Ventanas (8x16): en el
archivo de 16 no está (se usa la del sistema); en los otros altos lo hace una
de ancho fijo de aquí. Los documentos guardan el NÚMERO del tipo: los tipos
nuevos se añaden AL FINAL y los que hay no se cambian de sitio.

Solo hace falta para REHACER los archivos. Pide Pillow (pip install pillow) y
las fuentes: las de DejaVu y Bitstream, en /usr/share/fonts de Debian/Ubuntu
(paquetes fonts-dejavu-core y xfonts-scalable); las demás, de los paquetes
fonts-* que dice la tabla, desempaquetados (dpkg -x) en la carpeta que se da.

uso: hazfuentes.py carpeta-de-los-paquetes carpeta-de-salida TIPOS-DE-LETRA.md
"""
import os, struct, sys
from hispa437 import EXTRA, letra   # la 437 de Ventanas: Á, Ó, €... (tools/hispa437.py)
from PIL import Image, ImageDraw, ImageFont

X = os.path.join(sys.argv[1], 'usr/share/fonts')
S = '/usr/share/fonts'
VERA = 'licencia Bitstream Vera (libre) y dominio público'
# etiqueta, nombre en Pluma, archivo, origen, licencia. El nombre es nuestro:
# las licencias piden que lo cambiado no lleve el nombre de la fuente original.
TIPOS = [
    ('roble',      'Roble',       S + '/truetype/dejavu/DejaVuSerif.ttf', 'DejaVu Serif', VERA),
    ('roble_n',    'Roble negra', S + '/truetype/dejavu/DejaVuSerif-Bold.ttf', 'DejaVu Serif Bold', VERA),
    ('imprenta',   'Imprenta',    X + '/opentype/linux-libertine/LinLibertine_R.otf', 'Linux Libertine (fonts-linuxlibertine)', 'GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1'),
    ('carta',      'Carta',       S + '/X11/Type1/c0648bt_.pfb', 'Bitstream Charter (xfonts-scalable)', 'licencia libre de Bitstream (1989-1992)'),
    ('clasica',    'Clásica',     X + '/truetype/fonts-oldstandard/OldStandard-Regular.ttf', 'Old Standard (fonts-oldstandard)', 'OFL-1.1'),
    ('elegante',   'Elegante',    X + '/opentype/didot/GFSDidot.otf', 'GFS Didot (fonts-gfs-didot)', 'OFL-1.1'),
    ('pergamino',  'Pergamino',   X + '/opentype/ebgaramond/EBGaramond12-Regular.otf', 'EB Garamond (fonts-ebgaramond)', 'OFL-1.1'),
    ('losa',       'Losa',        X + '/opentype/roboto/slab/RobotoSlab-Regular.otf', 'Roboto Slab (fonts-roboto-slab)', 'Apache-2.0'),
    ('llano',      'Llano',       S + '/truetype/dejavu/DejaVuSans.ttf', 'DejaVu Sans', VERA),
    ('llano_n',    'Llano negra', S + '/truetype/dejavu/DejaVuSans-Bold.ttf', 'DejaVu Sans Bold', VERA),
    ('claro',      'Claro',       X + '/truetype/open-sans/OpenSans-Regular.ttf', 'Open Sans (fonts-open-sans)', 'Apache-2.0'),
    ('pantalla',   'Pantalla',    X + '/opentype/play/Play-Regular.otf', 'Play (fonts-play)', 'OFL-1.1'),
    ('tecnica',    'Técnica',     X + '/opentype/jura/Jura-Regular.otf', 'Jura (fonts-jura)', 'OFL-1.1'),
    ('redonda',    'Redonda',     X + '/truetype/quicksand/Quicksand-Regular.ttf', 'Quicksand (fonts-quicksand)', 'OFL-1.1'),
    ('rotulo',     'Rótulo',      X + '/opentype/linux-libertine/LinBiolinum_R.otf', 'Linux Biolinum (fonts-linuxlibertine)', 'GPL-2.0 o posterior con excepción de fuentes, y OFL-1.1'),
    ('estrecha',   'Estrecha',    X + '/opentype/dosis/Dosis-Book.otf', 'Dosis (fonts-dosis)', 'OFL-1.1'),
    ('maquina',    'Máquina',     X + '/opentype/courier-prime/Courier Prime.otf', 'Courier Prime (fonts-courier-prime)', 'OFL-1.1'),
    # (las de trazo libre tienen rabos muy largos: se les da el tamaño a mano y
    # lo que se sale de la casilla se recorta)
    ('fiesta',     'Fiesta',      X + '/opentype/lobster/lobster.otf', 'Lobster (fonts-lobster)', 'OFL-1.1', 15),
    ('manuscrita', 'Manuscrita',  X + '/truetype/breip/Breip.ttf', 'Breip (fonts-breip)', 'OFL-1.0', 18),
]
HISPA = S + '/truetype/dejavu/DejaVuSansMono.ttf'      # «Hispa» en los altos que no son 16
ALTOS = (12, 16, 24, 32)
LETRAS = list(range(32, 176)) + [225, 230, 241, 246, 248, 250, 253] + sorted(EXTRA)
MARGEN = 16


def dibuja(f, ch, alto, base):
    im = Image.new('1', (MARGEN + 4 * alto, alto), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    d.text((MARGEN, base), ch, font=f, fill=1, anchor='ls')
    return im


def tamano(ruta, alto, base):
    """el mayor en el que caben en la casilla las letras altas y las de rabo"""
    mejor = 6
    for t in range(6, 3 * alto):
        im = Image.new('1', (40 * alto, 4 * alto), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((4, 2 * alto), 'ÉÑdfhklgjpqy', font=ImageFont.truetype(ruta, t), fill=1, anchor='ls')
        bb = im.getbbox()
        if bb and bb[1] >= 2 * alto - base and bb[3] <= 2 * alto + alto - base:
            mejor = t
    return mejor


def tipo(ruta, t, alto, base):
    f = ImageFont.truetype(ruta, t)
    falta = bytes(f.getmask('￿', mode='1')), f.getmask('￿', mode='1').size
    tabla, datos, pos = [], [], 448
    for c in range(32, 256):
        ch = letra(c)
        m = f.getmask(ch, mode='1')
        if c not in LETRAS or (c != 32 and (bytes(m), m.size) == falta):
            tabla.append(0)
            continue
        im = dibuja(f, ch, alto, base)
        ancho = max(1, min(255, round(f.getlength(ch))))
        bb = im.crop((MARGEN, 0, im.width, alto)).getbbox()
        puntos = max(ancho, bb[2] if bb else 0)
        bpr = min(8, (puntos + 7) // 8)
        o = [ancho, bpr]
        for y in range(alto):
            for b in range(bpr):
                v = 0
                for x in range(8):
                    if im.getpixel((MARGEN + b * 8 + x, y)):
                        v |= 0x80 >> x
                o.append(v)
        tabla.append(pos)
        datos.append(o)
        pos += len(o)
    return struct.pack('<224H', *tabla) + bytes(b for o in datos for b in o)


doc = [[t[1], t[3], t[4], 0] for t in TIPOS]
total = 0
for alto in ALTOS:
    base = alto * 3 // 4
    blobs = [b'' if alto == 16 else tipo(HISPA, tamano(HISPA, alto, base), alto, base)]
    for k, (etq, nombre, ruta, origen, lic, *resto) in enumerate(TIPOS):
        t = round(resto[0] * alto / 16) if resto else tamano(ruta, alto, base)
        blobs.append(tipo(ruta, t, alto, base))
        doc[k][3] += len(blobs[-1])
    cab = b'TIPOS2\x1a\x00' + bytes([len(blobs), alto, base, 0])
    pos = len(cab) + 32 * len(blobs)
    for nombre, b in zip(['Hispa'] + [t[1] for t in TIPOS], blobs):
        cab += nombre.encode('cp437').ljust(24, b'\0') + struct.pack('<II', pos if b else 0, len(b))
        pos += len(b)
    nom = 'TIPOS.DAT' if alto == 16 else 'TIPOS%d.DAT' % alto
    open(os.path.join(sys.argv[2], nom), 'wb').write(cab + b''.join(blobs))
    print('%s: %d octetos' % (nom, pos))
    total += pos
md = ['# Los tipos de letra de Hispa Ventanas', '',
      'Los programas de «Productividad» (Pluma, y los que vengan) comparten los tipos de',
      'letra de `\\HISPADOS\\PROGS`: `TIPOS.DAT` (letra normal, en casilla de 16 puntos de',
      'pantalla) y `TIPOS12.DAT`, `TIPOS24.DAT` y `TIPOS32.DAT` (los otros tamaños). En el',
      'repositorio están en `programas/comun/`; los hace `tools/hazfuentes.py` y los lee',
      '`programas/comun/tipos.inc`. El tipo 0 es «Hispa», la letra de ancho fijo de Hispa',
      'Ventanas; en los tamaños que no son el normal la hace DejaVu Sans Mono.', '',
      'Cada tipo sale de una fuente libre. Lleva un nombre nuestro porque las licencias piden',
      'que lo cambiado no use el nombre original, y conserva la licencia de su fuente de origen:', '',
      '| N.º | Tipo | Sale de | Licencia | Octetos (los cuatro tamaños) |', '|---|---|---|---|---|']
for i, d in enumerate(doc):
    md.append('| %d | %s | %s | %s | %d |' % ((i + 1,) + tuple(d)))
md += ['', 'Tamaños: 9, 12, 18 y 24 puntos son los dibujados (casillas de 12, 16, 24 y 32 puntos de',
       'pantalla); 36 y 48 son el doble de 18 y de 24.', '',
       'Los documentos guardan el número del tipo: los nuevos se añaden al final de la tabla de',
       '`tools/hazfuentes.py` y los que hay no se cambian de sitio.', '']
open(sys.argv[3], 'w', encoding='utf-8').write('\n'.join(md))
print('%d tipos, %d octetos en total' % (len(TIPOS) + 1, total))
