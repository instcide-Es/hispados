#!/usr/bin/env python3
# w32/gl/hazstubs.py - OPENGL32.DLL tiene que exportar todas las funciones de
# OpenGL 1.1 (un programa que importa una que no está no llega a arrancar):
# las que HispaGL no hace (los evaluadores, sobre todo) salen de aquí, vacías
# y con sus argumentos (stdcall: quitan de la pila lo que les toca). Lee las
# de GL/gl.h (una por línea), mira cuáles ya están en los objetos (nm) y
# escribe:
#   - STUBS.c: las que faltan (devuelven 0);
#   - EXPORTA.txt: la lista de lo que exporta la DLL (gl* y wgl*), separada
#     por comas, para tools/elf2pe.py --exp.
#   hazstubs.py GL/gl.h STUBS.c EXPORTA.txt obj.o...
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import re, sys, subprocess

def main():
    gl_h, stubs, exporta, objs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
    definidas = set()
    for o in objs:
        out = subprocess.run(['nm', '-g', '--defined-only', o], capture_output=True, text=True, check=True).stdout
        for l in out.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'TDBR':
                definidas.add(p[2])
    protos = []
    for l in open(gl_h, encoding='utf-8'):
        m = re.match(r'GLAPI\s+(.+?)\s*APIENTRY\s+(\w+)\s*\((.*)\);', l)
        if m:
            protos.append((m.group(1).strip(), m.group(2), m.group(3).strip()))
    faltan = [p for p in protos if p[1] not in definidas]
    with open(stubs, 'w', encoding='utf-8') as f:
        f.write('/* NO SE EDITA: lo hace w32/gl/hazstubs.py (las de OpenGL 1.1 que HispaGL no hace) */\n')
        f.write('#include "GL/gl.h"\n')
        for ret, nom, args in faltan:
            cuerpo = '' if ret == 'void' else ' return 0;'
            f.write('%s APIENTRY %s(%s) {%s }\n' % (ret, nom, args, cuerpo))
    nombres = sorted((definidas | {p[1] for p in protos}))
    nombres = [n for n in nombres if n.startswith('gl') or n.startswith('wgl')]
    open(exporta, 'w').write(','.join(nombres))
    print('hazstubs: %d de gl.h, %d vacías, %d exportadas' % (len(protos), len(faltan), len(nombres)))

main()
