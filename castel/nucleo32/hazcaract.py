#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazcaract.py: (ACCESORIOS) hace nucleo32/caracnom.cas, lo que el
Mapa de caracteres (ucaract.cas) sabe de cada letra de la letra del sistema
(nucleo32/letra.cas, la página 437 de Hispa con sus letras de más:
tools/hispa437.py): cuáles tienen dibujo, su nombre en español, su código
Unicode y cómo se escribe con el teclado español del núcleo (las tablas de
nucleo32/consola.cas: es_normal, es_mayús, es_altgr y las teclas muertas).
Hay que volver a pasarlo si cambia la letra o el teclado.

uso: python3 nucleo32/hazcaract.py        (desde castel/)
"""
import os, re, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, '..', '..')
sys.path.insert(0, os.path.join(RAIZ, 'tools'))
import hispa437  # noqa: E402

NOMBRES = {
    ' ': 'espacio', '!': 'cierra exclamación', '"': 'comillas', '#': 'almohadilla',
    '$': 'dólar', '%': 'por ciento', '&': 'et (y)', "'": 'apóstrofo',
    '(': 'abre paréntesis', ')': 'cierra paréntesis', '*': 'asterisco', '+': 'más',
    ',': 'coma', '-': 'guion (menos)', '.': 'punto', '/': 'barra',
    ':': 'dos puntos', ';': 'punto y coma', '<': 'menor que', '=': 'igual',
    '>': 'mayor que', '?': 'cierra interrogación', '@': 'arroba', '[': 'abre corchete',
    '\\': 'barra invertida', ']': 'cierra corchete', '^': 'circunflejo', '_': 'guion bajo',
    '`': 'acento grave', '{': 'abre llave', '|': 'barra vertical', '}': 'cierra llave',
    '~': 'virgulilla', 'ª': 'ordinal femenino', 'º': 'ordinal masculino',
    '¿': 'abre interrogación', '¡': 'abre exclamación',
    '«': 'comillas latinas (abre)', '»': 'comillas latinas (cierra)',
    'ß': 'ese doble alemana', 'Œ': 'OE ligadas (mayúscula)',
    'œ': 'oe ligadas (minúscula)', 'π': 'pi (griega)',
    'Σ': 'sigma mayúscula (griega)', 'µ': 'micro', '©': 'derechos de autor (copyright)',
    '®': 'marca registrada', 'Ω': 'omega mayúscula (griega)',
    'δ': 'delta minúscula (griega)', '≡': 'idéntico a', '±': 'más o menos',
    '€': 'euro', '…': 'puntos suspensivos', '÷': 'dividido entre',
    '°': 'grado', '·': 'punto medio', 'ñ': 'eñe minúscula',
    'Ñ': 'eñe mayúscula', 'ç': 'ce con cedilla (minúscula)',
    'Ç': 'ce con cedilla (mayúscula)',
}
SIGNOS = {'́': 'con tilde', '̀': 'con acento grave', '̂': 'con circunflejo',
          '̈': 'con diéresis', '̃': 'con virgulilla'}
CIFRAS = ['cero', 'uno', 'dos', 'tres', 'cuatro', 'cinco', 'seis', 'siete', 'ocho', 'nueve']


def nombre(ch):
    import unicodedata
    if ch in NOMBRES:
        return NOMBRES[ch]
    if ch.isdigit():
        return 'cifra %s (%s)' % (ch, CIFRAS[int(ch)])
    d = unicodedata.normalize('NFD', ch)
    base = d[0]
    if base.isalpha() and base.isascii():
        t = '%s %s' % (base, 'mayúscula' if base.isupper() else 'minúscula')
        if len(d) > 1:
            t = '%s %s' % (base, SIGNOS.get(d[1], ''))
            t += ' (mayúscula)' if base.isupper() else ' (minúscula)'
        return t
    raise SystemExit('hazcaract: sin nombre para %r' % ch)


def tablas_teclado():
    """las tablas del teclado español de consola.cas: código de tecla -> letra"""
    src = open(os.path.join(AQUI, 'consola.cas'), encoding='utf-8').read()
    def tabla(n):
        m = re.search(r'variable %s: arreglo \[\d+\] de byte = "((?:[^"\\]|\\.)*)"' % n, src)
        t = m.group(1)
        out = []
        i = 0
        while i < len(t):
            if t[i] == '\\':
                c = t[i + 1]
                if c == 'x':
                    out.append(int(t[i + 2:i + 4], 16)); i += 4; continue
                out.append({'0': 0, 'e': 27, 't': 9, 'n': 10, '\\': 92, '"': 34}[c]); i += 2; continue
            out.append(ord(t[i])); i += 1
        return out
    def vocales(n):
        return tabla(n)
    return tabla('es_normal'), tabla('es_mayús'), tabla('es_altgr'), \
        {k: vocales(k) for k in ('con_agudo', 'con_grave', 'con_circunflejo', 'con_diéresis')}


# cómo se llama cada tecla (por su código), para decirlo
TECLA = {0x02: '1', 0x03: '2', 0x04: '3', 0x05: '4', 0x06: '5', 0x07: '6', 0x08: '7', 0x09: '8',
         0x0A: '9', 0x0B: '0', 0x0C: "'", 0x0D: '¡', 0x1A: '`', 0x1B: '+', 0x27: 'ñ',
         0x28: 'del acento', 0x29: 'º', 0x2B: 'ç', 0x33: ',', 0x34: '.', 0x35: '-', 0x39: 'espacio'}


def combinaciones():
    normal, mayus, altgr, muertas = tablas_teclado()
    r = {}
    def pon(c, t):
        if c >= 32 and c not in r:
            r[c] = t
    for k, c in enumerate(normal):
        if 'a' <= chr(c) <= 'z':
            pon(c, 'tecla %s' % chr(c).upper())
        elif k in TECLA and c >= 32:
            pon(c, 'tecla %s' % TECLA[k])
    for k, c in enumerate(mayus):
        if 'A' <= chr(c) <= 'Z':
            pon(c, 'Mayús + %s' % chr(c))
        elif k in TECLA and c >= 32:
            pon(c, 'Mayús + tecla %s' % TECLA[k])
    for k, c in enumerate(altgr):
        if k in TECLA and c >= 32:
            pon(c, 'Alt Gr + tecla %s' % TECLA[k])
    pon(ord('<'), 'tecla <')
    pon(ord('>'), 'Mayús + tecla <')
    voc = 'aeiouAEIOU'
    muerta = {'con_agudo': 'tecla del acento', 'con_grave': 'tecla `',
              'con_circunflejo': 'Mayús + tecla `', 'con_diéresis': 'Mayús + tecla del acento'}
    for n, tab in muertas.items():
        for i, c in enumerate(tab):
            if c != ord(voc[i]):
                v = voc[i] if i < 5 else 'Mayús + %s' % voc[i]
                pon(c, '%s y luego %s' % (muerta[n], v))
    # las teclas muertas solas: con el espacio
    pon(ord('^'), 'Mayús + tecla ` y luego espacio')
    pon(ord('`'), 'tecla ` y luego espacio')
    return r


def a_castel(t):
    out = ''
    for ch in t:
        c = hispa437.codigo(ch)
        if c is None:
            raise SystemExit('hazcaract: %r no está en la letra' % ch)
        if 32 <= c < 127 and ch not in '"\\|':
            out += ch
        else:
            out += '\\x%02X' % c
    return out


def main():
    src = open(os.path.join(AQUI, 'letra.cas'), encoding='utf-8').read()
    filas = [tuple(int(v, 16) for v in m.split(',')) for m in re.findall(r'db ([0-9A-Fx,]+)', src)]
    assert len(filas) == 256
    caja = filas[1]             # el dibujo de «no hay»: el de los códigos de control
    hay = [c for c in range(32, 256) if filas[c] != caja and (any(filas[c]) or c == 32)]
    comb = combinaciones()
    nombres, unis, teclas = [], [], []
    for c in hay:
        ch = hispa437.letra(c)
        nombres.append(a_castel(nombre(ch)))
        unis.append('%04X' % ord(ch))
        teclas.append(a_castel(comb.get(c, '')))
    s = []
    s.append('; =============================================================================')
    s.append(';  caracnom.cas - (ACCESORIOS) lo que sabe el Mapa de caracteres de cada letra')
    s.append(';  de la letra del sistema: las que tienen dibujo (%d), su nombre, su código' % len(hay))
    s.append(';  Unicode y cómo se escriben con el teclado español del núcleo.')
    s.append(';  NO SE EDITA: lo hace nucleo32/hazcaract.py (de letra.cas y consola.cas).')
    s.append(';  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.')
    s.append('; =============================================================================')
    s.append('módulo CaracNom')
    s.append('')
    s.append('    público constante N_LETRAS = %d' % len(hay))
    s.append('    ; los códigos que tienen dibujo, en orden')
    s.append('    público constante CÓDIGOS = "%s"' % ''.join('\\x%02X' % c for c in hay))
    s.append('    ; sus nombres, separados por |')
    s.append('    público constante NOMBRES = "%s"' % '|'.join(nombres))
    s.append('    ; su código Unicode (cuatro cifras hexadecimales cada uno)')
    s.append('    público constante UNICODE = "%s"' % ''.join(unis))
    s.append('    ; cómo se escriben con el teclado (vacío: no se puede)')
    s.append('    público constante TECLAS = "%s"' % '|'.join(teclas))
    s.append('')
    s.append('fin CaracNom.')
    open(os.path.join(AQUI, 'caracnom.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')
    print('hazcaract: %d letras con dibujo, %d con su combinación de teclas' % (len(hay), sum(1 for t in teclas if t)))


if __name__ == '__main__':
    main()
