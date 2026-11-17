#!/usr/bin/env python3
# mk1252.py - escribe la tabla de ventanas/win32.inc que pasa las letras de
# Windows (página 1252, de 80h a FFh) a las de Hispa Ventanas (página 437).
# Lo que no existe en la 437 sale como la letra sin acento o como «?».
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   python3 tools/mk1252.py      (la tabla sale por la salida, para pegarla)
BASE = {0xC0: 'A', 0xC1: 'A', 0xC2: 'A', 0xC3: 'A', 0xC8: 'E', 0xCA: 'E', 0xCB: 'E',
        0xCC: 'I', 0xCD: 'I', 0xCE: 'I', 0xCF: 'I', 0xD2: 'O', 0xD3: 'O', 0xD4: 'O',
        0xD5: 'O', 0xD8: 'O', 0xD9: 'U', 0xDA: 'U', 0xDB: 'U', 0xDD: 'Y', 0xE3: 'a',
        0xF5: 'o', 0xF8: 'o', 0xFD: 'y', 0x8A: 'S', 0x9A: 's', 0x8E: 'Z', 0x9E: 'z',
        0x80: 'E', 0xA9: 'c', 0xAE: 'R', 0xD7: 'x', 0x91: "'", 0x92: "'", 0x93: '"',
        0x94: '"', 0x96: '-', 0x97: '-', 0x85: '.'}
t = []
for b in range(0x80, 0x100):
    try:
        o = bytes([b]).decode('cp1252').encode('cp437')[0]
    except (UnicodeDecodeError, UnicodeEncodeError):
        o = ord(BASE.get(b, '?'))
    t.append(o)
for i in range(0, 128, 16):
    print('        db ' + ', '.join('0x%02X' % x for x in t[i:i + 16]))
