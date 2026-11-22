#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_rtf.py SALIDA.RTF: un RTF de prueba con lo que hacen Word y LibreOffice
# (tablas de letras, colores, estilos y listas; encabezado y pie con los
# campos PAGE y NUMPAGES; formato de letra y de párrafo; \uN, también fuera
# del plano básico; viñetas y números; tabulación con puntos; una imagen PNG
# en \shppict con su \nonshppict; una tabla con una celda unida a lo alto,
# otra a lo ancho (más ancha) y fondo; un salto de página).
import struct
import sys
import zlib


def png(w, h):
    raw = b''.join(b'\x00' + b''.join(bytes([(x * 8) % 256, (y * 8) % 256, 160]) for x in range(w)) for y in range(h))

    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')


p = png(32, 24).hex()
rtf = r'''{\rtf1\ansi\ansicpg1252\deff0\uc1
{\fonttbl{\f0\froman\fcharset0 Times New Roman;}{\f1\fswiss\fcharset0 Arial;}{\f2\fmodern Courier New;}}
{\colortbl;\red192\green0\blue0;\red255\green255\blue0;\red221\green235\blue247;}
{\stylesheet{\s0 Normal;}{\s1\b\fs32 heading 1;}{\s2 heading 2;}}
{\*\listtable{\list\listtemplateid1{\listlevel\levelnfc23{\leveltext\'01舦 ?;}}\listid100}{\list{\listlevel\levelnfc0{\leveltext\'02\'00.;}}\listid200}}
{\*\listoverridetable{\listoverride\listid100\listoverridecount0\ls1}{\listoverride\listid200\listoverridecount0\ls2}}
{\info{\title Prueba de RTF}{\author Carlos Sanz}}
\paperw11906\paperh16838\margl1418\margr1418\margt1418\margb1418\headery709\footery709
{\header\pard\qc\plain\f1\fs18 Encabezado de prueba\par}
{\footer\pard\qr\plain\f1\fs18 P\'e1gina {\field{\*\fldinst PAGE}{\fldrslt 1}} de {\field{\*\fldinst NUMPAGES}{\fldrslt 9}}\par}
\pard\s1\sb240\sa120\keepn\plain\f1\fs32\b T\'edtulo principal\par
\pard\qj\sa120\plain\f0\fs24 Un p\'e1rrafo con {\b negrita}, {\i cursiva}, {\ul subrayado}, {\strike tachado}, {\cf1 rojo}, {\highlight2 resaltado}, x{\super 2} y H{\sub 2}O; \u241?and\u250? \'80 (un cohete: \u-10179?\u-8576?).\line Otra l\'ednea\tab con tabulaci\'f3n.\par
\pard\qc\plain Centrado\par
\pard\ls1\ilvl0\li720\fi-360\plain {\listtext\'95\tab}Primera vi\'f1eta\par
{\listtext\'95\tab}Segunda vi\'f1eta\par
\pard\ls2\li720\fi-360\plain {\listtext 1.\tab}Uno\par
{\listtext 2.\tab}Dos\par
\pard\tqr\tldot\tx8000\plain Precio\tab 12,50\par
\pard\plain {\*\shppict{\pict\pngblip\picw32\pich24\picwgoal960\pichgoal720
''' + p + r'''}}{\nonshppict{\pict\wmetafile8 0102}}\par
\trowd\trgaph100\trleft0\clbrdrt\brdrs\brdrw10\clcbpat3\cellx3000\clbrdrt\brdrs\cellx6000\cellx9000
\pard\intbl\plain A1\cell B1\cell C1\cell\row
\trowd\trgaph100\clvmgf\cellx3000\cellx9000
\pard\intbl\plain A2 vertical\cell B2 ancha\cell\row
\trowd\trgaph100\clvmrg\cellx3000\cellx6000\cellx9000
\pard\intbl\plain \cell B3\cell C3\par otra l\'ednea\cell\row
\pard\plain Despu\'e9s de la tabla.\par
\page
\pard\plain Segunda p\'e1gina.\par
}'''
open(sys.argv[1], 'w').write(rtf)
