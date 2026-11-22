#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/win32/hazexp.py SALIDA.c FUENTES.c...: las tablas de lo que
exportan las DLL de dentro de WIN32.BIN (KERNEL32, USER32...): cada función
que empieza una línea con «API(tipo) Nombre(» (o «EXPC(tipo) Nombre(», las de
C, como wsprintfA) es de la DLL de su archivo (kernel32.c y archivos.c,
KERNEL32; user32.c..., USER32...), o de la que diga la última línea
«/* == DLL == */» de encima. «/* EXPORTA Nombre=simbolo */» exporta con otro
nombre."""
import re, sys, os
POR_ARCHIVO = {'kernel32': 'KERNEL32', 'archivos': 'KERNEL32', 'user32': 'USER32',
               'controles': 'USER32', 'menus': 'USER32', 'dialogos': 'USER32',
               'dx': 'USER32', 'recursos': 'USER32', 'gdi32': 'GDI32', 'comdlg32': 'COMDLG32', 'otros': 'ADVAPI32',
               'red': 'WSOCK32', 'registro': 'ADVAPI32'}
TODAS = ['KERNEL32', 'USER32', 'GDI32', 'COMDLG32', 'ADVAPI32', 'SHELL32', 'WSOCK32', 'VERSION', 'WINSPOOL']
tablas = {d: [] for d in TODAS}
for f in sys.argv[2:]:
    dll = POR_ARCHIVO.get(os.path.splitext(os.path.basename(f))[0], 'KERNEL32')
    for l in open(f, encoding='utf-8'):
        m = re.match(r'/\* == (\w+) == \*/', l)
        if m:
            dll = m.group(1)
            continue
        m = re.match(r'(?:API|EXPC)\([^)]*\)\s+(\w+)\s*\(', l)
        if m:
            tablas[dll].append((m.group(1), m.group(1)))
            continue
        m = re.match(r'/\* EXPORTA (\w+)=(\w+) \*/', l)
        if m:
            tablas[dll].append((m.group(1), m.group(2)))
# (JUEGOS) cada una entra por un trampolín que toma el cerrojo grande de
# WIN32.BIN (hilos.c, gc_puente), menos estas: no tocan nada compartido, o
# no vuelven (o lo hacen ellas)
SIN_CERROJO = {'ExitProcess', 'ExitThread', 'RaiseException', 'RtlUnwind', 'EnterCriticalSection',
               'LeaveCriticalSection', 'TryEnterCriticalSection', 'InitializeCriticalSection',
               'InitializeCriticalSectionAndSpinCount', 'InitializeCriticalSectionEx', 'DeleteCriticalSection',
               'SetCriticalSectionSpinCount', 'TlsGetValue', 'TlsSetValue', 'FlsGetValue', 'FlsSetValue',
               'GetCurrentThreadId', 'GetCurrentThread', 'GetCurrentProcess', 'GetCurrentProcessId',
               'GetLastError', 'SetLastError', 'GetTickCount', 'RtlCaptureContext',
               'HispaVuelveExcepcion'}
def trampolin(s):
    return s if s in SIN_CERROJO or s.startswith('Interlocked') else 'gc_t_' + s
with open(sys.argv[1], 'w') as o:
    o.write('/* exportan.c - NO SE EDITA: lo hace nucleo32/win32/hazexp.py */\n#include "w32.h"\n')
    vistos = set()
    for d in TODAS:
        for n, s in tablas[d]:
            if s not in vistos:
                vistos.add(s)
                o.write('extern char %s[];\n' % s)
                if trampolin(s) != s:
                    o.write('extern char %s[];\n__asm__(".text\\n%s: pushl $%s\\n jmp gc_puente\\n");\n' % (trampolin(s), trampolin(s), s))
    for d in TODAS:
        o.write('const Exp exp_%s[] = {\n' % d.lower())
        for n, s in sorted(set(tablas[d])):
            o.write('\t{ "%s", %s },\n' % (n, trampolin(s)))
        o.write('\t{ 0, 0 }\n};\nconst u32 n_%s = %d;\n' % (d.lower(), len(set(tablas[d]))))
