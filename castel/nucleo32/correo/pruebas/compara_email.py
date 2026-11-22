#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas/compara_email.py PROGRAMA ARCHIVO...: compara lo que saca
# mime_analiza (el texto de la principal y los nombres de los adjuntos) con
# lo del módulo email de Python, para cada mensaje. Saca «  OK   ...» o
# «  FAIL ...» por mensaje. (Lo que Python no hace igual a propósito, fuera:
# format=flowed, uuencode, las firmas, las imágenes de un related, las rutas
# de Windows con «\\» en el nombre y lo de dentro de un message/rfc822.)
import email, email.policy, os, re, subprocess, sys

prog = sys.argv[1]


def py(datos):
    m = email.message_from_bytes(datos, policy=email.policy.default)
    texto = None
    try:
        b = m.get_body(preferencelist=('plain',))
        if b is not None and b.get_content_type() == 'text/plain' and b.get_param('format', '').lower() != 'flowed':
            texto = b.get_content()
    except Exception:
        texto = None
    nombres = []
    pila = [(m, '')]
    while pila:
        p, padre = pila.pop(0)
        t = p.get_content_type()
        if t == 'message/rfc822' and p is not m:
            continue
        if p.is_multipart():
            pila[0:0] = [(h, t) for h in p.iter_parts()]
            continue
        if 'signature' in t:
            continue
        if padre == 'multipart/related' and p.get('content-id') and p.get_content_disposition() != 'attachment':
            continue
        try:
            f = p.get_filename()
        except Exception:
            f = None
        if f:
            f = re.split(r'[/\\]', f)[-1]
            nombres.append(''.join('_' if ord(c) < 32 or c == '\x7f' else c for c in f))
    return texto, nombres


def norm(t):
    return '\n'.join(r.rstrip() for r in t.replace('\r\n', '\n').replace('\r', '\n').split('\n')).rstrip()


fallos = 0
for a in sys.argv[2:]:
    datos = open(a, 'rb').read()
    try:
        texto, nombres = py(datos)
    except Exception as e:
        print('  OK   %s (Python no lo entiende: %s)' % (os.path.basename(a), type(e).__name__))
        continue
    r1 = subprocess.run([prog, 'principal', a], capture_output=True, timeout=60)
    r2 = subprocess.run([prog, 'nombres', a], capture_output=True, timeout=60)
    if r1.returncode or r2.returncode:
        print('  FAIL %s (el programa sale con %d/%d)' % (os.path.basename(a), r1.returncode, r2.returncode))
        fallos += 1
        continue
    mios = [x for x in r2.stdout.decode('utf-8', 'replace').split('\n') if x]
    malo = []
    uu = set(re.findall(rb'^begin [0-7]{3,4} (.+?)\r?$', datos, re.M))
    if texto is not None and not uu and '\ufffd' not in texto and norm(texto) != norm(r1.stdout.decode('utf-8', 'replace')):
        malo.append('el texto no es igual')
    if re.search(rb'name\*?=[^;\r\n]*\\', datos):
        nombres = mios = []	# (las rutas de Windows: Python quita las «\\»)
    for n in nombres:
        if n not in mios:
            malo.append('falta el adjunto «%s»' % n)
    for n in mios:
        if n not in nombres and not n.endswith('.eml') and n.encode() not in uu:
            malo.append('adjunto de más «%s»' % n)
    if malo:
        fallos += 1
        print('  FAIL %s: %s' % (os.path.basename(a), '; '.join(malo)))
    else:
        print('  OK   %s (como el email de Python)' % os.path.basename(a))
sys.exit(1 if fallos else 0)
