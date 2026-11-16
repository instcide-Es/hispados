#!/usr/bin/env python3
"""Prueba del cálculo de Cuentas fuera de Hispa Ventanas.

uso: cuentas_calc.py [CUANTAS_AL_AZAR]

Toma programas/cuentas/calculo.inc TAL CUAL (el valor de las celdas, las
fórmulas, la coma fija de 64 bits y leer y escribir números), lo ensambla con
NASM dentro de un programa de Linux de 32 bits que hace de hoja (se le dan
celdas y se le preguntan valores por la entrada) y compara cada respuesta, el
valor en cienmillonésimas y el texto que se vería en la celda (con su formato
de número, si lo tiene), o el texto si la fórmula da un texto, con un modelo
exacto hecho aquí con enteros de Python: casos elegidos y miles de fórmulas
al azar, con todas las funciones.
"""
import datetime, math, os, random, re, subprocess, sys, tempfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
ESC, VER, CARS, NCOL, NFIL, PROF = 10**8, 4, 10, 64, 1000, 400
DIA0 = datetime.date(1899, 12, 30)
HOY = (datetime.date(2026, 11, 16) - DIA0).days     # lo que contesta HOY() en la prueba
CAD_MAX = 255
LIM = 2**63

ARNES = r'''
        bits 32
        cpu 386
NCOL        equ 64
NFIL        equ 1000
CEL_MAX     equ 128
NHOJ        equ 3                   ; las hojas del libro: Hoja1, Hoja2 y Hoja3
        section .text
        global _start
_start:
        cld
        mov dword [nf_cars], 10     ; lo que cabe en una celda de las de siempre
        mov dword [moneda_txt], 'EUR'
.linea:
        mov edi, lin                ; una línea de la entrada, acabada en 0
.c:
        push edi
        mov eax, 3
        xor ebx, ebx
        mov ecx, edi
        mov edx, 1
        int 0x80
        pop edi
        cmp eax, 1
        jne .fin
        cmp byte [edi], 10
        je .hecha
        inc edi
        jmp .c
.hecha:
        mov byte [edi], 0
        mov esi, lin
        mov eax, [h_act]            ; (las celdas son de la hoja que toca)
        mov [ev_hoja], eax
        cmp byte [esi], '@'         ; «@2»: lo que sigue es de la hoja 2
        jne .noh
        movzx eax, byte [esi+1]
        sub eax, '1'
        mov [h_act], eax
        jmp .linea
.noh:
        cmp byte [esi], '!'         ; «!»: el libro, vacío
        je .vaciar
        cmp byte [esi], '?'         ; «?B7»: lo que vale y cómo se ve
        je .pregunta
        cmp byte [esi], '%'         ; «%640»: el formato con el que se ve
        je .formato
        call lee_ref                ; «B7 texto»: el texto de la celda
        inc esi
        push esi
        call cel_puntero
        mov edi, esi
        pop esi
.cp:
        lodsb
        stosb
        test al, al
        jnz .cp
        call calc_olvida            ; (ha cambiado una celda)
        jmp .linea
.formato:
        inc esi
        xor eax, eax
.fo:
        movzx ebx, byte [esi]
        sub ebx, '0'
        cmp ebx, 9
        ja .fof
        imul eax, 10
        add eax, ebx
        inc esi
        jmp .fo
.fof:
        mov [fmt_w], eax
        jmp .linea
.vaciar:
        mov edi, cel_txt
        mov ecx, NHOJ * NCOL * NFIL * CEL_MAX
        xor al, al
        rep stosb
        mov dword [h_act], 0
        call calc_olvida
        jmp .linea
.pregunta:
        inc esi
        cmp byte [esi], '?'
        jne .p1
        inc esi
        mov byte [lin], '?'
        jmp .p2
.p1:
        mov byte [lin], 0
.p2:
        call lee_ref
        cmp byte [lin], '?'         ; («??B7»: sin olvidar lo ya calculado)
        je .calc
        call calc_olvida
.calc:
        call calc_celda
        mov edi, salida
        cmp byte [ev_err], 0
        je .tipo
        mov al, 'E'
        stosb
        mov al, [ev_err]
        add al, '0'
        stosb
        jmp .nl
.tipo:
        mov bl, 'V'
        cmp byte [cv_tipo], 0
        je .letra
        cmp byte [cv_tipo], 2
        jne .val
        mov esi, eax                ; un texto: «T» y el texto
        mov al, 'T'
        stosb
.tx:
        lodsb
        test al, al
        jz .nl
        stosb
        jmp .tx
.letra:
        mov [edi], bl
        inc edi
        jmp .nl
.val:
        mov [v], eax
        mov [v+4], edx
        mov ecx, 16                 ; el valor, en hexadecimal
.h:
        mov ebx, [v+4]
        shr ebx, 28
        mov bl, [hex+ebx]
        mov [edi], bl
        inc edi
        shld [v+4], eax, 4
        shl eax, 4
        dec ecx
        jnz .h
        mov al, ' '
        stosb
        mov eax, [v]
        mov ebx, [fmt_w]
        call fmt_celda              ; y como se ve en la celda
.z:
        cmp byte [edi], 0
        je .nl
        inc edi
        jmp .z
.nl:
        mov al, 10
        stosb
        mov eax, 4
        mov ebx, 1
        mov ecx, salida
        mov edx, edi
        sub edx, salida
        int 0x80
        jmp .linea
.fin:
        mov eax, 1
        xor ebx, ebx
        int 0x80
hex         db "0123456789ABCDEF"
; el día de hoy, para HOY(): siempre el mismo (el 16/11/2026)
hoy_serie:
        mov eax, HOY
        ret
cel_puntero:
        push eax
        mov eax, [ev_hoja]
        imul eax, NFIL
        add eax, edx
        imul eax, NCOL
        add eax, ecx
        imul eax, CEL_MAX
        lea esi, [cel_txt+eax]
        pop eax
        ret
; la hoja que se llama como ESI (ECX letras): aquí, Hoja1, Hoja2 y Hoja3
hoja_busca:
        cmp ecx, 5
        jne .no
        mov eax, [esi]
        and eax, 0xDFDFDFDF
        cmp eax, 'HOJA'
        jne .no
        movzx eax, byte [esi+4]
        sub eax, '1'
        cmp eax, NHOJ
        jae .no
        clc
        ret
.no:
        stc
        ret
; ALEATORIO(): aquí, siempre lo mismo
azar_num:
        mov eax, 12345678
        ret
; la ficha donde se apunta el valor de una fórmula: aquí, una por celda
cel_cache:
        mov edi, [ev_hoja]
        imul edi, NFIL
        add edi, edx
        imul edi, NCOL
        add edi, ecx
        imul edi, CV_TAM
        add edi, fichas
        ret
%include "calculo.inc"
        section .bss
fichas      resb NHOJ * NCOL * NFIL * CV_TAM
h_act       resd 1
lin         resb 512
salida      resb 512
fmt_w       resd 1
v           resd 2
cel_txt     resb NHOJ * NCOL * NFIL * CEL_MAX
'''


def hacer(tmp):
    inc = open(os.path.join(RAIZ, 'programas', 'cuentas', 'calculo.inc'), encoding='utf-8').read()
    open(os.path.join(tmp, 'calculo.inc'), 'w', encoding='cp437').write(inc)
    asm = os.path.join(tmp, 'calc.asm')
    # (en .val, EAX se gasta al escribir el hexadecimal: num_fmt lo toma de v)
    open(asm, 'w', encoding='cp437').write(
        ARNES.replace('HOY', str(HOY))
             .replace('        mov eax, [v]\n        mov ebx, [fmt_w]',
                      '        mov eax, [v]\n        mov edx, [vv]\n        mov ebx, [fmt_w]')
             .replace('        mov [v+4], edx\n        mov ecx, 16',
                      '        mov [v+4], edx\n        mov [vv], edx\n        mov ecx, 16')
             .replace('v           resd 2', 'v           resd 2\nvv          resd 1'))
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'calc'), asm + '.o'], check=True)
    return os.path.join(tmp, 'calc')


# ---- el modelo: lo mismo, con enteros de Python ----
# Un valor es None (vacío), un entero (un número, en cienmillonésimas) o un
# str (un texto).
class Err(Exception):
    def __init__(self, n=1):
        self.n = n


def rdiv(a, b):
    """a / b redondeado al más cercano; la mitad, alejándose de cero"""
    q, r = divmod(abs(a), abs(b))
    if 2 * r >= abs(b):
        q += 1
    return q if (a < 0) == (b < 0) else -q


def mag(x):
    if abs(x) >= LIM:
        raise Err()
    return x


def suma(x):
    if not -LIM <= x < LIM:
        raise Err()
    return x


def mul(x, y):
    return mag(rdiv(x * y, ESC))


def div(x, y):
    if y == 0:
        raise Err()
    return mag(rdiv(x * ESC, y))


def raiz(x):
    n = x * ESC
    r = math.isqrt(n)
    return r + 1 if n - r * r > r else r


def lee_num(t, i):
    """como lee_num: -> (valor, sitio detrás) o None si no hay cifras o no cabe"""
    def por10(a):
        if (a >> 32) >= 0x0CCCCCCC:
            raise OverflowError
        return a * 10
    acc, coma, dec, hay, red, visto = 0, False, 0, False, False, False
    try:
        while i < len(t):
            c = t[i]
            if c in ',.':
                if coma:
                    break
                coma = True
            elif c.isdigit() and c.isascii():
                hay = True
                if coma and dec >= 8:
                    if not visto:
                        visto = True
                        red = c >= '5'
                else:
                    if coma:
                        dec += 1
                    acc = por10(acc) + int(c)
                    if acc >= LIM:
                        raise OverflowError
            else:
                break
            i += 1
        if not hay:
            return None
        while dec < 8:
            acc = por10(acc)
            dec += 1
        if red:
            acc += 1
        if acc >= LIM:
            return None
    except OverflowError:
        return None
    return acc, i


FECHA = re.compile(r'([0-9]{1,2})/([0-9]{1,2})/([0-9]{4})(?![0-9])')
REF = re.compile(r'(\$?)([A-Za-z]{1,2})(\$?)([0-9]+)')
PREF = re.compile(r'([A-Za-z0-9_]{1,15})!')         # «Hoja2!»: una celda de otra hoja del libro


def cad_a_num(t):
    """como cad_a_num: el número (o la fecha) que es ese texto, o None"""
    s = t.lstrip(' ')
    m = FECHA.match(s)
    if m:
        d, me, a = (int(g) for g in m.groups())
        try:
            dia = datetime.date(a, me, d) if a >= 1900 else None
        except ValueError:
            dia = None
        if dia is not None:
            return (dia - DIA0).days * ESC if s[m.end():].strip(' ') == '' else None
    s = s.rstrip(' ')
    neg = s[:1] == '-'
    if s[:1] in ('-', '+'):
        s = s[1:]
    r = lee_num(s, 0)
    if r is None or r[1] != len(s):
        return None
    # (espacios delante y detrás valen; entre el signo y el número, no)
    return -r[0] if neg else r[0]


MAY = str.maketrans('abcdefghijklmnopqrstuvwxyzñéüç', 'ABCDEFGHIJKLMNOPQRSTUVWXYZÑÉÜÇ')
MIN = str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZÑÉÜÇ', 'abcdefghijklmnopqrstuvwxyzñéüç')


def cad_cmp(a, b):
    a, b = a.translate(MAY).encode('cp437'), b.translate(MAY).encode('cp437')
    return (a > b) - (a < b)


def num(v):
    """en una cuenta: el vacío y el texto que no es un número valen 0"""
    if v is None:
        return 0
    if isinstance(v, str):
        n = cad_a_num(v)
        return 0 if n is None else n
    return v


def num_fijo(x, dec, sin_ceros=False, millares=False):
    neg, x = x < 0, abs(x)
    q = rdiv(x, 10 ** (8 - dec))
    ent, d = divmod(q, 10 ** dec)
    s = str(ent)
    if millares:
        s = '.'.join(reversed([s[::-1][i:i + 3][::-1] for i in range(0, len(s), 3)]))
    if dec:
        s += ',' + '%0*d' % (dec, d)
        if sin_ceros:
            s = s.rstrip('0').rstrip(',')
    return ('-' if neg and q else '') + s


def a_cad(v):
    if v is None:
        return ''
    return v if isinstance(v, str) else num_fijo(v, 8, True)


def compara(a, b):
    if a is None:
        if b is None:
            return 0
        a = '' if isinstance(b, str) else 0
    if b is None:
        b = '' if isinstance(a, str) else 0
    if isinstance(a, str) != isinstance(b, str):
        return 1 if isinstance(a, str) else -1        # un número es menor que un texto
    return cad_cmp(a, b) if isinstance(a, str) else (a > b) - (a < b)


def cumple(r, op):
    return (r == 0, r != 0, r < 0, r > 0, r <= 0, r >= 0)[op]


def colnum(letras):
    letras = letras.upper()
    return ord(letras) - 65 if len(letras) == 1 else (ord(letras[0]) - 64) * 26 + ord(letras[1]) - 65


def colnom(c):
    return chr(65 + c) if c < 26 else chr(64 + c // 26) + chr(65 + c % 26)


NADA = object()        # (lo que contesta mas_funciones si el nombre no es suyo)


class Hoja:
    """Una hoja de un libro (libro: la lista de sus hojas, compartida). Una
    celda de otra hoja se pide con esa hoja, más uno, por encima de los 16
    bits de la fila: así van los rangos de otra hoja, como en calculo.inc"""
    def __init__(self, libro=None, n=0):
        self.c = {}
        self.libro = [self] if libro is None else libro
        self.n = n
        self.nombre = 'Hoja%d' % (n + 1)

    def valor(self, col, fil, pila=(), prof=0):
        if fil >> 16:
            return self.libro[(fil >> 16) - 1].valor(col, fil & 0xFFFF, pila, prof)
        if not (0 <= col < NCOL and 0 <= fil < NFIL):
            raise Err(3)
        if (self.n, col, fil) in pila:
            raise Err(2)
        t = self.c.get((col, fil), '')
        if t == '':
            return None
        if t[0] != '=':
            n = cad_a_num(t)
            return t if n is None else n
        if prof >= PROF:
            raise Err()
        p = Formula(self, t[1:], pila + ((self.n, col, fil),), prof + 1)
        x = p.expr()
        if p.car() != '':
            raise Err()
        return 0 if x is None else x


class Formula:
    def __init__(self, hoja, t, pila, prof):
        self.h, self.t, self.i, self.pila, self.prof = hoja, t, 0, pila, prof

    def car(self):
        while self.i < len(self.t) and self.t[self.i] == ' ':
            self.i += 1
        return self.t[self.i] if self.i < len(self.t) else ''

    def expr(self):
        x = self.union()
        while True:
            c, sig = self.car(), self.t[self.i + 1:self.i + 2]
            if c == '=':
                op, n = 0, 1
            elif c == '<':
                op, n = (1, 2) if sig == '>' else (4, 2) if sig == '=' else (2, 1)
            elif c == '>':
                op, n = (5, 2) if sig == '=' else (3, 1)
            else:
                return x
            self.i += n
            y = self.union()
            x = ESC if cumple(compara(x, y), op) else 0

    def union(self):
        x = self.suma()
        while self.car() == '&':
            self.i += 1
            a = a_cad(x)
            x = (a + a_cad(self.suma()))[:CAD_MAX]
        return x

    def suma(self):
        x = self.term()
        while self.car() in ('+', '-') and self.car():
            op = self.car(); self.i += 1
            x = num(x)
            y = num(self.term())
            x = suma(x + y if op == '+' else x - y)
        return x

    def term(self):
        x = self.pot()
        while self.car() in ('*', '/') and self.car():
            op = self.car(); self.i += 1
            x = num(x)
            y = num(self.pot())
            x = mul(x, y) if op == '*' else div(x, y)
        return x

    def pot(self):
        x = self.factor()
        while self.car() == '^':
            self.i += 1
            x = num(x)
            x = self.potencia(x, num(self.factor()))
        return x

    @staticmethod
    def potencia(b, e):
        neg, e = e < 0, abs(e)
        n, r = divmod(e, ESC)
        if n >= 2 ** 32 or r not in (0, ESC // 2):
            raise Err()
        res, base = ESC, b
        while n:
            if n & 1:
                res = mul(res, base)
            n >>= 1
            if n:
                base = mul(base, base)
        if r:
            if b < 0:
                raise Err()
            res = mul(res, raiz(b))
        return div(ESC, res) if neg else res

    def factor(self):
        c = self.car()
        if c == '-':
            self.i += 1
            return -num(self.factor())
        if c == '+':
            self.i += 1
            return num(self.factor())
        x = self.primario()
        while self.car() == '%':
            self.i += 1
            x = div(num(x), 100 * ESC)
        return x

    def cierra(self):
        if self.car() != ')':
            raise Err()
        self.i += 1

    def abre(self):
        if self.car() != '(':
            raise Err()
        self.i += 1

    def salta(self):
        """un argumento que no se calcula: hasta su «;» o su «)»"""
        t, i, hondo = self.t, self.i, 0
        while i < len(t):
            c = t[i]
            if c == '"':
                i = t.find('"', i + 1)
                if i < 0:
                    i = len(t)
                    break
            elif c == '(':
                hondo += 1
            elif c == ')':
                if hondo == 0:
                    break
                hondo -= 1
            elif c == ';' and hondo == 0:
                break
            i += 1
        self.i = i

    def primario(self):
        c = self.car()
        if c == '(':
            self.i += 1
            x = self.expr()
            self.cierra()
            return x
        if c == '"':
            i, s = self.i + 1, ''
            while True:
                if i >= len(self.t):
                    raise Err()
                if self.t[i] == '"':
                    if self.t[i + 1:i + 2] != '"':
                        break
                    i += 1
                s += self.t[i]
                i += 1
            self.i = i + 1
            return s[:CAD_MAX]
        if c == '#':
            if self.t[self.i:self.i + 4] == '#REF':
                raise Err(3)
            raise Err()
        if c and (c in ',.' or c in '0123456789'):
            r = lee_num(self.t, self.i)
            if r is None:
                raise Err()
            self.i = r[1]
            return r[0]
        if c == '$' or (c and c.isascii() and c.isalpha()):
            return self.nombre()
        raise Err()

    def args1(self):
        self.abre()
        x = num(self.expr())
        self.cierra()
        return x

    def args2(self):
        self.abre()
        x = num(self.expr())
        if self.car() != ';':
            raise Err()
        self.i += 1
        y = num(self.expr())
        self.cierra()
        return x, y

    def hoja_pref(self, i):
        """-> (la hoja que nombra lo que hay en i, «Hoja2!», como la quiere
        Hoja.valor, o 0 si no nombra ninguna; y dónde sigue)"""
        m = PREF.match(self.t, i)
        if not m:
            return 0, i
        for h in self.h.libro:
            if h.nombre.upper() == m.group(1).upper():
                return (h.n + 1) << 16, m.end()
        raise Err(3)

    def nombre(self):
        hoja, j = self.hoja_pref(self.i)
        m = REF.match(self.t, j)
        if m:
            self.i = m.end()
            col, fil = colnum(m.group(2)), int(m.group(4)) - 1
            if not (0 <= col < NCOL and 0 <= fil < NFIL):
                raise Err(3)
            return self.h.valor(col, fil | hoja, self.pila, self.prof)
        if hoja:
            raise Err()
        m = re.compile(r'[A-Za-zñÑ.]*').match(self.t, self.i)
        self.i = m.end()
        nom = m.group().upper()[:15]
        if nom in ('SUMA', 'PROMEDIO', 'MAX', 'MIN', 'CONTAR', 'CONTARA', 'PRODUCTO', 'MEDIANA', 'VAR', 'DESVEST', 'MODA'):
            return self.rangos(nom)
        r = self.mas_funciones(nom)
        if r is not NADA:
            return r
        if nom == 'ABS':
            return abs(self.args1())
        if nom == 'REDONDEAR':
            x, d = self.args2()
            if d < 0 or d % ESC or d // ESC > 8:
                raise Err()
            p = 10 ** (8 - d // ESC)
            r = rdiv(abs(x), p) * p
            if r >= LIM:
                raise Err()
            return -r if x < 0 else r
        if nom == 'SI':
            self.abre()
            c = num(self.expr())
            if self.car() != ';':
                raise Err()
            self.i += 1
            if c:
                x = self.expr()
                if self.car() == ';':
                    self.i += 1
                    self.salta()
            else:
                self.salta()
                x = 0
                if self.car() == ';':
                    self.i += 1
                    x = self.expr()
            self.cierra()
            return x
        if nom in ('Y', 'O'):
            self.abre()
            r = nom == 'Y'
            while True:
                v = num(self.expr()) != 0
                r = (r and v) if nom == 'Y' else (r or v)
                if self.car() != ';':
                    break
                self.i += 1
            self.cierra()
            return ESC if r else 0
        if nom == 'NO':
            return 0 if self.args1() else ESC
        if nom == 'VERDADERO':
            return ESC
        if nom == 'FALSO':
            return 0
        if nom == 'ENTERO':
            return mag(self.args1() // ESC * ESC)
        if nom == 'RAIZ':
            x = self.args1()
            if x < 0:
                raise Err()
            return raiz(x)
        if nom == 'POTENCIA':
            return self.potencia(*self.args2())
        if nom == 'RESIDUO':
            a, b = self.args2()
            if b == 0:
                raise Err()
            return a % b
        if nom in ('CONTAR.SI', 'SUMAR.SI'):
            return self.si_rango(nom == 'SUMAR.SI')
        if nom == 'HOY':
            self.abre()
            self.cierra()
            return HOY * ESC
        if nom == 'CONCATENAR':
            self.abre()
            s = a_cad(self.expr())
            while self.car() == ';':
                self.i += 1
                s = (s + a_cad(self.expr()))[:CAD_MAX]
            self.cierra()
            return s
        if nom in ('LARGO', 'MAYUSC', 'MINUSC'):
            self.abre()
            s = a_cad(self.expr())
            self.cierra()
            return len(s) * ESC if nom == 'LARGO' else s.translate(MAY if nom == 'MAYUSC' else MIN)
        raise Err()

    def coma(self):
        """gasta un «;», si viene"""
        if self.car() == ';':
            self.i += 1
            return True
        return False

    def ent(self):
        x = num(self.expr())
        n = abs(x) // ESC
        if n >= 2 ** 31:
            raise Err()
        return -n if x < 0 else n

    @staticmethod
    def a_dia(x):
        if x < 0 or x // ESC > 2958465:
            raise Err()
        return x // ESC

    def mas_funciones(self, nom):
        if nom in ('PI', 'ALEATORIO'):
            self.abre()
            self.cierra()
            return 314159265 if nom == 'PI' else 12345678
        if nom == 'SIGNO':
            x = self.args1()
            return (x > 0) * ESC - (x < 0) * ESC
        if nom in ('TRUNCAR', 'REDONDEAR.MENOS', 'REDONDEAR.MAS'):
            self.abre()
            x = num(self.expr())
            if self.coma():
                d = self.ent()
            elif nom == 'TRUNCAR':
                d = 0
            else:
                raise Err()
            self.cierra()
            if not 0 <= d <= 8:
                raise Err()
            p = 10 ** (8 - d)
            q, r = divmod(abs(x), p)
            if nom == 'REDONDEAR.MAS' and r:
                q += 1
            if q * p >= LIM:
                raise Err()
            return -q * p if x < 0 else q * p
        if nom == 'PAGO':
            self.abre()
            tasa = num(self.expr())
            if not self.coma():
                raise Err()
            plazos = num(self.expr())
            if not self.coma():
                raise Err()
            capital = num(self.expr())
            self.cierra()
            if tasa == 0:
                return -div(capital, plazos)
            n = mul(capital, tasa)
            return -div(n, ESC - self.potencia(suma(tasa + ESC), -plazos))
        if nom == 'FECHA':
            self.abre()
            a = self.ent()
            if not self.coma():
                raise Err()
            m = self.ent()
            if not self.coma():
                raise Err()
            d = self.ent()
            self.cierra()
            try:
                if not 1900 <= a <= 9999:
                    raise ValueError
                return (datetime.date(a, m, d) - DIA0).days * ESC
            except ValueError:
                raise Err()
        if nom in ('AÑO', 'ANO', 'MES', 'DIA'):
            f = DIA0 + datetime.timedelta(self.a_dia(self.args1()))
            return (f.year if nom in ('AÑO', 'ANO') else f.month if nom == 'MES' else f.day) * ESC
        if nom == 'DIASEM':
            self.abre()
            x = num(self.expr())
            tipo = self.ent() if self.coma() else 1
            self.cierra()
            d = self.a_dia(x)
            if tipo not in (1, 2):
                raise Err()
            return ((d + (6 if tipo == 1 else 5)) % 7 + 1) * ESC
        if nom in ('ESNUMERO', 'ESTEXTO', 'ESBLANCO'):
            self.abre()
            x = self.expr()
            self.cierra()
            clase = 2 if x is None else 1 if isinstance(x, str) else 0
            return ESC if clase == ('ESNUMERO', 'ESTEXTO', 'ESBLANCO').index(nom) else 0
        if nom == 'SI.ERROR':
            self.abre()
            ini = self.i
            try:
                x = self.expr()
            except Err:
                self.i = ini
                self.salta()
                if not self.coma():
                    raise Err()
                x = self.expr()
                self.cierra()
                return x
            if not self.coma():
                raise Err()
            self.salta()
            self.cierra()
            return x
        if nom in ('IZQUIERDA', 'DERECHA'):
            self.abre()
            t = a_cad(self.expr())
            n = self.ent() if self.coma() else 1
            self.cierra()
            if n < 0:
                raise Err()
            n = min(n, len(t))
            return t[:n] if nom == 'IZQUIERDA' else t[len(t) - n:]
        if nom == 'EXTRAE':
            self.abre()
            t = a_cad(self.expr())
            if not self.coma():
                raise Err()
            ini = self.ent()
            if not self.coma():
                raise Err()
            n = self.ent()
            self.cierra()
            if n < 0 or ini < 1:
                raise Err()
            return t[ini - 1:ini - 1 + n]
        if nom == 'ENCONTRAR':
            self.abre()
            b = a_cad(self.expr())
            if not self.coma():
                raise Err()
            t = a_cad(self.expr())
            ini = self.ent() if self.coma() else 1
            self.cierra()
            if ini < 1 or ini - 1 > len(t):
                raise Err()
            k = t.find(b, ini - 1)
            if k < 0:
                raise Err()
            return (k + 1) * ESC
        if nom == 'ESPACIOS':
            self.abre()
            t = a_cad(self.expr())
            self.cierra()
            return ' '.join(x for x in t.split(' ') if x)
        if nom == 'VALOR':
            self.abre()
            x = self.expr()
            if isinstance(x, str):
                x = cad_a_num(x)
                if x is None:
                    raise Err()
            self.cierra()
            return num(x)
        if nom == 'TEXTO':
            self.abre()
            x = num(self.expr())
            if not self.coma():
                raise Err()
            f = a_cad(self.expr())
            self.cierra()
            if f[:1].translate(MAY) == 'D':
                return (DIA0 + datetime.timedelta(self.a_dia(x))).strftime('%d/%m/%Y')
            dec, mil, pct, tras = 0, False, False, False
            for c in f:
                if c == '%':
                    pct = True
                elif c == ',':
                    tras = True
                elif not tras:
                    mil = mil or c == '.'
                elif dec < 8:
                    dec += 1
            if pct:
                if abs(x) >> 32 >= 0x0147AE14:
                    raise Err()
                x *= 100
            return num_fijo(x, dec, False, mil) + ('%' if pct else '')
        if nom in ('BUSCARV', 'BUSCARH', 'COINCIDIR'):
            return self.buscar(nom)
        if nom == 'INDICE':
            self.abre()
            r = self.rango_lee()
            if r is None or not self.coma():
                raise Err()
            f = self.ent()
            c = self.ent() if self.coma() else None
            self.cierra()
            if c is None:
                f, c = (1, f) if r[1] == r[3] else (f, 1)
            if f < 1 or c < 1:
                raise Err()
            if r[1] + f - 1 > r[3] or r[0] + c - 1 > r[2]:
                raise Err(3)
            return self.h.valor(r[0] + c - 1, r[1] + f - 1, self.pila, self.prof)
        return NADA

    def buscar(self, nom):
        self.abre()
        v = self.expr()
        if not self.coma():
            raise Err()
        r = self.rango_lee()
        if r is None:
            raise Err()
        idx, exacto = 1, False
        if nom == 'COINCIDIR':
            if self.coma():
                tipo = self.ent()
                if tipo not in (0, 1):
                    raise Err()
                exacto = tipo == 0
        else:
            if not self.coma():
                raise Err()
            idx = self.ent()
            if self.coma():
                exacto = num(self.expr()) == 0
        self.cierra()
        filas, cols = r[3] - r[1], r[2] - r[0]
        vert = nom == 'BUSCARV' or (nom == 'COINCIDIR' and cols == 0)
        n, otro = (filas, cols) if vert else (cols, filas)
        if idx < 1:
            raise Err()
        if idx - 1 > otro and nom != 'COINCIDIR':
            raise Err(3)
        enc = -1
        for i in range(n + 1):
            c, f = (r[0], r[1] + i) if vert else (r[0] + i, r[1])
            x = self.h.valor(c, f, self.pila, self.prof)
            if x is None:
                continue
            k = compara(v, x)
            if k == 0 or (k > 0 and not exacto):
                enc = i
                if exacto:
                    break
            elif k < 0 and not exacto:
                break
        if enc < 0:
            raise Err()
        if nom == 'COINCIDIR':
            return (enc + 1) * ESC
        c, f = (r[0] + idx - 1, r[1] + enc) if vert else (r[0] + enc, r[1] + idx - 1)
        return self.h.valor(c, f, self.pila, self.prof)

    def rango_lee(self):
        """-> (c1, f1, c2, f2), o None si lo que viene no es un rango ni una
        celda que sea el argumento entero (y entonces no gasta nada)"""
        self.car()
        t = self.t
        hoja, j = self.hoja_pref(self.i)
        m = REF.match(t, j)
        if not m:
            return None
        j = m.end()
        while t[j:j + 1] == ' ':
            j += 1
        c1, f1 = colnum(m.group(2)), int(m.group(4)) - 1
        if t[j:j + 1] == ':':
            j += 1
            while t[j:j + 1] == ' ':
                j += 1
            m2 = REF.match(t, j)
            if not m2:
                self.i = j
                raise Err()
            self.i = m2.end()
            c2, f2 = colnum(m2.group(2)), int(m2.group(4)) - 1
        elif t[j:j + 1] in (';', ')') and j < len(t):
            self.i = j
            c2, f2 = c1, f1
        else:
            return None
        if not (0 <= c1 < NCOL and 0 <= c2 < NCOL and 0 <= f1 < NFIL and 0 <= f2 < NFIL):
            raise Err(3)
        return min(c1, c2), min(f1, f2) | hoja, max(c1, c2), max(f1, f2) | hoja

    def rangos(self, nom):
        e = {'n': 0, 'na': 0, 's': 0, 'ext': 0}
        lista = []

        def cuenta(v):
            if v is None:
                return
            e['na'] += 1
            if isinstance(v, str):
                return
            e['n'] += 1
            if nom == 'PRODUCTO':
                e['ext'] = v if e['n'] == 1 else mul(e['ext'], v)
                return
            if nom in ('MEDIANA', 'VAR', 'DESVEST', 'MODA'):
                if e['n'] > 512:
                    raise Err()
                lista.append(v)
                return
            e['s'] = suma(e['s'] + v)
            if e['n'] == 1 or (nom == 'MAX' and v > e['ext']) or (nom == 'MIN' and v < e['ext']):
                e['ext'] = v

        self.abre()
        while True:
            r = self.rango_lee()
            if r:
                for f in range(r[1], r[3] + 1):
                    for c in range(r[0], r[2] + 1):
                        cuenta(self.h.valor(c, f, self.pila, self.prof))
            else:
                cuenta(self.expr())
            if self.car() != ';':
                break
            self.i += 1
        self.cierra()
        if nom == 'SUMA':
            return e['s']
        if nom == 'PROMEDIO':
            return div(e['s'], e['n'] * ESC)
        if nom == 'CONTAR':
            return e['n'] * ESC
        if nom == 'CONTARA':
            return e['na'] * ESC
        if nom == 'MODA':
            veces, cual = 1, None
            for v in lista:
                if lista.count(v) > veces:
                    veces, cual = lista.count(v), v
            if cual is None:
                raise Err()
            return cual
        if nom == 'MEDIANA':
            lista.sort()
            n = len(lista)
            if n == 0:
                raise Err()
            return lista[n // 2] if n % 2 else div(suma(lista[n // 2] + lista[n // 2 - 1]), 2 * ESC)
        if nom in ('VAR', 'DESVEST'):
            n = len(lista)
            if n < 2:
                raise Err()
            s = 0
            for v in lista:
                s = suma(s + v)
            m, acc = div(s, n * ESC), 0
            for v in lista:
                d = suma(v - m)
                acc = suma(acc + mul(d, d))
            var = div(acc, (n - 1) * ESC)
            return raiz(var) if nom == 'DESVEST' else var
        return e['ext']

    def si_rango(self, sumar):
        self.abre()
        r = self.rango_lee()
        if r is None or self.car() != ';':
            raise Err()
        self.i += 1
        crit, op = self.expr(), 0
        if isinstance(crit, str):
            s = crit
            if s[:1] == '=':
                s = s[1:]
            elif s[:1] == '<':
                op, s = (1, s[2:]) if s[1:2] == '>' else (4, s[2:]) if s[1:2] == '=' else (2, s[1:])
            elif s[:1] == '>':
                op, s = (5, s[2:]) if s[1:2] == '=' else (3, s[1:])
            n = cad_a_num(s)
            crit = s if n is None else n
        else:
            crit = num(crit)
        sc, sf = r[0], r[1]
        if sumar and self.car() == ';':
            self.i += 1
            r2 = self.rango_lee()
            if r2 is None:
                raise Err()
            sc, sf = r2[0], r2[1]
        self.cierra()
        n, s = 0, 0
        for f in range(r[1], r[3] + 1):
            for c in range(r[0], r[2] + 1):
                v = self.h.valor(c, f, self.pila, self.prof)
                if v is not None and isinstance(v, str) == isinstance(crit, str):
                    ok = cumple(cad_cmp(v, crit) if isinstance(v, str) else (v > crit) - (v < crit), op)
                else:
                    ok = op == 1            # de otra clase: solo cumple «distinto de»
                if not ok:
                    continue
                n += 1
                if sumar and 0 <= c - r[0] + sc < NCOL and (f - r[1] + sf) & 0xFFFF < NFIL:
                    w = self.h.valor(c - r[0] + sc, f - r[1] + sf, self.pila, self.prof)
                    if w is not None and not isinstance(w, str):
                        s = suma(s + w)
        return s if sumar else n * ESC


def celda(x):
    neg, x = x < 0, abs(x)
    for d in range(VER, -1, -1):
        q = rdiv(x, 10 ** (8 - d))
        ent, dec = divmod(q, 10 ** d)
        ds = ('%0*d' % (d, dec)).rstrip('0') if d else ''
        s = ('-' if neg and q else '') + str(ent) + (',' + ds if ds else '')
        if len(s) <= CARS:
            return s
    return '#' * CARS


def celda_fmt(x, fmt):
    """como fmt_celda: el número con el formato de la celda (bits 7-9: la
    clase; 10-13: los decimales)"""
    clase, dec = fmt >> 7 & 7, min(fmt >> 10 & 15, 8)
    if clase == 0:
        return celda(x)
    s = None
    if clase in (1, 2, 3):
        s = num_fijo(x, dec, False, clase != 1) + (' EUR' if clase == 3 else '')
    elif clase == 4:
        if abs(x) >> 32 < 0x0147AE14:
            s = num_fijo(x * 100, dec) + '%'
    elif clase == 5:
        if 0 <= x // ESC <= 2958465 and x >= 0:
            s = (DIA0 + datetime.timedelta(x // ESC)).strftime('%d/%m/%Y')
    return s if s is not None and len(s) <= CARS else '#' * CARS


# ---- los casos ----
# fórmulas sueltas (sin el «=»): se ponen en Z100 y se pregunta por ella
SUELTAS = '''12,5*3
0,1+0,2
0,07*100
0,5*100
0,02*100
0,03*100
0,06*100
0,07*1000
0,00005*1
0,00005
0,00004
10/3
10/3*3
2/3
-2/3
1/3+1/3+1/3
7/2
-7/2
19,99*3
1,005*100
0,125*8
100*21/100
100*21%
21%
200%%
1234567,89*0,21
1234567,89*2
99999999*99999999
46000000000*2
92233720368
92233720369
92233720368,54775807
92233720368,54775808
1000000*1000000
50000*50000
1/0
(1+2)*(3+4)
-(2+3)*2
2*3+4*5
10-2-3
100/8/5
-0,00001*1
123456,789*1
1234567,891*1
-1234567,891*1
9,995
9,99995
99999,99995
999999999,5
9999999999,5
,5+,5
5,
1.5*2
3,14159265358979
0,000000005
0,000000004
1/3*3
1/7*7
1/9*9
22/7
1000000/3
0,1*0,1
0,01*0,01
0,0001*0,0001
0,00001*0,00001
33,33*1,5
49,995*1
2,675*100
1,15*100
8,2*3
4,35*100
92233720368+0,54775807
92233720368+1
-92233720368-1
90000000000+90000000000
3*-2
3--2
+5
((((1))))
1+
*2
(1+2
1 + 2 * 3
  7  /  2  
abs(-3,5)
ABS(2-5)*2
redondear(2,675;2)
REDONDEAR(1234,5678;0)
REDONDEAR(-1234,5678;1)
REDONDEAR(0,123456789;8)
REDONDEAR(5;9)
REDONDEAR(5;1,5)
REDONDEAR(5;-1)
REDONDEAR(5)
REDONDEAR(92233720368,5;0)
REDONDEAR(1/3;4)*3
hola
A
A+1
SUMA
SUMA(
SUMA()
SUMA(A1
SUMA(A1:)
SUMA(1:2)
suma(a1:a1)
Z101
Z0
AA1
A1B
2(3)
2 3
)
1,2,3
1..2
3%*200
-3%
2^10
2^0,5
2^-1
-2^2
2^3^2
0^0
0^-1
4^1,5
(-4)^0,5
2^0,3
1,0000001^1000000
10^11
9^0,5*9^0,5
POTENCIA(2;10)
potencia(1,5;3)
POTENCIA(2)
RAIZ(2)
RAIZ(16)
RAIZ(-1)
RAIZ(0,00000001)
RAIZ(92233720368)
RAIZ(2)*RAIZ(2)
ENTERO(2,5)
ENTERO(-2,5)
ENTERO(-3)
ENTERO(0,99999999)
ENTERO(-0,00000001)
ENTERO(-92233720368,5)
RESIDUO(10;3)
RESIDUO(-10;3)
RESIDUO(10;-3)
RESIDUO(-10;-3)
RESIDUO(7,5;2)
RESIDUO(5;0)
RESIDUO(9;3)
1=1
1=2
1<>2
2<1
2>1
2<=2
2>=3
1+1=2
"a"="A"
"a"<"b"
"b"<"a"
"pan"<>"vino"
1<"a"
"a">99999
"10"=10
1=1=1
SI(1;2;3)
SI(0;2;3)
SI(0;2)
SI(1;2)
SI(2>1;"sí";"no")
SI(2<1;"sí";"no")
SI(1;5;1/0)
SI(0;1/0;5)
SI(0;(1;2);7)
SI(1;8;"a;b")
SI(0;"a;b)";9)
SI(1/0;1;2)
SI(1)
SI()
SI(1;2;3;4)
Y(1;1;1)
Y(1;0;1)
Y(2>1;3>2)
O(0;0)
O(0;5)
O(1=2;2=2)
NO(0)
NO(7)
NO(1=1)
VERDADERO
FALSO
VERDADERO+VERDADERO
SI(Y(1;O(0;1));10;20)
"hola"
"a"&"b"
"a"&1
1&2
1,50&""
"x"&1/3
"dice ""hola"""
"sin cerrar
CONCATENAR("a";"b";"c")
CONCATENAR("total: ";12,5*2)
CONCATENAR(1)
LARGO("hola")
LARGO("")
LARGO(12345)
LARGO(1/3)
MAYUSC("hola Ñandú")
MINUSC("HOLA ÑANDU É")
mayusc("año")
"a"+1
"12"+1
"12,5"*2
-"3"
+"abc"
"5"%
HOY()
HOY()+1
HOY(1)
SUMA(1;2;3)
SUMA(1;"a";2)
SUMA(A1;5)
SUMA(1+1;2*3)
MAX(3;7;5)
MIN(3;7;5)
PROMEDIO(1;2;3;4)
CONTAR(1;"a";3)
CONTARA(1;"a";3)
CONTARA()
SUMA(A1:B2;3)
$A$1
A$1+$A1
$Z$1001
BL1000
BM1
AA1+AB2
#REF
#REF+1
#RE
1+#REF
CONTAR.SI(A1:A3;1)
SUMAR.SI(A1:A3;">0")
CONTAR.SI(1;1)
CONTAR.SI(A1:A3)
CONTAR.SIX(A1:A3;1)
16/11/2026
1&&2
A1&A2
A1=""
A1=0
A1<>""
PI()
PI()*2
PI
ALEATORIO()
SIGNO(-3,5)
SIGNO(0)
SIGNO(7)
TRUNCAR(2,789)
TRUNCAR(-2,789)
TRUNCAR(2,789;2)
TRUNCAR(-2,789;1)
TRUNCAR(5;9)
REDONDEAR.MAS(2,001;2)
REDONDEAR.MAS(-2,001;2)
REDONDEAR.MAS(2;2)
REDONDEAR.MAS(2,1)
REDONDEAR.MENOS(2,999;2)
REDONDEAR.MENOS(-2,999;0)
REDONDEAR.MAS(92233720368,1;0)
PAGO(0,05/12;120;10000)
PAGO(0;10;1000)
PAGO(0,1;1;100)
PAGO(0,05;0;100)
PAGO(1;2)
REDONDEAR(PAGO(5%/12;12*20;150000);2)
FECHA(2026;11;16)
FECHA(2024;2;29)
FECHA(2023;2;29)
FECHA(1899;12;31)
FECHA(2026;13;1)
FECHA(2026;11)
AÑO(FECHA(2026;11;16))
año(46342)
ANO(46342)
MES(46342)
DIA(46342)
DIA(-1)
MES(2958466)
DIA(46342,9)
DIASEM(46342)
DIASEM(46342;2)
DIASEM(46342;3)
DIASEM(0)
DIASEM(1)
DIASEM(FECHA(2026;11;22);2)
FECHA(AÑO(46342);MES(46342)+1;1)-1
ESNUMERO(5)
ESNUMERO("5")
ESNUMERO(A1)
ESTEXTO("a")
ESTEXTO(1&2)
ESBLANCO(A1)
ESBLANCO("")
ESBLANCO(0)
SI.ERROR(1/0;7)
SI.ERROR(5;7)
SI.ERROR(1/0;"mal")
SI.ERROR(RAIZ(-1);1/0)
SI.ERROR(SI.ERROR(1/0;2/0);3)
SI.ERROR(1/0)
SI.ERROR((1;2);9)
SI.ERROR("a;b)";1/0)
SI.ERROR(BM1;4)
SI.ERROR(1/0;2)+SI.ERROR(3;4)
1/0+SI.ERROR(1;2)
IZQUIERDA("Hispa";2)
IZQUIERDA("Hispa")
IZQUIERDA("Hispa";99)
IZQUIERDA("Hispa";0)
IZQUIERDA("Hispa";-1)
DERECHA("Hispa";3)
DERECHA("Hispa")
DERECHA("Hispa";99)
DERECHA(12345;2)
EXTRAE("Hispa Ventanas";7;3)
EXTRAE("Hispa";2;99)
EXTRAE("Hispa";9;2)
EXTRAE("Hispa";0;2)
EXTRAE("Hispa";1;0)
EXTRAE("Hispa";2)
ENCONTRAR("pa";"Hispa")
ENCONTRAR("PA";"Hispa")
ENCONTRAR("a";"banana";3)
ENCONTRAR("a";"banana";7)
ENCONTRAR("";"abc")
ENCONTRAR("";"abc";4)
ENCONTRAR("";"abc";5)
ENCONTRAR("ana";"banana";3)
ENCONTRAR("x";"abc")
ESPACIOS("  uno   dos  tres ")
ESPACIOS("")
ESPACIOS("   ")
LARGO(ESPACIOS(" a  b "))
VALOR("12,5")
VALOR("16/11/2026")
VALOR("doce")
VALOR(7)
VALOR("")
TEXTO(1234,567;"0,00")
TEXTO(1234,567;"#.##0,00")
TEXTO(1234,567;"0")
TEXTO(-1234567,5;"#.##0")
TEXTO(0,215;"0%")
TEXTO(0,215;"0,0%")
TEXTO(46342;"dd/mm/aaaa")
TEXTO(-1;"dd/mm/aaaa")
TEXTO(5;"0,000000000000")
TEXTO(92233720368;"0%")
TEXTO(5)
"Total: "&TEXTO(1234,5;"#.##0,00")&" EUR"
PRODUCTO(2;3;4)
PRODUCTO(1,5;"a";2)
PRODUCTO()
PRODUCTO(100000;100000;100000)
MEDIANA(3;1;2)
MEDIANA(4;1;3;2)
MEDIANA(5)
MEDIANA(-1;-5;7;0,5)
MEDIANA("a")
VAR(1;2;3;4)
VAR(5)
VAR(2;2;2)
DESVEST(1;2;3;4)
DESVEST(10;12;23;23;16;23;21;16)
DESVEST(1)
VAR(0,1;0,2;0,35)
MEDIANA(A1:C6)
PRODUCTO(A1:C6)
MODA(1;2;2;3;3)
MODA(3;3;2;2;1)
MODA(1;2;3)
MODA(5;5)
MODA(0,1;0,10;7)
MODA()
MODA(A1:C6)'''.split('\n')

# hojas enteras: celdas y lo que se pregunta
HOJAS = [
    # una factura
    ['A1 2', 'B1 19,99', 'C1 =A1*B1', 'A2 1,5', 'B2 33,33', 'C2 =A2*B2', 'A3 1', 'B3 -10', 'C3 =A3*B3',
     'C5 =SUMA(C1:C3)', 'C6 =REDONDEAR(C5*21%;2)', 'C7 =C5+C6', 'C8 =promedio(c1:c3)', 'C9 =MAX(C1:C3)',
     'C10 =MIN(C1:C3)', 'C11 =CONTAR(A1:C3)', 'C12 =SUMA(A1:B3;C5;C7)'],
    # textos, vacías y errores en los rangos
    ['A1 5', 'A2 texto', 'A4 -3', 'A5 =A1*2', 'A6  7 ', 'A7 - 7', 'A8 1.234,5', 'A9 +4',
     'B1 =SUMA(A1:A9)', 'B2 =PROMEDIO(A1:A9)', 'B3 =CONTAR(A1:A9)', 'B4 =MAX(A1:A9)', 'B5 =MIN(A1:A9)',
     'B6 =A2+1', 'B7 =A3+1', 'B8 =PROMEDIO(D1:D9)', 'B9 =MAX(D1:D9)', 'B10 =CONTAR(D1:D9)',
     'B11 =SUMA(A9:A1)', 'B12 =SUMA(B1:A1)', 'B13 =A8*2', 'B14 =A7', 'B15 =A6*2'],
    # circulares y cadenas
    ['A1 =A1', 'A2 =A3', 'A3 =A2', 'A4 =SUMA(A4:A5)', 'A5 1', 'A6 =A5+A5+A5', 'A7 =A1+1', 'A8 =1/0', 'A9 =A8+A1',
     'A10 =A1+A8', 'A11 =SUMA(A5:A8)', 'A12 =Z101+A1', 'A13 =A5/A99'],
    # una cadena más honda de lo que se admite
    ['A1 1'] + ['%s%d =%s%d+1' % (chr(65 + i // 100), i % 100 + 1, chr(65 + (i - 1) // 100), (i - 1) % 100 + 1)
                for i in range(1, 450)],
    # dinero: sumas largas de céntimos, exactas
    ['A%d 0,1' % i for i in range(1, 101)] + ['B1 =SUMA(A1:A100)', 'B2 =B1=10', 'B3 =PROMEDIO(A1:A100)',
                                               'B4 =SUMA(A1:A100)*3-30'],
    # una lista de la compra: criterios, textos y comparaciones
    ['A1 pan', 'B1 2', 'C1 1,2', 'A2 Vino', 'B2 1', 'C2 7,5', 'A3 PAN', 'B3 3', 'C3 1,25', 'A4 leche', 'B4 6', 'C4 0,89',
     'A5 queso', 'B5 -1', 'C5 12', 'A7 total', 'B6 texto',
     'D1 =B1*C1', 'D2 =B2*C2', 'D3 =B3*C3', 'D4 =B4*C4', 'D5 =B5*C5',
     'E1 =CONTAR.SI(A1:A5;"pan")', 'E2 =SUMAR.SI(A1:A5;"pan";D1:D5)', 'E3 =CONTAR.SI(B1:B6;">1")',
     'E4 =SUMAR.SI(B1:B6;">=2")', 'E5 =SUMAR.SI(B1:B6;"<0";C1:C6)', 'E6 =CONTAR.SI(B1:B6;"<>1")',
     'E7 =CONTAR.SI(A1:A7;"<>pan")', 'E8 =CONTAR.SI(B1:B5;B2)', 'E9 =SUMAR.SI(B1:B5;3;D1)', 'E10 =CONTAR.SI(A1:B7;"")',
     'E11 =CONTAR.SI(A1:A7;">=q")', 'E12 =SUMAR.SI(A1:A5;"=vino";B1:B5)', 'E13 =CONTAR.SI(B1:B6;"<=1")',
     'E14 =SUMAR.SI(D1:D5;"<"&5)', 'E15 =CONTAR.SI(A1:A5;A3)', 'E16 =SUMAR.SI(A1:A5;"pan";BK1:BL5)',
     'F1 =SI(D1>D2;"caro";"barato")', 'F2 =SI(A1=A3;1;0)', 'F3 =A1&" y "&A2', 'F4 =LARGO(A5)&" letras"',
     'F5 =MAYUSC(A4)', 'F6 =F5&F5', 'F7 =SI(B6="texto";F3;0)', 'F8 =CONTARA(A1:B7)', 'F9 =CONTAR(A1:B7)',
     'F10 =SI(B7="";"vacía";"llena")', 'F11 =B7=0', 'F12 =B7', 'F13 =B7&"x"', 'F14 =F3=F3', 'F15 =F1&F2',
     'F16 =SUMA(F1:F15)', 'F17 =CONTARA(F1:F15)', 'F18 =MAX(B1:B6;10)', 'F19 =F5+1', 'F20 =SI(F1="CARO";F5;F4)',
     'G1 =SI(G1;1;2)', 'G2 =SI(1;5;G2)', 'G3 =F3&F4&F5&F6', 'G4 =Y(A1="pan";B1=2;C1<2)', 'G5 =O(A1="x";F12)',
     'G6 =$B$1+B$2+$B3', 'G7 =SUMA($B$1:B$5)', 'G8 =ENTERO(C2)+RESIDUO(B4;4)', 'G9 =RAIZ(B4*B4)^2',
     'G10 =CONCATENAR(A1;B1;C1)', 'G11 =MINUSC(G10)&MAYUSC(G10)', 'G12 =NO(F11)', 'G13 =LARGO(G3)'],
    # fechas: lo tecleado como día/mes/año es un número de día
    ['A1 16/11/2026', 'A2 1/1/1900', 'A3 29/2/2024', 'A4 29/2/2023', 'A5 31/12/9999', 'A6 1/3/1900', 'A7 31/4/2026',
     'A8 1/13/2026', 'A9 16/11/26', 'A10 16/11/2026 ', 'A11  1/1/2000', 'A12 16/11/2026x', 'A13 0/1/2026',
     'A14 31/12/1899', 'A15 28/2/1900', 'A16 29/2/1900', 'A17 1/1/10000', 'A18 123/1/2026', 'A19 -1/1/2026',
     'B1 =A1+1', 'B2 =A1-A3', 'B3 =HOY()-A1', 'B4 =A4', 'B5 =A2', 'B6 =A3-A6', 'B7 =SI(A1>A3;"después";"antes")',
     'B8 =A7&""', 'B9 =A1&""', 'B10 =ENTERO(A1/7)', 'B11 =RESIDUO(A1;7)', 'B12 =CONTAR(A1:A19)', 'B13 =MAX(A1:A19)'],
    # una cadena de textos y un texto que crece hasta el tope
    ['A1 ab'] + ['A%d =A%d&A%d' % (i, i - 1, i - 1) for i in range(2, 11)] + ['B1 =LARGO(A10)', 'B2 =LARGO(A7)',
     'B3 =A3', 'B4 =A8=A8', 'B5 =MAYUSC(A9)=A9'],
    # columnas de dos letras y el borde de la hoja
    ['AA1 5', 'AB1 7', 'BL1000 9', 'AZ50 =AA1*AB1', 'BA1 =SUMA(AA1:AB1)', 'BL1 =BL1000+aa1', 'A1 =SUMA(A2:BL1000)',
     'B2 =BM1', 'B3 =A1001', 'B4 =SUMA(A1:BM1)', 'B5 =SUMA(AA1:AB1001)', 'B6 =ZZ1', 'B7 =SUMA(BL1000:BL1000)',
     'B8 =CONTAR.SI(AA1:BL1;">5")', 'B9 =$BL$1000', 'B10 =AAA1', 'B11 =A$1$', 'B12 =$$A1', 'B13 =$1'],
    # una tabla para buscar: códigos (ordenados), nombres y precios
    ['A1 10', 'B1 tornillo', 'C1 0,05', 'A2 20', 'B2 tuerca', 'C2 0,03', 'A3 30', 'B3 arandela', 'C3 0,01',
     'A4 40', 'B4 clavo', 'C4 0,02', 'A6 60', 'B6 broca', 'C6 2,5', 'E1 uno', 'F1 dos', 'G1 tres', 'E2 1', 'F2 2', 'G2 3',
     'D1 =BUSCARV(20;A1:C6;2)', 'D2 =BUSCARV(25;A1:C6;2)', 'D3 =BUSCARV(25;A1:C6;2;0)', 'D4 =BUSCARV(5;A1:C6;2)',
     'D5 =BUSCARV(99;A1:C6;3)', 'D6 =BUSCARV(40;A1:C6;4)', 'D7 =BUSCARV(40;A1:C6;0)', 'D8 =BUSCARV("clavo";B1:C6;2;FALSO)',
     'D9 =BUSCARV("CLAVO";B1:C6;2;0)', 'D10 =BUSCARV(50;A1:C6;2)', 'D11 =BUSCARV(60;A1:C6;3;0)*2',
     'D12 =BUSCARH("dos";E1:G2;2;0)', 'D13 =BUSCARH("tres";E1:G2;1;0)', 'D14 =BUSCARH("cuatro";E1:G2;2;0)',
     'D15 =COINCIDIR(30;A1:A6;0)', 'D16 =COINCIDIR(35;A1:A6)', 'D17 =COINCIDIR(35;A1:A6;0)', 'D18 =COINCIDIR("dos";E1:G1;0)',
     'D19 =COINCIDIR(60;A1:A6;1)', 'D20 =COINCIDIR(1;A1:A6;2)', 'D21 =INDICE(A1:C6;2;2)', 'D22 =INDICE(A1:C6;6;3)',
     'D23 =INDICE(A1:C6;7;1)', 'D24 =INDICE(E1:G1;3)', 'D25 =INDICE(A1:A6;3)', 'D26 =INDICE(A1:C6;0;1)',
     'D27 =INDICE(B1:B6;COINCIDIR(40;A1:A6;0))', 'D28 =BUSCARV(D15;A1:C6;2)', 'D29 =INDICE(A1:C6;5;2)&"x"',
     'D30 =SI.ERROR(BUSCARV(77;A1:C6;2;0);"no está")', 'D31 =BUSCARV(20;A1:C6)', 'D32 =BUSCARV(20;5;1)',
     'D33 =MEDIANA(A1:A6)', 'D34 =DESVEST(A1:A6)', 'D35 =VAR(C1:C6)', 'D36 =PRODUCTO(A1:A3)', 'D37 =ESBLANCO(A5)',
     'D38 =ESNUMERO(D1)', 'D39 =ESTEXTO(D1)', 'D40 =SI.ERROR(D40;1)', 'D41 =IZQUIERDA(B1;4)&DERECHA(B2;2)&EXTRAE(B3;2;3)',
     'D42 =ENCONTRAR("la";B4)', 'D43 =BUSCARV("";B1:C6;2;0)', 'D44 =COINCIDIR(A5;A1:A6;0)'],
]

# números con formato: (formato de la celda, lo tecleado). El formato: la
# clase (1 fijo, 2 millares, 3 moneda, 4 por cien, 5 fecha) por 128 y los
# decimales por 1024
# un libro de tres hojas (Hoja1, Hoja2 y Hoja3): lo que tiene cada una
LIBRO = [
    ['A1 3', 'A2 =Hoja2!A3', 'A3 =SUMA(Hoja2!A1:A3)', 'A4 =hoja2!a1+HOJA2!$A$2', 'A5 =Hoja9!A1', 'A6 =Hoja2!B1&"x"',
     'A7 =CONTAR.SI(Hoja2!A1:B3;">5")', 'A8 =SUMAR.SI(Hoja2!A1:A3;">5";Hoja3!A1:A3)', 'A9 =BUSCARV(7;Hoja2!A1:A3;1;0)',
     'A10 =INDICE(Hoja2!A1:A3;2)', 'A11 =Hoja2!Z1001', 'A12 =Hoja2!', 'A13 =SUMA(Hoja3!A1:A2;Hoja2!A1)',
     'A14 =Hoja2!C1', 'A15 =PROMEDIO(Hoja2!A1:A2;A1)', 'A16 =Hoja1!A1+1', 'A17 =SUMA(Hoja2!A1:Hoja2!A2)',
     'A18 =Hoja2!A1:A2', 'A19 =MAX(Hoja3!A1:B3)-MIN(Hoja2!A1:A3)', 'A20 =COINCIDIR(12;Hoja2!A1:A3;0)',
     'A21 =SUMAR.SI(A1:A3;">1";Hoja2!A1:A3)', 'A22 =SI(Hoja2!A1>4;Hoja3!A1;Hoja9!A1)', 'A23 =Hoja2 !A1',
     'A24 =CONTARA(Hoja2!A1:C3)', 'A25 =Hoja3!B2', 'B1 =Hoja3!A3'],
    ['A1 5', 'A2 7', 'A3 =A1+A2', 'B1 texto', 'C1 =Hoja1!A1*2', 'C2 =SUMA(A1:A3)+Hoja3!A1'],
    ['A1 =Hoja2!A3*10', 'A2 =Hoja3!A1', 'A3 =Hoja1!B1', 'B2 =Hoja1!A25'],
]

FORMATOS = [(c * 128 + d * 1024, t) for c, d, t in [
    (1, 2, '3,14159'), (1, 0, '2,5'), (1, 0, '-2,5'), (1, 8, '1/3'), (1, 2, '-0,001'), (1, 2, '1234567,891'),
    (1, 3, '12345,6789'), (1, 4, '99999,99995'), (1, 9, '1'), (1, 2, '0'), (1, 1, '0,05'), (1, 1, '0,04'),
    (2, 2, '1234,5'), (2, 0, '1234567'), (2, 2, '-1234,5'), (2, 0, '999'), (2, 0, '1000'), (2, 0, '-1000'),
    (2, 1, '12345,67'), (2, 2, '123456,7'), (2, 0, '12345678'), (2, 0, '123456789'), (2, 3, '0,5'),
    (3, 2, '12,5'), (3, 2, '1234,5'), (3, 0, '1234'), (3, 2, '-3'), (3, 0, '123456'), (3, 2, '0'),
    (4, 0, '0,21'), (4, 2, '0,21'), (4, 1, '1/3'), (4, 0, '1'), (4, 0, '-0,5'), (4, 0, '92233720368'),
    (4, 2, '12,3456'), (4, 0, '922337203'), (4, 0, '922337204'), (4, 0, '0,005'), (4, 0, '0,004'),
    (5, 0, '16/11/2026'), (5, 0, '1'), (5, 0, '0'), (5, 0, '-1'), (5, 0, '61'), (5, 0, '2958465'), (5, 0, '2958466'),
    (5, 0, '36526,75'), (5, 2, '29/2/2024'), (5, 0, 'HOY()'), (5, 0, '1/3/1900'), (5, 0, '31/12/1999'),
    (6, 0, '5'), (7, 2, '5'), (0, 2, '2,50'), (0, 0, '1/3'), (1, 2, '"texto"'), (3, 2, '1/0'),
]]


def main():
    cuantas = int(sys.argv[1]) if len(sys.argv) > 1 else 6000
    random.seed(20261116)

    def n_azar():
        k = random.random()
        if k < 0.3:
            return str(random.randint(0, 999))
        if k < 0.6:
            return '%d,%0*d' % (random.randint(0, 9999), random.randint(1, 4), random.randint(0, 9999))
        if k < 0.8:
            return '%d,%d' % (random.randint(0, 99999999), random.randint(0, 99999999))
        if k < 0.9:
            return '0,%0*d' % (random.randint(1, 10), random.randint(1, 99))
        return str(random.randint(0, 99999999999))

    def otra():
        """delante de una celda o de un rango: a veces, otra hoja del libro"""
        return random.choice(['Hoja2!', 'hoja3!', 'HOJA1!']) if random.random() < 0.2 else ''

    def e_azar(prof):
        if prof == 0 or random.random() < 0.3:
            k = random.random()
            if k < 0.12:
                return otra() + random.choice('ABC') + str(random.randint(1, 6))
            return ('-' if k < 0.25 else '') + n_azar() + ('%' if k > 0.95 else '')
        k = random.random()
        if k < 0.06:
            return '%s(%s%s%d:%s%d)' % (random.choice(['SUMA', 'PROMEDIO', 'MAX', 'MIN', 'CONTAR']), otra(),
                                        random.choice('ABC'), random.randint(1, 6), random.choice('ABC'), random.randint(1, 6))
        if k < 0.10:
            return 'REDONDEAR(%s;%d)' % (e_azar(prof - 1), random.randint(0, 8))
        if k < 0.13:
            return 'ABS(%s)' % e_azar(prof - 1)
        if k < 0.30:
            a, b, c = e_azar(prof - 1), e_azar(prof - 1), e_azar(prof - 1)
            rango = '%s%s%d:%s%d' % (otra(), random.choice('ABC'), random.randint(1, 6), random.choice('ABC'), random.randint(1, 6))
            return random.choice([
                lambda: 'SI(%s%s%s;%s;%s)' % (a, random.choice(['=', '<>', '<', '>', '<=', '>=']), b, c, e_azar(prof - 1)),
                lambda: 'ENTERO(%s)' % a,
                lambda: 'RAIZ(ABS(%s))' % a,
                lambda: 'RESIDUO(%s;%s)' % (a, b),
                lambda: 'POTENCIA(%s;%d)' % (a, random.randint(-3, 6)),
                lambda: '(%s)^%s' % (a, random.choice(['2', '3', '0,5', '-1', '1,5', '0', '10'])),
                lambda: 'Y(%s;%s)+O(%s;%s)+NO(%s)' % (a, b, b, c, a),
                lambda: '(%s%s%s)' % (a, random.choice(['=', '<>', '<', '>', '<=', '>=']), b),
                lambda: 'CONTAR.SI(%s;"%s%s")' % (rango, random.choice(['', '=', '<>', '<', '>', '<=', '>=']),
                                                  random.choice(['texto', n_azar(), '0', '-5'])),
                lambda: 'SUMAR.SI(%s;"%s%s";%s)' % (rango, random.choice(['<>', '<', '>', '<=', '>=']), n_azar(), rango),
                lambda: 'LARGO(%s&"x")' % a,
                lambda: 'LARGO(CONCATENAR(%s;MAYUSC("añ");%s))' % (a, b),
                lambda: 'SI(%s&""=%s&"";%s;A1)' % (a, b, c),
                lambda: 'MAX(%s;%s;%s)-MIN(%s;%s)' % (a, rango, b, c, rango),
                lambda: 'CONTARA(%s;%s)' % (rango, a),
                lambda: '%s(%s;%d)' % (random.choice(['TRUNCAR', 'REDONDEAR.MAS', 'REDONDEAR.MENOS']), a, random.randint(0, 8)),
                lambda: 'SIGNO(%s)*PI()' % a,
                lambda: '%s(%s;%s;%s)' % (random.choice(['MEDIANA', 'VAR', 'DESVEST', 'PRODUCTO', 'MODA']), a, rango, b),
                lambda: 'SI.ERROR(%s;%s)' % (a, b),
                lambda: 'PAGO((%s)%%;%d;%s)' % (a, random.randint(0, 40), b),
                lambda: 'DIASEM(ABS(%s))+MES(ABS(%s))+DIA(ABS(%s))' % (a, a, b),
                lambda: 'SI.ERROR(BUSCARV(%s;%s;%d;%d);-1)' % (a, rango, random.randint(1, 3), random.randint(0, 1)),
                lambda: 'SI.ERROR(COINCIDIR(%s;%s;%d);INDICE(%s;%d;%d))' % (a, rango, random.randint(0, 1), rango,
                                                                        random.randint(1, 4), random.randint(1, 3)),
                lambda: 'LARGO(TEXTO(%s;"%s"))' % (a, random.choice(['0', '0,00', '#.##0,00', '0%', '0,0'])),
                lambda: 'VALOR(IZQUIERDA(TEXTO(ABS(%s);"0,00");3))+ESNUMERO(%s)' % (a, b),
                lambda: 'ENCONTRAR(",";TEXTO(%s;"0,000"))' % a,
            ])()
        a, b = e_azar(prof - 1), e_azar(prof - 1)
        s = a + random.choice('+-*/*') + b
        return '(' + s + ')' if random.random() < 0.4 else s

    entrada, casos = [], []           # casos: (rótulo, hoja, col, fil)

    def pregunta(h, rot, col, fil):
        entrada.append('?%s%d' % (colnom(col), fil + 1))
        casos.append((rot, h, col, fil))

    def pon(h, linea):
        entrada.append(linea)
        ref, _, t = linea.partition(' ')
        m = REF.fullmatch(ref)
        h.c[(colnum(m.group(2)), int(m.group(4)) - 1)] = t

    # (cada pregunta se contesta con la hoja como está en ese momento: el
    # modelo se evalúa a la vez que se prepara la entrada)
    esperado = []

    def resuelve(h, col, fil, fmt=0):
        try:
            v = h.valor(col, fil)
            return 'V' if v is None else 'T' + v if isinstance(v, str) else '%016X %s' % (v & (2**64 - 1), celda_fmt(v, fmt))
        except Err as e:
            return 'E%d' % e.n
        except RecursionError:
            return 'E1'

    for f in SUELTAS:
        h = Hoja()
        entrada.append('!')
        pon(h, 'Z100 =' + f)
        pregunta(h, '=' + f, 25, 99)
        esperado.append(resuelve(h, 25, 99))
    for hoja in HOJAS:
        h = Hoja()
        entrada.append('!')
        for l in hoja:
            pon(h, l)
        for (col, fil) in sorted(h.c):
            pregunta(h, '%s%d «%s»' % (colnom(col), fil + 1, h.c[(col, fil)]), col, fil)
            esperado.append(resuelve(h, col, fil))
    # un libro de tres hojas: celdas y rangos de otra hoja
    libro = []
    libro += [Hoja(libro, n) for n in range(3)]
    entrada.append('!')
    for n, lineas in enumerate(LIBRO):
        entrada.append('@%d' % (n + 1))
        for l in lineas:
            pon(libro[n], l)
    for n in range(3):
        entrada.append('@%d' % (n + 1))
        for (col, fil) in sorted(libro[n].c):
            pregunta(libro[n], 'Hoja%d!%s%d «%s»' % (n + 1, colnom(col), fil + 1, libro[n].c[(col, fil)]), col, fil)
            esperado.append(resuelve(libro[n], col, fil))
    # los formatos de número
    h = Hoja()
    entrada.append('!')
    for fmt, t in FORMATOS:
        entrada.append('%%%d' % fmt)
        pon(h, 'C3 ' + (t if re.fullmatch(r'-?[0-9,]+|[0-9]+/[0-9]+/[0-9]+', t) else '=' + t))
        pregunta(h, 'formato %d: «%s»' % (fmt, h.c[(2, 2)]), 2, 2)
        esperado.append(resuelve(h, 2, 2, fmt))
    entrada.append('%0')
    # cada celda suma las dos de antes: sin apuntar lo calculado no acabaría
    # nunca (y el modelo, que no apunta, tampoco: aquí el resultado va a mano)
    entrada.append('!')
    entrada += ['A1 1', 'A2 1'] + ['A%d =A%d+A%d' % (i, i - 1, i - 2) for i in range(3, 51)] + ['?A50']
    casos.append(('Fibonacci: A50', None, 0, 49))
    esperado.append('%016X %s' % (12586269025 * ESC, celda(12586269025 * ESC)))
    # y una cadena de 2.600 celdas preguntada de abajo arriba: como cada una
    # queda apuntada, la última sale aunque esté a 2.599 fórmulas de la primera
    entrada.append('!')
    orden = [(c, f) for c in range(26) for f in range(100)]
    entrada.append('A1 1')
    for k in range(1, len(orden)):
        (c, f), (c0, f0) = orden[k], orden[k - 1]
        entrada.append('%s%d =%s%d+1' % (chr(65 + c), f + 1, chr(65 + c0), f0 + 1))
    for k in range(0, len(orden), 300):
        entrada.append('??%s%d' % (chr(65 + orden[k][0]), orden[k][1] + 1))
        casos.append(('cadena: la celda %d' % (k + 1), None, 0, 0))
        esperado.append('%016X %s' % ((k + 1) * ESC, celda((k + 1) * ESC)))
    entrada.append('??Z100')
    casos.append(('cadena: la última', None, 0, 0))
    esperado.append('%016X %s' % (2600 * ESC, celda(2600 * ESC)))
    libro = []
    libro += [Hoja(libro, n) for n in range(3)]
    h = libro[0]
    entrada.append('!')
    for _ in range(cuantas):
        if random.random() < 0.15:      # de vez en cuando cambia una celda de datos
            col, fil = random.randint(0, 2), random.randint(0, 5)
            k = random.random()
            t = '' if k < 0.15 else 'texto' if k < 0.25 else ('-' if k < 0.4 else '') + n_azar()
            n = random.choice([0, 0, 0, 1, 2])      # (a veces, de otra hoja del libro)
            if n:
                entrada.append('@%d' % (n + 1))
            pon(libro[n], '%s%d %s' % (chr(65 + col), fil + 1, t))
            if n:
                entrada.append('@1')
        f = e_azar(random.randint(1, 3))
        pon(h, 'Z100 =' + f)
        pregunta(h, '=' + f, 25, 99)
        esperado.append(resuelve(h, 25, 99))

    with tempfile.TemporaryDirectory() as tmp:
        prog = hacer(tmp)
        sal = subprocess.run([prog], input=('\n'.join(entrada) + '\n').encode('cp437'),
                             capture_output=True).stdout.decode('cp437').split('\n')
    mal = 0
    for (rot, _, _, _), r, esp in zip(casos, sal, esperado):
        if os.environ.get('VER') and re.search(os.environ['VER'], rot):
            print('       %s  ->  %s' % (rot, r))
        if r != esp:
            mal += 1
            if mal <= 25:
                print('  FAIL %s  ->  %s   (esperado %s)' % (rot, r, esp))
    if len(sal) - 1 != len(casos):
        print('  FAIL el programa ha contestado %d líneas de %d' % (len(sal) - 1, len(casos)))
        mal += 1
    if mal == 0:
        print('  OK   %d celdas: valor y texto iguales que el modelo exacto' % len(casos))
        err = sum(1 for e in esperado if len(e) == 2 and e[0] == 'E')
        otras = sum(1 for e in esperado if e[0] in 'VT')
        print('       (%d con número, %d con error, %d de texto o vacías)' % (len(esperado) - err - otras, err, otras))
    return 1 if mal else 0


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    sys.exit(main())
