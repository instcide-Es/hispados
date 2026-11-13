#!/usr/bin/env python3
"""Ensamblador de 6502 pequeño, para los juegos propios de Hispa-Nes.

uso: asm6502.py FUENTE.s SALIDA.nes [-l LISTADO] [-D NOMBRE=valor]...

Hecho para el proyecto (GPL-2.0-or-later), sin dependencias: con él se
ensamblan en la compilación HISPA1.NES y las ROM de prueba del emulador. El
archivo de salida es plano: los octetos salen en el orden en que se escriben
(cabecera iNES, programa, vectores y dibujos).

Sintaxis (una instrucción por línea; «;» empieza un comentario):
  etiqueta:           etiqueta global          @local:   local (vale hasta la
                                                         siguiente global)
  NOMBRE = expresión  constante
  lda #$10            inmediato                lda $10      página cero
  lda $1234           absoluto                 lda $10,x    indexados
  lda ($10,x)  lda ($10),y  jmp ($1234)        asl a  /  asl
  .org DIR            dirección del código que sigue (no mueve la salida)
  .pad DIR [,valor]   rellena hasta esa dirección
  .byte e,"texto",... octetos    .word e,...  palabras (bajo, alto)
  .res N [,valor]     N octetos iguales
  .incbin "archivo"   mete un archivo tal cual
  .include "archivo"
  .tiles ... .endtiles   dibujos de 8x8 en texto: 8 líneas de 8 caracteres
                      por dibujo («.» o 0, 1, 2, 3 = color); cada uno son 16
                      octetos (los dos planos de la consola)
Expresiones: $hex %binario decimal 'c' etiquetas + - * / & | ^ << >> ( ),
y delante < (octeto bajo) y > (octeto alto). Una dirección menor que 256 que
ya se conoce usa página cero; «a:» delante obliga a absoluto.
Small two-pass 6502 assembler (flat output), written for this project.
"""
import os, re, sys

IMP, ACC, IMM, ZP, ZPX, ZPY, ABS, ABSX, ABSY, IND, INDX, INDY, REL = range(13)
TAM = {IMP: 1, ACC: 1, IMM: 2, ZP: 2, ZPX: 2, ZPY: 2, ABS: 3, ABSX: 3, ABSY: 3, IND: 3, INDX: 2, INDY: 2, REL: 2}

OPS = {}


def _g(nombre, base, modos):
    """grupo «normal»: los códigos salen de sumar al de base"""
    suma = {INDX: 0x00, ZP: 0x04, IMM: 0x08, ABS: 0x0C, INDY: 0x10, ZPX: 0x14, ABSY: 0x18, ABSX: 0x1C}
    for m in modos:
        OPS.setdefault(nombre, {})[m] = base + suma[m]


for n, b in (('ora', 0x01), ('and', 0x21), ('eor', 0x41), ('adc', 0x61), ('lda', 0xA1), ('cmp', 0xC1), ('sbc', 0xE1)):
    _g(n, b, (INDX, ZP, IMM, ABS, INDY, ZPX, ABSY, ABSX))
_g('sta', 0x81, (INDX, ZP, ABS, INDY, ZPX, ABSY, ABSX))
for n, b in (('asl', 0x06), ('rol', 0x26), ('lsr', 0x46), ('ror', 0x66)):
    OPS[n] = {ZP: b, ACC: b + 4, IMP: b + 4, ABS: b + 8, ZPX: b + 0x10, ABSX: b + 0x18}
for n, b in (('dec', 0xC6), ('inc', 0xE6)):
    OPS[n] = {ZP: b, ABS: b + 8, ZPX: b + 0x10, ABSX: b + 0x18}
OPS.update({
    'bit': {ZP: 0x24, ABS: 0x2C},
    'jmp': {ABS: 0x4C, IND: 0x6C},
    'jsr': {ABS: 0x20},
    'stx': {ZP: 0x86, ABS: 0x8E, ZPY: 0x96},
    'sty': {ZP: 0x84, ABS: 0x8C, ZPX: 0x94},
    'ldx': {IMM: 0xA2, ZP: 0xA6, ABS: 0xAE, ZPY: 0xB6, ABSY: 0xBE},
    'ldy': {IMM: 0xA0, ZP: 0xA4, ABS: 0xAC, ZPX: 0xB4, ABSX: 0xBC},
    'cpx': {IMM: 0xE0, ZP: 0xE4, ABS: 0xEC},
    'cpy': {IMM: 0xC0, ZP: 0xC4, ABS: 0xCC},
})
for n, c in (('bpl', 0x10), ('bmi', 0x30), ('bvc', 0x50), ('bvs', 0x70), ('bcc', 0x90), ('bcs', 0xB0),
             ('bne', 0xD0), ('beq', 0xF0)):
    OPS[n] = {REL: c}
for n, c in (('brk', 0x00), ('php', 0x08), ('clc', 0x18), ('plp', 0x28), ('sec', 0x38), ('rti', 0x40),
             ('pha', 0x48), ('cli', 0x58), ('rts', 0x60), ('pla', 0x68), ('sei', 0x78), ('dey', 0x88),
             ('txa', 0x8A), ('tya', 0x98), ('txs', 0x9A), ('tay', 0xA8), ('tax', 0xAA), ('clv', 0xB8),
             ('tsx', 0xBA), ('iny', 0xC8), ('dex', 0xCA), ('cld', 0xD8), ('inx', 0xE8), ('nop', 0xEA),
             ('sed', 0xF8)):
    OPS[n] = {IMP: c}


class Error(Exception):
    pass


class Asm:
    def __init__(self, defs):
        self.sim = dict(defs)
        self.pasada = 0

    # ---- expresiones ----
    def expr(self, texto):
        self.t = re.findall(r"\$[0-9A-Fa-f]+|%[01]+|\d+|'.'|[@.]?[A-Za-z_][A-Za-z_0-9]*|<<|>>|[-+*/&|^()<>]", texto)
        if ''.join(self.t).replace(' ', '') != texto.replace(' ', '').replace('\t', ''):
            raise Error('no entiendo la expresión «%s»' % texto)
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
        while self.mira() in ('*', '/'):
            o = self.t[self.i]
            self.i += 1
            w = self.e_uno()
            v = v * w if o == '*' else (v // w if w else 0)
        return v

    def e_uno(self):
        t = self.mira()
        if t is None:
            raise Error('falta un valor')
        self.i += 1
        if t == '-':
            return -self.e_uno()
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
        if t[0] == '$':
            return int(t[1:], 16)
        if t[0] == '%':
            return int(t[1:], 2)
        if t[0].isdigit():
            return int(t)
        if t[0] == "'":
            return ord(t[1])
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
        tiles = None
        for ruta, num, linea in self.lineas(fuente):
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
                        f = l.strip('"')
                        if len(f) != 8 or any(c not in '.0123' for c in f):
                            raise Error('una fila de dibujo son 8 caracteres de «.0123»')
                        tiles.append([0 if c == '.' else int(c) for c in f])
                        if len(tiles) == 8:
                            for plano in (0, 1):
                                for fila in tiles:
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
        r, comillas = [], None
        for c in l:
            if comillas:
                if c == comillas:
                    comillas = None
            elif c in '"\'':
                comillas = c
            elif c == ';':
                break
            r.append(c)
        return ''.join(r)

    def pon(self, v):
        self.sal.append(v & 0xFF)
        self.pc += 1

    def define(self, n, v):
        if self.pasada == 1 and n in self.sim:
            raise Error('«%s» está dos veces' % n)
        self.sim[n] = v

    def linea(self, l):
        m = re.match(r'([@.]?[A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.+)$', l)
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
        m = re.match(r'(\.?[A-Za-z]+)\s*(.*)$', l)
        if not m:
            raise Error('no entiendo la línea')
        op, arg = m.group(1).lower(), m.group(2).strip()
        if op[0] == '.':
            self.directiva(op, arg)
        elif op in OPS:
            self.instruccion(op, arg)
        else:
            raise Error('no conozco «%s»' % op)

    def args(self, arg):
        r, act, comillas, nivel = [], '', None, 0
        for c in arg:
            if comillas:
                act += c
                if c == comillas:
                    comillas = None
            elif c in '"\'':
                comillas = c
                act += c
            elif c == ',' and nivel == 0:
                r.append(act.strip())
                act = ''
            else:
                nivel += (c == '(') - (c == ')')
                act += c
        if act.strip():
            r.append(act.strip())
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
        elif op in ('.byte', '.db'):
            for x in a:
                if len(x) >= 2 and x[0] == '"' and x[-1] == '"':
                    for c in x[1:-1]:
                        self.pon(ord(c))
                else:
                    v = self.expr(x)
                    if self.pasada == 2 and not -128 <= v <= 255:
                        raise Error('%d no cabe en un octeto' % v)
                    self.pon(v)
        elif op in ('.word', '.dw'):
            for x in a:
                v = self.expr(x)
                self.pon(v)
                self.pon(v >> 8)
        elif op in ('.res', '.ds'):
            v = self.expr(a[1]) if len(a) > 1 else 0
            for _ in range(self.expr(a[0])):
                self.pon(v)
        elif op == '.incbin':
            for b in open(os.path.join(self.dir_base, a[0].strip('"')), 'rb').read():
                self.pon(b)
        else:
            raise Error('no conozco la directiva «%s»' % op)

    def instruccion(self, op, arg):
        modos = OPS[op]
        fuerza_abs = False
        if arg.lower().startswith('a:'):
            fuerza_abs, arg = True, arg[2:].strip()
        al = arg.lower().replace(' ', '')
        v = 0
        if REL in modos:
            dest = self.expr(arg)
            d = dest - (self.pc + 2)
            if self.pasada == 2 and not -128 <= d <= 127:
                raise Error('el salto no llega (%d octetos)' % d)
            self.pon(modos[REL])
            self.pon(d)
            return
        if al == '' or (al == 'a' and ACC in modos):
            modo = ACC if ACC in modos else IMP
        elif arg[0] == '#':
            modo, v = IMM, self.expr(arg[1:])
            if self.pasada == 2 and not -128 <= v <= 255:
                raise Error('%d no cabe en un octeto' % v)
        elif al.endswith(',x)') and al[0] == '(':
            modo, v = INDX, self.expr(arg[arg.index('(') + 1:arg.rindex(',')])
        elif al.endswith('),y') and al[0] == '(':
            modo, v = INDY, self.expr(arg[arg.index('(') + 1:arg.rindex(')')])
        elif al[0] == '(' and al[-1] == ')' and IND in modos:
            modo, v = IND, self.expr(arg[arg.index('(') + 1:arg.rindex(')')])
        else:
            ind = None
            if al.endswith(',x') or al.endswith(',y'):
                ind = al[-1]
                arg = arg[:arg.rindex(',')]
            v = self.expr(arg)
            corto = not fuerza_abs and (not self.indef or arg.lstrip()[0] == '<') and 0 <= v < 256
            if self.pasada == 2:        # el tamaño es el de la primera pasada
                corto = self.tam[len(self.sal)] == 2
            zp, ab = {None: (ZP, ABS), 'x': (ZPX, ABSX), 'y': (ZPY, ABSY)}[ind]
            modo = zp if corto and zp in modos else ab
            if modo not in modos and zp in modos and 0 <= v < 256:
                modo = zp
        if modo not in modos:
            raise Error('«%s» no tiene ese direccionamiento' % op)
        if self.pasada == 1:
            self.tam[len(self.sal)] = TAM[modo]
        if modo in (INDX, INDY, ZP, ZPX, ZPY) and self.pasada == 2 and not 0 <= v < 256:
            raise Error('$%X no está en la página cero' % v)
        self.pon(modos[modo])
        if TAM[modo] >= 2:
            self.pon(v)
        if TAM[modo] == 3:
            self.pon(v >> 8)


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
    asm.tam = {}
    asm.pasa(resto[0], 1)
    asm.pasa(resto[0], 2)
    with open(resto[1], 'wb') as f:
        f.write(asm.sal)
    if lst:
        with open(lst, 'w', encoding='utf-8') as f:
            f.write('\n'.join(asm.lst) + '\n')
    print('%s: %d octetos' % (resto[1], len(asm.sal)))


if __name__ == '__main__':
    main()
