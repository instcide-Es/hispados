#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazletrasuave.py: hace nucleo32/letrasuave.cas, la letra
suavizada de los tamaños grandes de controles.cas (la «letra grande» de los
programas: 12x24 y 16x32), a partir de DejaVu Sans y DejaVu Sans Bold
(terceros/dejavu: licencia de Bitstream Vera; aquí se usan los originales,
con su «hinting» y su tabla de kerning, de /usr/share/fonts o de DEJAVU=
carpeta, comprobados por su SHA-256).

Rasteriza en tiempo de compilación con FreeType (con el «hinting» de la
propia letra y 256 tonos), y guarda cada letra con 16 tonos (4 bits por
punto), su avance y su kerning: proporcional, como la de cualquier sistema
de hoy. Dos tamaños (el renglón de 24 puntos: letra de 18; el de 32: letra
de 24), en normal y en negrita: las 224 letras de la página de Ventanas
(la 437 con Á, Í, Ó, Ú, €, «…»: tools/hispa437.py).

Necesita gcc y FreeType (libfreetype-dev) solo para volver a hacerla.
uso: python3 nucleo32/hazletrasuave.py        (desde castel/)
"""
import hashlib, os, subprocess, sys, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, '..', '..')
sys.path.insert(0, os.path.join(RAIZ, 'tools'))
import hispa437  # noqa: E402

LETRAS = {
    'DejaVuSans.ttf': 'ae7b7855e115a5966d8b1b3f80f254ccc117ec86f9965e202ee2940453837280',
    'DejaVuSans-Bold.ttf': '5c1247acef7f2b8522a31742c76d6adcb5569bacc0be7ceaa4dc39dd252ce895',
}
# (alto del renglón, puntos de la letra) de cada tamaño
TAMAÑOS = [(24, 18), (32, 24)]
MODO = 1            # el «hinting» de la propia letra (DejaVu lo trae bueno)
GAMMA = 0.85        # un poco más de tinta en los bordes (sobre fondo claro se ve más firme)
PRIMERA = 32        # de la 32 (el espacio) a la 255


def rasteriza(prog, letra, px):
    cods = [ord(hispa437.letra(c)) for c in range(PRIMERA, 256)]
    sal = subprocess.check_output([prog, letra, str(px), str(MODO)] + [str(u) for u in cods]).decode()
    G, K = {}, {}
    for l in sal.splitlines():
        p = l.split()
        if p[0] == 'M':
            asc, desc = int(p[1]), int(p[2])
        elif p[0] == 'G':
            u, av, iz, arr, an, al = map(int, p[1:7])
            G[u] = (av, iz, arr, an, al, bytes.fromhex(p[7]) if len(p) > 7 else b'')
        elif p[0] == 'K':
            k = round(int(p[3]) / 64)
            if k:
                K[(int(p[1]), int(p[2]))] = k
    return asc, desc, cods, G, K


def cara(prog, letra, renglón, px):
    """los octetos de una cara: la tabla (8 octetos por letra), el comienzo
    del kerning de cada letra (2 octetos, uno más al final), los pares
    (2 octetos: la letra de la derecha y el ajuste) y los puntos"""
    asc, desc, cods, G, K = rasteriza(prog, letra, px)
    base = (renglón - (asc + desc)) // 2 + asc
    tabla, ini, pares, puntos = [], [], [], []
    for i, u in enumerate(cods):
        av, iz, arr, an, al, bm = G[u]
        de_arriba = base - arr
        assert -128 <= iz < 128 and -128 <= de_arriba < 128 and an < 256 and al < 256 and av < 256
        off = len(puntos)
        assert off < 65536
        for y in range(al):
            fila = []
            for x in range(an):
                v = bm[y * an + x] / 255
                fila.append(round((v ** GAMMA) * 15))
            if len(fila) % 2:
                fila.append(0)
            for x in range(0, len(fila), 2):
                puntos.append(fila[x] << 4 | fila[x + 1])
        tabla += [av, iz & 255, de_arriba & 255, an, al, 0, off & 255, off >> 8]
        ini.append(len(pares) // 2)
        for j, w in enumerate(cods):
            k = K.get((u, w))
            if k:
                pares += [PRIMERA + j, k & 255]
    ini.append(len(pares) // 2)
    ini2 = []
    for v in ini:
        ini2 += [v & 255, v >> 8]
    return base, tabla, ini2, pares, puntos


def bloques(s, etiqueta, datos, ancho=40, largo=5000):
    """los octetos en «db», en bloques «ensamblador» que no pasen de «largo»
    letras (CASTEL no admite un bloque de más de 6000)"""
    filas = [datos[i:i + ancho] for i in range(0, len(datos), ancho)] or [[0]]
    n = largo
    primero = True
    for f in filas:
        l = '        db ' + ','.join(str(v) for v in f)
        if n + len(l) + 1 > largo:
            if not primero:
                s.append('    fin')
            s.append('    ensamblador')
            if primero:
                s.append('    %s:' % etiqueta)
            primero = False
            n = 0
        s.append(l)
        n += len(l) + 1
    s.append('    fin')


def main():
    carpeta = os.environ.get('DEJAVU', '/usr/share/fonts/truetype/dejavu')
    for nombre, suma in LETRAS.items():
        d = open(os.path.join(carpeta, nombre), 'rb').read()
        if hashlib.sha256(d).hexdigest() != suma:
            sys.exit('hazletrasuave.py: %s no es el DejaVu 2.37 de siempre' % nombre)
    with tempfile.TemporaryDirectory() as t:
        prog = os.path.join(t, 'hazletrasuave')
        cf = subprocess.check_output(['pkg-config', '--cflags', '--libs', 'freetype2']).decode().split()
        subprocess.check_call(['gcc', '-O2', os.path.join(AQUI, 'hazletrasuave.c'), '-o', prog] + cf)
        caras = []
        for renglón, px in TAMAÑOS:
            for nombre in LETRAS:
                caras.append((renglón, px, nombre) + cara(prog, os.path.join(carpeta, nombre), renglón, px))
    s = []
    s.append('; =============================================================================')
    s.append(';  letrasuave.cas - la letra suavizada de los tamaños grandes de controles.cas')
    s.append(';  (la «letra grande» de los programas): DejaVu Sans y DejaVu Sans Bold,')
    s.append(';  proporcionales, con 16 tonos por punto y su kerning, ya rasterizadas (con')
    s.append(';  FreeType y el «hinting» de la letra) para el renglón de 24 puntos (letra')
    s.append(';  de %d) y el de 32 (letra de %d). Las 224 letras de la página de Ventanas.' % (TAMAÑOS[0][1], TAMAÑOS[1][1]))
    s.append(';  NO SE EDITA: lo hace nucleo32/hazletrasuave.py.')
    s.append(';')
    s.append(';  Cada cara: 224 letras (de la 32 a la 255) de 8 octetos: el avance, dónde')
    s.append(';  empieza el dibujo (a la derecha del punto de la pluma; con signo), cuánto')
    s.append(';  por debajo de lo alto del renglón (con signo), su ancho y su alto, uno sin')
    s.append(';  usar, y dónde están sus puntos (16 bits). Los puntos: fila a fila, dos por')
    s.append(';  octeto (el de la izquierda arriba), de 0 (nada) a 15 (tinta entera);')
    s.append(';  cada fila acaba en un octeto entero. El kerning: de cada letra, dónde')
    s.append(';  empiezan sus pares (16 bits; 225, el último dice dónde se acaban) y los')
    s.append(';  pares: la letra de la derecha y el ajuste en puntos (con signo).')
    s.append(';')
    s.append(';  DejaVu: (c) Bitstream (Bitstream Vera) y los autores de DejaVu; licencia')
    s.append(';  de Bitstream Vera (terceros/dejavu/LICENSE). Libre de copiar y repartir.')
    s.append(';  Este módulo: Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o')
    s.append(';  posterior.')
    s.append('; =============================================================================')
    s.append('módulo LetraSuave')
    s.append('')
    s.append('    ; las caras: 0, renglón de 24 normal; 1, de 24 en negrita; 2 y 3, las de 32')
    s.append('    ; cada una, 6 palabras: la tabla, el comienzo del kerning, los pares, los')
    s.append('    ; puntos, el alto del renglón y dónde va la línea de base')
    s.append('    ensamblador')
    s.append('    LS_caras:')
    for i, c in enumerate(caras):
        s.append('        dd LS_t%d, LS_i%d, LS_k%d, LS_p%d, %d, %d' % (i, i, i, i, c[0], c[3]))
    s.append('    fin')
    s.append('')
    total = 0
    for i, c in enumerate(caras):
        renglón, px, nombre, base, tabla, ini, pares, puntos = c
        s.append('    ; ---- cara %d: %s a %d puntos, renglón de %d (base en %d): %d octetos de puntos, %d pares ----'
                 % (i, nombre[:-4], px, renglón, base, len(puntos), len(pares) // 2))
        bloques(s, 'LS_t%d' % i, tabla)
        bloques(s, 'LS_i%d' % i, ini)
        bloques(s, 'LS_k%d' % i, pares)
        bloques(s, 'LS_p%d' % i, puntos)
        s.append('')
        total += len(tabla) + len(ini) + len(pares) + len(puntos)
    s.append('    ; la cara n (0 a 3): sus 6 palabras')
    s.append('    público función cara_suave(n: nat32): puntero a nat32')
    s.append('        ensamblador')
    s.append('            mov eax, {n}')
    s.append('            and eax, 3')
    s.append('            imul eax, eax, 24')
    s.append('            add eax, LS_caras')
    s.append('        fin')
    s.append('    fin')
    s.append('')
    s.append('fin LetraSuave.')
    open(os.path.join(AQUI, 'letrasuave.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')
    print('letrasuave.cas: %d octetos de letra' % total)


if __name__ == '__main__':
    main()
