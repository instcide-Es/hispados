#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# orto_compara.py ARNES_C [CUANTAS]: el corrector de C (oficina/comun/orto.c,
# por su arnés pluma/pruebas/orto_arnes.c) contra el de Hispa-386
# (programas/comun/ortografia.inc, ensamblado como en
# tests/tools/ortografia_prueba.py): las mismas órdenes a los dos y las
# mismas respuestas. Formas del diccionario (tal cual, Primera, MAYÚSCULAS),
# estropeadas, inventadas y las faltas de la prueba de 386, con sus
# propuestas. Dice «N OK / M FAIL».
import os
import random
import subprocess
import sys
import tempfile

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
sys.path.insert(0, os.path.join(RAIZ, 'tests', 'tools'))
sys.path.insert(0, os.path.join(RAIZ, 'tools'))
import ortografia_prueba as op  # noqa: E402
import hazdiccio  # noqa: E402

arnes_c = os.path.abspath(sys.argv[1])
cuantas = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
random.seed(20261122)
DAT = os.path.join(RAIZ, 'programas', 'comun', 'DICCIO.DAT')
datos = open(DAT, 'rb').read()
formas = hazdiccio.lee(datos)
formas = sorted(formas) if not isinstance(formas, list) else formas
LET = b'abcdefghijklmnopqrstuvwxyz\xa4\xa0\x82\xa1\xa2\xa3'


def estropea(p):
    p = bytearray(p)
    i = random.randrange(len(p))
    k = random.randrange(4)
    if k == 0:
        p[i] = random.choice(LET)
    elif k == 1 and len(p) > 2:
        del p[i]
    elif k == 2:
        p.insert(i, random.choice(LET))
    elif i + 1 < len(p):
        p[i], p[i + 1] = p[i + 1], p[i]
    return bytes(p)


muestra = random.sample(formas, min(cuantas, len(formas)))
ordenes = []
for w in muestra:
    ordenes.append(b'P ' + w)
    ordenes.append(b'P ' + op.mayus(w))
    ordenes.append(b'P ' + op.mayus(w[:1]) + w[1:])
    ordenes.append(b'P ' + estropea(w))
for w in muestra[:cuantas // 4]:
    ordenes.append(b'S ' + estropea(w))
for w in muestra[:cuantas // 20]:
    ordenes.append(b'S ' + op.mayus(estropea(w)))
for f, _, _ in op.FALTAS:
    ordenes.append(b'S ' + op.cp(f))
    ordenes.append(b'P ' + op.cp(f))

with tempfile.TemporaryDirectory() as tmp:
    asm = op.hacer(tmp)
    open(os.path.join(tmp, 'A:\\PROGS\\DICCIO.DAT'), 'wb').write(datos)
    ent = b'\n'.join([b'D A:\\PROGS\\PLUMA.HVP'] + ordenes) + b'\n'
    r1 = subprocess.run([asm], input=ent, capture_output=True, cwd=tmp, timeout=900).stdout.split(b'\n')
    ent2 = b'\n'.join([b'D ' + DAT.encode()] + ordenes) + b'\n'
    r2 = subprocess.run([arnes_c], input=ent2, capture_output=True, timeout=900).stdout.split(b'\n')
ok = mal = 0
vistos = 0
for o, a, b in zip(ordenes, r1[1:], r2[1:]):
    a = a.replace(b'!', b'')
    if a == b:
        ok += 1
    else:
        mal += 1
        if vistos < 15:
            vistos += 1
            print('  distinto: %s -> 386 %s / C %s' % (o.decode('cp437'), a.decode('cp437'), b.decode('cp437')))
print('carga: 386 %s, C %s' % (r1[0].decode(), r2[0].decode()))
print('%d órdenes: %d iguales, %d distintas' % (len(ordenes), ok, mal))
print('%d OK / %d FAIL' % (ok + (r1[0] == r2[0] == b'H1'), mal + (r1[0] != b'H1' or r2[0] != b'H1')))
sys.exit(1 if mal else 0)
