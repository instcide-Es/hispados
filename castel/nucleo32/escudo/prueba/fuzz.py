#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# castel/nucleo32/escudo/prueba/fuzz.py ANFITRION BASE VUELTAS
# Prueba de resistencia del motor de Escudo (lo que pidió la revisión de
# seguridad del 21/11): estropea al azar un FIRMAS.ESC, firmas en texto, ZIP y
# PE (de BASE: la salida de hazprueba.py) y se los da a escudo-anfitrion
# (mejor, compilado con -fsanitize=address,undefined: hacer.sh --asan). Ni
# se puede colgar (20 s) ni salir mal (código 2 o más, o lo que diga ASan).
# Dice cuántos ha probado y los que han fallado (y los deja en BASE/fallos).
import os
import random
import subprocess
import sys

anf, base, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
r = random.Random(2111)
fallos = os.path.join(base, 'fallos')
os.makedirs(fallos, exist_ok=True)
semillas = {
    'esc': open(os.path.join(base, 'PRUEBA.ESC'), 'rb').read(),
    'zip': open(os.path.join(base, 'ARCHIVOS', 'ZIPZIP.ZIP'), 'rb').read(),
    'exe': open(os.path.join(base, 'ARCHIVOS', 'PRUEBA.EXE'), 'rb').read(),
    'ndb': open(os.path.join(base, 'FIRMAS', 'PRUEBA.NDB'), 'rb').read(),
    'ldb': open(os.path.join(base, 'FIRMAS', 'PRUEBA.LDB'), 'rb').read(),
}


def estropea(d):
    d = bytearray(d)
    for _ in range(r.randint(1, 12)):
        q = r.random()
        if not d:
            break
        i = r.randrange(len(d))
        if q < 0.4:
            d[i] ^= 1 << r.randrange(8)
        elif q < 0.6:
            d[i] = r.choice([0, 0xFF, 0x7F, 0x80, r.randrange(256)])
        elif q < 0.75 and i + 4 <= len(d):
            d[i:i + 4] = r.choice([b'\xff\xff\xff\xff', b'\x00\x00\x00\x80', b'\xff\xff\xff\x7f', os.urandom(4)])
        elif q < 0.85:
            del d[i:i + r.randint(1, 64)]
        else:
            d[i:i] = r.choice([b'(', b')', b'*', b'{', b'|', b'??', b'::i', b'&', b'0', os.urandom(8)]) * r.randint(1, 40)
    return bytes(d)


malos = 0
t = os.path.join(base, 'fuzz')
os.makedirs(t, exist_ok=True)
for k in range(vueltas):
    clase = r.choice(list(semillas))
    d = estropea(semillas[clase])
    if clase in ('esc', 'ndb', 'ldb'):
        f = os.path.join(t, 'F.' + clase)
        open(f, 'wb').write(d)
        args = [anf, '-q', '-f', f, os.path.join(base, 'ARCHIVOS', 'EICAR.COM'), os.path.join(base, 'ARCHIVOS', 'COMODIN.BIN')]
    else:
        f = os.path.join(t, 'A.' + clase)
        open(f, 'wb').write(d)
        args = [anf, '-q'] + sum((['-f', os.path.join(base, 'FIRMAS', x)] for x in sorted(os.listdir(os.path.join(base, 'FIRMAS')))), []) + [f]
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:allocator_may_return_null=1', UBSAN_OPTIONS='halt_on_error=1')
    try:
        p = subprocess.run(args, capture_output=True, timeout=20, env=env)
        mal = p.returncode >= 2 or b'ERROR: AddressSanitizer' in p.stderr or b'runtime error' in p.stderr
        por = 'código %d' % p.returncode
    except subprocess.TimeoutExpired:
        mal, por = True, 'se cuelga'
    if mal:
        malos += 1
        n = os.path.join(fallos, '%04d.%s' % (k, clase))
        open(n, 'wb').write(d)
        print('  %s: %s (%s)' % (n, por, (p.stderr.decode('latin-1').strip().split('\n') or [''])[-1][:120] if por != 'se cuelga' else ''))
print('fuzz.py: %d probados, %d mal' % (vueltas, malos))
sys.exit(1 if malos else 0)
