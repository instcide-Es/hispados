#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_fuentes.py DEJAVU SALIDA.c: las letras DejaVu (terceros/dejavu,
# recortadas a Europa occidental; su licencia, la de Bitstream Vera) dentro
# de Hispa-PDF, para pintar lo que no trae su letra. C portable (también
# para Hispa-64).
import os
import sys

d, sal = sys.argv[1], sys.argv[2]
nombres = ['SANS', 'SERIF', 'MONO', 'SANSN', 'SERIFN', 'MONON']
with open(sal, 'w') as f:
    f.write('/* hecho por haz_fuentes.py: las DejaVu (terceros/dejavu) */\n')
    f.write('#include "hpdf.h"\n')
    for n in nombres:
        b = open(os.path.join(d, n + '.TTF'), 'rb').read()
        f.write('static const unsigned char f_%s[%d] = {\n' % (n.lower(), len(b)))
        for i in range(0, len(b), 24):
            f.write(','.join(str(x) for x in b[i:i + 24]) + ',\n')
        f.write('};\n')
    f.write('void hpdf_fuentes(void)\n{\n\tpinta_fuentes(')
    f.write(', '.join('f_%s, sizeof f_%s' % (n.lower(), n.lower()) for n in nombres))
    f.write(');\n}\n')
