#!/usr/bin/env python3
# emu86.py - emulador mínimo de 8086 con un poco de DOS, para probar deprisa
# los .COM que saca ccastel sin arrancar QEMU (por ejemplo, las tardes en que
# el servidor no se puede cargar). No sustituye a pruebas.sh, que los corre en
# Hispa-8086 de verdad: solo sirve para no programar a ciegas. Probado con
# los programas de azar32.py; con otros (p. ej. ejemplos/bucles.cas) se queda
# colgado: le faltan instrucciones o funciones de DOS por hacer.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#   python3 castel/emu86.py PROGRAMA.COM [argumentos]   (los archivos, en la carpeta actual)
import sys, os


class Fin(Exception):
    pass


class CPU:
    def __init__(s, com, cola=b'', carpeta='.', entrada=b''):
        s.m = bytearray(0x110000)
        s.r = [0] * 8                # ax cx dx bx sp bp si di
        s.sg = [0x1000] * 4          # es cs ss ds
        s.ip = 0x100
        s.cf = s.zf = s.sf = s.of = s.df = 0
        s.m[0x10100:0x10100 + len(com)] = com
        s.m[0x10080] = len(cola)
        s.m[0x10081:0x10081 + len(cola)] = cola
        s.m[0x10081 + len(cola)] = 13
        s.r[4] = 0xFFFE
        s.sal = bytearray()
        s.carpeta = carpeta
        s.fich = {}
        s.entrada = entrada
        s.pos_ent = 0
        s.seg = None
        s.n = 0
        s.codigo = 0

    # memoria
    def rb(s, a):
        return s.m[a & 0xFFFFF]

    def rw(s, a):
        return s.m[a & 0xFFFFF] | (s.m[(a + 1) & 0xFFFFF] << 8)

    def wb(s, a, v):
        s.m[a & 0xFFFFF] = v & 0xFF

    def ww(s, a, v):
        s.m[a & 0xFFFFF] = v & 0xFF
        s.m[(a + 1) & 0xFFFFF] = (v >> 8) & 0xFF

    def lin(s, seg, off):
        return (s.sg[seg] << 4) + (off & 0xFFFF)

    def f8(s):
        v = s.rb(s.lin(1, s.ip))
        s.ip = (s.ip + 1) & 0xFFFF
        return v

    def f16(s):
        v = s.rw(s.lin(1, s.ip))
        s.ip = (s.ip + 2) & 0xFFFF
        return v

    def s8(s):
        v = s.f8()
        return v - 256 if v >= 128 else v

    # registros
    def g8(s, i):
        return (s.r[i & 3] >> 8) & 0xFF if i & 4 else s.r[i & 3] & 0xFF

    def p8(s, i, v):
        v &= 0xFF
        if i & 4:
            s.r[i & 3] = (s.r[i & 3] & 0xFF) | (v << 8)
        else:
            s.r[i & 3] = (s.r[i & 3] & 0xFF00) | v

    def push(s, v):
        s.r[4] = (s.r[4] - 2) & 0xFFFF
        s.ww(s.lin(2, s.r[4]), v)

    def pop(s):
        v = s.rw(s.lin(2, s.r[4]))
        s.r[4] = (s.r[4] + 2) & 0xFFFF
        return v

    def modrm(s):
        b = s.f8()
        mod, reg, rm = b >> 6, (b >> 3) & 7, b & 7
        if mod == 3:
            return reg, ('r', rm)
        r = s.r
        seg = 3
        if rm == 0: off = r[3] + r[6]
        elif rm == 1: off = r[3] + r[7]
        elif rm == 2: off = r[5] + r[6]; seg = 2
        elif rm == 3: off = r[5] + r[7]; seg = 2
        elif rm == 4: off = r[6]
        elif rm == 5: off = r[7]
        elif rm == 6:
            if mod == 0:
                off = s.f16()
            else:
                off = r[5]; seg = 2
        else: off = r[3]
        if mod == 1: off += s.s8()
        elif mod == 2: off += s.f16()
        if s.seg is not None:
            seg = s.seg
        return reg, ('m', s.lin(seg, off), off & 0xFFFF)

    def get(s, ea, w):
        if ea[0] == 'r':
            return s.r[ea[1]] if w else s.g8(ea[1])
        return s.rw(ea[1]) if w else s.rb(ea[1])

    def put(s, ea, w, v):
        if ea[0] == 'r':
            if w: s.r[ea[1]] = v & 0xFFFF
            else: s.p8(ea[1], v)
        elif w: s.ww(ea[1], v)
        else: s.wb(ea[1], v)

    def szf(s, v, w):
        bits = 16 if w else 8
        v &= (1 << bits) - 1
        s.zf = int(v == 0)
        s.sf = v >> (bits - 1)
        return v

    def alu(s, op, a, b, w):
        bits = 16 if w else 8
        mask = (1 << bits) - 1
        if op in (0, 2):                 # add adc
            c = s.cf if op == 2 else 0
            r = a + b + c
            s.cf = int(r > mask)
            s.of = (((a ^ r) & (b ^ r)) >> (bits - 1)) & 1
        elif op in (3, 5, 7):            # sbb sub cmp
            c = s.cf if op == 3 else 0
            r = a - b - c
            s.cf = int(r < 0)
            s.of = (((a ^ b) & (a ^ r)) >> (bits - 1)) & 1
        else:
            r = a | b if op == 1 else (a & b if op == 4 else a ^ b)
            s.cf = s.of = 0
        return s.szf(r, w)

    def desplaza(s, op, v, n, w):
        bits = 16 if w else 8
        mask = (1 << bits) - 1
        top = 1 << (bits - 1)
        for _ in range(n):
            if op == 0:   # rol
                c = (v >> (bits - 1)) & 1; v = ((v << 1) | c) & mask; s.cf = c
                s.of = ((v >> (bits - 1)) & 1) ^ s.cf
            elif op == 1:  # ror
                c = v & 1; v = (v >> 1) | (c << (bits - 1)); s.cf = c
                s.of = ((v >> (bits - 1)) ^ (v >> (bits - 2))) & 1
            elif op == 2:  # rcl
                c = (v >> (bits - 1)) & 1; v = ((v << 1) | s.cf) & mask; s.cf = c
                s.of = ((v >> (bits - 1)) & 1) ^ s.cf
            elif op == 3:  # rcr
                c = v & 1; v = (v >> 1) | (s.cf << (bits - 1)); s.cf = c
                s.of = ((v >> (bits - 1)) ^ (v >> (bits - 2))) & 1
            elif op in (4, 6):  # shl
                s.cf = (v >> (bits - 1)) & 1; v = (v << 1) & mask
                s.of = ((v >> (bits - 1)) & 1) ^ s.cf
                s.szf(v, w)
            elif op == 5:  # shr
                s.cf = v & 1; s.of = (v >> (bits - 1)) & 1; v >>= 1
                s.szf(v, w)
            else:          # sar
                s.cf = v & 1; v = (v >> 1) | (v & top); s.of = 0
                s.szf(v, w)
        return v

    def cond(s, c):
        r = [s.of, s.cf, s.zf, s.cf | s.zf, s.sf, 0, s.sf ^ s.of, (s.sf ^ s.of) | s.zf][c >> 1]
        if c >> 1 == 5:
            raise Exception("jp/jnp no emulado")
        return bool(r) != bool(c & 1)

    def banderas(s):
        return 0x0202 | s.cf | (s.zf << 6) | (s.sf << 7) | (s.df << 10) | (s.of << 11)

    def pon_banderas(s, v):
        s.cf = v & 1; s.zf = (v >> 6) & 1; s.sf = (v >> 7) & 1; s.df = (v >> 10) & 1; s.of = (v >> 11) & 1

    def cadena(s, op, rep):
        w = op & 1
        d = (-1 if s.df else 1) * (2 if w else 1)
        r = s.r
        sseg = 3 if s.seg is None else s.seg
        while True:
            if rep is not None and r[1] == 0:
                break
            k = op & 0xFE
            if k == 0xA4:
                v = s.rw(s.lin(sseg, r[6])) if w else s.rb(s.lin(sseg, r[6]))
                (s.ww if w else s.wb)(s.lin(0, r[7]), v)
                r[6] = (r[6] + d) & 0xFFFF; r[7] = (r[7] + d) & 0xFFFF
            elif k == 0xAA:
                (s.ww if w else s.wb)(s.lin(0, r[7]), r[0])
                r[7] = (r[7] + d) & 0xFFFF
            elif k == 0xAC:
                if w: r[0] = s.rw(s.lin(sseg, r[6]))
                else: s.p8(0, s.rb(s.lin(sseg, r[6])))
                r[6] = (r[6] + d) & 0xFFFF
            elif k == 0xA6:
                a = s.rw(s.lin(sseg, r[6])) if w else s.rb(s.lin(sseg, r[6]))
                b = s.rw(s.lin(0, r[7])) if w else s.rb(s.lin(0, r[7]))
                s.alu(7, a, b, w)
                r[6] = (r[6] + d) & 0xFFFF; r[7] = (r[7] + d) & 0xFFFF
            elif k == 0xAE:
                b = s.rw(s.lin(0, r[7])) if w else s.rb(s.lin(0, r[7]))
                s.alu(7, r[0] if w else r[0] & 0xFF, b, w)
                r[7] = (r[7] + d) & 0xFFFF
            if rep is None:
                break
            r[1] = (r[1] - 1) & 0xFFFF
            if k in (0xA6, 0xAE) and s.zf != (1 if rep == 0xF3 else 0):
                break

    # ---- DOS
    def texto0(s, off):
        a = s.lin(3, off)
        b = bytearray()
        while s.m[a]:
            b.append(s.m[a]); a += 1
        return b.decode('cp437').upper()

    def int21(s):
        r = s.r
        ah = r[0] >> 8
        s.cf = 0
        if ah == 2:
            s.sal.append(r[2] & 0xFF)
        elif ah == 9:
            a = s.lin(3, r[2])
            while s.m[a] != 36:
                s.sal.append(s.m[a]); a += 1
        elif ah == 0x4C:
            s.codigo = r[0] & 0xFF
            raise Fin()
        elif ah == 0x30:
            r[0] = 0x0003
        elif ah in (0x3C, 0x3D):
            nombre = os.path.join(s.carpeta, s.texto0(r[2]))
            try:
                if ah == 0x3C:
                    f = open(nombre, 'w+b')
                else:
                    f = open(nombre, ('rb', 'r+b', 'r+b')[r[0] & 3])
            except OSError:
                s.cf = 1; r[0] = 2
                return
            h = 5
            while h in s.fich:
                h += 1
            s.fich[h] = f
            r[0] = h
        elif ah == 0x3E:
            if r[3] in s.fich:
                s.fich.pop(r[3]).close()
            elif r[3] > 4:
                s.cf = 1; r[0] = 6
        elif ah == 0x3F:
            if r[3] == 0:
                d = s.entrada[s.pos_ent:s.pos_ent + r[1]]
                s.pos_ent += len(d)
            elif r[3] in s.fich:
                d = s.fich[r[3]].read(r[1])
            else:
                s.cf = 1; r[0] = 6
                return
            a = s.lin(3, r[2])
            s.m[a:a + len(d)] = d
            r[0] = len(d)
        elif ah == 0x40:
            a = s.lin(3, r[2])
            d = bytes(s.m[a:a + r[1]])
            if r[3] in (1, 2):
                s.sal += d
            elif r[3] in s.fich:
                if r[1] == 0:
                    s.fich[r[3]].truncate()
                else:
                    s.fich[r[3]].write(d)
            else:
                s.cf = 1; r[0] = 6
                return
            r[0] = len(d)
        elif ah == 0x41:
            try:
                os.remove(os.path.join(s.carpeta, s.texto0(r[2])))
            except OSError:
                s.cf = 1; r[0] = 2
        elif ah == 0x42:
            if r[3] not in s.fich:
                s.cf = 1; r[0] = 6
                return
            pos = (r[1] << 16) | r[2]
            if r[0] & 0xFF:
                pos = pos - (1 << 32) if pos >> 31 else pos
            f = s.fich[r[3]]
            f.seek(pos, r[0] & 0xFF)
            p = f.tell()
            r[0] = p & 0xFFFF; r[2] = (p >> 16) & 0xFFFF
        else:
            raise Exception(f"int 21h, AH={ah:02X}: no emulada")

    def interrupcion(s, n):
        if n == 0x21:
            s.int21()
        elif n == 0x10:
            if s.r[0] >> 8 == 0x0E:
                s.sal.append(s.r[0] & 0xFF)
        elif n == 0x16:
            s.r[0] = 0x1C0D
        elif n == 0x20:
            raise Fin()
        elif n == 0x12:
            s.r[0] = 640
        else:
            raise Exception(f"int {n:02X}h no emulada")

    def paso(s):
        r = s.r
        s.seg = None
        rep = None
        while True:
            op = s.f8()
            if op in (0x26, 0x2E, 0x36, 0x3E):
                s.seg = (op >> 3) & 3
            elif op in (0xF2, 0xF3):
                rep = op
            else:
                break
        if op < 0x40 and (op & 7) < 6:
            k = op >> 3
            w = op & 1
            if op & 4:
                b = s.f16() if w else s.f8()
                a = r[0] if w else r[0] & 0xFF
                v = s.alu(k, a, b, w)
                if k != 7:
                    if w: r[0] = v
                    else: s.p8(0, v)
            else:
                reg, ea = s.modrm()
                if op & 2:
                    a = r[reg] if w else s.g8(reg)
                    v = s.alu(k, a, s.get(ea, w), w)
                    if k != 7:
                        if w: r[reg] = v
                        else: s.p8(reg, v)
                else:
                    b = r[reg] if w else s.g8(reg)
                    v = s.alu(k, s.get(ea, w), b, w)
                    if k != 7:
                        s.put(ea, w, v)
        elif op < 0x40 and (op & 7) == 6:
            s.push(s.sg[op >> 3])
        elif op < 0x20 and (op & 7) == 7:
            s.sg[op >> 3] = s.pop()
        elif 0x40 <= op < 0x50:
            i = op & 7
            c = s.cf
            r[i] = s.alu(0 if op < 0x48 else 5, r[i], 1, 1)
            s.cf = c
        elif 0x50 <= op < 0x58:
            s.push(r[op & 7])
        elif 0x58 <= op < 0x60:
            r[op & 7] = s.pop()
        elif 0x70 <= op < 0x80:
            d = s.s8()
            if s.cond(op & 15):
                s.ip = (s.ip + d) & 0xFFFF
        elif op in (0x80, 0x81, 0x82, 0x83):
            w = op & 1
            reg, ea = s.modrm()
            if op == 0x81: b = s.f16()
            elif op == 0x83: b = s.s8() & 0xFFFF
            else: b = s.f8()
            v = s.alu(reg, s.get(ea, w), b, w)
            if reg != 7:
                s.put(ea, w, v)
        elif op in (0x84, 0x85):
            w = op & 1
            reg, ea = s.modrm()
            s.alu(4, s.get(ea, w), r[reg] if w else s.g8(reg), w)
        elif op in (0x86, 0x87):
            w = op & 1
            reg, ea = s.modrm()
            a = s.get(ea, w)
            if w:
                s.put(ea, 1, r[reg]); r[reg] = a
            else:
                s.put(ea, 0, s.g8(reg)); s.p8(reg, a)
        elif op in (0x88, 0x89):
            w = op & 1
            reg, ea = s.modrm()
            s.put(ea, w, r[reg] if w else s.g8(reg))
        elif op in (0x8A, 0x8B):
            w = op & 1
            reg, ea = s.modrm()
            if w: r[reg] = s.get(ea, 1)
            else: s.p8(reg, s.get(ea, 0))
        elif op == 0x8C:
            reg, ea = s.modrm()
            s.put(ea, 1, s.sg[reg & 3])
        elif op == 0x8E:
            reg, ea = s.modrm()
            s.sg[reg & 3] = s.get(ea, 1)
        elif op == 0x8D:
            reg, ea = s.modrm()
            r[reg] = ea[2]
        elif op == 0x8F:
            reg, ea = s.modrm()
            s.put(ea, 1, s.pop())
        elif 0x90 <= op < 0x98:
            i = op & 7
            r[0], r[i] = r[i], r[0]
        elif op == 0x98:
            r[0] = (r[0] & 0xFF) | (0xFF00 if r[0] & 0x80 else 0)
        elif op == 0x99:
            r[2] = 0xFFFF if r[0] & 0x8000 else 0
        elif op == 0x9C:
            s.push(s.banderas())
        elif op == 0x9D:
            s.pon_banderas(s.pop())
        elif op in (0xA0, 0xA1):
            a = s.lin(3 if s.seg is None else s.seg, s.f16())
            if op & 1: r[0] = s.rw(a)
            else: s.p8(0, s.rb(a))
        elif op in (0xA2, 0xA3):
            a = s.lin(3 if s.seg is None else s.seg, s.f16())
            if op & 1: s.ww(a, r[0])
            else: s.wb(a, r[0])
        elif op in (0xA4, 0xA5, 0xA6, 0xA7, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF):
            s.cadena(op, rep)
        elif op in (0xA8, 0xA9):
            w = op & 1
            s.alu(4, r[0] if w else r[0] & 0xFF, s.f16() if w else s.f8(), w)
        elif 0xB0 <= op < 0xB8:
            s.p8(op & 7, s.f8())
        elif 0xB8 <= op < 0xC0:
            r[op & 7] = s.f16()
        elif op == 0xC2:
            n = s.f16()
            s.ip = s.pop()
            r[4] = (r[4] + n) & 0xFFFF
        elif op == 0xC3:
            s.ip = s.pop()
            if s.ip == 0 and r[4] == 0:
                raise Fin()
        elif op in (0xC6, 0xC7):
            w = op & 1
            reg, ea = s.modrm()
            s.put(ea, w, s.f16() if w else s.f8())
        elif op == 0xCD:
            s.interrupcion(s.f8())
        elif op in (0xD0, 0xD1, 0xD2, 0xD3):
            w = op & 1
            reg, ea = s.modrm()
            n = (r[1] & 0xFF) if op & 2 else 1
            s.put(ea, w, s.desplaza(reg, s.get(ea, w), n, w))
        elif op in (0xE0, 0xE1, 0xE2):
            d = s.s8()
            r[1] = (r[1] - 1) & 0xFFFF
            if r[1] and (op == 0xE2 or s.zf == (op & 1)):
                s.ip = (s.ip + d) & 0xFFFF
        elif op == 0xE3:
            d = s.s8()
            if r[1] == 0:
                s.ip = (s.ip + d) & 0xFFFF
        elif op in (0xE4, 0xE5, 0xEC, 0xED):
            if op < 0xE8: s.f8()
            if op & 1: r[0] = 0xFFFF
            else: s.p8(0, 0xFF)
        elif op in (0xE6, 0xE7):
            s.f8()
        elif op in (0xEE, 0xEF):
            pass
        elif op == 0xE8:
            d = s.f16()
            s.push(s.ip)
            s.ip = (s.ip + d) & 0xFFFF
        elif op == 0xE9:
            d = s.f16()
            s.ip = (s.ip + d) & 0xFFFF
        elif op == 0xEB:
            d = s.s8()
            s.ip = (s.ip + d) & 0xFFFF
        elif op in (0xF6, 0xF7):
            w = op & 1
            bits = 16 if w else 8
            mask = (1 << bits) - 1
            reg, ea = s.modrm()
            v = s.get(ea, w)
            if reg in (0, 1):
                s.alu(4, v, s.f16() if w else s.f8(), w)
            elif reg == 2:
                s.put(ea, w, ~v & mask)
            elif reg == 3:
                x = s.alu(5, 0, v, w)
                s.put(ea, w, x)
            elif reg in (4, 5):
                a = r[0] if w else r[0] & 0xFF
                if reg == 5:
                    a = a - (mask + 1) if a >> (bits - 1) else a
                    v = v - (mask + 1) if v >> (bits - 1) else v
                p = a * v
                if w:
                    r[0] = p & 0xFFFF; r[2] = (p >> 16) & 0xFFFF
                    alto = r[2]
                else:
                    r[0] = p & 0xFFFF
                    alto = r[0] >> 8
                if reg == 4:
                    s.cf = s.of = int(alto != 0)
                else:
                    s.cf = s.of = int(not (-(1 << (bits - 1)) <= p < (1 << (bits - 1))))
            else:
                if v == 0:
                    raise Exception(f"división por cero en {s.ip:04X}")
                n = ((r[2] << 16) | r[0]) if w else r[0]
                if reg == 7:
                    n = n - (1 << (2 * bits)) if n >> (2 * bits - 1) else n
                    v = v - (mask + 1) if v >> (bits - 1) else v
                    q = abs(n) // abs(v)
                    if (n < 0) != (v < 0): q = -q
                    rr = n - q * v
                    if not -(1 << (bits - 1)) <= q < (1 << (bits - 1)):
                        raise Exception(f"desbordamiento de idiv en {s.ip:04X}")
                else:
                    q, rr = divmod(n, v)
                    if q > mask:
                        raise Exception(f"desbordamiento de div en {s.ip:04X}")
                if w:
                    r[0] = q & 0xFFFF; r[2] = rr & 0xFFFF
                else:
                    r[0] = (q & 0xFF) | ((rr & 0xFF) << 8)
        elif op == 0xF8: s.cf = 0
        elif op == 0xF9: s.cf = 1
        elif op in (0xFA, 0xFB): pass
        elif op == 0xFC: s.df = 0
        elif op == 0xFD: s.df = 1
        elif op in (0xFE, 0xFF):
            w = op & 1
            reg, ea = s.modrm()
            v = s.get(ea, w)
            if reg in (0, 1):
                c = s.cf
                s.put(ea, w, s.alu(0 if reg == 0 else 5, v, 1, w))
                s.cf = c
            elif reg == 2:
                s.push(s.ip); s.ip = v
            elif reg == 4:
                s.ip = v
            elif reg == 6:
                s.push(v)
            else:
                raise Exception(f"FF /{reg} no emulado")
        elif op == 0x90:
            pass
        else:
            raise Exception(f"instrucción {op:02X} no emulada en {s.ip - 1:04X}")

    def corre(s, maximo=30000000):
        s.push(0)
        try:
            while s.n < maximo:
                s.paso()
                s.n += 1
            s.sal += b'\n[emu86: demasiadas instrucciones]\n'
        except Fin:
            pass
        for f in s.fich.values():
            f.close()
        return bytes(s.sal)


if __name__ == '__main__':
    com = open(sys.argv[1], 'rb').read()
    cola = (' ' + ' '.join(sys.argv[2:])).encode('cp437') if len(sys.argv) > 2 else b''
    c = CPU(com, cola, entrada=b'' if sys.stdin.isatty() else sys.stdin.buffer.read())
    try:
        sal = c.corre()
    except Exception as e:
        sal = bytes(c.sal) + f"\n[emu86: {e}]\n".encode()
    sys.stdout.write(sal.replace(b'\r', b'').decode('cp437'))
    sys.exit(c.codigo)
