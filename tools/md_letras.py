#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Pasa los dibujos en texto (el formato «.tiles» de tools/asmz80.py, como
sms/comun/letras.s) al formato de la consola de 16 bits: 32 octetos por
dibujo, 4 bits por punto (el de la izquierda en los bits altos).

uso: md_letras.py DIBUJOS.s SALIDA.bin [COLOR]
  COLOR: el color que toma la tinta 1 (por omisión, 1); los demás, tal cual.
"""
import sys


def main():
    color = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    filas = []
    dentro = False
    for ln in open(sys.argv[1], encoding='utf-8'):
        ln = ln.split(';')[0].strip()
        if ln == '.tiles':
            dentro = True
            continue
        if not dentro or not ln:
            continue
        if ln.startswith('.'):
            if ln.startswith('.end'):
                dentro = False
                continue
            if not all(c in '.0123456789abcdefABCDEF' for c in ln) or len(ln) != 8:
                dentro = False
                continue
        if len(ln) == 8:
            filas.append(ln)
    sal = bytearray()
    for f in filas:
        px = [0 if c == '.' else int(c, 16) for c in f]
        px = [color if p == 1 else p for p in px]
        for i in range(0, 8, 2):
            sal.append(px[i] << 4 | px[i + 1])
    open(sys.argv[2], 'wb').write(sal)


if __name__ == '__main__':
    main()
