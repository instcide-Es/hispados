#!/usr/bin/env python3
"""Lo que Lector manda a la impresora al imprimir como se ve (ESC/P de agujas).

uso: lector_impresora.py LPT.BIN

LPT.BIN es lo que ha salido por LPT1 (QEMU con -parallel file:...) al
imprimir ETIQUETA.PDF (tests/tools/hazpdf.py) dos veces: primero la página 1
(Ctrl+P) y después el documento entero. Se descodifica como lo haría una
impresora de agujas: ESC @ (empieza), ESC A n (avance de n/72), ESC * 5 n1 n2
(una tira de 8 agujas de n1 + 256·n2 columnas, el bit 7 arriba), CR, LF
(avanza), FF (hoja fuera) y ESC 2 (avance de siempre). Y se comprueba que
salen las barras del código de barras donde tocan, punto por punto, y el
rectángulo negro y el gris de la hoja A4, reducida para que quepa.

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hazpdf

AN = 576
fallos = 0


def mira(texto, cond):
    global fallos
    print(('  OK   ' if cond else '  FAIL ') + texto)
    if not cond:
        fallos += 1


def descodifica(d):
    """-> lista de trabajos; cada uno, una lista de hojas; cada hoja, un
    diccionario {fila: bytearray(AN) de 0/1} y su alto (lo avanzado)."""
    trabajos, raros, anchos = [], [], set()
    hoja, fila, avance, fin = None, 0, 12, []
    i = 0
    while i < len(d):
        c = d[i]
        if c == 27:
            o = d[i + 1]
            if o == ord('@'):
                trabajos.append([])
                hoja, fila, avance = {}, 0, 12
                i += 2
            elif o == ord('A'):
                avance = d[i + 2]
                i += 3
            elif o == ord('2'):
                avance = 12
                fin.append(len(trabajos))
                i += 2
            elif o == ord('*'):
                m, n = d[i + 2], d[i + 3] + 256 * d[i + 4]
                anchos.add((m, n))
                cols = d[i + 5:i + 5 + n]
                for x, b in enumerate(cols):
                    for k in range(8):
                        if b & (0x80 >> k):
                            hoja.setdefault(fila + k, bytearray(AN + 8))[x] = 1
                i += 5 + n
            else:
                raros.append((i, 'ESC %r' % chr(o)))
                i += 2
        elif c == 13:
            i += 1
        elif c == 10:
            fila += avance
            i += 1
        elif c == 12:
            trabajos[-1].append((hoja, fila))
            hoja, fila = {}, 0
            i += 1
        else:
            raros.append((i, c))
            i += 1
    return trabajos, raros, anchos, fin


def negro(h, x, y):
    f = h[0].get(y)
    return bool(f and f[x])


def cuenta(h, x0, y0, x1, y1):
    return sum(negro(h, x, y) for y in range(y0, y1) for x in range(x0, x1))


def main():
    d = open(sys.argv[1], 'rb').read()
    mira('empieza por ESC @ y el avance de 8/72 (ESC A 8)', d[:5] == b'\x1b@\x1bA\x08')
    trabajos, raros, anchos, fin = descodifica(d)
    mira('solo órdenes gráficas (nada de texto suelto)%s' % ('' if not raros else ': %r' % raros[:5]), not raros)
    mira('las tiras, todas ESC * 5 de 576 columnas (72 puntos por pulgada) %r' % sorted(anchos), anchos == {(5, 576)})
    mira('dos impresiones: la página y el documento (%d)' % len(trabajos), len(trabajos) == 2)
    mira('cada una acaba con FF y ESC 2 (el avance de siempre)', fin == [1, 2] and d.endswith(b'\x0c\x1b2'))
    if len(trabajos) != 2 or len(trabajos[0]) != 1 or len(trabajos[1]) != 2:
        mira('una hoja en la primera y dos en la segunda (%s)' % [len(t) for t in trabajos], False)
        return
    et, et2, a4 = trabajos[0][0], trabajos[1][0], trabajos[1][1]
    mira('la etiqueta, igual sola que con el documento', et[0] == et2[0])
    # ---- la etiqueta: 288 x 144 puntos, a su tamaño, centrada (144 a la izquierda) ----
    mira('la etiqueta: 18 tiras (144 puntos de alto, su tamaño de verdad; avanza %d)' % et[1], et[1] == 144)
    ox = (AN - 288) // 2
    en_barra = set()
    for x, w in hazpdf.BARRAS:
        en_barra.update(range(ox + x, ox + x + w))
    x0, x1 = ox + hazpdf.BARRAS[0][0] - 4, ox + hazpdf.BARRAS[-1][0] + hazpdf.BARRAS[-1][1] + 4
    mal = [(x, y) for y in range(34, 104) for x in range(x0, x1) if negro(et, x, y) != (x in en_barra)]
    mira('las %d barras, punto por punto en su sitio (filas 34 a 103; %d puntos mal%s)' % (
        len(hazpdf.BARRAS), len(mal), '' if not mal else ', p. ej. %r' % mal[:6]), not mal)
    mira('encima de las barras, en blanco', cuenta(et, 0, 0, AN, 34) == 0)
    mira('entre las barras y el número, en blanco', cuenta(et, 0, 104, AN, 110) == 0)
    n = cuenta(et, ox + 28, 110, ox + 120, 126)
    mira('el número «LIBRO 000123», debajo (%d puntos negros)' % n, 60 < n < 900)
    mira('fuera de la etiqueta, en blanco', cuenta(et, 0, 0, ox, 144) == 0 and cuenta(et, ox + 288, 0, AN, 144) == 0)
    # ---- la hoja A4: no cabe en 576 x 760, se reduce a 760 de alto ----
    mira('la hoja A4: 95 tiras (760 puntos; avanza %d)' % a4[1], a4[1] == 760)
    z = 760 / hazpdf.A4[1]
    ox = (AN - round(hazpdf.A4[0] * z)) // 2
    px = lambda v: ox + round(v * z)
    py = lambda v: round((hazpdf.A4[1] - v) * z)
    n = cuenta(a4, px(100) + 2, py(700) + 2, px(300) - 2, py(600) - 2)
    t = (px(300) - px(100) - 4) * (py(600) - py(700) - 4)
    mira('el rectángulo negro, macizo (%d de %d)' % (n, t), n == t)
    mira('y fuera de él, alrededor, en blanco', cuenta(a4, px(100) - 6, py(700) - 6, px(100) - 2, py(600) + 6) == 0
         and cuenta(a4, px(100) - 6, py(600) + 2, px(300) + 6, py(600) + 6) == 0)
    n = cuenta(a4, px(350) + 2, py(700) + 2, px(500) - 2, py(600) - 2)
    t = (px(500) - px(350) - 4) * (py(600) - py(700) - 4)
    mira('el rectángulo gris, tramado (%d de %d puntos negros)' % (n, t), 0.4 * t < n < 0.95 * t)
    mira('entre los dos, en blanco', cuenta(a4, px(300) + 3, py(700), px(350) - 3, py(600)) == 0)
    n = cuenta(a4, px(100) - 2, py(780 + 14), px(300), py(780) + 4)
    mira('el título «ETIQUETAS DE PRUEBA» (%d puntos negros)' % n, 150 < n < 3000)
    mira('debajo de los rectángulos, en blanco', cuenta(a4, 0, py(600) + 3, AN, 760) == 0)


main()
sys.exit(1 if fallos else 0)
