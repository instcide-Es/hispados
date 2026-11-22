#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compara.py VALORES ESPERADO: ¿sale en VALORES (lo de «CUENTAS --valores»)
# todo lo de ESPERADO? Las líneas «[Hoja]» dicen de qué hoja es lo que
# sigue; las demás, «A1=lo que se ve» (con «\n», un salto de línea). Dice
# lo que falta y acaba con 1 si falta algo.
import sys
v = open(sys.argv[1], encoding='utf-8', errors='replace').read()
# las secciones de cada hoja
sec, cur = {}, None
for parte in v.split('\n['):
    if not parte.startswith('['):
        parte = '[' + parte
    if ']' not in parte:
        continue
    nom, resto = parte[1:].split(']', 1)
    sec[nom] = resto + '\n'
mal = 0
hoja = None
for l in open(sys.argv[2], encoding='utf-8'):
    l = l.rstrip('\n')
    if not l:
        continue
    if l.startswith('[') and l.endswith(']'):
        hoja = l[1:-1]
        continue
    e = l.replace('\\n', '\n')
    t = sec.get(hoja, '') if hoja is not None else v
    if ('\n' + e + '\n') not in ('\n' + t):
        k = e.split('=', 1)[0]
        hay = [x for x in t.split('\n') if x.startswith(k + '=')]
        print('falta [%s] %s (hay: %s)' % (hoja, repr(e), repr(hay[:1])))
        mal = 1
sys.exit(mal)
