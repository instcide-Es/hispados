#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_icono.py SALIDA.PNG - el icono del Reproductor (32 x 32, diseño propio):
# un cuadrado redondeado azul con una corchea blanca y, abajo a la derecha,
# el triángulo verde de «reproducir». Se hace PNG con zlib (sin bibliotecas).
import struct, sys, zlib

N = 32
px = [[(0, 0, 0, 0)] * N for _ in range(N)]


def mezcla(x, y, c, a):
    if 0 <= x < N and 0 <= y < N and a > 0:
        r, g, b, aa = px[y][x]
        t = a / 255.0
        px[y][x] = (int(r * (1 - t) + c[0] * t), int(g * (1 - t) + c[1] * t), int(b * (1 - t) + c[2] * t), max(aa, a))


def dentro(f, x, y):
    k = 0
    for sy in range(4):
        for sx in range(4):
            if f(x + (sx + 0.5) / 4, y + (sy + 0.5) / 4):
                k += 1
    return k * 255 // 16


def redondo(x, y, x0, y0, x1, y1, r):
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r and x0 <= x <= x1 and y0 <= y <= y1


for y in range(N):
    for x in range(N):
        # el fondo: degradado azul (de claro arriba a oscuro abajo), con borde
        a = dentro(lambda u, v: redondo(u, v, 1, 1, 31, 31, 7), x, y)
        if a:
            t = y / 31
            mezcla(x, y, (20, 51, 106), a)
            a2 = dentro(lambda u, v: redondo(u, v, 2, 2, 30, 30, 6), x, y)
            mezcla(x, y, (int(90 + (36 - 90) * t), int(166 + (104 - 166) * t), int(244 + (200 - 244) * t)), a2)
for y in range(N):
    for x in range(N):
        # la corchea: la cabeza (una elipse), el palo y la bandera
        a = dentro(lambda u, v: ((u - 11.5) / 4.6) ** 2 + ((v - 22.5) / 3.6) ** 2 <= 1, x, y)
        a = max(a, dentro(lambda u, v: 14.3 <= u <= 16.3 and 7 <= v <= 22.5, x, y))
        a = max(a, dentro(lambda u, v: 14.3 <= u <= 22 and 7 <= v <= 10.5 - (u - 14.3) * 0.15, x, y))
        a = max(a, dentro(lambda u, v: 20 <= u <= 22 and 7 <= v <= 14, x, y))
        mezcla(x, y, (255, 255, 255), a)
for y in range(N):
    for x in range(N):
        # «reproducir»: un triángulo verde con su borde oscuro
        def tri(u, v, m=0.0):
            return u >= 19 + m and v >= 18 + m * 1.2 + (u - 19) * 0.55 and v <= 30 - m * 1.2 - (u - 19) * 0.55
        mezcla(x, y, (18, 80, 30), dentro(tri, x, y))
        mezcla(x, y, (95, 203, 107), dentro(lambda u, v: tri(u, v, 1.2), x, y))

filas = b''.join(b'\0' + b''.join(bytes(p) for p in fila) for fila in px)


def trozo(t, d):
    return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)


open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', N, N, 8, 6, 0, 0, 0)) +
                              trozo(b'IDAT', zlib.compress(filas, 9)) + trozo(b'IEND', b''))
