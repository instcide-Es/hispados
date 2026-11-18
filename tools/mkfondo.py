#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Prepara una imagen de fondo de escritorio para Hispa Ventanas.

Hispa Ventanas (ventanas/fondo.inc, docs/VENTANAS.md, «La imagen de fondo»)
pone de fondo un BMP sin comprimir. Si sus colores son ya los fijos de la
paleta de Hispa Ventanas (el cubo de 6x6x6 y los 24 grises: los números 16 a
255), lo lee y lo pinta tal cual, sin convertir nada al arrancar. Esto hace
ese BMP a partir de una foto: la reduce al tamaño de la pantalla y la pasa a
esos 240 colores con difuminado (Floyd-Steinberg, en zigzag).

  python3 tools/mkfondo.py ORIGEN SALIDA.BMP [--tam 800x600]
                           [--modo recortar|ajustar|estirar] [--sin-difuminar]

  --tam    el tamaño de la pantalla (800x600 si no se dice)
  --modo   recortar (lo normal): llena la pantalla entera y lo que sobra por
           los lados (o por arriba y abajo) se queda fuera; ajustar: la
           imagen entera, con su proporción (el BMP sale más pequeño que la
           pantalla por un lado: Hispa Ventanas rellena lo que falta con el
           color del escritorio); estirar: tal cual, deformada

ORIGEN: un PPM (P6) o un BMP de 24 bits se leen aquí mismo; lo demás (JPEG,
PNG...) con Pillow si está o, si no, con ImageMagick (convert), ffmpeg o
djpeg, lo que haya. Todo lo demás (reducir, difuminar, escribir el BMP) es
de aquí, así que el resultado no depende de con qué se haya leído.

El fondo de la imagen de Hispa-486 (sistema/486/FONDO.BMP) sale de
sistema/486/HISPADOS.JPG (la imagen es de Carlos Sanz): «make fondo486».
Prepares a desktop wallpaper BMP (Hispa Ventanas palette, dithered).
"""
import os, re, struct, subprocess, sys, tempfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def paleta():
    """Los 256 colores de Hispa Ventanas (ventanas/datos.inc y video.inc)."""
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
    if len(cols) != 16:
        sys.exit('mkfondo: no se ha podido leer la paleta de ventanas/datos.inc')
    cols += [(r * 51, g * 51, b * 51) for r in range(6) for g in range(6) for b in range(6)]
    cols += [(i * 11,) * 3 for i in range(24)]
    return cols


def lee_ppm(d):
    m = re.match(rb'P6\s+(?:#[^\n]*\n\s*)*(\d+)\s+(\d+)\s+(\d+)\s', d)
    if not m or int(m.group(3)) != 255:
        sys.exit('mkfondo: el PPM no es P6 de 8 bits')
    w, h = int(m.group(1)), int(m.group(2))
    return w, h, d[m.end():m.end() + w * h * 3]


def lee_bmp(d):
    off, tam, w, h, _, bpp, comp = struct.unpack_from('<IIiiHHI', d, 10)
    if bpp != 24 or comp != 0:
        return None
    fila = (w * 3 + 3) & ~3
    filas = []
    for y in range(abs(h)):
        f = d[off + y * fila:off + y * fila + w * 3]
        filas.append(bytes(c for i in range(0, w * 3, 3) for c in (f[i + 2], f[i + 1], f[i])))
    if h > 0:
        filas.reverse()
    return w, abs(h), b''.join(filas)


def lee(ruta):
    """-> ancho, alto, puntos (rojo, verde, azul seguidos)"""
    d = open(ruta, 'rb').read()
    if d[:2] == b'P6':
        return lee_ppm(d)
    if d[:2] == b'BM':
        r = lee_bmp(d)
        if r:
            return r
    try:
        from PIL import Image
        im = Image.open(ruta).convert('RGB')
        return im.size[0], im.size[1], im.tobytes()
    except ImportError:
        pass
    with tempfile.TemporaryDirectory() as t:
        ppm = os.path.join(t, 'a.ppm')
        for orden in (['convert', ruta, '-depth', '8', ppm], ['magick', ruta, '-depth', '8', ppm],
                      ['ffmpeg', '-v', 'error', '-y', '-i', ruta, '-frames:v', '1', '-pix_fmt', 'rgb24', ppm],
                      ['djpeg', '-pnm', '-outfile', ppm, ruta]):
            try:
                if subprocess.run(orden, capture_output=True).returncode == 0 and os.path.exists(ppm):
                    return lee_ppm(open(ppm, 'rb').read())
            except FileNotFoundError:
                pass
    sys.exit('mkfondo: no sé leer %s (hace falta Pillow, ImageMagick, ffmpeg o djpeg; '
             'o darlo ya en PPM o en BMP de 24 bits)' % ruta)


def pesos(n_ent, n_sal):
    """Para cada punto de salida, los de entrada que le tocan y cuánto: al
    reducir, la media de los que tapa (por superficie); al ampliar, entre los
    dos vecinos."""
    r = []
    if n_sal >= n_ent:
        for i in range(n_sal):
            c = (i + 0.5) * n_ent / n_sal - 0.5
            a = int(c // 1)
            f = c - a
            r.append([(min(max(a, 0), n_ent - 1), 1 - f), (min(max(a + 1, 0), n_ent - 1), f)])
        return r
    for i in range(n_sal):
        a, b = i * n_ent / n_sal, (i + 1) * n_ent / n_sal
        l, j = [], int(a)
        while j < b and j < n_ent:
            p = min(b, j + 1) - max(a, j)
            if p > 1e-9:
                l.append((j, p / (b - a)))
            j += 1
        r.append(l)
    return r


def escala(w, h, d, x0, y0, rw, rh, nw, nh):
    """El trozo (x0, y0, rw x rh) de la imagen, a nw x nh -> filas de reales."""
    px = pesos(rw, nw)
    py = pesos(rh, nh)
    medio = []                              # primero a lo ancho
    for y in range(rh):
        o = ((y0 + y) * w + x0) * 3
        f = d[o:o + rw * 3]
        fila = [0.0] * (nw * 3)
        for i, l in enumerate(px):
            r = g = b = 0.0
            for j, p in l:
                r += f[j * 3] * p
                g += f[j * 3 + 1] * p
                b += f[j * 3 + 2] * p
            fila[i * 3:i * 3 + 3] = (r, g, b)
        medio.append(fila)
    sal = []
    for l in py:                            # y luego a lo alto
        fila = [0.0] * (nw * 3)
        for j, p in l:
            m = medio[j]
            for k in range(nw * 3):
                fila[k] += m[k] * p
        sal.append(fila)
    return sal


def cercano(r, g, b):
    """El color fijo de la paleta (16-255) más parecido -> número, (r, g, b)"""
    cr, cg, cb = (min(5, max(0, int(r / 51 + 0.5))), min(5, max(0, int(g / 51 + 0.5))),
                  min(5, max(0, int(b / 51 + 0.5))))
    dc = (r - cr * 51) ** 2 + (g - cg * 51) ** 2 + (b - cb * 51) ** 2
    k = min(23, max(0, int((r + g + b) / 33 + 0.5)))
    dg = (r - k * 11) ** 2 + (g - k * 11) ** 2 + (b - k * 11) ** 2
    if dg < dc:
        return 232 + k, (k * 11,) * 3
    return 16 + 36 * cr + 6 * cg + cb, (cr * 51, cg * 51, cb * 51)


def difumina(filas, nw, nh, difuminar=True):
    """Filas de reales -> filas de números de la paleta (Floyd-Steinberg en
    zigzag: el error de cada punto se reparte entre los que faltan)."""
    sal = []
    sig = [0.0] * ((nw + 2) * 3)
    for y in range(nh):
        act, sig = sig, [0.0] * ((nw + 2) * 3)
        f = filas[y]
        out = bytearray(nw)
        orden = range(nw) if y % 2 == 0 else range(nw - 1, -1, -1)
        s = 1 if y % 2 == 0 else -1
        for x in orden:
            o = (x + 1) * 3
            r = min(255.0, max(0.0, f[x * 3] + act[o]))
            g = min(255.0, max(0.0, f[x * 3 + 1] + act[o + 1]))
            b = min(255.0, max(0.0, f[x * 3 + 2] + act[o + 2]))
            n, c = cercano(r, g, b)
            out[x] = n
            if not difuminar:
                continue
            for k, e in enumerate((r - c[0], g - c[1], b - c[2])):
                act[o + s * 3 + k] += e * 7 / 16
                sig[o - s * 3 + k] += e * 3 / 16
                sig[o + k] += e * 5 / 16
                sig[o + s * 3 + k] += e * 1 / 16
        sal.append(bytes(out))
    return sal


def escribe_bmp(ruta, nw, nh, filas):
    pal = paleta()
    fila = (nw + 3) & ~3
    datos = b''.join(f + bytes(fila - nw) for f in reversed(filas))
    cab = struct.pack('<2sIHHI', b'BM', 54 + 1024 + len(datos), 0, 0, 54 + 1024)
    cab += struct.pack('<IiiHHIIiiII', 40, nw, nh, 1, 8, 0, len(datos), 2835, 2835, 256, 0)
    with open(ruta, 'wb') as o:
        o.write(cab + b''.join(bytes((b, g, r, 0)) for r, g, b in pal) + datos)
    return 54 + 1024 + len(datos)


def main():
    a = sys.argv[1:]
    tam, modo, dif, resto = (800, 600), 'recortar', True, []
    while a:
        x = a.pop(0)
        if x == '--tam' and a:
            m = re.match(r'(\d+)x(\d+)$', a.pop(0))
            if not m:
                sys.exit('mkfondo: --tam ANCHOxALTO')
            tam = (int(m.group(1)), int(m.group(2)))
        elif x == '--modo' and a:
            modo = a.pop(0)
        elif x == '--sin-difuminar':
            dif = False
        else:
            resto.append(x)
    if len(resto) != 2 or modo not in ('recortar', 'ajustar', 'estirar'):
        sys.exit(__doc__)
    w, h, d = lee(resto[0])
    W, H = tam
    x0, y0, rw, rh, nw, nh = 0, 0, w, h, W, H
    if modo == 'recortar':
        if w * H > h * W:                   # más ancha: sobran los lados
            rw = max(1, round(h * W / H))
            x0 = (w - rw) // 2
        else:
            rh = max(1, round(w * H / W))
            y0 = (h - rh) // 2
    elif modo == 'ajustar':
        if w * H > h * W:
            nh = max(1, round(h * W / w))
        else:
            nw = max(1, round(w * H / h))
    filas = difumina(escala(w, h, d, x0, y0, rw, rh, nw, nh), nw, nh, dif)
    n = escribe_bmp(resto[1], nw, nh, filas)
    print('%s: %d x %d, 8 bits, %d octetos (%d KB)' % (resto[1], nw, nh, n, (n + 1023) // 1024))


if __name__ == '__main__':
    main()
