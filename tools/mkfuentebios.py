#!/usr/bin/env python3
"""Fuente 8x16 de HispaBIOS (modo texto emulado sobre GOP).

Parte de la fuente de Hispa Ventanas (la que genera tools/mkfuente.py a partir
de ventanas/fuente.txt, diseño propio) y le añade, dibujados aquí por
programa, los caracteres de CP437 que el modo texto necesita y que esa fuente
no tiene: recuadros sencillos y dobles (B3h-DAh), bloques y tramas (B0h-B2h,
DBh-DFh), flechas y triángulos (10h-1Fh, 18h-1Bh) y algunos símbolos.
GPL-2.0-or-later.

uso: mkfuentebios.py fuente_ventanas.inc salida.bin
Escribe 4096 bytes (256 caracteres x 16 filas).
"""
import re
import sys

ALTO = 16
CAJA = [0, 0, 0, 0x7C, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x7C, 0, 0, 0, 0]


def leer(inc):
    tabla = []
    for linea in open(inc, encoding='utf-8'):
        m = re.match(r'\s*db\s+([0-9A-Fx, ]+)', linea)
        if m:
            tabla.append([int(v, 16) for v in m.group(1).split(',')])
    if len(tabla) != 256:
        sys.exit('%s: no tiene 256 caracteres' % inc)
    return tabla


# recuadros: (arriba, abajo, izquierda, derecha) con 0 nada, 1 sencilla, 2 doble
LINEAS = {
    0xB3: (1, 1, 0, 0), 0xB4: (1, 1, 1, 0), 0xB5: (1, 1, 2, 0), 0xB6: (2, 2, 1, 0),
    0xB7: (0, 2, 1, 0), 0xB8: (0, 1, 2, 0), 0xB9: (2, 2, 2, 0), 0xBA: (2, 2, 0, 0),
    0xBB: (0, 2, 2, 0), 0xBC: (2, 0, 2, 0), 0xBD: (2, 0, 1, 0), 0xBE: (1, 0, 2, 0),
    0xBF: (0, 1, 1, 0), 0xC0: (1, 0, 0, 1), 0xC1: (1, 0, 1, 1), 0xC2: (0, 1, 1, 1),
    0xC3: (1, 1, 0, 1), 0xC4: (0, 0, 1, 1), 0xC5: (1, 1, 1, 1), 0xC6: (1, 1, 0, 2),
    0xC7: (2, 2, 0, 1), 0xC8: (2, 0, 0, 2), 0xC9: (0, 2, 0, 2), 0xCA: (2, 0, 2, 2),
    0xCB: (0, 2, 2, 2), 0xCC: (2, 2, 0, 2), 0xCD: (0, 0, 2, 2), 0xCE: (2, 2, 2, 2),
    0xCF: (1, 0, 2, 2), 0xD0: (2, 0, 1, 1), 0xD1: (0, 1, 2, 2), 0xD2: (0, 2, 1, 1),
    0xD3: (2, 0, 0, 1), 0xD4: (1, 0, 0, 2), 0xD5: (0, 1, 0, 2), 0xD6: (0, 2, 0, 1),
    0xD7: (2, 2, 1, 1), 0xD8: (1, 1, 2, 2), 0xD9: (1, 0, 1, 0), 0xDA: (0, 1, 0, 1),
}

CX = 3            # columna de la línea vertical sencilla (la doble: 2 y 4)
CY = 7            # fila de la línea horizontal sencilla (la doble: 6 y 8)


def recuadro(arr, aba, izq, der):
    """Brazos desde el centro de la celda: sencillo (una línea) o doble (dos)."""
    px = [[0] * 8 for _ in range(ALTO)]
    xs = lambda n: [CX] if n == 1 else [CX - 1, CX + 1]
    ys = lambda n: [CY] if n == 1 else [CY - 1, CY + 1]
    hor = max(izq, der)
    ver = max(arr, aba)
    # hasta dónde llega cada brazo: al otro lado de las líneas que cruza
    y_lo = min(ys(hor)) if hor else CY
    y_hi = max(ys(hor)) if hor else CY
    x_lo = min(xs(ver)) if ver else CX
    x_hi = max(xs(ver)) if ver else CX
    for x in (xs(arr) if arr else []):
        for y in range(0, y_hi + 1):
            px[y][x] = 1
    for x in (xs(aba) if aba else []):
        for y in range(y_lo, ALTO):
            px[y][x] = 1
    for y in (ys(izq) if izq else []):
        for x in range(0, x_hi + 1):
            px[y][x] = 1
    for y in (ys(der) if der else []):
        for x in range(x_lo, 8):
            px[y][x] = 1
    return [sum(0x80 >> x for x in range(8) if px[y][x]) for y in range(ALTO)]


def dibujar(lineas):
    return [sum(0x80 >> x for x, c in enumerate(f) if c == '#') for f in lineas]


def rellenar(tabla):
    def libre(c):
        return tabla[c] == CAJA
    for c, (a, b, i, d) in LINEAS.items():
        tabla[c] = recuadro(a, b, i, d)
    # tramas y bloques
    tabla[0xB0] = [0x88 if y % 4 == 0 else 0x22 if y % 4 == 2 else 0 for y in range(ALTO)]
    tabla[0xB1] = [0xAA if y % 2 == 0 else 0x55 for y in range(ALTO)]
    tabla[0xB2] = [0xDD if y % 2 == 0 else 0x77 for y in range(ALTO)]
    tabla[0xDB] = [0xFF] * ALTO
    tabla[0xDC] = [0] * 8 + [0xFF] * 8
    tabla[0xDD] = [0xF0] * ALTO
    tabla[0xDE] = [0x0F] * ALTO
    tabla[0xDF] = [0xFF] * 8 + [0] * 8
    tabla[0xFE] = [0] * 4 + [0x7C] * 7 + [0] * 5
    tabla[0xFA] = [0] * 7 + [0x18, 0x18] + [0] * 7 if libre(0xFA) else tabla[0xFA]
    # flechas y triángulos
    tri_d = ['#.......', '##......', '###.....', '####....', '#####...', '######..', '#######.',
             '######..', '#####...', '####....', '###.....', '##......', '#.......']
    tri_i = [f[::-1] for f in tri_d]
    tabla[0x10] = [0] + dibujar(tri_d) + [0, 0]
    tabla[0x11] = [0] + dibujar([f[1:] + '.' for f in tri_i]) + [0, 0]
    up = ['...#....', '..###...', '.#####..', '#######.']
    tabla[0x1E] = [0] * 5 + dibujar(up) + [0] * 7
    tabla[0x1F] = [0] * 5 + dibujar(up[::-1]) + [0] * 7
    flecha_ar = ['...#....', '..###...', '.#.#.#..', '...#....', '...#....', '...#....', '...#....', '...#....']
    tabla[0x18] = [0] * 3 + dibujar(flecha_ar) + [0] * 5
    tabla[0x19] = [0] * 3 + dibujar(flecha_ar[::-1]) + [0] * 5
    tabla[0x12] = [0] * 2 + dibujar(['...#....', '..###...', '.#.#.#..', '...#....', '...#....', '...#....',
                                      '.#.#.#..', '..###...', '...#....']) + [0] * 5
    tabla[0x1A] = [0] * 5 + dibujar(['....#...', '.....#..', '#######.', '.....#..', '....#...']) + [0] * 6
    tabla[0x1B] = [0] * 5 + dibujar(['..#.....', '.#......', '#######.', '.#......', '..#.....']) + [0] * 6
    tabla[0x1D] = [0] * 5 + dibujar(['..#.#...', '.#...#..', '#######.', '.#...#..', '..#.#...']) + [0] * 6
    # caras, palos, notas
    tabla[0x01] = [0] * 3 + dibujar(['.#####..', '#.....#.', '#.#.#.#.', '#.....#.', '#.#.#.#.', '#..#..#.',
                                      '#.....#.', '.#####..']) + [0] * 5
    tabla[0x02] = [0] * 3 + dibujar(['.#####..', '#######.', '##.#.##.', '#######.', '##.#.##.', '###.###.',
                                      '#######.', '.#####..']) + [0] * 5
    tabla[0x03] = [0] * 4 + dibujar(['.##.##..', '#######.', '#######.', '.#####..', '..###...', '...#....']) + [0] * 6
    tabla[0x04] = [0] * 4 + dibujar(['...#....', '..###...', '.#####..', '#######.', '.#####..', '..###...', '...#....']) + [0] * 5
    tabla[0x05] = [0] * 3 + dibujar(['..###...', '..###...', '#######.', '#######.', '#.#.#.#.', '...#....', '..###...']) + [0] * 6
    tabla[0x06] = [0] * 3 + dibujar(['...#....', '..###...', '.#####..', '#######.', '#######.', '.#.#.#..', '...#....', '..###...']) + [0] * 5
    tabla[0x07] = [0] * 6 + [0x18, 0x3C, 0x3C, 0x18] + [0] * 6
    tabla[0x08] = [0xFF ^ v for v in tabla[0x07]]
    tabla[0x09] = [0] * 5 + [0x3C, 0x66, 0x42, 0x42, 0x66, 0x3C] + [0] * 5
    tabla[0x0A] = [0xFF ^ v for v in tabla[0x09]]
    tabla[0x0E] = [0] * 3 + dibujar(['..#####.', '..#...#.', '..#####.', '..#...#.', '..#...#.', '..#..##.', '.##.###.',
                                      '###..##.', '##......']) + [0] * 4
    tabla[0x0D] = [0] * 3 + dibujar(['..####..', '..#..#..', '..####..', '..#.....', '..#.....', '.##.....', '###.....',
                                      '##......']) + [0] * 5
    tabla[0x0F] = [0] * 4 + dibujar(['...#....', '#.###.#.', '.##.##..', '###.###.', '.##.##..', '#.###.#.', '...#....']) + [0] * 5
    tabla[0x14] = [0] * 3 + dibujar(['.######.', '####.##.', '####.##.', '.###.##.', '...#.##.', '...#.##.', '...#.##.',
                                      '...#.##.']) + [0] * 5
    tabla[0x16] = [0] * 10 + [0xFE] * 3 + [0] * 3
    tabla[0x17] = [0] * 2 + dibujar(['...#....', '..###...', '.#.#.#..', '...#....', '...#....', '.#.#.#..', '..###...',
                                      '...#....', '#######.']) + [0] * 5
    tabla[0x1C] = [0] * 6 + dibujar(['#.......', '#.......', '#######.']) + [0] * 7
    tabla[0x7F] = [0] * 5 + dibujar(['...#....', '..#.#...', '.#...#..', '#.....#.', '#.....#.', '#######.']) + [0] * 5
    tabla[0xF9] = [0] * 7 + [0x18] + [0] * 8 if libre(0xF9) else tabla[0xF9]
    # el resto que quede sin diseño: la caja (que se vea que falta)
    return tabla


def main():
    tabla = rellenar(leer(sys.argv[1]))
    with open(sys.argv[2], 'wb') as f:
        for g in tabla:
            f.write(bytes(g))


if __name__ == '__main__':
    main()
