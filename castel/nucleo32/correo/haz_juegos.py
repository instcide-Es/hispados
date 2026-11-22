#!/usr/bin/env python3
# haz_juegos.py [SALIDA] - Correo: hace juegos_tablas.h, las tablas de los
# juegos de caracteres de 8 bits (de los codecs de Python), sus alias, la
# página 437 de Hispa (tools/hispa437.py) con lo más parecido para lo que no
# tiene, y las letras «planas» (minúsculas sin tildes) para buscar.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import os, sys, unicodedata

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', '..', '..', 'tools'))
import hispa437  # noqa: E402

SAL = sys.argv[1] if len(sys.argv) > 1 else os.path.join(AQUI, 'juegos_tablas.h')

# (nombre de la tabla, codec de Python, alias normalizados: en minúsculas y
# solo letras y cifras, sin «x-» delante ni lo de detrás de «:»)
JUEGOS = [
    ('w1252', 'cp1252', ['windows1252', 'cp1252', 'win1252', 'ms1252', 'ansi',
                         # WHATWG: iso-8859-1 y us-ascii se leen como 1252
                         'iso88591', 'latin1', 'l1', 'cp819', 'ibm819', 'iso8859', 'csisolatin1', 'iso885911987']),
    ('i2', 'iso8859_2', ['iso88592', 'latin2', 'l2', 'csisolatin2']),
    ('i3', 'iso8859_3', ['iso88593', 'latin3', 'l3']),
    ('i4', 'iso8859_4', ['iso88594', 'latin4', 'l4']),
    ('i5', 'iso8859_5', ['iso88595', 'cyrillic', 'csisolatincyrillic']),
    ('i6', 'iso8859_6', ['iso88596', 'arabic']),
    ('i7', 'iso8859_7', ['iso88597', 'greek', 'greek8', 'elot928']),
    ('i8', 'iso8859_8', ['iso88598', 'hebrew', 'iso88598i', 'iso88598e']),
    ('i9', 'cp1254', ['iso88599', 'latin5', 'l5']),	# WHATWG: como 1254
    ('i10', 'iso8859_10', ['iso885910', 'latin6', 'l6']),
    ('i11', 'iso8859_11', ['iso885911', 'tis620']),
    ('i13', 'iso8859_13', ['iso885913', 'latin7', 'l7']),
    ('i14', 'iso8859_14', ['iso885914', 'latin8', 'l8']),
    ('i15', 'iso8859_15', ['iso885915', 'latin9', 'latin0', 'l9']),
    ('i16', 'iso8859_16', ['iso885916', 'latin10', 'l10']),
    ('w1250', 'cp1250', ['windows1250', 'cp1250', 'win1250']),
    ('w1251', 'cp1251', ['windows1251', 'cp1251', 'win1251']),
    ('w1253', 'cp1253', ['windows1253', 'cp1253']),
    ('w1254', 'cp1254', ['windows1254', 'cp1254']),
    ('w1255', 'cp1255', ['windows1255', 'cp1255']),
    ('w1256', 'cp1256', ['windows1256', 'cp1256']),
    ('w1257', 'cp1257', ['windows1257', 'cp1257']),
    ('w1258', 'cp1258', ['windows1258', 'cp1258']),
    ('c437', 'cp437', ['ibm437', 'cp437', '437', 'csibm437', 'pc8']),
    ('c850', 'cp850', ['ibm850', 'cp850', '850']),
    ('c852', 'cp852', ['ibm852', 'cp852', '852']),
    ('c866', 'cp866', ['ibm866', 'cp866', '866']),
    ('k8r', 'koi8_r', ['koi8r', 'koi8', 'cskoi8r']),
    ('k8u', 'koi8_u', ['koi8u']),
    ('mac', 'mac_roman', ['macintosh', 'mac', 'macroman', 'csmacintosh']),
]
UTF8 = ['utf8', 'unicode11utf8', 'unicode20utf8', 'utf']
# us-ascii: si lleva 8 bits, UTF-8 si lo es y si no la 1252
ASCII = ['usascii', 'ascii', 'ansix341968', 'iso646us', 'us', '646', 'csascii', 'ibm367', 'cp367']


def tabla(codec):
    v = []
    for b in range(0x80, 0x100):
        try:
            u = bytes([b]).decode(codec)
            v.append(ord(u) if len(u) == 1 and ord(u) < 0x10000 else 0xFFFD)
        except UnicodeDecodeError:
            # lo que no está: como en WHATWG, el control C1 de ese número
            v.append(b if codec == 'cp1252' and b < 0xA0 else 0xFFFD)
    return v


# ---- la 437 de Hispa: de Unicode a los octetos de Ventanas ----
A_MANO = {
    ' ': ' ', '­': '', '‘': "'", '’': "'", '‚': "'", '‛': "'",
    '′': "'", '‵': "'", '“': '"', '”': '"', '„': '"', '‟': '"',
    '″': '"', '«': None, '»': None, '‐': '-', '‑': '-', '‒': '-',
    '–': '-', '—': '-', '―': '-', '−': '-', '•': '·', '‣': '>',
    '⁃': '-', '‹': '<', '›': '>', '™': 'TM', '…': None, '‰': '%o',
    '†': '+', '‡': '+', 'ˆ': '^', '˜': '~', '​': '', '‌': '',
    '‍': '', '‎': '', '‏': '', '⁠': '', '﻿': '', '×': 'x',
    'Þ': 'Th', 'þ': 'th', 'Ð': 'D', 'ð': 'd', 'Đ': 'D', 'đ': 'd',
    'Ł': 'L', 'ł': 'l', 'Ø': 'O', 'ø': 'o', 'ı': 'i', '→': '->',
    '←': '<-', '↔': '<->', '⇒': '=>', '≠': '!=', '№': 'No',
    'Æ': None, 'æ': None, 'Œ': None, 'œ': None, '⁄': '/',
    '¸': ',', '¨': '"', '´': "'", '¯': '-', '¹': '1', '³': '3',
    '¾': '3/4', '¤': '$', '¦': '|', '©': None, '®': None,
    '€': None, '∙': None, '●': '*', '○': 'o', '◦': 'o', '✓': 'v',
    '✔': 'v', '✗': 'x', '✘': 'x', '★': '*', '☆': '*', 'µ': None,
}


def a437(ch):
    """los octetos de la 437 de Hispa para ch; None si no hay nada"""
    if ch in A_MANO and A_MANO[ch] is not None:
        s = A_MANO[ch]
        r = b''
        for c in s:
            k = hispa437.codigo(c) if ord(c) >= 0x80 else ord(c)
            if k is None:
                return None
            r += bytes([k])
        return r
    k = hispa437.codigo(ch)
    if k is not None and k >= 0x20:
        return bytes([k])
    o = ord(ch)
    if 0x2500 <= o <= 0x257F:
        n = unicodedata.name(ch, '')
        if 'HORIZONTAL' in n and 'VERTICAL' not in n:
            return b'-'
        if 'VERTICAL' in n and 'HORIZONTAL' not in n:
            return b'|'
        return b'+'
    d = unicodedata.normalize('NFKD', ch)
    d = ''.join(c for c in d if not unicodedata.combining(c))
    if not d or d == ch or len(d) > 3:
        return None
    r = b''
    for c in d:
        if ord(c) < 0x80:
            if ord(c) < 0x20:
                return None
            r += bytes([ord(c)])
        else:
            k = hispa437.codigo(c)
            if k is None or k < 0x20:
                return None
            r += bytes([k])
    return r


def candidatos():
    for o in list(range(0xA0, 0x3000)) + list(range(0xFB00, 0xFB07)) + list(range(0xFE30, 0xFF70)):
        ch = chr(o)
        if unicodedata.category(ch) in ('Cn', 'Cs'):
            continue
        yield ch


PLANO_A_MANO = {'ß': 'ss', 'æ': 'ae', 'œ': 'oe', 'ø': 'o', 'đ': 'd', 'ł': 'l', 'þ': 'th',
                'ð': 'd', 'ı': 'i', 'ħ': 'h', 'ŧ': 't', 'ŀ': 'l', 'ĸ': 'k', 'ŋ': 'n', 'ſ': 's'}


def plano(ch):
    lo = ch.lower()
    if len(lo) != 1:
        lo = lo[0]
    if lo in PLANO_A_MANO:
        return PLANO_A_MANO[lo]
    d = unicodedata.normalize('NFKD', lo)
    d = ''.join(c for c in d if not unicodedata.combining(c))
    return d.lower() if d else lo


def cad_c(b):
    return '"' + ''.join('\\x%02x' % x for x in b) + '"'


def main():
    o = []
    o.append('/* juegos_tablas.h - Correo: las tablas de los juegos de caracteres de 8 bits,')
    o.append(' * la 437 de Hispa y las letras planas (para buscar).')
    o.append(' * NO SE EDITA: lo hace haz_juegos.py (de los codecs de Python y de')
    o.append(' * tools/hispa437.py).')
    o.append(' * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */')
    o.append('#ifndef CORREO_JUEGOS_TABLAS_H')
    o.append('#define CORREO_JUEGOS_TABLAS_H')
    o.append('')
    o.append('/* de 0x80 a 0xFF: la letra (0xFFFD si no la hay) */')
    o.append('static const u16 jt_tablas[%d][128] = {' % len(JUEGOS))
    for nom, codec, _ in JUEGOS:
        v = tabla(codec)
        o.append('\t/* %s (%s) */ {' % (nom, codec))
        for i in range(0, 128, 8):
            o.append('\t\t' + ', '.join('0x%04x' % x for x in v[i:i + 8]) + ',')
        o.append('\t},')
    o.append('};')
    o.append('')
    alias = []
    for i, (nom, codec, al) in enumerate(JUEGOS):
        for a in al:
            alias.append((a, i))
    for a in UTF8:
        alias.append((a, -1))
    for a in ASCII:
        alias.append((a, -3))
    alias.sort()
    nombres = [a for a, _ in alias]
    assert len(set(nombres)) == len(nombres), 'alias repetidos'
    o.append('/* los alias (normalizados), por orden: la tabla (-1: UTF-8; -3: us-ascii) */')
    o.append('#define JT_N_ALIAS %d' % len(alias))
    o.append('static const struct { const char *alias; i32 t; } jt_alias[JT_N_ALIAS] = {')
    for a, i in alias:
        o.append('\t{"%s", %d},' % (a, i))
    o.append('};')
    o.append('#define JT_1252 0')
    o.append('')
    # de la 437 de Hispa a Unicode
    de = [ord(hispa437.letra(c)) for c in range(0x80, 0x100)]
    o.append('/* la 437 de Hispa (Ventanas): de 0x80 a 0xFF, la letra */')
    o.append('static const u16 jt_de437[128] = {')
    for i in range(0, 128, 8):
        o.append('\t' + ', '.join('0x%04x' % x for x in de[i:i + 8]) + ',')
    o.append('};')
    o.append('')
    a = []
    for ch in candidatos():
        r = a437(ch)
        if r is not None:
            a.append((ord(ch), r))
    o.append('/* de Unicode a la 437 de Hispa (lo que tiene, y lo más parecido), por orden */')
    o.append('#define JT_N_A437 %d' % len(a))
    o.append('static const struct { u16 letra; char s[4]; } jt_a437[JT_N_A437] = {')
    for u, r in a:
        o.append('\t{0x%04x, %s},' % (u, cad_c(r)))
    o.append('};')
    o.append('')
    p = []
    for rango in (range(0xC0, 0x250), range(0x370, 0x500), range(0x1E00, 0x1F00), range(0xFB00, 0xFB07)):
        for u in rango:
            ch = chr(u)
            if unicodedata.category(ch) in ('Cn', 'Cs'):
                continue
            s = plano(ch)
            b = s.encode('utf-8')
            if s != ch and 0 < len(b) <= 5:
                p.append((u, b))
    o.append('/* minúsculas sin tildes (lo que no está, se queda como está), por orden */')
    o.append('#define JT_N_PLANO %d' % len(p))
    o.append('static const struct { u16 letra; char s[6]; } jt_plano[JT_N_PLANO] = {')
    for u, b in p:
        o.append('\t{0x%04x, %s},' % (u, cad_c(b)))
    o.append('};')
    o.append('')
    o.append('#endif')
    with open(SAL, 'w', encoding='utf-8') as f:
        f.write('\n'.join(o) + '\n')
    print('haz_juegos.py: %s (%d juegos, %d alias, %d de la 437, %d planas)' % (SAL, len(JUEGOS), len(alias), len(a), len(p)))


main()
