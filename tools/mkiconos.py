#!/usr/bin/env python3
"""Dibuja los iconos de Hispa Ventanas y escribe ventanas/iconos.inc.

uso: mkiconos.py SALIDA.inc

Los iconos son diseños propios de 32x32 puntos hechos aquí con rectángulos,
círculos y líneas. Se guardan en texto (una fila de 32 caracteres por línea):
«.» es transparente y 0-9, a-f son los 16 colores de ventanas/datos.inc.
El archivo generado se puede retocar a mano: esto solo es la plantilla.
Draws the Hispa Ventanas icons (own designs) as text art.
"""
import math, sys

TINTA, MARRON, PETROLEO, PETROLEO_CL, TERRACOTA, TERRACOTA_CL, ARENA, ARENA_OSC, \
    CREMA, BLANCO, OCRE, OLIVA, ANIL, GRANATE, TURQUESA, PIEDRA = range(16)


class Icono:
    def __init__(self):
        self.p = [[None] * 32 for _ in range(32)]

    def punto(self, x, y, c):
        if 0 <= x < 32 and 0 <= y < 32:
            self.p[y][x] = c

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.punto(xx, yy, c)

    def marco(self, x, y, w, h, c):
        self.rect(x, y, w, 1, c)
        self.rect(x, y + h - 1, w, 1, c)
        self.rect(x, y, 1, h, c)
        self.rect(x + w - 1, y, 1, h, c)

    def disco(self, cx, cy, r, c):
        for y in range(32):
            for x in range(32):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    self.punto(x, y, c)

    def rombo(self, cx, cy, r, c):
        for y in range(-r, r + 1):
            for x in range(-(r - abs(y)), r - abs(y) + 1):
                self.punto(cx + x, cy + y, c)

    def linea(self, x0, y0, x1, y1, c):
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(n + 1):
            self.punto(round(x0 + (x1 - x0) * i / n), round(y0 + (y1 - y0) * i / n), c)

    def sombra(self):
        """sombra dura, abajo y a la derecha, como la de las ventanas"""
        q = [f[:] for f in self.p]
        for y in range(32):
            for x in range(32):
                if self.p[y][x] is None and x >= 2 and y >= 2 and self.p[y - 2][x - 2] is not None:
                    q[y][x] = TINTA
        self.p = q

    def texto(self):
        return [''.join('.' if c is None else '0123456789abcdef'[c] for c in f) for f in self.p]


def ventana():
    i = Icono()
    i.rect(2, 4, 26, 22, MARRON)
    i.rect(3, 5, 24, 5, TERRACOTA)
    i.rombo(6, 7, 2, OCRE)
    i.rect(3, 11, 24, 14, CREMA)
    i.rect(6, 14, 14, 1, ARENA_OSC)
    i.rect(6, 17, 18, 1, ARENA_OSC)
    i.rect(6, 20, 10, 1, ARENA_OSC)
    i.sombra()
    return i


def centro():
    i = Icono()
    i.rect(1, 3, 28, 25, MARRON)
    i.rect(2, 4, 26, 5, TERRACOTA)
    i.rombo(5, 6, 2, OCRE)
    i.rect(2, 10, 26, 17, CREMA)
    for n, (x, y, c) in enumerate(((8, 14, ANIL), (21, 14, OLIVA), (8, 22, OCRE), (21, 22, GRANATE))):
        i.rombo(x, y, 3, c)
    i.sombra()
    return i


def bloc():
    i = Icono()
    i.rect(5, 3, 20, 26, MARRON)
    i.rect(6, 4, 18, 24, BLANCO)
    i.rect(6, 4, 18, 4, TERRACOTA)
    for x in range(8, 23, 4):
        i.rect(x, 2, 2, 4, TINTA)
    for y in range(11, 26, 3):
        i.rect(8, y, 14 if y != 23 else 8, 1, ANIL)
    i.linea(26, 8, 17, 24, OCRE)
    i.linea(27, 9, 18, 25, OCRE)
    i.linea(27, 8, 18, 24, MARRON)
    i.punto(17, 25, TINTA)
    i.punto(16, 26, TINTA)
    i.sombra()
    return i


def calc():
    i = Icono()
    i.rect(5, 2, 21, 27, MARRON)
    i.rect(6, 3, 19, 25, ARENA)
    i.rect(8, 5, 15, 6, BLANCO)
    i.marco(8, 5, 15, 6, MARRON)
    i.rect(17, 7, 4, 2, TINTA)
    for f in range(4):
        for c in range(4):
            col = TERRACOTA if c == 3 else PETROLEO
            if f == 3 and c == 2:
                col = OCRE
            i.rect(8 + c * 4, 13 + f * 4, 3, 3, col)
    i.sombra()
    return i


def reloj():
    i = Icono()
    i.disco(15, 15, 13, MARRON)
    i.disco(15, 15, 11, CREMA)
    for h in range(12):
        a = h * math.pi / 6
        i.punto(round(15 + 9.5 * math.sin(a)), round(15 - 9.5 * math.cos(a)), TERRACOTA)
    for (x, y) in ((15, 5), (15, 25), (5, 15), (25, 15)):
        i.rect(x - (0 if x != 15 else 0), y, 1, 1, TINTA)
        i.punto(x, y, TINTA)
    i.linea(15, 15, 15, 7, TINTA)
    i.linea(16, 15, 16, 7, TINTA)
    i.linea(15, 15, 21, 18, ANIL)
    i.linea(15, 16, 21, 19, ANIL)
    i.disco(15, 15, 1, GRANATE)
    i.sombra()
    return i


def main():
    with open(sys.argv[1], 'w', encoding='utf-8') as f:
        f.write('; Hispa Ventanas - iconos (32x32). Diseños propios, hechos aquí.\n'
                '; GPL-2.0-or-later. Plantilla generada por tools/mkiconos.py; se\n'
                '; pueden retocar a mano: «.» transparente y 0-9, a-f los colores\n'
                '; de la paleta (datos.inc).\n')
        for nombre, fn in (('ventana', ventana), ('centro', centro), ('bloc', bloc),
                           ('calc', calc), ('reloj', reloj)):
            f.write('\nico_%s:\n' % nombre)
            for fila in fn().texto():
                f.write('        db "%s"\n' % fila)


if __name__ == '__main__':
    main()
