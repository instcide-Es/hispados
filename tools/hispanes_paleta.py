#!/usr/bin/env python3
"""La paleta de Hispa-Nes: los 64 colores de la consola, en rojo, verde y azul.

uso: hispanes_paleta.py            escribe la tabla para programas/hispanes.asm
     hispanes_paleta.py -p         la escribe como lista de Python (pruebas)

La consola no tiene una paleta en RGB: saca una señal de televisión NTSC. Cada
color es una onda cuadrada entre dos tensiones (cuatro niveles de brillo) con
una de doce fases (el tono); el tono 0 es la tensión alta fija (blancos y
grises), el 13 la baja fija (grises oscuros) y el 14 y el 15, negro. Aquí se
calcula lo que vería un televisor: se descompone esa onda en luminancia (Y)
y los dos ejes de color (I, Q), y de ahí a RGB con la matriz de siempre, con
una corrección de gamma suave. Las tensiones son las medidas que publica la
documentación del hardware; el resultado es una paleta propia (cada emulador
tiene la suya), calculada, no copiada.
Computes the NES palette from the documented NTSC signal levels.
"""
import math, sys

BAJA = (0.350, 0.518, 0.962, 1.550)     # voltios (relativos) por nivel de brillo
ALTA = (1.094, 1.506, 1.962, 1.962)
NEGRO, BLANCO = 0.518, 1.962
FASE = 3.9                               # giro del tono (en doceavos de vuelta)
SATURACION = 1.15


def color(n):
    nivel, tono = n >> 4, n & 15
    if tono >= 14:
        return (0, 0, 0)
    alta, baja = ALTA[nivel], BAJA[nivel]
    if tono == 0:
        baja = alta
    if tono == 13:
        alta = baja
    y = i = q = 0.0
    for p in range(12):
        v = alta if (tono + p) % 12 < 6 else baja
        v = (v - NEGRO) / (BLANCO - NEGRO)
        y += v / 12
        i += v * math.cos(math.pi * (p + FASE) / 6) / 6 * SATURACION
        q += v * math.sin(math.pi * (p + FASE) / 6) / 6 * SATURACION
    r = y + 0.956 * i + 0.621 * q
    g = y - 0.272 * i - 0.647 * q
    b = y - 1.106 * i + 1.703 * q
    return tuple(int(round(255 * min(1.0, max(0.0, c)) ** (1 / 1.1))) for c in (r, g, b))


PALETA = [color(n) for n in range(64)]

if __name__ == '__main__':
    if '-p' in sys.argv:
        print(PALETA)
    else:
        for f in range(0, 64, 4):
            print('        db ' + ',  '.join('0x%02X, 0x%02X, 0x%02X' % c for c in PALETA[f:f + 4])
                  + ('    ; %02Xh' % f))
