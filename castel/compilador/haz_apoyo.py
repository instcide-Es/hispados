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
    ";  multiplicar, dividir y desplazar en 32 bits; y las de los programas de",
    ";  32 bits (modelo núcleo, hispa32 y usuario), con su arranque. NO editar a mano: lo escribe",
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
# las de 32 bits: las de escribir y las de cada modelo (núcleo y hispa32)
NUCLEO = dict(ccastel.APOYO32)
NUCLEO.update(ccastel.APOYO_NUCLEO)
HISPA32 = dict(ccastel.APOYO32)
HISPA32.update(ccastel.APOYO_HISPA32)
USUARIO = dict(ccastel.APOYO32)
USUARIO.update(ccastel.APOYO_USUARIO)
lineas.append("    público constante N_APOYO_NÚCLEO = %d" % len(NUCLEO))
lineas.append("    público constante N_APOYO_HISPA32 = %d" % len(HISPA32))
lineas.append("    público constante N_APOYO_USUARIO = %d" % len(USUARIO))
for letra, tabla in (('N', NUCLEO), ('H', HISPA32), ('U', USUARIO)):
    for i, (nombre, txt) in enumerate(tabla.items()):
        lineas.append('    constante %sN_%d = "%s"' % (letra, i, nombre))
        lineas.append('    constante %sT_%d = "%s"' % (letra, i, literal(txt.strip('\n') + '\n')))
# y el arranque de 32 bits (lo que va delante del programa, y la tabla de lo
# que importa un programa hispa32)
for nombre, txt in ccastel.ARRANQUE32.items():
    clave = {'núcleo': 'NUCLEO', 'núcleo2': 'NUCLEO2', 'hispa32': 'HISPA32', 'hispa32_2': 'HISPA32_2',
             'hispa32_imp': 'HISPA32_IMP', 'usuario': 'USUARIO', 'usuario2': 'USUARIO2'}[nombre]
    lineas.append('    público constante ARRANQUE_%s = "%s"' % (clave, literal(txt.strip('\n') + '\n')))
lineas += [
    "",
    "    variable nombres_n: arreglo [N_APOYO_NÚCLEO] de puntero a byte = (%s)" % ', '.join('NN_%d' % i for i in range(len(NUCLEO))),
    "    variable textos_n: arreglo [N_APOYO_NÚCLEO] de puntero a byte = (%s)" % ', '.join('NT_%d' % i for i in range(len(NUCLEO))),
    "    variable nombres_h: arreglo [N_APOYO_HISPA32] de puntero a byte = (%s)" % ', '.join('HN_%d' % i for i in range(len(HISPA32))),
    "    variable textos_h: arreglo [N_APOYO_HISPA32] de puntero a byte = (%s)" % ', '.join('HT_%d' % i for i in range(len(HISPA32))),
    "    variable nombres_u: arreglo [N_APOYO_USUARIO] de puntero a byte = (%s)" % ', '.join('UN_%d' % i for i in range(len(USUARIO))),
    "    variable textos_u: arreglo [N_APOYO_USUARIO] de puntero a byte = (%s)" % ', '.join('UT_%d' % i for i in range(len(USUARIO))),
    "",
    "    ; las de 32 bits: las del modelo núcleo (núcleo = cierto), las del usuario",
    "    ; (usuario = cierto) o las del hispa32",
    "    público función apoyo32_nombre(núcleo: bool, usuario: bool, i: nat16): puntero a byte",
    "        si núcleo entonces devolver nombres_n[i] fin",
    "        si usuario entonces devolver nombres_u[i] fin",
    "        devolver nombres_h[i]",
    "    fin",
    "",
    "    público función apoyo32_texto(núcleo: bool, usuario: bool, i: nat16): puntero a byte",
    "        si núcleo entonces devolver textos_n[i] fin",
    "        si usuario entonces devolver textos_u[i] fin",
    "        devolver textos_h[i]",
    "    fin",
]
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

# [CASTEL64] las de 64 bits van aparte, en compilador/32/apoyo64.cas (el módulo
# Apoyo64, que solo importa la parte de atrás de 64 bits de verdad,
# compilador/32/gen64.cas): el CASTEL de 16 bits no las lleva (CASTEL64.md)
LINUX64 = dict(ccastel.APOYO64)
LINUX64.update(ccastel.APOYO_LINUX64)
NUCLEO64 = dict(ccastel.APOYO64)
NUCLEO64.update(ccastel.APOYO_NUCLEO64)
HISPA64 = dict(ccastel.APOYO64)
HISPA64.update(ccastel.APOYO_HISPA64)
l64 = [
    "; =============================================================================",
    ";  apoyo64.cas - [CASTEL64] las rutinas de apoyo de los programas de 64 bits",
    ";  (modelo linux64, núcleo64 y hispa64, CASTEL64.md) y su arranque. NO editar a mano: lo",
    ";  escribe compilador/haz_apoyo.py con el texto de APOYO64, APOYO_LINUX64,",
    ";  APOYO_NUCLEO64, APOYO_HISPA64 y ARRANQUE64 de ccastel.py, para que los dos compiladores",
    ";  pongan lo mismo, letra a letra.",
    ";  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.",
    "; =============================================================================",
    "módulo Apoyo64",
    "",
    "    público constante N_APOYO_LINUX64 = %d" % len(LINUX64),
    "    público constante N_APOYO_NÚCLEO64 = %d" % len(NUCLEO64),
    "    público constante N_APOYO_HISPA64 = %d" % len(HISPA64),
]
for letra, tabla in (('L', LINUX64), ('N', NUCLEO64), ('H', HISPA64)):
    for i, (nombre, txt) in enumerate(tabla.items()):
        l64.append('    constante %sN_%d = "%s"' % (letra, i, nombre))
        l64.append('    constante %sT_%d = "%s"' % (letra, i, literal(txt.strip('\n') + '\n')))
for nombre, txt in ccastel.ARRANQUE64.items():
    clave = {'linux64': 'LINUX64', 'linux64_datos': 'LINUX64_DATOS', 'núcleo64': 'NUCLEO64',
             'núcleo64_datos': 'NUCLEO64_DATOS', 'hispa64': 'HISPA64',
             'hispa64_2': 'HISPA64_2', 'hispa64_datos': 'HISPA64_DATOS'}[nombre]
    l64.append('    público constante ARRANQUE_%s = "%s"' % (clave, literal(txt.strip('\n') + '\n')))
l64 += [
    "",
    "    variable nombres_l: arreglo [N_APOYO_LINUX64] de puntero a byte = (%s)" % ', '.join('LN_%d' % i for i in range(len(LINUX64))),
    "    variable textos_l: arreglo [N_APOYO_LINUX64] de puntero a byte = (%s)" % ', '.join('LT_%d' % i for i in range(len(LINUX64))),
    "    variable nombres_n: arreglo [N_APOYO_NÚCLEO64] de puntero a byte = (%s)" % ', '.join('NN_%d' % i for i in range(len(NUCLEO64))),
    "    variable textos_n: arreglo [N_APOYO_NÚCLEO64] de puntero a byte = (%s)" % ', '.join('NT_%d' % i for i in range(len(NUCLEO64))),
    "    variable nombres_h: arreglo [N_APOYO_HISPA64] de puntero a byte = (%s)" % ', '.join('HN_%d' % i for i in range(len(HISPA64))),
    "    variable textos_h: arreglo [N_APOYO_HISPA64] de puntero a byte = (%s)" % ', '.join('HT_%d' % i for i in range(len(HISPA64))),
    "",
    "    ; las de linux64 (cual = 1), las de hispa64 (cual = 2) o las de núcleo64 (cual = 0)",
    "    público función apoyo64_nombre(cual: nat16, i: nat16): puntero a byte",
    "        si cual = 1 entonces devolver nombres_l[i] fin",
    "        si cual = 2 entonces devolver nombres_h[i] fin",
    "        devolver nombres_n[i]",
    "    fin",
    "",
    "    público función apoyo64_texto(cual: nat16, i: nat16): puntero a byte",
    "        si cual = 1 entonces devolver textos_l[i] fin",
    "        si cual = 2 entonces devolver textos_h[i] fin",
    "        devolver textos_n[i]",
    "    fin",
    "",
    "fin Apoyo64.",
    "",
]
with open(os.path.join(AQUI, '32', 'apoyo64.cas'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(l64))
