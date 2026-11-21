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
  texto15, negrita15 ...         lo mismo, con la «letra grande» de
                                 controles.cas (el renglón de 24 puntos: la
                                 letra suavizada de letrasuave.cas, DejaVu
                                 Sans; en negrita, DejaVu Sans Bold); los
                                 texto2 y negrita2, con la de 32. Se busca con
                                 la letra ya rasterizada: los puntos de tinta
                                 entera, todos del mismo color; los que no
                                 toca ninguna letra, de otro (la esquina: la
                                 pluma al empezar y lo alto del renglón)
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


def letra_suave():
    """las 4 caras de letrasuave.cas (0 y 1: el renglón de 24, normal y en
    negrita; 2 y 3: el de 32): cada una, (base, tabla, comienzos, pares, puntos)"""
    datos, actual, caras = {}, None, []
    for l in open(os.path.join(AQUI, 'letrasuave.cas'), encoding='utf-8'):
        m = re.match(r'\s+(LS_[tikp]\d+):', l)
        if m:
            actual = m.group(1)
            datos[actual] = bytearray()
            continue
        m = re.match(r'\s+dd LS_t\d+, LS_i\d+, LS_k\d+, LS_p\d+, (\d+), (\d+)', l)
        if m:
            caras.append(int(m.group(2)))
            continue
        m = re.match(r'\s+db ([0-9,]+)$', l)
        if m and actual:
            datos[actual] += bytes(int(v) for v in m.group(1).split(','))
    return [(caras[n], datos['LS_t%d' % n], datos['LS_i%d' % n], datos['LS_k%d' % n], datos['LS_p%d' % n]) for n in range(4)]


def _sg(b):
    return b - 256 if b >= 128 else b


def dibujo_suave(texto, cara):
    """los puntos del texto con la letra suavizada, como los pinta
    controles.cas: {(x, y): tono de 0 a 15} (el mayor, si se pisan)"""
    sys.path.insert(0, os.path.join(AQUI, '..', '..', 'tools'))
    import hispa437
    base, tabla, ini, pares, puntos = cara
    cod = []
    for ch in texto:
        c = hispa437.codigo(ch)
        cod.append(c if c is not None else ord('?'))
    pts = {}
    x = 0
    for i, c in enumerate(cod):
        if c < 32:
            c = 32
        if i > 0 and cod[i - 1] >= 32:
            a = cod[i - 1] - 32
            j0 = ini[2 * a] | ini[2 * a + 1] << 8
            j1 = ini[2 * a + 2] | ini[2 * a + 3] << 8
            for j in range(j0, j1):
                if pares[2 * j] == c:
                    x += _sg(pares[2 * j + 1])
                    break
        f = tabla[(c - 32) * 8:(c - 31) * 8]
        an, al = f[3], f[4]
        off = f[6] | f[7] << 8
        fila = (an + 1) // 2
        gx, gy = x + _sg(f[1]), _sg(f[2])
        for yy in range(al):
            for xx in range(an):
                b = puntos[off + yy * fila + xx // 2]
                t = b & 15 if xx & 1 else b >> 4
                if t:
                    k = (gx + xx, gy + yy)
                    pts[k] = max(pts.get(k, 0), t)
        x += f[0]
    return pts


def busca_suave(cap, x0, y0, x1, y1, texto, cara):
    """dónde está el texto (la pluma al empezar y lo alto del renglón), o None"""
    pts = dibujo_suave(texto, cara)
    if not pts:
        return None
    tinta = [k for k, t in pts.items() if t == 15]
    if not tinta:
        return None
    xs = [k[0] for k in pts]
    ys = [k[1] for k in pts]
    mx, Mx, my, My = min(xs), max(xs), min(ys), max(ys)
    fondo = [(a, b) for b in range(my, My + 1) for a in range(mx, Mx + 1) if (a, b) not in pts]
    for y in range(max(-my, y0), min(y1, cap.alto - 1 - My) + 1):
        for x in range(max(-mx, x0), min(x1, cap.ancho - 1 - Mx) + 1):
            c = cap.punto(x + tinta[0][0], y + tinta[0][1])
            if all(cap.punto(x + a, y + b) == c for a, b in tinta) and \
               all(cap.punto(x + a, y + b) != c for a, b in fondo):
                return x, y
    return None


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
    suaves = None
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
            if suaves is None:
                suaves = letra_suave()
            r = busca_suave(cap, x0, y0, x1, y1, t, suaves[(2 if m == 4 else 0) + (1 if op.startswith('negrita') else 0)])
            if r is None:
                mal.append('no está «%s»%s, %s, en (%d,%d)-(%d,%d)' % (t, ' (en negrita)' if op.startswith('negrita') else '', 'con la letra grande de 32' if m == 4 else 'con la letra grande de 24', x0, y0, x1, y1))
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
