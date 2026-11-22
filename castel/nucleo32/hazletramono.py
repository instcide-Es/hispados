#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazletramono.py: hace nucleo32/letramono.cas, la letra de ancho
fijo suavizada del Bloc de notas para los tamaños grandes (Ctrl+rueda): DejaVu
Sans Mono y DejaVu Sans Mono Bold (la negrita, para las palabras clave), a
partir de los originales de /usr/share/fonts (o de DEJAVU=carpeta),
comprobados por su SHA-256 (los mismos de terceros/dejavu/LEEME.txt).

Como hazletrasuave.py (y con su ayuda en C, hazletrasuave.c): se rasteriza en
tiempo de compilación con FreeType y el «hinting» de la letra, y cada letra
va con 16 tonos por punto. De ancho fijo: todas avanzan lo mismo (se
comprueba) y no hay kerning. Tres tamaños: el renglón de 20 puntos (letra de
15), el de 24 (18) y el de 32 (24); normal y negrita: seis caras de las 224
letras de la página de Ventanas (la 437 con Á, Í, Ó, Ú, €, «…»).
El tamaño normal (8x16) sigue siendo la letra del sistema (letra.cas).

Necesita gcc y FreeType (libfreetype-dev) solo para volver a hacerla.
uso: python3 nucleo32/hazletramono.py        (desde castel/)
"""
import hashlib, os, subprocess, sys, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import hazletrasuave as hs  # noqa: E402

LETRAS = {
    'DejaVuSansMono.ttf': 'c805f9436dbc268644c1d9584f01a601a653e028e08fd74b9b949f6cf8304d88',
    'DejaVuSansMono-Bold.ttf': '3a3c502eeff669a231549e80df9f7c49de109bafe303170409e905d0b31a38fe',
}
# (alto del renglón, puntos de la letra) de cada tamaño
TAMAÑOS = [(20, 15), (24, 18), (32, 24)]


def main():
    carpeta = os.environ.get('DEJAVU', '/usr/share/fonts/truetype/dejavu')
    for nombre, suma in LETRAS.items():
        d = open(os.path.join(carpeta, nombre), 'rb').read()
        if hashlib.sha256(d).hexdigest() != suma:
            sys.exit('hazletramono.py: %s no es el DejaVu 2.37 de siempre' % nombre)
    with tempfile.TemporaryDirectory() as t:
        prog = os.path.join(t, 'hazletrasuave')
        cf = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'freetype2']).decode().split()
        subprocess.check_call(['gcc', '-O2', os.path.join(AQUI, 'hazletrasuave.c'), '-o', prog] + cf)
        caras = []
        for renglón, px in TAMAÑOS:
            for nombre in LETRAS:
                base, tabla, ini, pares, puntos = hs.cara(prog, os.path.join(carpeta, nombre), renglón, px)
                avances = set(tabla[i] for i in range(0, len(tabla), 8))
                assert len(avances) == 1, (nombre, px, avances)
                caras.append((renglón, px, nombre, avances.pop(), base, tabla, puntos))
    s = []
    s.append('; =============================================================================')
    s.append(';  letramono.cas - la letra de ancho fijo suavizada del Bloc de notas, para')
    s.append(';  los tamaños grandes (Ctrl+rueda): DejaVu Sans Mono y DejaVu Sans Mono Bold')
    s.append(';  (la negrita, para las palabras clave), con 16 tonos por punto, ya')
    s.append(';  rasterizadas (con FreeType y el «hinting» de la letra) para los renglones')
    s.append(';  de 20, 24 y 32 puntos. Las 224 letras de la página de Ventanas.')
    s.append(';  NO SE EDITA: lo hace nucleo32/hazletramono.py.')
    s.append(';')
    s.append(';  Cada cara, como las de letrasuave.cas: 224 letras (de la 32 a la 255) de 8')
    s.append(';  octetos (el avance, dónde empieza el dibujo y cuánto por debajo de lo alto')
    s.append(';  del renglón, con signo; su ancho, su alto, uno sin usar y dónde están sus')
    s.append(';  puntos, 16 bits) y los puntos: fila a fila, dos por octeto, de 0 a 15.')
    s.append(';  Todas las letras de una cara avanzan lo mismo (es de ancho fijo).')
    s.append(';')
    s.append(';  DejaVu: (c) Bitstream (Bitstream Vera) y los autores de DejaVu; licencia')
    s.append(';  de Bitstream Vera (terceros/dejavu/LICENSE). Libre de copiar y repartir.')
    s.append(';  Este módulo: Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o')
    s.append(';  posterior.')
    s.append('; =============================================================================')
    s.append('módulo LetraMono')
    s.append('')
    s.append('    público constante CARAS_MONO = %d       ; normal y negrita de cada tamaño' % len(caras))
    s.append('')
    s.append('    ; las caras: 2*t normal y 2*t+1 negrita (t: 0, renglón de 20; 1, de 24;')
    s.append('    ; 2, de 32); cada una, 4 palabras: la tabla, los puntos, el alto del')
    s.append('    ; renglón y lo que avanza cada letra')
    s.append('    ensamblador')
    s.append('    LM_caras:')
    for i, c in enumerate(caras):
        s.append('        dd LM_t%d, LM_p%d, %d, %d' % (i, i, c[0], c[3]))
    s.append('    fin')
    s.append('')
    total = 0
    for i, c in enumerate(caras):
        renglón, px, nombre, av, base, tabla, puntos = c
        s.append('    ; ---- cara %d: %s a %d puntos, renglón de %d (base en %d), avance %d: %d octetos de puntos ----'
                 % (i, nombre[:-4], px, renglón, base, av, len(puntos)))
        hs.bloques(s, 'LM_t%d' % i, tabla)
        hs.bloques(s, 'LM_p%d' % i, puntos)
        s.append('')
        total += len(tabla) + len(puntos)
    s.append('    ; la cara n (0 a %d): sus 4 palabras' % (len(caras) - 1))
    s.append('    público función cara_mono(n: nat32): puntero a nat32')
    s.append('        si n >= CARAS_MONO entonces n := 0 fin')
    s.append('        ensamblador')
    s.append('            mov eax, {n}')
    s.append('            shl eax, 4')
    s.append('            add eax, LM_caras')
    s.append('        fin')
    s.append('    fin')
    s.append('')
    s.append('fin LetraMono.')
    open(os.path.join(AQUI, 'letramono.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')
    print('letramono.cas: %d octetos de letra, avances %s' % (total, [c[3] for c in caras]))


if __name__ == '__main__':
    main()
