#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""blocprueba/prueba_pag.py BIN CARPETA: prueba BlocPag (PPAG.BIN, con corre32)
contra lo que calcula Python: las tablas, el fuzz de ida y vuelta (FUZZ_PAG
vueltas, semilla fija), recorrer UTF-8 roto, adivinar la página y el tiempo."""
import os, random, struct, subprocess, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', '..', '..', 'tools'))
import hispa437  # noqa: E402

BIN, D = sys.argv[1], sys.argv[2]
R = os.path.join(AQUI, '..', '..', 'corre32', 'corre32.sh')
HUECOS = (0x81, 0x8D, 0x8F, 0x90, 0x9D)
bien = mal = 0


def ok(que):
    global bien
    bien += 1
    print('bien: ' + que)


def ko(que):
    global mal
    mal += 1
    print('MAL: ' + que)


def de_437(c):
    return ord(hispa437.letra(c))


def de_1252(c):
    return c if c in HUECOS else ord(bytes([c]).decode('cp1252'))


A437 = {de_437(c): c for c in range(256)}
A1252 = {de_1252(c): c for c in range(256)}


def corre(*args):
    return subprocess.run([R, BIN] + list(args), cwd=D, capture_output=True).stdout.decode('cp437')


# ---- lo de referencia ----
LARGO = {}
for b in range(0xC2, 0xE0): LARGO[b] = 2
for b in range(0xE0, 0xF0): LARGO[b] = 3
for b in range(0xF0, 0xF5): LARGO[b] = 4


def deco(d, i):
    """(octetos, carácter) como pag_decodifica"""
    b = d[i]
    if b < 0x80:
        return 1, b
    L = LARGO.get(b)
    if L and i + L <= len(d):
        try:
            ch = d[i:i + L].decode('utf-8')
            if len(ch) == 1:
                return L, ord(ch)
        except UnicodeDecodeError:
            pass
    if b == 0xED and i + 3 <= len(d) and d[i + 1] in (0xB2, 0xB3) and 0x80 <= d[i + 2] <= 0xBF:
        return 3, 0xD000 | ((d[i + 1] & 0x3F) << 6) | (d[i + 2] & 0x3F)
    return 1, 0xDC00 + b


def adentro(cp, d):
    if cp == 0:
        return ''.join(chr(de_437(b)) for b in d).encode('utf-8', 'surrogatepass')
    if cp == 2:
        return ''.join(chr(de_1252(b)) for b in d).encode('utf-8', 'surrogatepass')
    return d.decode('utf-8', 'surrogateescape').encode('utf-8', 'surrogatepass')


def afuera(cp, d):
    o, perd, i = bytearray(), 0, 0
    while i < len(d):
        k, u = deco(d, i)
        if 0xDC80 <= u <= 0xDCFF:
            o.append(u - 0xDC00)
        elif cp == 1:
            o += d[i:i + k]
        else:
            c = (A1252 if cp == 2 else A437).get(u)
            if c is None or (cp == 0 and 0x80 <= u < 0xA0):
                c = ord('?')
                perd += 1
            o.append(c)
        i += k
    return bytes(o), perd


# ---- 1. las tablas ----
sal = corre('TABLAS').splitlines()
malas = []
for c in range(256):
    p = sal[c].split() if c < len(sal) else []
    esp = ['437', str(c), str(de_437(c)), str(c), '1252', str(de_1252(c)), '1', str(c), '0']
    if p != esp:
        malas.append('%02X: %s (y es %s)' % (c, p, esp))
if malas:
    ko('las tablas: ' + '; '.join(malas[:5]))
else:
    ok('las 256 de la 437 de casa y de la 1252 (con sus cinco huecos), a Unicode y de vuelta, como hispa437.py y el cp1252 de Python; pag_letra pinta cada una con su código')

# ---- 2. los casos al azar ----
vueltas = int(os.environ.get('FUZZ_PAG', '3000'))
rnd = random.Random(20261122)
TROZOS = [b'\xc0\x80', b'\xe0\x80\x80', b'\xf0\x80\x80\x80', b'\xed\xa0\x80', b'\xed\xb2\x80', b'\xed\xb3\xbf',
          b'\xf4\x90\x80\x80', b'\xf5\x80', b'\xff', b'\xfe', b'\xc3', b'\xe2\x82', b'\xf0\x9f\x98', b'\x80', b'\xbf',
          'ñ'.encode(), '€'.encode(), '😀'.encode(), '中'.encode(), b'\xef\xbb\xbf', b'\r\n', b'\x00', b'\x1a']


def al_azar():
    t = rnd.randrange(4)
    n = rnd.choice((0, 1, 2, 3, 5, 17, rnd.randrange(300), rnd.randrange(3000)))
    if t == 0:
        return bytes(rnd.randrange(256) for _ in range(n))
    o = bytearray()
    while len(o) < n:
        x = rnd.randrange(10)
        if x < 4:
            o += bytes(rnd.choice(b'abc xyz\n') for _ in range(rnd.randrange(6)))
        elif x < 8:
            o += rnd.choice(TROZOS)
        else:
            o += chr(rnd.choice((rnd.randrange(0x80, 0x800), rnd.randrange(0x800, 0xD800), rnd.randrange(0xE000, 0x10000),
                                 rnd.randrange(0x10000, 0x110000)))).encode()
    if t == 2 and o:
        o = o[:rnd.randrange(len(o))]           # cortado
    return bytes(o)


def texto_dentro():
    s = []
    for _ in range(rnd.randrange(200)):
        x = rnd.randrange(8)
        if x < 3:
            s.append(chr(rnd.randrange(32, 127)))
        elif x == 3:
            s.append(chr(rnd.randrange(0xA0, 0x100)))
        elif x == 4:
            s.append(hispa437.letra(rnd.randrange(128, 256)))
        elif x == 5:
            s.append(chr(rnd.randrange(0xDC80, 0xDD00)))
        elif x == 6:
            s.append(chr(rnd.choice((0x4E2D, 0x1F600, 0x20AC, 0x2026, 0x201C, 0x0153, 0x85))))
        else:
            s.append(bytes([rnd.choice((0x81, 0x8D, 0x90, 0x9D, 0x99))]).decode('latin-1'))
    return ''.join(s).encode('utf-8', 'surrogatepass')


casos = []
for v in range(vueltas):
    d = al_azar()
    casos.append((rnd.choice((0, 1, 1, 2)), d))
    casos.append((0x40, d))
    casos.append((0x80 + rnd.randrange(3), texto_dentro() if v % 3 else d))
    if v % 10 == 0:
        casos.append((0x42, d))
        casos.append((0x43, d))
with open(os.path.join(D, 'casos.bin'), 'wb') as f:
    for c, d in casos:
        f.write(struct.pack('<BI', c, len(d)) + d)
r = corre('CASOS', 'casos.bin', 'salida.bin')
if 'malos (largos que no cuadran): 0' in r:
    ok('pag_largo_utf8 y pag_largo_de_utf8 dicen justo lo que escriben pag_a_utf8 y pag_de_utf8 (%d casos)' % len(casos))
else:
    ko('los largos no cuadran: ' + r.strip())
s = open(os.path.join(D, 'salida.bin'), 'rb').read()
pos = 0


def toma():
    global pos
    n = struct.unpack_from('<I', s, pos)[0]
    pos += 4
    d = s[pos:pos + n]
    pos += n
    return d


def toma32():
    global pos
    v = struct.unpack_from('<I', s, pos)[0]
    pos += 4
    return v


fallos = {}
cuenta = {}


def apunta(cl, que):
    fallos.setdefault(cl, []).append(que)


try:
    for c, d in casos:
        cuenta[c & 0xF0 if c >= 0x80 else c] = cuenta.get(c & 0xF0 if c >= 0x80 else c, 0) + 1
        if c <= 2:
            dentro, fuera, perd = toma(), toma(), toma32()
            if dentro != adentro(c, d):
                apunta('ida', '%d %r -> %r (y es %r)' % (c, d[:40], dentro[:60], adentro(c, d)[:60]))
            elif fuera != d or perd:
                apunta('ida', 'la vuelta de %r da %r (%d perdidas)' % (d[:40], fuera[:40], perd))
        elif c >= 0x80:
            fuera, perd = toma(), toma32()
            if (fuera, perd) != afuera(c - 0x80, d):
                apunta('vuelta', '%d %r -> %r %d (y es %r)' % (c - 0x80, d[:40], fuera[:40], perd, afuera(c - 0x80, d)))
        elif c == 0x40:
            lista, m = toma(), toma32()
            esp, i = [], 0
            while i < len(d):
                k, u = deco(d, i)
                esp.append((k, u))
                i += k
            dado = [struct.unpack_from('<BI', lista, j) for j in range(0, len(lista), 5)]
            if dado != esp:
                apunta('deco', '%r: %r (y es %r)' % (d[:30], dado[:8], esp[:8]))
            if m:
                apunta('anterior', '%r: %d sitios donde pag_anterior no da el de antes' % (d[:30], m))
        elif c == 0x42:
            t = toma()
            esp = afuera(0, d)[0]
            # (en pag_a_437 los escapados y lo que no está son «?»)
            o, i = bytearray(), 0
            while i < len(d):
                k, u = deco(d, i)
                cc = A437.get(u)
                o.append(ord('?') if cc is None or 0x80 <= u < 0xA0 or 0xDC80 <= u <= 0xDCFF else cc)
                i += k
            if t != bytes(o):
                apunta('a437', '%r -> %r (y es %r)' % (d[:30], t[:30], bytes(o)[:30]))
        elif c == 0x43:
            t = toma()
            if t != adentro(0, d):
                apunta('de437', '%r' % d[:30])
except struct.error:
    ko('la salida se acaba antes de tiempo')
for cl, que in (('ida', 'de fuera adentro y de vuelta (la 437, UTF-8 con lo roto escapado y la 1252): igual que Python (surrogateescape) y la vuelta, idéntica octeto a octeto'),
                ('vuelta', 'de dentro afuera en las tres (lo que no está, «?» y contado; los escapados, su octeto)'),
                ('deco', 'pag_decodifica, octeto a octeto sobre UTF-8 roto, cortado, demasiado largo, sustitutos y > 10FFFF: como Python, y nunca gasta más de lo que hay'),
                ('anterior', 'pag_anterior da siempre el comienzo del carácter de antes'),
                ('a437', 'pag_a_437 (para enseñar), con «?»'), ('de437', 'pag_437_a_utf8')):
    if cl in fallos:
        ko(que + ': ' + '; '.join(fallos[cl][:3]) + ' (%d fallos)' % len(fallos[cl]))
    else:
        ok(que)
print('      (%d vueltas: %d casos)' % (vueltas, len(casos)))

# ---- 3. adivinar la página ----
ES = ('¿Qué tal está la señora? Añoranza, corazón, camión y pingüino; «comillas», ¡olé!\r\n'
      'El niño comió paella en Cádiz. Ésta es la canción del verano, ¿o no? Nº 1, 2ª vez.\r\n') * 3
def en437(t):
    return bytes(A437[ord(ch)] for ch in t)
pruebas = [('español en la 437 de casa', en437(ES), 0, 0), ('español en la 1252', ES.encode('cp1252'), 2, 0),
           ('español en UTF-8', ES.encode(), 1, 0), ('español en UTF-8 con BOM', b'\xef\xbb\xbf' + ES.encode(), 1, 1),
           ('solo ASCII', b'hola, que tal\r\n' * 10, 0, 0), ('vacío', b'', 0, 0),
           ('un recuadro de la 437 (DOS)', en437('┌──────┐\r\n│ Menú │\r\n└──────┘\r\n'), 0, 0),
           ('una palabra suelta en la 1252: «año»', 'el año'.encode('cp1252'), 2, 0),
           ('una palabra suelta en la 437: «año»', en437('el año'), 0, 0),
           ('comillas tipográficas y € en la 1252', '“Precio”: 10 € — oferta'.encode('cp1252'), 2, 0),
           ('UTF-8 roto al final (cortado): no es UTF-8', ES.encode()[:-1] + b'\xc3', None, 0)]
with open(os.path.join(D, 'det.bin'), 'wb') as f:
    for _, d, _, _ in pruebas:
        f.write(struct.pack('<BI', 0x41, len(d)) + d)
corre('CASOS', 'det.bin', 'det.sal')
s = open(os.path.join(D, 'det.sal'), 'rb').read()
pos = 0
malas = []
for que, d, cp, bom in pruebas:
    a, b = toma32(), toma32()
    if (cp is not None and a != cp) or (cp is None and a == 1) or b != bom:
        malas.append('%s: %d %d' % (que, a, b))
if malas:
    ko('adivinar la página: ' + '; '.join(malas))
else:
    ok('adivinar la página: ' + ', '.join(p[0] for p in pruebas))

# ---- 4. lo que tarda ----
for l in corre('TIEMPO').splitlines():
    print('      ' + l)
print('== %d bien, %d mal ==' % (bien, mal))
sys.exit(1 if mal else 0)
