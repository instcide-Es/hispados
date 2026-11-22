#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas/fuzz.py PROGRAMA CORPUS VUELTAS [SEMILLA] [HILOS]: mensajes, HTML,
# vCard y cabeceras estropeados a propósito (octetos cambiados, cortes,
# trozos repetidos, fronteras cambiadas, multipart anidado 100 veces,
# cabeceras enormes, =?...?= sin cerrar, RFC 2231 con 1000 trozos, entidades
# enormes, etiquetas anidadas 10000 veces, vCard con renglones partidos sin
# fin) contra prueba_mime hecho con ASan y UBSan (pruebas_mime.sh). Cada uno
# pasa por mime, nombres, html, texto, 2047, 2231, dirs, fecha y vcard, con
# un tope de tiempo (lo cuadrático se nota). Una línea por vuelta que falle
# («  FAIL ...») y una al final («  OK   ...» o «  FAIL ...»); lo que rompe se
# guarda en /tmp/correo-fuzz/fallos. Sin red; el corpus son los casos/.
import os, random, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

prog, corpus, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
semilla = int(sys.argv[4]) if len(sys.argv) > 4 else 1
hilos = int(sys.argv[5]) if len(sys.argv) > 5 else max(1, (os.cpu_count() or 2) - 1)
TOPE = 20	# segundos por ejecución (con ASan)
W = '/tmp/correo-fuzz'
os.makedirs(W + '/fallos', exist_ok=True)
semillas = [open(os.path.join(corpus, n), 'rb').read() for n in sorted(os.listdir(corpus))
            if not n.endswith('.esperado') and os.path.isfile(os.path.join(corpus, n))]
ORDENES = ['mime', 'nombres', 'html', 'texto', '2047', '2231', 'dirs', 'fecha', 'vcard']


def hostil(r):
    k = r.randrange(12)
    if k == 0:   # multipart anidado 100 veces (y sin cerrar)
        s = b'From: a@b.es\r\nSubject: hondo\r\n'
        for i in range(100):
            s += b'Content-Type: multipart/mixed; boundary="b%d"\r\n\r\n--b%d\r\n' % (i, i)
        return s + b'Content-Type: text/plain\r\n\r\nfondo\r\n' + b''.join(b'--b%d--\r\n' % i for i in range(99, 50, -1))
    if k == 1:   # cabeceras enormes
        return b'Subject: ' + b'a' * 3_000_000 + b'\r\nFrom: ' + b'x ' * 500000 + b'\r\n\r\ncuerpo'
    if k == 2:   # =?...?= sin cerrar, muchas veces
        return b'Subject: ' + b'=?utf-8?q?' * 300000 + b'\r\n' + b'X: =?=?=?' * 100000 + b'\r\n\r\n'
    if k == 3:   # RFC 2231 con 1000 trozos (y repetidos)
        p = b''.join(b'; filename*%d*=%%C3%%B1%d' % (i, i) for i in range(1000))
        return (b'From: a@b.es\r\nContent-Type: application/pdf' + p + b'\r\nContent-Disposition: attachment' + p + p +
                b"; filename*0*=utf-8''x\r\nContent-Transfer-Encoding: base64\r\n\r\nQUJD\r\n")
    if k == 4:   # entidades enormes
        return b'<p>' + b'&#' + b'9' * 100000 + b';' + b'&' + b'a' * 100000 + b';' + b'&#x' + b'f' * 50000 + b'&amp' * 100000
    if k == 5:   # etiquetas anidadas 10000 veces
        t = r.choice([b'<b>', b'<blockquote>', b'<ul><li>', b'<table><tr><td>', b'<a href="http://x.es">', b'<div style="display:none">', b'<pre>', b'<ol start=4294967295><li>'])
        return t * 10000 + b'fondo' + b'</b></li></td>' * 3000
    if k == 6:   # vCard con renglones partidos sin fin
        return b'BEGIN:VCARD\r\nFN:a\r\n' + b' b\r\n' * 300000 + b'NOTE;ENCODING=QUOTED-PRINTABLE:' + b'=41=\r\n' * 200000 + b'\r\nEND:VCARD\r\n'
    if k == 7:   # muchas partes
        return b'Content-Type: multipart/mixed; boundary=z\r\n\r\n' + b'--z\r\nContent-Type: image/png; name=a.png\r\n\r\nx\r\n' * 5000
    if k == 8:   # direcciones y comentarios sin fin
        return b'From: ' + b'(' * 200000 + b'"' * 1001 + b'<' * 100000 + b'a@b, ' * 100000 + b'\r\n\r\n'
    if k == 9:   # comentarios de HTML y raros
        return b'<!--' * 100000 + b'<!' * 100000 + b'<script>' * 1000 + b'</scrip' * 10000 + b'<' * 100000
    if k == 10:  # uuencode y begin sin fin
        return b'Subject: uu\r\n\r\n' + b'begin 644 a.txt\r\nM' + b'!' * 60 + b'\r\n' * 1000 + b'begin 644 b\r\n' * 20000
    # message/rfc822 dentro de rfc822 sin fin
    return b'Content-Type: message/rfc822\r\n\r\n' * 5000 + b'Subject: fondo\r\n\r\nx'


def muta(r, d):
    d = bytearray(d)
    for _ in range(r.randint(1, 12)):
        k = r.randrange(11)
        if not d:
            break
        i = r.randrange(len(d))
        if k == 0:
            d[i] = r.randrange(256)
        elif k == 1:
            del d[i:i + r.randint(1, 200)]
        elif k == 2:
            j = r.randrange(len(d))
            d[i:i] = d[j:j + r.randint(1, 400)] * r.randint(1, 20)
        elif k == 3:
            d = d[:r.randrange(len(d))]
        elif k == 4:   # una frontera cambiada (o quitada, o cerrada antes)
            m = list(re.finditer(rb'(?m)^--[^\r\n]+', bytes(d)))
            if m:
                x = r.choice(m)
                d[x.start():x.end()] = r.choice([b'--', b'--' + bytes(d[x.start() + 2:x.end()]) + b'--', b'--x', b'', bytes(d[x.start():x.end() - 1])])
        elif k == 5:
            d[i:i] = r.choice([b'\r\n', b'\n\n', b'\r\n\r\n', b'--', b'=?utf-8?B?', b'?=', b'=?iso-8859-1?q?=E9', b'"', b'<', b'>', b'(', b')',
                               b';', b'\\', b'=', b'*0*=', b"utf-8''%", b'&', b'&#', b'<a href="', b'<img src=', b'<!--', b'-->', b'<pre>',
                               b'</', b'\xc3', b'\xff', b'\x00', b'begin 644 x\n', b'BEGIN:VCARD\n', b'END:VCARD\n', b'\n ', b'=\n'])
        elif k == 6:
            for j in range(i, min(len(d), i + 64)):
                d[j] ^= 1 << r.randrange(8)
        elif k == 7:   # un número cambiado
            m = list(re.finditer(rb'\d+', bytes(d[max(0, i - 2000):i + 2000])))
            if m:
                x = r.choice(m)
                a = max(0, i - 2000) + x.start()
                d[a:a + len(x.group())] = str(r.choice([0, 1, 2**31 - 1, 2**32, 99999999999, r.randrange(10**6)])).encode()
        elif k == 8:   # una cabecera de MIME cambiada
            d[i:i] = r.choice([b'\nContent-Type: multipart/alternative; boundary=', b'\nContent-Transfer-Encoding: base64\n',
                               b'\nContent-Transfer-Encoding: quoted-printable\n', b'\nContent-Type: message/rfc822\n\n',
                               b'\nContent-Type: text/plain; charset=koi8-r; format=flowed; delsp=yes\n', b'\nContent-Transfer-Encoding: x-uuencode\n'])
        elif k == 9:   # repetir todo
            d = d * r.randint(2, 6)
        else:
            d[i:i] = bytes(r.randrange(256) for _ in range(r.randint(1, 30)))
    return bytes(d[:4_000_000])


def una(v):
    r = random.Random(semilla * 1000003 + v)
    datos = hostil(r) if r.random() < 0.08 else muta(r, r.choice(semillas))
    f = W + '/v%d.dat' % v
    open(f, 'wb').write(datos)
    malo = None
    for orden in ORDENES:
        try:
            x = subprocess.run([prog, orden, f], capture_output=True, timeout=TOPE,
                               env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1', UBSAN_OPTIONS='print_stacktrace=1'))
            if x.returncode not in (0, 2) or b'Sanitizer' in x.stderr or b'runtime error' in x.stderr:
                malo = (orden, 'sale con %d: ' % x.returncode + x.stderr.decode('latin-1')[-3000:])
        except subprocess.TimeoutExpired:
            malo = (orden, 'tarda más de %d s (¿cuadrático?)' % TOPE)
        if malo:
            break
    if malo:
        n = W + '/fallos/s%d_v%d_%s.dat' % (semilla, v, malo[0])
        open(n, 'wb').write(datos)
        open(n + '.txt', 'w').write(malo[1])
        os.remove(f)
        return '  FAIL %s (%s): %s' % (n, malo[0], malo[1][:300].replace('\n', ' | '))
    os.remove(f)
    return None


fallos = 0
with ThreadPoolExecutor(hilos) as ex:
    for res in ex.map(una, range(vueltas)):
        if res:
            fallos += 1
            print(res, flush=True)
print('%s fuzz: %d vueltas (semilla %d), %d fallos' % ('  OK  ' if not fallos else '  FAIL', vueltas, semilla, fallos))
sys.exit(1 if fallos else 0)
