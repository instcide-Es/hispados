#!/usr/bin/env python3
"""Dibuja los iconos de Hispa Ventanas y escribe ventanas/iconos.inc.

uso: mkiconos.py SALIDA.inc [SALIDA2.inc]

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


# ---- iconos del Centro de programas con grupos (ventanas/iconos2.inc) ----
def carpeta():
    """un grupo: carpeta de cartón con pestaña"""
    i = Icono()
    i.rect(3, 6, 11, 4, MARRON)
    i.rect(4, 7, 9, 3, OCRE)
    i.rect(2, 9, 26, 18, MARRON)
    i.rect(3, 10, 24, 16, OCRE)
    i.rect(3, 13, 24, 1, MARRON)
    i.rect(3, 14, 24, 12, ARENA)
    i.rombo(15, 20, 3, TERRACOTA)
    i.sombra()
    return i


def programa():
    """un programa de Hispa-DOS cualquiera: pantalla de texto"""
    i = Icono()
    i.rect(2, 4, 26, 21, MARRON)
    i.rect(3, 5, 24, 19, PETROLEO)
    i.rect(5, 8, 10, 1, TURQUESA)
    i.rect(5, 11, 16, 1, TURQUESA)
    i.rect(5, 14, 7, 1, TURQUESA)
    i.rect(5, 17, 13, 1, TURQUESA)
    i.rect(5, 20, 4, 2, OCRE)
    i.sombra()
    return i


def juego():
    """un juego: palanca con su bola y dos botones"""
    i = Icono()
    i.rect(3, 19, 25, 8, MARRON)
    i.rect(4, 20, 23, 6, ARENA)
    i.rect(12, 9, 6, 11, MARRON)
    i.rect(13, 9, 4, 11, ARENA_OSC)
    i.rect(14, 9, 1, 11, PIEDRA)
    i.disco(15, 6, 4, MARRON)
    i.disco(15, 6, 3, TERRACOTA)
    i.punto(14, 5, TERRACOTA_CL)
    i.punto(13, 5, TERRACOTA_CL)
    i.punto(14, 4, TERRACOTA_CL)
    i.rect(6, 22, 4, 3, GRANATE)
    i.rect(21, 22, 4, 3, ANIL)
    i.sombra()
    return i


def disco():
    """un disquete"""
    i = Icono()
    i.rect(4, 3, 23, 24, MARRON)
    i.rect(5, 4, 21, 22, ANIL)
    i.punto(25, 4, MARRON)
    i.rect(9, 4, 12, 8, PIEDRA)
    i.rect(17, 5, 3, 6, ANIL)
    i.rect(8, 15, 15, 11, BLANCO)
    i.rect(10, 18, 11, 1, ARENA_OSC)
    i.rect(10, 21, 11, 1, ARENA_OSC)
    i.sombra()
    return i


def documento():
    """una hoja con la esquina doblada"""
    i = Icono()
    i.rect(6, 2, 19, 26, MARRON)
    i.rect(7, 3, 17, 24, BLANCO)
    for n in range(6):
        for x in range(19 + n, 25):
            i.p[2 + n][x] = None if x > 19 + n else MARRON
    i.rect(19, 2, 1, 6, MARRON)
    i.rect(19, 7, 6, 1, MARRON)
    i.linea(19, 2, 24, 7, MARRON)
    for y in range(4, 7):
        for x in range(20, 20 + (y - 3)):
            i.punto(x, y, ARENA)
    for y in range(11, 25, 3):
        i.rect(9, y, 13 if y != 23 else 7, 1, ANIL)
    i.sombra()
    return i


def herramienta():
    """un martillo"""
    i = Icono()
    i.rect(13, 9, 6, 19, MARRON)
    i.rect(14, 10, 4, 17, OCRE)
    i.rect(14, 10, 1, 17, CREMA)
    i.rect(6, 3, 20, 9, MARRON)
    i.rect(7, 4, 18, 7, PIEDRA)
    i.rect(7, 9, 18, 2, ARENA_OSC)
    i.rect(22, 4, 3, 7, ARENA_OSC)
    i.sombra()
    return i


def musica():
    """dos corcheas unidas"""
    i = Icono()
    i.rect(10, 5, 2, 17, TINTA)
    i.rect(22, 3, 2, 17, TINTA)
    for n in range(4):
        i.linea(10, 5 + n, 23, 3 + n, TINTA)
    i.disco(8, 22, 4, TINTA)
    i.disco(20, 20, 4, TINTA)
    i.disco(8, 22, 2, GRANATE)
    i.disco(20, 20, 2, GRANATE)
    i.sombra()
    return i


def ordenes():
    """el intérprete de órdenes: pantalla negra con el indicador"""
    i = Icono()
    i.rect(2, 4, 26, 21, MARRON)
    i.rect(3, 5, 24, 19, TINTA)
    i.linea(6, 9, 10, 13, OCRE)
    i.linea(10, 13, 6, 17, OCRE)
    i.linea(7, 9, 11, 13, OCRE)
    i.linea(11, 13, 7, 17, OCRE)
    i.rect(13, 17, 7, 2, CREMA)
    i.sombra()
    return i


NUEVOS = (('carpeta', carpeta), ('programa', programa), ('juego', juego), ('disco', disco),
          ('documento', documento), ('herramienta', herramienta), ('musica', musica),
          ('ordenes', ordenes))


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
    if len(sys.argv) > 2:
        with open(sys.argv[2], 'w', encoding='utf-8') as f:
            f.write('; Hispa Ventanas - iconos de los grupos y de los accesos directos (32x32).\n'
                    '; Diseños propios, hechos aquí. GPL-2.0-or-later. Plantilla generada por\n'
                    '; tools/mkiconos.py (segundo argumento); se pueden retocar a mano.\n')
            for nombre, fn in NUEVOS:
                f.write('\nico_%s:\n' % nombre)
                for fila in fn().texto():
                    f.write('        db "%s"\n' % fila)


if __name__ == '__main__':
    main()
