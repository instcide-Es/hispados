#!/usr/bin/env python3
"""Cuenta puntos de un color en una zona de una captura (PPM de QEMU).

uso: lector_puntos.py CAPTURA.ppm X Y ANCHO ALTO COLOR MIN MAX NOMBRE

COLOR es un número de la paleta de Hispa Ventanas (0-255), «tinta» (cualquier
punto oscuro: texto negro o casi) o RRGGBB. Escribe «  OK   ...» si en la zona
hay entre MIN y MAX puntos de ese color, y «  FAIL ...» si no. La usa
tests/run_lector_tests.sh para comprobar que en la página hay letras, rayas y
recuadros donde tienen que estar, sin depender del dibujo de cada letra.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ventanas_check as v


def main():
    cap = v.Captura(sys.argv[1])
    x, y, an, al = (int(a) for a in sys.argv[2:6])
    color, mn, mx, nombre = sys.argv[6], int(sys.argv[7]), int(sys.argv[8]), sys.argv[9]
    if color == 'tinta':
        es = lambda c: sum(c) < 3 * 96
    elif len(color) == 6:
        q = tuple(bytes.fromhex(color))
        es = lambda c: all(abs(a - b) <= v.MARGEN for a, b in zip(c, q))
    else:
        q = v.paleta()[int(color)]
        es = lambda c: all(abs(a - b) <= v.MARGEN for a, b in zip(c, q))
    n = sum(1 for yy in range(y, y + al) for xx in range(x, x + an) if es(cap.px(xx, yy)))
    bien = mn <= n <= mx
    print('  %s %s: %s (%d puntos en %d,%d %dx%d; se esperaban de %d a %d)' % (
        'OK  ' if bien else 'FAIL', os.path.basename(sys.argv[1]).replace('.ppm', ''), nombre, n, x, y, an, al, mn, mx))
    sys.exit(0 if bien else 1)


if __name__ == '__main__':
    main()
