#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""prueba/fuzz.py ANFITRION CARPETA_DE_SEMILLAS VUELTAS [SEMILLA]

El fuzz de Comprimidos: ZIP malformados (los de CARPETA cambiados al azar:
octetos sueltos, trozos cortados o repetidos, números de los campos puestos a
0, 0xFFFF o 0xFFFFFFFF) que «anfitrion fuzz» (con ASan y UBSan) abre, prueba,
saca y rehace. Falla si alguno da un error de ASan o de UBSan, se cae o tarda
más de 20 s. Escribe los que fallan en CARPETA/fallos/.
"""
import os, random, subprocess, sys

anf, carpeta, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 2611)
semillas = [open(os.path.join(carpeta, n), 'rb').read() for n in sorted(os.listdir(carpeta)) if n.endswith('.zip')]
tmp = os.path.join(carpeta, 'fuzz.tmp')
fallos = 0


def cambia(d):
    d = bytearray(d)
    for _ in range(random.randint(1, 6)):
        q = random.random()
        if not d:
            break
        i = random.randrange(len(d))
        if q < 0.35:
            d[i] = random.randrange(256)
        elif q < 0.5:
            n = random.choice([2, 4])
            v = random.choice([0, 0xFF, 0xFFFF, 0xFFFFFFFF, random.randrange(1 << 32)])
            d[i:i + n] = (v & ((1 << (8 * n)) - 1)).to_bytes(n, 'little')
        elif q < 0.65:
            del d[i:i + random.randint(1, 64)]
        elif q < 0.8:
            j = random.randrange(len(d))
            d[i:i] = d[j:j + random.randint(1, 200)]
        elif q < 0.9:
            # cerca de las firmas (donde están los campos)
            for f in (b'PK\x01\x02', b'PK\x03\x04', b'PK\x05\x06', b'PK\x06\x06'):
                k = d.find(f, random.randrange(len(d)))
                if k >= 0:
                    p = k + random.randrange(4, 46)
                    if p < len(d):
                        d[p] = random.randrange(256)
                    break
        else:
            d = d[:random.randrange(len(d))]
    return bytes(d)


for v in range(vueltas):
    datos = cambia(random.choice(semillas))
    open(tmp, 'wb').write(datos)
    try:
        r = subprocess.run([anf, 'fuzz', tmp, os.path.join(carpeta, 'fuzz.dir')], capture_output=True, timeout=20,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=0', UBSAN_OPTIONS='halt_on_error=1'))
        malo = r.returncode != 0 or b'ERROR: AddressSanitizer' in r.stderr or b'runtime error' in r.stderr or b'LeakSanitizer' in r.stderr
        texto = r.stderr.decode(errors='replace')
    except subprocess.TimeoutExpired:
        malo, texto = True, 'tarda más de 20 s'
    if malo:
        fallos += 1
        os.makedirs(os.path.join(carpeta, 'fallos'), exist_ok=True)
        open(os.path.join(carpeta, 'fallos', 'f%04d.zip' % v), 'wb').write(datos)
        print('FALLO %d:' % v, '\n'.join(texto.splitlines()[:12]))
os.remove(tmp)
print('fuzz: %d vueltas, %d fallos' % (vueltas, fallos))
sys.exit(1 if fallos else 0)
