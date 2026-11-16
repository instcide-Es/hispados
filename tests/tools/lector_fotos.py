#!/usr/bin/env python3
"""Las fotos y los escaneos con los que se comprueba que Lector enseña las JPEG.

uso: lector_fotos.py haz CARPETA         hace FOTOS.PDF, ESCANG.PDF y ESCANC.PDF
     lector_fotos.py comprueba CARPETA   el núcleo de Lector, con esos PDF:
                                         cada miniatura se parece a su foto
     lector_fotos.py pantalla CAPTURA.ppm CARPETA NOMBRE.PDF PAGINA [aviso]
                                         lo mismo, en una captura de la pantalla
                                         de Lector con esa página a la vista
                                         (entera a lo ancho; «aviso»: lleva franja)
     lector_fotos.py oscuro CAPTURA.ppm X Y    sale con 0 si ese punto es oscuro

Las JPEG de tests/tools/hazpdf.py son de bloques lisos, para saber cada punto;
estas son «de verdad»: las hace ImageMagick (libjpeg) con sus imágenes de
serie (el mago del logotipo, la rosa) y con texto: una foto en color 4:2:0,
otra 4:2:2, una gris, una CMYK de Adobe (con su /Decode), una progresiva, una
grande (se reduce al vuelo; muy comprimida, para que el PDF quepa en el
disquete de la prueba) y una con codificación aritmética, si el libjpeg
de la máquina la sabe hacer (esa no se sabe pintar: recuadro y aviso). Y una
página A4 «escaneada» a 150 puntos por pulgada (1240 x 1754), en gris y en
color: texto, una foto, algo de giro, ruido y desenfoque, como salen de un
escáner; van en la segunda página de su PDF, detrás de una de texto, para
medir cuánto tarda Lector en componerla sin contar la lectura del disco.

«pantalla» busca la hoja en la captura (lo que no es el fondo crema de la
zona de lectura), saca de su ancho la escala y compara cada foto que se vea
(al menos un tercio) con la misma foto reducida por ImageMagick a lo que ocupa.

«comprueba» pasa cada PDF por el núcleo (tests/tools/lector_nucleo.py) y
compara cada miniatura, pasada de la paleta a rojo, verde y azul y promediada
en cuadros de 4 x 4 (para quitar el tramado), con la misma foto reducida por
ImageMagick a ese tamaño.
"""
import json, os, subprocess, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hazpdf
from hazpdf import A4, AV_IMAGEN, AV_ROTO, AV_MEMORIA


def convierte(*arg):
    r = subprocess.run(['convert'] + list(arg) + ['jpg:-'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if r.returncode or not r.stdout.startswith(b'\xff\xd8'):
        raise RuntimeError('convert %s: %s' % (' '.join(arg), r.stderr.decode('utf-8', 'replace')[:200]))
    return r.stdout


def medidas(j):
    """ancho, alto y componentes de un JPEG, de su SOF"""
    p = 2
    while p < len(j):
        m, n = j[p + 1], int.from_bytes(j[p + 2:p + 4], 'big')
        if 0xC0 <= m <= 0xCF and m not in (0xC4, 0xC8, 0xCC):
            return int.from_bytes(j[p + 7:p + 9], 'big'), int.from_bytes(j[p + 5:p + 7], 'big'), j[p + 9], m
        p += 2 + n
    raise ValueError('sin SOF')


TEXTO = ['ACTA DE LA REUNION', '',
         'Reunidos en el local social a las seis de la tarde, con la',
         'asistencia de cuarenta y dos socios, se trataron los puntos',
         'del orden del dia que se detallan a continuacion.', '',
         '1. Lectura y aprobacion del acta anterior.',
         '2. Estado de cuentas del tercer trimestre: 10.518,79 euros',
         '   en el banco y 196,90 en la caja.',
         '3. Excursion de otono: salida el dia 21 a las 8:30 desde',
         '   la plaza; el autocar tiene 54 plazas.',
         '4. Loteria de Navidad: se reparten 200 participaciones.',
         '5. Ruegos y preguntas.', '',
         'Se aprueban las cuentas por unanimidad. La tesorera explica',
         'que las cuotas de 2026 se cobraran en enero, por el banco,',
         'y que quien lo prefiera puede pagar en el local los martes',
         'y los jueves por la tarde.', '',
         'Sin mas asuntos que tratar, se levanta la sesion a las',
         'siete y cuarto, de lo que doy fe como secretario.']


def escaneo(color):
    """Una página A4 a 150 puntos por pulgada, como sale de un escáner."""
    a = ['-size', '1240x1754', 'xc:#f6f4ee' if color else 'xc:#f4f4f4', '-font', 'Helvetica']
    a += ['-fill', '#1a3e8c' if color else '#202020', '-pointsize', '52', '-annotate', '+120+200', TEXTO[0]]
    a += ['-fill', '#181818', '-pointsize', '30']
    y = 300
    for l in TEXTO[2:]:
        if l:
            a += ['-annotate', '+120+%d' % y, l]
        y += 46
    a += ['-fill', 'black', '-draw', 'rectangle 60,60 140,140']        # una marca negra, arriba a la izquierda
    a += ['-fill', '#c02020' if color else '#606060', '-draw', 'rectangle 1100,60 1180,140']
    a += ['-stroke', '#c02020' if color else '#505050', '-strokewidth', '6', '-fill', 'none', '-draw', 'circle 930,1480 930,1380',
          '-stroke', 'none', '-fill', '#c02020' if color else '#505050', '-pointsize', '34', '-annotate', '+850+1492', 'PAGADO']
    a += ['(', 'rose:', '-resize', '420x', ')', '-geometry', '+140+1300', '-composite']
    a += ['-background', 'white', '-rotate', '0.4', '-gravity', 'center', '-crop', '1240x1754+0+0', '+repage',
          '-attenuate', '0.35', '+noise', 'Gaussian', '-blur', '0x0.5']
    if color:
        a += ['-sampling-factor', '2x2', '-quality', '45']
    else:
        a += ['-colorspace', 'Gray', '-quality', '50']
    return convierte(*a)


def haz(carpeta):
    os.makedirs(carpeta, exist_ok=True)
    lista = []                                              # lo que hay en cada PDF, para «comprueba»
    hazpdf.FILTROS['DCTDecode'] = lambda d: d

    def imagen(p, j, mas=''):
        an, al, nc, _ = medidas(j)
        cs = {1: '/DeviceGray', 3: '/DeviceRGB', 4: '/DeviceCMYK'}[nc]
        return p.flujo(j, ' /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace %s /BitsPerComponent 8%s' % (an, al, cs, mas), ('DCTDecode',))

    # ---- FOTOS.PDF ----
    p = hazpdf.Pdf()
    f1 = p.fuente('Helvetica')
    fotos = [('Mago', convierte('logo:', '-sampling-factor', '2x2', '-quality', '85'), 40, 560, 300, 225, 'Color 4:2:0 (640 x 480)', ''),
             ('Rosa', convierte('rose:', '-resize', '350x230!', '-sampling-factor', '2x1', '-quality', '90'), 360, 670, 175, 115, 'Color 4:2:2', ''),
             ('Gris', convierte('wizard:', '-colorspace', 'Gray', '-quality', '80'), 360, 430, 150, 200, 'Gris (480 x 640)', ''),
             ('Cmyk', convierte('rose:', '-resize', '280x184!', '-colorspace', 'CMYK', '-quality', '90'), 40, 400, 140, 92, 'CMYK de Adobe', ' /Decode [1 0 1 0 1 0 1 0]'),
             ('Prog', convierte('logo:', '-resize', '400x300', '-interlace', 'JPEG', '-quality', '80'), 200, 380, 150, 112, 'Progresiva', ''),
             ('Grande', convierte('wizard:', '-resize', '1500x2000!', '-quality', '22'), 40, 150, 150, 200, 'Grande (1500 x 2000)', '')]
    try:
        arit = convierte('rose:', '-define', 'jpeg:arithmetic-coding=on', '-quality', '80')
        if medidas(arit)[3] not in (0xC9, 0xCA):
            raise RuntimeError('no la hace aritmética')
    except Exception:
        arit = convierte('rose:', '-quality', '80')         # (este libjpeg no sabe: se le cambia la cabecera)
        i = arit.index(b'\xff\xc0')
        arit = arit[:i] + b'\xff\xc9' + arit[i + 2:]
    fotos.append(('Arit', arit, 220, 150, 140, 92, 'Aritmetica: no se sabe', ''))
    s, xo = '', {}
    for n, j, x, y, an, al, rotulo, mas in fotos:
        xo[n] = imagen(p, j, mas)
        s += 'q %d 0 0 %d %d %d cm /%s Do Q BT /F1 9 Tf %d %d Td (%s) Tj ET\n' % (an, al, x, y, n, x, y - 11, rotulo)
        open(os.path.join(carpeta, n + '.jpg'), 'wb').write(j)
        lista.append(dict(pdf='FOTOS.PDF', pag=0, x=x, y=A4[1] - y - al, an=an, al=al, jpg=n + '.jpg', sabe=n != 'Arit', gris=n == 'Gris'))
    s += 'BT /F1 14 Tf 40 800 Td (Fotos JPEG en un PDF) Tj ET\n'
    raiz = p.paginas([p.flujo(s)], {'F1': f1}, mas_recursos=' /XObject << %s >>' % ' '.join('/%s %d 0 R' % kv for kv in xo.items()))
    open(os.path.join(carpeta, 'FOTOS.PDF'), 'wb').write(p.clasico(raiz))
    # ---- ESCANG.PDF y ESCANC.PDF: una página de texto y la escaneada ----
    for nombre, color in (('ESCANG', False), ('ESCANC', True)):
        j = escaneo(color)
        p = hazpdf.Pdf()
        f1 = p.fuente('Helvetica')
        im = imagen(p, j)
        c1 = p.flujo('BT /F1 14 Tf 72 780 Td (La pagina siguiente es un escaneo a 150 puntos por pulgada.) Tj ET')
        c2 = p.flujo('q %s 0 0 %s 0 0 cm /Esc Do Q' % A4)
        raiz = p.paginas([c1, c2], {'F1': f1}, mas_recursos=' /XObject << /Esc %d 0 R >>' % im)
        open(os.path.join(carpeta, nombre + '.PDF'), 'wb').write(p.clasico(raiz))
        open(os.path.join(carpeta, nombre + '.jpg'), 'wb').write(j)
        lista.append(dict(pdf=nombre + '.PDF', pag=1, x=0, y=0, an=A4[0], al=A4[1], jpg=nombre + '.jpg', sabe=True, gris=not color))
    json.dump(lista, open(os.path.join(carpeta, 'fotos.json'), 'w'))
    for f in ('FOTOS.PDF', 'ESCANG.PDF', 'ESCANC.PDF'):
        print('%-12s %7d octetos' % (f, os.path.getsize(os.path.join(carpeta, f))))


def a_rgb(k):
    """un color de la paleta de Hispa Ventanas (el cubo de 6x6x6 y los grises), a rojo, verde y azul"""
    if 16 <= k < 232:
        k -= 16
        return (k // 36 * 51, k // 6 % 6 * 51, k % 6 * 51)
    if k >= 232:
        return ((k - 232) * 11,) * 3
    return (0, 0, 0)


def cuadros(pts, an, al, n=4):
    """la media de cada cuadro de n x n (pts: an*al ternas)"""
    s = []
    for y in range(0, al - n + 1, n):
        for x in range(0, an - n + 1, n):
            t = [0, 0, 0]
            for yy in range(y, y + n):
                for xx in range(x, x + n):
                    c = pts[yy * an + xx]
                    t[0] += c[0]; t[1] += c[1]; t[2] += c[2]
            s.append(tuple(v / (n * n) for v in t))
    return s


def comprueba(carpeta):
    from lector_nucleo import Nucleo, monta
    lista = json.load(open(os.path.join(carpeta, 'fotos.json')))
    fallos = 0

    def res(ok, t):
        nonlocal fallos
        fallos += not ok
        print('  %s %s' % ('OK  ' if ok else 'FAIL', t))

    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        vistos = {}
        for pdf in sorted({e['pdf'] for e in lista}):
            t0 = time.time()
            vistos[pdf] = Nucleo(prog, os.path.join(carpeta, pdf), tiempo=120)
            n = vistos[pdf]
            res(not n.colgado and n.codigo == 0 and n.err == 0, '%s: se abre (%.2f s fuera de QEMU)' % (pdf, time.time() - t0))
        for e in lista:
            n = vistos[e['pdf']]
            d = '%s, %s' % (e['pdf'], e['jpg'])
            if n.colgado or n.codigo or e['pag'] >= len(n.pags):
                res(False, d + ': no hay página')
                continue
            pg = n.pags[e['pag']]
            cand = [k for k in pg.de('I') if abs(k[1] - e['x']) < 0.6 and abs(k[2] - e['y']) < 0.6 and abs(k[3] - e['an']) < 0.6]
            if not cand:
                res(False, d + ': no está la imagen en su sitio')
                continue
            mini = cand[0][8]
            if not e['sabe']:
                res(mini is None and pg.avisos & AV_IMAGEN and pg.que == 'JPEG aritmético', d + ': recuadro, con su aviso («%s»)' % pg.que)
                continue
            if mini is None:
                res(False, d + ': se ha quedado en recuadro (avisos %d, «%s»)' % (pg.avisos, pg.que))
                continue
            tw, th, pts = mini
            mio = [a_rgb(k) for k in pts]
            ref = subprocess.run(['convert', os.path.join(carpeta, e['jpg']), '-colorspace', 'sRGB', '-resize', '%dx%d!' % (tw, th), '-depth', '8', 'rgb:-'],
                                 stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
            if len(ref) != tw * th * 3:
                res(False, d + ': ImageMagick no da la referencia')
                continue
            suyo = [tuple(ref[i:i + 3]) for i in range(0, len(ref), 3)]
            a, b = cuadros(mio, tw, th), cuadros(suyo, tw, th)
            dif = sum(abs(x - y) for p, q in zip(a, b) for x, y in zip(p, q)) / (3 * len(a))
            peor = max(max(abs(x - y) for x, y in zip(p, q)) for p, q in zip(a, b))
            # el tramado (51 niveles por paso del cubo) y los 24 grises dejan unos pocos niveles de media
            res(dif < 9 and pg.avisos & (AV_IMAGEN | AV_ROTO | AV_MEMORIA) == (0 if e['pdf'] != 'FOTOS.PDF' else AV_IMAGEN),
                '%s: miniatura de %d x %d; en cuadros de 4 x 4 difiere de la foto %.1f de media (%.0f el peor); avisos %d' % (d, tw, th, dif, peor, pg.avisos))
    print('  %s fotos y escaneos en el núcleo: %d fallos' % ('OK  ' if not fallos else 'FAIL', fallos))
    sys.exit(1 if fallos else 0)


def lee_ppm(ruta):
    d = open(ruta, 'rb').read()
    c = d.split(None, 4)
    assert c[0] == b'P6' and c[3] == b'255'
    return int(c[1]), int(c[2]), c[4]


def pantalla(ppm, carpeta, pdf, pag, aviso):
    an, al, d = lee_ppm(ppm)
    pt = lambda x, y: tuple(d[(y * an + x) * 3:(y * an + x) * 3 + 3])
    lista = [e for e in json.load(open(os.path.join(carpeta, 'fotos.json'))) if e['pdf'] == pdf and e['pag'] == pag and e['sabe']]
    # la zona de lectura (bajo la barra y la franja del aviso, hasta las barras
    # de desplazamiento) y, dentro, la hoja: lo que no es del color del fondo
    zx0, zx1, zy0, zy1 = 44, 738, (130 if aviso else 110), 536
    fondo = pt(45, zy0 + 1)
    xs = [x for x in range(zx0, zx1) if any(pt(x, y) != fondo for y in range(zy0, zy1, 3))]
    ys = [y for y in range(zy0, zy1) if any(pt(x, y) != fondo for x in range(zx0, zx1, 3))]
    fallos = 0
    if not xs or not ys or xs[-1] - xs[0] < 100:
        print('  FAIL %s: no se ve la hoja en la captura' % pdf)
        sys.exit(1)
    px0, py0, esc = xs[0], ys[0], (xs[-1] - xs[0] + 1) / A4[0]
    vistas = 0
    for e in lista:
        x0, y0 = px0 + e['x'] * esc, py0 + e['y'] * esc
        w, h = max(1, round(e['an'] * esc)), max(1, round(e['al'] * esc))
        x0, y0 = round(x0), round(y0)
        # lo que se ve de ella
        a, b, c, f = max(x0, zx0), max(y0, zy0), min(x0 + w, zx1), min(y0 + h, zy1)
        if c - a < 8 or f - b < 8 or (c - a) * (f - b) < w * h / 3:
            continue
        vistas += 1
        ref = subprocess.run(['convert', os.path.join(carpeta, e['jpg']), '-colorspace', 'sRGB', '-resize', '%dx%d!' % (w, h), '-depth', '8', 'rgb:-'],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
        suyo = [tuple(ref[((y - y0) * w + (x - x0)) * 3:((y - y0) * w + (x - x0)) * 3 + 3]) for y in range(b, f) for x in range(a, c)]
        mio = [pt(x, y) for y in range(b, f) for x in range(a, c)]
        ca, cb = cuadros(mio, c - a, f - b), cuadros(suyo, c - a, f - b)
        dif = sum(abs(u - v) for p, q in zip(ca, cb) for u, v in zip(p, q)) / (3 * len(ca))
        # un recuadro gris (o la hoja en blanco) queda a 40 o más de cualquier foto
        ok = dif < 16
        fallos += not ok
        print('  %s %s en la pantalla (%d x %d puntos en %d,%d; se ve un %d %%): en cuadros de 4 x 4 difiere de la foto %.1f de media' % (
            'OK  ' if ok else 'FAIL', e['jpg'], w, h, x0, y0, 100 * (c - a) * (f - b) // (w * h), dif))
    if not vistas:
        print('  FAIL %s: no hay ninguna foto a la vista' % pdf)
        fallos += 1
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    if len(sys.argv) >= 6 and sys.argv[1] == 'pantalla':
        pantalla(sys.argv[2], sys.argv[3], sys.argv[4], int(sys.argv[5]), len(sys.argv) > 6)
    elif len(sys.argv) == 5 and sys.argv[1] == 'oscuro':
        an, al, d = lee_ppm(sys.argv[2])
        i = (int(sys.argv[4]) * an + int(sys.argv[3])) * 3
        sys.exit(0 if sum(d[i:i + 3]) < 240 else 1)
    elif len(sys.argv) == 3 and sys.argv[1] == 'haz':
        haz(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == 'comprueba':
        comprueba(sys.argv[2])
    else:
        sys.exit(__doc__)
