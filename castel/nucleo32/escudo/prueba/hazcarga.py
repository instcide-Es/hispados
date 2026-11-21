#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# castel/nucleo32/escudo/prueba/hazcarga.py SALIDA MB [NDB] [LDB] [SUMAS]
# Para medir lo que tarda Escudo: firmas de mentira con el formato de ClamAV
# (SALIDA/CARGA.NDB con NDB patrones, una de cada cinco con comodines y «*»;
# SALIDA/CARGA.LDB con LDB lógicas de tres subfirmas; SALIDA/CARGA.HDB con
# SUMAS sumas MD5) y un archivo de MB megas de octetos al azar (GRANDE.BIN)
# que no casa con ninguna. Lo usa castel/pruebas.sh (parte 20): en Linux y en
# QEMU, MB/s.
import hashlib
import os
import random
import sys

sal, mb = sys.argv[1], int(sys.argv[2])
ndb = int(sys.argv[3]) if len(sys.argv) > 3 else 20000
ldb = int(sys.argv[4]) if len(sys.argv) > 4 else 5000
sumas = int(sys.argv[5]) if len(sys.argv) > 5 else 100000
os.makedirs(sal, exist_ok=True)
r = random.Random(2026)


def hx(n):
    return ''.join('%02x' % r.randrange(256) for _ in range(n))


with open(os.path.join(sal, 'CARGA.NDB'), 'w') as f:
    for i in range(ndb):
        h = hx(r.randint(8, 40))
        if i % 5 == 0:
            h = h[:10] + '??' + h[10:] + '*' + hx(6)
        f.write('Carga.Ndb-%d-0:%d:*:%s\n' % (i, r.choice([0, 0, 1]), h))
with open(os.path.join(sal, 'CARGA.LDB'), 'w') as f:
    for i in range(ldb):
        f.write('Carga.Ldb-%d-0;Target:0;0&1&2;%s;%s;%s\n' % (i, hx(r.randint(6, 16)), hx(r.randint(6, 16)), hx(r.randint(6, 16))))
with open(os.path.join(sal, 'CARGA.HDB'), 'w') as f:
    for i in range(sumas):
        f.write('%s:%d:Carga.Hdb-%d-0\n' % (hashlib.md5(b'%d' % i).hexdigest(), r.randint(100, 4000000), i))
with open(os.path.join(sal, 'GRANDE.BIN'), 'wb') as f:
    for _ in range(mb):
        f.write(r.randbytes(1 << 20))
