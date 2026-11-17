#!/usr/bin/env python3
# elf2pe.py - enlazador pequeño: objetos ELF32 (gcc -m32 -c) -> un .EXE PE de
# consola de Win32. Para las pruebas de Hispa-486 (docs/486/HISPA-486.md): así
# se pueden hacer programas Win32 en C sin un compilador cruzado de Windows.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#   tools/elf2pe.py -o PROG.EXE [-e entrada] [--reloc] [--base 0x400000]
#                   [-i DLL:func,func...] obj.o ...
#
# Los símbolos que quedan sin definir se importan: de la DLL que diga -i, o de
# KERNEL32.dll si no lo dice. Con --reloc se añade la sección .reloc (para
# probar que el cargador reubica). Hecho a partir de la especificación pública
# de los formatos ELF y PE/COFF.
import struct, sys, argparse

SEC_AL, ARCH_AL = 0x1000, 0x200

def al(x, a):
    return (x + a - 1) // a * a

class Obj:
    def __init__(self, nombre):
        d = open(nombre, 'rb').read()
        if d[:4] != b'\x7fELF' or d[4] != 1:
            sys.exit(f'{nombre}: no es ELF de 32 bits')
        (e_shoff,) = struct.unpack_from('<I', d, 0x20)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', d, 0x2E)
        self.nombre, self.d = nombre, d
        self.secs = []
        for i in range(e_shnum):
            self.secs.append(struct.unpack_from('<IIIIIIIIII', d, e_shoff + i * e_shentsize))
        shstr = self.secs[e_shstrndx]
        self.snom = lambda off: d[shstr[4] + off: d.index(b'\0', shstr[4] + off)].decode()
        self.simbolos = []
        for s in self.secs:
            if s[1] == 2:  # SYMTAB
                strt = self.secs[s[6]]
                for k in range(s[5] // 16):
                    nm, val, tam, info, oth, shn = struct.unpack_from('<IIIBBH', d, s[4] + k * 16)
                    n = d[strt[4] + nm: d.index(b'\0', strt[4] + nm)].decode()
                    self.simbolos.append((n, val, info >> 4, info & 15, shn))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('-o', required=True)
    ap.add_argument('-e', default='inicio')
    ap.add_argument('--base', default='0x400000')
    ap.add_argument('--reloc', action='store_true')
    ap.add_argument('-i', action='append', default=[])
    ap.add_argument('objs', nargs='+')
    a = ap.parse_args()
    base = int(a.base, 0)
    dll_de = {}
    orden_dll = []
    for x in a.i:
        dll, fs = x.split(':')
        orden_dll.append(dll)
        for f in fs.split(','):
            dll_de[f] = dll
    objs = [Obj(n) for n in a.objs]
    # las secciones de cada clase, una tras otra: texto (código y solo
    # lectura), datos y bss
    clases = {'t': bytearray(), 'd': bytearray()}
    bss_tam = 0
    donde = {}  # (obj, índice de sección) -> (clase, desplazamiento)
    for oi, o in enumerate(objs):
        for si, s in enumerate(o.secs):
            n = o.snom(s[0])
            tipo, flags = s[1], s[2]
            if not (flags & 2):  # SHF_ALLOC
                continue
            if tipo == 8:  # NOBITS
                bss_tam = al(bss_tam, max(s[8], 1))
                donde[(oi, si)] = ('b', bss_tam)
                bss_tam += s[5]
                continue
            if tipo != 1:
                continue
            c = 'd' if (flags & 1) else 't'  # SHF_WRITE
            b = clases[c]
            b.extend(b'\0' * (al(len(b), max(s[8], 1)) - len(b)))
            donde[(oi, si)] = (c, len(b))
            b.extend(o.d[s[4]: s[4] + s[5]])
    # los símbolos globales
    glob = {}
    for oi, o in enumerate(objs):
        for (n, val, bind, typ, shn) in o.simbolos:
            if bind in (1, 2) and shn != 0 and shn < 0xFF00 and (oi, shn) in donde:
                glob[n] = (oi, shn, val)
    # lo que falta: importaciones
    faltan = []
    for oi, o in enumerate(objs):
        for (n, val, bind, typ, shn) in o.simbolos:
            if shn == 0 and n and n not in glob and n not in faltan:
                faltan.append(n)
    por_dll = {}
    for f in faltan:
        dll = dll_de.get(f, 'KERNEL32.dll')
        por_dll.setdefault(dll, []).append(f)
        if dll not in orden_dll:
            orden_dll.append(dll)
    # las direcciones: .text, .data (con las importaciones), .bss
    rva_t = SEC_AL
    rva_d = al(rva_t + max(len(clases['t']), 1), SEC_AL)
    d = clases['d']
    d.extend(b'\0' * (al(len(d), 4) - len(d)))
    imp_off = len(d)
    dlls = [x for x in orden_dll if x in por_dll]
    # el bloque de importaciones: descriptores, ILT, IAT, nombres
    n_desc = len(dlls) + 1
    off = imp_off + n_desc * 20
    ilt_off, iat_off = {}, {}
    for x in dlls:
        ilt_off[x] = off; off += 4 * (len(por_dll[x]) + 1)
    for x in dlls:
        iat_off[x] = off; off += 4 * (len(por_dll[x]) + 1)
    nombres = bytearray()
    nom_off = {}
    pista_off = {}
    for x in dlls:
        nom_off[x] = off + len(nombres)
        nombres += x.encode() + b'\0'
        if len(nombres) % 2:
            nombres += b'\0'
        for f in por_dll[x]:
            pista_off[f] = off + len(nombres)
            nombres += b'\0\0' + f.encode() + b'\0'
            if len(nombres) % 2:
                nombres += b'\0'
    d.extend(b'\0' * (off - len(d)))
    d.extend(nombres)
    def rd(o):
        return rva_d + o
    for k, x in enumerate(dlls):
        struct.pack_into('<IIIII', d, imp_off + k * 20, rd(ilt_off[x]), 0, 0, rd(nom_off[x]), rd(iat_off[x]))
        for j, f in enumerate(por_dll[x]):
            struct.pack_into('<I', d, ilt_off[x] + 4 * j, rd(pista_off[f]))
            struct.pack_into('<I', d, iat_off[x] + 4 * j, rd(pista_off[f]))
    imp_dir = (rd(imp_off), n_desc * 20)
    iat_dir = (rd(min(iat_off.values())), off - min(iat_off.values())) if dlls else (0, 0)
    # los saltos a las importaciones: «jmp [iat]» en el texto (para las
    # llamadas directas de gcc: call func)
    t = clases['t']
    t.extend(b'\0' * (al(len(t), 8) - len(t)))
    salto = {}
    for x in dlls:
        for j, f in enumerate(por_dll[x]):
            salto[f] = len(t)
            t.extend(b'\xFF\x25' + struct.pack('<I', base + rd(iat_off[x] + 4 * j)) + b'\x90\x90')
    rva_d = al(rva_t + len(t), SEC_AL)  # (el texto ha crecido: recolocar)
    # rehacer las RVA de las importaciones con el rva_d nuevo
    for k, x in enumerate(dlls):
        struct.pack_into('<IIIII', d, imp_off + k * 20, rd(ilt_off[x]), 0, 0, rd(nom_off[x]), rd(iat_off[x]))
        for j, f in enumerate(por_dll[x]):
            struct.pack_into('<I', d, ilt_off[x] + 4 * j, rd(pista_off[f]))
            struct.pack_into('<I', d, iat_off[x] + 4 * j, rd(pista_off[f]))
    for x in dlls:
        for j, f in enumerate(por_dll[x]):
            struct.pack_into('<I', t, salto[f] + 2, base + rd(iat_off[x] + 4 * j))
    imp_dir = (rd(imp_off), n_desc * 20)
    iat_dir = (rd(min(iat_off.values())), off - min(iat_off.values())) if dlls else (0, 0)
    rva_b = al(rva_d + max(len(d), 1), SEC_AL)
    rva_de = {'t': rva_t, 'd': rva_d, 'b': rva_b}
    def dir_sec(oi, shn):
        c, o = donde[(oi, shn)]
        return base + rva_de[c] + o
    def dir_sim(oi, idx):
        n, val, bind, typ, shn = objs[oi].simbolos[idx]
        if typ == 3:  # SECTION
            return dir_sec(oi, shn)
        if shn != 0 and shn < 0xFF00:
            if (oi, shn) not in donde:
                sys.exit(f'{n}: en una sección que no se carga')
            return dir_sec(oi, shn) + val
        if shn == 0xFFF1:
            return val
        if n in glob:
            g = glob[n]
            return dir_sec(g[0], g[1]) + g[2]
        if n in salto:
            return base + rva_t + salto[n]
        sys.exit(f'símbolo sin resolver: {n}')
    # los reubicados
    fijos = []  # RVA de los DIR32 (para .reloc)
    for oi, o in enumerate(objs):
        for s in o.secs:
            if s[1] != 9:  # REL
                continue
            dest = s[7]
            if (oi, dest) not in donde:
                continue
            c, o_dest = donde[(oi, dest)]
            buf = clases[c]
            for k in range(s[5] // 8):
                r_off, r_info = struct.unpack_from('<II', o.d, s[4] + k * 8)
                tipo, idx = r_info & 0xFF, r_info >> 8
                p = o_dest + r_off
                (sumando,) = struct.unpack_from('<i', buf, p)
                S = dir_sim(oi, idx)
                P = base + rva_de[c] + p
                if tipo == 1:  # R_386_32
                    struct.pack_into('<I', buf, p, (S + sumando) & 0xFFFFFFFF)
                    fijos.append(rva_de[c] + p)
                elif tipo in (2, 4):  # R_386_PC32 / PLT32
                    struct.pack_into('<I', buf, p, (S + sumando - P) & 0xFFFFFFFF)
                else:
                    sys.exit(f'{o.nombre}: reubicado de tipo {tipo} no soportado (compila con -fno-pic)')
    # los DIR32 de los saltos a importaciones también
    for f, o_s in salto.items():
        fijos.append(rva_t + o_s + 2)
    # .reloc
    reloc = bytearray()
    if a.reloc:
        pags = {}
        for r in sorted(fijos):
            pags.setdefault(r & ~0xFFF, []).append(r & 0xFFF)
        for pg, offs in sorted(pags.items()):
            if len(offs) % 2:
                offs = offs + [None]
            reloc += struct.pack('<II', pg, 8 + 2 * len(offs))
            for x in offs:
                reloc += struct.pack('<H', 0 if x is None else (3 << 12) | x)
    rva_r = al(rva_b + max(bss_tam, 1), SEC_AL)
    secciones = [('.text', t, rva_t, len(t), 0x60000020),
                 ('.data', d, rva_d, len(d), 0xC0000040),
                 ('.bss', None, rva_b, bss_tam, 0xC0000080)]
    if reloc:
        secciones.append(('.reloc', reloc, rva_r, len(reloc), 0x42000040))
    tam_img = al(secciones[-1][2] + max(secciones[-1][3], 1), SEC_AL)
    if a.e not in glob:
        sys.exit(f'no está la entrada {a.e}')
    entrada = dir_sec(glob[a.e][0], glob[a.e][1]) + glob[a.e][2] - base
    # la cabecera
    cab_tam = ARCH_AL
    sal = bytearray(cab_tam)
    sal[0:2] = b'MZ'
    struct.pack_into('<I', sal, 0x3C, 0x40)
    pe = 0x40
    sal[pe:pe + 4] = b'PE\0\0'
    caract = 0x0103 if not reloc else 0x0102
    struct.pack_into('<HHIIIHH', sal, pe + 4, 0x14C, len(secciones), 0, 0, 0, 224, caract)
    o = pe + 24
    struct.pack_into('<HBBIIIIIIIII', sal, o, 0x10B, 2, 0, len(t), len(d), bss_tam,
                     entrada, rva_t, rva_d, base, SEC_AL, ARCH_AL)
    struct.pack_into('<HHHHHHIIIIHHIIIIII', sal, o + 40, 4, 0, 0, 0, 4, 0, 0, tam_img,
                     cab_tam, 0, 3, 0, 0x100000, 0x1000, 0x100000, 0x1000, 0, 16)
    dirs = [(0, 0)] * 16
    dirs[1] = imp_dir
    dirs[12] = iat_dir
    if reloc:
        dirs[5] = (rva_r, len(reloc))
    for k, (r, n) in enumerate(dirs):
        struct.pack_into('<II', sal, o + 96 + 8 * k, r, n)
    st = o + 224
    pos = cab_tam
    cuerpo = bytearray()
    for k, (n, b, r, vt, fl) in enumerate(secciones):
        tam_arch = al(len(b), ARCH_AL) if b else 0
        struct.pack_into('<8sIIIIIIHHI', sal, st + 40 * k, n.encode(), vt, r, tam_arch,
                         pos if b else 0, 0, 0, 0, 0, fl)
        if b:
            cuerpo += b + b'\0' * (tam_arch - len(b))
            pos += tam_arch
    open(a.o, 'wb').write(sal + cuerpo)

main()
