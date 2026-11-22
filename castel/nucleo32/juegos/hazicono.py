#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/juegos/hazicono.py JUEGO SALIDA.ICP

El icono de 32 x 32 del acceso del menú Inicio de un juego (un .ICP:
«HPIC», el ancho, el alto y 32 x 32 puntos 0xAARRGGBB; ver tools/hpicono.py).
Los de los juegos que vienen de Hispa-386 (culebra, minas, solitario,
tragabolas, almacen) son los de su cabecera HVP, con la paleta de Hispa
Ventanas; los nuevos (bloques, rompemur, pinball) se dibujan aquí, con
diseño propio. Sin bibliotecas de fuera.
"""
import os
import struct
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import datos386  # noqa: E402

PALETA = [0x1A1410, 0x3B2A20, 0x1F4E5A, 0x2C6675, 0xC2542D, 0xE07B4F, 0xC9B79C, 0x8F7F68,
          0xFBF3E0, 0xFFFFFF, 0xE8B33A, 0x6B8E3D, 0x2F4B8F, 0x9E2B2B, 0x7FC6BF, 0xD8D2C4]
N = 32


def de386(nombre):
    p = [[0] * N for _ in range(N)]
    for y, fila in enumerate(datos386.icono(nombre)):
        for x, ch in enumerate(fila):
            if ch != '.':
                p[y][x] = 0xFF000000 | PALETA[int(ch, 16)]
    return p


class Lienzo:
    def __init__(self):
        self.p = [[0] * N for _ in range(N)]

    def punto(self, x, y, c):
        if 0 <= x < N and 0 <= y < N:
            self.p[y][x] = 0xFF000000 | c

    def rect(self, x, y, an, al, c):
        for j in range(y, y + al):
            for i in range(x, x + an):
                self.punto(i, j, c)

    def circulo(self, cx, cy, r, c):
        for j in range(N):
            for i in range(N):
                if (i + 0.5 - cx) ** 2 + (j + 0.5 - cy) ** 2 <= r * r:
                    self.punto(i, j, c)

    def azulejo(self, x, y, l, c, claro, oscuro, trama):
        self.rect(x, y, l, l, c)
        self.rect(x, y, l, 1, claro)
        self.rect(x, y, 1, l, claro)
        self.rect(x, y + l - 1, l, 1, oscuro)
        self.rect(x + l - 1, y, 1, l, oscuro)
        m = l // 2
        if trama == 'rombo':
            for k in range(-2, 3):
                self.rect(x + m - (2 - abs(k)), y + m + k, 2 * (2 - abs(k)) + 1, 1, claro)
        elif trama == 'rayas':
            for k in range(2, l - 2, 3):
                self.rect(x + 2, y + k, l - 4, 1, oscuro)
        elif trama == 'puntos':
            for a in (l // 3, 2 * l // 3):
                for b in (l // 3, 2 * l // 3):
                    self.rect(x + a - 1, y + b - 1, 2, 2, claro)
        elif trama == 'diagonal':
            for k in range(l):
                if (k % 3) == 0:
                    for t in range(l - 2):
                        if 1 <= t + k - l // 2 < l - 1:
                            self.punto(x + 1 + t, y + (t + k - l // 2), oscuro)


def bloques():
    z = Lienzo()
    # un pozo con piezas encajadas y una que cae
    z.rect(4, 2, 24, 28, 0x24364A)
    z.rect(3, 2, 1, 29, 0x8FA3B8)
    z.rect(28, 2, 1, 29, 0x8FA3B8)
    z.rect(3, 30, 26, 1, 0x8FA3B8)
    L = 6
    abajo = [(0, 'rayas', 0xC0573E), (1, 'rayas', 0xC0573E), (2, 'rayas', 0xC0573E), (3, 'puntos', 0x2F5D9E)]
    for i, t, c in abajo:
        z.azulejo(4 + i * L, 24, L, c, 0xF0D0C0 if c == 0xC0573E else 0xB8CCEE, 0x5A2418 if c == 0xC0573E else 0x14284A, t)
    for i, j, c, cl, os_, t in ((0, 18, 0x6E8B3D, 0xD4E4B0, 0x2E3C14, 'diagonal'), (1, 18, 0x6E8B3D, 0xD4E4B0, 0x2E3C14, 'diagonal'),
                                (3, 18, 0x2F5D9E, 0xB8CCEE, 0x14284A, 'puntos'), (2, 12, 0xD0A030, 0xFFF0C0, 0x5C4410, 'rombo'),
                                (1, 6, 0xD0A030, 0xFFF0C0, 0x5C4410, 'rombo'), (2, 6, 0xD0A030, 0xFFF0C0, 0x5C4410, 'rombo'),
                                (3, 6, 0xD0A030, 0xFFF0C0, 0x5C4410, 'rombo')):
        z.azulejo(4 + i * L, j, L, c, cl, os_, t)
    return z.p


def rompemur():
    z = Lienzo()
    z.rect(1, 1, 30, 30, 0x16213A)
    colores = [(0xC0573E, 0xF0B8A0), (0xD0A030, 0xFFE8A8), (0x2E8C83, 0xA8E4DC), (0x8A4F7D, 0xE0B8D8)]
    for f, (c, cl) in enumerate(colores):
        for k in range(4):
            x = 2 + k * 7 + (3 if f % 2 else 0)
            if x + 6 > 30:
                continue
            z.rect(x, 3 + f * 4, 6, 3, c)
            z.rect(x, 3 + f * 4, 6, 1, cl)
    z.circulo(17.5, 22.5, 2.3, 0xF4F4F4)
    z.punto(16, 21, 0xFFFFFF)
    z.rect(9, 27, 14, 3, 0x9AA8BC)
    z.rect(9, 27, 14, 1, 0xE8EEF6)
    z.rect(9, 27, 2, 3, 0xC0573E)
    z.rect(21, 27, 2, 3, 0xC0573E)
    return z.p


def pinball():
    z = Lienzo()
    # la mesa en perspectiva (un trapecio), un bumper, la bola y un flipper
    for j in range(2, 31):
        t = (j - 2) / 28.0
        a = int(round(9 - 7 * t))
        z.rect(a, j, 32 - 2 * a, 1, 0x2A1E5C if (j // 3) % 2 else 0x31236A)
    for j in range(2, 31):
        t = (j - 2) / 28.0
        a = int(round(9 - 7 * t))
        z.punto(a, j, 0xE8B33A)
        z.punto(31 - a, j, 0xE8B33A)
    z.circulo(16, 10, 4.2, 0xC0573E)
    z.circulo(16, 9.4, 2.6, 0xFFD27A)
    z.circulo(9.5, 13, 2.2, 0x2E8C83)
    z.circulo(22.5, 13, 2.2, 0x2E8C83)
    # el flipper (de abajo a la izquierda, inclinado)
    for k in range(9):
        z.rect(8 + k, 25 + k // 3, 2, 2, 0xF4F4F4)
    for k in range(9):
        z.rect(22 - k, 25 + k // 3, 2, 2, 0xF4F4F4)
    z.circulo(19.5, 19.5, 2.4, 0xD8DEE8)
    z.punto(18, 18, 0xFFFFFF)
    z.punto(19, 18, 0xFFFFFF)
    return z.p


DE_386 = {'culebra': 'culebra.asm', 'minas': 'minas.asm', 'solitario': 'solitario.asm',
          'tragabolas': 'tragabolas.asm', 'almacen': 'almacen.asm'}
NUEVOS = {'bloques': bloques, 'rompemur': rompemur, 'pinball': pinball}


def main():
    if len(sys.argv) != 3:
        sys.exit('uso: hazicono.py JUEGO SALIDA.ICP')
    j = sys.argv[1]
    if j in DE_386:
        p = de386(DE_386[j])
    elif j in NUEVOS:
        p = NUEVOS[j]()
    else:
        sys.exit('hazicono: no sé el icono de «%s»' % j)
    d = b'HPIC' + struct.pack('<II', N, N) + b''.join(struct.pack('<I', p[y][x]) for y in range(N) for x in range(N))
    with open(sys.argv[2], 'wb') as f:
        f.write(d)


if __name__ == '__main__':
    main()
