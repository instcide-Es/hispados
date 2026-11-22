#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""compat/pruebas/falso_win32.py FUENTES_WIN32 CARPETA_WIN32 COMPRUEBA [/MAQUINA] ruta...

(COMPAT) Para las pruebas del Asistente en Linux: hace de «WIN32 COMPRUEBA»
con la misma salida que castel/nucleo32/win32/diagnostico.c (la de las
personas, o la /MAQUINA que se ha pedido a la rama win32-hl), en la página
437 y con CR LF. Lo que tiene WIN32 «por dentro» (KERNEL32, USER32...) lo
saca de sus fuentes como hazexp.py (las líneas «API(...) Nombre(»); las DLL
de fuera, de la carpeta del programa o de CARPETA_WIN32 (sus exportaciones).
En QEMU, la parte 49 usa el WIN32.BIN de verdad.
"""
import glob
import os
import re
import struct
import sys

POR = {'kernel32': 'KERNEL32', 'archivos': 'KERNEL32', 'user32': 'USER32', 'controles': 'USER32', 'menus': 'USER32',
       'dialogos': 'USER32', 'dx': 'USER32', 'recursos': 'USER32', 'gdi32': 'GDI32', 'comdlg32': 'COMDLG32',
       'otros': 'ADVAPI32', 'red': 'WSOCK32', 'registro': 'ADVAPI32'}


def propias(carpeta):
    t = {}
    for f in glob.glob(os.path.join(carpeta, '*.c')):
        dll = POR.get(os.path.splitext(os.path.basename(f))[0], 'KERNEL32')
        for l in open(f, encoding='utf-8', errors='replace'):
            m = re.match(r'/\* == (\w+) == \*/', l)
            if m:
                dll = m.group(1)
                continue
            m = re.match(r'(?:API|EXPC)\([^)]*\)\s+(\w+)\s*\(', l) or re.match(r'/\* EXPORTA (\w+)=\w+ \*/', l)
            if m:
                t.setdefault(dll + '.DLL', set()).add(m.group(1))
    return t


def secciones(d, pe):
    ns, op = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
    r = []
    for k in range(ns):
        s = pe + 24 + op + k * 40
        vs, va, rs, rp = struct.unpack_from('<IIII', d, s + 8)
        r.append((va, max(vs, rs), rp, rs))
    return r


def rva(ss, x):
    for va, t, rp, rs in ss:
        if va <= x < va + t:
            return rp + x - va if x - va < rs else None
    return None


def cad(d, o):
    return d[o:d.index(b'\0', o)].decode('latin1')


def lee_pe(ruta):
    try:
        d = open(ruta, 'rb').read()
    except OSError:
        return None
    if d[:2] != b'MZ' or len(d) < 0x200:
        return False
    pe = struct.unpack_from('<I', d, 0x3C)[0]
    if d[pe:pe + 4] != b'PE\0\0' or struct.unpack_from('<H', d, pe + 4)[0] != 0x14C:
        return False
    ss = secciones(d, pe)
    op = pe + 24
    car = struct.unpack_from('<H', d, pe + 22)[0]
    sub = struct.unpack_from('<H', d, op + 68)[0]
    imp = []
    idr = struct.unpack_from('<I', d, op + 104)[0]
    if idr:
        o = rva(ss, idr)
        while o is not None:
            ilt, _, _, nom, iat = struct.unpack_from('<IIIII', d, o)
            if not nom:
                break
            fs = []
            t = rva(ss, ilt or iat)
            while t is not None:
                v = struct.unpack_from('<I', d, t)[0]
                if not v:
                    break
                fs.append(('#%d' % (v & 0xFFFF)) if v & 0x80000000 else cad(d, rva(ss, v) + 2))
                t += 4
            imp.append((cad(d, rva(ss, nom)).upper(), fs))
            o += 20
    exp = set()
    edr = struct.unpack_from('<I', d, op + 96)[0]
    if edr:
        o = rva(ss, edr)
        ne, pn = struct.unpack_from('<I', d, o + 24)[0], struct.unpack_from('<I', d, o + 32)[0]
        pn = rva(ss, pn)
        for k in range(ne):
            exp.add(cad(d, rva(ss, struct.unpack_from('<I', d, pn + 4 * k)[0])))
    return {'dll': bool(car & 0x2000), 'sub': sub, 'imp': imp, 'exp': exp, 'tam': len(d)}


def main():
    fuentes, w32 = sys.argv[1], sys.argv[2]
    args = sys.argv[3:]
    assert args and args[0].upper() == 'COMPRUEBA'
    args = args[1:]
    maquina = bool(args) and args[0].upper() == '/MAQUINA' and os.environ.get('FALSO_MAQUINA') == '1'
    if maquina:
        args = args[1:]
    tab = propias(fuentes)
    out = []
    di = out.append
    mirados = set()
    cola = []
    total = {'faltan': 0, 'mirados': 0}

    def comprueba(ruta):
        if ruta in mirados:
            return
        mirados.add(ruta)
        base = os.path.basename(ruta)
        p = lee_pe(ruta)
        if p is None:
            di('ARCHIVO\t%s\tNOESTA\t0\t0\t0' % ruta if maquina else '%s: no se encuentra.' % ruta.replace('/', '\\'))
            return
        if p is False:
            di('ARCHIVO\t%s\tNOPE\t0\t0\t0' % ruta if maquina else '%s: no es un programa de Windows de 32 bits (PE de 386).' % base)
            return
        total['mirados'] += 1
        clase = 'DLL' if p['dll'] else 'VENTANAS' if p['sub'] == 2 else 'CONSOLA' if p['sub'] == 3 else 'PROGRAMA'
        nfun = sum(len(f) for _, f in p['imp'])
        lineas = []
        nfal = 0
        for dll, fs in p['imp']:
            if dll in tab:
                for f in fs:
                    if f not in tab[dll]:
                        nfal += 1
                        lineas.append('FALTA\t%s\t%s' % (dll, f) if maquina else '  falta %s!%s' % (dll, f))
                continue
            r = None
            for c in (os.path.dirname(ruta), w32):
                for n in os.listdir(c) if os.path.isdir(c) else []:
                    if n.upper() == dll:
                        r = os.path.join(c, n)
                        break
                if r:
                    break
            q = lee_pe(r) if r else None
            if not q:
                nfal += len(fs)
                lineas.append('FALTADLL\t%s\t%d\t%s' % (dll, len(fs), 'NO32' if q is False else 'NOESTA') if maquina else
                              '  FALTA LA DLL %s (%d %s)%s' % (dll, len(fs), 'función' if len(fs) == 1 else 'funciones',
                                                              ': está, pero no es de 32 bits' if q is False else ''))
                continue
            if not os.path.abspath(r).startswith(os.path.abspath(w32)) and r not in cola:
                cola.append(r)
            for f in fs:
                if f not in q['exp']:
                    nfal += 1
                    lineas.append('FALTA\t%s\t%s' % (dll, f) if maquina else '  falta %s!%s' % (dll, f))
        if maquina:
            di('ARCHIVO\t%s\t%s\t%d\t%d\t%d' % (ruta, clase, p['tam'], len(p['imp']), nfun))
            out.extend(lineas)
            di('FINARCHIVO\t%d' % nfal)
        else:
            di('%s: %s' % (base, {'DLL': 'DLL', 'VENTANAS': 'programa de ventanas', 'CONSOLA': 'programa de consola'}.get(clase, 'programa')))
            out.extend(lineas)
            pl = 'función' if nfun == 1 else 'funciones'
            di('  -> %d DLL, %d %s: %s' % (len(p['imp']), nfun, pl, ('falta %d.' if nfal == 1 else 'faltan %d.') % nfal if nfal else 'está todo.'))
        total['faltan'] += nfal

    def carpeta(c, nivel):
        for n in sorted(os.listdir(c)):
            r = os.path.join(c, n)
            if os.path.isdir(r):
                if nivel < 3:
                    carpeta(r, nivel + 1)
            elif n.upper().endswith(('.EXE', '.DLL')):
                comprueba(r)

    if maquina:
        di('#COMPRUEBA 1')
    else:
        di('WIN32 COMPRUEBA: las funciones de Windows que piden, y las que aún no hay')
    for a in args:
        if os.path.isdir(a):
            carpeta(a, 0)
        else:
            comprueba(a)
    for r in cola:
        comprueba(r)
    if maquina:
        di('TOTAL\t%d\t%d' % (total['mirados'], total['faltan']))
    elif total['faltan']:
        di('En total: %d %s; %s %d %s.' % (total['mirados'], 'archivo' if total['mirados'] == 1 else 'archivos',
                                           'falta' if total['faltan'] == 1 else 'faltan', total['faltan'],
                                           'función' if total['faltan'] == 1 else 'funciones'))
    elif total['mirados']:
        di('En total: %d %s; no falta nada.' % (total['mirados'], 'archivo' if total['mirados'] == 1 else 'archivos'))
    else:
        di('No hay nada que mirar. Uso: WIN32 COMPRUEBA programa.exe (o una carpeta)')
    sys.stdout.buffer.write(''.join(l + '\r\n' for l in out).encode('cp437', 'replace'))


if __name__ == '__main__':
    main()
