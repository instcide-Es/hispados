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
#  Los saltos, como NASM: se pasa por el archivo las veces que haga falta. En
#  cada pasada un salto va corto si llega (a 127 octetos) contando con dónde
#  están ahora las etiquetas: las de atrás, donde han quedado en esta pasada;
#  las de delante, donde quedaron en la anterior (en la primera aún no se
#  saben: se supone que llega). Cuando ninguna etiqueta cambia de sitio, se ha
#  acabado. Un «jmp» largo son 3 octetos; un salto condicional largo, el
#  contrario por encima de un «jmp» (5 octetos).
#  [CASTEL64] Y, desde el 21/11/2026, «bits 64» (CASTEL64.md §8): el x86-64 que
#  saca el compilador para «modelo linux64» y «núcleo64» (los registros de 64
#  bits y r8-r15, el prefijo REX, movsxd, cdqe, cqo, iretq, syscall, dq...),
#  con las mismas formas que NASM.
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# =============================================================================
import sys

R32 = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
R16 = ['ax', 'cx', 'dx', 'bx', 'sp', 'bp', 'si', 'di']
R8 = ['al', 'cl', 'dl', 'bl', 'ah', 'ch', 'dh', 'bh']
# [CASTEL64] los de 64 bits y los nuevos (r8 a r15) de cada tamaño: el número
# es su sitio en la lista (de 8 en adelante, con el bit de más en REX)
R64 = ['rax', 'rcx', 'rdx', 'rbx', 'rsp', 'rbp', 'rsi', 'rdi'] + [f'r{i}' for i in range(8, 16)]
R32X = [f'r{i}d' for i in range(8, 16)]
R16X = [f'r{i}w' for i in range(8, 16)]
R8X = [f'r{i}b' for i in range(8, 16)]
RS = ['es', 'cs', 'ss', 'ds', 'fs', 'gs']
RC = ['cr0', 'cr1', 'cr2', 'cr3', 'cr4']
PREF_SEG = {'es': 0x26, 'cs': 0x2E, 'ss': 0x36, 'ds': 0x3E, 'fs': 0x64, 'gs': 0x65}
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
           'xlatb': 0xD7, 'iret': 0xCF, 'into': 0xCE,
           'cdq': 0x99, 'cwde': 0x98, 'lodsd': 0xAD, 'stosd': 0xAB, 'movsd': 0xA5, 'cmpsd': 0xA7,
           'scasd': 0xAF, 'pushad': 0x60, 'popad': 0x61, 'pushfd': 0x9C, 'popfd': 0x9D, 'iretd': 0xCF,
           'pusha': 0x60, 'popa': 0x61, 'int3': 0xCC,
           'cdqe': 0x98, 'cqo': 0x99, 'iretq': 0xCF, 'lodsq': 0xAD, 'stosq': 0xAB, 'movsq': 0xA5,
           'cmpsq': 0xA7, 'scasq': 0xAF}
# [CASTEL64] las sueltas de dos octetos (0Fh y otro)
SUELTAS2 = {'syscall': 0x05, 'sysret': 0x07, 'rdmsr': 0x32, 'wrmsr': 0x30, 'cpuid': 0xA2, 'rdtsc': 0x31}
# las sueltas que son de 16 o de 32 bits (las otras van como diga «bits»): en
# el otro modo llevan delante el prefijo 66h
TAM_SUELTA = {'cwd': 2, 'cbw': 2, 'lodsw': 2, 'stosw': 2, 'movsw': 2, 'cmpsw': 2, 'scasw': 2,
              'cdq': 4, 'cwde': 4, 'lodsd': 4, 'stosd': 4, 'movsd': 4, 'cmpsd': 4, 'scasd': 4,
              'pushad': 4, 'popad': 4, 'pushfd': 4, 'popfd': 4, 'iretd': 4,
              'cdqe': 8, 'cqo': 8, 'iretq': 8, 'lodsq': 8, 'stosq': 8, 'movsq': 8, 'cmpsq': 8, 'scasq': 8}
PREFIJOS = {'rep': 0xF3, 'repe': 0xF3, 'repz': 0xF3, 'repne': 0xF2, 'repnz': 0xF2}
BUCLES = {'loopne': 0xE0, 'loopnz': 0xE0, 'loope': 0xE1, 'loopz': 0xE1, 'loop': 0xE2, 'jcxz': 0xE3,
          'jecxz': 0xE3}
ESCALAS = {1: 0, 2: 1, 4: 2, 8: 3}


class ErrorEns(Exception):
    pass


class Op:
    """Un operando: k es 'r64', 'r32', 'r16', 'r8', 'rs', 'rc', 'mem', 'inm' o 'lejos'."""
    def __init__(self, k):
        self.k = k
        self.reg = 0
        self.tam = 0                # 1, 2, 4 o 0 (no se sabe)
        self.seg = None             # prefijo de segmento
        self.rm = -1                # memoria: 0-7, o -1 si es una dirección sin registros
        self.v = 0                  # el número (inmediato o desplazamiento)
        self.sim = False            # ¿lleva alguna etiqueta la cuenta?
        self.falta = False          # ¿alguna que aún no se sabe? (la primera pasada)
        self.v2 = 0                 # 'lejos': el segmento
        self.a32 = False            # memoria con registros de 32 bits: [base+índice*escala+n]
        self.base = -1              # (en 64 bits, de 0 a 15)
        self.indice = -1
        self.escala = 1
        self.a67 = False            # [CASTEL64] en 64 bits, con registros de 32: el prefijo 67h


class Ensamblador:
    def __init__(self, lineas):
        self.lineas = lineas
        self.sim = {}               # etiqueta -> valor (de la pasada anterior, y de esta)
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

    def mascara(self):
        """Las cuentas son de 32 bits; en «bits 64», de 64 (salvo multiplicar,
        dividir y el resto, que son de 32: con los 32 de abajo de cada uno)."""
        return 0xFFFFFFFFFFFFFFFF if self.bits == 64 else 0xFFFFFFFF

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
                v = (v & 0xFFFFFFFF) * (w & 0xFFFFFFFF) & 0xFFFFFFFF    # (en 64 bits, solo los 32 de abajo)
            else:
                v &= 0xFFFFFFFF
                w &= 0xFFFFFFFF
                if w == 0:
                    if self.final:
                        self.error("división por cero")
                    w = 1
                v = v // w if op == b'/' else v % w
            v &= self.mascara()

    def c_unario(self):
        self.c_blancos()
        if self.ei >= len(self.e):
            self.error("falta un número o una etiqueta")
        c = chr(self.e[self.ei])
        if c == '-':
            self.ei += 1
            return -self.c_unario() & self.mascara()
        if c == '+':
            self.ei += 1
            return self.c_unario()
        if c == '~':
            self.ei += 1
            return ~self.c_unario() & self.mascara()
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
        for pal, t in ((b'byte', 1), (b'word', 2), (b'dword', 4), (b'qword', 8)):
            if bajo.startswith(pal) and len(s) > len(pal) and s[len(pal)] in b' \t[':
                tam = t
                s = s[len(pal):].strip()
                bajo = s.lower()
        nombre = bajo.decode('latin-1')
        if nombre in R64 or nombre in R32X or nombre in R16X or nombre in R8X:
            if self.bits != 64:
                self.error(f"«{nombre}» es un registro de 64 bits (falta «bits 64»)")
            for lista, k, t, desde in ((R64, 'r64', 8, 0), (R32X, 'r32', 4, 8), (R16X, 'r16', 2, 8), (R8X, 'r8', 1, 8)):
                if nombre in lista:
                    o = Op(k)
                    o.reg = lista.index(nombre) + desde
                    o.tam = t
                    return o
        if nombre in R32:
            o = Op('r32')
            o.reg = R32.index(nombre)
            o.tam = 4
            return o
        if nombre in RC:
            o = Op('rc')
            o.reg = RC.index(nombre)
            o.tam = 4
            return o
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
        o.falta = self.e_falta
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
        regs32 = []                 # (registro, escala)
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
            r32 = reg_escalado(n)
            if r32 is None and self.bits == 64:
                r32 = reg_escalado(n, 64)
            if n in ('bx', 'bp', 'si', 'di') and signo == b'+':
                regs.append(n)
            elif r32 is not None and signo == b'+':
                regs32.append(r32)
            elif t:
                resto += signo + t
            elif j < len(s) or i == 0:
                if not (i == 0 and j < len(s)):
                    self.error("dirección mal escrita")
            if j >= len(s):
                break
            signo = s[j:j + 1]
            i = j + 1
        if regs32:
            if regs:
                self.error("no se mezclan registros de 16 y de 32 bits en una dirección")
            self.memoria32(o, regs32)
        elif self.bits >= 32:
            o.a32 = True
        if regs:
            if self.bits >= 32:
                self.error("en 32 bits las direcciones van con registros de 32 (ebx, esi...)")
            regs.sort(key=lambda r: ('bx', 'bp', 'si', 'di').index(r))
            if tuple(regs) not in BASES:
                self.error("esos registros no valen juntos en una dirección")
            o.rm = BASES[tuple(regs)]
        if resto:
            o.v, o.sim = self.cuenta(resto)
        return o

    def memoria32(self, o, regs):
        """[base+índice*escala]: como NASM, el primero sin escala es la base y el
        otro el índice (salvo esp, que no puede ser índice); un índice por 2, 3,
        5 o 9 sin base se parte en base e índice ([esi*2] es [esi+esi]). En 64
        bits, con los registros de 64 (o con los de 32 y el prefijo 67h)."""
        if self.bits == 16:
            self.error("en 16 bits las direcciones van con registros de 16 (bx, bp, si, di)")
        o.a32 = True
        anchos = {r[0] >= 100 for r in regs}
        if len(anchos) > 1:
            self.error("no se mezclan registros de 32 y de 64 bits en una dirección")
        if self.bits == 64 and not anchos.pop():
            o.a67 = True
        regs = [(r - 100 if r >= 100 else r, e) for r, e in regs]
        base = indice = -1
        escala = 1
        for r, e in regs:
            if e is None and base < 0:
                base = r
            elif indice < 0:
                indice = r
                escala = e or 1
            else:
                self.error("demasiados registros en la dirección")
        if indice == 4:
            if escala != 1 or base == 4:
                self.error("esp no vale como índice")
            base, indice = indice, base
        if base < 0 and escala in (2, 3, 5, 9):
            base = indice
            escala -= 1
        if escala not in ESCALAS:
            self.error("la escala es 1, 2, 4 u 8")
        o.base, o.indice, o.escala = base, indice, escala

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

    def d(self, v):
        self.b(v, v >> 8, v >> 16, v >> 24)

    def p66(self, t):
        """El prefijo de tamaño (66h) si la operación no es del tamaño del modo
        (en 64 bits, el de las operaciones es 32)."""
        if t in (2, 4) and t != min(self.bits // 8, 4):
            self.b(0x66)

    def rex(self, t, r=0, x=0, b=0):
        """[CASTEL64] El prefijo REX, si hace falta: W (operación de 64 bits), R
        (el registro de ModRM, de 8 a 15), X (el índice) y B (la base, o el
        registro de r/m o del código)."""
        v = (8 if t == 8 else 0) | (4 if r >= 8 else 0) | (2 if x >= 8 else 0) | (1 if b >= 8 else 0)
        if v:
            if self.bits != 64:
                self.error("eso es de 64 bits (falta «bits 64»)")
            self.b(0x40 | v)

    def pre(self, t, r=0, x=0, b=0):
        """El de tamaño (66h) y el REX, por ese orden."""
        self.p66(t)
        self.rex(t, r, x, b)

    def modrm(self, reg, o, codigo, t=0):
        """El prefijo de segmento, el de tamaño (si la operación es de t octetos y
        no es la del modo), el REX (en 64 bits), el código y el octeto ModRM (con
        su desplazamiento)."""
        if o.k != 'mem':
            self.pre(t, reg, 0, o.reg)
            self.b(*codigo, 0xC0 | ((reg & 7) << 3) | (o.reg & 7))
            return
        if o.seg:
            self.b(PREF_SEG[o.seg])
        if o.a67:
            self.b(0x67)
        self.p66(t)
        self.rex(t, reg, max(o.indice, 0), max(o.base, 0))
        reg &= 7
        self.b(*codigo)
        d = o.v & 0xFFFFFFFF                    # (el desplazamiento, de 32 bits con su signo)
        d = d if d < 0x80000000 else d - 0x100000000
        if o.a32:
            self.modrm32(reg, o, d)
            return
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

    def modrm32(self, reg, o, d):
        """ModRM (y SIB) de una dirección de 32 bits, con el desplazamiento más corto."""
        base, ind = o.base, o.indice
        largo = o.sim or not -128 <= d <= 127
        if base < 0 and ind < 0:
            if self.bits == 64:                 # en 64 bits, [n] es con SIB (sin él, sería relativo a RIP)
                self.b(0x04 | (reg << 3), 0x25)
            else:
                self.b(0x05 | (reg << 3))
            self.d(o.v)
            return
        if base < 0:                            # índice sin base: SIB con 32 bits de desplazamiento
            self.b(0x04 | (reg << 3), (ESCALAS[o.escala] << 6) | ((ind & 7) << 3) | 5)
            self.d(o.v)
            return
        if largo:
            mod = 0x80
        elif d == 0 and base & 7 != 5:
            mod = 0
        else:
            mod = 0x40
        if ind < 0 and base & 7 != 4:
            self.b(mod | (reg << 3) | (base & 7))
        else:
            self.b(mod | (reg << 3) | 4, (ESCALAS[o.escala] << 6) | ((4 if ind < 0 else ind & 7) << 3) | (base & 7))
        if mod == 0x80:
            self.d(o.v)
        elif mod == 0x40:
            self.b(d)

    def corto(self, o, t=2):
        """¿Vale el inmediato como un octeto con signo (para la forma 83h)? Como en
        NASM: con una etiqueta, no (aunque el número quepa); con una que aún no
        se sabe (en la primera pasada), se supone que sí."""
        if o.falta:
            return True
        if o.sim:
            return False
        if t == 8:
            v = o.v & 0xFFFFFFFFFFFFFFFF
            return v <= 0x7F or v >= 0xFFFFFFFFFFFFFF80
        if t == 4:
            v = o.v & 0xFFFFFFFF
            return v <= 0x7F or v >= 0xFFFFFF80
        v = o.v & 0xFFFF
        return v <= 0x7F or v >= 0xFF80

    def tam_de(self, a, b=None):
        t = a.tam or (b.tam if b is not None else 0)
        if a.tam and b is not None and b.tam and a.tam != b.tam and b.k != 'inm':
            self.error("los dos operandos no son del mismo tamaño")
        if not t:
            self.error("no se sabe el tamaño: falta «byte», «word» o «dword»")
        return t

    def es_rm(self, o):
        return o.k in ('r64', 'r32', 'r16', 'r8', 'mem')

    def es_reg(self, o):
        return o.k in ('r64', 'r32', 'r16', 'r8')

    # ---- una instrucción ----
    def instruccion(self, nem, ops):
        n = len(ops)
        if nem in SUELTAS and n == 0:
            self.pre(TAM_SUELTA.get(nem, 0))
            self.b(SUELTAS[nem])
            return
        if nem in SUELTAS2 and n == 0:
            self.b(0x0F, SUELTAS2[nem])
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
            if self.es_reg(a) and c.k == 'mem':
                a, c = c, a
            if self.es_rm(a) and self.es_reg(c):
                t = self.tam_de(a, c)
                self.modrm(c.reg, a, [0x84 + (t != 1)], t)
            elif self.es_rm(a) and c.k == 'inm':
                t = self.tam_de(a, c)
                if a.k != 'mem' and a.reg == 0:
                    self.pre(t)
                    self.b(0xA8 + (t != 1))
                else:
                    self.modrm(0, a, [0xF6 + (t != 1)], t)
                self.inm(c.v, t)
            else:
                self.error("operandos que no valen para «test»")
        elif nem in ('inc', 'dec') and n == 1:
            d = nem == 'dec'
            if a.k in ('r16', 'r32') and self.bits != 64:
                self.p66(a.tam)
                self.b(0x40 + 8 * d + a.reg)
            elif self.es_rm(a):
                t = self.tam_de(a)
                self.modrm(d, a, [0xFE + (t != 1)], t)
            else:
                self.error(f"operando que no vale para «{nem}»")
        elif nem == 'imul' and n >= 2:
            self.imul(ops)
        elif nem in UNARIAS and n == 1:
            if not self.es_rm(a):
                self.error(f"operando que no vale para «{nem}»")
            t = self.tam_de(a)
            self.modrm(UNARIAS[nem], a, [0xF6 + (t != 1)], t)
        elif nem in GIROS and n == 2:
            if not self.es_rm(a):
                self.error(f"operando que no vale para «{nem}»")
            t = self.tam_de(a)
            if c.k == 'r8' and c.reg == 1:
                self.modrm(GIROS[nem], a, [0xD2 + (t != 1)], t)
            elif c.k == 'inm' and c.v == 1 and not c.sim:
                self.modrm(GIROS[nem], a, [0xD0 + (t != 1)], t)
            elif c.k == 'inm' and self.bits >= 32:
                self.modrm(GIROS[nem], a, [0xC0 + (t != 1)], t)
                self.b(c.v)
            else:
                self.error("en el 8086 se desplaza 1 o «cl»")
        elif nem == 'movsxd' and n == 2 and a.k == 'r64' and (c.k == 'r32' or (c.k == 'mem' and c.tam in (0, 4))):
            self.modrm(a.reg, c, [0x63], 8)
        elif nem in ('movzx', 'movsx') and n == 2 and a.k in ('r16', 'r32', 'r64') and self.es_rm(c):
            t = c.tam
            if t not in (1, 2) or (t == 2 and a.tam == 2):
                self.error(f"«{nem}»: lo de la derecha es un byte o una palabra (falta «byte» o «word»)")
            self.modrm(a.reg, c, [0x0F, (0xB6 if nem == 'movzx' else 0xBE) + (t == 2)], a.tam)
        elif nem == 'push' and n == 1:
            if a.k in ('r16', 'r64') and self.bits == 64:
                self.p66(a.tam)
                self.rex(0, 0, 0, a.reg)
                self.b(0x50 + (a.reg & 7))
            elif a.k in ('r16', 'r32') and self.bits != 64:
                self.p66(a.tam)
                self.b(0x50 + a.reg)
            elif a.k == 'rs' and a.reg < 4:
                self.b(0x06 + 8 * a.reg)
            elif a.k == 'rs':
                self.b(0x0F, 0xA0 + 8 * (a.reg - 4))
            elif a.k == 'mem':
                self.modrm(6, a, [0xFF], a.tam if a.tam != 8 else 0) if self.bits == 64 else \
                    self.modrm(6, a, [0xFF], a.tam or self.bits // 8)
            elif a.k == 'inm' and self.bits >= 32:
                if self.corto(a, 4):
                    self.b(0x6A, a.v)
                else:
                    self.b(0x68)
                    self.d(a.v)
            else:
                self.error("operando que no vale para «push»")
        elif nem == 'pop' and n == 1:
            if a.k in ('r16', 'r64') and self.bits == 64:
                self.p66(a.tam)
                self.rex(0, 0, 0, a.reg)
                self.b(0x58 + (a.reg & 7))
            elif a.k in ('r16', 'r32') and self.bits != 64:
                self.p66(a.tam)
                self.b(0x58 + a.reg)
            elif a.k == 'rs' and a.reg < 4 and a.reg != 1:
                self.b(0x07 + 8 * a.reg)
            elif a.k == 'rs' and a.reg >= 4:
                self.b(0x0F, 0xA1 + 8 * (a.reg - 4))
            elif a.k == 'mem':
                self.modrm(0, a, [0x8F], a.tam if a.tam != 8 else 0) if self.bits == 64 else \
                    self.modrm(0, a, [0x8F], a.tam or self.bits // 8)
            else:
                self.error("operando que no vale para «pop»")
        elif nem == 'xchg' and n == 2:
            if a.k in ('r16', 'r32', 'r64') and c.k == a.k and (a.reg == 0 or c.reg == 0) \
                    and not (a.k == 'r32' and self.bits == 64 and a.reg == c.reg):
                self.pre(a.tam, 0, 0, a.reg + c.reg)
                self.b(0x90 + ((a.reg + c.reg) & 7))
            elif self.es_reg(a) and self.es_rm(c):
                t = self.tam_de(a, c)
                self.modrm(a.reg, c, [0x86 + (t != 1)], t)
            elif a.k == 'mem' and self.es_reg(c):
                t = self.tam_de(a, c)
                self.modrm(c.reg, a, [0x86 + (t != 1)], t)
            else:
                self.error("operandos que no valen para «xchg»")
        elif nem == 'lea' and n == 2 and a.k in ('r16', 'r32', 'r64') and c.k == 'mem':
            self.modrm(a.reg, c, [0x8D], a.tam)
        elif nem == 'ltr' and n == 1 and a.k in ('mem', 'r16'):
            self.modrm(3, a, [0x0F, 0x00])
        elif nem in ('lgdt', 'lidt', 'sgdt', 'sidt') and n == 1 and a.k == 'mem':
            self.modrm({'sgdt': 0, 'sidt': 1, 'lgdt': 2, 'lidt': 3}[nem], a, [0x0F, 0x01])
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
        elif nem == 'in' and n == 2 and self.es_reg(a) and a.reg == 0:
            self.p66(a.tam)
            if c.k == 'r16' and c.reg == 2:
                self.b(0xEC + (a.tam != 1))
            elif c.k == 'inm':
                self.b(0xE4 + (a.tam != 1), c.v)
            else:
                self.error("el puerto es «dx» o un número")
        elif nem == 'out' and n == 2 and self.es_reg(c) and c.reg == 0:
            self.p66(c.tam)
            if a.k == 'r16' and a.reg == 2:
                self.b(0xEE + (c.tam != 1))
            elif a.k == 'inm':
                self.b(0xE6 + (c.tam != 1), a.v)
            else:
                self.error("el puerto es «dx» o un número")
        else:
            self.error(f"instrucción que no entiendo: «{nem}» con {n} operando{'s' if n != 1 else ''}")

    def imul(self, ops):
        """imul de dos o tres operandos (386): registro, r/m e inmediato."""
        a = ops[0]
        c = ops[1]
        if a.k not in ('r16', 'r32', 'r64') or len(ops) > 3:
            self.error("operandos que no valen para «imul»")
        if len(ops) == 2 and c.k == 'inm':
            c, i = a, c                         # imul eax, 10 = imul eax, eax, 10
        elif len(ops) == 3:
            i = ops[2]
            if not self.es_rm(c) or i.k != 'inm':
                self.error("operandos que no valen para «imul»")
        else:
            if not self.es_rm(c):
                self.error("operandos que no valen para «imul»")
            self.modrm(a.reg, c, [0x0F, 0xAF], self.tam_de(a, c))
            return
        t = self.tam_de(a, c)
        if self.corto(i, t):
            self.modrm(a.reg, c, [0x6B], t)
            self.b(i.v)
        else:
            self.modrm(a.reg, c, [0x69], t)
            self.inm(i.v, t)

    def inm(self, v, t):
        if t == 1:
            self.b(v)
        elif t in (4, 8):                       # (en 64 bits, de 32 con su signo)
            self.d(v)
        else:
            self.w(v)

    def dir_fija(self, o):
        """La dirección de una memoria sin registros: de 16 o de 32 bits según el modo."""
        if o.a32:
            self.d(o.v)
        else:
            self.w(o.v)

    def mov(self, a, c):
        if self.bits == 64 and a.k in ('r64', 'r32') and c.k == 'inm':
            self.mov64(a, c)
        elif self.bits == 64 and (a.k == 'rc' and c.k == 'r64' or a.k == 'r64' and c.k == 'rc'):
            r, x = (a, c) if a.k == 'rc' else (c, a)
            self.b(0x0F, 0x22 if a.k == 'rc' else 0x20, 0xC0 | (r.reg << 3) | x.reg)
        elif self.es_reg(a) and a.reg == 0 and c.k == 'mem' and c.rm < 0 and c.base < 0 and c.indice < 0 \
                and self.bits != 64:
            if c.seg:
                self.b(PREF_SEG[c.seg])
            self.p66(a.tam)
            self.b(0xA0 + (a.tam != 1))
            self.dir_fija(c)
        elif self.es_reg(c) and c.reg == 0 and a.k == 'mem' and a.rm < 0 and a.base < 0 and a.indice < 0 \
                and self.bits != 64:
            if a.seg:
                self.b(PREF_SEG[a.seg])
            self.p66(c.tam)
            self.b(0xA2 + (c.tam != 1))
            self.dir_fija(a)
        elif self.es_rm(a) and self.es_reg(c):
            t = self.tam_de(a, c)
            self.modrm(c.reg, a, [0x88 + (t != 1)], t)
        elif self.es_reg(a) and c.k == 'mem':
            t = self.tam_de(a, c)
            self.modrm(a.reg, c, [0x8A + (t != 1)], t)
        elif a.k in ('r16', 'r32', 'mem') and c.k == 'rs':
            self.modrm(c.reg, a, [0x8C], a.tam if a.k == 'r16' else 0)
        elif a.k == 'rs' and c.k in ('r16', 'r32', 'mem'):
            self.modrm(a.reg, c, [0x8E])
        elif a.k == 'rc' and c.k == 'r32':
            self.b(0x0F, 0x22, 0xC0 | (a.reg << 3) | c.reg)
        elif a.k == 'r32' and c.k == 'rc':
            self.b(0x0F, 0x20, 0xC0 | (c.reg << 3) | a.reg)
        elif a.k == 'r8' and c.k == 'inm':
            self.b(0xB0 + a.reg, c.v)
        elif a.k in ('r16', 'r32') and c.k == 'inm':
            self.p66(a.tam)
            self.b(0xB8 + a.reg)
            self.inm(c.v, a.tam)
        elif a.k == 'mem' and c.k == 'inm':
            t = self.tam_de(a, c)
            self.modrm(0, a, [0xC6 + (t != 1)], t)
            self.inm(c.v, t)
        else:
            self.error("operandos que no valen para «mov»")

    def mov64(self, a, c):
        """[CASTEL64] mov a un registro de 64 (o de 32) un número: como NASM, el
        de 32 bits si vale (pone a cero los de arriba), el de 32 con su signo
        (C7h) si es negativo y cabe, o el de 64 entero; con una etiqueta, si es
        de 64 bits, siempre el de 64 entero."""
        if a.k == 'r32':
            self.rex(0, 0, 0, a.reg)
            self.b(0xB8 + (a.reg & 7))
            self.d(c.v)
            return
        v = c.v & 0xFFFFFFFFFFFFFFFF
        if not c.sim and not c.falta and v <= 0xFFFFFFFF:
            self.rex(0, 0, 0, a.reg)
            self.b(0xB8 + (a.reg & 7))
            self.d(v)
        elif not c.sim and not c.falta and v >= 0xFFFFFFFF80000000:
            self.rex(8, 0, 0, a.reg)
            self.b(0xC7, 0xC0 | (a.reg & 7))
            self.d(v)
        else:
            self.rex(8, 0, 0, a.reg)
            self.b(0xB8 + (a.reg & 7))
            self.d(v)
            self.d(v >> 32)

    def cuenta2(self, i, a, c):
        if self.es_rm(a) and self.es_reg(c):
            t = self.tam_de(a, c)
            self.modrm(c.reg, a, [8 * i + (t != 1)], t)
        elif self.es_reg(a) and c.k == 'mem':
            t = self.tam_de(a, c)
            self.modrm(a.reg, c, [8 * i + 2 + (t != 1)], t)
        elif self.es_rm(a) and c.k == 'inm':
            t = self.tam_de(a, c)
            if t != 1 and self.corto(c, t):
                self.modrm(i, a, [0x83], t)
                self.b(c.v)
            elif a.k != 'mem' and a.reg == 0:
                self.pre(t)
                self.b(8 * i + 4 + (t != 1))
                self.inm(c.v, t)
            else:
                self.modrm(i, a, [0x80 + (t != 1)], t)
                self.inm(c.v, t)
        else:
            self.error("operandos que no valen para esa cuenta")

    def salto(self, nem, ops):
        if len(ops) != 1:
            self.error(f"«{nem}» lleva un operando")
        s = ops[0].strip()
        bajo = s.lower()
        forma = None
        # «strict»: como en NASM, el tamaño que se dice y no otro (aquí
        # «short» y «near» ya se cumplen siempre; NASM 3, sin él, acorta
        # un «jmp near» que llega con un octeto)
        if bajo.startswith(b'strict ') or bajo.startswith(b'strict\t'):
            s = s[6:].strip()
            bajo = s.lower()
        for pal in (b'short', b'near', b'dword'):
            if bajo.startswith(pal + b' '):
                forma = pal if pal != b'dword' else None
                s = s[len(pal):].strip()
                bajo = s.lower()
        b32 = self.bits >= 32
        if nem in ('jmp', 'call'):
            o = self.operando(s)
            if o.k in ('r16', 'r32', 'r64', 'mem'):
                self.modrm(4 if nem == 'jmp' else 2, o, [0xFF])
                return
            if o.k == 'lejos':
                self.b(0xEA if nem == 'jmp' else 0x9A)
                if b32:
                    self.d(o.v)
                else:
                    self.w(o.v)
                self.w(o.v2)
                return
            if o.k != 'inm':
                self.error(f"operando que no vale para «{nem}»")
            destino = o.v
        else:
            destino, _ = self.cuenta(s)
        if b32:
            d = (destino - (self.pc + 2)) & 0xFFFFFFFF
            llega = d <= 0x7F or d >= 0xFFFFFF80 or self.e_falta
        else:
            d = (destino - (self.pc + 2)) & 0xFFFF
            llega = d <= 0x7F or d >= 0xFF80 or self.e_falta    # (lo que aún no se sabe, se supone cerca)
        if nem == 'call':
            self.b(0xE8)
            if b32:
                self.d(destino - (self.pc + 4))
            else:
                self.w(destino - (self.pc + 2))
        elif nem in BUCLES:
            if not llega and self.final:
                self.error(f"«{nem}» no llega tan lejos (127 octetos)")
            if (nem == 'jcxz' and b32) or (nem == 'jecxz' and not b32):
                self.b(0x67)                    # (la cuenta, desde detrás del prefijo)
                d = (destino - (self.pc + 2)) & (0xFFFFFFFF if b32 else 0xFFFF)
            self.b(BUCLES[nem], d)
        else:
            if forma == b'short' and not llega and self.final:
                self.error("el salto corto no llega")
            if forma == b'near' or (forma is None and not llega):
                if nem == 'jmp':
                    self.b(0xE9)
                    if b32:
                        self.d(destino - (self.pc + 4))
                    else:
                        self.w(destino - (self.pc + 2))
                elif b32:
                    self.b(0x0F, 0x80 + CONDICIONES_J[nem])
                    self.d(destino - (self.pc + 4))
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
        if nem in ('db', 'dw', 'dd', 'dq'):
            self.datos({'db': 1, 'dw': 2, 'dd': 4, 'dq': 8}[nem], resto)
        elif nem in ('resb', 'resw', 'resd', 'resq'):
            self.reserva(self.fijo(resto, nem) * {'resb': 1, 'resw': 2, 'resd': 4, 'resq': 8}[nem])
        elif nem == 'cpu':
            pass
        elif nem == 'bits':
            self.bits = self.fijo(resto, nem)
            if self.bits not in (16, 32, 64):
                self.error("«bits» es 16, 32 o 64")
        elif nem == 'org':
            # [HISPA-64] en 64 bits aunque vaya en «bits 32» (el núcleo de la
            # mitad alta: «org 0xFFFFFFFF80100000» con el arranque de 32 bits)
            b, self.bits = self.bits, 64
            self.pc = self.fijo(resto, nem)
            self.bits = b
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
        self.bits = 16              # el modo: 16 o 32 bits («bits 32»)
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
            self.pasada()
            self.primera = False
            n += 1
            if not self.cambio:
                break
            if n > 1000:                         # (el núcleo de la 0.8.4 pide unas 300: lo que se mueve avanza poco a poco)
                raise ErrorEns("los saltos no se asientan")
        self.final = True
        self.pasada()
        return bytes(self.sal)


CONDICIONES_J = {'j' + k: v for k, v in CONDICIONES.items()}


def reg_escalado(n, bits=32):
    """«esi», «esi*4» o «4*esi»: (registro, escala); la escala, None si no la lleva.
    Con bits = 64, los de 64 bits («rsi*4», «r12»): su número más 100."""
    lista = R32 if bits == 32 else R64
    mas = 0 if bits == 32 else 100
    if n in lista:
        return (lista.index(n) + mas, None)
    dos = n.replace(' ', '').replace('\t', '').split('*')
    if len(dos) == 2:
        if dos[1] in lista:
            dos.reverse()
        if dos[0] in lista and dos[1].isdigit():
            return (lista.index(dos[0]) + mas, int(dos[1]))
    return None


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
