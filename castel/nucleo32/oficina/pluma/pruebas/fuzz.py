#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# fuzz.py VUELCA CARPETA VUELTAS: estropea a propósito los .ODT y .DOCX de
# prueba (los de haz_docs.py, en CARPETA) y los pasa por los lectores de
# Pluma 2 («VUELCA --lee», compilado con ASan y UBSan: pruebas/formatos.sh).
# Estropea los octetos del ZIP, lo corta, cambia el XML de dentro (octetos,
# cortes, trozos repetidos, anidado muy hondo, entidades, números enormes en
# las celdas repetidas y unidas, estilos que se heredan a sí mismos, rutas
# que se salen, partes que faltan) y hace «bombas» de descompresión. Si
# algo falla, deja el archivo en CARPETA/fallo-N.* y acaba con 1.
import os, random, subprocess, sys, zipfile, io

vuelca, carpeta, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
rnd = random.Random(int(os.environ.get('SEMILLA', '2026')))
orig = {x: open(os.path.join(carpeta, 'prueba.' + x), 'rb').read() for x in ('odt', 'docx')}

def partes(d):
    z = zipfile.ZipFile(io.BytesIO(d))
    return [(i.filename, z.read(i.filename)) for i in z.infolist()]

def zipea(ps, comprimir=True):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w') as z:
        for n, d in ps:
            z.writestr(zipfile.ZipInfo(n) if n == 'mimetype' else n, d,
                       compress_type=zipfile.ZIP_STORED if n == 'mimetype' or not comprimir else zipfile.ZIP_DEFLATED)
    return b.getvalue()

def estropea_octetos(d, n):
    d = bytearray(d)
    for _ in range(n):
        if d:
            d[rnd.randrange(len(d))] = rnd.randrange(256)
    return bytes(d)

def estropea_xml(x):
    x = bytearray(x)
    k = rnd.randrange(12)
    if k == 0 and x:
        return estropea_octetos(x, rnd.randint(1, 20))
    if k == 1 and x:
        return bytes(x[:rnd.randrange(len(x))])
    if k == 2 and len(x) > 10:
        a = rnd.randrange(len(x)); b = min(len(x), a + rnd.randint(1, 400))
        return bytes(x[:b] + x[a:b] * rnd.randint(2, 30) + x[b:])
    if k == 3:
        prof = rnd.choice([300, 3000, 60000])
        t = rnd.choice([b'text:span', b'w:sdt', b'text:list', b'table:table', b'w:tbl', b'text:section', b'w:hyperlink'])
        i = rnd.randrange(len(x)) if x else 0
        return bytes(x[:i] + (b'<' + t + b'>') * prof + b'hondo' + (b'</' + t + b'>') * prof + x[i:])
    if k == 4:
        i = rnd.randrange(len(x)) if x else 0
        return bytes(x[:i] + b'&amp;&#x10FFFF;&#0;&#xD800;&#99999999999;&lt' * rnd.randint(1, 500) + b'<![CDATA[' + x[i:])
    if k == 5:
        rep = rnd.choice([b'99999999', b'2147483647', b'-5', b'0', b'65536', b'4294967296'])
        for a in (b'number-columns-repeated="', b'number-rows-repeated="', b'number-columns-spanned="', b'number-rows-spanned="',
                  b'w:gridSpan w:val="', b'w:sz w:val="', b'w:ilvl w:val="', b'w:numId w:val="', b'text:c="', b'cx="', b'w:w="', b'w:pos="', b'w:left="'):
            x = x.replace(a + b'2"', a + rep + b'"').replace(a + b'1"', a + rep + b'"').replace(a + b'3"', a + rep + b'"')
        return bytes(x)
    if k == 6:
        return bytes(x.replace(b'style:parent-style-name="Standard"', b'style:parent-style-name="P1"').replace(
            b'<w:basedOn w:val="Normal"/>', b'<w:basedOn w:val="Ttulo1"/>').replace(b'w:styleId="Normal"', b'w:styleId="Ttulo1"'))
    if k == 7:
        return bytes(x.replace(b'Pictures/foto.png', b'../../../../etc/passwd').replace(b'media/image1.png', b'/../../x').replace(b'rId10', b'rId4'))
    if k == 8:
        return bytes(x.replace(b'<table:covered-table-cell/>', b'').replace(b'<w:vMerge/>', b'<w:vMerge w:val="restart"/>').replace(b'</w:tr>', b'</w:tr><w:tr></w:tr>'))
    if k == 9:
        return b'<!DOCTYPE a [<!ENTITY a "&a;&a;"><!ENTITY b SYSTEM "file:///etc/passwd">]>' + bytes(x).replace('Viñeta'.encode(), b'&a;&b;')
    if k == 10:
        return bytes(x.replace(b'"', b'')) if rnd.random() < 0.5 else bytes(x.replace(b'>', b'', rnd.randint(1, 30)))
    return bytes(x.replace(b'<', b'<<', rnd.randint(1, 30)))

def caso(x):
    k = rnd.randrange(10)
    d = orig[x]
    if k == 0:
        return estropea_octetos(d, rnd.randint(1, 50))
    if k == 1:
        return d[:rnd.randrange(len(d))]
    if k == 2:
        # una bomba: un XML de muchos megas de lo mismo, comprimido
        ps = partes(d)
        n = 'content.xml' if x == 'odt' else 'word/document.xml'
        relleno = b'<text:p>' + b'a' * 1000 + b'</text:p>' if x == 'odt' else b'<w:p><w:r><w:t>' + b'a' * 1000 + b'</w:t></w:r></w:p>'
        tam = rnd.choice([1, 20, 80])
        ps = [(p, (q.replace(b'</office:text>' if x == 'odt' else b'<w:sectPr', relleno * (tam * 1000) + (b'</office:text>' if x == 'odt' else b'<w:sectPr'), 1)) if p == n else q) for p, q in ps]
        return zipea(ps)
    if k == 3:
        # muchas entradas
        ps = partes(d) + [('basura/%d.xml' % i, b'<a/>') for i in range(rnd.choice([100, 30000]))]
        return zipea(ps, False)
    if k == 4:
        # falta una parte
        ps = partes(d)
        del ps[rnd.randrange(len(ps))]
        return zipea(ps)
    # una parte de XML estropeada (o varias)
    ps = partes(d)
    for _ in range(rnd.randint(1, 3)):
        i = rnd.randrange(len(ps))
        if ps[i][0].endswith('.xml') or ps[i][0].endswith('.rels'):
            ps[i] = (ps[i][0], estropea_xml(ps[i][1]))
    return zipea(ps)

env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
fallos = 0
LOTE = 10
for v in range(0, vueltas, LOTE):
    nombres = []
    for j in range(min(LOTE, vueltas - v)):
        x = rnd.choice(['odt', 'docx'])
        n = os.path.join(carpeta, 'fz%d.%s' % (j, x))
        open(n, 'wb').write(caso(x))
        nombres.append(n)
    r = subprocess.run([vuelca, '--lee'] + nombres, env=env, capture_output=True, timeout=600)
    if r.returncode != 0:
        # ¿cuál? uno a uno
        for n in nombres:
            r = subprocess.run([vuelca, '--lee', n], env=env, capture_output=True, timeout=600)
            if r.returncode != 0:
                fallos += 1
                dest = os.path.join(carpeta, 'fallo-%d%s' % (fallos, os.path.splitext(n)[1]))
                os.replace(n, dest)
                sys.stderr.write('FALLO (%s):\n%s\n' % (dest, r.stderr.decode('utf-8', 'replace')[-3000:]))
        if fallos >= 3:
            break
print('%d vueltas, %d fallos' % (v + LOTE if v + LOTE < vueltas else vueltas, fallos))
sys.exit(1 if fallos else 0)
