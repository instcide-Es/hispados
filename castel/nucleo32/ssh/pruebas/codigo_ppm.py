#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# ssh/pruebas/codigo_ppm.py CAPTURA.ppm - (REMOTA) el código de 6 cifras de la
# ventana «Dejar entrar a Claude» (ssh/remota.c), leído de una captura de
# QEMU (screendump): el recuadro blanco de 420 x 66 y las cifras de 5 x 7
# cuadros de 7 puntos que pinta cifra_grande. Escribe el código, o nada (y
# sale con 1) si no lo hay: para las pruebas (castel/pruebas_remota.sh).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import sys

CIFRAS = [(14, 17, 19, 21, 25, 17, 14), (4, 12, 4, 4, 4, 4, 14), (14, 17, 1, 2, 4, 8, 31), (31, 2, 4, 2, 1, 17, 14),
          (2, 6, 10, 18, 31, 2, 2), (31, 16, 30, 1, 1, 17, 14), (6, 8, 16, 30, 17, 17, 14), (31, 1, 2, 4, 8, 8, 8),
          (14, 17, 17, 14, 17, 17, 14), (14, 17, 17, 15, 1, 2, 12)]


def lee(ruta):
    d = open(ruta, 'rb').read()
    campos, i = [], 0
    while len(campos) < 4:
        while d[i:i + 1].isspace():
            i += 1
        if d[i:i + 1] == b'#':
            while d[i:i + 1] != b'\n':
                i += 1
            continue
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        campos.append(d[i:j])
        i = j
    return int(campos[1]), int(campos[2]), d[i + 1:]


def main(ruta):
    an, al, p = lee(ruta)
    px = lambda x, y: p[(y * an + x) * 3:(y * an + x) * 3 + 3]
    blanco = b'\xff\xff\xff'
    fila = lambda y: p[y * an * 3:(y + 1) * an * 3]
    for y in range(al - 66):
        f = fila(y)
        k = f.find(blanco * 420)
        while k >= 0 and k % 3:
            k = f.find(blanco * 420, k + 1)
        if k < 0:
            continue
        x = k // 3
        if all(px(x + dx, y + dy) == blanco for dx, dy in ((0, 65), (419, 65), (419, 0))):
            cod = ''
            for i in range(6):
                bx = x + 75 + i * 42 + (18 if i >= 3 else 0)
                filas = tuple(sum((1 << (4 - c)) for c in range(5) if px(bx + c * 7 + 3, y + 6 + r * 7 + 3)[0] < 0x80)
                              for r in range(7))
                if filas not in CIFRAS:
                    return 1
                cod += str(CIFRAS.index(filas))
            print(cod)
            return 0
    return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
