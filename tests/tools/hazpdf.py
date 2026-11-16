#!/usr/bin/env python3
"""Hace los PDF con los que se prueba Lector (programas/lector.asm).

uso: hazpdf.py CARPETA        deja en CARPETA todos los PDF (NOMBRE.PDF, en 8.3)

Están escritos «a mano», objeto a objeto, para saber exactamente lo que
llevan: sin comprimir y con FlateDecode, ASCIIHex, ASCII85 y RunLength, varias
páginas, árbol de páginas con herencia, tabla xref clásica, actualizaciones
incrementales, índice en flujo y flujos de objetos (PDF 1.5, con predictor de
PNG), las fuentes de siempre con WinAnsi, MacRoman, Standard, /Differences y
/ToUnicode, fuentes compuestas (Type0), rayas, rectángulos, curvas, imágenes,
formularios, páginas giradas... y los que no se pueden leer (cifrado, filtro
desconocido, letra con codificación propia) y los rotos. Las JPEG también se
hacen aquí (jpeg_liso): cada bloque de 8x8 de un color, para saber punto por
punto lo que tiene que salir; las fotos y los escaneos de la parte que se
mira en la pantalla los hace tests/tools/lector_fotos.py con ImageMagick.

Cada caso es (nombre, octetos, lo que se espera); lo que se espera lo comprueba
tests/tools/lector_pdf.py contra lo que dice el núcleo de Lector:
  ('error', n)                 pdf_abrir da ese error (0: se abre)
  ('paginas', n)               tiene n páginas
  ('reparado', 0/1)            ha habido que rehacer el índice
  ('muchos', 0/1)              tiene más páginas u objetos de los que caben
  ('titulo', t), ('autor', t)  la información del documento
  ('tam', pag, ancho, alto)    el tamaño de la página, en puntos
  ('aviso', pag, bits)         la página lleva esos avisos (AV_*)
  ('sin_aviso', pag, bits)     y no lleva esos
  ('texto', pag, t, x, y, tam[, letra[, ancho]])  hay un trozo de texto t que
                               empieza en x, y (desde arriba), de ese tamaño
  ('contiene', pag, t)         algún trozo de texto contiene t
  ('no_texto', pag, t)         ninguno contiene t
  ('lineas', pag, [l1, l2...]) el texto extraído, línea a línea
  ('raya', pag, x0, y0, x1, y1[, grosor[, color]])
  ('rect', pag, x, y, an, al[, color])
  ('imagen', pag, x, y, an, al)
  ('ilegible', pag, x, y)      hay un trozo ilegible ahí
  ('figura', pag, nvertices)   hay una figura rellena con esos vértices
  ('ncosas', pag, clase, n)    cuántas cosas de esa clase hay
  ('mini', pag, x, y, an, al, ancho, alto, puntos, giro)  hay una imagen ahí con esa
                               miniatura (puntos: los colores de la paleta; None: no se sabe pintar)
  ('recorte', pag, clase, texto o color[, x0, y0, x1, y1])  esa cosa lleva ese recorte (o ninguno)
  ('que', pag, nombre)         el formato de imagen que la página dice que no sabe pintar ('' ninguno)
  ('halla', buscar, pag, veces)
Las páginas se cuentan desde 0; las medidas van en puntos, con la «y» desde
arriba de la página (como las deja Lector).
"""
import base64, os, struct, sys, zlib

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'programas', 'lector'))
import haznombres

A4 = (595.28, 841.89)
AV_ILEGIBLE, AV_DESCON, AV_FILTRO, AV_ROTO, AV_LLENA, AV_IMAGEN, AV_MEMORIA = 1, 2, 4, 8, 16, 32, 64
ROBLE, ROBLE_N, IMPRENTA, LLANO, LLANO_N, CLARO, MAQUINA = 1, 2, 3, 9, 10, 11, 17
NEGRO, BLANCO = 16, 231


def ancho(texto, tam, tabla=0):
    """Lo que ocupa un texto en Helvetica (0), Helvetica-Bold (1), Times (2), Times-Bold (3) o Courier (4)."""
    if tabla == 4:
        return 0.6 * tam * len(texto)
    t = haznombres.ANCHOS[tabla]
    quita = str.maketrans('áéíóúüñÁÉÍÓÚÜÑ¿¡«»', 'aeiouunAEIOUUN?!""')
    return sum(t[ord(c) - 32] if 32 <= ord(c) < 127 else 500 for c in texto.translate(quita)) * tam / 1000


def cad(s, cod='latin-1'):
    """Una cadena literal de PDF."""
    b = s.encode(cod) if isinstance(s, str) else s
    return b'(' + b.replace(b'\\', b'\\\\').replace(b'(', b'\\(').replace(b')', b'\\)') + b')'


def rl(d):
    """RunLengthDecode: trozos repetidos y trozos tal cual."""
    s, i = bytearray(), 0
    while i < len(d):
        j = i
        while j < len(d) and j - i < 128 and d[j] == d[i]:
            j += 1
        if j - i >= 2:
            s += bytes([257 - (j - i), d[i]])
            i = j
        else:
            j = i
            while j < len(d) and j - i < 128 and not (j + 1 < len(d) and d[j] == d[j + 1]):
                j += 1
            s += bytes([j - i - 1]) + d[i:j]
            i = j
    return bytes(s) + b'\x80'


def lzw(d, early=1):
    """LZWDecode: códigos de 9 a 12 bits, el de más peso primero."""
    bits, acc, nb, sal = 9, 0, 0, bytearray()

    def pon(c):
        nonlocal acc, nb
        acc = (acc << bits) | c
        nb += bits
        while nb >= 8:
            nb -= 8
            sal.append((acc >> nb) & 255)
    tabla = {bytes([i]): i for i in range(256)}
    sig, w = 258, b''
    pon(256)
    for c in d:
        wc = w + bytes([c])
        if wc in tabla:
            w = wc
            continue
        pon(tabla[w])
        tabla[wc] = sig
        sig += 1
        if sig - 1 + early >= (1 << bits) and bits < 12:
            bits += 1
        if sig >= 4094:
            pon(256)
            tabla = {bytes([i]): i for i in range(256)}
            sig, bits = 258, 9
        w = bytes([c])
    if w:
        pon(tabla[w])
        # (el que lee añade su entrada con un código de retraso: el tamaño cambia igual)
        sig += 1
        if sig - 1 + early >= (1 << bits) and bits < 12:
            bits += 1
    pon(257)
    if nb:
        sal.append((acc << (8 - nb)) & 255)
    return bytes(sal)


FILTROS = {
    'LZWDecode': lzw,
    'FlateDecode': lambda d: zlib.compress(d, 9),
    'ASCIIHexDecode': lambda d: b'\n'.join(d.hex().upper().encode()[i:i + 72] for i in range(0, 2 * len(d), 72)) + b'>',
    'ASCII85Decode': lambda d: base64.a85encode(d, wrapcol=72) + b'~>',
    'RunLengthDecode': rl,
}


class Pdf:
    """Un PDF, objeto a objeto."""

    def __init__(self, ver='1.4'):
        self.ver = ver
        self.o = {}                 # número -> cuerpo
        self.n = 0
        self.trailer = b''          # más cosas para el «trailer»

    def nuevo(self):
        self.n += 1
        return self.n

    def pon(self, num, cuerpo):
        self.o[num] = cuerpo if isinstance(cuerpo, bytes) else cuerpo.encode('latin-1')
        return num

    def obj(self, cuerpo):
        return self.pon(self.nuevo(), cuerpo)

    def flujo(self, datos, dic='', filtros=(), num=None, largo=None):
        """Un flujo; «filtros», en el orden en que hay que deshacerlos."""
        if isinstance(datos, str):
            datos = datos.encode('latin-1')
        for f in reversed(filtros):
            datos = FILTROS[f](datos)
        d = dic
        if len(filtros) == 1:
            d += ' /Filter /%s' % filtros[0]
        elif filtros:
            d += ' /Filter [%s]' % ' '.join('/' + f for f in filtros)
        cuerpo = ('<< /Length %s%s >>\nstream\n' % (largo if largo is not None else len(datos), d)).encode('latin-1') + datos + b'\nendstream'
        return self.pon(num if num else self.nuevo(), cuerpo)

    def fuente(self, base, cod='/WinAnsiEncoding', mas=''):
        return self.obj('<< /Type /Font /Subtype /Type1 /BaseFont /%s%s%s >>' % (base, ' /Encoding ' + cod if cod else '', mas))

    def paginas(self, contenidos, fuentes, caja=A4, mas_recursos='', mas_pagina=None):
        """Un árbol de una sola rama: cada página, con su contenido (un número
        de objeto o una lista). Devuelve el catálogo."""
        raiz = self.nuevo()
        rec = '<< /Font << %s >>%s >>' % (' '.join('/%s %d 0 R' % (k, v) for k, v in fuentes.items()), mas_recursos)
        hijos = []
        for i, c in enumerate(contenidos):
            cont = '%d 0 R' % c if isinstance(c, int) else '[%s]' % ' '.join('%d 0 R' % x for x in c)
            mas = (mas_pagina or {}).get(i, '')
            hijos.append(self.obj('<< /Type /Page /Parent %d 0 R /Contents %s%s >>' % (raiz, cont, mas)))
        self.pon(raiz, '<< /Type /Pages /Kids [%s] /Count %d /MediaBox [0 0 %s %s] /Resources %s >>' % (
            ' '.join('%d 0 R' % h for h in hijos), len(hijos), caja[0], caja[1], rec))
        return self.obj('<< /Type /Catalog /Pages %d 0 R >>' % raiz)

    # ---- escribir ----
    def _cuerpo(self, nums, base=0):
        s, donde = b'', {}
        for n in nums:
            donde[n] = base + len(s)
            s += b'%d 0 obj\n' % n + self.o[n] + b'\nendobj\n'
        return s, donde

    def clasico(self, raiz, info=None, orden=None):
        """Con la tabla xref de siempre. «orden»: en qué orden van los objetos."""
        cab = ('%%PDF-%s\n%%\xe2\xe3\xcf\xd3\n' % self.ver).encode('latin-1')
        cuerpo, donde = self._cuerpo(orden or sorted(self.o), len(cab))
        s = cab + cuerpo
        xref = len(s)
        s += b'xref\n0 %d\n' % (self.n + 1)
        s += b'0000000000 65535 f \n'
        for n in range(1, self.n + 1):
            s += b'%010d 00000 n \n' % donde[n] if n in donde else b'0000000000 00001 f \n'
        s += b'trailer\n<< /Size %d /Root %d 0 R' % (self.n + 1, raiz)
        if info:
            s += b' /Info %d 0 R' % info
        s += self.trailer + b' >>\nstartxref\n%d\n%%%%EOF\n' % xref
        return s

    def moderno(self, raiz, info=None, en_flujo=(), predictor=True, por_flujo=100):
        """PDF 1.5: los objetos de «en_flujo», dentro de flujos de objetos
        comprimidos; y el índice, en un flujo /XRef (con predictor de PNG)."""
        cab = ('%%PDF-%s\n%%\xe2\xe3\xcf\xd3\n' % self.ver).encode('latin-1')
        en_flujo = [n for n in sorted(en_flujo) if not self.o[n].rstrip().endswith(b'endstream')]
        sitio = {}                  # número -> (clase, a, b)
        for i in range(0, len(en_flujo), por_flujo):
            grupo = en_flujo[i:i + por_flujo]
            cab_os, datos = b'', b''
            for k, n in enumerate(grupo):
                cab_os += b'%d %d ' % (n, len(datos))
                datos += self.o[n] + b'\n'
            num = self.flujo(cab_os + datos, ' /Type /ObjStm /N %d /First %d' % (len(grupo), len(cab_os)), ('FlateDecode',))
            for k, n in enumerate(grupo):
                sitio[n] = (2, num, k)
        sueltos = [n for n in sorted(self.o) if n not in sitio]
        cuerpo, donde = self._cuerpo(sueltos, len(cab))
        for n in sueltos:
            sitio[n] = (1, donde[n], 0)
        xnum = self.nuevo()
        sitio[xnum] = (1, len(cab) + len(cuerpo), 0)
        filas = b''
        for n in range(0, self.n + 1):
            c, a, b = sitio.get(n, (0, 0, 65535 if n == 0 else 0))
            filas += struct.pack('>BIH', c, a, b)
        dic = ' /Type /XRef /Size %d /W [1 4 2] /Root %d 0 R' % (self.n + 1, raiz)
        if info:
            dic += ' /Info %d 0 R' % info
        dic += self.trailer.decode('latin-1')
        if predictor:
            # predictor 12 («Up») de PNG: cada fila, menos la de arriba
            ant, pred = bytes(7), b''
            for i in range(0, len(filas), 7):
                f = filas[i:i + 7]
                pred += b'\x02' + bytes((x - y) & 255 for x, y in zip(f, ant))
                ant = f
            filas = pred
            dic += ' /DecodeParms << /Predictor 12 /Columns 7 >>'
        z = zlib.compress(filas, 9)
        xobj = ('%d 0 obj\n<< /Length %d /Filter /FlateDecode%s >>\nstream\n' % (xnum, len(z), dic)).encode('latin-1') + z + b'\nendstream\nendobj\n'
        s = cab + cuerpo + xobj
        return s + b'startxref\n%d\n%%%%EOF\n' % sitio[xnum][1]


def incremento(base, cambios, raiz, tam, libres=(), info=None):
    """Añade a un PDF una actualización incremental: objetos nuevos o cambiados
    ({número: cuerpo}) y objetos que pasan a estar libres."""
    prev = int(base[base.rindex(b'startxref') + 9:].split()[0])
    s, donde = base, {}
    for n in sorted(cambios):
        donde[n] = len(s)
        c = cambios[n] if isinstance(cambios[n], bytes) else cambios[n].encode('latin-1')
        s += b'%d 0 obj\n' % n + c + b'\nendobj\n'
    xref = len(s)
    s += b'xref\n'
    for n in sorted(set(donde) | set(libres)):
        s += b'%d 1\n' % n
        s += b'%010d 00000 n \n' % donde[n] if n in donde else b'0000000000 00001 f \n'
    s += b'trailer\n<< /Size %d /Root %d 0 R /Prev %d' % (tam, raiz, prev)
    if info:
        s += b' /Info %d 0 R' % info
    return s + b' >>\nstartxref\n%d\n%%%%EOF\n' % xref


def flujo_crudo(datos, dic=''):
    if isinstance(datos, str):
        datos = datos.encode('latin-1')
    return ('<< /Length %d%s >>\nstream\n' % (len(datos), dic)).encode('latin-1') + datos + b'\nendstream'


# =============================================================================
#  Los casos
# =============================================================================
def caso_carta():
    """Una carta sin comprimir: Helvetica, acentos y eñes en WinAnsi."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    c = p.flujo('BT\n/F1 12 Tf\n72 770 Td\n(Albinyana, 16 de noviembre de 2026) Tj\n'
                '0 -28 Td\n(Querida se\xf1ora Mart\xednez:) Tj\n'
                '0 -20 Td\n(\xbfC\xf3mo est\xe1 usted? \xa1Qu\xe9 alegr\xeda saber de la asociaci\xf3n!) Tj\n'
                '0 -16 Td\n(El n\xba 3 de la calle, 2\xaa planta. Ping\xfcino, cig\xfce\xf1a, \xd1and\xfa.) Tj\n'
                '0 -16 Td\n(Son 25 \x80 \\(veinticinco\\) \x96 \xabgracias\xbb\x85) Tj\n'
                '0 -16 Td\n(\xc1RBOL, \xc9XITO, \xcdNDICE, \xd3SCAR, \xdaTIL) Tj\n'
                'ET\n')
    raiz = p.paginas([c], {'F1': f1})
    info = p.obj('<< /Title (Carta de prueba) /Author (Mar\xeda Nu\xf1ez) /Producer (hazpdf.py) >>')
    y0 = A4[1] - 770
    e = [('error', 0), ('paginas', 1), ('reparado', 0), ('titulo', 'Carta de prueba'), ('autor', 'María Nuñez'),
         ('tam', 0, A4[0], A4[1]), ('sin_aviso', 0, 255), ('muchos', 0),
         ('texto', 0, 'Albinyana, 16 de noviembre de 2026', 72, y0, 12, CLARO, ancho('Albinyana, 16 de noviembre de 2026', 12)),
         ('texto', 0, 'Querida señora Martínez:', 72, y0 + 28, 12, CLARO),
         ('lineas', 0, ['Albinyana, 16 de noviembre de 2026', '', 'Querida señora Martínez:',
                        '¿Cómo está usted? ¡Qué alegría saber de la asociación!',
                        'El nº 3 de la calle, 2ª planta. Pingüino, cigüeña, Ñandú.',
                        'Son 25 EUR (veinticinco) - «gracias»...',
                        'ARBOL, ÉXITO, INDICE, OSCAR, UTIL']),
         ('halla', 'señora', 0, 1), ('halla', 'CIGUENA', 0, 0), ('halla', 'cigueña', 0, 1), ('halla', 'árbol', 0, 1)]
    return 'CARTA', p.clasico(raiz, info), e


def caso_fuentes():
    """Tres páginas comprimidas: las fuentes de siempre y todas las órdenes de texto."""
    p = Pdf()
    f = {'He': p.fuente('Helvetica'), 'HeB': p.fuente('Helvetica-Bold'), 'HeI': p.fuente('Helvetica-Oblique'),
         'Ti': p.fuente('Times-Roman'), 'TiB': p.fuente('Times-Bold'), 'TiI': p.fuente('Times-Italic'),
         'Co': p.fuente('Courier'), 'CoB': p.fuente('Courier-Bold'), 'Ar': p.fuente('ABCDEF+Verdana', mas=' /FirstChar 65 /LastChar 67 /Widths [700 710 720]')}
    c1 = p.flujo('BT /He 24 Tf 72 780 Td (Helvetica 24) Tj ET\n'
                 'BT /HeB 18 Tf 72 740 Td (Helvetica negra 18) Tj ET\n'
                 'BT /HeI 12 Tf 72 715 Td (Helvetica inclinada 12) Tj ET\n'
                 'BT /Ti 12 Tf 72 690 Td (Times 12) Tj /TiB 12 Tf 100 0 Td (Times negra) Tj /TiI 9 Tf 100 0 Td (Times cursiva 9) Tj ET\n'
                 'BT /Co 10 Tf 72 660 Td (Courier 10: 0123456789) Tj 0 -12 Td /CoB 10 Tf (Courier negra) Tj ET\n'
                 'BT /Ar 10 Tf 72 620 Td (ABC) Tj (CBA) Tj ET\n', filtros=('FlateDecode',))
    # la segunda: T*, TL, ', ", Tc, Tw, Tz, Ts, TD, Tm y TJ
    c2 = p.flujo('BT /He 10 Tf 14 TL 72 800 Td (uno) Tj T* (dos) Tj (tres) \' 2 1 (cuatro cinco) " ET\n'
                 'BT /Co 10 Tf 72 700 Td 1 Tc (abc) Tj 0 Tc ( ) Tj (d) Tj ET\n'
                 'BT /Co 10 Tf 72 680 Td 5 Tw (a b) Tj 0 Tw (c) Tj ET\n'
                 'BT /Co 10 Tf 72 660 Td 200 Tz (ab) Tj 100 Tz (c) Tj ET\n'
                 'BT /Co 10 Tf 72 640 Td (x) Tj 4 Ts /Co 7 Tf (2) Tj 0 Ts ET\n'
                 'BT /He 10 Tf 72 620 TD (TD uno) Tj 0 -15 TD (TD dos) Tj T* (TD tres) Tj ET\n'
                 'BT /He 1 Tf 20 0 0 20 72 560 Tm (Tm veinte) Tj ET\n'
                 'BT /Co 10 Tf 72 520 Td [(AB) -1000 (CD) 50 (EF) -600 (GH)] TJ ET\n'
                 'q 2 0 0 2 100 100 cm BT /Co 10 Tf 10 20 Td (doble) Tj ET Q\n'
                 'BT /Co 10 Tf 300 400 Td (fuera) Tj ET\n'
                 'BT /Co 10 Tf 72 480 Td <486F6C61> Tj <2c20 6D 756E646F2> Tj (\\101\\102\\0603 \\(x\\) \\\\ fin\\\nal) Tj ET\n',
                 filtros=('FlateDecode',))
    c3 = p.flujo('BT /Ti 12 Tf 72 780 Td (Tercera p\xe1gina) Tj ET\n', filtros=('FlateDecode',))
    raiz = p.paginas([c1, c2, c3], f)
    H = A4[1]
    e = [('error', 0), ('paginas', 3), ('sin_aviso', 0, 255), ('sin_aviso', 1, 255),
         ('texto', 0, 'Helvetica 24', 72, H - 780, 24, CLARO, ancho('Helvetica 24', 24)),
         ('texto', 0, 'Helvetica negra 18', 72, H - 740, 18, LLANO_N, ancho('Helvetica negra 18', 18, 1)),
         ('texto', 0, 'Helvetica inclinada 12', 72, H - 715, 12, CLARO),
         ('texto', 0, 'Times 12', 72, H - 690, 12, IMPRENTA, ancho('Times 12', 12, 2)),
         ('texto', 0, 'Times negra', 172, H - 690, 12, ROBLE_N, ancho('Times negra', 12, 3)),
         ('texto', 0, 'Times cursiva 9', 272, H - 690, 9, IMPRENTA),
         ('texto', 0, 'Courier 10: 0123456789', 72, H - 660, 10, MAQUINA, 6 * 22),
         ('texto', 0, 'Courier negra', 72, H - 648, 10, MAQUINA),
         ('texto', 0, 'ABCCBA', 72, H - 620, 10, LLANO, 2 * (7 + 7.1 + 7.2)),     # sus /Widths
         ('lineas', 0, ['Helvetica 24', '', 'Helvetica negra 18', '', 'Helvetica inclinada 12', '',
                        'Times 12   Times negra   Times cursiva 9', '', 'Courier 10: 0123456789', 'Courier negra',
                        '', 'ABCCBA']),
         ('texto', 1, 'uno', 72, H - 800, 10), ('texto', 1, 'dos', 72, H - 786, 10), ('texto', 1, 'tres', 72, H - 772, 10),
         ('texto', 1, 'cuatro cinco', 72, H - 758, 10, CLARO, ancho('cuatro cinco', 10) + 12 * 1 + 2),
         ('texto', 1, 'abc d', 72, H - 700, 10, MAQUINA, 3 * 7 + 8 + 6),        # con Tc=1 cada letra avanza 7 (y sigue el Tw=2 de antes)
         ('texto', 1, 'a bc', 72, H - 680, 10, MAQUINA, 18 + 5 + 6),            # Tw=5 en el espacio
         ('texto', 1, 'abc', 72, H - 660, 10, MAQUINA, 24 + 6),                 # Tz=200: «ab», el doble de ancho
         ('texto', 1, 'x', 72, H - 640, 10), ('texto', 1, '2', 78, H - 644, 7),  # subido 4 puntos
         ('texto', 1, 'TD uno', 72, H - 620, 10), ('texto', 1, 'TD dos', 72, H - 605, 10), ('texto', 1, 'TD tres', 72, H - 590, 10),
         ('texto', 1, 'Tm veinte', 72, H - 560, 20, CLARO, ancho('Tm veinte', 20)),
         ('texto', 1, 'AB', 72, H - 520, 10, MAQUINA, 12),
         ('texto', 1, 'CDEF', 72 + 12 + 10, H - 520, 10, MAQUINA, 24 - 0.5),    # -1000: un hueco de 10; 50: medio punto atrás
         ('texto', 1, 'GH', 72 + 12 + 10 + 23.5 + 6, H - 520, 10, MAQUINA, 12),
         ('texto', 1, 'doble', 120, H - 140, 20, MAQUINA, 60),                  # con «cm» al doble
         ('texto', 1, 'fuera', 300, H - 400, 10),                               # y sin él, tras «Q»
         ('texto', 1, 'Hola, mundo AB03 (x) \\ final', 72, H - 480, 10, MAQUINA),
         ('contiene', 1, 'AB'), ('halla', 'ab cdef gh', 1, 1),
         ('texto', 2, 'Tercera página', 72, H - 780, 12, IMPRENTA),
         ('halla', 'tercera', 2, 1), ('halla', 'tercera', 0, 0)]
    return 'FUENTES', p.clasico(raiz), e


def caso_factura():
    """Una factura: recuadros, rayas de tabla, colores, números a la derecha,
    subrayado, dos flujos de contenido, árbol de páginas en dos niveles con
    herencia, una imagen y una imagen metida en el contenido."""
    p = Pdf()
    he, heb = p.fuente('Helvetica'), p.fuente('Helvetica-Bold')
    img = p.flujo(b'\0\0\0\x0cjP  \r\n\x87\n' + bytes(40), ' /Type /XObject /Subtype /Image /Width 8 /Height 8 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /JPXDecode')
    filas = [('Cuota anual 2026', '1', '25,00'), ('Excursión a Tordera', '2', '61,90'), ('Lotería de Navidad', '10', '200,00')]
    s = 'q 0.8 0.95 1 rg 50 700 495 30 re f Q\n'                    # la banda del título
    s += '0.5 w 0 0 0 RG 50 580 495 100 re S\n'                    # el recuadro de la tabla
    s += '50 660 m 545 660 l S 400 580 m 400 680 l S 460 580 m 460 680 l S\n'
    s += 'BT /HeB 16 Tf 60 708 Td (FACTURA 144-2026) Tj ET\n'
    s += 'BT /HeB 10 Tf 60 666 Td (Concepto) Tj ET BT /HeB 10 Tf %.2f 666 Td (Uds.) Tj ET BT /HeB 10 Tf %.2f 666 Td (Importe) Tj ET\n' % (
        455 - ancho('Uds.', 10, 1), 540 - ancho('Importe', 10, 1))
    esp = []
    y = 646
    for con, uds, imp in filas:
        s += 'BT /He 10 Tf 60 %d Td %s Tj ET ' % (y, cad(con).decode('latin-1'))
        s += 'BT /He 10 Tf %.2f %d Td (%s) Tj ET ' % (455 - ancho(uds, 10), y, uds)
        s += 'BT /He 10 Tf %.2f %d Td (%s) Tj ET\n' % (540 - ancho(imp, 10), y, imp)
        esp.append(('texto', 0, con, 60, A4[1] - y, 10, CLARO))
        esp.append(('texto', 0, imp, 540 - ancho(imp, 10), A4[1] - y, 10, CLARO, ancho(imp, 10)))
        y -= 18
    c1 = p.flujo(s, filtros=('FlateDecode',))
    s2 = 'BT /HeB 12 Tf 1 0 0 rg %.2f 560 Td (Total: 286,90) Tj ET\n' % (540 - ancho('Total: 286,90', 12, 1))
    s2 += '1 0 0 RG 1.5 w %.2f 557 m 540 557 l S\n' % (540 - ancho('Total: 286,90', 12, 1))  # el subrayado
    s2 += '0 0.5 0 rg 0 0 1 RG 2 w 50 500 60 30 re B\n'             # relleno y borde
    s2 += '0.2 0.2 0.2 0.1 k 120 500 60 30 re f 0.5 g 190 500 60 30 re f 0 0 0 1 K 0 G\n'
    s2 += 'q 100 0 0 80 400 450 cm /Im1 Do Q\n'
    s2 += 'q 40 0 0 40 300 450 cm BI /W 2 /H 2 /CS /G /BPC 8 ID \x00\xff EI\xff\x00 EI Q\n'
    s2 += 'BT /He 8 Tf 0 g 50 440 Td (Pagado por transferencia.) Tj ET\n'
    c2 = p.flujo(s2.encode('latin-1'))
    # el árbol: raíz -> (rama A -> página 1), (página 2, con su propia caja)
    raiz, rama = p.nuevo(), p.nuevo()
    pg1 = p.obj('<< /Type /Page /Parent %d 0 R /Contents [%d 0 R %d 0 R] >>' % (rama, c1, c2))
    c3 = p.flujo('BT /He 10 Tf 20 180 Td (Apaisada) Tj ET')
    pg2 = p.obj('<< /Type /Page /Parent %d 0 R /Contents %d 0 R /MediaBox [0 0 300 200] >>' % (raiz, c3))
    rec = p.obj('<< /Font << /He %d 0 R /HeB %d 0 R >> /XObject << /Im1 %d 0 R >> >>' % (he, heb, img))
    p.pon(rama, '<< /Type /Pages /Parent %d 0 R /Kids [%d 0 R] /Count 1 >>' % (raiz, pg1))
    caja = p.obj('[0 0 %s %s]' % A4)
    p.pon(raiz, '<< /Type /Pages /Kids [%d 0 R %d 0 R] /Count 2 /MediaBox %d 0 R /Resources %d 0 R >>' % (rama, pg2, caja, rec))
    cat = p.obj('<< /Type /Catalog /Pages %d 0 R >>' % raiz)
    H = A4[1]
    xt = 540 - ancho('Total: 286,90', 12, 1)
    e = [('error', 0), ('paginas', 2), ('tam', 0, A4[0], A4[1]), ('tam', 1, 300, 200),
         ('aviso', 0, AV_IMAGEN), ('sin_aviso', 0, 255 - AV_IMAGEN), ('sin_aviso', 1, 255), ('que', 0, 'JPEG 2000'), ('que', 1, ''),
         ('rect', 0, 50, H - 730, 495, 30, 16 + 36 * 4 + 6 * 5 + 5),     # 0,8 0,95 1 -> niveles 4, 5 y 5
         ('raya', 0, 50, H - 580, 545, H - 580, 0.5, NEGRO), ('raya', 0, 545, H - 580, 545, H - 680),
         ('raya', 0, 545, H - 680, 50, H - 680), ('raya', 0, 50, H - 680, 50, H - 580),
         ('raya', 0, 50, H - 660, 545, H - 660), ('raya', 0, 400, H - 580, 400, H - 680), ('raya', 0, 460, H - 580, 460, H - 680),
         ('texto', 0, 'FACTURA 144-2026', 60, H - 708, 16, LLANO_N),
         ('texto', 0, 'Importe', 540 - ancho('Importe', 10, 1), H - 666, 10, LLANO_N, ancho('Importe', 10, 1)),
         ('texto', 0, 'Total: 286,90', xt, H - 560, 12, LLANO_N),
         ('raya', 0, xt, H - 557, 540, H - 557, 1.5, 16 + 36 * 5),
         ('rect', 0, 50, H - 530, 60, 30, 16 + 6 * 3),                   # verde 0,5 -> nivel 3 (2,5 redondeado)
         ('raya', 0, 50, H - 500, 110, H - 500, 2, 16 + 5),
         ('rect', 0, 120, H - 530, 60, 30, 232 + 17),                    # CMYK 0,2 0,2 0,2 0,1 -> gris 0,72
         ('rect', 0, 190, H - 530, 60, 30, 232 + 12),                    # gris 0,5
         ('imagen', 0, 400, H - 530, 100, 80), ('imagen', 0, 300, H - 490, 40, 40),
         ('texto', 0, 'Pagado por transferencia.', 50, H - 440, 8, CLARO),
         ('no_texto', 0, 'EI'), ('ncosas', 0, 'I', 2),
         ('lineas', 0, ['FACTURA 144-2026', '', 'Concepto   Uds.   Importe', '', 'Cuota anual 2026   1   25,00',
                        'Excursión a Tordera   2   61,90', 'Lotería de Navidad   10   200,00', '', 'Total: 286,90', '',
                        'Pagado por transferencia.']),
         ('texto', 1, 'Apaisada', 20, 20, 10), ('halla', '61,90', 0, 1)] + esp
    return 'FACTURA', p.clasico(cat), e


def caso_moderno(nombre='MODERNO', predictor=True):
    """PDF 1.5: índice en flujo (con predictor de PNG) y objetos en flujos de objetos."""
    p = Pdf('1.5')
    f1 = p.fuente('Times-Roman')
    cs = [p.flujo('BT /F1 14 Tf 72 760 Td (P\xe1gina %d del PDF 1.5) Tj ET\n' % (i + 1), filtros=('FlateDecode',)) for i in range(5)]
    raiz = p.paginas(cs, {'F1': f1})
    info = p.obj('<< /Title <FEFF0054006900740075006C006F00200063006F006E002000F1002000790020201C0063006F006D0069006C006C00610073201D> /Author (An\xf3nimo) >>')
    e = [('error', 0), ('paginas', 5), ('reparado', 0), ('titulo', 'Titulo con ñ y "comillas"'), ('autor', 'Anónimo')]
    e += [('texto', i, 'Página %d del PDF 1.5' % (i + 1), 72, A4[1] - 760, 14, IMPRENTA) for i in range(5)]
    return nombre, p.moderno(raiz, info, en_flujo=set(p.o), predictor=predictor, por_flujo=4), e


def caso_incremental():
    """Un PDF al que se le ha cambiado después una página y borrado un objeto."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    c1 = p.flujo('BT /F1 12 Tf 72 760 Td (Texto original de la primera) Tj ET')
    c2 = p.flujo('BT /F1 12 Tf 72 760 Td (Segunda sin cambios) Tj ET')
    sobra = p.obj('(un objeto que luego se borra)')
    raiz = p.paginas([c1, c2], {'F1': f1})
    info = p.obj('<< /Title (Antes) >>')
    base = p.clasico(raiz, info)
    nuevo = p.n + 1
    s = incremento(base, {c1: flujo_crudo('BT /F1 12 Tf 72 760 Td (Texto corregido) Tj ET'),
                          nuevo: '<< /Title (Despu\xe9s) >>'}, raiz, nuevo + 1, libres=[sobra], info=nuevo)
    # y otra más encima: la tercera versión
    s = incremento(s, {c2: flujo_crudo('BT /F1 12 Tf 72 700 Td (Segunda, cambiada al final) Tj ET')}, raiz, nuevo + 1, info=nuevo)
    e = [('error', 0), ('paginas', 2), ('reparado', 0), ('titulo', 'Después'),
         ('texto', 0, 'Texto corregido', 72, A4[1] - 760, 12), ('no_texto', 0, 'original'),
         ('texto', 1, 'Segunda, cambiada al final', 72, A4[1] - 700, 12), ('no_texto', 1, 'sin cambios')]
    return 'INCREMEN', s, e


def caso_filtros():
    """Los otros filtros: ASCIIHex, ASCII85, RunLength, encadenados; /Length
    por referencia, equivocado y sin él; parámetros en lista."""
    p = Pdf()
    f1 = p.fuente('Courier')
    txt = lambda i, t: 'BT /F1 10 Tf 72 %d Td (%s) Tj ET\n' % (780 - 20 * i, t)
    cs = [p.flujo(txt(0, 'en hexadecimal'), filtros=('ASCIIHexDecode',)),
          p.flujo(txt(1, 'en ASCII85'), filtros=('ASCII85Decode',)),
          p.flujo(txt(2, 'con RunLength: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'), filtros=('RunLengthDecode',)),
          p.flujo(txt(3, 'ASCII85 y Flate'), filtros=('ASCII85Decode', 'FlateDecode')),
          p.flujo(txt(4, 'hexadecimal, RunLength y Flate'), filtros=('ASCIIHexDecode', 'RunLengthDecode', 'FlateDecode'))]
    # el largo, en otro objeto (que va detrás)
    n, nl = p.nuevo(), p.nuevo()
    d = txt(5, 'largo por referencia').encode()
    p.flujo(d, num=n, largo='%d 0 R' % nl)
    p.pon(nl, '%d' % len(d))
    cs.append(n)
    cs.append(p.flujo(txt(6, 'largo equivocado'), largo=7))
    cs.append(p.pon(p.nuevo(), b'<< >>\nstream\r\n' + txt(7, 'sin largo').encode() + b'\r\nendstream'))
    # Flate con predictor de PNG (todas las clases de fila) y de TIFF, con los parámetros en lista
    d = txt(8, 'con predictor de PNG').encode().ljust(72)
    filas = [d[i:i + 12] for i in range(0, 72, 12)]
    pred, ant = b'', bytes(12)
    for i, f in enumerate(filas):
        t = i % 5
        if t == 0:
            pred += b'\x00' + f
        elif t == 1:
            pred += b'\x01' + bytes((f[k] - (f[k - 1] if k else 0)) & 255 for k in range(12))
        elif t == 2:
            pred += b'\x02' + bytes((f[k] - ant[k]) & 255 for k in range(12))
        elif t == 3:
            pred += b'\x03' + bytes((f[k] - ((f[k - 1] if k else 0) + ant[k]) // 2) & 255 for k in range(12))
        else:
            def paeth(a, b, c):
                pp = a + b - c
                pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                return a if pa <= pb and pa <= pc else (b if pb <= pc else c)
            pred += b'\x04' + bytes((f[k] - paeth(f[k - 1] if k else 0, ant[k], ant[k - 1] if k else 0)) & 255 for k in range(12))
        ant = f
    z = zlib.compress(pred)
    cs.append(p.pon(p.nuevo(), ('<< /Length %d /Filter [/FlateDecode] /DecodeParms [<< /Predictor 15 /Columns 12 >>] >>\nstream\n' % len(z)).encode() + z + b'\nendstream'))
    d = txt(9, 'con predictor de TIFF').encode().ljust(64)
    tiff = b''.join(bytes((d[i + k] - (d[i + k - 1] if k else 0)) & 255 for k in range(16)) for i in range(0, 64, 16))
    z = zlib.compress(tiff)
    cs.append(p.pon(p.nuevo(), ('<< /Length %d /Filter /FlateDecode /DecodeParms << /Predictor 2 /Columns 16 >> >>\nstream\n' % len(z)).encode() + z + b'\nendstream'))
    # LZW: largo, para que los códigos pasen de 9 a 12 bits y la tabla se vacíe; y con /EarlyChange 0
    import random
    rnd = random.Random(7)
    relleno = ''.join('%% %s\n' % ' '.join(rnd.choice(['uno', 'dos', 'tres', 'catorce', 'x', 'yz', str(rnd.randint(0, 99999))]) for _ in range(12)) for _ in range(900))
    cs.append(p.flujo(relleno + txt(10, 'con LZW'), filtros=('LZWDecode',)))
    z = lzw((relleno + txt(11, 'con LZW sin adelanto')).encode(), 0)
    cs.append(p.pon(p.nuevo(), ('<< /Length %d /Filter /LZWDecode /DecodeParms << /EarlyChange 0 >> >>\nstream\n' % len(z)).encode() + z + b'\nendstream'))
    cs.append(p.flujo(txt(12, 'ASCII85 y LZW'), filtros=('ASCII85Decode', 'LZWDecode')))
    raiz = p.paginas([cs], {'F1': f1})
    nombres = ['en hexadecimal', 'en ASCII85', 'con RunLength: ' + 'a' * 48, 'ASCII85 y Flate', 'hexadecimal, RunLength y Flate',
               'largo por referencia', 'largo equivocado', 'sin largo', 'con predictor de PNG', 'con predictor de TIFF',
               'con LZW', 'con LZW sin adelanto', 'ASCII85 y LZW']
    e = [('error', 0), ('paginas', 1), ('sin_aviso', 0, 255), ('lineas', 0, [x for n in nombres for x in (n, '')][:-1])]
    e += [('texto', 0, t, 72, A4[1] - 780 + 20 * i, 10, MAQUINA) for i, t in enumerate(nombres)]
    return 'FILTROS', p.clasico(raiz), e


def caso_codif():
    """Codificaciones: MacRoman, Standard, /Differences y /ToUnicode; una
    fuente compuesta (Type0) con /W; y una de TeX sin /Encoding."""
    p = Pdf()
    mac = p.fuente('Helvetica', '/MacRomanEncoding')
    std = p.fuente('Times-Roman', '')
    dif = p.fuente('Helvetica', '<< /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences [1 /aacute /eacute /ntilde 65 /Ntilde /questiondown /uni00FA /Euro /fi /g17 /.notdef /a.sc] >>')
    # códigos propios (1, 2, 3...) con /ToUnicode: lo que hacen muchos programas
    cmap = ('/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n1 begincodespacerange <00> <FF> endcodespacerange\n'
            '4 beginbfchar\n<01> <0048>\n<02> <006F>\n<03> <006C>\n<04> <00E1>\nendbfchar\n'
            '2 beginbfrange\n<10> <12> <0041>\n<20> <22> [<00F1> <FB01> <0066006C>]\nendbfrange\nendcmap end end\n')
    tu = p.flujo(cmap, filtros=('FlateDecode',))
    desc = p.obj('<< /Type /FontDescriptor /FontName /AAAAAA+Rara /Flags 4 /FontBBox [0 0 1000 1000] /ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 >>')
    pro = p.obj('<< /Type /Font /Subtype /TrueType /BaseFont /AAAAAA+Rara-Bold /FirstChar 1 /LastChar 34 /Widths [600 500 250 500 %s 700 700 700 %s 600 600 600] /FontDescriptor %d 0 R /ToUnicode %d 0 R >>' % ('0 ' * 11, '0 ' * 13, desc, tu))
    # compuesta: códigos de dos octetos = números de dibujo
    cmap0 = ('/CIDInit /ProcSet findresource begin 12 dict begin begincmap\n1 begincodespacerange <0000> <FFFF> endcodespacerange\n'
             '1 beginbfrange\n<0000> <FFFF> <0000>\nendbfrange\n'        # (como mPDF: luego se pisa lo que haga falta)
             '3 beginbfchar\n<0003> <0020>\n<0100> <00D1>\n<0101> <20AC>\nendbfchar\n'
             '1 beginbfrange\n<0024> <003D> <0041>\nendbfrange\n1 beginbfrange\n<0044> <005D> <0061>\nendbfrange\nendcmap end end\n')
    tu0 = p.flujo(cmap0)
    desc0 = p.obj('<< /Type /FontDescriptor /FontName /BBBBBB+DejaVuSerif /Flags 6 /FontBBox [0 0 1000 1000] /ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 >>')
    cid = p.obj('<< /Type /Font /Subtype /CIDFontType2 /BaseFont /BBBBBB+DejaVuSerif /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> '
                '/FontDescriptor %d 0 R /DW 500 /W [3 [300] 36 38 700 68 [600 610 620] 256 257 800] >>' % desc0)
    t0 = p.obj('<< /Type /Font /Subtype /Type0 /BaseFont /BBBBBB+DejaVuSerif /Encoding /Identity-H /DescendantFonts [%d 0 R] /ToUnicode %d 0 R >>' % (cid, tu0))
    tex = p.obj('<< /Type /Font /Subtype /Type1 /BaseFont /CCCCCC+CMR10 /FirstChar 0 /LastChar 127 /FontDescriptor %d 0 R >>' % desc)
    s = ('BT /Mac 12 Tf 72 780 Td (a\x87 e\x8e n\x96 \xc0Qu\x8e? \xd2s\x92\xd3 \xc7ya\xc8) Tj ET\n'       # á é ñ ¿Qué? “sí” «ya»
         'BT /Std 12 Tf 72 760 Td (`comillas\' \xa1ol\xe9! \xaeel \xd0 \xfb) Tj ET\n'                       # ‘comillas’ ¡ol(Ø)! fi-el — ß
         'BT /Dif 12 Tf 72 740 Td (c\001f\002 \003u AB\003C DE F G) Tj ET\n'                                # cáfé ñu ÑB... ¿ ú € fi ?
         'BT /Pro 12 Tf 72 720 Td <0102030400> Tj <101112> Tj < 20 21 22 > Tj ET\n'                         # Holá + ABC + ñ fi fl
         'BT /T0 10 Tf 72 700 Td <002400250026000300440045004601000101> Tj <0FFF> Tj <FB01> Tj ET\n'         # ABC abcÑ€ + uno sin letra + «fi» (U+FB01)
         'BT /Tex 10 Tf 72 680 Td (Texto de TeX: e\\014caz, \\013ores y o\\015cina \\\\as\\042) Tj ET\n')    # ligaduras fi, ff, fl y comillas“”
    c = p.flujo(s.encode('latin-1'))
    raiz = p.paginas([c], {'Mac': mac, 'Std': std, 'Dif': dif, 'Pro': pro, 'T0': t0, 'Tex': tex})
    H = A4[1]
    e = [('error', 0), ('paginas', 1), ('aviso', 0, AV_DESCON), ('sin_aviso', 0, AV_ILEGIBLE),
         ('texto', 0, 'aá eé nñ ¿Qué? "sí" «ya»', 72, H - 780, 12, CLARO),
         ('texto', 0, "'comillas' ¡olO! fiel - ß", 72, H - 760, 12, IMPRENTA),
         ('texto', 0, 'cáfé ñu Ñ¿ñú EURfi ? G', 72, H - 740, 12, CLARO),
         ('texto', 0, 'HoláABCñfifl', 72, H - 720, 12, LLANO_N, (600 + 500 + 250 + 500 + 2100 + 1800) * 0.012),
         ('texto', 0, 'ABC abcÑEUR?fi', 72, H - 700, 10, ROBLE, (700 * 3 + 300 + 600 + 610 + 620 + 800 * 2 + 500 * 2) / 100),
         ('texto', 0, 'Texto de TeX: eficaz, ffores y oflcina "as"', 72, H - 680, 10, IMPRENTA)]
    return 'CODIF', p.clasico(raiz), e


def caso_cifrado():
    p = Pdf()
    f1 = p.fuente('Helvetica')
    c = p.flujo(bytes(range(256)))
    raiz = p.paginas([c], {'F1': f1})
    enc = p.obj('<< /Filter /Standard /V 1 /R 2 /O (%s) /U (%s) /P -44 >>' % ('o' * 32, 'u' * 32))
    p.trailer = b' /Encrypt %d 0 R /ID [<00112233445566778899AABBCCDDEEFF> <00112233445566778899AABBCCDDEEFF>]' % enc
    return 'CIFRADO', p.clasico(raiz), [('error', 3)]


def caso_cifrado15():
    """Cifrado y con el índice en flujo."""
    p = Pdf('1.6')
    f1 = p.fuente('Helvetica')
    c = p.flujo(bytes(range(256)))
    raiz = p.paginas([c], {'F1': f1})
    enc = p.obj('<< /Filter /Standard /V 4 /R 4 /Length 128 /O (%s) /U (%s) /P -44 >>' % ('o' * 32, 'u' * 32))
    p.trailer = b' /Encrypt %d 0 R /ID [<00> <00>]' % enc
    return 'CIFRA15', p.moderno(raiz, en_flujo=set()), [('error', 3)]


def caso_no_sabe():
    """Lo que Lector no sabe: un filtro desconocido en el contenido (página
    1), una letra con codificación propia (página 2), letras que no existen
    en la 437 (página 3) y, en la 4, un contenido estropeado."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    c1 = p.pon(p.nuevo(), b'<< /Length 20 /Filter /JBIG2Decode >>\nstream\n' + bytes(range(100, 120)) + b'\nendstream')
    desc = p.obj('<< /Type /FontDescriptor /FontName /AAAAAA+Propia /Flags 4 /FontBBox [0 0 1000 1000] /ItalicAngle 0 /Ascent 800 /Descent -200 /CapHeight 700 /StemV 80 /FontFile2 %d 0 R >>' % p.flujo(bytes(64), ' /Length1 64'))
    pro = p.obj('<< /Type /Font /Subtype /TrueType /BaseFont /AAAAAA+Propia /FirstChar 1 /LastChar 5 /Widths [500 500 500 500 500] /FontDescriptor %d 0 R >>' % desc)
    cid = p.obj('<< /Type /Font /Subtype /CIDFontType2 /BaseFont /BBBBBB+Otra /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> /FontDescriptor %d 0 R /DW 1000 >>' % desc)
    t0 = p.obj('<< /Type /Font /Subtype /Type0 /BaseFont /BBBBBB+Otra /Encoding /Identity-H /DescendantFonts [%d 0 R] >>' % cid)
    sym = p.fuente('Symbol', '')
    c2 = p.flujo('BT /F1 12 Tf 72 780 Td (Se lee) Tj ET\nBT /Pro 12 Tf 72 760 Td <0102030405> Tj (\\001\\002) Tj ET\n'
                 'BT /T0 10 Tf 72 740 Td <00010002> Tj ET\nBT /Sym 10 Tf 72 720 Td (abg) Tj ET\n'
                 'BT /Falta 10 Tf 72 700 Td (sin fuente) Tj ET\nBT /F1 12 Tf 72 680 Td (Tambi\xe9n se lee) Tj ET\n')
    gr = p.fuente('Helvetica', '<< /Type /Encoding /Differences [65 /uni0416 /uni4E2D /alpha /uni0431] >>')
    c3 = p.flujo('BT /Gr 12 Tf 72 780 Td (ABCD normal) Tj ET')
    mal = zlib.compress(b'BT /F1 12 Tf 72 780 Td (Hasta aqui llega) Tj ET\n' + bytes(i * 7 % 251 for i in range(3000)))
    c4 = p.pon(p.nuevo(), b'<< /Length %d /Filter /FlateDecode >>\nstream\n' % (len(mal) - 300) + mal[:-300] + b'\nendstream')
    c5 = p.pon(p.nuevo(), b'<< /Length 30 /Filter /FlateDecode >>\nstream\n' + bytes(range(30)) + b'\nendstream')
    raiz = p.paginas([c1, c2, c3, c4, c5], {'F1': f1, 'Pro': pro, 'T0': t0, 'Sym': sym, 'Gr': gr})
    H = A4[1]
    e = [('error', 0), ('paginas', 5),
         ('aviso', 0, AV_FILTRO), ('ncosas', 0, 'T', 0),
         ('aviso', 1, AV_ILEGIBLE), ('sin_aviso', 1, AV_FILTRO | AV_ROTO),
         ('texto', 1, 'Se lee', 72, H - 780, 12), ('texto', 1, 'También se lee', 72, H - 680, 12),
         ('ncosas', 1, 'T', 2), ('ilegible', 1, 72, H - 760), ('ilegible', 1, 72, H - 740), ('ilegible', 1, 72, H - 720),
         ('ncosas', 1, 'X', 3), ('lineas', 1, ['Se lee', '', '', '', '', 'También se lee'][0:1] + ['', 'También se lee']),
         ('aviso', 2, AV_DESCON), ('texto', 2, '??α? normal', 72, H - 780, 12),
         ('aviso', 3, AV_ROTO), ('texto', 3, 'Hasta aqui llega', 72, H - 780, 12),
         ('aviso', 4, AV_ROTO), ('ncosas', 4, 'T', 0)]
    return 'NOSABE', p.clasico(raiz), e


def caso_graficos():
    """Curvas, figuras rellenas, formularios y páginas giradas."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    s = ('0 0 1 rg 100 700 m 200 700 l 150 780 l h f\n'                          # un triángulo relleno
         '1 0 0 RG 300 700 m 300 760 360 760 360 700 c S\n'                      # una curva
         '300 600 m 330 660 360 600 v 390 540 420 600 y S\n'                     # v, y
         '0 1 0 rg 100 500 m 200 500 l 200 560 l 100 560 l f\n'                  # un rectángulo hecho con rayas
         '0.5 0.5 0 rg 300 500 50 40 re 400 500 50 40 re f*\n'                   # dos rectángulos de una vez
         '0 g 100 400 m 180 400 l 140 460 l b\n'                                 # cerrar, rellenar y trazar
         '50 300 m 60 310 l n q 1 1 1 rg 0 0 10 10 re W n Q\n'                   # nada: se descarta (y un recorte que se deshace)
         'q 1 0 0 1 50 100 cm /Fm1 Do Q q 0.5 0 0 0.5 300 100 cm /Fm1 Do Q\n'    # el mismo formulario, dos veces
         '0 g BT /F1 10 Tf 0 1 -1 0 500 300 Tm (Hacia arriba) Tj ET\n'           # texto girado
         'BT /F1 10 Tf -1 0 0 -1 500 200 Tm (Cabeza abajo) Tj ET\n')
    c = p.flujo(s)
    ff = p.fuente('Courier')
    fm = p.flujo('0.9 g 0 0 100 50 re f 0 G 0 0 100 50 re S 0 g BT /FF 10 Tf 10 20 Td (En el formulario) Tj ET q 1 0 0 1 0 60 cm /Fm1 Do Q',
                 ' /Type /XObject /Subtype /Form /BBox [0 0 100 50] /Matrix [2 0 0 2 0 0] /Resources << /Font << /FF %d 0 R >> >>' % ff)
    c2 = p.flujo('BT /F1 12 Tf 30 50 Td (Girada un cuarto) Tj ET 10 10 100 20 re f')
    c3 = p.flujo('BT /F1 12 Tf 30 50 Td (Girada media vuelta) Tj ET')
    c4 = p.flujo('BT /F1 12 Tf 30 50 Td (Girada tres cuartos) Tj ET')
    c5 = p.flujo('BT /F1 12 Tf 130 150 Td (Caja con origen) Tj ET')
    raiz = p.paginas([c, c2, c3, c4, c5], {'F1': f1}, mas_recursos=' /XObject << /Fm1 %d 0 R >>' % fm,
                     mas_pagina={1: ' /MediaBox [0 0 400 200] /Rotate 90', 2: ' /MediaBox [0 0 400 200] /Rotate 180',
                                 3: ' /MediaBox [0 0 400 200] /Rotate -90', 4: ' /MediaBox [100 100 400 300] /CropBox [110 120 400 300]'})
    H = A4[1]
    e = [('error', 0), ('paginas', 5), ('sin_aviso', 0, 255),
         ('figura', 0, 4), ('ncosas', 0, 'G', 2),
         ('raya', 0, 360 - 60 * 0.669921875 + 60 * 0.669921875 - 60 + 60, H - 700, 360, H - 700, None, 16 + 36 * 5)]
    e = e[:-1]
    e += [('ncosas', 0, 'L', 8 + 16 + 3 + 2 * 4),
          ('rect', 0, 100, H - 560, 100, 60, 16 + 6 * 5), ('rect', 0, 300, H - 540, 50, 40), ('rect', 0, 400, H - 540, 50, 40),
          ('raya', 0, 140, H - 460, 100, H - 400),
          # el formulario: su matriz lo dobla; dentro, se llama a sí mismo (se corta a los ocho niveles)
          ('rect', 0, 50, H - 200, 200, 100, 232 + 21), ('texto', 0, 'En el formulario', 70, H - 140, 20, MAQUINA),
          ('rect', 0, 300, H - 150, 100, 50, 232 + 21), ('texto', 0, 'En el formulario', 310, H - 120, 10, MAQUINA),
          ('texto', 0, 'Hacia arriba', 500, H - 300, 10), ('texto', 0, 'Cabeza abajo', 500, H - 200, 10),
          ('tam', 1, 200, 400), ('texto', 1, 'Girada un cuarto', 50, 30, 12), ('rect', 1, 10, 10, 20, 100),
          ('tam', 2, 400, 200), ('texto', 2, 'Girada media vuelta', 370, 50, 12),
          ('tam', 3, 200, 400), ('texto', 3, 'Girada tres cuartos', 150, 370, 12),
          ('tam', 4, 290, 180), ('texto', 4, 'Caja con origen', 20, 150, 12)]
    return 'GRAFICOS', p.clasico(raiz), e


def caso_largo(n=40):
    """Un documento de muchas páginas (comprimido y en PDF 1.5), para pasar de
    una a otra y buscar; cada página lleva su número y una palabra propia."""
    p = Pdf('1.5')
    f1, f2 = p.fuente('Times-Roman'), p.fuente('Times-Bold')
    cs = []
    for i in range(n):
        s = 'BT /F2 16 Tf 72 780 Td (Cap\xedtulo %d) Tj ET\n' % (i + 1)
        y = 750
        for k in range(30):
            s += 'BT /F1 11 Tf 72 %d Td (L\xednea %d de la p\xe1gina %d: en un lugar de la Mancha, de cuyo nombre no quiero acordarme.) Tj ET\n' % (y, k + 1, i + 1)
            y -= 14
        if i == 26:
            s += 'BT /F1 11 Tf 72 300 Td (Aqu\xed est\xe1 el tesoro escondido.) Tj ET\n'
        if i in (3, 30):
            s += 'BT /F1 11 Tf 72 300 Td (Una pista del tesoro.) Tj ET\n'
        cs.append(p.flujo(s, filtros=('FlateDecode',)))
    raiz = p.paginas(cs, {'F1': f1, 'F2': f2})
    info = p.obj('<< /Title (El libro largo) /Author (Miguel) >>')
    e = [('error', 0), ('paginas', n), ('titulo', 'El libro largo'),
         ('texto', 0, 'Capítulo 1', 72, A4[1] - 780, 16, ROBLE_N), ('texto', n - 1, 'Capítulo %d' % n, 72, A4[1] - 780, 16, ROBLE_N),
         ('halla', 'tesoro', 26, 1), ('halla', 'tesoro', 3, 1), ('halla', 'tesoro', 0, 0), ('halla', 'mancha', 5, 30)]
    return 'LARGO', p.moderno(raiz, info, en_flujo=set(p.o), por_flujo=30), e


def caso_espacios():
    """Texto hecho letra a letra y con huecos en vez de espacios (como TeX):
    tiene que salir junto y con sus espacios, para buscar y guardar."""
    p = Pdf()
    f1 = p.fuente('Times-Roman')
    s = 'BT /F1 10 Tf 72 780 Td '
    for ch in 'Letra a letra':                                         # cada letra, con su Td
        s += '(%s) Tj %.3f 0 Td ' % (ch, ancho(ch, 10, 2))
    s += 'ET\nBT /F1 10 Tf 72 760 Td [(Con)-333(huecos)-333(en)-333(vez)-333(de)-333(espacios)] TJ ET\n'
    s += 'BT /F1 10 Tf 72 740 Td [(A)120(V)80(A)-20(TAR)] TJ ( y sigue) Tj ET\n'   # ajustes pequeños: una palabra
    s += 'BT /F1 10 Tf 300 720 Td (derecha) Tj ET BT /F1 10 Tf 72 720 Td (izquierda) Tj ET\n'   # al revés
    s += 'BT /F1 10 Tf 72 700 Td (H) Tj /F1 7 Tf 0 -2 Td ( ) Tj ET BT /F1 7 Tf 79.2 698 Td (2) Tj /F1 10 Tf 3.5 2 Td (O) Tj ET\n'   # un subíndice
    c = p.flujo(s)
    raiz = p.paginas([c], {'F1': f1})
    e = [('error', 0), ('texto', 0, 'Letra a letra', 72, A4[1] - 780, 10, IMPRENTA, ancho('Letra a letra', 10, 2)),
         ('lineas', 0, ['Letra a letra', '', 'Con huecos en vez de espacios', '', 'AVATAR y sigue', '', 'izquierda   derecha', '', 'H2O']),
         ('halla', 'en vez de', 0, 1), ('halla', 'avatar', 0, 1)]
    return 'ESPACIOS', p.clasico(raiz), e


# ---- los rotos: a partir de uno bueno ----
def rotos():
    _, carta, _ = caso_carta()
    _, fue, e_fue = caso_fuentes()
    _, mod, e_mod = caso_moderno()
    H = A4[1]
    lee_carta = [('paginas', 1), ('texto', 0, 'Querida señora Martínez:', 72, H - 770 + 28, 12)]
    casos = []
    # el «startxref» apunta a otro sitio
    i = carta.rindex(b'startxref')
    casos.append(('MALXREF', carta[:i] + b'startxref\n12345\n%%EOF\n', [('error', 0), ('reparado', 1)] + lee_carta + [('titulo', 'Carta de prueba')]))
    # sin índice ni «trailer»: cortado justo antes
    casos.append(('SINXREF', carta[:carta.rindex(b'xref\n0 ')], [('error', 0), ('reparado', 1)] + lee_carta))
    # el índice, con todas las direcciones corridas (un archivo al que le han cambiado los fines de línea)
    j = carta.index(b'%\xe2')
    casos.append(('CORRIDO', carta[:j] + b'% una linea de mas\n' + carta[j:], [('error', 0), ('reparado', 1)] + lee_carta))
    # con basura delante de la cabecera y detrás del final
    casos.append(('BASURA', b'\r\n\r\nbasura de un correo\r\n' + carta + b'\r\n--frontera--\r\n' * 40, [('error', 0)] + lee_carta))
    # cortado a la mitad del contenido de la tercera página (los contenidos van al final,
    # comprimidos): se leen las dos primeras y lo que se salve de la tercera
    p = Pdf()
    f1 = p.fuente('Helvetica')
    cs = [p.flujo('BT /F1 12 Tf 72 780 Td (P\xe1gina %d, entera) Tj ET\n' % (i + 1) + ''.join(
        'BT /F1 9 Tf 72 %d Td (relleno %d %d) Tj ET\n' % (700 - 10 * k, i, k * 7919 % 1000) for k in range(60)), filtros=('FlateDecode',)) for i in range(3)]
    raiz = p.paginas(cs, {'F1': f1})
    d = p.clasico(raiz, orden=[n for n in sorted(p.o) if n not in cs] + cs)
    k = d.rindex(b'endstream') - 150
    casos.append(('CORTADO', d[:k], [('error', 0), ('reparado', 1), ('paginas', 3), ('texto', 0, 'Página 1, entera', 72, H - 780, 12),
                                     ('sin_aviso', 1, AV_ROTO), ('texto', 1, 'Página 2, entera', 72, H - 780, 12),
                                     ('aviso', 2, AV_ROTO), ('texto', 2, 'Página 3, entera', 72, H - 780, 12), ('contiene', 2, 'relleno 2 0')]))
    # cortado en mitad de la tabla xref: están todos los objetos
    casos.append(('CORTADO2', fue[:fue.rindex(b'xref\n0 ') + 100], [('error', 0), ('reparado', 1), ('paginas', 3), ('contiene', 0, 'Helvetica 24')]))
    # PDF 1.5 sin el final (ni índice en flujo ni «startxref»): se rehace con los flujos de objetos
    casos.append(('CORTA15', mod[:mod.rindex(b' 0 obj\n<< /Length', 0, mod.rindex(b'startxref'))],
                  [('error', 0), ('reparado', 1), ('paginas', 5), ('texto', 2, 'Página 3 del PDF 1.5', 72, H - 760, 14)]))
    # cosas que no son un PDF
    casos.append(('NOPDF', b'Esto es un archivo de texto corriente.\r\n' * 30, [('error', 1)]))
    casos.append(('VACIO', b'', [('error', 1)]))
    casos.append(('SOLOCAB', b'%PDF-1.4\n', [('error', 2)]))
    casos.append(('SINPAG', b'%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntrailer\n<< /Root 1 0 R >>\n', [('error', 5)]))
    # un árbol de páginas que se muerde la cola, y referencias que dan vueltas
    casos.append(('BUCLE', b'%PDF-1.4\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n2 0 obj\n<< /Type /Pages /Kids [2 0 R 3 0 R 2 0 R] /Count 3 >>\nendobj\n'
                  b'3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 5 0 R 100] /Contents 4 0 R /Resources 4 0 R >>\nendobj\n'
                  b'4 0 obj\n4 0 R\nendobj\n5 0 obj\n6 0 R\nendobj\n6 0 obj\n5 0 R\nendobj\ntrailer\n<< /Root 1 0 R /Size 7 >>\n',
                  [('error', 0), ('reparado', 1), ('paginas', 1), ('tam', 0, 612, 792), ('ncosas', 0, 'T', 0)]))
    return casos


def caso_imagenes():
    """Imágenes: RGB, grises de 2 bits, 1 bit al revés, Indexed, CMYK, máscara,
    una grande (se reduce), una dentro del contenido, una dada la vuelta y una
    JPEG que no lo es (recuadro y aviso)."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    im = lambda datos, dic, filtros=(): p.flujo(datos, ' /Type /XObject /Subtype /Image' + dic, filtros)
    x = {'Rgb': im(bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255]), ' /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8', ('FlateDecode',)),
         'G2': im(bytes([0b00011011]), ' /Width 4 /Height 1 /ColorSpace /DeviceGray /BitsPerComponent 2'),
         'B1': im(bytes([0b10101010]), ' /Width 8 /Height 1 /ColorSpace /DeviceGray /BitsPerComponent 1 /Decode [1 0]', ('ASCIIHexDecode',)),
         'Ind': im(bytes([0, 1, 2, 3]), ' /Width 2 /Height 2 /ColorSpace [/Indexed /DeviceRGB 3 <FF0000 00FF00 0000FF 808080>] /BitsPerComponent 8', ('RunLengthDecode',)),
         'Cmyk': im(bytes([0, 255, 255, 0]), ' /Width 1 /Height 1 /ColorSpace /DeviceCMYK /BitsPerComponent 8', ('ASCII85Decode',)),
         'Mas': im(bytes([0b10000000, 0b01000000]), ' /Width 2 /Height 2 /ImageMask true /BitsPerComponent 1'),
         'Gra': im(bytes([128]) * (1300 * 4), ' /Width 1300 /Height 4 /ColorSpace /DeviceGray /BitsPerComponent 8', ('LZWDecode',)),
         'Icc': im(bytes([0, 0, 255, 255, 255, 0]), ' /Width 2 /Height 1 /ColorSpace /CsIcc /BitsPerComponent 8'),
         'Jpg': im(b'\xff\xd8\xff\xe0' + bytes(40), ' /Width 8 /Height 8 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode')}
    icc = p.flujo(bytes(64), ' /N 3')
    s = ''.join('q 100 0 0 50 %d %d cm /%s Do Q\n' % (60 + 130 * (i % 4), 700 - 80 * (i // 4), n) for i, n in enumerate(x))
    s += '0 0 1 rg q 40 0 0 40 60 500 cm /Mas Do Q\n'
    s += 'q 100 0 0 -50 200 550 cm /Rgb Do Q q 0 50 -100 0 500 500 cm /Rgb Do Q\n'      # dada la vuelta; girada
    s += 'q 60 0 0 60 60 400 cm BI /W 2 /H 2 /CS /G /BPC 8 ID \x00\xff\xff\x00 EI Q\n'
    s += 'BT /F1 10 Tf 60 380 Td (Tras las im\xe1genes) Tj ET\n'
    c = p.flujo(s.encode('latin-1'))
    raiz = p.paginas([c], {'F1': f1}, mas_recursos=' /XObject << %s >> /ColorSpace << /CsIcc [/ICCBased %d 0 R] >>' % (
        ' '.join('/%s %d 0 R' % kv for kv in x.items()), icc))
    H = A4[1]
    N = 255
    sitio = lambda i: (60 + 130 * (i % 4), H - (700 - 80 * (i // 4)) - 50, 100, 50)
    e = [('error', 0), ('paginas', 1), ('aviso', 0, AV_IMAGEN), ('ncosas', 0, 'I', 13), ('que', 0, 'JPEG dañado'),
         ('mini', 0) + sitio(0) + (2, 2, bytes([196, 46, 21, 231]), 0),
         ('mini', 0) + sitio(1) + (4, 1, bytes([16, 240, 247, 231]), 0),
         ('mini', 0) + sitio(2) + (8, 1, bytes([16, 231] * 4), 0),
         ('mini', 0) + sitio(3) + (2, 2, bytes([196, 46, 21, 244]), 0),
         ('mini', 0) + sitio(4) + (1, 1, bytes([196]), 0),
         ('mini', 0) + sitio(5) + (2, 2, bytes([N, 16, 16, N]), 0),                 # la máscara, con el relleno de entonces (negro)
         ('mini', 0) + sitio(6) + (434, 2, bytes([244]) * 868, 0),                  # 1300 x 4, a un tercio
         ('mini', 0) + sitio(7) + (2, 1, bytes([21, 226]), 0),                      # azul y amarillo (ICCBased de 3)
         ('mini', 0) + sitio(8) + (0, 0, None, 0),                                  # la JPEG: sin miniatura
         ('mini', 0, 60, H - 540, 40, 40, 2, 2, bytes([N, 21, 21, N]), 0),          # la máscara, en azul
         ('mini', 0, 200, H - 550, 100, 50, 2, 2, bytes([196, 46, 21, 231]), 2),    # dada la vuelta
         ('mini', 0, 400, H - 550, 100, 50, 2, 2, bytes([196, 46, 21, 231]), 5),    # girada un cuarto (las columnas, hacia arriba)
         ('mini', 0, 60, H - 460, 60, 60, 2, 2, bytes([16, 231, 231, 16]), 0),      # la de dentro del contenido
         ('texto', 0, 'Tras las imágenes', 60, H - 380, 10), ('no_texto', 0, 'EI')]
    return 'IMAGENES', p.clasico(raiz), e


# ---------------------------------------------------------------------------
#  JPEG «lisos»: cada bloque de 8x8, de un color. Solo llevan el primer
#  coeficiente de cada bloque, así que se sabe exactamente lo que da cada punto
# ---------------------------------------------------------------------------
BAYER = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]


def a_paleta(r, g, b, x, y):
    """El color de la paleta que da Lector a un punto (im_color, en imagen.inc)."""
    if max(r, g, b) - min(r, g, b) <= 20:
        v = (r + g + b) // 3
        if v >= 248:
            return BLANCO
        if v <= 5:
            return NEGRO
        return 232 + min(23, (v + 5) // 11)
    u = BAYER[(y & 3) * 4 + (x & 3)] * 255 // 16 + 8
    return 16 + 36 * ((5 * r + u) // 255) + 6 * ((5 * g + u) // 255) + (5 * b + u) // 255


def ycc_rgb(y, cb, cr):
    """De YCbCr a rojo, verde y azul, con las cuentas de libjpeg (y de jpeg.inc)."""
    cb, cr = cb - 128, cr - 128
    t = lambda v: max(0, min(255, v))
    return (t(y + ((91881 * cr + 32768) >> 16)), t(y + ((-22554 * cb - 46802 * cr + 32768) >> 16)), t(y + ((116130 * cb + 32768) >> 16)))


def jpeg_liso(comp, an=None, al=None, marca='jfif', prog=False, ri=0, sof=None, ids=None, fines=None):
    """comp: de cada componente, (h, v, filas de valores 0-255 de sus bloques).
    marca: 'jfif', ('adobe', transformación) o None. Devuelve el JPEG. En «fines»
    (una lista) deja en qué bit de los datos del barrido acaba cada bloque."""
    hmax, vmax = max(c[0] for c in comp), max(c[1] for c in comp)
    mcux, mcuy = len(comp[0][2][0]) // comp[0][0], len(comp[0][2]) // comp[0][1]
    an, al = an or mcux * 8 * hmax, al or mcuy * 8 * vmax
    seg = lambda m, c: bytes([0xFF, m]) + struct.pack('>H', len(c) + 2) + c
    d = b'\xff\xd8'
    if marca == 'jfif':
        d += seg(0xE0, b'JFIF\0\1\1\0\0\1\0\1\0\0')
    elif marca:
        d += seg(0xEE, b'Adobe\0\x64\0\0\0\0' + bytes([marca[1]]))
    d += seg(0xDB, b'\0' + bytes([1] * 64))                             # cuantización: todo unos
    ids = ids or list(range(1, len(comp) + 1))
    d += seg(sof or (0xC2 if prog else 0xC0), struct.pack('>BHHB', 8, al, an, len(comp)) + b''.join(bytes([ids[i], c[0] << 4 | c[1], 0]) for i, c in enumerate(comp)))
    d += seg(0xC4, b'\0' + bytes([0, 0, 0, 12] + [0] * 12) + bytes(range(12)))  # DC: la clase, en 4 bits
    d += seg(0xC4, b'\x10' + bytes([1] + [0] * 15) + b'\0')            # AC: solo «se acabó», un bit a 0
    if ri:
        d += seg(0xDD, struct.pack('>H', ri))
    d += seg(0xDA, bytes([len(comp)]) + b''.join(bytes([ids[i], 0]) for i in range(len(comp))) + (b'\0\0\0' if prog else b'\0\x3f\0'))
    sal, acc, nb = bytearray(), 0, 0

    def pon(v, n):
        nonlocal acc, nb
        acc, nb = (acc << n) | (v & ((1 << n) - 1)), nb + n
        while nb >= 8:
            o = (acc >> (nb - 8)) & 255
            sal.append(o)
            if o == 255:
                sal.append(0)
            nb -= 8
        acc &= (1 << nb) - 1

    pred = [0] * len(comp)
    for n in range(mcux * mcuy):
        my, mx = divmod(n, mcux)
        if ri and n and n % ri == 0:
            if nb:
                pon(0xFF, 8 - nb)
            sal += bytes([0xFF, 0xD0 + (n // ri - 1) % 8])
            pred = [0] * len(comp)
        for i, (h, v, vals) in enumerate(comp):
            for bv in range(v):
                for bh in range(h):
                    dc = 8 * (vals[my * v + bv][mx * h + bh] - 128)
                    dif, pred[i] = dc - pred[i], dc
                    cat = abs(dif).bit_length()
                    pon(cat, 4)
                    if cat:
                        pon(dif if dif > 0 else dif + (1 << cat) - 1, cat)
                    if not prog:
                        pon(0, 1)
                    if fines is not None:
                        fines.append(len(sal) * 8 + nb)
    if nb:
        pon(0xFF, 8 - nb)
    return d + bytes(sal) + b'\xff\xd9'


def mini_lisa(comp, color, lado_pt, an=None, al=None):
    """La miniatura que tiene que salir de un jpeg_liso pintado con lado_pt puntos
    de lado largo: (ancho, alto, puntos). color: de los valores de los componentes
    de un punto, su rojo, verde y azul."""
    hmax, vmax = max(c[0] for c in comp), max(c[1] for c in comp)
    an = an or len(comp[0][2][0]) // comp[0][0] * 8 * hmax
    al = al or len(comp[0][2]) // comp[0][1] * 8 * vmax
    lado = max(64, min(1280, int(lado_pt * 2)))
    r, m = 0, max(an, al)
    while r < 3 and (m >> 1) >= lado:
        r, m = r + 1, m >> 1
    tw, th = -(-an >> r), -(-al >> r)
    assert max(tw, th) <= lado, 'esta se encogería: no se sabe punto por punto'
    pts = bytearray()
    for y in range(th):
        for x in range(tw):
            fx, fy = x << r, y << r
            v = [c[2][fy * c[1] // (8 * vmax)][fx * c[0] // (8 * hmax)] for c in comp]
            k = a_paleta(*color(v), x, y)
            pts.append(254 if k == 255 else k)
    return tw, th, bytes(pts)


def cmyk_rgb(c, m, y, k):
    return tuple((255 - v) * (255 - k) // 255 for v in (c, m, y))


def caso_jpeg():
    """Imágenes JPEG: gris, color (YCbCr 4:4:4 y 4:2:0), RGB de Adobe, sin girar
    por /ColorTransform, CMYK de Adobe con su /Decode (y sin él), con índices,
    gris al revés, progresiva, con reinicios, detrás de ASCII85, grandes (se
    reducen al vuelo y se encogen), cortada (sale lo que hay, con aviso), con
    /SMask y metidas en el contenido."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    im = lambda datos, dic, filtros=(): p.flujo(datos, ' /Type /XObject /Subtype /Image' + dic, filtros + ('DCTDecode',))
    FILTROS['DCTDecode'] = lambda d: d
    gris = [(1, 1, [[0, 60, 128, 255], [200, 30, 90, 160], [255, 128, 5, 250]])]
    ycc = [(1, 1, [[76, 150, 29], [255, 0, 128]]), (1, 1, [[85, 44, 255], [128, 128, 128]]), (1, 1, [[255, 21, 107], [128, 128, 128]])]
    c420 = [(2, 2, [[40, 80, 120, 160], [60, 100, 140, 180], [200, 220, 240, 255], [0, 20, 40, 60]]), (1, 1, [[90, 200], [128, 60]]), (1, 1, [[240, 100], [128, 180]])]
    rgb = [(1, 1, [[255, 0, 0, 40]]), (1, 1, [[0, 255, 0, 80]]), (1, 1, [[0, 0, 255, 200]])]
    cmyk = [(1, 1, [[255, 0, 255, 128]]), (1, 1, [[255, 255, 0, 128]]), (1, 1, [[0, 255, 255, 128]]), (1, 1, [[255, 255, 255, 60]])]
    ind = [(1, 1, [[0, 1], [2, 3]])]
    dos = [(1, 1, [[0] * 50 + [255] * 50])]
    liso = lambda n: [(1, 1, [[128] * n])]
    cortada_c = [(1, 1, [[10 + 3 * (x + 7 * y) for x in range(4)] for y in range(6)])]
    fines = []
    cortada = jpeg_liso(cortada_c, fines=fines)
    corte = (fines[13] + 7) // 8                                        # hasta el segundo bloque de la cuarta fila, y lo que quepa
    cortada = cortada[:cortada.index(b'\xff\xda') + 10 + corte]
    enteros = len([f for f in fines if f <= corte * 8])
    x = {
        'Gris': (im(jpeg_liso(gris), ' /Width 32 /Height 24 /ColorSpace /DeviceGray /BitsPerComponent 8'), gris, lambda v: (v[0],) * 3),
        'Ycc': (im(jpeg_liso(ycc), ' /Width 24 /Height 16 /ColorSpace /DeviceRGB /BitsPerComponent 8'), ycc, lambda v: ycc_rgb(*v)),
        'C420': (im(jpeg_liso(c420, ri=1), ' /Width 32 /Height 32 /ColorSpace /DeviceRGB /BitsPerComponent 8'), c420, lambda v: ycc_rgb(*v)),
        'Rgb': (im(jpeg_liso(rgb, marca=('adobe', 0)), ' /Width 32 /Height 8 /ColorSpace /DeviceRGB /BitsPerComponent 8'), rgb, lambda v: tuple(v)),
        'RgbCT': (im(jpeg_liso(rgb, marca=None), ' /Width 32 /Height 8 /ColorSpace /DeviceRGB /BitsPerComponent 8 /DecodeParms << /ColorTransform 0 >>'), rgb, lambda v: tuple(v)),
        'YccCT': (im(jpeg_liso(ycc, marca=None, ids=[82, 71, 66]), ' /Width 24 /Height 16 /ColorSpace /DeviceRGB /BitsPerComponent 8 /DecodeParms << /ColorTransform 1 >>'), ycc, lambda v: ycc_rgb(*v)),
        'Cmyk': (im(jpeg_liso(cmyk, marca=('adobe', 0)), ' /Width 32 /Height 8 /ColorSpace /DeviceCMYK /BitsPerComponent 8 /Decode [1 0 1 0 1 0 1 0]'), cmyk,
                 lambda v: cmyk_rgb(*(255 - t for t in v))),
        'CmykSin': (im(jpeg_liso(cmyk, marca=('adobe', 0)), ' /Width 32 /Height 8 /ColorSpace /DeviceCMYK /BitsPerComponent 8'), cmyk, lambda v: cmyk_rgb(*v)),
        'Ycck': (im(jpeg_liso(ycc + [(1, 1, [[255, 255, 200], [255, 0, 128]])], marca=('adobe', 2)), ' /Width 24 /Height 16 /ColorSpace /DeviceCMYK /BitsPerComponent 8 /Decode [1 0 1 0 1 0 1 0]'),
                 ycc + [(1, 1, [[255, 255, 200], [255, 0, 128]])], lambda v: cmyk_rgb(*(list(ycc_rgb(*v[:3])) + [255 - v[3]]))),
        'Ind': (im(jpeg_liso(ind), ' /Width 16 /Height 16 /ColorSpace [/Indexed /DeviceRGB 3 <FF0000 00FF00 0000FF 808080>] /BitsPerComponent 8'), ind,
                lambda v: [(255, 0, 0), (0, 255, 0), (0, 0, 255), (128, 128, 128)][v[0]]),
        'GInv': (im(jpeg_liso(gris), ' /Width 32 /Height 24 /ColorSpace /DeviceGray /BitsPerComponent 8 /Decode [1 0]'), gris, lambda v: (255 - v[0],) * 3),
        'Prog': (im(jpeg_liso(gris, prog=True), ' /Width 32 /Height 24 /ColorSpace /DeviceGray /BitsPerComponent 8'), gris, lambda v: (v[0],) * 3),
        'A85': (im(jpeg_liso(ycc), ' /Width 24 /Height 16 /ColorSpace /DeviceRGB /BitsPerComponent 8', ('ASCII85Decode',)), ycc, lambda v: ycc_rgb(*v)),
        'Dos': (im(jpeg_liso(dos), ' /Width 800 /Height 8 /ColorSpace /DeviceGray /BitsPerComponent 8'), dos, lambda v: (v[0],) * 3),
        'Raro': (im(jpeg_liso(gris, an=27, al=19), ' /Width 27 /Height 19 /ColorSpace /DeviceGray /BitsPerComponent 8'), gris, lambda v: (v[0],) * 3),
    }
    otras = {'Larga': im(jpeg_liso(liso(200)), ' /Width 1600 /Height 8 /ColorSpace /DeviceGray /BitsPerComponent 8'),
             'Enorme': im(jpeg_liso(liso(1600)), ' /Width 12800 /Height 8 /ColorSpace /DeviceGray /BitsPerComponent 8'),
             'Cortada': im(cortada, ' /Width 32 /Height 48 /ColorSpace /DeviceGray /BitsPerComponent 8'),
             'ConS': p.flujo(jpeg_liso(ycc), ' /Type /XObject /Subtype /Image /Width 24 /Height 16 /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask %d 0 R /Filter /DCTDecode' % (p.n + 2))}
    p.flujo(bytes([255, 0, 130, 127, 128, 200]), ' /Type /XObject /Subtype /Image /Width 3 /Height 2 /ColorSpace /DeviceGray /BitsPerComponent 8')
    H = A4[1]
    s, e = '', [('error', 0), ('paginas', 1), ('que', 0, '')]
    for i, (n, (num, comp, color)) in enumerate(x.items()):
        px, py = 40 + 135 * (i % 4), 760 - 70 * (i // 4)
        s += 'q 120 0 0 50 %d %d cm /%s Do Q\n' % (px, py, n)
        if n == 'Dos':
            e.append(('mini', 0, px, H - py - 50, 120, 50) + mini_lisa(comp, color, 100) + (0,))   # (a 1/4 al vuelo: 800 -> 200)
            s = s.replace('q 120 0 0 50 %d %d cm /Dos' % (px, py), 'q 100 0 0 50 %d %d cm /Dos' % (px, py))
            e[-1] = ('mini', 0, px, H - py - 50, 100, 50) + e[-1][6:]
        elif n == 'Raro':
            e.append(('mini', 0, px, H - py - 50, 120, 50) + mini_lisa(comp, color, 120, 27, 19) + (0,))
        else:
            e.append(('mini', 0, px, H - py - 50, 120, 50) + mini_lisa(comp, color, 120) + (0,))
    # las grandes: 1600 x 8 pintada a 500 puntos (se encoge a 1000 x 5) y 12800 x 8 a 300 (a 1/8 al vuelo y a 600 x 1)
    s += 'q 500 0 0 2.5 40 440 cm /Larga Do Q q 300 0 0 2 40 420 cm /Enorme Do Q\n'
    e += [('mini', 0, 40, H - 442.5, 500, 2.5, 1000, 5, bytes([244]) * 5000, 0), ('mini', 0, 40, H - 422, 300, 2, 600, 1, bytes([244]) * 600, 0)]
    # la cortada: salen las filas de bloques enteras y, de la que se queda a medias,
    # lo que haya con el resto en gris; lo demás deja ver la hoja
    s += 'q 40 0 0 60 40 340 cm /Cortada Do Q\n'
    tw, th, pts = mini_lisa(cortada_c, lambda v: (v[0],) * 3, 60)
    pts = bytearray(pts)
    filas, resto = divmod(enteros, 4)
    assert 0 < filas < 6 and resto
    for y in range(filas * 8, 48):
        for xx in range(32):
            if y >= filas * 8 + 8:
                pts[y * 32 + xx] = 255
            elif xx >= resto * 8:
                pts[y * 32 + xx] = a_paleta(128, 128, 128, xx, y)
    e += [('mini', 0, 40, H - 400, 40, 60, tw, th, bytes(pts), 0), ('aviso', 0, AV_ROTO), ('sin_aviso', 0, AV_IMAGEN)]
    # con /SMask: se ve donde la opacidad pasa de la mitad (3 x 2 sobre 24 x 16)
    s += 'q 60 0 0 40 120 340 cm /ConS Do Q\n'
    tw, th, pts = mini_lisa(ycc, lambda v: ycc_rgb(*v), 60)
    alfa = [[255, 0, 130], [127, 128, 200]]
    e.append(('mini', 0, 120, H - 380, 60, 40, tw, th, bytes(pts[y * tw + xx] if alfa[y * 2 // th][xx * 3 // tw] >= 128 else 255 for y in range(th) for xx in range(tw)), 0))
    # metidas en el contenido: una JPEG en hexadecimal y una con Flate
    j = jpeg_liso([(1, 1, [[90, 200]])])
    s += 'q 32 0 0 16 220 340 cm BI /W 16 /H 8 /CS /G /BPC 8 /F [/AHx /DCT] ID %s> EI Q\n' % j.hex().upper()
    e.append(('mini', 0, 220, H - 356, 32, 16) + mini_lisa([(1, 1, [[90, 200]])], lambda v: (v[0],) * 3, 32) + (0,))
    s += 'q 20 0 0 20 280 340 cm BI /W 2 /H 2 /CS /RGB /BPC 8 /F /Fl ID '
    c = s.encode('latin-1') + zlib.compress(bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])) + b' EI Q\nBT /F1 10 Tf 40 300 Td (Tras las JPEG) Tj ET\n'
    e += [('mini', 0, 280, H - 360, 20, 20, 2, 2, bytes([196, 46, 21, 231]), 0), ('texto', 0, 'Tras las JPEG', 40, H - 300, 10), ('no_texto', 0, 'EI'),
          ('ncosas', 0, 'I', len(x) + 6)]
    todas = dict((n, v[0]) for n, v in x.items())
    todas.update(otras)
    raiz = p.paginas([p.flujo(c)], {'F1': f1}, mas_recursos=' /XObject << %s >>' % ' '.join('/%s %d 0 R' % kv for kv in todas.items()))
    return 'JPEG', p.clasico(raiz), e


def caso_jpeg_no():
    """Las imágenes que no se saben, cada una en su página, con su nombre en
    el aviso: JPEG 2000, fax, JBIG2, JPEG aritmético, sin pérdidas, jerárquico,
    de 12 bits, uno que no es un JPEG, un filtro inventado, y un espacio de
    color que no se conoce (esa, sin nombre)."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    base = jpeg_liso([(1, 1, [[10, 200]])])
    FILTROS.update({k: (lambda d: d) for k in ('DCTDecode', 'JPXDecode', 'CCITTFaxDecode', 'JBIG2Decode', 'Raro')})
    lista = [(base, 'DCTDecode', '/DeviceGray', ''),
             (bytes(60), 'JPXDecode', '/DeviceRGB', 'JPEG 2000'), (bytes(60), 'CCITTFaxDecode', '/DeviceGray', 'fax CCITT'),
             (bytes(60), 'JBIG2Decode', '/DeviceGray', 'JBIG2'),
             (jpeg_liso([(1, 1, [[10, 200]])], sof=0xC9), 'DCTDecode', '/DeviceGray', 'JPEG aritmético'),
             (jpeg_liso([(1, 1, [[10, 200]])], sof=0xC3), 'DCTDecode', '/DeviceGray', 'JPEG sin pérdidas'),
             (jpeg_liso([(1, 1, [[10, 200]])], sof=0xC5), 'DCTDecode', '/DeviceGray', 'JPEG jerárquico'),
             (base.replace(b'\x00\x0b\x08', b'\x00\x0b\x0c', 1), 'DCTDecode', '/DeviceGray', 'JPEG de 12 bits'),
             (b'esto no es un JPEG' + bytes(30), 'DCTDecode', '/DeviceGray', 'JPEG dañado'),
             (base[:base.index(b'\xff\xda') + 10], 'DCTDecode', '/DeviceGray', 'JPEG dañado'),
             (bytes(60), 'Raro', '/DeviceGray', 'Raro'),
             (bytes(16), None, '[/Separation /Oro /DeviceCMYK 9 0 R]', '')]
    cont, xo, e = [], {}, [('error', 0), ('paginas', len(lista))]
    for i, (d, filtro, cs, que) in enumerate(lista):
        xo['Im%d' % i] = p.flujo(d, ' /Type /XObject /Subtype /Image /Width 16 /Height 8 /ColorSpace %s /BitsPerComponent 8' % cs, (filtro,) if filtro else ())
        cont.append(p.flujo('q 100 0 0 50 60 700 cm /Im%d Do Q BT /F1 10 Tf 60 680 Td (Imagen %d) Tj ET' % (i, i)))
        e += [('que', i, que), ('texto', i, 'Imagen %d' % i, 60, A4[1] - 680, 10)]
        if i:
            e += [('aviso', i, AV_IMAGEN), ('mini', i, 60, A4[1] - 750, 100, 50, 0, 0, None, 0)]
        else:
            e += [('sin_aviso', i, 255), ('mini', i, 60, A4[1] - 750, 100, 50) + mini_lisa([(1, 1, [[10, 200]])], lambda v: (v[0],) * 3, 100) + (0,)]
    raiz = p.paginas(cont, {'F1': f1}, mas_recursos=' /XObject << %s >>' % ' '.join('/%s %d 0 R' % kv for kv in xo.items()))
    return 'JPEGNO', p.clasico(raiz), e


def caso_mascaras():
    """La transparencia de una imagen, a todo o nada: /SMask (opacidad, de 8
    bits y de más resolución que la imagen), /Mask con otra imagen de un bit,
    /Mask con colores (un índice de la paleta; un gris), y el /Decode al revés
    en RGB y en CMYK."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    N = 255
    im = lambda datos, dic: p.flujo(datos, ' /Type /XObject /Subtype /Image' + dic)
    alfa = im(bytes([255, 255, 0, 0, 255, 200, 100, 0, 128, 127, 0, 0, 0, 0, 0, 0]), ' /Width 4 /Height 4 /ColorSpace /DeviceGray /BitsPerComponent 8')
    uno = im(bytes([0b01000000, 0b10000000]), ' /Width 2 /Height 2 /ImageMask true /BitsPerComponent 1')
    cuatro = bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255])
    x = {'S': im(cuatro, ' /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask %d 0 R' % alfa),
         'M': im(cuatro, ' /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Mask %d 0 R' % uno),
         'K': im(bytes([0, 1, 2, 3]), ' /Width 2 /Height 2 /ColorSpace [/Indexed /DeviceRGB 3 <FF0000 00FF00 0000FF 808080>] /BitsPerComponent 8 /Mask [1 2]'),
         'G': im(bytes([0, 100, 101, 255]), ' /Width 4 /Height 1 /ColorSpace /DeviceGray /BitsPerComponent 8 /Mask [100 100]'),
         'R': im(cuatro, ' /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Decode [1 0 1 0 1 0]'),
         'C': im(bytes([255, 0, 0, 255]), ' /Width 1 /Height 1 /ColorSpace /DeviceCMYK /BitsPerComponent 8 /Decode [1 0 1 0 1 0 1 0]')}
    s = '1 1 0 rg 30 690 560 70 re f\n'                               # una banda amarilla debajo: se tiene que ver por los huecos
    s += ''.join('q 100 0 0 50 %d 700 cm /%s Do Q\n' % (40 + 110 * i, n) for i, n in enumerate(x))
    s += 'BT /F1 10 Tf 0 g 40 680 Td (Tras las transparencias) Tj ET\n'
    raiz = p.paginas([p.flujo(s)], {'F1': f1}, mas_recursos=' /XObject << %s >>' % ' '.join('/%s %d 0 R' % kv for kv in x.items()))
    H = A4[1]
    sitio = lambda i: (40 + 110 * i, H - 750, 100, 50)
    e = [('error', 0), ('paginas', 1), ('sin_aviso', 0, 255), ('ncosas', 0, 'I', 6),
         ('mini', 0) + sitio(0) + (2, 2, bytes([196, N, 21, N]), 0),                    # la opacidad de cada punto: 255, 0 / 128, 0
         ('mini', 0) + sitio(1) + (2, 2, bytes([196, N, N, 231]), 0),                   # se ve donde la máscara de un bit lleva un 0
         ('mini', 0) + sitio(2) + (2, 2, bytes([196, N, N, 244]), 0),                   # los índices 1 y 2 no se pintan
         ('mini', 0) + sitio(3) + (4, 1, bytes([16, N, 241, 231]), 0),                  # el gris 100, tampoco
         ('mini', 0) + sitio(4) + (2, 2, bytes([51, 201, 226, 16]), 0),                 # RGB al revés: cian, magenta, amarillo y negro
         ('mini', 0) + sitio(5) + (1, 1, bytes([196]), 0),                              # CMYK al revés (queda 0, 1, 1, 0: rojo)
         ('texto', 0, 'Tras las transparencias', 40, H - 680, 10)]
    return 'MASCARAS', p.clasico(raiz), e


def caso_recortes():
    """Recortes: un rectángulo (W n), otro dentro de otro (W*), q/Q, y uno que
    no es un rectángulo (no recorta)."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    s = ('q 100 600 200 100 re W n 0 0 1 rg 0 0 600 800 re f BT /F1 10 Tf 110 650 Td 0 g (dentro del recorte) Tj ET\n'
         'q 150 550 300 100 re W* n 1 0 0 rg 0 0 600 800 re f 0 0 m 600 800 l S Q\n'
         'BT /F1 10 Tf 110 630 Td (otra vez el primero) Tj ET Q\n'
         'BT /F1 10 Tf 110 500 Td (libre) Tj ET\n'
         'q 100 300 m 200 300 l 150 400 l h W n BT /F1 10 Tf 110 350 Td (en un tri\xe1ngulo) Tj ET Q\n'
         'q 300 300 100 50 re 0 1 0 rg W f BT /F1 10 Tf 310 320 Td (recorte y relleno) Tj ET Q\n')
    c = p.flujo(s.encode('latin-1'))
    raiz = p.paginas([c], {'F1': f1})
    H = A4[1]
    r1, r2, r3 = (100, H - 700, 300, H - 600), (150, H - 650, 300, H - 600), (300, H - 350, 400, H - 300)
    e = [('error', 0), ('sin_aviso', 0, 255),
         ('recorte', 0, 'R', 21) + r1, ('recorte', 0, 'T', 'dentro del recorte') + r1,
         ('recorte', 0, 'R', 196) + r2, ('recorte', 0, 'L', 16) + r2,
         ('recorte', 0, 'T', 'otra vez el primero') + r1,
         ('recorte', 0, 'T', 'libre'), ('recorte', 0, 'T', 'en un triángulo'),
         ('recorte', 0, 'R', 46), ('recorte', 0, 'T', 'recorte y relleno') + r3]
    return 'RECORTES', p.clasico(raiz), e


def caso_bomba():
    """Un formulario que se pinta a sí mismo diez veces (diez elevado a ocho
    con ocho niveles): Lector tiene que cortar a tiempo, y decirlo."""
    p = Pdf()
    f1 = p.fuente('Helvetica')
    fm = p.flujo('BT /F1 10 Tf 10 10 Td (bomba) Tj ET ' + 'q 1 0 0 1 3 7 cm /Fm1 Do Q ' * 10,
                 ' /Type /XObject /Subtype /Form /BBox [0 0 100 50]')
    c = p.flujo('BT /F1 12 Tf 72 780 Td (Antes de la bomba) Tj ET /Fm1 Do BT /F1 12 Tf 72 700 Td (Fuera) Tj ET')
    raiz = p.paginas([c], {'F1': f1}, mas_recursos=' /XObject << /Fm1 %d 0 R >>' % fm)
    return 'BOMBA', p.clasico(raiz), [('error', 0), ('paginas', 1), ('aviso', 0, AV_LLENA),
                                      ('texto', 0, 'Antes de la bomba', 72, A4[1] - 780, 12), ('contiene', 0, 'bomba')]


def caso_muchas(n=4100):
    """Más páginas de las que caben (4.000): se ven las primeras y se avisa."""
    p = Pdf()
    f1 = p.fuente('Courier')
    c = p.flujo('BT /F1 12 Tf 72 780 Td (Una de muchas) Tj ET')
    raiz = p.paginas([c] * n, {'F1': f1})
    return 'MUCHAS', p.clasico(raiz), [('error', 0), ('paginas', 4000), ('muchos', 1),
                                       ('texto', 3999, 'Una de muchas', 72, A4[1] - 780, 12, MAQUINA)]


def casos():
    c = [caso_carta(), caso_fuentes(), caso_factura(), caso_moderno(), caso_moderno('MODERNO2', False), caso_incremental(),
         caso_filtros(), caso_codif(), caso_cifrado(), caso_cifrado15(), caso_no_sabe(), caso_graficos(), caso_largo(),
         caso_espacios(), caso_bomba(), caso_muchas(), caso_imagenes(), caso_recortes(), caso_jpeg(), caso_jpeg_no(), caso_mascaras()]
    return c + rotos()


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(sys.argv[1], exist_ok=True)
    for nombre, datos, _ in casos():
        open(os.path.join(sys.argv[1], nombre + '.PDF'), 'wb').write(datos)
        print('%-12s %7d octetos' % (nombre + '.PDF', len(datos)))


if __name__ == '__main__':
    main()
