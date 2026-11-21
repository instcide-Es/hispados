#!/usr/bin/env python3
# tests/gl/compara.py - compara las imágenes de HispaGL con las de referencia
# (las de Mesa, o las guardadas en tests/gl/ref/) y hace una hoja con las dos
# y la diferencia. Sin bibliotecas de fuera (PPM y PNG a mano).
#   compara.py hispagl/ referencia/ [--hoja hoja.png] [--umbral N] [--max-malos P]
#   compara.py --png entrada.ppm salida.png      (convertir)
#   compara.py --ppm entrada.png salida.ppm
# Un punto es «malo» si alguno de sus colores se aparta más de N (24) de la
# referencia; la imagen pasa si los malos no llegan al P % (3) y la media de
# la diferencia no pasa de 4 (en 0..255). Las diferencias de 5-6-5 (HispaGL)
# frente a 8-8-8 (Mesa) y de un punto en los bordes de los triángulos son
# normales; una textura, una luz o una mezcla mal hechas no pasan.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import sys, os, struct, zlib

def lee_ppm(ruta):
    d = open(ruta, 'rb').read()
    campos, i = [], 0
    while len(campos) < 4:
        while d[i:i + 1].isspace():
            i += 1
        if d[i:i + 1] == b'#':
            while d[i:i + 1] != b'\n':
                i += 1
            continue
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        campos.append(d[i:j])
        i = j
    i += 1
    w, h = int(campos[1]), int(campos[2])
    return w, h, d[i:i + w * h * 3]

def escribe_png(ruta, w, h, rgb):
    filas = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def trozo(t, c):
        return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += trozo(b'IDAT', zlib.compress(filas, 9)) + trozo(b'IEND', b'')
    open(ruta, 'wb').write(png)

def lee_png(ruta):
    d = open(ruta, 'rb').read()
    i, idat, w = 8, b'', 0
    while i < len(d):
        n, t = struct.unpack('>I4s', d[i:i + 8])
        c = d[i + 8:i + 8 + n]
        if t == b'IHDR':
            w, h, bits, tipo = struct.unpack('>IIBB', c[:10])
            assert bits == 8 and tipo == 2
        elif t == b'IDAT':
            idat += c
        i += 12 + n
    raw = zlib.decompress(idat)
    out, prev, p = bytearray(), bytearray(w * 3), 0
    for y in range(h):
        f = raw[p]; p += 1
        fila = bytearray(raw[p:p + w * 3]); p += w * 3
        for x in range(w * 3):
            a = fila[x - 3] if x >= 3 else 0
            b = prev[x]
            c = prev[x - 3] if x >= 3 else 0
            if f == 1: fila[x] = (fila[x] + a) & 255
            elif f == 2: fila[x] = (fila[x] + b) & 255
            elif f == 3: fila[x] = (fila[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                fila[x] = (fila[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += fila
        prev = fila
    return w, h, bytes(out)

def lee(ruta):
    return lee_png(ruta) if ruta.endswith('.png') else lee_ppm(ruta)

def compara(a, b, umbral):
    malos, suma, n = 0, 0, len(a) // 3
    dif = bytearray(len(a))
    for i in range(n):
        d0, d1, d2 = abs(a[i * 3] - b[i * 3]), abs(a[i * 3 + 1] - b[i * 3 + 1]), abs(a[i * 3 + 2] - b[i * 3 + 2])
        m = max(d0, d1, d2)
        suma += d0 + d1 + d2
        if m > umbral:
            malos += 1
            dif[i * 3] = 255
        else:
            dif[i * 3] = dif[i * 3 + 1] = dif[i * 3 + 2] = min(255, m * 8)
    return malos * 100.0 / n, suma / (3.0 * n), bytes(dif)

# clases de color para las capturas de QEMU (la pantalla de Hispa Ventanas
# puede ser de 8 bits: los colores se acercan a su paleta)
CLASES = {
    'rojo': lambda r, g, b: r > 130 and g < 100 and b < 100 and r > g * 2,
    'amarillo': lambda r, g, b: r > 170 and g > 140 and b < 120,
    'verde': lambda r, g, b: g > 110 and r < 120 and b < 130 and g > r + 40,
    'azul': lambda r, g, b: b > 140 and r < 130 and g < 140 and b > r + 50,
    'baldosa': lambda r, g, b: r > 90 and g > 60 and b > 40 and r >= b + 15 and r < 250,
    'ladrillo': lambda r, g, b: r > 60 and r >= g + 25 and r >= b + 25,
    'negro': lambda r, g, b: r < 20 and g < 20 and b < 20,
    'claro': lambda r, g, b: r > 200 and g > 200 and b > 200,
}

def cuenta(args):
    # --cuenta CAPTURA.ppm X Y ANCHO ALTO CLASE MIN MAX NOMBRE
    w, h, d = lee(args[0])
    x0, y0, an, al = (int(v) for v in args[1:5])
    clase, mn, mx, nom = args[5], int(args[6]), int(args[7]), ' '.join(args[8:])
    f = CLASES[clase]
    n = 0
    for y in range(max(0, y0), min(h, y0 + al)):
        for x in range(max(0, x0), min(w, x0 + an)):
            i = (y * w + x) * 3
            if f(d[i], d[i + 1], d[i + 2]):
                n += 1
    ok = mn <= n <= mx
    print('  %s %s: %d puntos %s en (%d,%d) %dx%d (entre %d y %d)' % ('OK  ' if ok else 'FAIL', nom, n, clase, x0, y0, an, al, mn, mx))
    return 0 if ok else 1

def main():
    args = sys.argv[1:]
    if args and args[0] == '--cuenta':
        return cuenta(args[1:])
    if args and args[0] == '--png':
        w, h, d = lee_ppm(args[1]); escribe_png(args[2], w, h, d); return 0
    if args and args[0] == '--ppm':
        w, h, d = lee_png(args[1])
        open(args[2], 'wb').write(b'P6\n%d %d\n255\n' % (w, h) + d); return 0
    hoja, umbral, max_malos, max_media = None, 24, 3.0, 4.0
    pos = []
    k = 0
    while k < len(args):
        if args[k] == '--hoja': hoja = args[k + 1]; k += 2
        elif args[k] == '--umbral': umbral = int(args[k + 1]); k += 2
        elif args[k] == '--max-malos': max_malos = float(args[k + 1]); k += 2
        else: pos.append(args[k]); k += 1
    dh, dr = pos
    nombres = sorted(f[:-4] for f in os.listdir(dh) if f.endswith('.ppm'))
    filas, fallos = [], 0
    for nom in nombres:
        ref = os.path.join(dr, nom + '.png')
        if not os.path.exists(ref):
            ref = os.path.join(dr, nom + '.ppm')
        if not os.path.exists(ref):
            print('  --   %-14s sin referencia' % nom)
            continue
        w, h, a = lee_ppm(os.path.join(dh, nom + '.ppm'))
        w2, h2, b = lee(ref)
        if (w, h) != (w2, h2):
            print('  FAIL %-14s tamaño distinto' % nom); fallos += 1; continue
        malos, media, dif = compara(a, b, umbral)
        ok = malos <= max_malos and media <= max_media
        print('  %s %-14s %5.2f %% de puntos distintos, diferencia media %.2f' % ('OK  ' if ok else 'FAIL', nom, malos, media))
        fallos += not ok
        filas.append((w, h, a, b, dif))
    if hoja and filas:
        w, h = filas[0][0], filas[0][1]
        W, H = w * 3 + 8, (h + 4) * len(filas)
        img = bytearray(b'\x40' * (W * H * 3))
        for j, (w_, h_, a, b, dif) in enumerate(filas):
            for col, src in enumerate((a, b, dif)):
                for y in range(h):
                    o = ((j * (h + 4) + y) * W + col * (w + 4)) * 3
                    img[o:o + w * 3] = src[y * w * 3:(y + 1) * w * 3]
        escribe_png(hoja, W, H, bytes(img))
    return 1 if fallos else 0

sys.exit(main())
