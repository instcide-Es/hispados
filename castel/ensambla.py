#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  ensambla.py - el ensamblador propio de Castel, en Python (la referencia).
#  Entiende el NASM que saca ccastel.py (y el que se escribe a mano en los
#  bloques «ensamblador» de los ejemplos) y da el mismo archivo que NASM,
#  octeto a octeto: las mismas formas de cada instrucción y los mismos saltos
#  cortos y largos. El de verdad, el que corre dentro de Hispa-8086, es
#  ensamblador/ensambla.cas, escrito en Castel a partir de este: los dos
#  tienen que dar siempre lo mismo (pruebas.sh lo comprueba).
#
#  uso:   python3 -I ensambla.py ENTRADA.asm [-o SALIDA]
#
#  Lo que entiende (ver docs/ENSAMBLADOR.md):
#   - las instrucciones del 8086 que hacen falta (mov, las cuentas, saltos,
#     pila, cadenas, puertos...), con registros, memoria [base+índice+n],
#     «byte»/«word», el prefijo de segmento [es:...] y números o cuentas con
#     etiquetas;
#   - etiquetas «nombre:» y locales «.nombre:» (de la última no local);
#   - db dw dd resb resw resd times, cpu, bits, org, absolute, section, align.
#  Los saltos: empiezan todos cortos y se alargan los que no llegan, hasta que
#  nada cambia (lo mismo que hace NASM): un «jmp» largo son 3 octetos; un
#  salto condicional largo, el contrario por encima de un «jmp» (5 octetos).
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# =============================================================================
import sys

R16 = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di']
R8 = ['al', 'cl', 'dl', 'bl', 'ah', 'ch', 'dh', 'bh']
RS = ['es', 'cs', 'ss', 'ds']
PREF_SEG = {'es': 0x26, 'cs': 0x2E, 'ss': 0x36, 'ds': 0x3E}
BASES = {('bx', 'si'): 0, ('bx', 'di'): 1, ('bp', 'si'): 2, ('bp', 'di'): 3,
         ('si',): 4, ('di',): 5, ('bp',): 6, ('bx',): 7}
CUENTAS = {'add': 0, 'or': 1, 'adc': 2, 'sbb': 3, 'and': 4, 'sub': 5, 'xor': 6, 'cmp': 7}
GIROS = {'rol': 0, 'ror': 1, 'rcl': 2, 'rcr': 3, 'shl': 4, 'sal': 4, 'shr': 5, 'sar': 7}
UNARIAS = {'not': 2, 'neg': 3, 'mul': 4, 'imul': 5, 'div': 6, 'idiv': 7}
CONDICIONES = {'o': 0, 'no': 1, 'b': 2, 'c': 2, 'nae': 2, 'nb': 3, 'nc': 3, 'ae': 3, 'e': 4, 'z': 4,
               'ne': 5, 'nz': 5, 'be': 6, 'na': 6, 'a': 7, 'nbe': 7, 's': 8, 'ns': 9, 'p': 10,
               'pe': 10, 'np': 11, 'po': 11, 'l': 12, 'nge': 12, 'ge': 13, 'nl': 13, 'le': 14,
               'ng': 14, 'g': 15, 'nle': 15}
SUELTAS = {'cld': 0xFC, 'std': 0xFD, 'cli': 0xFA, 'sti': 0xFB, 'clc': 0xF8, 'stc': 0xF9, 'cmc': 0xF5,
           'cwd': 0x99, 'cbw': 0x98, 'pushf': 0x9C, 'popf': 0x9D, 'lodsb': 0xAC, 'lodsw': 0xAD,
           'stosb': 0xAA, 'stosw': 0xAB, 'movsb': 0xA4, 'movsw': 0xA5, 'cmpsb': 0xA6, 'cmpsw': 0xA7,
           'scasb': 0xAE, 'scasw': 0xAF, 'nop': 0x90, 'hlt': 0xF4, 'lahf': 0x9F, 'sahf': 0x9E,
           'xlatb': 0xD7, 'iret': 0xCF, 'into': 0xCE}
PREFIJOS = {'rep': 0xF3, 'repe': 0xF3, 'repz': 0xF3, 'repne': 0xF2, 'repnz': 0xF2}
BUCLES = {'loopne': 0xE0, 'loopnz': 0xE0, 'loope': 0xE1, 'loopz': 0xE1, 'loop': 0xE2, 'jcxz': 0xE3}


class ErrorEns(Exception):
    pass


class Op:
    """Un operando: k es 'r16', 'r8', 'rs', 'mem', 'inm' o 'lejos'."""
    def __init__(self, k):
        self.k = k
        self.reg = 0
        self.tam = 0                # 1, 2 o 0 (no se sabe)
        self.seg = None             # prefijo de segmento
        self.rm = -1                # memoria: 0-7, o -1 si es una dirección sin registros
        self.v = 0                  # el número (inmediato o desplazamiento)
        self.sim = False            # ¿lleva alguna etiqueta la cuenta?
        self.v2 = 0                 # 'lejos': el segmento


class Ensamblador:
    def __init__(self, lineas):
        self.lineas = lineas
        self.sim = {}               # etiqueta -> valor (de la pasada anterior, y de esta)
        self.largos = set()         # los saltos (por su número de línea) que van largos
        self.final = False

    def error(self, msg):
        raise ErrorEns(f"línea {self.nl}: {msg}")

    # ---- las cuentas: | ^ & << >> + - * / % y paréntesis ----
    def cuenta(self, s):
        """El valor de una cuenta: (valor, ¿lleva etiquetas?)."""
        self.e = s
        self.ei = 0
        self.e_sim = False
        self.e_falta = False        # ¿alguna etiqueta que aún no se ha visto?
        v = self.c_nivel(0)
        self.c_blancos()
        if self.ei < len(self.e):
            self.error(f"no entiendo «{s.strip()}»")
        return v, self.e_sim

    NIVELES = ((b'|',), (b'^',), (b'&',), (b'<<', b'>>'), (b'+', b'-'), (b'*', b'/', b'%'))

    def c_blancos(self):
        while self.ei < len(self.e) and self.e[self.ei] in b' \t':
            self.ei += 1

    def c_nivel(self, n):
        if n == len(self.NIVELES):
            return self.c_unario()
        v = self.c_nivel(n + 1)
        while True:
            self.c_blancos()
            op = None
            for o in self.NIVELES[n]:
                if self.e.startswith(o, self.ei):
                    op = o
            if op is None:
                return v
            self.ei += len(op)
            w = self.c_nivel(n + 1)
            if op == b'|':
                v |= w
            elif op == b'^':
                v ^= w
            elif op == b'&':
                v &= w
            elif op == b'<<':
                v <<= w
            elif op == b'>>':
                v >>= w
            elif op == b'+':
                v += w
            elif op == b'-':
                v -= w
            elif op == b'*':
                v *= w
            else:
                if w == 0:
                    if self.final:
                        self.error("división por cero")
                    w = 1
                v = v // w if op == b'/' else v % w
            v &= 0xFFFFFFFF

    def c_unario(self):
        self.c_blancos()
        if self.ei >= len(self.e):
            self.error("falta un número o una etiqueta")
        c = chr(self.e[self.ei])
        if c == '-':
            self.ei += 1
            return -self.c_unario() & 0xFFFFFFFF
        if c == '+':
            self.ei += 1
            return self.c_unario()
        if c == '~':
            self.ei += 1
            return ~self.c_unario() & 0xFFFFFFFF
        if c == '(':
            self.ei += 1
            v = self.c_nivel(0)
            self.c_blancos()
            if self.ei >= len(self.e) or self.e[self.ei] != 41:
                self.error("falta un «)»")
            self.ei += 1
            return v
        if c == "'":
            if self.ei + 2 >= len(self.e) or self.e[self.ei + 2] != 39:
                self.error("carácter mal cerrado")
            self.ei += 3
            return self.e[self.ei - 2]
        j = self.ei
        while self.ei < len(self.e) and (es_de_nombre(self.e[self.ei])):
            self.ei += 1
        p = self.e[j:self.ei]
        if not p:
            self.error(f"no entiendo «{bytes(self.e[j:]).decode('utf-8', 'replace').strip()}»")
        if 48 <= p[0] <= 57:
            return self.numero(p)
        self.e_sim = True
        if p[0] == 46:                          # .local
            p = self.ambito + p
        if p not in self.sim:
            if self.final:
                self.error(f"etiqueta sin definir: «{p.decode('utf-8', 'replace')}»")
            self.e_falta = True
            return 0
        return self.sim[p]

    def numero(self, p):
        t = p.decode('latin-1').lower()
        try:
            if t.startswith('0x'):
                return int(t[2:], 16)
            if t.endswith('h'):
                return int(t[:-1], 16)
            return int(t)
        except ValueError:
            self.error(f"número mal escrito: «{t}»")

    # ---- los operandos ----
    def operando(self, s):
        s = s.strip()
        tam = 0
        bajo = s.lower()
        for pal, t in ((b'byte', 1), (b'word', 2)):
            if bajo.startswith(pal) and len(s) > 4 and s[4] in b' \t[':
                tam = t
                s = s[4:].strip()
                bajo = s.lower()
        nombre = bajo.decode('latin-1')
        if nombre in R16:
            o = Op('r16')
            o.reg = R16.index(nombre)
            o.tam = 2
            return o
        if nombre in R8:
            o = Op('r8')
            o.reg = R8.index(nombre)
            o.tam = 1
            return o
        if nombre in RS:
            o = Op('rs')
            o.reg = RS.index(nombre)
            o.tam = 2
            return o
        if s.startswith(b'['):
            if not s.endswith(b']'):
                self.error("falta un «]»")
            return self.memoria(s[1:-1], tam)
        dos = parte(s, b':')
        if len(dos) == 2:
            o = Op('lejos')
            o.v2, _ = self.cuenta(dos[0])
            o.v, _ = self.cuenta(dos[1])
            return o
        o = Op('inm')
        o.tam = tam
        o.v, o.sim = self.cuenta(s)
        return o

    def memoria(self, s, tam):
        o = Op('mem')
        o.tam = tam
        dos = parte(s, b':')
        if len(dos) == 2:
            seg = dos[0].strip().lower().decode('latin-1')
            if seg not in PREF_SEG:
                self.error("delante de «:» va un registro de segmento")
            o.seg = seg
            s = dos[1]
        # los registros (sumados) y lo demás, que es una cuenta
        regs = []
        resto = b''
        i = 0
        signo = b'+'
        while i <= len(s):
            j = i
            prof = 0
            while j < len(s) and not (s[j] in b'+-' and prof == 0 and j > i):
                if s[j] == 40:
                    prof += 1
                elif s[j] == 41:
                    prof -= 1
                j += 1
            t = s[i:j].strip()
            n = t.lower().decode('latin-1')
            if n in ('bx', 'bp', 'si', 'di') and signo == b'+':
                regs.append(n)
            elif t:
                resto += signo + t
            elif j < len(s) or i == 0:
                if not (i == 0 and j < len(s)):
                    self.error("dirección mal escrita")
            if j >= len(s):
                break
            signo = s[j:j + 1]
            i = j + 1
        if regs:
            regs.sort(key=lambda r: ('bx', 'bp', 'si', 'di').index(r))
            if tuple(regs) not in BASES:
                self.error("esos registros no valen juntos en una dirección")
            o.rm = BASES[tuple(regs)]
        if resto:
            o.v, o.sim = self.cuenta(resto)
        return o

    # ---- sacar octetos ----
    def b(self, *octetos):
        for x in octetos:
            if not self.absoluto:
                self.pos += 1
                if self.final:
                    self.sal.append(x & 0xFF)
            self.pc += 1

    def w(self, v):
        self.b(v, v >> 8)

    def modrm(self, reg, o, codigo):
        """El prefijo de segmento, el código y el octeto ModRM (con su desplazamiento)."""
        if o.k != 'mem':
            self.b(*codigo, 0xC0 | (reg << 3) | o.reg)
            return
        if o.seg:
            self.b(PREF_SEG[o.seg])
        self.b(*codigo)
        d = o.v if o.v < 0x80000000 else o.v - 0x100000000
        if o.rm < 0:
            self.b(0x06 | (reg << 3))
            self.w(o.v)
        elif o.sim or not -128 <= d <= 127:
            self.b(0x80 | (reg << 3) | o.rm)
            self.w(o.v)
        elif d == 0 and o.rm != 6:
            self.b((reg << 3) | o.rm)
        else:
            self.b(0x40 | (reg << 3) | o.rm, d)

    def corto(self, o):
        """¿Vale el inmediato como un octeto con signo (para la forma 83h)?"""
        if o.sim:
            return False
        v = o.v & 0xFFFF
        return v <= 0x7F or v >= 0xFF80

    def tam_de(self, a, b=None):
        t = a.tam or (b.tam if b is not None else 0)
        if a.tam and b is not None and b.tam and a.tam != b.tam and b.k != 'inm':
            self.error("los dos operandos no son del mismo tamaño")
        if not t:
            self.error("no se sabe el tamaño: falta «byte» o «word»")
        return t

    def es_rm(self, o):
        return o.k in ('r16', 'r8', 'mem')

    # ---- una instrucción ----
    def instruccion(self, nem, ops):
        n = len(ops)
        if nem in SUELTAS and n == 0:
            self.b(SUELTAS[nem])
            return
        if nem in CONDICIONES_J or nem == 'jmp' or nem == 'call' or nem in BUCLES:
            self.salto(nem, ops)
            return
        ops = [self.operando(x) for x in ops]
        a = ops[0] if n > 0 else None
        c = ops[1] if n > 1 else None
        if nem == 'mov' and n == 2:
            self.mov(a, c)
        elif nem in CUENTAS and n == 2:
            self.cuenta2(CUENTAS[nem], a, c)
        elif nem == 'test' and n == 2:
            if a.k in ('r8', 'r16') and c.k == 'mem':
                a, c = c, a
            if self.es_rm(a) and c.k in ('r8', 'r16'):
                self.modrm(c.reg, a, [0x84 + (self.tam_de(a, c) == 2)])
            elif self.es_rm(a) and c.k == 'inm':
                t = self.tam_de(a, c)
                if a.k != 'mem' and a.reg == 0:
                    self.b(0xA8 + (t == 2))
                else:
                    self.modrm(0, a, [0xF6 + (t == 2)])
                self.inm(c.v, t)
            else:
                self.error("operandos que no valen para «test»")
        elif nem in ('inc', 'dec') and n == 1:
            d = nem == 'dec'
            if a.k == 'r16':
                self.b(0x40 + 8 * d + a.reg)
            elif self.es_rm(a):
                self.modrm(d, a, [0xFE + (self.tam_de(a) == 2)])
            else:
                self.error(f"operando que no vale para «{nem}»")
        elif nem in UNARIAS and n == 1:
            if not self.es_rm(a):
                self.error(f"operando que no vale para «{nem}»")
            self.modrm(UNARIAS[nem], a, [0xF6 + (self.tam_de(a) == 2)])
        elif nem in GIROS and n == 2:
            if not self.es_rm(a):
                self.error(f"operando que no vale para «{nem}»")
            t = self.tam_de(a)
            if c.k == 'r8' and c.reg == 1:
                self.modrm(GIROS[nem], a, [0xD2 + (t == 2)])
            elif c.k == 'inm' and c.v == 1 and not c.sim:
                self.modrm(GIROS[nem], a, [0xD0 + (t == 2)])
            else:
                self.error("en el 8086 se desplaza 1 o «cl»")
        elif nem == 'push' and n == 1:
            if a.k == 'r16':
                self.b(0x50 + a.reg)
            elif a.k == 'rs':
                self.b(0x06 + 8 * a.reg)
            elif a.k == 'mem':
                self.modrm(6, a, [0xFF])
            else:
                self.error("operando que no vale para «push»")
        elif nem == 'pop' and n == 1:
            if a.k == 'r16':
                self.b(0x58 + a.reg)
            elif a.k == 'rs' and a.reg != 1:
                self.b(0x07 + 8 * a.reg)
            elif a.k == 'mem':
                self.modrm(0, a, [0x8F])
            else:
                self.error("operando que no vale para «pop»")
        elif nem == 'xchg' and n == 2:
            if a.k == 'r16' and c.k == 'r16' and (a.reg == 0 or c.reg == 0):
                self.b(0x90 + a.reg + c.reg)
            elif a.k in ('r8', 'r16') and self.es_rm(c):
                self.modrm(a.reg, c, [0x86 + (self.tam_de(a, c) == 2)])
            elif a.k == 'mem' and c.k in ('r8', 'r16'):
                self.modrm(c.reg, a, [0x86 + (self.tam_de(a, c) == 2)])
            else:
                self.error("operandos que no valen para «xchg»")
        elif nem == 'lea' and n == 2 and a.k == 'r16' and c.k == 'mem':
            self.modrm(a.reg, c, [0x8D])
        elif nem == 'int' and n == 1 and a.k == 'inm':
            self.b(0xCD, a.v)
        elif nem in ('ret', 'retn', 'retf'):
            base = 0xCA if nem == 'retf' else 0xC2
            if n == 0:
                self.b(base + 1)
            elif n == 1 and a.k == 'inm':
                self.b(base)
                self.w(a.v)
            else:
                self.error(f"operando que no vale para «{nem}»")
        elif nem == 'in' and n == 2 and a.k in ('r8', 'r16') and a.reg == 0:
            if c.k == 'r16' and c.reg == 2:
                self.b(0xEC + (a.tam == 2))
            elif c.k == 'inm':
                self.b(0xE4 + (a.tam == 2), c.v)
            else:
                self.error("el puerto es «dx» o un número")
        elif nem == 'out' and n == 2 and c.k in ('r8', 'r16') and c.reg == 0:
            if a.k == 'r16' and a.reg == 2:
                self.b(0xEE + (c.tam == 2))
            elif a.k == 'inm':
                self.b(0xE6 + (c.tam == 2), a.v)
            else:
                self.error("el puerto es «dx» o un número")
        else:
            self.error(f"instrucción que no entiendo: «{nem}» con {n} operando{'s' if n != 1 else ''}")

    def inm(self, v, t):
        if t == 1:
            self.b(v)
        else:
            self.w(v)

    def mov(self, a, c):
        if a.k in ('r8', 'r16') and a.reg == 0 and c.k == 'mem' and c.rm < 0:
            if c.seg:
                self.b(PREF_SEG[c.seg])
            self.b(0xA0 + (a.tam == 2))
            self.w(c.v)
        elif c.k in ('r8', 'r16') and c.reg == 0 and a.k == 'mem' and a.rm < 0:
            if a.seg:
                self.b(PREF_SEG[a.seg])
            self.b(0xA2 + (c.tam == 2))
            self.w(a.v)
        elif self.es_rm(a) and c.k in ('r8', 'r16'):
            self.modrm(c.reg, a, [0x88 + (self.tam_de(a, c) == 2)])
        elif a.k in ('r8', 'r16') and c.k == 'mem':
            self.modrm(a.reg, c, [0x8A + (self.tam_de(a, c) == 2)])
        elif a.k in ('r16', 'mem') and c.k == 'rs':
            self.modrm(c.reg, a, [0x8C])
        elif a.k == 'rs' and c.k in ('r16', 'mem'):
            self.modrm(a.reg, c, [0x8E])
        elif a.k == 'r8' and c.k == 'inm':
            self.b(0xB0 + a.reg, c.v)
        elif a.k == 'r16' and c.k == 'inm':
            self.b(0xB8 + a.reg)
            self.w(c.v)
        elif a.k == 'mem' and c.k == 'inm':
            t = self.tam_de(a, c)
            self.modrm(0, a, [0xC6 + (t == 2)])
            self.inm(c.v, t)
        else:
            self.error("operandos que no valen para «mov»")

    def cuenta2(self, i, a, c):
        if self.es_rm(a) and c.k in ('r8', 'r16'):
            self.modrm(c.reg, a, [8 * i + (self.tam_de(a, c) == 2)])
        elif a.k in ('r8', 'r16') and c.k == 'mem':
            self.modrm(a.reg, c, [8 * i + 2 + (self.tam_de(a, c) == 2)])
        elif self.es_rm(a) and c.k == 'inm':
            t = self.tam_de(a, c)
            if t == 2 and self.corto(c):
                self.modrm(i, a, [0x83])
                self.b(c.v)
            elif a.k != 'mem' and a.reg == 0:
                self.b(8 * i + 4 + (t == 2))
                self.inm(c.v, t)
            else:
                self.modrm(i, a, [0x80 + (t == 2)])
                self.inm(c.v, t)
        else:
            self.error("operandos que no valen para esa cuenta")

    def salto(self, nem, ops):
        if len(ops) != 1:
            self.error(f"«{nem}» lleva un operando")
        s = ops[0].strip()
        bajo = s.lower()
        forma = None
        for pal in (b'short', b'near'):
            if bajo.startswith(pal + b' '):
                forma = pal
                s = s[len(pal):].strip()
        if nem in ('jmp', 'call'):
            o = self.operando(s)
            if o.k in ('r16', 'mem'):
                self.modrm(4 if nem == 'jmp' else 2, o, [0xFF])
                return
            if o.k == 'lejos':
                self.b(0xEA if nem == 'jmp' else 0x9A)
                self.w(o.v)
                self.w(o.v2)
                return
            if o.k != 'inm':
                self.error(f"operando que no vale para «{nem}»")
            destino = o.v
        else:
            destino, _ = self.cuenta(s)
        d = (destino - (self.pc + 2)) & 0xFFFF
        llega = d <= 0x7F or d >= 0xFF80 or self.e_falta    # (lo que aún no se sabe, se supone cerca)
        if nem == 'call':
            self.b(0xE8)
            self.w(destino - (self.pc + 2))
        elif nem in BUCLES:
            if not llega and self.final:
                self.error(f"«{nem}» no llega tan lejos (127 octetos)")
            self.b(BUCLES[nem], d)
        else:
            if forma == b'near' or (forma is None and not llega):
                self.largos.add(self.nl)
            if forma == b'short' and not llega and self.final:
                self.error("el salto corto no llega")
            if self.nl in self.largos:
                if nem == 'jmp':
                    self.b(0xE9)
                    self.w(destino - (self.pc + 2))
                else:
                    self.b(0x70 + (CONDICIONES_J[nem] ^ 1), 3, 0xE9)
                    self.w(destino - (self.pc + 2))
            else:
                self.b(0xEB if nem == 'jmp' else 0x70 + CONDICIONES_J[nem], d)

    # ---- los datos y las órdenes al ensamblador ----
    def datos(self, tam, resto):
        for x in parte(resto, b','):
            x = x.strip()
            if len(x) >= 2 and x[0] in b'"\'' and x[-1] == x[0]:
                for ch in x[1:-1]:
                    self.b(ch)
                for _ in range(-(len(x) - 2) % tam):
                    self.b(0)
                continue
            v, _ = self.cuenta(x)
            for k in range(tam):
                self.b(v >> (8 * k))

    def reserva(self, n):
        if not self.absoluto:
            for _ in range(n):
                self.b(0)
        else:
            self.pc += n

    def fijo(self, s, que):
        """Una cuenta que hay que saber ya (sin etiquetas de más adelante)."""
        v, _ = self.cuenta(s)
        return v

    def linea(self, ln):
        ln = sin_comentario(ln).strip()
        # etiqueta
        dos = parte(ln, b':')
        if len(dos) >= 2 and dos[0] and all(es_de_nombre(ch) for ch in dos[0]) \
                and not (48 <= dos[0][0] <= 57) and dos[0].lower().decode('latin-1') not in RS:
            nombre = dos[0]
            if nombre[0] == 46:
                nombre = self.ambito + nombre
            else:
                self.ambito = nombre
            if self.primera and nombre in self.sim:
                self.error(f"etiqueta repetida: «{nombre.decode('utf-8', 'replace')}»")
            if self.sim.get(nombre) != self.pc:
                self.cambio = True
            self.sim[nombre] = self.pc
            ln = ln[len(dos[0]) + 1:].strip()
        if not ln:
            return
        k = 0
        while k < len(ln) and ln[k] not in b' \t':
            k += 1
        nem = ln[:k].lower().decode('latin-1')
        resto = ln[k:].strip()
        while nem in PREFIJOS:
            self.b(PREFIJOS[nem])
            k = 0
            while k < len(resto) and resto[k] not in b' \t':
                k += 1
            nem = resto[:k].lower().decode('latin-1')
            resto = resto[k:].strip()
            if not nem:
                return
        if nem == 'times':
            k = 0
            while k < len(resto) and resto[k] not in b' \t':
                k += 1
            veces = self.fijo(resto[:k], 'times')
            for _ in range(veces):
                self.orden(resto[k:].strip())
            return
        self.orden(nem.encode('latin-1') + b' ' + resto)

    def orden(self, ln):
        k = 0
        while k < len(ln) and ln[k] not in b' \t':
            k += 1
        nem = ln[:k].lower().decode('latin-1')
        resto = ln[k:].strip()
        if nem in ('db', 'dw', 'dd'):
            self.datos({'db': 1, 'dw': 2, 'dd': 4}[nem], resto)
        elif nem in ('resb', 'resw', 'resd'):
            self.reserva(self.fijo(resto, nem) * {'resb': 1, 'resw': 2, 'resd': 4}[nem])
        elif nem in ('cpu', 'bits'):
            pass
        elif nem == 'org':
            self.pc = self.fijo(resto, nem)
        elif nem == 'absolute':
            self.absoluto = True
            self.pc = self.fijo(resto, nem)
        elif nem == 'section':
            # otra sección: sigue en el archivo donde iba (alineada), pero sus
            # direcciones vuelven a contar desde «vstart»
            self.absoluto = False
            for p in resto.split()[1:]:
                p = p.lower()
                if p.startswith(b'align='):
                    self.alinea(self.fijo(p[6:], nem))
                elif p.startswith(b'vstart='):
                    self.pc = self.fijo(p[7:], nem)
                else:
                    self.error("de «section» solo entiendo «vstart=» y «align=»")
        elif nem == 'align':
            # «align N, db 0»: ceros hasta que la posición en el archivo sea múltiplo de N
            dos = parte(resto, b',')
            self.alinea(self.fijo(dos[0], nem))
        else:
            self.instruccion(nem, [x for x in parte(resto, b',')] if resto else [])

    def alinea(self, n):
        if n < 1 or n & (n - 1):
            self.error("se alinea a una potencia de 2")
        while self.pos % n:
            self.b(0)

    def pasada(self):
        self.pc = 0                 # la dirección
        self.pos = 0                # la posición en el archivo
        self.absoluto = False
        self.ambito = b''
        self.sal = bytearray()
        self.cambio = False
        for i, ln in enumerate(self.lineas):
            self.nl = i + 1
            self.linea(ln)

    def ensambla(self):
        self.primera = True
        n = 0
        while True:
            antes = set(self.largos)
            self.pasada()
            self.primera = False
            n += 1
            if not self.cambio and antes == self.largos:
                break
            if n > 200:
                raise ErrorEns("los saltos no se asientan")
        self.final = True
        self.pasada()
        return bytes(self.sal)


CONDICIONES_J = {'j' + k: v for k, v in CONDICIONES.items()}


def es_de_nombre(ch):
    return ch >= 128 or 48 <= ch <= 57 or 65 <= ch <= 90 or 97 <= ch <= 122 or ch in b'_.$@?'


def sin_comentario(ln):
    comilla = 0
    for i, ch in enumerate(ln):
        if comilla:
            if ch == comilla:
                comilla = 0
        elif ch in b'"\'':
            comilla = ch
        elif ch == 59:
            return ln[:i]
    return ln


def parte(s, sep):
    """Parte s por el separador, sin mirar dentro de comillas, corchetes ni paréntesis."""
    trozos = []
    comilla = 0
    prof = 0
    j = 0
    for i, ch in enumerate(s):
        if comilla:
            if ch == comilla:
                comilla = 0
        elif ch in b'"\'':
            comilla = ch
        elif ch in b'[(':
            prof += 1
        elif ch in b'])':
            prof -= 1
        elif ch == sep[0] and prof == 0:
            trozos.append(s[j:i])
            j = i + 1
    trozos.append(s[j:])
    return trozos


def main():
    args = sys.argv[1:]
    salida = None
    if '-o' in args:
        i = args.index('-o')
        salida = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        print("uso: ensambla.py ENTRADA.asm [-o SALIDA]", file=sys.stderr)
        return 2
    entrada = args[0]
    if salida is None:
        salida = entrada.rsplit('.', 1)[0] + '.COM'
    with open(entrada, 'rb') as f:
        lineas = f.read().replace(b'\r', b'').split(b'\n')
    try:
        datos = Ensamblador(lineas).ensambla()
    except ErrorEns as e:
        print(f"{entrada}: error: {e}", file=sys.stderr)
        return 1
    with open(salida, 'wb') as f:
        f.write(datos)
    return 0


if __name__ == '__main__':
    sys.exit(main())
