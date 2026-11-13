#!/usr/bin/env python3
# Hispa-DOS / Hispa 32: qué funciones del sistema usan los programas Win32.
#
#   tools/importaciones.py CARPETA [CARPETA...] [-o lista.md] [--csv lista.csv]
#                          [--top N] [--dll KERNEL32.DLL]
#
# Recorre las carpetas, abre cada .EXE/.DLL/.SCR/.CPL/.OCX de formato PE
# (Win32) y lee su tabla de importaciones: la lista de funciones de otras
# DLL que el programa necesita. No lee ni guarda código de nadie, solo
# nombres. Al final ordena las funciones por cuántos programas las usan,
# para decidir qué hace falta primero en Hispa 32.
#
# Los programas que se analizan los aporta cada usuario y NO van al
# proyecto; lo que se puede publicar es la lista de nombres resultante.
#
# Lists the Win32 API functions imported by PE files, ranked by how many
# programs use them.
#
# Licencia GPL-2.0 o posterior.

import argparse
import collections
import csv
import os
import struct
import sys

EXTS = {'.exe', '.dll', '.scr', '.cpl', '.ocx', '.drv'}


class PE:
    def __init__(self, data):
        self.d = data
        if len(data) < 0x40 or data[:2] != b'MZ':
            raise ValueError('no es MZ')
        off = struct.unpack_from('<I', data, 0x3C)[0]
        if off + 24 > len(data) or data[off:off + 4] != b'PE\0\0':
            raise ValueError('no es PE')
        self.machine, nsec, _, _, _, optsz, self.chars = \
            struct.unpack_from('<HHIIIHH', data, off + 4)
        opt = off + 24
        magic = struct.unpack_from('<H', data, opt)[0]
        if magic == 0x10B:          # PE32
            self.bits = 32
            ndir_off = opt + 92
        elif magic == 0x20B:        # PE32+ (64 bits)
            self.bits = 64
            ndir_off = opt + 108
        else:
            raise ValueError('cabecera opcional desconocida')
        ndir = struct.unpack_from('<I', data, ndir_off)[0]
        self.dirs = []
        for i in range(min(ndir, 16)):
            self.dirs.append(struct.unpack_from('<II', data, ndir_off + 4 + 8 * i))
        self.secs = []
        s = opt + optsz
        for i in range(nsec):
            if s + 40 > len(data):
                break
            vsize, va, rsize, raw = struct.unpack_from('<IIII', data, s + 8)
            self.secs.append((va, max(vsize, rsize), raw, rsize))
            s += 40

    def off(self, rva):
        for va, size, raw, rsize in self.secs:
            if va <= rva < va + size:
                o = rva - va
                if o >= rsize:
                    return None
                return raw + o
        return None

    def cstr(self, rva, maxlen=256):
        o = self.off(rva)
        if o is None:
            return None
        e = self.d.find(b'\0', o, o + maxlen)
        if e < 0:
            return None
        return self.d[o:e].decode('latin-1')

    def u(self, fmt, rva):
        o = self.off(rva)
        if o is None or o + struct.calcsize(fmt) > len(self.d):
            return None
        return struct.unpack_from(fmt, self.d, o)

    def imports(self):
        """[(dll, función o '#ordinal', retrasada)]"""
        out = []
        if len(self.dirs) > 1 and self.dirs[1][0]:
            out += self._tabla(self.dirs[1][0], False)
        if len(self.dirs) > 13 and self.dirs[13][0]:
            out += self._tabla(self.dirs[13][0], True)
        return out

    def _tabla(self, rva, retrasada):
        out = []
        tsz = 32 if retrasada else 20
        for n in range(4096):
            if retrasada:
                v = self.u('<IIIIIIII', rva + n * tsz)
                if not v or not any(v):
                    break
                attrs, name_rva, _, _, int_rva = v[:5]
                if not (attrs & 1):     # formato antiguo con direcciones
                    break               # absolutas: raro, se descarta
                thunk = int_rva
            else:
                v = self.u('<IIIII', rva + n * tsz)
                if not v or not any(v):
                    break
                orig, _, _, name_rva, first = v
                thunk = orig or first
            dll = self.cstr(name_rva)
            if not dll:
                break
            dll = dll.upper()
            if not dll.endswith('.DLL') and '.' not in dll:
                dll += '.DLL'
            step = 8 if self.bits == 64 else 4
            flag = 1 << (63 if self.bits == 64 else 31)
            for k in range(65536):
                t = self.u('<Q' if self.bits == 64 else '<I', thunk + k * step)
                if not t or not t[0]:
                    break
                t = t[0]
                if t & flag:
                    out.append((dll, '#%d' % (t & 0xFFFF), retrasada))
                else:
                    nm = self.cstr((t & 0x7FFFFFFF) + 2)
                    if nm is None:
                        break
                    out.append((dll, nm, retrasada))
        return out


def recorrer(carpetas):
    for c in carpetas:
        if os.path.isfile(c):
            yield c
            continue
        for raiz, _, ficheros in os.walk(c):
            for f in ficheros:
                if os.path.splitext(f)[1].lower() in EXTS:
                    yield os.path.join(raiz, f)


def main():
    ap = argparse.ArgumentParser(description='Funciones Win32 que importan los programas, '
                                 'ordenadas por cuántos las usan.')
    ap.add_argument('carpetas', nargs='+')
    ap.add_argument('-o', '--md', help='guardar la lista en Markdown')
    ap.add_argument('--csv', help='guardar la lista completa en CSV')
    ap.add_argument('--top', type=int, default=50, help='funciones a mostrar (50)')
    ap.add_argument('--dll', action='append', help='solo estas DLL (se puede repetir)')
    ap.add_argument('--64', dest='con64', action='store_true',
                    help='contar también programas de 64 bits')
    a = ap.parse_args()
    solo = {x.upper() for x in a.dll} if a.dll else None

    usos = collections.Counter()        # (dll, función) -> programas
    por_dll = collections.Counter()     # dll -> programas
    vistos = set()                      # mismos bytes = un solo programa
    progs = []                          # funciones de cada programa
    n_ok = n_otros = n_64 = n_sin = 0
    for ruta in recorrer(a.carpetas):
        try:
            with open(ruta, 'rb') as f:
                d = f.read()
            pe = PE(d)
        except (OSError, ValueError, struct.error):
            n_otros += 1                # DOS, Win16 (NE), dañados...
            continue
        if pe.bits == 64 and not a.con64:
            n_64 += 1
            continue
        h = hash(d)
        if h in vistos:
            continue
        vistos.add(h)
        try:
            imps = pe.imports()
        except struct.error:
            imps = []
        funcs = {(dll, fn) for dll, fn, _ in imps if not solo or dll in solo}
        if not funcs:
            n_sin += 1                  # sin importaciones (o ninguna de --dll)
            continue
        n_ok += 1
        usos.update(funcs)
        progs.append(funcs)
        por_dll.update({dll for dll, _ in funcs})

    if not n_ok:
        print('No se ha encontrado ningún programa Win32 (PE).', file=sys.stderr)
        return 1

    orden = sorted(usos.items(), key=lambda x: (-x[1], x[0]))
    print('Programas Win32 analizados: %d (descartados: %d que no son PE de 32 bits'
          '%s%s)' % (n_ok, n_otros, ', %d de 64 bits' % n_64 if n_64 else '',
                   ', %d sin importaciones' % n_sin if n_sin else ''))
    print('Funciones distintas: %d en %d DLL\n' % (len(usos), len(por_dll)))
    print('DLL más usadas:')
    for dll, n in por_dll.most_common(15):
        print('  %-20s %4d programas (%3d %%)' % (dll, n, 100 * n // n_ok))
    print('\nFunciones más usadas:')
    for (dll, fn), n in orden[:a.top]:
        print('  %4d (%3d %%)  %s!%s' % (n, 100 * n // n_ok, dll, fn))

    # cuántas funciones hacen falta para que funcione el X % de los programas
    # (aproximado: un programa «funciona» si tiene todas sus funciones)
    rango = {k: i for i, (k, _) in enumerate(orden)}
    necesita = sorted(max((rango[k] + 1 for k in fs), default=0) for fs in progs)
    print('\nSi se implementan las N funciones más usadas, programas que tendrían')
    print('todas las suyas (orientativo: una función puede existir y fallar):')
    import bisect
    for m in (25, 50, 100, 200, 300, 500, 750, 1000, 1500, 2000):
        if m > len(orden):
            break
        c = bisect.bisect_right(necesita, m)
        print('  %5d funciones -> %4d programas (%3d %%)' % (m, c, 100 * c // n_ok))
    for pct in (50, 80, 90):
        i = max(0, -(-pct * n_ok // 100) - 1)
        print('  el %d %% de los programas necesita las %d primeras' % (pct, necesita[i]))
    if a.md:
        with open(a.md, 'w', encoding='utf-8') as f:
            f.write('# Funciones Win32 por uso\n\n')
            f.write('%d programas analizados, %d funciones distintas.\n\n' % (n_ok, len(usos)))
            f.write('| Programas | % | DLL | Función |\n|---:|---:|---|---|\n')
            for (dll, fn), n in orden:
                f.write('| %d | %d | %s | %s |\n' % (n, 100 * n // n_ok, dll, fn))
    if a.csv:
        with open(a.csv, 'w', newline='', encoding='utf-8') as f:
            w = csv.writer(f)
            w.writerow(['dll', 'funcion', 'programas', 'porcentaje'])
            for (dll, fn), n in orden:
                w.writerow([dll, fn, n, round(100 * n / n_ok, 1)])
    return 0


if __name__ == '__main__':
    sys.exit(main())
