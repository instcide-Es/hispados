#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""blocprueba/hazexp.py PEXP.BIN CORRE32 CARPETA: prueba BlocExp (las
expresiones regulares del Bloc de notas) con pexp.cas en corre32.

  1. una tabla de casos a mano (con lo que tiene que salir);
  2. el tope: patrones de los que hacen sufrir a un motor con vuelta atrás;
  3. lo que tarda (un literal en 10 MB, [0-9]+ en 1 MB);
  4. el fuzz contra el «re» de Python (FUZZ_EXP vueltas, 3000 si no se
     dice; semilla fija, FUZZ_SEMILLA): patrones al azar de lo que entiende
     BlocExp, textos ASCII y con UTF-8 (ñ, á, €), desde 0 y desde otra
     posición, hacia delante y hacia atrás, y con reemplazo;
  5. patrones rotos al azar: compilar falla con su mensaje o encuentra, pero
     nunca revienta ni se cuelga.
Escribe «bien: ...» o «mal: ...»; acaba con 1 si algo va mal.
"""
import os
import random
import re
import subprocess
import sys

SIN_CAJA, LITERAL, PALABRA = 1, 2, 4
fallos = 0


def bien(t):
    print('bien: ' + t)


def mal(t):
    global fallos
    fallos += 1
    print('mal: ' + t)


def hx(s):
    return s.encode('utf-8').hex() if isinstance(s, str) else s.hex()


def corre(prog, corre32, carpeta, lineas, nombre):
    ruta = os.path.join(carpeta, nombre)
    with open(ruta, 'w') as f:
        f.write('\n'.join(lineas) + '\n')
    r = subprocess.run([corre32, os.path.abspath(prog), nombre], capture_output=True, timeout=600, cwd=carpeta)
    sal = r.stdout.decode('latin-1').replace('\r', '').split('\n')
    if sal and sal[-1] == '':
        sal.pop()
    return r.returncode, sal


def linea(k, pat, opc, texto, pos, plantilla='', tope=0):
    return '%s|%d|%d|%s|%s|%d|%s' % (k, opc, tope, hx(pat), hx(texto), pos, hx(plantilla))


def boff(texto, i):
    """de posición en caracteres a octetos de UTF-8"""
    return len(texto[:i].encode('utf-8'))


def sustituto_py(m, plantilla):
    out = []
    i = 0
    while i < len(plantilla):
        c = plantilla[i]
        if c == '\\' and i + 1 < len(plantilla):
            i += 1
            c = plantilla[i]
            if c.isdigit():
                k = int(c)
                if k <= (m.re.groups or 0) and m.group(k) is not None:
                    out.append(m.group(k))
                i += 1
                continue
            c = {'n': '\n', 't': '\t'}.get(c, c)
        out.append(c)
        i += 1
    return ''.join(out)


def esperado_py(k, pat, opc, texto, pos, plantilla=''):
    """lo que tiene que decir pexp, según el «re» de Python (None si Python no lo compila)"""
    fl = re.M
    if opc & SIN_CAJA:
        fl |= re.I
    p = re.escape(pat) if opc & LITERAL else pat
    if opc & PALABRA:
        p = r'(?<!\w)(?:' + p + r')(?!\w)'
    try:
        rx = re.compile(p, fl)
    except (re.error, OverflowError, RecursionError):
        return None
    if k == 'A':
        m = None
        for q in range(pos - 1, -1, -1):
            m = rx.match(texto, q)
            if m:
                break
    else:
        m = rx.search(texto, pos)
    if not m:
        return '- 0'
    s = '%d %d' % (boff(texto, m.start()), boff(texto, m.end()))
    if rx.groups >= 1 and m.span(1) != (-1, -1):
        s += ' %d %d' % (boff(texto, m.start(1)), boff(texto, m.end(1)))
    else:
        s += ' x x'
    s += ' 0'
    if k == 'S':
        s += ' ' + hx(sustituto_py(m, plantilla))
    return s


# ---- 1. la tabla a mano: (clase, patrón, opciones, texto, posición (en caracteres), plantilla, lo que sale) ----
# lo que sale: «py» (lo que diga Python), «E» (no compila) o el resultado tal cual (en octetos)
A_MANO = [
    ('B', 'mundo', 0, 'hola mundo', 0, '', 'py'),
    ('B', 'o', 0, 'hola mundo', 2, '', 'py'),
    ('B', 'a.c', 0, 'abc a\nc axc', 0, '', 'py'),
    ('B', 'a.c', 0, 'añc', 0, '', '0 4 x x 0'),              # «.» es la ñ entera (2 octetos)
    ('B', 'x*', 0, 'abc', 0, '', '0 0 x x 0'),               # el vacío también se da
    ('B', 'b+', 0, 'abbbc', 0, '', 'py'),
    ('B', 'b+?', 0, 'abbbc', 0, '', 'py'),
    ('B', 'a{2,3}', 0, 'aaaa', 0, '', 'py'),
    ('B', 'a{2,3}?', 0, 'aaaa', 0, '', 'py'),
    ('B', 'a{2}', 0, 'a aa', 0, '', 'py'),
    ('B', 'a{2,}', 0, 'aaaaa', 0, '', 'py'),
    ('B', 'a{,2}b', 0, 'aaab', 0, '', 'py'),
    ('B', 'x{', 0, 'ax{', 0, '', 'py'),                     # una «{» que no es repetición es una letra
    ('B', '[a-c]+', 0, 'xxbcaz', 0, '', 'py'),
    ('B', '[^a-c ]+', 0, 'abc def', 0, '', 'py'),
    ('B', '[]a]+', 0, 'x]a]', 0, '', 'py'),
    ('B', '[a-]+', 0, 'x-a-', 0, '', 'py'),
    ('B', '[\\d.]+', 0, 'v 1.25 x', 0, '', 'py'),
    ('B', '[á-ú]+', 0, 'xé', 0, '', '1 3 x x 0'),
    ('B', '[ñÑ]', 0, 'aÑo', 0, '', '1 3 x x 0'),
    ('B', '^b', 0, 'a\nb', 0, '', 'py'),
    ('B', 'a$', 0, 'xa\nya', 0, '', 'py'),
    ('B', '^$', 0, 'a\n\nb', 0, '', 'py'),
    ('B', '(a|ab)(c|bcd)', 0, 'abcd', 0, '', 'py'),        # la prioridad de Perl (el primero que casa)
    ('B', '(ab|a)(bcd|c)', 0, 'abcd', 0, '', 'py'),
    ('B', '(?:ab)+(c)', 0, 'ababc', 0, '', 'py'),
    ('B', '(a)|b', 0, 'b', 0, '', 'py'),                    # el grupo 1 no participa
    ('B', '\\bpa\\w*', 0, 'apa pato', 0, '', 'py'),
    ('B', '\\Ba', 0, 'a ba', 0, '', 'py'),
    ('B', '\\w+', 0, '¿año?', 0, '', '2 6 x x 0'),           # la ñ es de palabra
    ('B', '\\d+', 0, 'x 2026 y', 0, '', 'py'),
    ('B', '\\s+', 0, 'a \t b', 0, '', 'py'),
    ('B', '\\D\\W\\S', 0, 'a1 ', 0, '', '- 0'),
    ('B', '\\.\\(\\)', 0, 'a.()', 0, '', 'py'),
    ('B', '\\x41', 0, 'zA', 0, '', 'py'),
    ('B', 'a\\nb', 0, 'xa\nb', 0, '', 'py'),
    ('B', 'MUNDO', SIN_CAJA, 'hola mundo', 0, '', 'py'),
    ('B', '[A-C]+', SIN_CAJA, 'xabcz', 0, '', 'py'),
    ('B', 'ñ', SIN_CAJA, 'AÑO', 0, '', '1 3 x x 0'),         # sin caja también con las de Latin-1
    ('B', 'Á', SIN_CAJA, 'tá', 0, '', '1 3 x x 0'),
    ('B', 'Ŝ', SIN_CAJA, 'xŝ', 0, '', '1 3 x x 0'),          # y las de Latin Extended-A
    ('B', 'ÿ', SIN_CAJA, 'Ÿ', 0, '', '0 2 x x 0'),
    ('B', 'a.b', LITERAL, 'axb a.b', 0, '', 'py'),          # tal cual: el punto es un punto
    ('B', 'A.B', LITERAL | SIN_CAJA, 'axb a.b', 0, '', 'py'),
    ('B', 'cas', PALABRA, 'casa cas', 0, '', 'py'),
    ('B', 'cas', LITERAL | PALABRA, 'casa cas', 0, '', 'py'),
    ('B', 'año', PALABRA, 'añoso año', 0, '', '7 11 x x 0'),
    ('B', 'a|b', PALABRA, 'ab b', 0, '', 'py'),
    ('B', '', LITERAL, 'abc', 2, '', '2 2 x x 0'),
    ('A', 'o', 0, 'hola mundo', 10, '', 'py'),               # hacia atrás
    ('A', 'o', 0, 'hola mundo', 9, '', 'py'),
    ('A', 'o', 0, 'hola mundo', 1, '', '- 0'),
    ('A', 'o', LITERAL, 'hola mundo', 5, '', 'py'),
    ('A', '\\w+', 0, 'ab cd', 5, '', 'py'),
    ('A', 'ñ', 0, 'ñañ', 1, '', '0 2 x x 0'),                # no empieza a mitad de la ñ
    ('S', '(\\w+) (\\w+)', 0, 'hola mundo', 0, '\\2 \\1', 'py'),
    ('S', 'a', 0, 'xa', 0, '[\\0]\\n\\t\\\\', 'py'),
    ('S', '(x)?a', 0, 'a', 0, '<\\1>', 'py'),
    # los errores
    ('B', '(ab', 0, '', 0, '', 'E 0'),
    ('B', 'ab)', 0, '', 0, '', 'E 2'),
    ('B', '[ab', 0, '', 0, '', 'E 0'),
    ('B', '*a', 0, '', 0, '', 'E 0'),
    ('B', 'a|+', 0, '', 0, '', 'E 2'),
    ('B', 'ab\\', 0, '', 0, '', 'E 2'),
    ('B', 'a**', 0, '', 0, '', 'E 2'),
    ('B', '(a)\\1', 0, '', 0, '', 'E 3'),
    ('B', '[z-a]', 0, '', 0, '', 'E 3'),
    ('B', 'a{3,2}', 0, '', 0, '', 'E 1'),
    ('B', 'a{2000}', 0, '', 0, '', 'E 1'),
    ('B', '\\q', 0, '', 0, '', 'E 0'),
    ('B', '(?=a)', 0, '', 0, '', 'E 0'),
    ('B', '\\xZZ', 0, '', 0, '', 'E 0'),
    ('B', '(a{1000}){1000}', 0, '', 0, '', 'E 0'),          # demasiado larga
]


def a_mano(prog, corre32, carpeta):
    lineas, esper = [], []
    for k, pat, opc, texto, pos, pla, esp in A_MANO:
        lineas.append(linea(k, pat, opc, texto, boff(texto, pos), pla))
        esper.append(esperado_py(k, pat, opc, texto, pos, pla) if esp == 'py' else esp)
    rc, sal = corre(prog, corre32, carpeta, lineas, 'mano.txt')
    malos = []
    for n, caso in enumerate(A_MANO):
        dicho = sal[n] if n < len(sal) else '(nada)'
        e = esper[n]
        if e is None:
            malos.append('%r: Python no lo compila' % (caso,))
        elif e.startswith('E'):
            if dicho != e:
                malos.append('%r: dice «%s» y es «%s»' % (caso[:5], dicho, e))
        elif dicho != e:
            malos.append('%r: dice «%s» y es «%s»' % (caso[:5], dicho, e))
    if rc != 0 or malos:
        mal('la tabla a mano: %d de %d mal (salida %d): %s' % (len(malos), len(A_MANO), rc, '; '.join(malos[:8])))
    else:
        bien('la tabla a mano: %d casos (buscar, buscar hacia atrás, reemplazar, sin caja, literal, palabra entera y los errores con su columna)' % len(A_MANO))


def tope(prog, corre32, carpeta):
    casos = [
        ('B', '(a*)*b', 0, 'a' * 5000, 0, 0, '- 0'),
        ('B', '(a|aa)*c', 0, 'a' * 20000, 0, 0, '- 0'),
        ('B', '(a+)+$', 0, 'a' * 3000 + 'b', 0, 0, '- 0'),
        ('B', '(x+x+)+y', 0, 'x' * 3000, 0, 0, '- 0'),
        ('B', '(a|aa)*c', 0, 'a' * 20000, 0, 1000, '- 1'),
        ('A', '(a|aa)*c', 0, 'a' * 2000, 2000, 5000, '- 1'),
    ]
    lineas = [linea(k, p, o, t, s, '', tp) for k, p, o, t, s, tp, e in casos]
    import time
    t0 = time.time()
    rc, sal = corre(prog, corre32, carpeta, lineas, 'tope.txt')
    dt = time.time() - t0
    malos = ['%s dice «%s» y es «%s»' % (c[1], sal[n] if n < len(sal) else '(nada)', c[6]) for n, c in enumerate(casos)
             if (sal[n] if n < len(sal) else '') != c[6]]
    if rc != 0 or malos:
        mal('el tope: ' + '; '.join(malos))
    else:
        bien('el tope: (a*)*b, (a|aa)*c, (a+)+$ y (x+x+)+y en textos de miles de letras acaban sin vuelta atrás (%.1f s todo); con un tope pequeño, exp_agotado' % dt)


def velocidad(prog, corre32, carpeta):
    casos = [
        ('V|%d|0|%s|%s|%d|' % (LITERAL, hx('zzzq'), hx('hola, buenos días; adiós\n'), 10 * 1024 * 1024), 'un literal en 10 MB (sin encontrarlo)', 10),
        ('V|%d|0|%s|%s|%d|' % (LITERAL | SIN_CAJA, hx('ZZZQ'), hx('hola, buenos días; adiós\n'), 10 * 1024 * 1024), 'un literal sin caja en 10 MB', 300),
        ('V|0|100000000|%s|%s|%d|' % (hx('[0-9]+x'), hx('cuenta 12345 y 678\n'), 1024 * 1024), '[0-9]+x en 1 MB (con muchas cifras)', 500),
        ('V|0|100000000|%s|%s|%d|' % (hx('\\w+ \\w+;'), hx('palabra otra cosa\n'), 1024 * 1024), '\\w+ \\w+; en 1 MB', 1000),
    ]
    rc, sal = corre(prog, corre32, carpeta, [c[0] for c in casos], 'vel.txt')
    partes = []
    for n, (l, que, máx) in enumerate(casos):
        s = sal[n] if n < len(sal) else ''
        m = re.match(r'^- 0 tiempo (\d+)$', s)
        if not m:
            mal('lo que tarda %s: dice «%s»' % (que, s))
            return
        cs = int(m.group(1))
        partes.append('%s, %d ms' % (que, cs * 10))
        if cs > máx:
            mal('%s tarda %d ms (lo más: %d)' % (que, cs * 10, máx * 10))
            return
    bien('lo que tarda (en el anfitrión): ' + '; '.join(partes))


# ---- 4. el fuzz ----
LETRAS = ['a', 'b', 'c', 'A', 'B', ' ', '\n', '1', '2', '_', 'ñ', 'á', '€', '.', '-']
META = '.^$*+?()[]{}|\\'


def esc(c):
    if c == '\n':
        return '\\n'
    return '\\' + c if c in META else c


def atomo(r, prof):
    """(patrón, puede casar con nada)"""
    x = r.random()
    if x < 0.35:
        return esc(r.choice(LETRAS)), False
    if x < 0.42:
        return '.', False
    if x < 0.55:
        neg = '^' if r.random() < 0.3 else ''
        cosas = []
        for _ in range(r.randint(1, 3)):
            y = r.random()
            if y < 0.3:
                a, b = sorted([r.choice('abcxyz012'), r.choice('abcxyz012')])
                cosas.append(a + '-' + b)
            elif y < 0.45:
                cosas.append(r.choice(['\\d', '\\w', '\\s']))
            else:
                c = r.choice(LETRAS)
                cosas.append('\\' + c if c in ']\\^-' else ('\\n' if c == '\n' else c))
        return '[' + neg + ''.join(cosas) + ']', False
    if x < 0.62:
        return r.choice(['\\d', '\\w', '\\s', '\\D', '\\W', '\\S']), False
    if x < 0.68:
        return r.choice(['^', '$', '\\b', '\\B']), True
    if prof < 3:
        p, nul = alternativa(r, prof + 1)
        return ('(' if r.random() < 0.7 else '(?:') + p + ')', nul
    return esc(r.choice(LETRAS)), False


def seguidos(r, prof):
    partes, nul = [], True
    for _ in range(r.randint(1, 3)):
        p, n = atomo(r, prof)
        if r.random() < 0.4:
            if n:
                q = '?'
            else:
                q = r.choice(['*', '+', '?', '{2}', '{1,3}', '{0,2}', '{2,}', '{,2}'])
            if r.random() < 0.3:
                q += '?'
            p += q
            n = n or q[0] in '*?' or q.startswith('{0') or q.startswith('{,')
        partes.append(p)
        nul = nul and n
    return ''.join(partes), nul


def alternativa(r, prof):
    p, n = seguidos(r, prof)
    while r.random() < 0.25:
        q, m = seguidos(r, prof)
        p, n = p + '|' + q, n or m
    return p, n


def fuzz(prog, corre32, carpeta):
    vueltas = int(os.environ.get('FUZZ_EXP', '3000'))
    r = random.Random(int(os.environ.get('FUZZ_SEMILLA', '2026')))
    casos = []
    while len(casos) < vueltas:
        pat, _ = alternativa(r, 0)
        sin = r.random() < 0.2
        letras = [c for c in LETRAS if ord(c) < 128] if sin else LETRAS
        texto = ''.join(r.choice(letras) for _ in range(r.randint(0, 30)))
        opc = SIN_CAJA if sin else 0
        if r.random() < 0.1:
            opc |= PALABRA
        if r.random() < 0.05:
            opc |= LITERAL
        k = r.choice('BBBAS')
        pos = 0 if r.random() < 0.5 else r.randint(0, len(texto))
        if k == 'A':
            pos = r.randint(0, len(texto) + 1)
        pla = r.choice(['\\1', '<\\0>', 'x\\2y', '\\\\n']) if k == 'S' else ''
        e = esperado_py(k, pat, opc, texto, pos, pla)
        if e is None or (texto == '' and '\\B' in pat):     # (en Python, \B nunca casa en un texto vacío)
            continue
        casos.append((k, pat, opc, texto, pos, pla, e))
    lineas = [linea(k, p, o, t, boff(t, s) if k != 'A' or s <= len(t) else len(t.encode()) + 1, pl) for k, p, o, t, s, pl, e in casos]
    rc, sal = corre(prog, corre32, carpeta, lineas, 'fuzz.txt')
    malos = []
    for n, c in enumerate(casos):
        dicho = sal[n] if n < len(sal) else '(nada)'
        if dicho != c[6]:
            malos.append('%s %r opc %d en %r desde %d: dice «%s» y Python «%s»' % (c[0], c[1], c[2], c[3], c[4], dicho, c[6]))
    if rc != 0 or malos:
        mal('fuzz contra el re de Python: %d de %d distintos (salida %d)' % (len(malos), len(casos), rc))
        for m in malos[:12]:
            print('        ' + m)
    else:
        bien('fuzz contra el re de Python: %d patrones al azar (con UTF-8, sin caja, palabra entera, literales, hacia atrás y reemplazos), todos iguales' % len(casos))


def rotos(prog, corre32, carpeta):
    r = random.Random(7)
    al = list('ab()[]{}|*+?\\^$.-,:0129dwsbnx') + ['ñ']
    lineas = []
    for _ in range(int(os.environ.get('FUZZ_EXP', '3000')) // 2):
        pat = ''.join(r.choice(al) for _ in range(r.randint(1, 12)))
        texto = ''.join(r.choice('ab(){}[]1 \nñ') for _ in range(r.randint(0, 20)))
        lineas.append(linea(r.choice('BA'), pat, r.choice([0, 1, 4]), texto, 0 if r.random() < 0.5 else len(texto.encode())))
    try:
        rc, sal = corre(prog, corre32, carpeta, lineas, 'rotos.txt')
    except subprocess.TimeoutExpired:
        mal('patrones rotos: se ha colgado')
        return
    buenos = sum(1 for s in sal if re.match(r'^(E \d+|- [01]|\d+ \d+ (\d+ \d+|x x) [01])$', s))
    errores = sum(1 for s in sal if s.startswith('E '))
    if rc != 0 or len(sal) != len(lineas) or buenos != len(lineas):
        mal('patrones rotos: %d de %d respuestas bien (salida %d)' % (buenos, len(lineas), rc))
    else:
        bien('patrones rotos al azar: %d, sin reventar ni colgarse (%d no compilan, con su mensaje)' % (len(lineas), errores))


def main():
    prog, corre32, carpeta = sys.argv[1:4]
    a_mano(prog, corre32, carpeta)
    tope(prog, corre32, carpeta)
    velocidad(prog, corre32, carpeta)
    fuzz(prog, corre32, carpeta)
    rotos(prog, corre32, carpeta)
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
