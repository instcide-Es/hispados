#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# orto_fuzz.py ARNES_C [VUELTAS]: DICCIO.DAT estropeado a propósito (octetos
# cambiados, cortado, alargado, la cabecera, y arcos que apuntan a cualquier
# sitio —también hacia atrás: ciclos— con la suma rehecha para que pase la
# comprobación) y, con cada uno, cargar, mirar y proponer con el arnés del
# corrector de C hecho con ASan y UBSan: nada puede romperse ni colgarse.
import os
import random
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
arnes = os.path.abspath(sys.argv[1])
vueltas = int(sys.argv[2]) if len(sys.argv) > 2 else 200
random.seed(1122)
dat = open(os.path.join(RAIZ, 'programas', 'comun', 'DICCIO.DAT'), 'rb').read()
n = struct.unpack_from('<I', dat, 8)[0]
PAL = [b'casa', b'haver', b'tanbien', b'CANPO', b'Arvol', b'qeu', b'llendo', b'a' * 63, b'zzzzzzzzzzzzzzzzzzzzzzzzzzzzzz']


def con_suma(d):
    d = bytearray(d)
    na = (len(d) - 32) // 4
    s = sum(struct.unpack_from('<%dI' % na, d, 32)) & 0xFFFFFFFF
    struct.pack_into('<I', d, 16, s)
    return bytes(d)


def estropea(k):
    d = bytearray(dat)
    if k == 0:
        for _ in range(random.randint(1, 40)):
            d[random.randrange(len(d))] = random.randrange(256)
    elif k == 1:
        d = d[:random.randrange(len(d))]
    elif k == 2:
        d += bytes(random.randrange(256) for _ in range(random.randint(1, 9)))
    elif k == 3:
        d[random.randrange(32)] = random.randrange(256)
    elif k == 4:
        # arcos a cualquier arco que existe (ciclos), con la suma bien
        for _ in range(random.randint(1, 3000)):
            i = random.randrange(n)
            a = struct.unpack_from('<I', d, 32 + 4 * i)[0]
            a = (a & 0x3FF) | (random.randrange(n) << 10)
            struct.pack_into('<I', d, 32 + 4 * i, a)
        d = con_suma(d)
    elif k == 5:
        # quitar marcas de fin de nodo y de palabra, con la suma bien
        for _ in range(random.randint(1, 3000)):
            i = random.randrange(n)
            a = struct.unpack_from('<I', d, 32 + 4 * i)[0] ^ random.choice((0x100, 0x200, 0x300))
            struct.pack_into('<I', d, 32 + 4 * i, a)
        d = con_suma(d)
    elif k == 6:
        # la palabra más larga, cualquiera
        d[20] = random.randrange(256)
        d = con_suma(d)
    return bytes(d)


malas = cargados = 0
with tempfile.TemporaryDirectory() as tmp:
    for v in range(vueltas):
        k = v % 7
        f = os.path.join(tmp, 'D.DAT')
        open(f, 'wb').write(estropea(k))
        ent = [b'D ' + f.encode()]
        for p in PAL:
            ent += [b'P ' + p, b'S ' + p]
        ent.append(b'F esto es una prueva de testo')
        try:
            r = subprocess.run([arnes], input=b'\n'.join(ent) + b'\n', capture_output=True, timeout=60)
        except subprocess.TimeoutExpired:
            malas += 1
            print('  se cuelga (clase %d, vuelta %d)' % (k, v))
            continue
        if r.returncode != 0 or b'ERROR' in r.stderr or b'runtime error' in r.stderr:
            malas += 1
            print('  falla (clase %d, vuelta %d): %s' % (k, v, r.stderr.decode(errors='replace')[:300]))
        elif r.stdout.startswith(b'H1'):
            cargados += 1
print('%d vueltas, %d cargados (con la suma rehecha), %d fallos' % (vueltas, cargados, malas))
print('%d OK / %d FAIL' % (vueltas - malas, malas))
sys.exit(1 if malas else 0)
