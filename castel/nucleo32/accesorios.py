#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/accesorios.py CARPETA [--raiz] [--escritorio]

(ACCESORIOS) Deja en CARPETA los accesos del menú Inicio de los accesorios
(la Calculadora, el Reloj y el Mapa de caracteres: CALC.MNU, RELOJ.MNU y
CARACTER.MNU, castel/nucleo32/accesos.cas) y sus iconos (CALC.ICP, RELOJ.ICP
y CARACT.ICP: 32 x 32 puntos 0xAARRGGBB, dibujados aquí mismo, sin PNG).
Los .MNU van en la página 437, con CR LF, y con «tipo=Accesorios» (la
carpeta de «Todos los programas» de la 0.8.5; un menú de antes no lo mira).

Sin más, para el sistema instalado (el paquete «accesori»): los programas en
\\HISPA y los iconos en \\HISPA\\MENU. Con --raiz, para un disco sin instalar
(las pruebas): los programas en la raíz y los iconos en \\MENU. Con
--escritorio, también con su icono en el tapiz.
"""
import os
import struct
import sys

LADO = 32
ACCESOS = [
    ('CALC', 'Calculadora', 'CALC.BIN', 'CALC.ICP'),
    ('RELOJ', 'Reloj', 'RELOJ.BIN', 'RELOJ.ICP'),
    ('CARACTER', 'Mapa de caracteres', 'CARACTER.BIN', 'CARACT.ICP'),
]


def lienzo():
    return [[0] * LADO for _ in range(LADO)]


def rect(p, x, y, an, al, c):
    for j in range(y, y + al):
        for i in range(x, x + an):
            if 0 <= i < LADO and 0 <= j < LADO:
                p[j][i] = 0xFF000000 | c


def circulo(p, cx, cy, r, c):
    for j in range(LADO):
        for i in range(LADO):
            if (i + 0.5 - cx) ** 2 + (j + 0.5 - cy) ** 2 <= r * r:
                p[j][i] = 0xFF000000 | c


def linea(p, x0, y0, x1, y1, c, grueso=2):
    n = max(abs(x1 - x0), abs(y1 - y0)) * 4 + 1
    for k in range(n + 1):
        x = x0 + (x1 - x0) * k / n
        y = y0 + (y1 - y0) * k / n
        rect(p, int(x - grueso / 2 + 0.5), int(y - grueso / 2 + 0.5), grueso, grueso, c)


def icono_calc():
    p = lienzo()
    rect(p, 5, 1, 22, 30, 0x374151)          # la caja
    rect(p, 7, 3, 18, 7, 0xDCFCE7)           # el visor
    rect(p, 17, 5, 2, 3, 0x111827)
    rect(p, 21, 5, 2, 3, 0x111827)
    for f in range(4):
        for c in range(3):
            rect(p, 7 + c * 6, 12 + f * 5, 5, 4, 0xF9FAFB)
    rect(p, 19, 22, 5, 9 - 1, 0x166534)      # el igual, verde
    return p


def icono_reloj():
    p = lienzo()
    circulo(p, 16, 16, 15, 0x1F2937)
    circulo(p, 16, 16, 13, 0xFFFFFF)
    for k in range(12):
        import math
        a = k * math.pi / 6
        x = 16 + 11 * math.sin(a)
        y = 16 - 11 * math.cos(a)
        rect(p, int(x - 1 + 0.5), int(y - 1 + 0.5), 2, 2, 0x374151)
    linea(p, 16, 16, 16, 7, 0x111827, 2)      # el minutero
    linea(p, 16, 16, 22, 19, 0x111827, 3)     # la aguja de las horas
    circulo(p, 16, 16, 2, 0xB45309)
    return p


# una «Á» de 5 x 7 puntos, para el icono del mapa
A_TILDE = ['...#.', '..#..', '.###.', '#...#', '#####', '#...#', '#...#']


def icono_caract():
    p = lienzo()
    rect(p, 2, 2, 28, 28, 0x6B7280)
    rect(p, 3, 3, 26, 26, 0xFFFFFF)
    for k in range(1, 4):
        rect(p, 3 + k * 26 // 4, 3, 1, 26, 0xD1D5DB)
        rect(p, 3, 3 + k * 26 // 4, 26, 1, 0xD1D5DB)
    rect(p, 3 + 26 // 4 + 1, 3 + 26 // 4 + 1, 26 // 4 * 2, 26 // 4 * 2, 0x1D4ED8)
    for j, fila in enumerate(A_TILDE):
        for i, c in enumerate(fila):
            if c == '#':
                rect(p, 11 + i * 2, 9 + j * 2, 2, 2, 0xFFFFFF)
    return p


def guarda_icp(nombre, p):
    with open(nombre, 'wb') as f:
        f.write(b'HPIC' + struct.pack('<II', LADO, LADO))
        for fila in p:
            f.write(struct.pack('<%dI' % LADO, *fila))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    d = sys.argv[1]
    raiz = '--raiz' in sys.argv
    tapiz = '--escritorio' in sys.argv
    os.makedirs(d, exist_ok=True)
    iconos = {'CALC.ICP': icono_calc(), 'RELOJ.ICP': icono_reloj(), 'CARACT.ICP': icono_caract()}
    for n, p in iconos.items():
        guarda_icp(os.path.join(d, n), p)
    for base, nombre, prog, ico in ACCESOS:
        if raiz:
            prog_ruta, ico_ruta = '\\' + prog, '\\MENU\\' + ico
        else:
            prog_ruta, ico_ruta = '\\HISPA\\' + prog, '\\HISPA\\MENU\\' + ico
        lineas = ['nombre=' + nombre, 'programa=' + prog_ruta, 'icono=' + ico_ruta, 'tipo=Accesorios']
        if tapiz:
            lineas.append('escritorio=si')
        open(os.path.join(d, base + '.MNU'), 'wb').write(('\r\n'.join(lineas) + '\r\n').encode('cp437'))


if __name__ == '__main__':
    main()
