#!/usr/bin/env python3
"""Ensamblador de Z80 pequeño, para los juegos propios de Hispa-System.

uso: asmz80.py FUENTE.s SALIDA [-l LISTADO] [-D NOMBRE=valor]...

Hecho para el proyecto (GPL-2.0-or-later), sin dependencias, con la misma
forma que tools/asm6502.py: con él se ensamblan en la compilación los
cartuchos propios (.SMS) y las ROM de prueba del emulador. La salida es
plana: los octetos salen en el orden en que se escriben.

Sintaxis (una instrucción por línea; «;» empieza un comentario):
  etiqueta:           etiqueta global          @local:   local (vale hasta la
                                                         siguiente global)
  NOMBRE = expresión  constante (también «NOMBRE equ expresión»)
  Instrucciones con los nombres de Zilog, en mayúsculas o minúsculas:
  ld a,(ix+5)  ld (iy-2),n  ld hl,(nn)  jr nz,etiqueta  djnz @bucle ...
  Todas las oficiales (principales, CB, ED, DD/FD, DDCB/FDCB) y las no
  documentadas habituales: ixh ixl iyh iyl (en ld, inc, dec y las de la
  ALU), sll (o sl1), «in (c)» o «in f,(c)», «out (c),0» y las de DDCB que
  dejan además el resultado en un registro: «rlc (ix+d),b», «set 3,(iy+d),a».
  (ix) sin desplazamiento es (ix+0). «add a,x» y «add x» valen igual (lo
  mismo con adc, sub, sbc, and, xor, or y cp). «ex af,af'».
  .org DIR            dirección del código que sigue (no mueve la salida)
  .pad DIR [,valor]   rellena la salida hasta esa dirección (por omisión FFh)
  .align N [,valor]   rellena hasta una dirección múltiplo de N
  .byte e,"texto",... octetos (también .db; en el texto, \\" \\\\ \\n)
  .word e,...         palabras, bajo y alto (también .dw)
  .res N [,valor]     N octetos iguales (también .ds; por omisión 0)
  .incbin "archivo"   mete un archivo tal cual
  .include "archivo"  (las rutas, desde el archivo que incluye)
  .tiles ... .endtiles  dibujos de 8x8 en texto para la Master System: 8
                      líneas de 8 caracteres por dibujo («.» o 0-9, a-f =
                      color 0-15); cada uno, 32 octetos: por fila, los planos
                      0, 1, 2 y 3 (bit 7 = el punto de la izquierda)
  .cabecera_sega CODIGO, VERSION, REGION_TAM
                      los 16 octetos de la cabecera del cartucho ("TMR SEGA",
                      dos ceros, la suma de los octetos de la salida que hay
                      antes, el código de producto en BCD, la versión y el
                      octeto de región y tamaño, p. ej. 4Ch)
Expresiones: $hex 0xhex 0FFh %binario 0b101 decimal 'c' etiquetas, $ (la
dirección actual), + - * / % & | ^ << >> ~ ( ), y delante - + ~ < (octeto
bajo) y > (octeto alto). Un operando entre paréntesis es memoria: «ld a,(5)».
Small two-pass Z80 assembler (flat output), written for this project.
"""
import os, re, sys


class Error(Exception):
    pass


R8 = {'b': 0, 'c': 1, 'd': 2, 'e': 3, 'h': 4, 'l': 5, 'a': 7}
XH = {'ixh': (0xDD, 4), 'ixl': (0xDD, 5), 'iyh': (0xFD, 4), 'iyl': (0xFD, 5)}
R16 = {'bc': 0, 'de': 1, 'hl': 2, 'sp': 3}
COND = {'nz': 0, 'z': 1, 'nc': 2, 'c': 3, 'po': 4, 'pe': 5, 'p': 6, 'm': 7}
ALU = {'add': 0, 'adc': 1, 'sub': 2, 'sbc': 3, 'and': 4, 'xor': 5, 'or': 6, 'cp': 7}
ROT = {'rlc': 0, 'rrc': 1, 'rl': 2, 'rr': 3, 'sla': 4, 'sra': 5, 'sll': 6, 'sl1': 6, 'srl': 7}
BITS = {'bit': 0x40, 'res': 0x80, 'set': 0xC0}
SOLAS = {
    'nop': [0x00], 'halt': [0x76], 'di': [0xF3], 'ei': [0xFB], 'exx': [0xD9], 'daa': [0x27],
    'cpl': [0x2F], 'scf': [0x37], 'ccf': [0x3F], 'rlca': [0x07], 'rrca': [0x0F], 'rla': [0x17],
    'rra': [0x1F], 'neg': [0xED, 0x44], 'retn': [0xED, 0x45], 'reti': [0xED, 0x4D],
    'rld': [0xED, 0x6F], 'rrd': [0xED, 0x67], 'ldi': [0xED, 0xA0], 'cpi': [0xED, 0xA1],
    'ini': [0xED, 0xA2], 'outi': [0xED, 0xA3], 'ldd': [0xED, 0xA8], 'cpd': [0xED, 0xA9],
    'ind': [0xED, 0xAA], 'outd': [0xED, 0xAB], 'ldir': [0xED, 0xB0], 'cpir': [0xED, 0xB1],
    'inir': [0xED, 0xB2], 'otir': [0xED, 0xB3], 'lddr': [0xED, 0xB8], 'cpdr': [0xED, 0xB9],
    'indr': [0xED, 0xBA], 'otdr': [0xED, 0xBB],
}
OTRAS = set(SOLAS) | set(ALU) | set(ROT) | set(BITS) | {
    'ld', 'push', 'pop', 'ex', 'inc', 'dec', 'jp', 'jr', 'djnz', 'call', 'ret', 'rst', 'in', 'out', 'im'}


def entre_parentesis(t):
    """¿t es «( ... )» con los dos paréntesis emparejados entre sí?"""
    if len(t) < 2 or t[0] != '(' or t[-1] != ')':
        return False
    nivel = 0
    for i, c in enumerate(t):
        nivel += (c == '(') - (c == ')')
        if nivel == 0 and i < len(t) - 1:
            return False
    return True


class Asm:
    def __init__(self, defs):
        self.sim = dict(defs)
        self.pasada = 0
        self.cabeceras = []

    # ---- expresiones ----
    def fichas(self, texto):
        r, i, n = [], 0, len(texto)
        while i < n:
            c = texto[i]
            if c in ' \t':
                i += 1
                continue
            valor = r and (r[-1][0] in "0123456789'$_@." or r[-1][0].isalpha() or r[-1] == ')')
            m = None
            if c == '$':
                m = re.compile(r'\$[0-9A-Fa-f]+|\$').match(texto, i)
            elif c == '%' and not valor:
                m = re.compile(r'%[01]+').match(texto, i)
            elif c.isdigit():
                m = re.compile(r'[0-9][0-9A-Fa-f]*[hH](?![A-Za-z0-9_])|0[xX][0-9A-Fa-f]+|0[bB][01]+(?![0-9A-Za-z_])|[0-9]+').match(texto, i)
            elif c == "'":
                m = re.compile(r"'.'").match(texto, i)
            elif c.isalpha() or c in '_@.':
                m = re.compile(r'[@.]?[A-Za-z_][A-Za-z_0-9]*').match(texto, i)
            else:
                m = re.compile(r'<<|>>|[-+*/%&|^~()<>]').match(texto, i)
            if not m:
                raise Error('no entiendo la expresión «%s»' % texto)
            r.append(m.group(0))
            i = m.end()
        return r

    def expr(self, texto):
        self.t = self.fichas(texto)
        if not self.t:
            raise Error('falta un valor')
        self.i = 0
        self.indef = False
        v = self.e_or()
        if self.i != len(self.t):
            raise Error('sobra algo en «%s»' % texto)
        return v

    def mira(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def e_or(self):
        v = self.e_and()
        while self.mira() in ('|', '^'):
            o = self.t[self.i]
            self.i += 1
            w = self.e_and()
            v = v | w if o == '|' else v ^ w
        return v

    def e_and(self):
        v = self.e_desp()
        while self.mira() == '&':
            self.i += 1
            v &= self.e_desp()
        return v

    def e_desp(self):
        v = self.e_suma()
        while self.mira() in ('<<', '>>'):
            o = self.t[self.i]
            self.i += 1
            w = self.e_suma()
            v = v << w if o == '<<' else v >> w
        return v

    def e_suma(self):
        v = self.e_mul()
        while self.mira() in ('+', '-'):
            o = self.t[self.i]
            self.i += 1
            w = self.e_mul()
            v = v + w if o == '+' else v - w
        return v

    def e_mul(self):
        v = self.e_uno()
        while self.mira() in ('*', '/', '%'):
            o = self.t[self.i]
            self.i += 1
            w = self.e_uno()
            if o == '*':
                v = v * w
            elif o == '/':
                v = int(v / w) if w else 0
            else:
                v = v % w if w else 0
        return v

    def e_uno(self):
        t = self.mira()
        if t is None:
            raise Error('falta un valor')
        self.i += 1
        if t == '-':
            return -self.e_uno()
        if t == '+':
            return self.e_uno()
        if t == '~':
            return ~self.e_uno()
        if t == '<':
            return self.e_uno() & 0xFF
        if t == '>':
            return (self.e_uno() >> 8) & 0xFF
        if t == '(':
            v = self.e_or()
            if self.mira() != ')':
                raise Error('falta «)»')
            self.i += 1
            return v
        if t == '$':
            return self.pc
        if t[0] == '$':
            return int(t[1:], 16)
        if t[0] == '%':
            return int(t[1:], 2)
        if t[0].isdigit():
            tl = t.lower()
            if tl.endswith('h'):
                return int(t[:-1], 16)
            if tl.startswith('0x'):
                return int(t[2:], 16)
            if tl.startswith('0b'):
                return int(t[2:], 2)
            return int(t)
        if t[0] == "'":
            return ord(t[1])
        if t in (')', '*', '/', '%', '&', '|', '^', '<<', '>>'):
            raise Error('falta un valor antes de «%s»' % t)
        n = self.nombre(t)
        if n in self.sim:
            return self.sim[n]
        if self.pasada == 2:
            raise Error('no existe «%s»' % t)
        self.indef = True
        return 0

    def nombre(self, t):
        return self.ambito + t if t[0] == '@' else t

    # ---- pasadas ----
    def lineas(self, ruta):
        try:
            texto = open(ruta, encoding='utf-8').read().split('\n')
        except OSError as e:
            raise Error('no se puede leer %s: %s' % (ruta, e))
        for n, l in enumerate(texto, 1):
            m = re.match(r'\s*\.include\s+"([^"]+)"', l)
            if m:
                yield from self.lineas(os.path.join(os.path.dirname(ruta), m.group(1)))
            else:
                yield ruta, n, l

    def pasa(self, fuente, n):
        self.pasada = n
        self.pc = 0
        self.sal = bytearray()
        self.ambito = ''
        self.lst = []
        self.cabeceras = []
        tiles = None
        try:
            todas = list(self.lineas(fuente))
        except Error as e:
            sys.exit('%s: %s' % (fuente, e))
        for ruta, num, linea in todas:
            self.donde = '%s:%d' % (ruta, num)
            self.dir_base = os.path.dirname(ruta)
            ini = len(self.sal)
            pc0 = self.pc
            try:
                l = self.sin_comentario(linea).strip()
                if tiles is not None:
                    if l.lower() == '.endtiles':
                        if tiles:
                            raise Error('un dibujo a medias (%d filas)' % len(tiles))
                        tiles = None
                    elif l:
                        f = l.strip('"').lower()
                        if len(f) != 8 or any(c not in '.0123456789abcdef' for c in f):
                            raise Error('una fila de dibujo son 8 caracteres de «.», 0-9 y a-f')
                        tiles.append([0 if c == '.' else int(c, 16) for c in f])
                        if len(tiles) == 8:
                            for fila in tiles:
                                for plano in range(4):
                                    self.pon(sum(((p >> plano) & 1) << (7 - x) for x, p in enumerate(fila)))
                            tiles = []
                elif l.lower() == '.tiles':
                    tiles = []
                elif l:
                    self.linea(l)
            except Error as e:
                sys.exit('%s: %s\n    %s' % (self.donde, e, linea.strip()))
            if n == 2:
                self.lst.append('%04X  %-24s %s' % (pc0 & 0xFFFF, self.sal[ini:ini + 8].hex(' ').upper(), linea))
        if tiles is not None:
            sys.exit('%s: falta .endtiles' % fuente)

    @staticmethod
    def sin_comentario(l):
        r, comillas, i = [], None, 0
        while i < len(l):
            c = l[i]
            if comillas:
                if c == '\\' and comillas == '"' and i + 1 < len(l):
                    r.append(c)
                    i += 1
                    c = l[i]
                elif c == comillas:
                    comillas = None
            elif c == "'" and ''.join(r[-2:]).lower() == 'af':
                pass                        # af'
            elif c in '"\'':
                comillas = c
            elif c == ';':
                break
            r.append(c)
            i += 1
        return ''.join(r)

    def pon(self, v):
        self.sal.append(v & 0xFF)
        self.pc += 1

    def define(self, n, v):
        if self.pasada == 1 and n in self.sim:
            raise Error('«%s» está dos veces' % n)
        self.sim[n] = v

    def linea(self, l):
        m = re.match(r'([@.]?[A-Za-z_][A-Za-z_0-9]*)(?:\s*=|\s+[eE][qQ][uU]\s)\s*(.+)$', l)
        if m and not m.group(1).startswith('.'):
            v = self.expr(m.group(2))
            if self.pasada == 1 and self.indef:
                raise Error('una constante necesita valores ya definidos')
            self.define(self.nombre(m.group(1)), v)
            return
        m = re.match(r'(@?[A-Za-z_][A-Za-z_0-9]*):\s*(.*)$', l)
        if m:
            n = m.group(1)
            if n[0] != '@':
                self.ambito = n
                self.define(n, self.pc)
            else:
                self.define(self.ambito + n, self.pc)
            l = m.group(2)
            if not l:
                return
        m = re.match(r'(\.?[A-Za-z_][A-Za-z_0-9]*)\s*(.*)$', l)
        if not m:
            raise Error('no entiendo la línea')
        op, arg = m.group(1).lower(), m.group(2).strip()
        if op[0] == '.':
            self.directiva(op, arg)
        elif op in OTRAS:
            for b in self.instruccion(op, self.args(arg)):
                self.pon(b)
        else:
            raise Error('no conozco «%s»' % op)

    def args(self, arg):
        r, act, comillas, nivel, i = [], '', None, 0, 0
        while i < len(arg):
            c = arg[i]
            if comillas:
                act += c
                if c == '\\' and comillas == '"' and i + 1 < len(arg):
                    i += 1
                    act += arg[i]
                elif c == comillas:
                    comillas = None
            elif c == "'" and act.strip().lower() == 'af':
                act += c
            elif c in '"\'':
                comillas = c
                act += c
            elif c == ',' and nivel == 0:
                r.append(act.strip())
                act = ''
            else:
                nivel += (c == '(') - (c == ')')
                act += c
            i += 1
        if act.strip():
            r.append(act.strip())
        elif r:
            raise Error('falta un operando detrás de la coma')
        return r

    @staticmethod
    def texto(x):
        r, i = [], 1
        while i < len(x) - 1:
            c = x[i]
            if c == '\\':
                i += 1
                c = {'n': '\n', 'r': '\r', 't': '\t', '0': '\0'}.get(x[i], x[i])
            r.append(ord(c) if ord(c) < 256 else ord('?'))
            i += 1
        return r

    def directiva(self, op, arg):
        a = self.args(arg)
        if op == '.org':
            self.pc = self.expr(a[0])
        elif op == '.pad':
            hasta = self.expr(a[0])
            v = self.expr(a[1]) if len(a) > 1 else 0xFF
            if hasta < self.pc:
                raise Error('.pad $%04X: ya se va por $%04X (sobran %d octetos)' % (hasta, self.pc, self.pc - hasta))
            while self.pc < hasta:
                self.pon(v)
        elif op == '.align':
            n = self.expr(a[0])
            v = self.expr(a[1]) if len(a) > 1 else 0xFF
            if n <= 0:
                raise Error('.align necesita un número mayor que 0')
            while self.pc % n:
                self.pon(v)
        elif op in ('.byte', '.db'):
            for x in a:
                if len(x) >= 2 and x[0] == '"' and x[-1] == '"':
                    for c in self.texto(x):
                        self.pon(c)
                else:
                    self.pon(self.n8(x))
        elif op in ('.word', '.dw'):
            for x in a:
                v = self.expr(x)
                if self.pasada == 2 and not -32768 <= v <= 65535:
                    raise Error('%d no cabe en una palabra' % v)
                self.pon(v)
                self.pon(v >> 8)
        elif op in ('.res', '.ds'):
            v = self.expr(a[1]) if len(a) > 1 else 0
            n = self.expr(a[0])
            if self.indef:
                raise Error('.res necesita un número ya conocido')
            for _ in range(n):
                self.pon(v)
        elif op == '.incbin':
            try:
                datos = open(os.path.join(self.dir_base, a[0].strip('"')), 'rb').read()
            except OSError as e:
                raise Error('no se puede leer %s: %s' % (a[0], e))
            for b in datos:
                self.pon(b)
        elif op == '.cabecera_sega':
            if len(a) != 3:
                raise Error('.cabecera_sega CODIGO, VERSION, REGION_TAM')
            cod, ver, reg = (self.expr(x) for x in a)
            if not 0 <= cod <= 99999 or not 0 <= ver <= 15 or not 0 <= reg <= 255:
                raise Error('código de 0 a 99999, versión de 0 a 15 y región de 0 a 255')
            d = '%05d' % cod
            self.cabeceras.append(len(self.sal))
            for c in b'TMR SEGA\0\0':
                self.pon(c)
            self.pon(0)                     # la suma, al final
            self.pon(0)
            self.pon(int(d[3:5], 16))
            self.pon(int(d[1:3], 16))
            self.pon((int(d[0]) << 4) | ver)
            self.pon(reg)
        else:
            raise Error('no conozco la directiva «%s»' % op)

    def remata(self):
        for p in self.cabeceras:
            s = sum(self.sal[:p]) & 0xFFFF
            self.sal[p + 10] = s & 0xFF
            self.sal[p + 11] = s >> 8

    # ---- operandos ----
    def n8(self, x):
        v = self.expr(x)
        if self.pasada == 2 and not -128 <= v <= 255:
            raise Error('%d no cabe en un octeto' % v)
        return v & 0xFF

    def n16(self, x):
        v = self.expr(x)
        if self.pasada == 2 and not -32768 <= v <= 65535:
            raise Error('%d no cabe en una palabra' % v)
        return [v & 0xFF, (v >> 8) & 0xFF]

    def operando(self, x):
        """('r', n) registro de 8 bits (6 = (hl)); ('xh', prefijo, n); ('x',
        prefijo, desplazamiento) para (ix+d); ('rr', nombre) bc de hl sp af
        ix iy; ('ind', nombre) (bc) (de) (sp) (c); ('mem', texto) (nn);
        ('esp', nombre) i r af'; ('n', texto) un valor"""
        l = x.lower().replace(' ', '').replace('\t', '')
        if l in R8:
            return ('r', R8[l])
        if l == '(hl)':
            return ('r', 6)
        if l in XH:
            return ('xh',) + XH[l]
        if l in ('bc', 'de', 'hl', 'sp', 'af', 'ix', 'iy'):
            return ('rr', l)
        if l in ('i', 'r', "af'"):
            return ('esp', l)
        if l in ('(bc)', '(de)', '(sp)', '(c)'):
            return ('ind', l[1:-1])
        if entre_parentesis(x.strip()):
            dentro = x.strip()[1:-1].strip()
            dl = dentro.lower()
            if dl[:2] in ('ix', 'iy') and (len(dl) == 2 or dl[2:].lstrip()[:1] in ('+', '-')):
                pref = 0xDD if dl[1] == 'x' else 0xFD
                resto = dentro[2:].strip()
                d = self.expr(resto) if resto else 0
                if self.pasada == 2 and not -128 <= d <= 127:
                    raise Error('el desplazamiento %d no cabe (de -128 a 127)' % d)
                return ('x', pref, d & 0xFF)
            return ('mem', dentro)
        return ('n', x)

    def instruccion(self, op, a):
        try:
            r = self.codifica(op, a, [self.operando(x) for x in a])
        except (IndexError, KeyError, TypeError, ValueError):
            r = None
        if r is None:
            raise Error('«%s %s» no es una instrucción del Z80' % (op, ', '.join(a)))
        return r

    def reg8(self, o):
        """r, ixh..., (ix+d) -> (prefijo, n, desplazamiento) o None"""
        if o[0] == 'r':
            return (None, o[1], None)
        if o[0] == 'xh':
            return (o[1], o[2], None)
        if o[0] == 'x':
            return (o[1], 6, o[2])
        return None

    @staticmethod
    def con(pref, *b, d=None):
        r = [pref] if pref else []
        r.append(b[0])
        if d is not None:
            r.append(d)
        return r + list(b[1:])

    def codifica(self, op, a, o):
        n = len(o)
        if op in SOLAS:
            return list(SOLAS[op]) if n == 0 else None
        if op == 'ld':
            return self.c_ld(a, o)
        if op in ALU:
            if n == 2 and o[0][0] == 'rr':
                return self.c_alu16(op, o)
            if n == 2:
                if o[0] != ('r', 7):
                    return None
                a, o = a[1:], o[1:]
            if len(o) != 1:
                return None
            k = ALU[op]
            g = self.reg8(o[0])
            if g:
                return self.con(g[0], 0x80 + k * 8 + g[1], d=g[2])
            if o[0][0] == 'n':
                return [0xC6 + k * 8, self.n8(a[0])]
            return None
        if op in ('inc', 'dec'):
            if n != 1:
                return None
            dec = op == 'dec'
            g = self.reg8(o[0])
            if g:
                return self.con(g[0], 0x04 + g[1] * 8 + dec, d=g[2])
            if o[0][0] == 'rr' and o[0][1] != 'af':
                p, k = self.rr(o[0][1])
                return self.con(p, 0x03 + k * 16 + dec * 8)
            return None
        if op in ROT or op in BITS:
            if op in ROT:
                k, resto, base = ROT[op] * 8, o, 0
            else:
                b = self.expr(a[0])
                if not 0 <= b <= 7:
                    raise Error('el bit va de 0 a 7')
                k, resto, base = BITS[op] + b * 8, o[1:], 0
            if len(resto) == 1:
                g = self.reg8(resto[0])
                if not g:
                    return None
                if g[0] and g[2] is None:
                    return None             # ixh no vale en CB
                if g[0]:
                    return [g[0], 0xCB, g[2], k + 6]
                return [0xCB, k + g[1]]
            if len(resto) == 2 and op != 'bit' and resto[0][0] == 'x' and resto[1][0] == 'r' and resto[1][1] != 6:
                return [resto[0][1], 0xCB, resto[0][2], k + resto[1][1]]
            return None
        if op in ('push', 'pop'):
            if n != 1 or o[0][0] != 'rr' or o[0][1] == 'sp':
                return None
            b = 0xC5 if op == 'push' else 0xC1
            if o[0][1] == 'af':
                return [b + 0x30]
            p, k = self.rr(o[0][1])
            return self.con(p, b + k * 16)
        if op == 'ex':
            t = tuple(x.lower().replace(' ', '') for x in a)
            if t == ('de', 'hl'):
                return [0xEB]
            if t == ('af', "af'"):
                return [0x08]
            if t[0] == '(sp)' and n == 2 and o[1][0] == 'rr' and o[1][1] in ('hl', 'ix', 'iy'):
                p, _ = self.rr(o[1][1])
                return self.con(p, 0xE3)
            return None
        if op == 'jp':
            if n == 1 and (o[0] in (('r', 6),) or (o[0][0] == 'x' and o[0][2] == 0 and '+' not in a[0] and '-' not in a[0])):
                return [0xE9] if o[0][0] == 'r' else [o[0][1], 0xE9]
            if n == 1 and o[0][0] == 'rr' and o[0][1] in ('hl', 'ix', 'iy'):
                p, _ = self.rr(o[0][1])
                return self.con(p, 0xE9)
            if n == 1 and o[0][0] == 'n':
                return [0xC3] + self.n16(a[0])
            if n == 2 and o[1][0] == 'n':
                return [0xC2 + self.cond(a[0]) * 8] + self.n16(a[1])
            return None
        if op == 'call':
            if n == 1 and o[0][0] == 'n':
                return [0xCD] + self.n16(a[0])
            if n == 2 and o[1][0] == 'n':
                return [0xC4 + self.cond(a[0]) * 8] + self.n16(a[1])
            return None
        if op == 'ret':
            if n == 0:
                return [0xC9]
            if n == 1:
                return [0xC0 + self.cond(a[0]) * 8]
            return None
        if op in ('jr', 'djnz'):
            if op == 'djnz':
                if n != 1:
                    return None
                b, dest = 0x10, a[0]
            elif n == 1:
                b, dest = 0x18, a[0]
            elif n == 2:
                c = self.cond(a[0])
                if c > 3:
                    raise Error('jr solo admite nz, z, nc y c')
                b, dest = 0x20 + c * 8, a[1]
            else:
                return None
            d = self.expr(dest) - (self.pc + 2)
            if self.pasada == 2 and not -128 <= d <= 127:
                raise Error('el salto no llega (%d octetos; de -128 a 127)' % d)
            return [b, d & 0xFF]
        if op == 'rst':
            v = self.expr(a[0]) if n == 1 else -1
            if v not in range(0, 0x40, 8):
                raise Error('rst va a 0, 8, 10h, ..., 38h')
            return [0xC7 + v]
        if op == 'im':
            v = self.expr(a[0]) if n == 1 else -1
            if v not in (0, 1, 2):
                raise Error('im 0, 1 o 2')
            return [0xED, (0x46, 0x56, 0x5E)[v]]
        if op == 'in':
            if n == 1 and o[0] == ('ind', 'c'):
                return [0xED, 0x70]
            if n == 2 and o[1] == ('ind', 'c'):
                if a[0].lower().strip() == 'f':
                    return [0xED, 0x70]
                if o[0][0] == 'r' and o[0][1] != 6:
                    return [0xED, 0x40 + o[0][1] * 8]
                return None
            if n == 2 and o[0] == ('r', 7) and o[1][0] == 'mem':
                return [0xDB, self.n8(o[1][1])]
            return None
        if op == 'out':
            if n == 2 and o[0] == ('ind', 'c'):
                if o[1][0] == 'r' and o[1][1] != 6:
                    return [0xED, 0x41 + o[1][1] * 8]
                if o[1][0] == 'n' and self.expr(a[1]) == 0:
                    return [0xED, 0x71]
                return None
            if n == 2 and o[0][0] == 'mem' and o[1] == ('r', 7):
                return [0xD3, self.n8(o[0][1])]
            return None
        return None

    def cond(self, x):
        return COND[x.lower().strip()]

    @staticmethod
    def rr(nombre):
        """bc de hl sp ix iy -> (prefijo, número)"""
        if nombre == 'ix':
            return 0xDD, 2
        if nombre == 'iy':
            return 0xFD, 2
        return None, R16[nombre]

    def c_alu16(self, op, o):
        d, s = o[0][1], o[1]
        if s[0] != 'rr' or s[1] == 'af':
            return None
        if op in ('adc', 'sbc') and d == 'hl' and s[1] in R16:
            return [0xED, (0x4A if op == 'adc' else 0x42) + R16[s[1]] * 16]
        if op == 'add' and d in ('hl', 'ix', 'iy'):
            if s[1] in ('ix', 'iy', 'hl'):
                if s[1] != d:
                    return None
                k = 2
            else:
                k = R16[s[1]]
            p, _ = self.rr(d)
            return self.con(p, 0x09 + k * 16)
        return None

    def c_ld(self, a, o):
        if len(o) != 2:
            return None
        d, s = o
        gd, gs = self.reg8(d), self.reg8(s)
        # entre registros de 8 bits (y con ixh, (ix+d))
        if gd and gs:
            if d == ('r', 6) and s == ('r', 6):
                return None
            if d[0] == 'x' and s[0] == 'x':
                return None
            if d[0] == 'x' or s[0] == 'x':
                if d[0] == 'xh' or s[0] == 'xh':
                    return None
                x, r = (d, s) if d[0] == 'x' else (s, d)
                if r[1] == 6:
                    return None
                if d[0] == 'x':
                    return [x[1], 0x70 + r[1], x[2]]
                return [x[1], 0x46 + r[1] * 8, x[2]]
            if d[0] == 'xh' or s[0] == 'xh':
                pref = d[1] if d[0] == 'xh' else s[1]
                for g in (d, s):
                    if g[0] == 'xh' and g[1] != pref:
                        return None
                    if g[0] == 'r' and g[1] in (4, 5, 6):
                        return None
                return [pref, 0x40 + gd[1] * 8 + gs[1]]
            return [0x40 + gd[1] * 8 + gs[1]]
        if gd and s[0] == 'n':
            return self.con(gd[0], 0x06 + gd[1] * 8, d=gd[2]) + [self.n8(a[1])]
        if d == ('r', 7):
            if s == ('ind', 'bc'):
                return [0x0A]
            if s == ('ind', 'de'):
                return [0x1A]
            if s[0] == 'mem':
                return [0x3A] + self.n16(s[1])
            if s == ('esp', 'i'):
                return [0xED, 0x57]
            if s == ('esp', 'r'):
                return [0xED, 0x5F]
        if s == ('r', 7):
            if d == ('ind', 'bc'):
                return [0x02]
            if d == ('ind', 'de'):
                return [0x12]
            if d[0] == 'mem':
                return [0x32] + self.n16(d[1])
            if d == ('esp', 'i'):
                return [0xED, 0x47]
            if d == ('esp', 'r'):
                return [0xED, 0x4F]
        if d[0] == 'rr' and d[1] != 'af':
            p, k = self.rr(d[1])
            if s[0] == 'n':
                return self.con(p, 0x01 + k * 16) + self.n16(a[1])
            if s[0] == 'mem':
                if k == 2:
                    return self.con(p, 0x2A) + self.n16(s[1])
                return [0xED, 0x4B + k * 16] + self.n16(s[1])
            if d[1] == 'sp' and s[0] == 'rr' and s[1] in ('hl', 'ix', 'iy'):
                p, _ = self.rr(s[1])
                return self.con(p, 0xF9)
            return None
        if d[0] == 'mem' and s[0] == 'rr' and s[1] != 'af':
            p, k = self.rr(s[1])
            if k == 2:
                return self.con(p, 0x22) + self.n16(d[1])
            return [0xED, 0x43 + k * 16] + self.n16(d[1])
        return None


def main():
    a = sys.argv[1:]
    defs, lst, resto = {}, None, []
    while a:
        x = a.pop(0)
        if x == '-D':
            n, v = a.pop(0).split('=')
            defs[n] = int(v, 0)
        elif x == '-l':
            lst = a.pop(0)
        else:
            resto.append(x)
    if len(resto) != 2:
        sys.exit(__doc__)
    asm = Asm(defs)
    asm.pasa(resto[0], 1)
    asm.pasa(resto[0], 2)
    asm.remata()
    with open(resto[1], 'wb') as f:
        f.write(asm.sal)
    if lst:
        with open(lst, 'w', encoding='utf-8') as f:
            f.write('\n'.join(asm.lst) + '\n')
    print('%s: %d octetos' % (resto[1], len(asm.sal)))


if __name__ == '__main__':
    main()
