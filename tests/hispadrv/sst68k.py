#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Pasa un archivo de pruebas de una instrucción del 68000 (JSON comprimido,
el formato de SingleStepTests/680x0) al formato de mdnativo -t.

uso: sst68k.py PRUEBAS.json.gz SALIDA.bin

Se quitan las pruebas que acaban en un error de dirección (leen el vector
3): Hispa-Drive no lo emula (los juegos no lo usan).
"""
import gzip, json, struct, sys

REGS = ['d0', 'd1', 'd2', 'd3', 'd4', 'd5', 'd6', 'd7', 'a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6',
        'usp', 'ssp', 'sr', 'pc']


def estado(e, con_pref):
    b = b''.join(struct.pack('<I', e[r] & 0xFFFFFFFF) for r in REGS)
    if con_pref:
        b += struct.pack('<II', *e['prefetch'])
    b += struct.pack('<I', len(e['ram']))
    for a, v in e['ram']:
        b += struct.pack('<II', a, v)
    return b


def main():
    datos = json.load(gzip.open(sys.argv[1]))
    sal = bytearray()
    quitadas = 0
    for p in datos:
        if any(t[0] == 'r' and t[3] in (12, 14) for t in p.get('transactions', [])):
            quitadas += 1
            continue
        n = p['name'].encode()
        sal += struct.pack('<I', len(n)) + n + b'\0' * (-len(n) % 4)
        sal += estado(p['initial'], True) + estado(p['final'], False)
        sal += struct.pack('<I', p['length'])
    open(sys.argv[2], 'wb').write(sal)
    if quitadas:
        sys.stderr.write('%s: %d con error de dirección, fuera\n' % (sys.argv[1], quitadas))


if __name__ == '__main__':
    main()
