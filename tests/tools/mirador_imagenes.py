#!/usr/bin/env python3
"""Las imágenes de la prueba de Mirador en QEMU, y la comparación de lo que se ve.

uso: mirador_imagenes.py haz DIR
        hace en DIR las imágenes de la prueba (pequeñas: van en un disquete) y
        sus referencias en PNG (lo que tiene que verse de cada una)
     mirador_imagenes.py ve CAPTURA.ppm X Y ANCHO ALTO REFERENCIA [giro] [margen] [nombre]
        ¿se ve en ese rectángulo de la captura la imagen de REFERENCIA (a ese
        tamaño)? Se comparan las medias de cuadros de 16x16 puntos: la
        pantalla tiene 256 colores y la imagen va tramada, pero de lejos tiene
        que ser la misma. «giro»: nada, izq, der, h, v o media (cómo está
        girada o volteada en la pantalla), o «X Y»: se ve, punto por
        punto, el trozo de la referencia que empieza en esa esquina
     mirador_imagenes.py bmp ARCHIVO.BMP REFERENCIA BITS [giro] [margen] [nombre]
        ¿es el BMP guardado la imagen de REFERENCIA, con esos bits por punto?
     mirador_imagenes.py impreso SALIDA.prn REFERENCIA [apaisada] [nombre]
        ¿son los gráficos ESC/P mandados a la impresora la imagen de REFERENCIA?
Necesita Pillow. Escribe «  OK   ...» o «  FAIL ...».
"""
import os, re, struct, sys, zlib

from PIL import Image, ImageDraw


def paisaje(an, al):
    """un dibujo que se reconoce: cielo, sol, montañas, un lago y una casa"""
    im = Image.new('RGB', (an, al))
    px = im.load()
    for y in range(al):
        for x in range(an):
            t = y / al
            px[x, y] = (int(90 + 130 * t), int(150 + 80 * t), int(235 - 40 * t))
    d = ImageDraw.Draw(im)
    s = an // 8
    d.ellipse((an - 2 * s, s // 3, an - s, s // 3 + s), fill=(255, 220, 60))
    d.polygon([(0, al * 0.75), (an * 0.25, al * 0.30), (an * 0.5, al * 0.75)], fill=(110, 100, 90))
    d.polygon([(an * 0.3, al * 0.75), (an * 0.62, al * 0.22), (an, al * 0.75)], fill=(80, 110, 70))
    d.polygon([(an * 0.55, al * 0.36), (an * 0.62, al * 0.22), (an * 0.70, al * 0.36)], fill=(245, 245, 250))
    d.rectangle((0, al * 0.75, an, al), fill=(40, 90, 150))
    for i in range(6):
        d.line((an * 0.1 * i, al * (0.8 + 0.03 * i), an * (0.3 + 0.1 * i), al * (0.8 + 0.03 * i)), fill=(120, 170, 220))
    d.rectangle((an * 0.08, al * 0.58, an * 0.22, al * 0.75), fill=(200, 60, 40))
    d.polygon([(an * 0.06, al * 0.58), (an * 0.15, al * 0.46), (an * 0.24, al * 0.58)], fill=(90, 40, 30))
    return im


def barras(an, al):
    """ocho barras de colores que están en la paleta de Hispa Ventanas (el cubo): tienen que salir exactas"""
    cols = [(255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0), (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0)]
    im = Image.new('RGB', (an, al))
    d = ImageDraw.Draw(im)
    for i, c in enumerate(cols):
        d.rectangle((i * an // 8, 0, (i + 1) * an // 8 - 1, al - 1), fill=c)
    d.rectangle((0, al - al // 4, an - 1, al - 1), fill=(102, 102, 102))
    d.rectangle((0, al - al // 4, an // 2 - 1, al - 1), fill=(204, 204, 204))
    return im


def flecha(lado):
    """una flecha hacia arriba a la derecha sobre transparente (para ver giros y la transparencia)"""
    im = Image.new('RGBA', (lado, lado), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = lado / 16
    d.polygon([(14 * u, 2 * u), (14 * u, 9 * u), (11.5 * u, 6.5 * u), (5 * u, 13 * u), (3 * u, 11 * u), (9.5 * u, 4.5 * u), (7 * u, 2 * u)],
              fill=(204, 0, 0, 255))
    d.rectangle((1 * u, 1 * u, 4 * u, 4 * u), fill=(0, 102, 204, 255))
    return im


def png_trozo(n, d):
    return struct.pack('>I', len(d)) + n + d + struct.pack('>I', zlib.crc32(n + d) & 0xFFFFFFFF)


def png_grande(an, al):
    """un PNG de paleta enorme que ocupa poco: cuatro cuadrantes de color y una diagonal"""
    pal = bytes((255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 0, 0, 0, 0))
    z = zlib.compressobj(9)
    out = b''
    for y in range(al):
        f = bytearray(an)
        for x in range(an):
            f[x] = (1 if x >= an // 2 else 0) + (2 if y >= al // 2 else 0)
        d = y * an // al
        f[max(0, d - 8):d + 8] = b'\4' * len(f[max(0, d - 8):d + 8])
        out += z.compress(b'\0' + bytes(f))
    out += z.flush()
    return (b'\x89PNG\r\n\x1a\n' + png_trozo(b'IHDR', struct.pack('>IIBBBBB', an, al, 8, 3, 0, 0, 0)) +
            png_trozo(b'PLTE', pal) + png_trozo(b'IDAT', out) + png_trozo(b'IEND', b''))


def haz(d):
    os.makedirs(d, exist_ok=True)
    g = lambda n: os.path.join(d, n)
    p = paisaje(320, 200)
    p.save(g('ref_paisaje.png'))
    p.save(g('PAISAJE.PNG'))
    p.save(g('PLAYA.JPG'), quality=88)
    Image.open(g('PLAYA.JPG')).convert('RGB').save(g('ref_playa.png'))
    p.quantize(256, dither=Image.Dither.NONE).save(g('LAGO.GIF'), interlace=True)
    Image.open(g('LAGO.GIF')).convert('RGB').save(g('ref_lago.png'))
    p.save(g('MONTE.TGA'), compression='tga_rle')
    p.resize((160, 100)).save(g('CASA.PPM'))
    p.resize((160, 100)).save(g('ref_casa.png'))
    b = barras(320, 160)
    b.save(g('ref_barras.png'))
    b.quantize(16, dither=Image.Dither.NONE).save(g('BARRAS.PCX'))
    b.convert('P', palette=Image.Palette.ADAPTIVE, colors=16).save(g('BARRAS.BMP'))
    f = flecha(64)
    f.save(g('FLECHA.PNG'))
    f.save(g('FLECHA.ICO'), sizes=[(16, 16), (32, 32), (48, 48)])
    # la flecha, como se tiene que ver: sobre el ajedrezado y sobre blanco
    for nombre, lado in (('ref_flecha', 64), ('ref_flecha48', 48)):
        ff = f if lado == 64 else Image.open(g('FLECHA.ICO'))
        if lado != 64:
            ff.size = (48, 48)
            ff = ff.convert('RGBA')
        fondo = Image.new('RGB', (lado, lado))
        fp = fondo.load()
        for y in range(lado):
            for x in range(lado):
                v = 0xCC if ((x >> 3) ^ (y >> 3)) & 1 else 0xFF
                fp[x, y] = (v, v, v)
        fondo.paste(ff, (0, 0), ff)
        fondo.save(g(nombre + '.png'))
        blanco = Image.new('RGB', (lado, lado), (255, 255, 255))
        blanco.paste(ff, (0, 0), ff)
        blanco.save(g(nombre + '_b.png'))
    # una alta y estrecha, y una ancha (para ajustar, desplazar y la impresión apaisada)
    paisaje(1280, 800).save(g('ANCHA.JPG'), quality=60)
    Image.open(g('ANCHA.JPG')).convert('RGB').save(g('ref_ancha.png'))
    t = paisaje(200, 320).transpose(Image.Transpose.ROTATE_90)
    t.save(g('TORRE.PNG'))
    t.save(g('ref_torre.png'))
    # lo que no se puede ver, o solo en parte
    d1 = open(g('PAISAJE.PNG'), 'rb').read()
    open(g('CORTADA.PNG'), 'wb').write(d1[:len(d1) * 55 // 100])
    open(g('NOTA.BMP'), 'wb').write(b'Esto no es una imagen: es una nota que alguien ha llamado .BMP\r\n' * 4)
    barras(16, 16).save(g('RARO.BMP'))
    bm = bytearray(open(g('RARO.BMP'), 'rb').read())
    bm[30] = 4                          # (dice que lleva dentro un JPEG)
    open(g('RARO.BMP'), 'wb').write(bytes(bm))
    # enormes que ocupan poco: se tienen que reducir al leerlas
    open(g('ENORME.PNG'), 'wb').write(png_grande(3000, 2000))
    e = Image.new('RGB', (2560, 1920))
    ed = ImageDraw.Draw(e)
    ed.rectangle((0, 0, 1279, 959), fill=(255, 0, 0))
    ed.rectangle((1280, 0, 2559, 959), fill=(0, 255, 0))
    ed.rectangle((0, 960, 1279, 1919), fill=(0, 0, 255))
    ed.rectangle((1280, 960, 2559, 1919), fill=(255, 255, 0))
    e.save(g('FOTON.JPG'), quality=30)
    e.resize((400, 300)).save(g('ref_foton.png'))
    r = Image.new('RGB', (300, 200))
    rd = ImageDraw.Draw(r)
    rd.rectangle((0, 0, 149, 99), fill=(255, 0, 0))
    rd.rectangle((150, 0, 299, 99), fill=(0, 255, 0))
    rd.rectangle((0, 100, 149, 199), fill=(0, 0, 255))
    rd.rectangle((150, 100, 299, 199), fill=(255, 255, 0))
    rd.line((0, 0, 299, 199), fill=(0, 0, 0), width=2)
    r.save(g('ref_enorme.png'))
    return 0


def gira(im, giro):
    T = Image.Transpose
    return {'': im, 'nada': im, 'izq': im.transpose(T.ROTATE_90), 'der': im.transpose(T.ROTATE_270),
            'h': im.transpose(T.FLIP_LEFT_RIGHT), 'v': im.transpose(T.FLIP_TOP_BOTTOM),
            'media': im.transpose(T.ROTATE_180)}[giro]


def medias(im, lado=16):
    an, al = im.size
    px = im.load()
    out = []
    for y in range(0, al - lado + 1, lado):
        for x in range(0, an - lado + 1, lado):
            s = [0, 0, 0]
            for j in range(lado):
                for i in range(lado):
                    p = px[x + i, y + j]
                    s[0] += p[0]
                    s[1] += p[1]
                    s[2] += p[2]
            out.append(tuple(v / (lado * lado) for v in s))
    return out


def compara(a, b, margen, nombre):
    lado = 16 if min(a.size) >= 64 else 8
    ma, mb = medias(a, lado), medias(b, lado)
    if not ma:
        print('  FAIL %s: la zona es demasiado pequeña' % nombre)
        return 1
    difs = sorted(sum(abs(x - y) for x, y in zip(p, q)) / 3 for p, q in zip(ma, mb))
    media = sum(difs) / len(difs)
    peor = difs[len(difs) * 95 // 100 - 1] if len(difs) >= 20 else difs[-1]
    ok = media <= margen and peor <= margen * 4
    print('  %s %s (diferencia media %.1f; el 95 %% de los cuadros, hasta %.1f)' % ('OK  ' if ok else 'FAIL', nombre, media, peor))
    return 0 if ok else 1


def captura(ruta):
    d = open(ruta, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return Image.frombytes('RGB', (int(m.group(1)), int(m.group(2))), d[m.end():])


def ve(a):
    cap = captura(a[0])
    x, y, an, al = (int(v) for v in a[1:5])
    ref = Image.open(a[5]).convert('RGB')
    giro = a[6] if len(a) > 6 else ''
    margen = float(a[7]) if len(a) > 7 else 10
    nombre = a[8] if len(a) > 8 else 'se ve %s' % os.path.basename(a[5])
    if ' ' in giro:         # «X Y»: el trozo de la referencia que empieza ahí, punto por punto
        rx, ry = (int(v) for v in giro.split())
        ref = ref.crop((rx, ry, rx + an, ry + al))
    else:
        ref = gira(ref, giro).resize((an, al), Image.Resampling.BOX)
    return compara(cap.crop((x, y, x + an, y + al)), ref, margen, nombre)


def bmp(a):
    nombre = a[5] if len(a) > 5 else 'el BMP guardado es %s' % os.path.basename(a[1])
    try:
        im = Image.open(a[0])
        im.load()
    except Exception as e:
        print('  FAIL %s: no se abre (%s)' % (nombre, e))
        return 1
    d = open(a[0], 'rb').read()
    bits = struct.unpack_from('<H', d, 28)[0]
    ref = gira(Image.open(a[1]).convert('RGB'), a[3] if len(a) > 3 else '')
    if bits != int(a[2]):
        print('  FAIL %s: es de %d bits y se esperaba de %s' % (nombre, bits, a[2]))
        return 1
    if im.size != ref.size:
        print('  FAIL %s: mide %dx%d y se esperaba %dx%d' % ((nombre,) + im.size + ref.size))
        return 1
    return compara(im.convert('RGB'), ref, float(a[4]) if len(a) > 4 else 10, nombre + ' (%d bits, %dx%d)' % ((bits,) + im.size))


def impreso(a):
    """rehace la hoja de los gráficos ESC/P (ESC * 5 n1 n2 y columnas de 8 agujas) y la compara"""
    d = open(a[0], 'rb').read()
    ref = Image.open(a[1]).convert('L')
    apaisada = len(a) > 2 and a[2] == 'apaisada'
    nombre = a[3] if len(a) > 3 else 'lo impreso es %s' % os.path.basename(a[1])
    filas = []          # tiras de 8 filas, de 576 columnas
    i, n_ff = 0, 0
    tira = None
    while i < len(d):
        if d[i] == 27 and d[i + 1:i + 2] == b'@':
            i += 2
        elif d[i] == 27 and d[i + 1:i + 2] == b'A':
            i += 3
        elif d[i] == 27 and d[i + 1:i + 2] == b'*':
            n = d[i + 3] + 256 * d[i + 4]
            tira = d[i + 5:i + 5 + n]
            i += 5 + n
        elif d[i] == 10:
            filas.append(tira)
            tira = None
            i += 1
        elif d[i] == 12:
            n_ff += 1
            i += 1
        elif d[i] == 13:
            i += 1
        else:
            print('  FAIL %s: hay un octeto que no es de los gráficos (%d en %d)' % (nombre, d[i], i))
            return 1
    if n_ff != 1 or not filas:
        print('  FAIL %s: %d tiras y %d saltos de hoja' % (nombre, len(filas), n_ff))
        return 1
    hoja = Image.new('L', (576, len(filas) * 8), 255)
    px = hoja.load()
    for t, tira in enumerate(filas):
        for x, b in enumerate(tira or b''):
            for k in range(8):
                if b & (0x80 >> k):
                    px[x, t * 8 + k] = 0
    caja = hoja.point(lambda v: 255 - v).getbbox()
    if not caja:
        print('  FAIL %s: la hoja sale en blanco' % nombre)
        return 1
    if apaisada:
        ref = ref.transpose(Image.Transpose.ROTATE_270)
    # la imagen ocupa el ancho de la hoja o su alto, centrada a lo ancho
    esc = min(576 / ref.size[0], 760 / ref.size[1], 4)
    an, al = max(1, int(ref.size[0] * esc)), max(1, int(ref.size[1] * esc))
    x0 = (576 - an) // 2
    if al > hoja.size[1] or abs(len(filas) * 8 - al) > 8:
        print('  FAIL %s: la hoja tiene %d filas de puntos y la imagen tenía que ocupar %d' % (nombre, len(filas) * 8, al))
        return 1
    rec = hoja.crop((x0, 0, x0 + an, al)).convert('RGB')
    return compara(rec, ref.resize((an, al), Image.Resampling.BOX).convert('RGB'), 14,
                   nombre + ' (%dx%d puntos%s)' % (an, al, ', apaisada' if apaisada else ''))


if __name__ == '__main__':
    orden = sys.argv[1] if len(sys.argv) > 1 else ''
    if orden == 'haz':
        sys.exit(haz(sys.argv[2]))
    if orden == 've':
        sys.exit(ve(sys.argv[2:]))
    if orden == 'bmp':
        sys.exit(bmp(sys.argv[2:]))
    if orden == 'impreso':
        sys.exit(impreso(sys.argv[2:]))
    print(__doc__)
    sys.exit(2)
