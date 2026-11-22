#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_letras.py DEJAVU SALIDA.c: las letras DejaVu (terceros/dejavu,
# recortadas a Europa occidental; su licencia, la de Bitstream Vera) dentro
# de los programas de la oficina de 32 bits (oficina/comun/letras.c). C
# portable (también para Hispa-64). Como pdf/haz_fuentes.py.
import os
import sys

d, sal = sys.argv[1], sys.argv[2]
nombres = ['SANS', 'SANSN', 'SERIF', 'SERIFN', 'MONO', 'MONON']
with open(sal, 'w') as f:
    f.write('/* hecho por oficina/comun/haz_letras.py: las DejaVu (terceros/dejavu) */\n')
    f.write('#include "letras.h"\n')
    for n in nombres:
        b = open(os.path.join(d, n + '.TTF'), 'rb').read()
        f.write('static const unsigned char f_%s[%d] = {\n' % (n.lower(), len(b)))
        for i in range(0, len(b), 24):
            f.write(','.join(str(x) for x in b[i:i + 24]) + ',\n')
        f.write('};\n')
    f.write('void letras_dejavu(void)\n{\n')
    for n in nombres:
        f.write('\tletras_mete(f_%s, sizeof f_%s, 0);\n' % (n.lower(), n.lower()))
    f.write('}\n')
