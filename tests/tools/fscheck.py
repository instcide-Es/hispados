#!/usr/bin/env python3
"""Comprueba desde fuera los ficheros S0.DAT..S7.DAT que deja «FSTEST S» en
la raíz de una imagen FAT: el octeto en la posición o del fichero f tiene que
ser (o_bajo xor o_alto) + 31*f.

uso: fscheck.py IMG   (sale con 1 si algo no cuadra)
"""
import subprocess, sys

img = sys.argv[1]
bad = 0
total = 0
for f in range(8):
    r = subprocess.run(['mtype', '-i', img, f'::S{f}.DAT'], capture_output=True)
    if r.returncode != 0:
        print(f"S{f}.DAT: no se puede leer")
        bad = 1
        continue
    d = r.stdout
    total += len(d)
    for o, b in enumerate(d):
        if b != (((o & 255) ^ (o >> 8)) + 31 * f) & 255:
            print(f"S{f}.DAT: octeto {o} distinto ({b:02X})")
            bad = 1
            break
if total == 0:
    print("los ocho ficheros están vacíos")
    bad = 1
sys.exit(bad)
