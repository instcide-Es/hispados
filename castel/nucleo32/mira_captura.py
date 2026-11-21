#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/mira_captura.py CAPTURA.ppm COMPROBACIÓN...

Mira puntos de una captura de la pantalla del escritorio de Hispa-Pentium (un
PPM de QEMU: «screendump» en su monitor), como hacen las pruebas de Hispa
Ventanas. Las comprobaciones:

  tam ANCHO ALTO                 lo que mide la captura
  rgb X Y RRGGBB [MARGEN]        el punto es de ese color (con ese margen por
                                 cada componente; 6 si no se dice)
  nrgb X Y RRGGBB [MARGEN]       el punto NO es de ese color
  texto X0 Y0 X1 Y1 TEXTO        el texto, pintado con la letra del sistema
                                 (nucleo32/letra.cas) en cualquier color, está
                                 con su esquina de arriba a la izquierda en
                                 algún punto de ese rectángulo
  negrita X0 Y0 X1 Y1 TEXTO      lo mismo, en negrita (cada letra, dos veces)
  notexto X0 Y0 X1 Y1 TEXTO      el texto (fino o en negrita) NO está ahí
  texto2, negrita2 ...           como texto y negrita, con la letra a doble
                                 tamaño (16x32: cada punto, 2x2; en negrita,
                                 otra vez un punto a la derecha)
  texto15, negrita15 ...         lo mismo, a vez y media (12x24: cada punto,
                                 uno o dos, alternando), la «letra grande» de
                                 controles.cas
  igual X Y ANCHO ALTO OTRA.ppm  ese rectángulo es igual en la otra captura
  distinto X Y ANCHO ALTO OTRA.ppm   y que no lo es

Con «-d», dice dónde ha encontrado cada texto. Acaba con 0 si todo está bien;
si no, dice qué falla y acaba con 1.
"""
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))


def letra():
    """las 256 letras de letra.cas: cada una, sus 16 filas"""
    filas = []
    for l in open(os.path.join(AQUI, 'letra.cas'), encoding='utf-8'):
        m = re.match(r'\s+db\s+([0-9A-Fa-fx,]+)', l)
        if m:
            filas.append([int(v, 16) for v in m.group(1).split(',')])
    assert len(filas) == 256
    return filas


class Captura:
    def __init__(self, ruta):
        d = open(ruta, 'rb').read()
        m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
        if not m:
            sys.exit('mira_captura.py: %s no es un PPM' % ruta)
        self.ancho, self.alto = int(m.group(1)), int(m.group(2))
        self.d = d[m.end():]

    def punto(self, x, y):
        o = (y * self.ancho + x) * 3
        return self.d[o:o + 3]


def dibujo(texto, negrita, tabla, m=2):
    """los puntos del texto: una lista de filas, cada una una lista de 0 y 1
    (m: la escala en medios, como controles.cas: 2, 8x16; 3, 12x24; 4, 16x32)"""
    cod = texto.encode('cp437', 'replace')
    ancho = len(cod) * 4 * m + (1 if negrita else 0)
    filas = [[0] * ancho for _ in range(8 * m)]
    for i, c in enumerate(cod):
        for f in range(16):
            v = tabla[c][f]
            for b in range(8):
                if v & (0x80 >> b):
                    for yy in range(f * m // 2, (f + 1) * m // 2):
                        for xx in range(b * m // 2, (b + 1) * m // 2):
                            filas[yy][i * 4 * m + xx] = 1
                            if negrita:
                                filas[yy][i * 4 * m + xx + 1] = 1
    return filas


def busca(cap, x0, y0, x1, y1, texto, negrita, tabla, m=2):
    """dónde está el texto (x, y), o None"""
    filas = dibujo(texto, negrita, tabla, m)
    ancho = len(filas[0])
    alto = len(filas)
    tinta = [(x, f) for f in range(alto) for x in range(ancho) if filas[f][x]]
    fondo = [(x, f) for f in range(alto) for x in range(ancho) if not filas[f][x]]
    if not tinta:
        return None
    for y in range(max(0, y0), min(y1, cap.alto - alto) + 1):
        for x in range(max(0, x0), min(x1, cap.ancho - ancho) + 1):
            c = cap.punto(x + tinta[0][0], y + tinta[0][1])
            if all(cap.punto(x + a, y + b) == c for a, b in tinta) and \
               all(cap.punto(x + a, y + b) != c for a, b in fondo):
                return x, y
    return None


def main():
    a = sys.argv[1:]
    dice = False
    if a and a[0] == '-d':
        dice = True
        a = a[1:]
    if len(a) < 1:
        sys.exit(__doc__)
    cap = Captura(a[0])
    tabla = letra()
    mal = []
    i = 1
    while i < len(a):
        op = a[i]
        if op == 'tam':
            if (cap.ancho, cap.alto) != (int(a[i + 1]), int(a[i + 2])):
                mal.append('mide %dx%d y no %sx%s' % (cap.ancho, cap.alto, a[i + 1], a[i + 2]))
            i += 3
        elif op in ('rgb', 'nrgb'):
            x, y, c = int(a[i + 1]), int(a[i + 2]), bytes.fromhex(a[i + 3])
            i += 4
            margen = 6
            if i < len(a) and a[i].isdigit():
                margen = int(a[i])
                i += 1
            p = cap.punto(x, y)
            es = all(abs(p[k] - c[k]) <= margen for k in range(3))
            if es != (op == 'rgb'):
                mal.append('el punto (%d,%d) es %s y %s %s' % (x, y, p.hex().upper(), 'se esperaba' if op == 'rgb' else 'no tenía que ser', c.hex().upper()))
        elif op in ('texto2', 'negrita2', 'texto15', 'negrita15'):
            x0, y0, x1, y1 = (int(v) for v in a[i + 1:i + 5])
            t = a[i + 5]
            i += 6
            m = 4 if op.endswith('2') else 3
            r = busca(cap, x0, y0, x1, y1, t, op.startswith('negrita'), tabla, m)
            if r is None:
                mal.append('no está «%s»%s, %s, en (%d,%d)-(%d,%d)' % (t, ' (en negrita)' if op.startswith('negrita') else '', 'a doble tamaño' if m == 4 else 'a vez y media', x0, y0, x1, y1))
            elif dice:
                print('«%s» en (%d,%d)' % (t, r[0], r[1]))
        elif op in ('texto', 'negrita', 'notexto'):
            x0, y0, x1, y1 = (int(v) for v in a[i + 1:i + 5])
            t = a[i + 5]
            i += 6
            if op == 'notexto':
                if busca(cap, x0, y0, x1, y1, t, False, tabla) or busca(cap, x0, y0, x1, y1, t, True, tabla):
                    mal.append('«%s» está en (%d,%d)-(%d,%d), y no tenía que estar' % (t, x0, y0, x1, y1))
            else:
                r = busca(cap, x0, y0, x1, y1, t, op == 'negrita', tabla)
                if r is None:
                    mal.append('no está «%s»%s en (%d,%d)-(%d,%d)' % (t, ' (en negrita)' if op == 'negrita' else '', x0, y0, x1, y1))
                elif dice:
                    print('«%s» en (%d,%d)' % (t, r[0], r[1]))
        elif op in ('igual', 'distinto'):
            x, y, w, h = (int(v) for v in a[i + 1:i + 5])
            otra = Captura(a[i + 5])
            i += 6
            es = all(cap.d[((y + f) * cap.ancho + x) * 3:((y + f) * cap.ancho + x + w) * 3] ==
                     otra.d[((y + f) * otra.ancho + x) * 3:((y + f) * otra.ancho + x + w) * 3] for f in range(h))
            if es != (op == 'igual'):
                mal.append('la zona (%d,%d) %dx%d %s' % (x, y, w, h, 'ha cambiado' if op == 'igual' else 'no ha cambiado'))
        else:
            sys.exit('mira_captura.py: no entiendo «%s»' % op)
    for m in mal:
        print('mira_captura.py: %s: %s' % (os.path.basename(a[0]), m))
    sys.exit(1 if mal else 0)


if __name__ == '__main__':
    main()
