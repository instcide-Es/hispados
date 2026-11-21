#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# castel/nucleo32/escudo/prueba/compara.py ESPERADO.TXT SALIDA
# Compara lo que ha dicho el motor (líneas «...ARCHIVO[ > dentro]: NOMBRE
# ENCONTRADO» o «...ARCHIVO: limpio», de escudo-anfitrion o de ESCUDO /LISTA
# en Hispa-Pentium) con lo que tenía que decir (hazprueba.py). Dice cada
# fallo y acaba con 0 si todo cuadra.
import re
import sys

esperado = {}
for l in open(sys.argv[1], encoding='utf-8'):
    l = l.split()
    if len(l) == 2:
        esperado[l[0].upper()] = l[1]
dicho = {}
for l in open(sys.argv[2], encoding='utf-8', errors='replace'):
    l = l.strip().replace('\\', '/')
    m = re.match(r'^(.*?)(?: > .*)?: (\S+) ENCONTRADO$', l)
    if m:
        dicho[m.group(1).split('/')[-1].upper()] = m.group(2)
        continue
    m = re.match(r'^(.*?): limpio', l)
    if m:
        dicho[m.group(1).split('/')[-1].upper()] = 'limpio'
mal = 0
for k, v in sorted(esperado.items()):
    d = dicho.get(k)
    if d != v:
        print('  %s: tenía que ser «%s» y es «%s»' % (k, v, d))
        mal += 1
print('compara.py: %d de %d bien' % (len(esperado) - mal, len(esperado)))
sys.exit(1 if mal else 0)
