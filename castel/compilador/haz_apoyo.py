#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# haz_apoyo.py - escribe compilador/apoyo.cas (el módulo Apoyo del compilador
# de Castel escrito en Castel) a partir de las rutinas de apoyo de ccastel.py
# (APOYO): así los dos compiladores llevan exactamente el mismo texto. Hay que
# volver a ejecutarlo si se toca APOYO en ccastel.py:
#     python3 -I compilador/haz_apoyo.py
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(AQUI))
import ccastel


def literal(texto):
    """El texto como literal de Castel: lo que no es ASCII, octeto a octeto (UTF-8)."""
    sal = ''
    for b in texto.encode('utf-8'):
        if b == 10:
            sal += '\\n'
        elif b in (34, 39, 92):
            sal += '\\' + chr(b)
        elif 32 <= b < 127:
            sal += chr(b)
        else:
            sal += '\\x%02X' % b
    return sal


lineas = [
    "; =============================================================================",
    ";  apoyo.cas - las rutinas de apoyo en ensamblador que el compilador añade a",
    ";  los programas (solo las que se usan): escribir números y textos, y",
    ";  multiplicar, dividir y desplazar en 32 bits. NO editar a mano: lo escribe",
    ";  compilador/haz_apoyo.py con el texto de APOYO de ccastel.py, para que los",
    ";  dos compiladores pongan lo mismo, letra a letra.",
    ";  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.",
    "; =============================================================================",
    "módulo Apoyo",
    "",
    "    público constante N_APOYO = %d" % len(ccastel.APOYO),
    "",
]
for i, (nombre, txt) in enumerate(ccastel.APOYO.items()):
    lineas.append('    constante N_%d = "%s"' % (i, nombre))
    lineas.append('    constante T_%d = "%s"' % (i, literal(txt.strip('\n') + '\n')))
lineas += [
    "",
    "    variable nombres: arreglo [N_APOYO] de puntero a byte = (%s)" % ', '.join('N_%d' % i for i in range(len(ccastel.APOYO))),
    "    variable textos: arreglo [N_APOYO] de puntero a byte = (%s)" % ', '.join('T_%d' % i for i in range(len(ccastel.APOYO))),
    "",
    "    ; el nombre de la rutina número i (desde 0)",
    "    público función apoyo_nombre(i: nat16): puntero a byte",
    "        devolver nombres[i]",
    "    fin",
    "",
    "    ; su texto: sus líneas, cada una acabada en un salto de línea",
    "    público función apoyo_texto(i: nat16): puntero a byte",
    "        devolver textos[i]",
    "    fin",
    "",
    "fin Apoyo.",
    "",
]
with open(os.path.join(AQUI, 'apoyo.cas'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(lineas))
