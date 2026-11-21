#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/win32/hazdatos.py SALIDA.h: los datos de WIN32.BIN que salen de
otros sitios, para no copiarlos a mano: la letra de 8x16 de Hispa-Pentium
(nucleo32/letra.cas, la de Hispa Ventanas) y las tablas de la página 1252 a
la del núcleo (la 437 con las letras de más de tools/hispa437.py) y al revés,
como las de tools/mk1252.py."""
import os, re, sys
AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', '..', '..', 'tools'))
from hispa437 import codigo, letra

BASE = {0xD8: 'O', 0xDD: 'Y', 0xF8: 'o', 0xFD: 'y', 0x8A: 'S', 0x9A: 's', 0x8E: 'Z',
        0x9E: 'z', 0x9F: 'Y', 0xD7: 'x', 0x91: "'", 0x92: "'", 0x93: '"', 0x94: '"',
        0x96: '-', 0x97: '-', 0x82: ',', 0x84: '"', 0x86: '+', 0x87: '+', 0x88: '^',
        0x89: '%', 0x8B: '<', 0x9B: '>', 0x98: '~', 0x95: 0xFA, 0xA6: '|', 0xA8: '"',
        0xAF: '-', 0xB3: '3', 0xB4: "'", 0xB8: ',', 0xB9: '1', 0xD0: 'D', 0xF0: 'd'}
a437 = []
for b in range(0x80, 0x100):
    try:
        o = codigo(bytes([b]).decode('cp1252'))
    except UnicodeDecodeError:
        o = None
    if o is None:
        v = BASE.get(b, '?')
        o = v if isinstance(v, int) else ord(v)
    a437.append(o)
a1252 = []
for b in range(0x80, 0x100):
    try:
        o = letra(b).encode('cp1252')[0]
    except UnicodeEncodeError:
        o = ord('?')
    a1252.append(o)
filas = []
for l in open(os.path.join(AQUI, '..', 'letra.cas'), encoding='utf-8'):
    m = re.match(r'\s+db\s+(0x[0-9A-Fa-f]{2}(,0x[0-9A-Fa-f]{2}){15})\s*$', l)
    if m:
        filas.append(m.group(1))
assert len(filas) == 256, len(filas)
with open(sys.argv[1], 'w') as f:
    f.write('/* datos.h - NO SE EDITA: lo hace nucleo32/win32/hazdatos.py */\n')
    f.write('const u8 t1252_437[128] = {%s};\n' % ','.join('0x%02X' % x for x in a437))
    f.write('const u8 t437_1252[128] = {%s};\n' % ','.join('0x%02X' % x for x in a1252))
    f.write('const u8 letra8x16[256 * 16] = {\n%s\n};\n' % ',\n'.join(filas))
