#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""blocprueba/prueba_dif.py BIN CARPETA: prueba BlocDif (PDIF.BIN, con corre32)
contra Python: casos al azar (FUZZ_DIF vueltas, semilla fija): los trozos
cubren a y b en orden, convierten a en b, lo IGUAL es igual, y lo tocado es
lo mínimo (n + m - 2·LCS); el tope; y lo que tarda con los grandes."""
import os, random, struct, subprocess, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
BIN, D = sys.argv[1], sys.argv[2]
R = os.path.join(AQUI, '..', '..', 'corre32', 'corre32.sh')
bien = mal = 0


def ok(q):
    global bien
    bien += 1
    print('bien: ' + q)


def ko(q):
    global mal
    mal += 1
    print('MAL: ' + q)


def lcs(a, b):
    p = [0] * (len(b) + 1)
    for x in a:
        q = [0]
        for j, y in enumerate(b):
            q.append(p[j] + 1 if x == y else max(p[j + 1], q[j]))
        p = q
    return p[-1]


rnd = random.Random(20261122)
vueltas = int(os.environ.get('FUZZ_DIF', '1500'))
casos = [([], [], 0), ([1, 2, 3], [1, 2, 3], 0), ([1, 2, 3], [], 0), ([], [4, 5], 0), ([1, 2, 3], [4, 5, 6], 0),
         ([1, 2, 3, 4], [1, 9, 3, 4], 0), (list(range(50)), list(range(100, 150)), 10), (list(range(30)), list(range(30))[::-1], 0)]
for v in range(vueltas):
    alf = rnd.choice((2, 3, 5, 20, 1000))
    na, nb = rnd.randrange(60), rnd.randrange(60)
    if v % 4 == 0:
        a = [rnd.randrange(alf) for _ in range(na)]
        b = [x for x in a if rnd.random() > 0.2]
        for _ in range(rnd.randrange(6)):
            b.insert(rnd.randrange(len(b) + 1), rnd.randrange(alf))
    else:
        a = [rnd.randrange(alf) for _ in range(na)]
        b = [rnd.randrange(alf) for _ in range(nb)]
    casos.append((a, b, rnd.choice((0, 0, 0, rnd.randrange(1, 40)))))
with open(os.path.join(D, 'casos.bin'), 'wb') as f:
    for a, b, t in casos:
        f.write(struct.pack('<%dI' % (3 + len(a) + len(b)), len(a), len(b), *a, *b, t))
r = subprocess.run([R, BIN, 'CASOS', 'casos.bin', 'salida.bin'], cwd=D, capture_output=True).stdout.decode('cp437')
s = open(os.path.join(D, 'salida.bin'), 'rb').read()
pos = 0
fallos = {'cubre': [], 'convierte': [], 'mínimo': [], 'tope': [], 'junta': []}
for a, b, tope in casos:
    okc, n = struct.unpack_from('<II', s, pos)
    pos += 8
    tr = [struct.unpack_from('<5I', s, pos + 20 * i) for i in range(n)]
    pos += 20 * n
    x = y = 0
    hecho = []
    tocadas = 0
    for cl, a0, an, b0, bn in tr:
        if a0 != x or b0 != y or (cl == 0 and an != bn) or (cl == 1 and bn) or (cl == 2 and an) or (cl == 3 and (not an or not bn)) or an + bn == 0:
            fallos['cubre'].append((a, b, tr))
            break
        if cl == 0 and a[a0:a0 + an] != b[b0:b0 + bn]:
            fallos['convierte'].append((a, b, tr))
            break
        hecho += b[b0:b0 + bn]
        if cl:
            tocadas += an + bn
        x += an
        y += bn
    else:
        if (x, y) != (len(a), len(b)) or hecho != b:
            fallos['cubre'].append((a, b, tr))
            continue
        for t1, t2 in zip(tr, tr[1:]):
            if t1[0] != 0 and t2[0] != 0 or t1[0] == 0 and t2[0] == 0:
                fallos['junta'].append((a, b, tr))
                break
        mínimo = len(a) + len(b) - 2 * lcs(a, b)
        if tope and mínimo > tope:
            if okc:
                fallos['tope'].append((a, b, tope, 'dice que no se pasó', mínimo))
        else:
            if not okc:
                fallos['tope'].append((a, b, tope, 'dice que se pasó', mínimo))
            elif tocadas != mínimo:
                fallos['mínimo'].append((a, b, tocadas, mínimo))
for k, q in (('cubre', 'los trozos van en orden, sin huecos ni encimas, y cubren a y b enteros'),
             ('convierte', 'lo IGUAL es igual y el guion convierte a en b'),
             ('junta', 'no hay dos trozos de cambios seguidos ni dos iguales seguidos (QUITADO junto a AÑADIDO es CAMBIADO)'),
             ('mínimo', 'lo tocado es lo mínimo: n + m - 2·LCS (el LCS, por programación dinámica)'),
             ('tope', 'el tope: falso justo cuando hay más cambios que él, y entonces lo de en medio, un CAMBIADO')):
    if fallos[k]:
        ko(q + ': %d fallos; el primero: %r' % (len(fallos[k]), fallos[k][0]))
    else:
        ok(q)
print('      (%d casos; %s)' % (len(casos), r.strip()))
if 'en uso al final: 0' not in r:
    ko('la memoria no se suelta entera: ' + r.strip())
for l in subprocess.run([R, BIN, 'TIEMPO'], cwd=D, capture_output=True).stdout.decode('cp437').splitlines():
    print('      ' + l)
    if l.startswith('mal'):
        ko(l)
print('== %d bien, %d mal ==' % (bien, mal))
sys.exit(1 if mal else 0)
