#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazblocpag.py: pone al día las tablas de blocpag.cas (el módulo
BlocPag del Bloc de notas: los juegos de caracteres), entre las marcas
«(lo que sigue lo hace hazblocpag.py)» y «(hasta aquí, hazblocpag.py)».

  - la 437 de casa (tools/hispa437.py: la 437 con Á, Í, Ó, Ú, €...) y la
    Windows-1252 a Unicode: 256 códigos cada una (los de control, 0..31 y
    127, son ellos mismos; los cinco huecos de la 1252 —81, 8D, 8F, 90, 9D—
    van a U+0081.., los de control C1: ida y vuelta exacta);
  - al revés: de Latin-1 (U+0000..00FF) a la 437, una tabla directa, y lo
    demás de la 437 (los recuadros, el griego, €...) y de la 1252 en dos
    tablas ordenadas por Unicode, para buscar por la mitad.

uso: python3 nucleo32/hazblocpag.py        (desde castel/)
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', '..', 'tools'))
import hispa437  # noqa: E402

HUECOS_1252 = (0x81, 0x8D, 0x8F, 0x90, 0x9D)


def de_437(c):
    return ord(hispa437.letra(c))


def de_1252(c):
    if c in HUECOS_1252:
        return c
    return ord(bytes([c]).decode('cp1252'))


def lista(nombre, tipo, valores, por_linea=12):
    s = ['    variable %s: arreglo [%d] de %s = (' % (nombre, len(valores), tipo)]
    for i in range(0, len(valores), por_linea):
        trozo = ', '.join('0x%04X' % v for v in valores[i:i + por_linea])
        s.append('        ' + trozo + (',' if i + por_linea < len(valores) else ')'))
    return s


def tablas():
    t437 = [de_437(c) for c in range(256)]
    t1252 = [de_1252(c) for c in range(256)]
    assert len(set(t437)) == 256 and len(set(t1252)) == 256
    l1 = [0xFFFF] * 256
    for c, u in enumerate(t437):
        if u < 256:
            l1[u] = c
    o437 = sorted((u, c) for c, u in enumerate(t437) if u >= 256)
    o1252 = sorted((u, c) for c, u in enumerate(t1252) if u >= 256)
    s = []
    s.append('    ; (lo que sigue lo hace hazblocpag.py: no se edita a mano)')
    s.append('    ; la 437 de casa y la 1252, a Unicode')
    s += lista('t437', 'nat16', t437)
    s += lista('t1252', 'nat16', t1252)
    s.append('    ; de Latin-1 (U+0000..00FF) a la 437 de casa (0xFFFF: no está)')
    s += lista('l1_437', 'nat16', l1)
    s.append('    ; lo demás de la 437 y de la 1252, por orden de Unicode: el carácter y su código')
    s.append('    constante N437 = %d' % len(o437))
    s += lista('o437_u', 'nat16', [u for u, c in o437])
    s += lista('o437_c', 'nat16', [c for u, c in o437])
    s.append('    constante N1252 = %d' % len(o1252))
    s += lista('o1252_u', 'nat16', [u for u, c in o1252])
    s += lista('o1252_c', 'nat16', [c for u, c in o1252])
    s.append('    ; (hasta aquí, hazblocpag.py)')
    return s


def main():
    ruta = os.path.join(AQUI, 'blocpag.cas')
    l = open(ruta, encoding='utf-8').read().split('\n')
    a = next(i for i, x in enumerate(l) if 'lo que sigue lo hace hazblocpag.py' in x)
    b = next(i for i, x in enumerate(l) if 'hasta aquí, hazblocpag.py' in x)
    l[a:b + 1] = tablas()
    open(ruta, 'w', encoding='utf-8').write('\n'.join(l))
    print('blocpag.cas: tablas al día')


if __name__ == '__main__':
    main()
