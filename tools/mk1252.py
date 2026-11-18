#!/usr/bin/env python3
# mk1252.py - escribe la tabla de ventanas/win32.inc que pasa las letras de
# Windows (página 1252, de 80h a FFh) a las de Hispa Ventanas (la 437 con las
# letras de más de tools/hispa437.py: Á, Ó, €...), y la contraria
# (ventanas/win32u.inc).
# Lo que no existe en Ventanas sale como la letra sin acento, como algo que se
# le parezca o como «?».
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   python3 tools/mk1252.py      (la tabla sale por la salida, para pegarla)
from hispa437 import codigo, letra

BASE = {0xD8: 'O', 0xDD: 'Y', 0xF8: 'o', 0xFD: 'y', 0x8A: 'S', 0x9A: 's', 0x8E: 'Z',
        0x9E: 'z', 0x9F: 'Y', 0xD7: 'x', 0x91: "'", 0x92: "'", 0x93: '"', 0x94: '"',
        0x96: '-', 0x97: '-', 0x82: ',', 0x84: '"', 0x86: '+', 0x87: '+', 0x88: '^',
        0x89: '%', 0x8B: '<', 0x9B: '>', 0x98: '~', 0x95: 0xFA, 0xA6: '|', 0xA8: '"',
        0xAF: '-', 0xB3: '3', 0xB4: "'", 0xB8: ',', 0xB9: '1', 0xD0: 'D', 0xF0: 'd'}
t = []
for b in range(0x80, 0x100):
    try:
        o = codigo(bytes([b]).decode('cp1252'))
    except UnicodeDecodeError:
        o = None
    if o is None:
        v = BASE.get(b, '?')
        o = v if isinstance(v, int) else ord(v)
    t.append(o)
for i in range(0, 128, 16):
    print('        db ' + ', '.join('0x%02X' % x for x in t[i:i + 16]))
# y la contraria (ventanas/win32u.inc, w32_437): de la de Ventanas a la 1252
print('w32_437:')
t = []
for b in range(0x80, 0x100):
    try:
        o = letra(b).encode('cp1252')[0]
    except UnicodeEncodeError:
        o = ord('?')
    t.append(o)
for i in range(0, 128, 16):
    print('        db ' + ', '.join('0x%02X' % x for x in t[i:i + 16]))
