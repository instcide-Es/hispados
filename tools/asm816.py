#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Ensamblador propio para la consola de 16 bits de 1990: el procesador
principal (65C816) y el del sonido (SPC700). Para las ROM de prueba de
Hispa-Snes y el arranque del procesador de sonido (docs/HISPASNES.md).

uso: asm816.py FUENTE SALIDA [--lista LISTADO]

Sintaxis (una instrucción por línea; «;» empieza un comentario):
  etiqueta:          @local: (vale hasta la siguiente etiqueta sin «@»)
  NOMBRE = expr      una constante
  .cpu 65816 | .cpu spc700
  .lorom | .hirom    cómo van las direcciones a la ROM (por omisión LoROM)
  .rom KB            el tamaño de la ROM (se rellena con FFh)
  .org dir           sigue en esa dirección (del procesador; con la ROM
                     según .lorom o .hirom)
  .base dir / .endbase   el código que sigue se ensambla como si estuviera
                     en «dir» (para copiarlo luego: el del SPC700), pero va
                     donde toca en la ROM
  .a8 .a16 .i8 .i16  el ancho del acumulador y de los índices (REP y SEP
                     con un número también lo cambian)
  .byte/.db  .word/.dw  .long/.dl  .ascii "txt"  .asciiz "txt"
  .res N [, relleno]   .align N   .incbin archivo   .include archivo
  .cabecera "TITULO", mapa, tipo, rom, sram   la cabecera del cartucho (en
                     FFC0h del banco 0 si es LoROM, o el equivalente) y su
                     suma, que se calcula al final
  Números: $1F, %1010, 0x1F, 31, 'c'. Expresiones de Python con + - * / % &
  | ^ << >> y lo(x), hi(x), bank(x). El tamaño de la dirección: «lda.b»
  (página directa), «lda.w» (absoluta), «lda.l» (larga); sin nada, un
  número de dos cifras hexadecimales o una constante menor de 100h es de
  página directa, uno de seis cifras o mayor de FFFFh es largo, y lo demás
  absoluto.
  SPC700, con la sintaxis de su fabricante: mov a,#$12  mov a,$12  mov a,!$1234
  mov a,$12+x  mov a,!$1234+y  mov a,(x)  mov a,(x)+  mov a,[$12+x]
  mov a,[$12]+y  mov $12,$34 (destino, origen)  set1 $12.3  bbs $12.3,etq
  and1 c,/$1234.5  tcall 3  pcall $20  jmp [!$1234+x]
"""
import os, re, sys

# ---------------------------------------------------------------------------
#  65C816: mnemónico -> {modo: código}
# ---------------------------------------------------------------------------
OPS816 = {}


def _def(mn, **modos):
    OPS816.setdefault(mn, {}).update(modos)


for mn, base in (('ora', 0x00), ('and', 0x20), ('eor', 0x40), ('adc', 0x60),
                 ('sta', 0x80), ('lda', 0xA0), ('cmp', 0xC0), ('sbc', 0xE0)):
    m = dict(dpxi=base + 1, sr=base + 3, dp=base + 5, dpil=base + 7, imm=base + 9,
             abs=base + 0xD, long=base + 0xF, dpiy=base + 0x11, dpi=base + 0x12,
             sriy=base + 0x13, dpx=base + 0x15, dpily=base + 0x17, absy=base + 0x19,
             absx=base + 0x1D, longx=base + 0x1F)
    if mn == 'sta':
        del m['imm']
    _def(mn, **m)
for mn, base in (('asl', 0x00), ('rol', 0x20), ('lsr', 0x40), ('ror', 0x60)):
    _def(mn, dp=base + 6, acc=base + 0xA, abs=base + 0xE, dpx=base + 0x16, absx=base + 0x1E)
_def('inc', dp=0xE6, acc=0x1A, abs=0xEE, dpx=0xF6, absx=0xFE)
_def('dec', dp=0xC6, acc=0x3A, abs=0xCE, dpx=0xD6, absx=0xDE)
_def('tsb', dp=0x04, abs=0x0C)
_def('trb', dp=0x14, abs=0x1C)
_def('bit', dp=0x24, abs=0x2C, dpx=0x34, absx=0x3C, imm=0x89)
_def('ldx', imm=0xA2, dp=0xA6, abs=0xAE, dpy=0xB6, absy=0xBE)
_def('ldy', imm=0xA0, dp=0xA4, abs=0xAC, dpx=0xB4, absx=0xBC)
_def('stx', dp=0x86, abs=0x8E, dpy=0x96)
_def('sty', dp=0x84, abs=0x8C, dpx=0x94)
_def('stz', dp=0x64, dpx=0x74, abs=0x9C, absx=0x9E)
_def('cpx', imm=0xE0, dp=0xE4, abs=0xEC)
_def('cpy', imm=0xC0, dp=0xC4, abs=0xCC)
_def('jmp', abs=0x4C, ind=0x6C, absxi=0x7C, long=0x5C, indl=0xDC)
_def('jml', long=0x5C, indl=0xDC)
_def('jsr', abs=0x20, absxi=0xFC, long=0x22)
_def('jsl', long=0x22)
_def('pei', dpi=0xD4)
_def('pea', abs=0xF4)
_def('per', rell=0x62)
_def('brl', rell=0x82)
_def('rep', imm8=0xC2)
_def('sep', imm8=0xE2)
_def('cop', imm8=0x02)
_def('wdm', imm8=0x42)
_def('brk', imm8=0x00, imp=0x00)
_def('mvn', mov=0x54)
_def('mvp', mov=0x44)
for mn, c in (('bpl', 0x10), ('bmi', 0x30), ('bvc', 0x50), ('bvs', 0x70), ('bra', 0x80),
              ('bcc', 0x90), ('bcs', 0xB0), ('bne', 0xD0), ('beq', 0xF0)):
    _def(mn, rel=c)
for mn, c in (('php', 0x08), ('phd', 0x0B), ('clc', 0x18), ('tcs', 0x1B), ('plp', 0x28),
              ('pld', 0x2B), ('sec', 0x38), ('tsc', 0x3B), ('rti', 0x40), ('pha', 0x48),
              ('phk', 0x4B), ('cli', 0x58), ('phy', 0x5A), ('tcd', 0x5B), ('rts', 0x60),
              ('pla', 0x68), ('rtl', 0x6B), ('sei', 0x78), ('ply', 0x7A), ('tdc', 0x7B),
              ('dey', 0x88), ('txa', 0x8A), ('phb', 0x8B), ('tya', 0x98), ('txs', 0x9A),
              ('txy', 0x9B), ('tay', 0xA8), ('tax', 0xAA), ('plb', 0xAB), ('clv', 0xB8),
              ('tsx', 0xBA), ('tyx', 0xBB), ('iny', 0xC8), ('dex', 0xCA), ('wai', 0xCB),
              ('cld', 0xD8), ('phx', 0xDA), ('stp', 0xDB), ('inx', 0xE8), ('nop', 0xEA),
              ('xba', 0xEB), ('sed', 0xF8), ('plx', 0xFA), ('xce', 0xFB)):
    _def(mn, imp=c)

LARGO816 = dict(imp=0, acc=0, dp=1, dpx=1, dpy=1, dpi=1, dpil=1, dpxi=1, dpiy=1, dpily=1,
                sr=1, sriy=1, abs=2, absx=2, absy=2, ind=2, absxi=2, indl=2, long=3,
                longx=3, rel=1, rell=2, imm8=1, mov=2)
ANCHO_A = ('ora', 'and', 'eor', 'adc', 'sta', 'lda', 'cmp', 'sbc', 'bit')
ANCHO_X = ('ldx', 'ldy', 'cpx', 'cpy')

# ---------------------------------------------------------------------------
#  SPC700: (mnemónico, (patrón de operandos...)) -> código, y cómo se escribe
# ---------------------------------------------------------------------------
OPSSPC = {}


def _s(mn, ops, c):
    OPSSPC[(mn, ops)] = c


for mn, b in (('or', 0x00), ('and', 0x20), ('eor', 0x40), ('cmp', 0x60), ('adc', 0x80), ('sbc', 0xA0)):
    _s(mn, ('A', 'd'), b + 4)
    _s(mn, ('A', '!a'), b + 5)
    _s(mn, ('A', '(X)'), b + 6)
    _s(mn, ('A', '[d+X]'), b + 7)
    _s(mn, ('A', '#'), b + 8)
    _s(mn, ('d', 'd'), b + 9)
    _s(mn, ('A', 'd+X'), b + 0x14)
    _s(mn, ('A', '!a+X'), b + 0x15)
    _s(mn, ('A', '!a+Y'), b + 0x16)
    _s(mn, ('A', '[d]+Y'), b + 0x17)
    _s(mn, ('d', '#'), b + 0x18)
    _s(mn, ('(X)', '(Y)'), b + 0x19)
for mn, b in (('asl', 0x00), ('rol', 0x20), ('lsr', 0x40), ('ror', 0x60)):
    _s(mn, ('d',), b + 0x0B)
    _s(mn, ('!a',), b + 0x0C)
    _s(mn, ('d+X',), b + 0x1B)
    _s(mn, ('A',), b + 0x1C)
for mn, b in (('dec', 0x80), ('inc', 0xA0)):
    _s(mn, ('d',), b + 0x0B)
    _s(mn, ('!a',), b + 0x0C)
    _s(mn, ('d+X',), b + 0x1B)
    _s(mn, ('A',), b + 0x1C)
_s('dec', ('X',), 0x1D)
_s('inc', ('X',), 0x3D)
_s('dec', ('Y',), 0xDC)
_s('inc', ('Y',), 0xFC)
_s('cmp', ('X', '!a'), 0x1E)
_s('cmp', ('X', 'd'), 0x3E)
_s('cmp', ('X', '#'), 0xC8)
_s('cmp', ('Y', '!a'), 0x5E)
_s('cmp', ('Y', 'd'), 0x7E)
_s('cmp', ('Y', '#'), 0xAD)
for mn, c in (('nop', 0x00), ('clrp', 0x20), ('setp', 0x40), ('clrc', 0x60), ('setc', 0x80),
              ('ei', 0xA0), ('di', 0xC0), ('clrv', 0xE0), ('ret', 0x6F), ('reti', 0x7F),
              ('notc', 0xED), ('sleep', 0xEF), ('stop', 0xFF), ('brk', 0x0F)):
    _s(mn, (), c)
_s('push', ('PSW',), 0x0D)
_s('push', ('A',), 0x2D)
_s('push', ('X',), 0x4D)
_s('push', ('Y',), 0x6D)
_s('pop', ('PSW',), 0x8E)
_s('pop', ('A',), 0xAE)
_s('pop', ('X',), 0xCE)
_s('pop', ('Y',), 0xEE)
_s('tset1', ('!a',), 0x0E)
_s('tclr1', ('!a',), 0x4E)
_s('decw', ('d',), 0x1A)
_s('incw', ('d',), 0x3A)
_s('cmpw', ('YA', 'd'), 0x5A)
_s('addw', ('YA', 'd'), 0x7A)
_s('subw', ('YA', 'd'), 0x9A)
_s('movw', ('YA', 'd'), 0xBA)
_s('movw', ('d', 'YA'), 0xDA)
_s('jmp', ('[!a+X]',), 0x1F)
_s('jmp', ('!a',), 0x5F)
_s('call', ('!a',), 0x3F)
_s('pcall', ('u',), 0x4F)
_s('cbne', ('d', 'r'), 0x2E)
_s('cbne', ('d+X', 'r'), 0xDE)
_s('dbnz', ('d', 'r'), 0x6E)
_s('dbnz', ('Y', 'r'), 0xFE)
for mn, c in (('bpl', 0x10), ('bra', 0x2F), ('bmi', 0x30), ('bvc', 0x50), ('bvs', 0x70),
              ('bcc', 0x90), ('bcs', 0xB0), ('bne', 0xD0), ('beq', 0xF0)):
    _s(mn, ('r',), c)
_s('or1', ('C', 'm.b'), 0x0A)
_s('or1', ('C', '/m.b'), 0x2A)
_s('and1', ('C', 'm.b'), 0x4A)
_s('and1', ('C', '/m.b'), 0x6A)
_s('eor1', ('C', 'm.b'), 0x8A)
_s('mov1', ('C', 'm.b'), 0xAA)
_s('mov1', ('m.b', 'C'), 0xCA)
_s('not1', ('m.b',), 0xEA)
_s('mul', ('YA',), 0xCF)
_s('div', ('YA', 'X'), 0x9E)
_s('xcn', ('A',), 0x9F)
_s('das', ('A',), 0xBE)
_s('daa', ('A',), 0xDF)
# MOV
for ops, c in ((('A', '#'), 0xE8), (('A', '(X)'), 0xE6), (('A', '(X)+'), 0xBF), (('A', 'd'), 0xE4),
               (('A', 'd+X'), 0xF4), (('A', '!a'), 0xE5), (('A', '!a+X'), 0xF5), (('A', '!a+Y'), 0xF6),
               (('A', '[d+X]'), 0xE7), (('A', '[d]+Y'), 0xF7), (('X', '#'), 0xCD), (('X', 'd'), 0xF8),
               (('X', 'd+Y'), 0xF9), (('X', '!a'), 0xE9), (('Y', '#'), 0x8D), (('Y', 'd'), 0xEB),
               (('Y', 'd+X'), 0xFB), (('Y', '!a'), 0xEC), (('A', 'X'), 0x7D), (('A', 'Y'), 0xDD),
               (('X', 'A'), 0x5D), (('Y', 'A'), 0xFD), (('X', 'SP'), 0x9D), (('SP', 'X'), 0xBD),
               (('(X)', 'A'), 0xC6), (('(X)+', 'A'), 0xAF), (('d', 'A'), 0xC4), (('d+X', 'A'), 0xD4),
               (('!a', 'A'), 0xC5), (('!a+X', 'A'), 0xD5), (('!a+Y', 'A'), 0xD6), (('[d+X]', 'A'), 0xC7),
               (('[d]+Y', 'A'), 0xD7), (('d', 'X'), 0xD8), (('d+Y', 'X'), 0xD9), (('!a', 'X'), 0xC9),
               (('d', 'Y'), 0xCB), (('d+X', 'Y'), 0xDB), (('!a', 'Y'), 0xCC), (('d', 'd'), 0xFA),
               (('d', '#'), 0x8F)):
    _s('mov', ops, c)


class Error(Exception):
    pass


class Asm:
    def __init__(self):
        self.simbolos = {}
        self.constantes = set()

    # ---- expresiones ----
    def valor(self, txt, linea, obligado=False):
        t = txt.strip()
        if not t:
            raise Error('falta un valor')

        def num(m):
            s = m.group(0)
            if s.startswith('$'):
                return str(int(s[1:], 16))
            if s.startswith('%'):
                return str(int(s[1:], 2))
            return s
        t = re.sub(r"'(.)'", lambda m: str(ord(m.group(1))), t)
        t = re.sub(r'\$[0-9A-Fa-f]+|%[01]+\b', num, t)

        def sim(m):
            n = m.group(0)
            if n in ('lo', 'hi', 'bank'):
                return n
            if re.match(r'0[xX][0-9a-fA-F]+$|\d+$', n):
                return n
            if n.startswith('@'):
                n = self.ambito + n
            if n in self.simbolos:
                return '(%d)' % self.simbolos[n]
            if obligado or self.pasada_final:
                raise Error('no se conoce «%s»' % m.group(0))
            self.desconocido = True
            return '0'
        t = re.sub(r'@?[A-Za-z_][A-Za-z_0-9.]*|0[xX][0-9a-fA-F]+|\d+', sim, t)
        try:
            v = eval(t, {'__builtins__': {}}, {'lo': lambda x: x & 0xFF, 'hi': lambda x: (x >> 8) & 0xFF,
                                               'bank': lambda x: (x >> 16) & 0xFF})
        except Exception:
            raise Error('expresión mala: %s' % txt)
        return int(v)

    # ---- la ROM ----
    def fisica(self, dir_):
        if self.base is not None:
            return self.fis
        if self.cpu == 'spc700':
            return dir_
        if self.mapa == 'hirom':
            return ((dir_ >> 16) & 0x3F) * 0x10000 + (dir_ & 0xFFFF)
        return ((dir_ >> 16) & 0x7F) * 0x8000 + ((dir_ & 0xFFFF) - 0x8000)

    def emite(self, bs):
        for b in bs:
            f = self.fisica(self.pc) if self.base is None else self.fis
            if self.pasada_final:
                if f < 0 or f >= len(self.rom):
                    raise Error('fuera de la ROM: %X' % self.pc)
                if self.lleno[f]:
                    raise Error('ya se había escrito en %X' % self.pc)
                self.rom[f] = b & 0xFF
                self.lleno[f] = 1
            self.pc += 1
            if self.base is not None:
                self.fis += 1
                self.pc &= 0xFFFFFF
            elif self.cpu == '65816' and (self.pc & 0xFFFF) == 0 and self.mapa == 'lorom':
                self.pc += 0x8000

    # ---- una pasada ----
    def tiles_emite(self, filas, bpp):
        for t in range(0, len(filas) - 7, 8):
            px = [[int(c, 16) if c != '.' else 0 for c in f] for f in filas[t:t + 8]]
            out = []
            for par in range(0, bpp, 2):
                for y in range(8):
                    for pl in (par, par + 1):
                        b = 0
                        for x in range(8):
                            if (px[y][x] >> pl) & 1:
                                b |= 0x80 >> x
                        out.append(b)
            self.emite(out)

    def pasada(self, lineas, final):
        self.pasada_final = final
        self.pc = 0x8000
        self.base = None
        self.fis = 0
        self.cpu = '65816'
        self.mapa = 'lorom'
        self.a8 = self.i8 = True
        self.ambito = ''
        self.cabecera = None
        self.cambios = 0
        if final:
            self.rom = bytearray(b'\xFF' * self.tam_rom)
            self.lleno = bytearray(self.tam_rom)
            self.listado = []
        self.tiles_bpp = 0
        filas = []
        for archivo, n, l in lineas:
            try:
                if self.tiles_bpp:
                    t = l.split(';')[0].strip()
                    if re.match(r'^[.0-9a-fA-F]{8}$', t):
                        filas.append(t)
                        continue
                    if t == '' :
                        continue
                    self.tiles_emite(filas, self.tiles_bpp)
                    filas = []
                    self.tiles_bpp = 0
                self.linea(l)
            except Error as e:
                sys.exit('%s:%d: %s' % (archivo, n, e))
        if self.tiles_bpp:
            self.tiles_emite(filas, self.tiles_bpp)

    def linea(self, l):
        l = re.sub(r";.*", "", l) if '"' not in l else self.sin_comentario(l)
        l = l.strip()
        if not l:
            return
        m = re.match(r'(@?[A-Za-z_][A-Za-z_0-9]*):\s*(.*)$', l)
        if m:
            nom = m.group(1)
            if not nom.startswith('@'):
                self.ambito = nom
            else:
                nom = self.ambito + nom
            self.define(nom, self.pc)
            l = m.group(2).strip()
            if not l:
                return
        m = re.match(r'([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.+)$', l)
        if m:
            v = self.valor(m.group(2), l)
            self.define(m.group(1), v)
            self.constantes.add(m.group(1))
            return
        if l.startswith('.'):
            return self.directiva(l)
        if self.cpu == 'spc700':
            return self.ins_spc(l)
        return self.ins_816(l)

    @staticmethod
    def sin_comentario(l):
        dentro = False
        for i, ch in enumerate(l):
            if ch == '"':
                dentro = not dentro
            elif ch == ';' and not dentro:
                return l[:i]
        return l

    def define(self, nom, v):
        if self.simbolos.get(nom) != v:
            self.cambios += 1
        self.simbolos[nom] = v

    def directiva(self, l):
        d, _, resto = l.partition(' ')
        d = d.lower()
        resto = resto.strip()
        if d == '.cpu':
            self.cpu = 'spc700' if '700' in resto else '65816'
        elif d in ('.lorom', '.hirom'):
            self.mapa = d[1:]
        elif d == '.rom':
            pass
        elif d == '.org':
            self.pc = self.valor(resto, l, True)
        elif d == '.base':
            self.fis = self.fisica(self.pc)
            self.pc_fuera = self.pc
            self.cpu_fuera = self.cpu
            self.base = True
            self.pc = self.valor(resto, l, True)
        elif d == '.endbase':
            n = self.fis - self.fisica_de(self.pc_fuera)
            self.base = None
            self.pc = self.pc_fuera
            self.cpu = '65816'
            for _ in range(n):
                self.avanza()
        elif d == '.a8':
            self.a8 = True
        elif d == '.a16':
            self.a8 = False
        elif d == '.i8':
            self.i8 = True
        elif d == '.i16':
            self.i8 = False
        elif d in ('.byte', '.db'):
            for p in self.partes(resto):
                if p.startswith('"'):
                    self.emite(p[1:-1].encode('latin-1'))
                else:
                    self.emite([self.valor(p, l)])
        elif d in ('.word', '.dw'):
            for p in self.partes(resto):
                v = self.valor(p, l)
                self.emite([v, v >> 8])
        elif d in ('.long', '.dl'):
            for p in self.partes(resto):
                v = self.valor(p, l)
                self.emite([v, v >> 8, v >> 16])
        elif d in ('.ascii', '.asciiz'):
            t = resto.strip()[1:-1].encode('latin-1')
            self.emite(t + (b'\0' if d == '.asciiz' else b''))
        elif d == '.res':
            ps = self.partes(resto)
            n = self.valor(ps[0], l, True)
            r = self.valor(ps[1], l) if len(ps) > 1 else 0
            self.emite([r] * n)
        elif d == '.align':
            n = self.valor(resto, l, True)
            while self.pc % n:
                self.emite([0])
        elif d == '.incbin':
            self.emite(open(self.ruta(resto.strip('"')), 'rb').read())
        elif d == '.endtiles':
            pass
        elif d == '.tiles':
            self.tiles_bpp = self.valor(resto, l, True) if resto else 2
        elif d == '.cabecera':
            ps = self.partes(resto)
            titulo = ps[0].strip('"').encode('latin-1')[:21].ljust(21, b' ')
            mapa, tipo, rom, sram = (self.valor(p, l) for p in ps[1:5])
            self.cabecera = (self.fisica(self.pc), mapa)
            self.emite(titulo + bytes([mapa, tipo, rom, sram, 1, 0x33, 0]) + b'\0\0\xFF\xFF')
        else:
            raise Error('no se conoce %s' % d)

    def fisica_de(self, pc):
        b, self.base = self.base, None
        f = self.fisica(pc)
        self.base = b
        return f

    def avanza(self):
        self.pc += 1
        if (self.pc & 0xFFFF) == 0 and self.mapa == 'lorom':
            self.pc += 0x8000

    def ruta(self, nom):
        return os.path.join(self.dir, nom)

    @staticmethod
    def partes(txt):
        out, cur, dentro, prof = [], '', False, 0
        for ch in txt:
            if ch == '"':
                dentro = not dentro
            if ch == ',' and not dentro and prof == 0:
                out.append(cur.strip())
                cur = ''
                continue
            if not dentro:
                prof += ch in '(['
                prof -= ch in ')]'
            cur += ch
        if cur.strip():
            out.append(cur.strip())
        return out

    # ---- 65C816 ----
    def ins_816(self, l):
        m = re.match(r'([A-Za-z]{3})(\.[bwlBWL])?\s*(.*)$', l)
        if not m:
            raise Error('no se entiende: %s' % l)
        mn = m.group(1).lower()
        suf = (m.group(2) or '').lower()[1:]
        op = m.group(3).strip()
        if mn not in OPS816:
            raise Error('no se conoce la instrucción %s' % mn)
        modos = OPS816[mn]
        ol = op.lower().replace(' ', '')
        modo, expr = None, None
        if ol == '' or ol == 'a':
            modo = 'acc' if ol == 'a' and 'acc' in modos else ('acc' if 'acc' in modos and 'imp' not in modos else 'imp')
            if mn == 'brk' and ol == '':
                modo = 'imp'
        elif ol.startswith('#'):
            expr = op.strip()[1:]
            modo = 'imm8' if 'imm8' in modos else 'imm'
        elif mn in ('mvn', 'mvp'):
            a, b = self.partes(op)
            v1, v2 = self.valor(a, l), self.valor(b, l)
            self.emite([modos['mov'], v2, v1])
            return
        else:
            mm = re.match(r'\((.+),s\),y$', ol)
            if mm:
                modo, expr = 'sriy', self.recorta(op, mm.group(1))
            elif re.match(r'\((.+),x\)$', ol):
                expr = self.recorta(op, ol[1:-3])
                modo = 'absxi' if 'absxi' in modos else 'dpxi'
            elif re.match(r'\((.+)\),y$', ol):
                expr, modo = self.recorta(op, ol[1:-3]), 'dpiy'
            elif re.match(r'\[(.+)\],y$', ol):
                expr, modo = self.recorta(op, ol[1:-3]), 'dpily'
            elif re.match(r'\((.+)\)$', ol):
                expr = self.recorta(op, ol[1:-1])
                modo = 'ind' if 'ind' in modos else 'dpi'
            elif re.match(r'\[(.+)\]$', ol):
                expr = self.recorta(op, ol[1:-1])
                modo = 'indl' if 'indl' in modos else 'dpil'
            elif ol.endswith(',s'):
                expr, modo = op[:op.lower().rindex(',')], 'sr'
            elif ol.endswith(',x') or ol.endswith(',y'):
                expr = op[:op.lower().rindex(',')]
                modo = self.tam(mn, modos, suf, expr, l, ol[-1])
            else:
                expr = op
                if 'rel' in modos:
                    modo = 'rel'
                elif 'rell' in modos:
                    modo = 'rell'
                else:
                    modo = self.tam(mn, modos, suf, expr, l, '')
        if modo not in modos:
            raise Error('%s no tiene ese modo (%s)' % (mn, modo))
        c = modos[modo]
        if modo in ('imp', 'acc'):
            self.emite([c])
            return
        if modo == 'imm':
            ancho8 = self.i8 if mn in ANCHO_X else self.a8
            v = self.valor(expr, l)
            self.emite([c, v] if ancho8 else [c, v, v >> 8])
            return
        v = self.valor(expr, l)
        if modo == 'imm8':
            self.emite([c, v])
            if mn in ('rep', 'sep') and not self.desconocido_en(expr):
                if v & 0x20:
                    self.a8 = (mn == 'sep')
                if v & 0x10:
                    self.i8 = (mn == 'sep')
            return
        if modo == 'rel':
            d = v - (self.pc + 2)
            if self.pasada_final and not -128 <= d <= 127:
                raise Error('salto demasiado largo (%d)' % d)
            self.emite([c, d & 0xFF])
            return
        if modo == 'rell':
            d = v - (self.pc + 3)
            self.emite([c, d & 0xFF, (d >> 8) & 0xFF])
            return
        n = LARGO816[modo]
        self.emite([c] + [(v >> (8 * i)) & 0xFF for i in range(n)])

    def desconocido_en(self, expr):
        self.desconocido = False
        self.valor(expr, '')
        return self.desconocido

    @staticmethod
    def recorta(op, sub):
        # devuelve la expresión original (con mayúsculas) que corresponde a «sub»
        i = op.replace(' ', '').lower().find(sub)
        return op.replace(' ', '')[i:i + len(sub)]

    def tam(self, mn, modos, suf, expr, l, idx):
        nom = {'': ('dp', 'abs', 'long'), 'x': ('dpx', 'absx', 'longx'), 'y': ('dpy', 'absy', None)}[idx]
        if suf:
            t = {'b': 0, 'w': 1, 'l': 2}[suf]
        else:
            e = expr.strip()
            self.desconocido = False
            v = self.valor(e, l)
            if re.match(r'\$[0-9A-Fa-f]{1,2}$', e):
                t = 0
            elif re.match(r'\$[0-9A-Fa-f]{6}$', e) or v > 0xFFFF:
                t = 2
            elif re.match(r'[A-Za-z_]\w*$', e) and e in self.constantes and v < 0x100:
                t = 0
            else:
                t = 1
        for i in list(range(t, 3)) + list(range(t - 1, -1, -1)):
            if nom[i] and nom[i] in modos:
                if i < t and suf:
                    continue
                return nom[i]
        raise Error('%s: modo no válido' % mn)

    # ---- SPC700 ----
    def clase_spc(self, o):
        ol = o.lower().replace(' ', '')
        tab = {'a': 'A', 'x': 'X', 'y': 'Y', 'sp': 'SP', 'psw': 'PSW', 'ya': 'YA', 'c': 'C',
               '(x)': '(X)', '(x)+': '(X)+', '(y)': '(Y)'}
        if ol in tab:
            return tab[ol], None
        if ol.startswith('#'):
            return '#', o.strip()[1:]
        m = re.match(r'\[!(.+)\+x\]$', ol)
        if m:
            return '[!a+X]', self.recorta(o, m.group(1))
        m = re.match(r'\[(.+)\+x\]$', ol)
        if m:
            return '[d+X]', self.recorta(o, m.group(1))
        m = re.match(r'\[(.+)\]\+y$', ol)
        if m:
            return '[d]+Y', self.recorta(o, m.group(1))
        m = re.match(r'(/?)!?(.+)\.([0-7])$', ol)
        if m and not re.match(r'.*\+[xy]$', ol):
            return (m.group(1) + 'BIT'), (self.recorta(o, m.group(2)), int(m.group(3)))
        if ol.startswith('!'):
            if ol.endswith('+x'):
                return '!a+X', self.recorta(o, ol[1:-2])
            if ol.endswith('+y'):
                return '!a+Y', self.recorta(o, ol[1:-2])
            return '!a', self.recorta(o, ol[1:])
        if ol.endswith('+x'):
            return 'd+X', self.recorta(o, ol[:-2])
        if ol.endswith('+y'):
            return 'd+Y', self.recorta(o, ol[:-2])
        return 'd', o.strip()

    def ins_spc(self, l):
        m = re.match(r'([A-Za-z0-9]+)\s*(.*)$', l)
        mn = m.group(1).lower()
        ops = self.partes(m.group(2)) if m.group(2).strip() else []
        clases = [self.clase_spc(o) for o in ops]
        # instrucciones de un bit de la página directa
        if mn in ('set1', 'clr1', 'bbs', 'bbc') and clases and clases[0][0] == 'BIT':
            ex, b = clases[0][1]
            d = self.valor(ex, l) & 0xFF
            base = {'set1': 0x02, 'clr1': 0x12, 'bbs': 0x03, 'bbc': 0x13}[mn] | (b << 5)
            if mn in ('set1', 'clr1'):
                self.emite([base, d])
            else:
                r = self.valor(ops[1], l) - (self.pc + 3)
                self.chk_rel(r)
                self.emite([base, d, r & 0xFF])
            return
        if mn == 'tcall':
            n = self.valor(ops[0], l)
            self.emite([0x01 | (n << 4)])
            return
        if mn == 'pcall':
            self.emite([0x4F, self.valor(ops[0], l) & 0xFF])
            return
        pat = []
        for (cl, ex), o in zip(clases, ops):
            if cl in ('BIT', '/BIT'):
                pat.append('/m.b' if cl == '/BIT' else 'm.b')
            else:
                pat.append(cl)
        # relativos: la última «d» de un salto es la etiqueta
        if mn in ('bpl', 'bra', 'bmi', 'bvc', 'bvs', 'bcc', 'bcs', 'bne', 'beq', 'cbne', 'dbnz'):
            pat[-1] = 'r'
        if mn == 'pcall':
            pat = ['u']
        clave = (mn, tuple(pat))
        if clave not in OPSSPC:
            # !a en lugar de d (o al revés), si el número lo permite
            alt = tuple('!a' if p == 'd' else p for p in pat)
            if (mn, alt) in OPSSPC:
                clave = (mn, alt)
            else:
                raise Error('SPC700: no se conoce «%s» con %s' % (mn, ' '.join(pat)))
        c = OPSSPC[clave]
        pat = clave[1]
        out = [c]
        vals = []
        for (cl, ex), p in zip(clases, pat):
            if p in ('#', 'd', 'd+X', 'd+Y', '[d+X]', '[d]+Y', 'u'):
                vals.append(('b', self.valor(ex, l)))
            elif p in ('!a', '!a+X', '!a+Y', '[!a+X]'):
                vals.append(('w', self.valor(ex, l)))
            elif p in ('m.b', '/m.b'):
                e, b = ex
                vals.append(('w', (self.valor(e, l) & 0x1FFF) | (b << 13)))
            elif p == 'r':
                vals.append(('r', self.valor(ex, l)))
        # el orden de los octetos: en dd,ds y en d,#i, el destino va el último
        if len(vals) == 2 and pat in (('d', 'd'), ('d', '#')):
            vals = [vals[1], vals[0]]
        largo = 1 + sum(2 if t == 'w' else 1 for t, _ in vals)
        for t, v in vals:
            if t == 'b':
                out.append(v & 0xFF)
            elif t == 'w':
                out += [v & 0xFF, (v >> 8) & 0xFF]
            else:
                r = v - (self.pc + largo)
                self.chk_rel(r)
                out.append(r & 0xFF)
        self.emite(out)

    def chk_rel(self, r):
        if self.pasada_final and not -128 <= r <= 127:
            raise Error('salto demasiado largo (%d)' % r)


def leer(ruta, out):
    d = os.path.dirname(ruta)
    for n, l in enumerate(open(ruta, encoding='utf-8'), 1):
        m = re.match(r'\s*\.include\s+"([^"]+)"', l)
        if m:
            leer(os.path.join(d, m.group(1)), out)
        else:
            out.append((ruta, n, l))


def main():
    fuente, salida = sys.argv[1], sys.argv[2]
    lineas = []
    leer(fuente, lineas)
    a = Asm()
    a.dir = os.path.dirname(fuente)
    tam = 0
    for _, _, l in lineas:
        m = re.match(r'\s*\.rom\s+(\d+)', l)
        if m:
            tam = int(m.group(1)) * 1024
    a.tam_rom = tam or 0x10000
    for _ in range(12):
        a.pasada(lineas, False)
        if a.cambios == 0:
            break
    a.pasada(lineas, True)
    rom = a.rom
    if a.cabecera:
        f, mapa = a.cabecera
        rom[f + 0x1C:f + 0x20] = b'\xFF\xFF\x00\x00'
        s = sum(rom) & 0xFFFF
        rom[f + 0x1C] = (s ^ 0xFFFF) & 0xFF
        rom[f + 0x1D] = (s ^ 0xFFFF) >> 8
        rom[f + 0x1E] = s & 0xFF
        rom[f + 0x1F] = s >> 8
    open(salida, 'wb').write(bytes(rom))


if __name__ == '__main__':
    main()
