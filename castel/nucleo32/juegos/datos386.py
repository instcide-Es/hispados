#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/juegos/datos386.py SALIDA.h LETRA.h

Saca de los fuentes de los juegos de Hispa-386 (juegos/*.asm, Hispa
Ventanas) lo que los juegos de Hispa-Pentium usan tal cual, para que haya un
solo sitio donde están: los 24 niveles de Almacén, los tres jardines de
Tragabolas, los dibujos de las cartas del Solitario (las pintas francesas y
españolas, en tres tamaños, y las figuras) con sus colores, y los iconos de
32 x 32 de cada juego (los de la cabecera HVP). Lo escribe como un .h de C.
En LETRA.h, la letra de 8x16 del escritorio (nucleo32/letra.cas, la de
Hispa Ventanas). Lo llama hacer.sh; no hace falta pasarlo a mano.
"""
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, '..', '..', '..')
JUEGOS = os.path.join(RAIZ, 'juegos')

CADENA = re.compile(r'^\s*db\s+"([^"]*)"\s*(,\s*0)?\s*(;.*)?$')
ETIQUETA = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*):')
# los nombres de los colores de Hispa Ventanas (ventanas/defs.inc) y los del cubo
COLORES = {
    'C_TINTA': 0, 'C_MARRON': 1, 'C_PETROLEO': 2, 'C_PETROLEO_CL': 3, 'C_TERRACOTA': 4,
    'C_TERRACOTA_CL': 5, 'C_ARENA': 6, 'C_ARENA_OSC': 7, 'C_CREMA': 8, 'C_BLANCO': 9,
    'C_OCRE': 10, 'C_OLIVA': 11, 'C_ANIL': 12, 'C_GRANATE': 13, 'C_TURQUESA': 14, 'C_PIEDRA': 15,
    'ROJO': 16, 'NEGRO': 17, 'PIEL': 18, 'GRIS': 19,
}


def lineas(nombre):
    return open(os.path.join(JUEGOS, nombre), encoding='utf-8').read().split('\n')


def cadenas_desde(ls, i, alto=None):
    """los «db "..."» seguidos desde la línea i (saltando vacías y
    comentarios) hasta otra cosa (o hasta tener «alto»)"""
    r = []
    while i < len(ls):
        l = ls[i]
        m = CADENA.match(l)
        if m:
            r.append(m.group(1))
            if alto and len(r) == alto:
                break
        elif l.strip() == '' or l.strip().startswith(';'):
            pass
        else:
            break
        i += 1
    return r


def busca(ls, etiqueta):
    for i, l in enumerate(ls):
        if l.startswith(etiqueta + ':') or re.match(r'^' + re.escape(etiqueta) + r'\s', l):
            return i
    sys.exit('datos386: no está «%s»' % etiqueta)


def icono(nombre):
    ls = lineas(nombre)
    for i, l in enumerate(ls):
        if l.startswith('HVP_CABECERA'):
            c = cadenas_desde(ls, i + 1, 32)
            assert len(c) == 32 and all(len(x) == 32 for x in c), nombre
            return c
    sys.exit('datos386: %s no tiene icono' % nombre)


def c_texto(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def almacen():
    ls = lineas('almacen.asm')
    i = busca(ls, 'niveles') + 1
    niveles, filas = [], []
    while not ls[i].startswith('niveles_fin'):
        l = ls[i].strip()
        m = CADENA.match(ls[i])
        if m:
            filas.append(m.group(1))
        elif re.match(r'^db\s+0\s*(;.*)?$', l):
            niveles.append(filas)
            filas = []
        i += 1
    assert len(niveles) == 24, len(niveles)
    return niveles


def jardines():
    ls = lineas('tragabolas.asm')
    i = busca(ls, 'labs') + 1
    c = cadenas_desde(ls, i)
    assert len(c) == 3 * 17 and all(len(x) == 25 for x in c), len(c)
    return [c[k * 17:(k + 1) * 17] for k in range(3)]


def dibujos(ls, etiqueta, n, an, al):
    i = busca(ls, etiqueta)
    c = cadenas_desde(ls, i + 1)
    assert len(c) == n * al and all(len(x) == an for x in c), (etiqueta, len(c))
    return ''.join(c)


def colores(ls, etiqueta):
    i = busca(ls, etiqueta)
    t = ls[i].split('db', 1)[1] + ',' + ls[i + 1].split('db', 1)[1]
    v = [COLORES[x.strip()] for x in t.split(';')[0].split(',') if x.strip()]
    assert len(v) == 12, (etiqueta, v)
    return v


def letra(salida):
    v = []
    for l in open(os.path.join(AQUI, '..', 'letra.cas'), encoding='utf-8'):
        m = re.match(r'^\s+db\s+(0x[0-9A-Fa-f]{2}(,0x[0-9A-Fa-f]{2})*)\s*$', l)
        if m:
            v += [int(x, 16) for x in m.group(1).split(',')]
    assert len(v) == 256 * 16, len(v)
    s = ['/* letra.h - la letra de 8x16 de Hispa-Pentium (nucleo32/letra.cas), para los juegos.',
         ' * NO SE EDITA: lo hace nucleo32/juegos/datos386.py.',
         ' * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */',
         'static const unsigned char letra8x16[256 * 16] = {']
    for i in range(0, len(v), 16):
        s.append('\t' + ', '.join('0x%02X' % x for x in v[i:i + 16]) + ',')
    s.append('};')
    # (y lo que es cada código: para pasar el UTF-8 de los fuentes a la página de Ventanas)
    sys.path.insert(0, os.path.join(RAIZ, 'tools'))
    import hispa437
    u = [ord(hispa437.letra(c)) if c >= 128 else c for c in range(256)]
    s.append('static const unsigned short uni437[256] = {')
    for i in range(0, 256, 16):
        s.append('\t' + ', '.join('0x%04X' % x for x in u[i:i + 16]) + ',')
    s.append('};')
    with open(salida, 'w', encoding='utf-8') as f:
        f.write('\n'.join(s) + '\n')


def main():
    if len(sys.argv) != 3:
        sys.exit('uso: datos386.py SALIDA.h LETRA.h')
    letra(sys.argv[2])
    s = ['/* datos386.h - lo de los juegos de Hispa-386 que usan los de Hispa-Pentium.',
         ' * NO SE EDITA: lo hace nucleo32/juegos/datos386.py de los fuentes de juegos/.',
         ' * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */',
         '#ifndef DATOS386_H', '#define DATOS386_H', '']
    for j, f in (('culebra', 'culebra.asm'), ('minas', 'minas.asm'), ('bloques', 'bloques.asm'),
                 ('solitario', 'solitario.asm'), ('tragabolas', 'tragabolas.asm'), ('almacen', 'almacen.asm')):
        s.append('static const char *const ico386_%s[32] = {' % j)
        s += ['\t' + c_texto(x) + ',' for x in icono(f)]
        s.append('};')
    s.append('')
    s.append('/* Almacén: H muro, . suelo, x marca, c caja, C caja en su marca, a el mozo, A el mozo en una marca */')
    s.append('#define ALM_NIVELES 24')
    s.append('static const char *const alm_nivel[ALM_NIVELES] = {')
    for n in almacen():
        s.append('\t' + c_texto('\n'.join(n)) + ',')
    s.append('};')
    s.append('')
    s.append('/* Tragabolas: # seto, . bolita, o campana (25 x 17) */')
    s.append('static const char *const trg_jardin[3] = {')
    for j in jardines():
        s.append('\t' + c_texto(''.join(j)) + ',')
    s.append('};')
    s.append('')
    ls = lineas('solitario.asm')
    s.append('/* Solitario: «.» transparente, 0-9 a-f los 16 colores de Ventanas, x/y/z los del palo,')
    s.append(' * r rojo, n negro, p piel. Francesa: corazones, diamantes, picas, tréboles;')
    s.append(' * española: oros, copas, espadas, bastos */')
    for nombre, n, an, al in (('d_pipf', 4, 13, 13), ('d_pipe', 4, 13, 13), ('d_peqf', 4, 9, 9), ('d_peqe', 4, 9, 9),
                              ('d_p7f', 4, 7, 7), ('d_p7e', 4, 7, 7), ('d_figf', 3, 19, 20), ('d_fige', 3, 19, 26)):
        s.append('static const char sol_%s[] = %s;' % (nombre[2:], c_texto(dibujos(ls, nombre, n, an, al))))
    s.append('/* los tres colores (x, y, z) de las figuras de cada palo (0-15 Ventanas; 16 rojo, 17 negro) */')
    s.append('static const unsigned char sol_colf[12] = { %s };' % ', '.join(map(str, colores(ls, 'fig_colf'))))
    s.append('static const unsigned char sol_cole[12] = { %s };' % ', '.join(map(str, colores(ls, 'fig_cole'))))
    s.append('')
    s.append('#endif')
    with open(sys.argv[1], 'w', encoding='utf-8') as f:
        f.write('\n'.join(s) + '\n')


if __name__ == '__main__':
    main()
