#!/usr/bin/env python3
"""Comprobaciones de píxeles sobre capturas de Hispa Ventanas (PPM de QEMU).

uso: ventanas_check.py CAPTURA.ppm COMPROBACION...
  tam ANCHO ALTO                    tamaño de la captura
  px X Y COLOR [nombre]             el punto es de ese color de la paleta
  rgb X Y RRGGBB [nombre]           el punto es de ese color (rojo, verde, azul)
  ventana X Y ANCHO ALTO activa|no  marco, barra de título, interior y sombra
  marco X Y ANCHO ALTO activa|no    lo mismo sin mirar el interior (ventanas
                                    con controles o maximizadas)
  titulo X Y activa|no TEXTO        el título de la ventana que está en X,Y
  texto X Y COLOR FONDO fina|negrita TEXTO   texto dibujado con la fuente propia
  indice X Y COLOR TEXTO            el índice de una carta del Solitario (14x9
                                    puntos: las filas 3 a 11 de la fuente, en
                                    negrita; dos cifras, juntas) sobre blanco
  indice_rev X Y COLOR TEXTO        el mismo, girado media vuelta (el de abajo)
  cursor X Y                        el cursor del ratón tiene la punta ahí
  igual X Y ANCHO ALTO OTRA.ppm X2 Y2 MAXDIF   la zona es igual que en OTRA
  busca X0 Y0 X1 Y1 COLOR FONDO TEXTO   el texto (fino) está en algún sitio con
                                    la esquina dentro del rectángulo
  nobusca X0 Y0 X1 Y1 COLOR FONDO TEXTO  y que no está
  cambia X Y ANCHO ALTO OTRA.ppm MINDIF        la zona ha cambiado respecto a OTRA
Los colores se dan por su número (0-15) en la paleta de ventanas/datos.inc (o
16-255: el cubo de 6x6x6 y los grises de ventanas/video.inc) y se comparan con un margen (las pantallas de 15/16 bits y el DAC de 6 bits
redondean). La fuente esperada se lee de build/gen/ventanas/fuente.inc.
Escribe una línea «  OK   ...» o «  FAIL ...» por comprobación.
Pixel checks on Hispa Ventanas screenshots.
"""
import os, re, sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
MARGEN = 10
BORDE, TIT_H, SOMBRA = 2, 22, 5
TINTA, MARRON, PETROLEO, PETROLEO_CL, TERRACOTA = 0, 1, 2, 3, 4
ARENA, CREMA, OCRE = 6, 8, 10


def paleta():
    cols, dentro = [], False
    for l in open(os.path.join(RAIZ, 'ventanas', 'datos.inc'), encoding='utf-8'):
        if l.startswith('paleta16:'):
            dentro = True
            continue
        if dentro:
            m = re.match(r'\s+db\s+0x(\w+),\s*0x(\w+),\s*0x(\w+)', l)
            if m:
                cols.append(tuple(int(x, 16) for x in m.groups()))
            elif cols:
                break
    assert len(cols) == 16, 'no se ha podido leer la paleta'
    # el cubo de 6x6x6 (16-231) y los grises (232-255), como vid_paleta
    cols += [(r * 51, g * 51, b * 51) for r in range(6) for g in range(6) for b in range(6)]
    cols += [(i * 11,) * 3 for i in range(24)]
    return cols


_fuente = []


def fuente():
    if _fuente:
        return _fuente[0]
    tabla = []
    for l in open(os.path.join(RAIZ, 'build', 'gen', 'ventanas', 'fuente.inc'), encoding='utf-8'):
        m = re.match(r'\s+db\s+([0-9A-Fa-fx, ]+)', l)
        if m:
            tabla.append([int(x, 16) for x in m.group(1).replace(' ', '').split(',')])
    assert len(tabla) == 256
    _fuente.append(tabla)
    return tabla


class Captura:
    def __init__(self, ruta):
        d = open(ruta, 'rb').read()
        m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
        self.w, self.h = int(m.group(1)), int(m.group(2))
        self.d = d[m.end():]

    def px(self, x, y):
        if not (0 <= x < self.w and 0 <= y < self.h):
            return (-999, -999, -999)
        o = (y * self.w + x) * 3
        return tuple(self.d[o:o + 3])


PAL = paleta()
fallos = 0


def es(c, n):
    return all(abs(a - b) <= MARGEN for a, b in zip(c, PAL[n]))


def res(ok, texto):
    global fallos
    print(('  OK   ' if ok else '  FAIL ') + texto)
    if not ok:
        fallos += 1


def texto_ok(cap, x, y, fg, bg, negrita, texto):
    """Compara la zona con el texto dibujado con la fuente propia."""
    F = fuente()
    mal = 0
    for i, ch in enumerate(texto.encode('cp437')):
        for fila in range(16):
            bits = F[ch][fila] << 1
            if negrita:
                bits |= F[ch][fila]
            for col in range(8):
                # (la novena columna de la negrita cae en el carácter siguiente)
                enc = bits & (0x100 >> col)
                if negrita and col == 0 and i > 0:
                    enc = enc or (F[texto.encode('cp437')[i - 1]][fila] & 1)
                c = cap.px(x + i * 8 + col, y + fila)
                if not es(c, fg if enc else bg):
                    mal += 1
    return mal


def texto_en(cap, x, y, fg, bg, cod):
    """Como texto_ok, fino, parando en el primer punto distinto."""
    F = fuente()
    for fila in range(16):
        for i, ch in enumerate(cod):
            bits = F[ch][fila] << 1
            for col in range(8):
                if not es(cap.px(x + i * 8 + col, y + fila), fg if bits & (0x100 >> col) else bg):
                    return False
    return True


def busca(cap, x0, y0, x1, y1, fg, bg, texto):
    F = fuente()
    cod = texto.encode('cp437')
    # los puntos del texto (primero los de tinta: descartan antes)
    pts = []
    for fila in range(16):
        for i, ch in enumerate(cod):
            bits = F[ch][fila] << 1
            for col in range(8):
                pts.append((i * 8 + col, fila, bool(bits & (0x100 >> col))))
    pts.sort(key=lambda p: not p[2])
    ancho, alto = 8 * len(cod), 16
    clase = {}
    def c(x, y):
        k = (x, y)
        v = clase.get(k)
        if v is None:
            q = cap.px(x, y)
            v = 1 if es(q, fg) else (2 if es(q, bg) else 0)
            clase[k] = v
        return v
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if all(c(x + dx, y + dy) == (1 if f else 2) for dx, dy, f in pts):
                return (x, y)
    return None


def indice_ok(cap, x, y, fg, texto, girado=False):
    """El índice de una carta (juegos/solitario.asm, hacer_indices)."""
    F = fuente()
    marca = [[False] * 14 for _ in range(9)]
    x0 = 3 if len(texto) == 1 else 0
    for ch in texto.encode('cp437'):
        for fila in range(9):
            b = F[ch][3 + fila]
            b |= b >> 1
            for col in range(8):
                if b & (0x80 >> col) and x0 + col < 14:
                    marca[fila][x0 + col] = True
        x0 += 6
    if girado:
        marca = [f[::-1] for f in marca[::-1]]
    return sum(1 for f in range(9) for c in range(14)
               if not es(cap.px(x + c, y + f), fg if marca[f][c] else 9))


def main():
    cap = Captura(sys.argv[1])
    nombre = os.path.basename(sys.argv[1]).replace('.ppm', '')
    a = sys.argv[2:]
    i = 0
    while i < len(a):
        op = a[i]
        if op == 'tam':
            w, h = int(a[i + 1]), int(a[i + 2])
            res((cap.w, cap.h) == (w, h), '%s: pantalla de %dx%d%s' % (
                nombre, w, h, '' if (cap.w, cap.h) == (w, h) else ' (es de %dx%d)' % (cap.w, cap.h)))
            i += 3
        elif op == 'px':
            x, y, n = int(a[i + 1]), int(a[i + 2]), int(a[i + 3])
            c = cap.px(x, y)
            res(es(c, n), '%s: %s (%d,%d)%s' % (nombre, a[i + 4], x, y,
                '' if es(c, n) else ' es %s y no %s' % (c, PAL[n])))
            i += 5
        elif op == 'rgb':
            x, y = int(a[i + 1]), int(a[i + 2])
            quiere = tuple(bytes.fromhex(a[i + 3]))
            c = cap.px(x, y)
            bien = all(abs(p - q) <= MARGEN for p, q in zip(c, quiere))
            res(bien, '%s: %s (%d,%d)%s' % (nombre, a[i + 4], x, y,
                '' if bien else ' es %s y no %s' % (c, quiere)))
            i += 5
        elif op in ('ventana', 'marco'):
            x, y, w, h = (int(v) for v in a[i + 1:i + 5])
            act = a[i + 5] == 'activa'
            p = [
                (es(cap.px(x, y + h // 2), MARRON), 'marco izquierdo'),
                (es(cap.px(x + w - 1, y + h // 2), MARRON), 'marco derecho'),
                (es(cap.px(x + w // 2, y + h - 1), MARRON), 'marco inferior'),
                (es(cap.px(x + BORDE + 3, y + BORDE + 3), TERRACOTA if act else ARENA), 'barra de título'),
                (es(cap.px(x + w - 8, y + BORDE + TIT_H), MARRON), 'línea bajo el título'),
                (es(cap.px(x + BORDE + 13, y + BORDE + TIT_H // 2), OCRE if act else 7), 'rombo'),
            ]
            if op == 'ventana':
                p.append((es(cap.px(x + w - 8, y + BORDE + 3), TERRACOTA if act else ARENA),
                          'barra de título (derecha)'))
                p.append((es(cap.px(x + w - 8, y + h - 8), CREMA), 'interior'))
            mal = [t for ok, t in p if not ok]
            res(not mal, '%s: ventana %s en (%d,%d) de %dx%d%s' % (
                nombre, 'activa' if act else 'no activa', x, y, w, h,
                '' if not mal else ' - falla: ' + ', '.join(mal)))
            i += 6
        elif op == 'titulo':
            x, y = int(a[i + 1]), int(a[i + 2])
            act = a[i + 3] == 'activa'
            t = a[i + 4]
            mal = texto_ok(cap, x + BORDE + 26, y + BORDE + (TIT_H - 16) // 2,
                           CREMA if act else MARRON, TERRACOTA if act else ARENA, act, t)
            res(mal == 0, '%s: título «%s»%s%s' % (nombre, t, ' en negrita' if act else '',
                                                   '' if mal == 0 else ' (%d puntos distintos)' % mal))
            i += 5
        elif op == 'texto':
            x, y, fg, bg = (int(v) for v in a[i + 1:i + 5])
            t = a[i + 6]
            mal = texto_ok(cap, x, y, fg, bg, a[i + 5] == 'negrita', t)
            res(mal == 0, '%s: texto «%s»%s' % (nombre, t, '' if mal == 0 else ' (%d puntos distintos)' % mal))
            i += 7
        elif op in ('busca', 'nobusca'):
            x0, y0, x1, y1, fg, bg = (int(v) for v in a[i + 1:i + 7])
            t = a[i + 7]
            donde = busca(cap, x0, y0, x1, y1, fg, bg, t)
            if op == 'busca':
                res(donde is not None, '%s: «%s» está%s' % (nombre, t, ' (en %d,%d)' % donde if donde else
                                                           ' (no, entre %d,%d y %d,%d)' % (x0, y0, x1, y1)))
            else:
                res(donde is None, '%s: «%s» no está%s' % (nombre, t, '' if donde is None else ' (sí: en %d,%d)' % donde))
            i += 8
        elif op in ('indice', 'indice_rev'):
            x, y, fg = (int(v) for v in a[i + 1:i + 4])
            t = a[i + 4]
            mal = indice_ok(cap, x, y, fg, t, op == 'indice_rev')
            res(mal == 0, '%s: índice «%s»%s en (%d,%d)%s' % (nombre, t, ' (girado)' if op == 'indice_rev' else '', x, y,
                                                          '' if mal == 0 else ' (%d puntos distintos)' % mal))
            i += 5
        elif op == 'cursor':
            x, y = int(a[i + 1]), int(a[i + 2])
            ok = (es(cap.px(x, y), TINTA) and es(cap.px(x + 1, y + 3), OCRE) and
                  es(cap.px(x + 4, y + 8), OCRE) and es(cap.px(x, y + 10), TINTA))
            res(ok, '%s: cursor del ratón en (%d,%d)' % (nombre, x, y))
            i += 3
        elif op == 'igual':
            x, y, w, h = (int(v) for v in a[i + 1:i + 5])
            otra = Captura(a[i + 5])
            x2, y2, maxdif = int(a[i + 6]), int(a[i + 7]), int(a[i + 8])
            dif = sum(1 for yy in range(h) for xx in range(w)
                      if cap.px(x + xx, y + yy) != otra.px(x2 + xx, y2 + yy))
            res(dif <= maxdif, '%s: zona (%d,%d) %dx%d igual que en %s (%d,%d): %d puntos distintos' % (
                nombre, x, y, w, h, os.path.basename(a[i + 5]).replace('.ppm', ''), x2, y2, dif))
            i += 9
        elif op == 'cambia':
            x, y, w, h = (int(v) for v in a[i + 1:i + 5])
            otra = Captura(a[i + 5])
            mindif = int(a[i + 6])
            dif = sum(1 for yy in range(h) for xx in range(w)
                      if cap.px(x + xx, y + yy) != otra.px(x + xx, y + yy))
            res(dif >= mindif, '%s: zona (%d,%d) %dx%d distinta de la de %s: %d puntos' % (
                nombre, x, y, w, h, os.path.basename(a[i + 5]).replace('.ppm', ''), dif))
            i += 7
        else:
            raise SystemExit('comprobación desconocida: ' + op)
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
