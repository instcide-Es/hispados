#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  Hispa-DOS - tools/firma_versiones.py: firmar versiones.txt para ACTUALIZAR
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  Desde la 0.9.31, ACTUALIZAR (utils/actualizar.asm) solo instala lo que
#  diga un versiones.txt firmado con la clave de Hispa-DOS (Ed25519, la misma
#  que la de Hispa-Pentium: lib/clave_firma.inc = castel/nucleo32/clave.cas),
#  y solo un paquete cuyo SHA-256 y tamaño cuadren con los que dice él.
#  (docs/RED.md, «La firma de versiones.txt».)
#
#  Lo que hace «firma»:
#    1. En cada sección con «paquete=URL», busca el archivo (el último trozo
#       de la URL) en la carpeta de los paquetes (por omisión, la de
#       versiones.txt), y pone detrás de «paquete=» sus líneas
#           sha256=<64 cifras hex>
#           octetos=<tamaño>
#       (quita las que hubiera). Un ACTUALIZAR de antes (0.9.30 o anterior)
#       no conoce esas claves y no las mira.
#    2. Quita la línea «firma=» que hubiera y firma lo que queda: Ed25519 de
#           b"HISPADOS-VERSIONES 1\n" + el archivo, hasta la línea de la firma
#       (la etiqueta, para que la firma de otra cosa, como un catálogo de
#       Hispa-Pentium, no valga aquí).
#    3. Pone al final la línea «firma=ed25519 <128 cifras hex>».
#  Todo lo que ACTUALIZAR lee va por delante de «firma=»; lo de detrás no lo
#  mira. El archivo entero tiene que caber en 4095 octetos (BODY_MAX).
#
#  Uso:
#    firma_versiones.py firma VERSIONES.TXT --clave PRIVADA [--paquetes CARPETA]
#                       [-o SALIDA]        (sin -o, lo reescribe en el sitio)
#    firma_versiones.py comprueba VERSIONES.TXT [--publica HEX|clave.inc|clave.cas]
#                       [--paquetes CARPETA]   (la firma, y las sumas si están
#                       los paquetes; por omisión, la clave de lib/clave_firma.inc)
#    firma_versiones.py sha ARCHIVO...      (el SHA-256 y el tamaño, para la web)
#
#  PRIVADA es un archivo con las 64 cifras hex de la semilla (el formato de
#  «tools/hpq.py clave»). La clave de verdad la tiene Carlos: nunca va en el
#  repositorio. Para las pruebas: tests/firma/clave_prueba.key (su pública,
#  tests/firma/clave_prueba.inc, solo la lleva build/ACTPRUEB.COM).
# =============================================================================

import hashlib
import os
import re
import sys

ETIQUETA = b'HISPADOS-VERSIONES 1\n'
BODY_MAX = 4096
AQUI = os.path.dirname(os.path.abspath(__file__))
CLAVE_REAL = os.path.join(AQUI, '..', 'lib', 'clave_firma.inc')


def falla(m):
    sys.stderr.write('firma_versiones: %s\n' % m)
    sys.exit(1)


def _ed():
    try:
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.hazmat.primitives import serialization
    except ImportError:
        falla('hace falta el módulo «cryptography» de Python (python3-cryptography)')
    return ed25519, serialization


def lee_privada(ruta):
    ed25519, _ = _ed()
    t = open(ruta).read().strip()
    if not re.match(r'^[0-9a-f]{64}$', t):
        falla('%s no es una clave privada (64 cifras hexadecimales, como las de hpq.py)' % ruta)
    return ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(t))


def lee_publica(texto):
    if texto is None:
        texto = CLAVE_REAL
    if os.path.isfile(texto):
        t = open(texto, encoding='utf-8').read()
        if 'pública' in t:          # un clave.cas de Castel
            t = t.split('pública')[-1]
        elif 'clave_act:' in t:     # un .inc de NASM
            t = t.split('clave_act:')[-1]
        b = re.findall(r'0x([0-9A-Fa-f]{2})', t)[:32]
        pub = bytes(int(x, 16) for x in b)
    else:
        pub = bytes.fromhex(texto)
    if len(pub) != 32:
        falla('la clave pública tiene que tener 32 octetos')
    return pub


def sha(ruta):
    h = hashlib.sha256()
    n = 0
    with open(ruta, 'rb') as f:
        while True:
            b = f.read(1 << 20)
            if not b:
                break
            h.update(b)
            n += len(b)
    return h.hexdigest(), n


def lineas(datos):
    """Las líneas, con su fin de línea (CR LF o LF) tal cual."""
    return datos.splitlines(keepends=True)


def clave_de(linea):
    m = re.match(rb'^[ \t]*([A-Za-z0-9_]+)[ \t]*=', linea)
    return m.group(1).lower().decode() if m else None


def valor_de(linea):
    return linea.split(b'=', 1)[1].strip()


def parte_firma(datos):
    """(lo firmado, la firma en bytes o None): lo firmado acaba donde empieza
    la primera línea «firma=» (como lo busca ACTUALIZAR)."""
    pos = 0
    for l in lineas(datos):
        if clave_de(l) == 'firma':
            v = valor_de(l).split()
            if len(v) != 2 or v[0].lower() != b'ed25519' or not re.match(rb'^[0-9A-Fa-f]{128}$', v[1]):
                return datos[:pos], None
            return datos[:pos], bytes.fromhex(v[1].decode())
        pos += len(l)
    return datos, None


def pon_sumas(datos, carpeta):
    fin = b'\r\n' if b'\r\n' in datos else b'\n'
    sale = []
    faltan = []
    for l in lineas(datos):
        k = clave_de(l)
        if k in ('sha256', 'octetos', 'firma'):
            continue
        sale.append(l if l.endswith(b'\n') else l + fin)
        if k == 'paquete':
            url = valor_de(l).decode('latin-1')
            nombre = url.rstrip('/').split('/')[-1]
            ruta = os.path.join(carpeta, nombre)
            if not os.path.isfile(ruta):
                faltan.append(ruta)
                continue
            h, n = sha(ruta)
            sale.append(b'sha256=' + h.encode() + fin)
            sale.append(b'octetos=%d' % n + fin)
    if faltan:
        falla('no están los paquetes: ' + ', '.join(faltan))
    return b''.join(sale), fin


def firma(ruta, privada, carpeta, salida):
    k = lee_privada(privada)
    datos = open(ruta, 'rb').read()
    cuerpo, fin = pon_sumas(datos, carpeta or os.path.dirname(os.path.abspath(ruta)))
    f = k.sign(ETIQUETA + cuerpo)
    todo = cuerpo + b'firma=ed25519 ' + f.hex().encode() + fin
    if len(todo) > BODY_MAX - 1:
        falla('versiones.txt firmado ocupa %d octetos: ACTUALIZAR solo lee %d' % (len(todo), BODY_MAX - 1))
    with open(salida or ruta, 'wb') as s:
        s.write(todo)
    print('firmado: %s (%d octetos; clave %s...)' % (salida or ruta, len(todo), publica_de(k).hex()[:16]))


def publica_de(k):
    _, serialization = _ed()
    return k.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def comprueba(ruta, publica, carpeta):
    ed25519, _ = _ed()
    pub = lee_publica(publica)
    datos = open(ruta, 'rb').read()
    if len(datos) > BODY_MAX - 1:
        print('MAL: ocupa %d octetos (más de %d)' % (len(datos), BODY_MAX - 1))
        return 1
    firmado, f = parte_firma(datos)
    if f is None:
        print('MAL: no lleva la línea «firma=ed25519 ...»')
        return 1
    try:
        ed25519.Ed25519PublicKey.from_public_bytes(pub).verify(f, ETIQUETA + firmado)
    except Exception:
        print('MAL: la firma no es buena con la clave %s...' % pub.hex()[:16])
        return 1
    print('firma buena (clave %s...)' % pub.hex()[:16])
    mal = 0
    if carpeta:
        cur = {}
        for l in lineas(firmado) + [b'[fin]']:
            if l.strip().startswith(b'['):
                if 'paquete' in cur:
                    nombre = cur['paquete'].rstrip('/').split('/')[-1]
                    r = os.path.join(carpeta, nombre)
                    if os.path.isfile(r):
                        h, n = sha(r)
                        bien = h == cur.get('sha256') and str(n) == cur.get('octetos')
                        print('  %s: %s' % (nombre, 'cuadra' if bien else 'NO CUADRA'))
                        mal |= not bien
                cur = {}
            k = clave_de(l)
            if k:
                cur[k] = valor_de(l).decode('latin-1')
    return 1 if mal else 0


def principal(a):
    if len(a) < 3:
        sys.stderr.write(open(__file__, encoding='utf-8').read().split('# Uso:')[1].split('# PRIVADA')[0])
        return 2
    orden, ruta = a[1], a[2]
    op = {'--clave': None, '--paquetes': None, '-o': None, '--publica': None}
    i = 3
    while i < len(a):
        if a[i] in op and i + 1 < len(a):
            op[a[i]] = a[i + 1]
            i += 2
        elif orden == 'sha':
            i += 1
        else:
            falla('no sé qué es «%s»' % a[i])
    if orden == 'firma':
        if not op['--clave']:
            falla('falta --clave RUTA (la privada)')
        firma(ruta, op['--clave'], op['--paquetes'], op['-o'])
        return 0
    if orden == 'comprueba':
        return comprueba(ruta, op['--publica'], op['--paquetes'])
    if orden == 'sha':
        for r in a[2:]:
            h, n = sha(r)
            print('%s  %s  (%d octetos)' % (h, os.path.basename(r), n))
        return 0
    falla('orden desconocida: %s (firma, comprueba, sha)' % orden)


if __name__ == '__main__':
    sys.exit(principal(sys.argv))
