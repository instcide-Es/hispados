#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Las tablas del YM2612 de Hispa-Drive (programas/hispadrv/ym.inc).

uso: genym.py SALIDA.inc

  ym_seno     1024 palabras con signo: una vuelta del seno, ±8191 (14 bits)
  ym_exp      1024 palabras: la atenuación (pasos de 0,09375 dB, como la
              del chip: 10 bits, 96 dB) a ganancia lineal (65535 = 0 dB)
  ym_egsh     64 octetos: cada cuánto avanza la envolvente con cada
              velocidad (2^n ticks)
  ym_eginc    64 x 8 octetos: y cuánto (la tabla de la documentación
              pública del chip)
  ym_dt       4 x 32 octetos: la desafinación por el código de la nota
Se hacen aquí (con coma flotante) para que el emulador no la necesite (un
486SX no la tiene).
"""
import math, sys


def main():
    out = ['; generado por tools/genym.py: no se toca a mano']
    sen = [round(8191 * math.sin(2 * math.pi * (i + 0.5) / 1024)) for i in range(1024)]
    ex = [round(65535 * 10 ** (-a * 0.09375 / 20)) for a in range(1024)]
    ex[1023] = 0
    out.append('        align 4')
    out.append('ym_seno:')
    for i in range(0, 1024, 16):
        out.append('        dw ' + ', '.join(str(v) for v in sen[i:i + 16]))
    out.append('ym_exp:')
    for i in range(0, 1024, 16):
        out.append('        dw ' + ', '.join(str(v) for v in ex[i:i + 16]))
    sh = [max(0, 11 - (r >> 2)) for r in range(64)]
    out.append('ym_egsh:')
    for i in range(0, 64, 16):
        out.append('        db ' + ', '.join(str(v) for v in sh[i:i + 16]))
    pat = [[0, 1, 0, 1, 0, 1, 0, 1], [0, 1, 0, 1, 1, 1, 0, 1], [0, 1, 1, 1, 0, 1, 1, 1], [0, 1, 1, 1, 1, 1, 1, 1]]
    alto = {48: [[1] * 8, [1, 1, 1, 2, 1, 1, 1, 2], [1, 2, 1, 2, 1, 2, 1, 2], [1, 2, 2, 2, 1, 2, 2, 2]],
            52: [[2] * 8, [2, 2, 2, 4, 2, 2, 2, 4], [2, 4, 2, 4, 2, 4, 2, 4], [2, 4, 4, 4, 2, 4, 4, 4]],
            56: [[4] * 8, [4, 4, 4, 8, 4, 4, 4, 8], [4, 8, 4, 8, 4, 8, 4, 8], [4, 8, 8, 8, 4, 8, 8, 8]],
            60: [[8] * 8] * 4}
    out.append('ym_eginc:')
    for r in range(64):
        if r < 2:
            fila = [0] * 8
        elif r < 48:
            fila = pat[r & 3]
        else:
            fila = alto[r & ~3][r & 3]
        out.append('        db ' + ', '.join(str(v) for v in fila))
    dt = [[0] * 32,
          [0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 8, 8],
          [1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 16, 16, 16],
          [2, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 20, 22, 22, 22, 22]]
    out.append('ym_dt:')
    for f in dt:
        out.append('        db ' + ', '.join(str(v) for v in f))
    open(sys.argv[1], 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
