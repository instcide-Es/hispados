#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""compat-resumen.py [CARPETA]

(COMPAT) El resumen acumulado de los informes del Asistente de
compatibilidad (los de /root/FTP/HispaDOS/Compatibilidad/<AAAAMMDD>/*.txt,
o los de CARPETA): qué funciones de Windows piden más los programas, para
saber qué hacer antes en WIN32. Cada programa cuenta una vez (el mismo
conjunto de .EXE, por sus SHA-256; si no hay suma, por sus nombres) y con su
último informe (así lo que ya se ha hecho deja de contar cuando llegan
informes con el win32 nuevo). Lo de los archivos que no hacen falta (el
instalador, el servidor dedicado...) se cuenta aparte.

Deja en CARPETA:
  resumen.csv   dll;funcion;programas;programas_no_importantes;informes;ejemplos
                (funcion vacía: falta la DLL entera)
  resumen.json  lo mismo, con la fecha, los totales y las versiones de win32
"""
import collections
import glob
import json
import os
import re
import sys
import time

C = sys.argv[1] if len(sys.argv) > 1 else '/root/FTP/HispaDOS/Compatibilidad'
R = re.compile(r'^(\d{8})/(\d{6})-([0-9a-f]{8})-[0-9a-f]{6}\.txt$')


def lee(r):
    d = {'archivos': [], 'faltas': [], 'win32': '?', 'hispa': '?'}
    try:
        t = open(r, encoding='utf-8').read()
    except (OSError, UnicodeDecodeError):
        return None
    l = t.split('\n')
    if not l or l[0] != 'HISPA-COMPAT 1':
        return None
    for x in l[1:]:
        k, _, v = x.partition('=')
        c = v.split('\t')
        if k == 'win32':
            d['win32'] = v
        elif k == 'hispa-pentium':
            d['hispa'] = v
        elif k == 'archivo' and len(c) == 6:
            d['archivos'].append({'nombre': c[0].upper(), 'sha': c[2], 'clase': c[3], 'importa': c[5] == 'si'})
        elif k == 'falta' and len(c) == 3:
            d['faltas'].append((c[0].upper(), c[1].upper(), c[2]))
        elif k == 'faltadll' and len(c) == 3:
            d['faltas'].append((c[0].upper(), c[1].upper(), ''))
    return d


def main():
    ultimos = {}
    n_inf = 0
    for r in sorted(glob.glob(os.path.join(C, '2*', '*.txt'))):
        rel = os.path.relpath(r, C).replace(os.sep, '/')
        m = R.match(rel)
        if not m:
            continue
        d = lee(r)
        if not d:
            continue
        n_inf += 1
        exes = sorted(a['sha'] if a['sha'] != '-' else a['nombre'] for a in d['archivos'] if a['clase'] in ('ventanas', 'consola', 'programa'))
        clave = '|'.join(exes) or '|'.join(sorted(a['nombre'] for a in d['archivos']))
        d['cuando'] = m.group(1) + m.group(2)
        d['ejemplo'] = ', '.join(sorted({a['nombre'] for a in d['archivos'] if a['clase'] in ('ventanas', 'consola', 'programa') and a['importa']}))[:80]
        if clave not in ultimos or ultimos[clave]['cuando'] <= d['cuando']:
            ultimos[clave] = d
    prog = collections.Counter()
    prog_no = collections.Counter()
    informes = collections.Counter()
    ejemplos = collections.defaultdict(list)
    versiones = collections.Counter()
    for d in ultimos.values():
        versiones[d['win32']] += 1
        importa = {a['nombre']: a['importa'] for a in d['archivos']}
        vistas = set()
        for arch, dll, fn in d['faltas']:
            k = (dll, fn)
            imp = importa.get(arch, True)
            if (k, imp) in vistas:
                continue
            vistas.add((k, imp))
            (prog if imp else prog_no)[k] += 1
            if imp and len(ejemplos[k]) < 3 and d['ejemplo'] and d['ejemplo'] not in ejemplos[k]:
                ejemplos[k].append(d['ejemplo'])
    for r in glob.glob(os.path.join(C, '2*', '*.txt')):
        d = lee(r)
        if d:
            for k in {(dll, fn) for _, dll, fn in d['faltas']}:
                informes[k] += 1
    claves = sorted(set(prog) | set(prog_no), key=lambda k: (-prog[k], -prog_no[k], k))
    filas = [{'dll': k[0], 'funcion': k[1], 'programas': prog[k], 'programas_no_importantes': prog_no[k],
              'informes': informes[k], 'ejemplos': ejemplos[k]} for k in claves]
    tmp = os.path.join(C, '.resumen.csv')
    with open(tmp, 'w', encoding='utf-8') as f:
        f.write('dll;funcion;programas;programas_no_importantes;informes;ejemplos\n')
        for x in filas:
            f.write('%s;%s;%d;%d;%d;%s\n' % (x['dll'], x['funcion'], x['programas'], x['programas_no_importantes'], x['informes'],
                                           ' / '.join(e.replace(';', ',') for e in x['ejemplos'])))
    os.replace(tmp, os.path.join(C, 'resumen.csv'))
    tmp = os.path.join(C, '.resumen.json')
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump({'generado': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'informes': n_inf, 'programas': len(ultimos),
                   'win32': dict(versiones), 'funciones': filas}, f, ensure_ascii=False, indent=1)
    os.replace(tmp, os.path.join(C, 'resumen.json'))
    print('compat-resumen: %d informes, %d programas, %d funciones' % (n_inf, len(ultimos), len(filas)))


if __name__ == '__main__':
    main()
