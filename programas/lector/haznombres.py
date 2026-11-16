#!/usr/bin/env python3
"""Hace programas/lector/nombres.inc: las tablas con las que Lector pasa el
texto de un PDF a la página 437 de Hispa-DOS.

uso: python3 programas/lector/haznombres.py

  lat_437     Latin-1 (A0-FF) -> 437; lo que no existe, por su letra sin
              acento o por algo que se le parezca
  uni_tab     los demás caracteres Unicode que se saben poner
  win_tab, mac_tab, std_tab   WinAnsi (80-9F), MacRoman (80-FF) y la
              codificación Standard de Adobe (20-FF) -> Unicode
  gl_tab      los nombres de letra de /Differences (aacute, ntilde...)
  sin_acento  de una letra 437 (80-FF) a la ASCII a la que se parece, para
              saber su ancho en las fuentes de siempre
  an_std      el ancho (en milésimas) de las letras 20-7E de Helvetica,
              Helvetica-Bold, Times-Roman y Times-Bold (de sus AFM)
  exp_tab     las letras que al escribirlas son varias (ligaduras, el euro)
"""
import os, unicodedata

AQUI = os.path.dirname(os.path.abspath(__file__))

# códigos de «varias letras»: no pueden ser 9, 10 ni 13
EXP = {1: 'fi', 2: 'fl', 3: 'ff', 4: 'ffi', 5: 'ffl', 6: 'EUR', 7: '...', 8: 'OE',
       11: 'oe', 12: 'TM', 14: '(c)', 15: '(R)'}
A_MANO = {
    0xA0: ' ', 0xA4: '?', 0xA6: '|', 0xA7: 0x15, 0xA8: '"', 0xA9: 14, 0xAD: '-', 0xAE: 15, 0xAF: '-',
    0xB3: '3', 0xB4: "'", 0xB6: 0x14, 0xB8: ',', 0xB9: '1', 0xBE: '?', 0xD7: 'x', 0xD8: 'O', 0xF8: 'o',
    0xD0: 'D', 0xF0: 'd', 0xDE: '?', 0xFE: '?',
    0x2018: "'", 0x2019: "'", 0x201A: ',', 0x201B: "'", 0x201C: '"', 0x201D: '"', 0x201E: '"',
    0x2010: '-', 0x2011: '-', 0x2012: '-', 0x2013: '-', 0x2014: '-', 0x2015: '-', 0x2212: '-',
    0x2022: 0xF9, 0x25CF: 0xF9, 0x25E6: 0xF9, 0x2043: '-', 0x25A0: 0xFE, 0x25AA: 0xFE,
    0x2026: 7, 0x20AC: 6, 0x2122: 12, 0x0152: 8, 0x0153: 11,
    0x0131: 'i', 0x0141: 'L', 0x0142: 'l', 0x02C6: '^', 0x02DC: '~', 0x2039: '<', 0x203A: '>',
    0x2030: '%', 0x2020: '+', 0x2021: '+', 0x2044: '/', 0x2215: '/', 0x2032: "'", 0x2033: '"',
    0x2002: ' ', 0x2003: ' ', 0x2004: ' ', 0x2005: ' ', 0x2006: ' ', 0x2007: ' ', 0x2008: ' ',
    0x2009: ' ', 0x200A: ' ', 0x202F: ' ', 0x3000: ' ',
    0xFB00: 3, 0xFB01: 1, 0xFB02: 2, 0xFB03: 4, 0xFB04: 5,
    0x03BC: 0xE6, 0x03B2: 0xE1, 0x2217: '*', 0x00B7: 0xFA, 0x2219: 0xF9, 0x2190: '<', 0x2192: '>', 0x2713: 'x', 0x2714: 'x',
    0x02C7: '^', 0x02D8: '~', 0x02D9: '.', 0x02DA: 0xF8, 0x02DB: ',', 0x02DD: '"', 0x0192: 0x9F,
}


def a437(u):
    """El octeto 437 (o el código de varias letras) de un carácter; 0 si no hay."""
    if u in A_MANO:
        v = A_MANO[u]
        return ord(v) if isinstance(v, str) else v
    try:
        b = chr(u).encode('cp437')[0]
        if b >= 32:
            return b
    except UnicodeEncodeError:
        pass
    d = unicodedata.normalize('NFKD', chr(u))
    if d and d[0].isascii() and d[0].isalnum() and all(unicodedata.combining(c) for c in d[1:]):
        return ord(d[0])
    return 0


STD = {0x27: 0x2019, 0x60: 0x2018, 0xA1: 0xA1, 0xA2: 0xA2, 0xA3: 0xA3, 0xA4: 0x2044, 0xA5: 0xA5, 0xA6: 0x192,
       0xA7: 0xA7, 0xA8: 0xA4, 0xA9: 0x27, 0xAA: 0x201C, 0xAB: 0xAB, 0xAC: 0x2039, 0xAD: 0x203A, 0xAE: 0xFB01,
       0xAF: 0xFB02, 0xB1: 0x2013, 0xB2: 0x2020, 0xB3: 0x2021, 0xB4: 0xB7, 0xB6: 0xB6, 0xB7: 0x2022,
       0xB8: 0x201A, 0xB9: 0x201E, 0xBA: 0x201D, 0xBB: 0xBB, 0xBC: 0x2026, 0xBD: 0x2030, 0xBF: 0xBF,
       0xC1: 0x60, 0xC2: 0xB4, 0xC3: 0x2C6, 0xC4: 0x2DC, 0xC5: 0xAF, 0xC6: 0x2D8, 0xC7: 0x2D9, 0xC8: 0xA8,
       0xCA: 0x2DA, 0xCB: 0xB8, 0xCD: 0x2DD, 0xCE: 0x2DB, 0xCF: 0x2C7, 0xD0: 0x2014, 0xE1: 0xC6, 0xE3: 0xAA,
       0xE8: 0x141, 0xE9: 0xD8, 0xEA: 0x152, 0xEB: 0xBA, 0xF1: 0xE6, 0xF5: 0x131, 0xF8: 0x142, 0xF9: 0xF8,
       0xFA: 0x153, 0xFB: 0xDF}

GL = {}
for i, n in enumerate('space exclam quotedbl numbersign dollar percent ampersand quotesingle parenleft parenright '
                      'asterisk plus comma hyphen period slash zero one two three four five six seven eight nine '
                      'colon semicolon less equal greater question at'.split()):
    GL[n] = 0x20 + i
for i, n in enumerate('bracketleft backslash bracketright asciicircum underscore grave'.split()):
    GL[n] = 0x5B + i
for i, n in enumerate('braceleft bar braceright asciitilde'.split()):
    GL[n] = 0x7B + i
for i, n in enumerate('exclamdown cent sterling currency yen brokenbar section dieresis copyright ordfeminine '
                      'guillemotleft logicalnot sfthyphen registered macron degree plusminus twosuperior threesuperior '
                      'acute mu paragraph periodcentered cedilla onesuperior ordmasculine guillemotright onequarter '
                      'onehalf threequarters questiondown Agrave Aacute Acircumflex Atilde Adieresis Aring AE Ccedilla '
                      'Egrave Eacute Ecircumflex Edieresis Igrave Iacute Icircumflex Idieresis Eth Ntilde Ograve Oacute '
                      'Ocircumflex Otilde Odieresis multiply Oslash Ugrave Uacute Ucircumflex Udieresis Yacute Thorn '
                      'germandbls agrave aacute acircumflex atilde adieresis aring ae ccedilla egrave eacute ecircumflex '
                      'edieresis igrave iacute icircumflex idieresis eth ntilde ograve oacute ocircumflex otilde '
                      'odieresis divide oslash ugrave uacute ucircumflex udieresis yacute thorn ydieresis'.split()):
    GL[n] = 0xA1 + i
GL.update({'quoteleft': 0x2018, 'quoteright': 0x2019, 'quotesinglbase': 0x201A, 'quotereversed': 0x201B,
           'quotedblleft': 0x201C, 'quotedblright': 0x201D, 'quotedblbase': 0x201E, 'endash': 0x2013,
           'emdash': 0x2014, 'bullet': 0x2022, 'ellipsis': 0x2026, 'Euro': 0x20AC, 'euro': 0x20AC,
           'trademark': 0x2122, 'OE': 0x152, 'oe': 0x153, 'dotlessi': 0x131, 'Lslash': 0x141, 'lslash': 0x142,
           'Scaron': 0x160, 'scaron': 0x161, 'Zcaron': 0x17D, 'zcaron': 0x17E, 'Ydieresis': 0x178,
           'circumflex': 0x2C6, 'tilde': 0x2DC, 'guilsinglleft': 0x2039, 'guilsinglright': 0x203A,
           'perthousand': 0x2030, 'dagger': 0x2020, 'daggerdbl': 0x2021, 'fraction': 0x2044, 'florin': 0x192,
           'fi': 0xFB01, 'fl': 0xFB02, 'ff': 0xFB00, 'ffi': 0xFB03, 'ffl': 0xFB04, 'minus': 0x2212,
           'nbspace': 0xA0, 'nonbreakingspace': 0xA0, 'middot': 0xB7, 'caron': 0x2C7, 'breve': 0x2D8,
           'dotaccent': 0x2D9, 'ring': 0x2DA, 'ogonek': 0x2DB, 'hungarumlaut': 0x2DD, 'lessequal': 0x2264,
           'greaterequal': 0x2265, 'notequal': 0x2260, 'approxequal': 0x2248, 'infinity': 0x221E,
           'pi': 0x3C0, 'alpha': 0x3B1, 'beta': 0x3B2, 'Gamma': 0x393, 'delta': 0x3B4, 'epsilon': 0x3B5,
           'sigma': 0x3C3, 'Sigma': 0x3A3, 'tau': 0x3C4, 'Phi': 0x3A6, 'phi': 0x3C6, 'Theta': 0x398,
           'Omega': 0x3A9, 'radical': 0x221A, 'arrowright': 0x2192, 'arrowleft': 0x2190, 'asteriskmath': 0x2217,
           'hyphenminus': 0x2D, 'softhyphen': 0xAD, 'periodinferior': 0x2E, 'underscoredbl': 0x5F,
           'Ccaron': 0x10C, 'ccaron': 0x10D, 'Rcaron': 0x158, 'rcaron': 0x159, 'Aogonek': 0x104, 'aogonek': 0x105,
           'Eogonek': 0x118, 'eogonek': 0x119, 'Nacute': 0x143, 'nacute': 0x144, 'Sacute': 0x15A, 'sacute': 0x15B,
           'Zacute': 0x179, 'zacute': 0x17A, 'Zdotaccent': 0x17B, 'zdotaccent': 0x17C, 'Cacute': 0x106,
           'cacute': 0x107, 'Gbreve': 0x11E, 'gbreve': 0x11F, 'Idotaccent': 0x130, 'Scedilla': 0x15E,
           'scedilla': 0x15F, 'Ecaron': 0x11A, 'ecaron': 0x11B, 'Uring': 0x16E, 'uring': 0x16F})

HELV = [278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556, 556,
        556, 556, 556, 556, 278, 278, 584, 584, 584, 556, 1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667,
        556, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556, 333, 556,
        556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556, 556, 556, 333, 500, 278, 556, 500, 722,
        500, 500, 500, 334, 260, 334, 584]
HELVB = [278, 333, 474, 556, 556, 889, 722, 238, 333, 333, 389, 584, 278, 333, 278, 278, 556, 556, 556, 556, 556, 556,
         556, 556, 556, 556, 333, 333, 584, 584, 584, 611, 975, 722, 722, 722, 722, 667, 611, 778, 722, 278, 556, 722,
         611, 833, 722, 778, 667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 333, 278, 333, 584, 556, 333, 556,
         611, 556, 611, 556, 333, 611, 611, 278, 278, 556, 278, 889, 611, 611, 611, 611, 389, 556, 333, 611, 556, 778,
         556, 556, 500, 389, 280, 389, 584]
TIMES = [250, 333, 408, 500, 500, 833, 778, 180, 333, 333, 500, 564, 250, 333, 250, 278, 500, 500, 500, 500, 500, 500,
         500, 500, 500, 500, 278, 278, 564, 564, 564, 444, 921, 722, 667, 667, 722, 611, 556, 722, 722, 333, 389, 722,
         611, 889, 722, 722, 556, 722, 667, 556, 611, 722, 722, 944, 722, 722, 611, 333, 278, 333, 469, 500, 333, 444,
         500, 444, 500, 444, 333, 500, 500, 278, 278, 500, 278, 778, 500, 500, 500, 500, 333, 389, 278, 500, 500, 722,
         500, 500, 444, 480, 200, 480, 541]
TIMESB = [250, 333, 555, 500, 500, 1000, 833, 278, 333, 333, 500, 570, 250, 333, 250, 278, 500, 500, 500, 500, 500,
          500, 500, 500, 500, 500, 333, 333, 570, 570, 570, 500, 930, 722, 667, 722, 722, 667, 611, 778, 778, 389, 500,
          778, 667, 944, 722, 778, 611, 778, 722, 556, 667, 722, 722, 1000, 722, 722, 667, 333, 278, 333, 581, 500, 333,
          500, 556, 444, 556, 444, 333, 500, 556, 278, 333, 556, 278, 833, 556, 500, 556, 556, 444, 389, 333, 556, 500,
          722, 500, 500, 444, 394, 220, 394, 520]
ANCHOS = [HELV, HELVB, TIMES, TIMESB]


def filas(vals, por=12, pal='db', fmt='0x%02X'):
    return ''.join('        %s %s\n' % (pal, ', '.join(fmt % v for v in vals[i:i + por])) for i in range(0, len(vals), por))


def main():
    for t in ANCHOS:
        assert len(t) == 95
    s = ['; nombres.inc - Lector: las tablas para pasar el texto de un PDF a la página 437.\n'
         '; Lo hace programas/lector/haznombres.py: no se toca a mano.\n'
         '; Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n\n']
    s.append('lat_437:                                ; Latin-1, de A0 a FF\n' + filas([a437(u) for u in range(0xA0, 0x100)]))
    otros = set(A_MANO) | {ord(bytes([b]).decode('cp437')) for b in range(128, 256)} | set(range(0x100, 0x180)) | set(GL.values())
    otros = sorted(u for u in otros if u > 0xFF and a437(u))
    s.append('uni_tab:                                ; dw Unicode, db 437; acaba en 0\n')
    s.append(''.join('        dw 0x%04X\n        db 0x%02X\n' % (u, a437(u)) for u in otros) + '        dw 0\n')
    s.append('win_tab:                                ; WinAnsi, de 80 a 9F\n')
    s.append(filas([ord(bytes([b]).decode('cp1252', errors='replace')) if bytes([b]).decode('cp1252', errors='replace') != '�' else 0
                    for b in range(0x80, 0xA0)], 8, 'dw', '0x%04X'))
    s.append('mac_tab:                                ; MacRoman, de 80 a FF\n')
    s.append(filas([ord(bytes([b]).decode('mac_roman')) for b in range(0x80, 0x100)], 8, 'dw', '0x%04X'))
    s.append('std_tab:                                ; Standard de Adobe, de 20 a FF\n')
    s.append(filas([STD.get(c, c if c < 0x7F else 0) for c in range(0x20, 0x100)], 8, 'dw', '0x%04X'))
    s.append('sin_acento:                             ; de una letra 437 (80-FF), la ASCII que se le parece\n')
    sa = []
    for b in range(128, 256):
        u = bytes([b]).decode('cp437')
        d = unicodedata.normalize('NFKD', u)
        sa.append(ord(d[0]) if d[0].isascii() and d[0].isalnum() else ord('n'))
    s.append(filas(sa))
    s.append('an_std:                                 ; anchos de 20 a 7E: Helvetica, Helvetica-Bold, Times-Roman, Times-Bold\n')
    for t in ANCHOS:
        s.append(filas(t, 12, 'dw', '%d'))
    s.append('exp_tab:                                ; de cada código de 0 a 15, sus letras (4 octetos, acabadas en 0)\n')
    for c in range(16):
        t = EXP.get(c, '')
        s.append('        db %s\n' % ', '.join(['"%s"' % t] * bool(t) + ['0'] * (4 - len(t))))
    s.append('gl_tab:                                 ; db largo, nombre, dw Unicode; acaba en 0\n')
    for n in sorted(GL):
        if not a437(GL[n]):
            continue                    # (no hay con qué ponerla)
        s.append('        db %d, "%s"\n        dw 0x%04X\n' % (len(n), n, GL[n]))
    s.append('        db 0\n')
    open(os.path.join(AQUI, 'nombres.inc'), 'w', encoding='utf-8').write(''.join(s))


if __name__ == '__main__':
    main()
