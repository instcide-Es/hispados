#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/win32/hazicono.py SALIDA.ICP: el icono del acceso de «Naves» en
el menú Inicio (un .ICP: castel/nucleo32/accesos.cas): la nave del juego
(w32/naves/naves.c, dib_nave) al doble, sobre un cuadro de noche con
estrellas."""
import struct, sys
NAVE = ["       aa       ", "       aa       ", "      abba      ", "      abba      ", "     abbbba     ",
        " r   abccba   r ", " r  abbccbba  r ", "aaa abbbbbba aaa", "abaabbbbbbbbaaba", "abbbbbbbbbbbbbba",
        "aabbbbaaaabbbbaa", " aaaa  oo  aaaa ", "       yy       ", "                ", "                ", "                "]
COLOR = {'a': 0xA0A8B8, 'b': 0xF4F6FA, 'c': 0x40D0F0, 'r': 0xE03828, 'o': 0xF08A28, 'y': 0xF8E040}
p = []
for y in range(32):
    for x in range(32):
        v = 0
        if 1 <= x <= 30 and 1 <= y <= 30:
            v = 0xFF101830
            if (x * 7 + y * 13) % 41 == 0:
                v = 0xFFC8D0E0
        c = NAVE[(y - 4) // 2][x // 2] if 4 <= y < 36 and (y - 4) // 2 < 16 else ' '
        if c in COLOR:
            v = 0xFF000000 | COLOR[c]
        p.append(v)
with open(sys.argv[1], 'wb') as f:
    f.write(b'HPIC' + struct.pack('<II', 32, 32) + struct.pack('<%dI' % len(p), *p))
