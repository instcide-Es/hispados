# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/skia-lista.py - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
# sección 22): desde el out/h64 del gn de Skia (en su carpeta: python3 -I
# skia-lista.py), terceros/skia/compila.txt (/tmp/skia.lista) y la lista de
# los archivos que hay que copiar a terceros/skia (/tmp/skia.archivos).
import subprocess, shlex, sys, os, collections
N = os.path.expanduser('~/h64e/herr/ninja'); O = 'out/h64'
libs = ['libskia.a', 'libskshaper.a', 'libskunicode_core.a', 'libskunicode_icu.a', 'libskcms.a']
vistos = collections.OrderedDict()
for l in libs:
    out = subprocess.run([N, '-C', O, '-t', 'commands', l], capture_output=True, text=True).stdout
    for línea in out.splitlines():
        a = shlex.split(línea)
        if not a or not (a[0].endswith('g++') or a[0].endswith('gcc')) or '-c' not in a: continue
        src = a[a.index('-c') + 1]; obj = a[a.index('-o') + 1]
        if obj in vistos: continue
        fl = []; i = 1
        while i < len(a):
            x = a[i]
            if x in ('-MD',): i += 1; continue
            if x in ('-MF', '-o', '-c'): i += 2; continue
            if x.startswith('-I'):
                d = x[2:]
                if d == '../..': x = '-I$K'
                elif d.startswith('../../'): x = '-I$K/' + d[6:]
                elif '/terceros/' in d: x = '-I$T/' + d.split('/terceros/')[1]
                elif d.endswith('/h64e/inc'): x = '$INC'
                else: x = '-I$K/' + os.path.relpath(d, '../..') if not d.startswith('/') else x
            fl.append(x); i += 1
        c = 'C' if a[0].endswith('gcc') else 'X'
        vistos[obj] = (c, ' '.join(fl), os.path.normpath(src)[6:] if src.startswith('../../') else src)
grupos = collections.OrderedDict()
for obj, (c, fl, src) in vistos.items():
    grupos.setdefault((c, fl), []).append(src)
with open('/tmp/skia.lista', 'w') as f:
    for (c, fl), srcs in grupos.items():
        f.write('@%s %s\n' % (c, fl))
        for s in srcs: f.write(s + '\n')
# las cabeceras
deps = subprocess.run([N, '-C', O, '-t', 'deps'] + list(vistos.keys()), capture_output=True, text=True).stdout
cab = set()
for l in deps.splitlines():
    l = l.strip()
    if l.startswith('../../'): cab.add(os.path.normpath(l)[6:])
srcs = set(s for _, _, s in vistos.values())
with open('/tmp/skia.archivos', 'w') as f:
    for x in sorted(cab | srcs): f.write(x + '\n')
print(len(vistos), 'objetos,', len(grupos), 'grupos,', len(cab), 'cabeceras,', len(srcs), 'fuentes')
