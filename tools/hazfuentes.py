#!/usr/bin/env python3
"""Hace TIPOS.DAT, los tipos de letra que comparten los programas de Hispa
Ventanas (Pluma y los demás de «Productividad»), a partir de fuentes libres.

  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.

Dibuja cada letra de la página 437 en una casilla de 16 filas (la línea base,
en la fila 12), en blanco y negro. El archivo (lo lee programas/comun/tipos.inc):

  0   «TIPOS1», 26, 0
  8   dd  cuántos tipos
  12  de cada tipo, 32 octetos: su nombre (24, acabado en 0), dd dónde
      empieza en el archivo y dd cuánto ocupa
  y de cada tipo: 224 palabras (dónde está cada carácter, del 32 al 255,
  contando desde el principio del tipo; 0: no lo trae y se pone el de la letra
  de Hispa Ventanas) y, de cada carácter: su ancho, cuántos octetos lleva cada
  fila (1 o 2) y sus 16 filas (el punto de la izquierda, el bit más alto).

Los documentos guardan el NÚMERO del tipo (1, el primero de la tabla): los
tipos nuevos se añaden AL FINAL y los que hay no se cambian de sitio.

Solo hace falta para REHACER TIPOS.DAT. Pide Pillow (pip install pillow) y
las fuentes: las de DejaVu y Bitstream, en /usr/share/fonts de Debian/Ubuntu
(paquetes fonts-dejavu-core y xfonts-scalable); las demás, de los paquetes
fonts-* que dice la tabla, desempaquetados (dpkg -x) en la carpeta que se da.

uso: hazfuentes.py carpeta-de-los-paquetes TIPOS.DAT TIPOS-DE-LETRA.md
"""
import os, struct, sys
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
BASE = 12
LETRAS = list(range(32, 176)) + [225, 230, 241, 246, 248, 250, 253]


def dibuja(f, ch, alto=24):
    im = Image.new('1', (40, alto), 0)
    d = ImageDraw.Draw(im)
    d.fontmode = '1'
    d.text((4, BASE), ch, font=f, fill=1, anchor='ls')
    return im


def tamano(ruta):
    """el mayor en el que caben en la casilla las letras altas y las de rabo"""
    mejor = 8
    for t in range(8, 25):
        bb = dibuja(ImageFont.truetype(ruta, t), 'ÉÑdfhklgjpqy', 40).crop((0, 0, 40, 40)).getbbox()
        im = Image.new('1', (400, 48), 0)
        d = ImageDraw.Draw(im)
        d.fontmode = '1'
        d.text((4, 24), 'ÉÑdfhklgjpqy', font=ImageFont.truetype(ruta, t), fill=1, anchor='ls')
        bb = im.getbbox()
        if bb and bb[1] >= 24 - BASE and bb[3] <= 24 + 16 - BASE:
            mejor = t
    return mejor


blobs = []
doc = []
total = 0
for etq, nombre, ruta, origen, lic, *resto in TIPOS:
    t = resto[0] if resto else tamano(ruta)
    f = ImageFont.truetype(ruta, t)
    falta = bytes(f.getmask('￿', mode='1')), f.getmask('￿', mode='1').size
    tabla, datos, pos = [], [], 448
    for c in range(32, 256):
        ch = bytes([c]).decode('cp437')
        m = f.getmask(ch, mode='1')
        if c not in LETRAS or (c != 32 and (bytes(m), m.size) == falta):
            tabla.append(0)
            continue
        im = dibuja(f, ch)
        ancho = max(1, min(16, round(f.getlength(ch))))
        filas = []
        for y in range(16):
            v = 0
            for x in range(16):
                if im.getpixel((4 + x, y)):
                    v |= 0x8000 >> x
            filas.append(v)
        if any(v & 0xFF for v in filas):
            o = [ancho, 2] + [b for v in filas for b in (v & 0xFF, v >> 8)]
        else:
            o = [ancho, 1] + [v >> 8 for v in filas]
        tabla.append(pos)
        datos.append(o)
        pos += len(o)
    blobs.append(struct.pack('<224H', *tabla) + bytes(b for o in datos for b in o))
    total += pos
    doc.append((nombre, origen, t, lic, pos))
cab = b'TIPOS1\x1a\x00' + struct.pack('<I', len(TIPOS))
pos = len(cab) + 32 * len(TIPOS)
for (etq, nombre, *_r), b in zip(TIPOS, blobs):
    cab += nombre.encode('cp437').ljust(24, b'\0') + struct.pack('<II', pos, len(b))
    pos += len(b)
open(sys.argv[2], 'wb').write(cab + b''.join(blobs))
md = ['# Los tipos de letra de Hispa Ventanas', '',
      'Los programas de «Productividad» (Pluma, y los que vengan) comparten los tipos de',
      'letra del archivo `\\HISPADOS\\PROGS\\TIPOS.DAT` (en el repositorio,',
      '`programas/comun/TIPOS.DAT`; lo hace `tools/hazfuentes.py` y lo lee',
      '`programas/comun/tipos.inc`). El tipo 0 es «Hispa», la letra de ancho fijo de Hispa',
      'Ventanas, que no está en el archivo.', '',
      'Cada tipo sale de una fuente libre, dibujada en una casilla de 16 puntos de alto. Lleva',
      'un nombre nuestro porque las licencias piden que lo cambiado no use el nombre original.',
      'Cada uno conserva la licencia de su fuente de origen:', '',
      '| N.º | Tipo | Sale de | Puntos | Licencia | Octetos |', '|---|---|---|---|---|---|']
for i, d in enumerate(doc):
    md.append('| %d | %s | %s | %d | %s | %d |' % ((i + 1,) + d))
md += ['', 'Los documentos guardan el número del tipo: los nuevos se añaden al final de la tabla de',
       '`tools/hazfuentes.py` y los que hay no se cambian de sitio.', '']
open(sys.argv[3], 'w', encoding='utf-8').write('\n'.join(md))
print('%d tipos, %d octetos' % (len(TIPOS), pos))
