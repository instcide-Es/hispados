#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""correo/haz_letra.py: hace correo/letra8x16.h, la letra del sistema de
Hispa-Pentium (la de nucleo32/letra.cas: 256 letras de 8x16, la 437 de Hispa)
para que Correo pinte el texto en su propio lienzo (lienzo.c) y vuelque la
ventana de una vez (sin parpadeo). La saca de lo mismo que hazletra.py.

uso: python3 -I castel/nucleo32/correo/haz_letra.py
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..'))
import hazletra  # noqa: E402

filas = hazletra.tabla()
s = ['/* letra8x16.h - la letra del sistema de Hispa-Pentium (la de letra.cas): 256',
     ' * letras de 8x16, una fila por octeto, el punto de la izquierda en el bit alto.',
     ' * NO SE EDITA: lo hace correo/haz_letra.py.',
     ' * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */',
     'static const unsigned char letra8x16[256][16] = {']
for f in filas:
    s.append('\t{ ' + ', '.join('0x%02X' % v for v in f) + ' },')
s.append('};')
open(os.path.join(AQUI, 'letra8x16.h'), 'w').write('\n'.join(s) + '\n')
