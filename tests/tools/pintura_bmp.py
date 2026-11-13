#!/usr/bin/env python3
"""Imágenes BMP para la prueba de Pintura (tests/run_pintura_tests.sh).

uso: pintura_bmp.py crea CARPETA
         escribe las imágenes de prueba: T1.BMP (1 bit), T4.BMP (4 bits),
         T8.BMP (8 bits), T24.BMP (24 bits), TD24.BMP (24 bits con la primera
         fila arriba), GRANDE.BMP (600x400, no cabe en la ventana), RLE.BMP
         (comprimida: no se admite), ENORME.BMP (2000x10: demasiado grande) y
         ROTO.BMP (no es una imagen)
     pintura_bmp.py color ARCHIVO X Y
         escribe RRGGBB: el color con el que Pintura enseña ese punto de la
         imagen de prueba (el más parecido de la paleta de Hispa Ventanas)
     pintura_bmp.py mira ARCHIVO COMPROBACION...
         tam ANCHO ALTO          tamaño; además mira siempre que sea un BMP
                                 de 8 bits sin comprimir con la paleta de
                                 Hispa Ventanas y el tamaño de archivo justo
         px X Y N [nombre]       el punto es del color N (0-15) de la paleta
         rgb X Y RRGGBB [nombre] el punto es de ese color
         igual OTRO.BMP          cada punto es el color de OTRO.BMP pasado a
                                 la paleta de Hispa Ventanas
Escribe una línea «  OK   ...» o «  FAIL ...» por comprobación.
BMP fixtures and checks for the Pintura test.
"""
import os, re, struct, sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')


def paleta16():
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
    assert len(cols) == 16
    return cols


def paleta():
    """Los 256 colores de Hispa Ventanas (ventanas/video.inc, vid_paleta)."""
    p = list(paleta16())
    for i in range(216):
        p.append((i // 36 * 51, i % 36 // 6 * 51, i % 6 * 51))
    for i in range(24):
        p.append((i * 11,) * 3)
    return p


PAL = paleta()


def cercano(rgb):
    """El color de la paleta al que Pintura pasa un color (rgb_a_ind)."""
    r, g, b = rgb
    for i in range(16):
        if PAL[i] == rgb:
            return i
    if r == g == b:
        i = min((r + 5) // 11, 23)
        k = (r + 25) // 51
        if abs(k * 51 - r) < abs(i * 11 - r):
            return 16 + 43 * k
        return 232 + i
    return 16 + 36 * ((r + 25) // 51) + 6 * ((g + 25) // 51) + (b + 25) // 51


def escribe(ruta, ancho, alto, bits, pal, punto, arriba=False, compresion=0):
    """punto(x, y) -> índice (o (r, g, b) con 24 bits); y=0 es la fila de arriba."""
    fila = (ancho * bits + 31) // 32 * 4
    datos = bytearray()
    for k in range(alto):
        y = k if arriba else alto - 1 - k
        f = bytearray(fila)
        for x in range(ancho):
            v = punto(x, y)
            if bits == 24:
                f[x * 3:x * 3 + 3] = bytes((v[2], v[1], v[0]))
            elif bits == 8:
                f[x] = v
            elif bits == 4:
                f[x // 2] |= v << (0 if x & 1 else 4)
            else:
                f[x // 8] |= v << (7 - x % 8)
        datos += f
    tabla = b''.join(bytes((b, g, r, 0)) for r, g, b in pal)
    donde = 14 + 40 + len(tabla)
    cab = struct.pack('<2sIHHI', b'BM', donde + len(datos), 0, 0, donde)
    cab += struct.pack('<IiiHHIIiiII', 40, ancho, -alto if arriba else alto, 1, bits, compresion,
                       len(datos), 2835, 2835, len(pal), 0)
    with open(ruta, 'wb') as f:
        f.write(cab + tabla + datos)


# ---- las imágenes de prueba: qué color tiene cada punto ----
P16 = paleta16()
# 8 bits: una paleta rara de 256; estos son los que se usan (bandas de 8)
RAROS = [P16[4], P16[12], (255, 0, 0), (121, 121, 121), (250, 10, 10), (100, 100, 100),
         (255, 255, 255), (0, 0, 0), (0, 128, 255), (51, 102, 153)]


def pal8():
    p = [((i * 7) % 256, (255 - i) % 256, (i * 3 + 40) % 256) for i in range(256)]
    for i, c in enumerate(RAROS):
        p[200 + i] = c
    p[210] = P16[11]
    return p


def punto(nombre, x, y):
    """-> (r, g, b) del punto en la imagen de prueba."""
    if nombre == 'T1':                  # damero de 8 en añil y ocre
        return (P16[12], P16[10])[(x // 8 + y // 8) % 2]
    if nombre == 'T4':                  # bandas de 4 con los 16 colores; arriba, al revés
        return P16[(15 - x // 4 % 16) if y < 6 else x // 4 % 16]
    if nombre == 'T8':                  # bandas de 8; las 5 filas de arriba, verde oliva
        return P16[11] if y < 5 else RAROS[x // 8 % 10]
    if nombre == 'T24':                 # lo mismo, y abajo un degradado
        if y >= 40:
            return (x * 4, 255 - x * 4, y * 5)
        return P16[11] if y < 5 else RAROS[x // 8 % 10]
    if nombre == 'TD24':                # arriba terracota, abajo turquesa
        return P16[4] if y < 10 else P16[14]
    if nombre == 'GRANDE':              # cuadros de 50 con los 16 colores
        return P16[(x // 50 + y // 50 * 3) % 16]
    raise SystemExit('imagen desconocida: ' + nombre)


def crea(d):
    escribe(os.path.join(d, 'T1.BMP'), 70, 40, 1, [P16[12], P16[10]],
            lambda x, y: (x // 8 + y // 8) % 2)
    escribe(os.path.join(d, 'T4.BMP'), 66, 30, 4, P16,
            lambda x, y: (15 - x // 4 % 16) if y < 6 else x // 4 % 16)
    p8 = pal8()
    escribe(os.path.join(d, 'T8.BMP'), 83, 37, 8, p8,
            lambda x, y: 210 if y < 5 else 200 + x // 8 % 10)
    escribe(os.path.join(d, 'T24.BMP'), 61, 48, 24, [], lambda x, y: punto('T24', x, y))
    escribe(os.path.join(d, 'TD24.BMP'), 32, 20, 24, [], lambda x, y: punto('TD24', x, y), arriba=True)
    escribe(os.path.join(d, 'GRANDE.BMP'), 600, 400, 8, PAL,
            lambda x, y: (x // 50 + y // 50 * 3) % 16)
    escribe(os.path.join(d, 'RLE.BMP'), 16, 16, 8, PAL, lambda x, y: 0, compresion=1)
    escribe(os.path.join(d, 'ENORME.BMP'), 2000, 10, 1, [P16[0], P16[9]], lambda x, y: 0)
    with open(os.path.join(d, 'ROTO.BMP'), 'wb') as f:
        f.write(b'Esto no es una imagen.\r\n' * 20)


class Bmp:
    def __init__(self, ruta):
        d = open(ruta, 'rb').read()
        self.d = d
        self.firma, self.tam, _, _, self.donde = struct.unpack_from('<2sIHHI', d, 0)
        (self.cab, self.an, self.al, self.planos, self.bits, self.comp, self.timg, _, _,
         self.ncol, _) = struct.unpack_from('<IiiHHIIiiII', d, 14)
        self.arriba = self.al < 0
        self.al = abs(self.al)
        n = self.ncol or (1 << self.bits if self.bits <= 8 else 0)
        self.pal = [(d[14 + self.cab + i * 4 + 2], d[14 + self.cab + i * 4 + 1], d[14 + self.cab + i * 4])
                    for i in range(n)]
        self.fila = (self.an * self.bits + 31) // 32 * 4

    def px(self, x, y):
        k = y if self.arriba else self.al - 1 - y
        o = self.donde + k * self.fila
        if self.bits == 24:
            return (self.d[o + x * 3 + 2], self.d[o + x * 3 + 1], self.d[o + x * 3])
        if self.bits == 8:
            v = self.d[o + x]
        elif self.bits == 4:
            v = self.d[o + x // 2] >> (0 if x & 1 else 4) & 15
        else:
            v = self.d[o + x // 8] >> (7 - x % 8) & 1
        return self.pal[v]


fallos = 0


def res(ok, texto):
    global fallos
    print(('  OK   ' if ok else '  FAIL ') + texto)
    if not ok:
        fallos += 1


def mira(a):
    ruta = a[0]
    nombre = os.path.basename(ruta)
    try:
        b = Bmp(ruta)
    except Exception as e:
        res(False, '%s: no se puede leer (%s)' % (nombre, e))
        return
    fila = (b.an + 3) & ~3
    pegas = []
    if b.firma != b'BM':
        pegas.append('firma')
    if (b.cab, b.planos, b.bits, b.comp) != (40, 1, 8, 0):
        pegas.append('cabecera %r' % ((b.cab, b.planos, b.bits, b.comp),))
    if b.donde != 1078 or b.tam != 1078 + fila * b.al or len(b.d) != b.tam:
        pegas.append('tamaños (imagen en %d, dice %d y ocupa %d)' % (b.donde, b.tam, len(b.d)))
    if b.pal != PAL:
        pegas.append('la paleta no es la de Hispa Ventanas')
    res(not pegas, '%s: BMP de 8 bits sin comprimir, con la paleta de Hispa Ventanas y %d octetos%s' % (
        nombre, len(b.d), '' if not pegas else ' - falla: ' + ', '.join(pegas)))
    i = 1
    while i < len(a):
        op = a[i]
        if op == 'tam':
            w, h = int(a[i + 1]), int(a[i + 2])
            res((b.an, b.al) == (w, h), '%s: de %dx%d%s' % (
                nombre, w, h, '' if (b.an, b.al) == (w, h) else ' (es de %dx%d)' % (b.an, b.al)))
            i += 3
        elif op in ('px', 'rgb'):
            x, y = int(a[i + 1]), int(a[i + 2])
            if op == 'px':
                quiere = PAL[int(a[i + 3])]
            else:
                quiere = tuple(bytes.fromhex(a[i + 3]))
            c = b.px(x, y) if 0 <= x < b.an and 0 <= y < b.al else None
            res(c == quiere, '%s: %s (%d,%d)%s' % (nombre, a[i + 4], x, y,
                '' if c == quiere else ' es %s y no %s' % (c, quiere)))
            i += 5
        elif op == 'igual':
            o = Bmp(a[i + 1])
            mal = -1
            if (o.an, o.al) == (b.an, b.al):
                mal = sum(1 for y in range(b.al) for x in range(b.an)
                          if b.px(x, y) != PAL[cercano(o.px(x, y))])
            res(mal == 0, '%s: los %d puntos son los de %s pasados a la paleta%s' % (
                nombre, b.an * b.al, os.path.basename(a[i + 1]),
                '' if mal == 0 else ' (%s)' % ('otro tamaño' if mal < 0 else '%d distintos' % mal)))
            i += 2
        else:
            raise SystemExit('comprobación desconocida: ' + op)


def main():
    if sys.argv[1] == 'crea':
        crea(sys.argv[2])
    elif sys.argv[1] == 'color':
        print('%02X%02X%02X' % PAL[cercano(punto(os.path.basename(sys.argv[2]).split('.')[0],
                                                  int(sys.argv[3]), int(sys.argv[4])))])
    else:
        mira(sys.argv[2:])
        sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
