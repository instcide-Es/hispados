#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/hazblocdef.py: hace nucleo32/blocdef.cas, los lenguajes «de
serie» del Bloc de notas: los .LEN de nucleo32/bloc/lenguajes, tal cual (en
UTF-8), dentro del programa. Así se colorea aunque no estén los archivos (un
sistema recién instalado desde la memoria USB o el CD: INSTALAR no hace
carpetas dentro de las de \\HISPA); los de \\HISPA\\BLOC\\LENGUAJES y
\\BLOC\\LENGUAJES se leen antes y mandan (si se llaman igual, vale el
primero), y ahí se añaden más.
uso: python3 nucleo32/hazblocdef.py        (desde castel/)
"""
import glob, os, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import hazletrasuave as hs  # noqa: E402


def main():
    lens = sorted(glob.glob(os.path.join(AQUI, 'bloc', 'lenguajes', '*.LEN')))
    s = []
    s.append('; =============================================================================')
    s.append(';  blocdef.cas - los lenguajes de serie del Bloc de notas: los .LEN de')
    s.append(';  nucleo32/bloc/lenguajes, tal cual (en UTF-8), dentro de BLOC.BIN, por si')
    s.append(';  no están los archivos. NO SE EDITA: lo hace nucleo32/hazblocdef.py.')
    s.append(';  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.')
    s.append('; =============================================================================')
    s.append('módulo BlocDef')
    s.append('')
    s.append('    público constante DE_SERIE = %d' % len(lens))
    s.append('')
    s.append('    ; de cada uno: dónde empieza y cuánto mide')
    s.append('    ensamblador')
    s.append('    BD_tabla:')
    for i, f in enumerate(lens):
        n = len(open(f, 'rb').read())
        s.append('        dd BD_%d, %d     ; %s' % (i, n, os.path.basename(f)))
    s.append('    fin')
    s.append('')
    for i, f in enumerate(lens):
        hs.bloques(s, 'BD_%d' % i, list(open(f, 'rb').read()))
    s.append('')
    s.append('    ; el de serie número i: dónde empieza (y en n, cuánto mide)')
    s.append('    público función de_serie(i: nat32, ref n: nat32): puntero a byte')
    s.append('        variable p: puntero a nat32')
    s.append('        si i >= DE_SERIE entonces')
    s.append('            n := 0')
    s.append('            devolver nulo')
    s.append('        fin')
    s.append('        ensamblador')
    s.append('            mov eax, BD_tabla')
    s.append('            mov {p}, eax')
    s.append('        fin')
    s.append('        n := p[i * 2 + 1]')
    s.append('        devolver puntero(p[i * 2])')
    s.append('    fin')
    s.append('')
    s.append('fin BlocDef.')
    open(os.path.join(AQUI, 'blocdef.cas'), 'w', encoding='utf-8').write('\n'.join(s) + '\n')
    print('blocdef.cas: %d lenguajes' % len(lens))


if __name__ == '__main__':
    main()
