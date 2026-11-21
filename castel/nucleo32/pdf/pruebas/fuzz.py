#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas/fuzz.py PROGRAMA CORPUS VUELTAS [SEMILLA]: PDF estropeados a
# propósito (octetos cambiados, cortados, trozos repetidos, números
# cambiados, anidados sin fin, flujos que crecen mucho) contra Hispa-PDF
# hecho con ASan y UBSan (anfitrion.sh). Cada uno pasa por INFO, TEXTO,
# IMAGENES (a 30 ppp), COMPRIME y UNE. Lo que rompe se guarda en
# /tmp/hpdf-fuzz/fallos. Sin red; el corpus no va en el repositorio. (Sin
# mirar las fugas: un error sin vuelta salta sin soltar la memoria, y el
# sistema la recoge al acabar el programa.)
import os, random, re, subprocess, sys, zlib

prog, corpus, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
random.seed(int(sys.argv[4]) if len(sys.argv) > 4 else 1)
W = '/tmp/hpdf-fuzz'
os.makedirs(W + '/fallos', exist_ok=True)
semillas = [open(os.path.join(corpus, n), 'rb').read() for n in sorted(os.listdir(corpus)) if n.lower().endswith('.pdf')]
semillas = [s for s in semillas if len(s) < 3_000_000]

def hostil():
    """PDF hechos a mano para los límites."""
    r = random.randrange(6)
    if r == 0:   # anidado sin fin
        return b'%PDF-1.4\n1 0 obj ' + b'[' * 100000 + b'\nendobj\ntrailer<</Root 1 0 R>>\n%%EOF'
    if r == 1:   # bomba de Flate
        z = zlib.compress(b'\0' * 50_000_000, 9)
        return (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n'
                b'3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 600 800]/Contents 4 0 R>>endobj\n'
                b'4 0 obj<</Length ' + str(len(z)).encode() + b'/Filter/FlateDecode>>stream\n' + z + b'\nendstream endobj\ntrailer<</Root 1 0 R>>\n%%EOF')
    if r == 2:   # árbol de páginas que se apunta a sí mismo
        return (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[2 0 R 3 0 R 2 0 R]/Count 9>>endobj\n'
                b'3 0 obj<</Type/Page/Parent 2 0 R/Contents 3 0 R>>endobj\ntrailer<</Root 1 0 R/Size 99999999>>\n%%EOF')
    if r == 3:   # Length que se apunta a sí mismo, Prev en bucle
        return (b'%PDF-1.5\n1 0 obj<</Length 1 0 R>>stream\nxx\nendstream endobj\nxref\n0 2\n0000000000 65535 f \n0000000009 00000 n \n'
                b'trailer<</Root 1 0 R/Prev 60/Size 2>>\nstartxref\n60\n%%EOF')
    if r == 4:   # contenido con q sin fin y formularios que se llaman
        c = b'q ' * 5000 + b'/F Do ' * 50 + b'BT /X 9 Tf (abc) Tj ET ' + b'Q ' * 3
        return (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n'
                b'3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 600 800]/Contents 4 0 R/Resources<</XObject<</F 5 0 R>>>>>>endobj\n'
                b'4 0 obj<</Length ' + str(len(c)).encode() + b'>>stream\n' + c + b'\nendstream endobj\n'
                b'5 0 obj<</Type/XObject/Subtype/Form/BBox[0 0 9 9]/Resources<</XObject<</F 5 0 R>>>>/Length 6>>stream\n/F Do\nendstream endobj\n'
                b'trailer<</Root 1 0 R>>\n%%EOF')
    # imagen enorme
    return (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n'
            b'3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 600 800]/Contents 4 0 R/Resources<</XObject<</I 5 0 R>>>>>>endobj\n'
            b'4 0 obj<</Length 20>>stream\nq 600 0 0 800 0 0 cm /I Do Q\nendstream endobj\n'
            b'5 0 obj<</Type/XObject/Subtype/Image/Width 60000/Height 60000/BitsPerComponent 8/ColorSpace/DeviceRGB/Length 3>>stream\nabc\nendstream endobj\n'
            b'trailer<</Root 1 0 R>>\n%%EOF')

def muta(d):
    d = bytearray(d)
    for _ in range(random.randint(1, 12)):
        r = random.randrange(8)
        if not d:
            break
        i = random.randrange(len(d))
        if r == 0:
            d[i] = random.randrange(256)
        elif r == 1:
            del d[i:i + random.randint(1, 2000)]
        elif r == 2:
            j = random.randrange(len(d))
            d[i:i] = d[j:j + random.randint(1, 4000)]
        elif r == 3:
            d = d[:random.randrange(len(d))]
        elif r == 4:
            m = list(re.finditer(rb'\d+', bytes(d[max(0, i - 5000):i + 5000])))
            if m:
                x = random.choice(m)
                a = max(0, i - 5000) + x.start()
                d[a:a + len(x.group())] = str(random.choice([0, 1, -1, 2**31 - 1, 2**32, 99999999, random.randrange(10**6)])).encode()
        elif r == 5:
            d[i:i] = random.choice([b'<<', b'>>', b'[', b']', b'(', b')', b' R ', b'stream\n', b'endstream', b'obj', b'/Filter/FlateDecode', b'\\'])
        elif r == 6:
            for k in range(i, min(len(d), i + 64)):
                d[k] ^= 1 << random.randrange(8)
        else:
            d[i:i] = b' ' + bytes(random.randrange(256) for _ in range(random.randint(1, 30)))
    return bytes(d)

fallos = 0
for v in range(vueltas):
    datos = hostil() if random.random() < 0.08 else muta(random.choice(semillas))
    f = W + '/a%d.pdf' % os.getpid()
    open(f, 'wb').write(datos)
    for orden in (['INFO', f], ['TEXTO', f, '/SALIDA=' + W + '/t.txt'], ['IMAGENES', f, '/PAGINAS=1', '/PUNTOS=30', '/SALIDA=' + W + '/i.png'],
                  ['COMPRIME', f, '/SALIDA=' + W + '/c.pdf'], ['UNE', f, f, '/SALIDA=' + W + '/u.pdf']):
        try:
            r = subprocess.run([prog] + orden, capture_output=True, timeout=60, env=dict(os.environ, HPDF_MEMORIA='1500', ASAN_OPTIONS='detect_leaks=0'))
            malo = r.returncode not in (0, 1, 2) or b'Sanitizer' in r.stderr or b'runtime error' in r.stderr
            por = r.stderr.decode('latin-1')[-3000:]
        except subprocess.TimeoutExpired:
            malo, por = True, 'tarda más de 60 s'
        if malo:
            fallos += 1
            n = W + '/fallos/f%d_%d_%s.pdf' % (os.getpid(), v, orden[0])
            open(n, 'wb').write(datos)
            open(n + '.txt', 'w').write(por)
            print('FALLO', n, por[:300].replace('\n', ' | '))
            break
print('fuzz: %d vueltas, %d fallos' % (vueltas, fallos))
sys.exit(1 if fallos else 0)
