#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazletra.py: hace nucleo32/letra.cas, la letra de mapa de bits del
entorno gráfico de Hispa-Pentium, a partir del diseño propio de Hispa
Ventanas (ventanas/fuente.txt, con tools/mkfuente.py): 256 letras de 8x16
puntos, en la página 437 con las letras de más de Ventanas (Á, Í, Ó, Ú, €...:
tools/hispa437.py). Solo hay que volver a pasarlo si cambia el diseño.

uso: python3 nucleo32/hazletra.py        (desde castel/)
"""
import os, re, subprocess, sys, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, '..', '..')


def tabla():
    """las 256 letras, cada una sus 16 filas (un octeto por fila)"""
    with tempfile.TemporaryDirectory() as t:
        inc = os.path.join(t, 'fuente.inc')
        subprocess.check_call([sys.executable, os.path.join(RAIZ, 'tools', 'mkfuente.py'),
                               os.path.join(RAIZ, 'ventanas', 'fuente.txt'), inc])
        filas = []
        for l in open(inc):
            m = re.match(r'\s+db\s+([0-9A-Fa-fx,]+)', l)
            if m:
                filas.append([int(v, 16) for v in m.group(1).split(',')])
    assert len(filas) == 256 and all(len(f) == 16 for f in filas)
    return filas


def main():
    filas = tabla()
    s = []
    s.append('; =============================================================================')
    s.append(';  letra.cas - la letra de mapa de bits del entorno gráfico de Hispa-Pentium:')
    s.append(';  256 letras de 8x16 puntos (16 octetos cada una, una fila por octeto, el')
    s.append(';  punto de la izquierda en el bit alto). Es la «Hispa 8x16» de Hispa Ventanas')
    s.append(';  (ventanas/fuente.txt, diseño propio), en la página 437 con las letras de más')
    s.append(';  de Ventanas (Á, Í, Ó, Ú, €...). NO SE EDITA: lo hace nucleo32/hazletra.py.')
    s.append(';  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.')
    s.append('; =============================================================================')
    s.append('módulo Letra')
    s.append('')
    s.append('    público constante ANCHO_LETRA = 8')
    s.append('    público constante ALTO_LETRA = 16')
    s.append('')
    s.append('    ensamblador')
    s.append('    HP_letra8x16:')
    for c in range(256):
        s.append('        db ' + ','.join('0x%02X' % v for v in filas[c]))
    s.append('    fin')
    s.append('')
    s.append('    ; dónde están las 16 filas de la letra c')
    s.append('    público función dibujo_letra(c: nat32): puntero a byte')
    s.append('        ensamblador')
    s.append('            mov eax, {c}')
    s.append('            and eax, 255')
    s.append('            shl eax, 4')
    s.append('            add eax, HP_letra8x16')
    s.append('        fin')
    s.append('    fin')
    s.append('')
    s.append('fin Letra.')
    open(os.path.join(AQUI, 'letra.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')


if __name__ == '__main__':
    main()
