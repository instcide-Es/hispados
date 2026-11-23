#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazletra437.py: hace nucleo32/letra437.cas, la letra de la consola
de texto cuando no hay modo de texto (UEFI: la pinta textouefi.cas en la
pantalla del firmware). Es la de la HispaBIOS (tools/mkfuentebios.py): la de
letra.cas con los recuadros, los bloques, las flechas y los símbolos de la
página 437 que la consola de texto necesita (INSTALAR, los programas de modo
texto). Solo hay que volver a pasarlo si cambia el diseño.

uso: python3 nucleo32/hazletra437.py        (desde castel/)
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
sys.path.insert(0, os.path.join(AQUI, '..', '..', 'tools'))
import hazletra          # noqa: E402  (la tabla de letra.cas)
import mkfuentebios      # noqa: E402  (lo que le añade la HispaBIOS)


def main():
    filas = mkfuentebios.rellenar(hazletra.tabla())
    assert len(filas) == 256 and all(len(f) == 16 for f in filas)
    s = ['; =============================================================================',
         ';  letra437.cas - (UEFI) la letra de la consola de texto pintada en la',
         ';  pantalla gráfica del firmware (textouefi.cas): 256 letras de 8x16, la',
         ';  página 437 entera, con sus recuadros y bloques (la de la HispaBIOS,',
         ';  tools/mkfuentebios.py). NO SE EDITA: lo hace nucleo32/hazletra437.py.',
         ';  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.',
         '; =============================================================================',
         'módulo Letra437', '']
    for c in range(256):
        if c % 64 == 0:
            s.append('    ensamblador')
            if c == 0:
                s.append('    HP_letra437:')
        s.append('        db ' + ','.join('0x%02X' % v for v in filas[c]))
        if c % 64 == 63:
            s.append('    fin')
            s.append('')
    s += ['    ; dónde están las 16 filas de la letra c',
          '    público función dibujo_437(c: nat32): puntero a byte',
          '        ensamblador',
          '            mov eax, {c}',
          '            and eax, 255',
          '            shl eax, 4',
          '            add eax, HP_letra437',
          '        fin',
          '    fin', '', 'fin Letra437.']
    open(os.path.join(AQUI, 'letra437.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')


if __name__ == '__main__':
    main()
