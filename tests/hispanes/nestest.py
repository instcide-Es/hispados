#!/usr/bin/env python3
"""Compara la traza de Hispa-Nes con la de referencia de «nestest».

uso: nestest.py NESNATIVO_TRAZA nestest.nes nestest.log

nestest.nes (de Kevin Horton, de libre distribución; no va en el proyecto:
lo descarga tests/run_hispanes_tests.sh) prueba todas las instrucciones del
procesador, oficiales y no oficiales. Arrancado en C000h no necesita
pantalla, y su registro de referencia dice, instrucción a instrucción, el
contador de programa, A, X, Y, P, S y los ciclos que lleva. Aquí se corre
el núcleo con la traza activada y se compara todo.
"""
import re, struct, subprocess, sys, tempfile, os

exe, rom, log = sys.argv[1:4]
with tempfile.TemporaryDirectory() as t:
    tr = subprocess.run([exe, rom, '3', os.path.join(t, 'x.pgm'), '-', '-', 'C000'],
                        stdout=subprocess.PIPE, check=True).stdout
ref = []
for l in open(log, encoding='latin-1'):
    m = re.match(r'([0-9A-F]{4}) .* A:(..) X:(..) Y:(..) P:(..) SP:(..) .*CYC:(\d+)', l)
    if m:
        ref.append(tuple(int(x, 16) for x in m.groups()[:6]) + (int(m.group(7)),))
n = min(len(ref), len(tr) // 12)
t0 = None
for i in range(n):
    pc, a, x, y, p, s, _, pts = struct.unpack_from('<HBBBBBBI', tr, i * 12)
    if t0 is None:
        t0 = pts - ref[0][6] * 3
    mio = (pc, a, x, y, p, s, (pts - t0) // 3)
    if mio != ref[i] or (pts - t0) % 3:
        print('  FAIL nestest: en la instrucción %d sale PC=%04X A=%02X X=%02X Y=%02X P=%02X S=%02X ciclo %d' % ((i + 1,) + mio))
        print('       y se esperaba       PC=%04X A=%02X X=%02X Y=%02X P=%02X S=%02X ciclo %d' % ref[i])
        sys.exit(1)
if n < len(ref):
    print('  FAIL nestest: la traza se queda en %d instrucciones de %d' % (n, len(ref)))
    sys.exit(1)
print('  OK   nestest: %d instrucciones iguales que la referencia (registros, banderas y ciclos)' % n)
