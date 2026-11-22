#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas/haz_pdf.py SALIDA.PDF LETRA PAGINAS [objstm]: un PDF de prueba
# hecho a mano (sin nada de fuera): cada página dice «Documento LETRA,
# página N» (con eñe y tildes, en WinAnsi) en Helvetica, con un recuadro de
# color y un degradado de grises; con «objstm», los objetos en flujos de
# objetos y la tabla en un flujo (PDF 1.5), comprimidos.
import sys, zlib

sal, letra, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
objstm = len(sys.argv) > 4
colores = ['0.11 0.31 0.85', '0.71 0.33 0.04', '0.09 0.4 0.2', '0.6 0.1 0.1']
objs = {}
objs[1] = b'<< /Type /Catalog /Pages 2 0 R >>'
kids = []
fuente = 3
objs[fuente] = b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>'
k = 4
for p in range(1, n + 1):
    texto = ('Documento %s, p\xe1gina %d' % (letra, p)).encode('cp1252')
    c = (b'q %s rg 60 600 480 160 re f Q\n' % colores[p % 4].encode() +
         b''.join(b'%.2f g %d 100 40 300 re f\n' % (i / 10, 60 + i * 48) for i in range(10)) +
         b'BT /F1 36 Tf 70 660 Td 1 g (' + texto + b') Tj ET\n' +
         b'BT /F1 14 Tf 60 560 Td 0 g (Hispa-PDF: prueba de Castel y C, con e\xf1e) Tj ET\n')
    z = zlib.compress(c)
    objs[k] = b'<< /Length %d /Filter /FlateDecode >>\nstream\n' % len(z) + z + b'\nendstream'
    objs[k + 1] = b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents %d 0 R >>' % k
    kids.append(k + 1)
    k += 2
objs[2] = b'<< /Type /Pages /Kids [%s] /Count %d >>' % (b' '.join(b'%d 0 R' % x for x in kids), n)
info = k
objs[info] = b'<< /Title (Prueba %s) /Producer (haz_pdf.py) >>' % letra.encode()
total = info + 1
out = bytearray(b'%PDF-1.5\n%\xe2\xe3\xcf\xd3\n')
pos = {}
if not objstm:
    for i in range(1, total):
        pos[i] = len(out)
        out += b'%d 0 obj\n' % i + objs[i] + b'\nendobj\n'
    x = len(out)
    out += b'xref\n0 %d\n0000000000 65535 f \n' % total + b''.join(b'%010d 00000 n \n' % pos[i] for i in range(1, total))
    out += b'trailer\n<< /Size %d /Root 1 0 R /Info %d 0 R >>\nstartxref\n%d\n%%%%EOF\n' % (total, info, x)
else:
    sueltos = [i for i in range(1, total) if b'stream' not in objs[i]]
    cab = b''
    cuerpo = b''
    for i in sueltos:
        cab += b'%d %d ' % (i, len(cuerpo))
        cuerpo += objs[i] + b'\n'
    z = zlib.compress(cab + cuerpo)
    ost = total
    for i in range(1, total):
        if i not in sueltos:
            pos[i] = len(out)
            out += b'%d 0 obj\n' % i + objs[i] + b'\nendobj\n'
    pos[ost] = len(out)
    out += b'%d 0 obj\n<< /Type /ObjStm /N %d /First %d /Length %d /Filter /FlateDecode >>\nstream\n' % (ost, len(sueltos), len(cab), len(z)) + z + b'\nendstream\nendobj\n'
    xr = ost + 1
    filas = b''
    for i in range(0, xr + 1):
        if i == 0:
            filas += bytes([0, 0, 0, 0, 0xff])
        elif i in sueltos:
            filas += bytes([2]) + ost.to_bytes(3, 'big') + bytes([sueltos.index(i)])
        elif i == xr:
            filas += bytes([1]) + len(out).to_bytes(3, 'big') + bytes([0])
        else:
            filas += bytes([1]) + pos[i].to_bytes(3, 'big') + bytes([0])
    z = zlib.compress(filas)
    x = len(out)
    out += b'%d 0 obj\n<< /Type /XRef /Size %d /W [1 3 1] /Root 1 0 R /Info %d 0 R /Length %d /Filter /FlateDecode >>\nstream\n' % (xr, xr + 1, info, len(z)) + z + b'\nendstream\nendobj\n'
    out += b'startxref\n%d\n%%%%EOF\n' % x
open(sal, 'wb').write(out)
