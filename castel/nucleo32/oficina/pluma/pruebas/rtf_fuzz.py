#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# rtf_fuzz.py PLUMA CORPUS... [--vueltas N]: el lector de RTF de Pluma 2
# (pluma/f_rtf.c) con RTF estropeados a propósito: octetos cambiados,
# cortes, trozos repetidos, llaves sin cerrar, anidado muy hondo, \binN,
# hex roto en las imágenes, \uN y números enormes, filas y celdas de más.
# Cada uno se abre (--info) y se escribe otra vez en RTF y en PLU con el
# Pluma hecho con ASan y UBSan: nada puede romperse ni colgarse.
import os
import random
import subprocess
import sys
import tempfile

args = sys.argv[1:]
vueltas = 300
if '--vueltas' in args:
    i = args.index('--vueltas')
    vueltas = int(args[i + 1])
    del args[i:i + 2]
pluma = os.path.abspath(args[0])
corpus = [open(f, 'rb').read() for f in args[1:]]
random.seed(20261122)

CONTROLES = [b'\\bin999999999 ', b'\\bin5 ', b'\\u-99999999999?', b'\\u55357?', b'\\u56832?', b'\\uc99 ', b'\\fs99999999999 ',
             b'\\cellx-99999999 ', b'\\cellx99999999 ', b'\\clvmrg', b'\\clmrg', b'\\clvmgf', b'\\row ', b'\\cell ', b'\\intbl ',
             b'\\trowd', b'\\pict\\pngblip 89504e470d0a1a0a00zz', b'{\\*\\shppict{\\pict\\jpegblip ffd8ffe0', b'\\par ',
             b'\\ls99999 ', b'\\ilvl99 ', b'\\li-99999999 ', b'\\sl-2147483648 ', b'\\tx99999999 ', b'\\highlight99999 ',
             b'\\cf-5 ', b'\\f-99999 ', b"\\'zz", b"\\'", b'\\', b'{', b'}', b'{\\header ', b'{\\field{\\*\\fldinst PAGE}',
             b'\\page ', b'\\pgnstarts-9 ', b'{\\colortbl\\red999\\green-5;', b'{\\fonttbl{\\f0 ' + b'x' * 300 + b';}}',
             b'\\pnlvlblt', b'{\\*\\pn\\pndec}', b'\\sect ', b'\\titlepg', b'\\paperw-5 ', b'\\margl99999999 ']


def estropea(d):
    d = bytearray(d)
    k = random.randrange(8)
    if k == 0:
        for _ in range(random.randint(1, 30)):
            if d:
                d[random.randrange(len(d))] = random.randrange(256)
    elif k == 1:
        d = d[:random.randrange(len(d) + 1)]
    elif k == 2:
        for _ in range(random.randint(1, 20)):
            i = random.randrange(len(d) + 1)
            d[i:i] = random.choice(CONTROLES)
    elif k == 3:
        a = random.randrange(len(d) + 1)
        b = min(len(d), a + random.randrange(2000))
        d[a:a] = d[a:b] * random.randint(2, 30)
    elif k == 4:
        i = random.randrange(len(d) + 1)
        d[i:i] = b'{' * random.choice((300, 5000, 200000))
    elif k == 5:
        fila = b'\\trowd' + b''.join(b'\\cellx%d' % (100 * j) for j in range(random.choice((3, 70, 200)))) + \
            b'\\pard\\intbl ' + b'x\\cell ' * random.choice((2, 80)) + b'\\row\n'
        i = random.randrange(len(d) + 1)
        d[i:i] = fila * random.choice((1, 50, 5000))
    elif k == 6:
        d = d.replace(b'}', b'', random.randint(1, 50))
    else:
        i = random.randrange(len(d) + 1)
        d[i:i] = b''.join(random.choice(CONTROLES) for _ in range(200))
    if not d.startswith(b'{\\rtf'):
        d = b'{\\rtf1' + bytes(d)
    return bytes(d)


malas = 0
with tempfile.TemporaryDirectory() as tmp:
    e = os.path.join(tmp, 'E.RTF')
    for v in range(vueltas):
        datos = estropea(random.choice(corpus))
        open(e, 'wb').write(datos)
        for orden in (['--info', e], ['--convierte', e, os.path.join(tmp, 'S.RTF')], ['--convierte', e, os.path.join(tmp, 'S.PLU')]):
            try:
                r = subprocess.run([pluma] + orden, capture_output=True, timeout=120, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1'))
            except subprocess.TimeoutExpired:
                malas += 1
                open(os.path.join('/tmp', 'rtf_fuzz_cuelga_%d.rtf' % v), 'wb').write(datos)
                print('  se cuelga: vuelta %d, %s' % (v, orden[0]))
                break
            if r.returncode not in (0, 1) or b'ERROR: AddressSanitizer' in r.stderr or b'runtime error' in r.stderr \
                    or b'LeakSanitizer' in r.stderr:
                malas += 1
                open(os.path.join('/tmp', 'rtf_fuzz_falla_%d.rtf' % v), 'wb').write(datos)
                print('  falla: vuelta %d, %s, código %d: %s' % (v, orden[0], r.returncode, r.stderr.decode(errors='replace')[-400:]))
                break
print('%d vueltas (%d ejecuciones), %d fallos' % (vueltas, vueltas * 3, malas))
print('%d OK / %d FAIL' % (vueltas - malas, malas))
sys.exit(1 if malas else 0)
