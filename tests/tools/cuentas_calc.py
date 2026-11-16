#!/usr/bin/env python3
"""Prueba del cálculo de Cuentas fuera de Hispa Ventanas.

uso: cuentas_calc.py [CUANTAS_AL_AZAR]

Toma programas/cuentas/calculo.inc TAL CUAL (el valor de las celdas, las
fórmulas, la coma fija de 64 bits y leer y escribir números), lo ensambla con
NASM dentro de un programa de Linux de 32 bits que hace de hoja (se le dan
celdas y se le preguntan valores por la entrada) y compara cada respuesta, el
valor en cienmillonésimas y el texto que se vería en la celda, con un modelo
exacto hecho aquí con enteros de Python: casos elegidos y miles de fórmulas
al azar.
"""
import os, random, re, subprocess, sys, tempfile

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
ESC, VER, CARS, NCOL, NFIL, PROF = 10**8, 4, 10, 26, 100, 400
LIM = 2**63

ARNES = r'''
        bits 32
        cpu 386
NCOL        equ 26
NFIL        equ 100
CEL_CARS    equ 10
CEL_MAX     equ 128
        section .text
        global _start
_start:
        cld
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
        cmp byte [esi], '!'         ; «!»: la hoja, vacía
        je .vaciar
        cmp byte [esi], '?'         ; «?B7»: lo que vale y cómo se ve
        je .pregunta
        call ref                    ; «B7 texto»: el texto de la celda
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
.vaciar:
        mov edi, cel_txt
        mov ecx, NCOL * NFIL * CEL_MAX
        xor al, al
        rep stosb
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
        call ref
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
        mov bl, 'T'
        cmp byte [cv_tipo], 2
        jne .val
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
        call num_fmt                ; y como se ve en la celda
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
; ESI=«B7» -> ECX=columna EDX=fila, y ESI detrás
ref:
        movzx ecx, byte [esi]
        sub ecx, 'A'
        inc esi
        xor edx, edx
.l:
        movzx eax, byte [esi]
        sub eax, '0'
        cmp eax, 9
        ja .f
        imul edx, 10
        add edx, eax
        inc esi
        jmp .l
.f:
        dec edx
        ret
cel_puntero:
        push eax
        mov eax, edx
        imul eax, NCOL
        add eax, ecx
        imul eax, CEL_MAX
        lea esi, [cel_txt+eax]
        pop eax
        ret
%include "calculo.inc"
        section .bss
lin         resb 512
salida         resb 128
v           resd 2
cel_txt     resb NCOL * NFIL * CEL_MAX
'''


def hacer(tmp):
    inc = open(os.path.join(RAIZ, 'programas', 'cuentas', 'calculo.inc'), encoding='utf-8').read()
    open(os.path.join(tmp, 'calculo.inc'), 'w', encoding='cp437').write(inc)
    asm = os.path.join(tmp, 'calc.asm')
    # (en .val, EAX se gasta al escribir el hexadecimal: num_fmt lo toma de v)
    open(asm, 'w', encoding='cp437').write(
        ARNES.replace('        mov eax, [v]\n        call num_fmt',
                      '        mov eax, [v]\n        mov edx, [vv]\n        call num_fmt')
             .replace('        mov [v+4], edx\n        mov ecx, 16',
                      '        mov [v+4], edx\n        mov [vv], edx\n        mov ecx, 16')
             .replace('v           resd 2', 'v           resd 2\nvv          resd 1'))
    subprocess.run([os.environ.get('NASM', 'nasm'), '-f', 'elf32', '-i', tmp + os.sep, '-o', asm + '.o', asm], check=True)
    subprocess.run(['ld', '-m', 'elf_i386', '-o', os.path.join(tmp, 'calc'), asm + '.o'], check=True)
    return os.path.join(tmp, 'calc')


# ---- el modelo: lo mismo, con enteros de Python ----
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


class Hoja:
    def __init__(self):
        self.c = {}

    def valor(self, col, fil, pila=(), prof=0):
        """-> (valor, tipo): 0 vacía, 1 número, 2 texto"""
        if not (0 <= col < NCOL and 0 <= fil < NFIL):
            raise Err(3)
        if (col, fil) in pila:
            raise Err(2)
        t = self.c.get((col, fil), '')
        if t == '':
            return 0, 0
        if t[0] != '=':
            s = t.strip(' ')
            neg = s[:1] == '-'
            if s[:1] in ('-', '+'):
                s = s[1:]
            r = lee_num(s, 0)
            if r is None or r[1] != len(s):
                return 0, 2
            # (espacios delante y detrás valen; entre el signo y el número, no)
            return (-r[0] if neg else r[0]), 1
        if prof >= PROF:
            raise Err()
        p = Formula(self, t[1:], pila + ((col, fil),), prof + 1)
        x = p.expr()
        if p.car() != '':
            raise Err()
        return x, 1


class Formula:
    def __init__(self, hoja, t, pila, prof):
        self.h, self.t, self.i, self.pila, self.prof = hoja, t, 0, pila, prof

    def car(self):
        while self.i < len(self.t) and self.t[self.i] == ' ':
            self.i += 1
        return self.t[self.i] if self.i < len(self.t) else ''

    def expr(self):
        x = self.term()
        while self.car() in ('+', '-') and self.car():
            op = self.car(); self.i += 1
            y = self.term()
            x = suma(x + y if op == '+' else x - y)
        return x

    def term(self):
        x = self.factor()
        while self.car() in ('*', '/') and self.car():
            op = self.car(); self.i += 1
            y = self.factor()
            x = self.mul(x, y) if op == '*' else self.div(x, y)
        return x

    @staticmethod
    def mul(x, y):
        return mag(rdiv(x * y, ESC))

    @staticmethod
    def div(x, y):
        if y == 0:
            raise Err()
        return mag(rdiv(x * ESC, y))

    def factor(self):
        c = self.car()
        if c == '-':
            self.i += 1
            return -self.factor()
        if c == '+':
            self.i += 1
            return self.factor()
        x = self.primario()
        while self.car() == '%':
            self.i += 1
            x = self.div(x, 100 * ESC)
        return x

    def cierra(self):
        if self.car() != ')':
            raise Err()
        self.i += 1

    def abre(self):
        if self.car() != '(':
            raise Err()
        self.i += 1

    def primario(self):
        c = self.car()
        if c == '(':
            self.i += 1
            x = self.expr()
            self.cierra()
            return x
        if c and (c in ',.' or c in '0123456789'):
            r = lee_num(self.t, self.i)
            if r is None:
                raise Err()
            self.i = r[1]
            return r[0]
        if c and c.isascii() and c.isalpha():
            return self.nombre()
        raise Err()

    def nombre(self):
        j = self.i
        while j < len(self.t) and self.t[j].isascii() and self.t[j].isalpha():
            j += 1
        nom = self.t[self.i:j].upper()
        if len(nom) == 1 and j < len(self.t) and self.t[j] in '0123456789':
            col, fil = self.ref()
            return self.h.valor(col, fil, self.pila, self.prof)[0]
        self.i = j
        if nom in ('SUMA', 'PROMEDIO', 'MAX', 'MIN', 'CONTAR'):
            return self.rangos(nom)
        if nom == 'ABS':
            self.abre()
            x = self.expr()
            self.cierra()
            return abs(x)
        if nom == 'REDONDEAR':
            self.abre()
            x = self.expr()
            if self.car() != ';':
                raise Err()
            self.i += 1
            d = self.expr()
            self.cierra()
            if d < 0 or d % ESC or d // ESC > 8:
                raise Err()
            p = 10 ** (8 - d // ESC)
            r = rdiv(abs(x), p) * p
            if r >= LIM:
                raise Err()
            return -r if x < 0 else r
        raise Err()

    def ref(self):
        c = self.car()
        if not (c and c.isascii() and c.isalpha()):
            raise Err()
        col = ord(c.upper()) - 65
        self.i += 1
        j = self.i
        while j < len(self.t) and self.t[j] in '0123456789':
            j += 1
        if j == self.i:
            raise Err()
        fil = int(self.t[self.i:j]) - 1
        self.i = j
        if not 0 <= fil < NFIL:
            raise Err(3)
        return col, fil

    def rangos(self, nom):
        n, s, ext = 0, 0, 0
        self.abre()
        while True:
            c1, f1 = self.ref()
            c2, f2 = c1, f1
            if self.car() == ':':
                self.i += 1
                c2, f2 = self.ref()
            c1, c2 = min(c1, c2), max(c1, c2)
            f1, f2 = min(f1, f2), max(f1, f2)
            for f in range(f1, f2 + 1):
                for c in range(c1, c2 + 1):
                    v, tipo = self.h.valor(c, f, self.pila, self.prof)
                    if tipo != 1:
                        continue
                    n += 1
                    s = suma(s + v)
                    if n == 1 or (nom == 'MAX' and v > ext) or (nom == 'MIN' and v < ext):
                        ext = v
            if self.car() != ';':
                break
            self.i += 1
        self.cierra()
        if nom == 'SUMA':
            return s
        if nom == 'PROMEDIO':
            return self.div(s, n * ESC)
        if nom == 'CONTAR':
            return n * ESC
        return ext


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
-3%'''.split('\n')

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
]


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

    def e_azar(prof):
        if prof == 0 or random.random() < 0.3:
            k = random.random()
            if k < 0.12:
                return random.choice('ABC') + str(random.randint(1, 6))
            return ('-' if k < 0.25 else '') + n_azar() + ('%' if k > 0.95 else '')
        k = random.random()
        if k < 0.06:
            return '%s(%s%d:%s%d)' % (random.choice(['SUMA', 'PROMEDIO', 'MAX', 'MIN', 'CONTAR']),
                                      random.choice('ABC'), random.randint(1, 6), random.choice('ABC'), random.randint(1, 6))
        if k < 0.10:
            return 'REDONDEAR(%s;%d)' % (e_azar(prof - 1), random.randint(0, 8))
        if k < 0.13:
            return 'ABS(%s)' % e_azar(prof - 1)
        a, b = e_azar(prof - 1), e_azar(prof - 1)
        s = a + random.choice('+-*/*') + b
        return '(' + s + ')' if random.random() < 0.4 else s

    entrada, casos = [], []           # casos: (rótulo, hoja, col, fil)

    def pregunta(h, rot, col, fil):
        entrada.append('?%s%d' % (chr(65 + col), fil + 1))
        casos.append((rot, h, col, fil))

    def pon(h, linea):
        entrada.append(linea)
        ref, _, t = linea.partition(' ')
        h.c[(ord(ref[0]) - 65, int(ref[1:]) - 1)] = t

    # (cada pregunta se contesta con la hoja como está en ese momento: el
    # modelo se evalúa a la vez que se prepara la entrada)
    esperado = []

    def resuelve(h, col, fil):
        try:
            v, tipo = h.valor(col, fil)
            return 'V' if tipo == 0 else 'T' if tipo == 2 else '%016X %s' % (v & (2**64 - 1), celda(v))
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
            pregunta(h, '%s%d «%s»' % (chr(65 + col), fil + 1, h.c[(col, fil)]), col, fil)
            esperado.append(resuelve(h, col, fil))
    # cada celda suma las dos de antes: sin apuntar lo calculado no acabaría
    # nunca (y el modelo, que no apunta, tampoco: aquí el resultado va a mano)
    entrada.append('!')
    entrada += ['A1 1', 'A2 1'] + ['A%d =A%d+A%d' % (i, i - 1, i - 2) for i in range(3, 51)] + ['?A50']
    casos.append(('Fibonacci: A50', None, 0, 49))
    esperado.append('%016X %s' % (12586269025 * ESC, celda(12586269025 * ESC)))
    # y una cadena de 2.600 celdas preguntada de abajo arriba: como cada una
    # queda apuntada, la última sale aunque esté a 2.599 fórmulas de la primera
    entrada.append('!')
    orden = [(c, f) for c in range(NCOL) for f in range(NFIL)]
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
    h = Hoja()
    entrada.append('!')
    for _ in range(cuantas):
        if random.random() < 0.15:      # de vez en cuando cambia una celda de datos
            col, fil = random.randint(0, 2), random.randint(0, 5)
            k = random.random()
            t = '' if k < 0.15 else 'texto' if k < 0.25 else ('-' if k < 0.4 else '') + n_azar()
            pon(h, '%s%d %s' % (chr(65 + col), fil + 1, t))
        f = e_azar(random.randint(1, 3))
        pon(h, 'Z100 =' + f)
        pregunta(h, '=' + f, 25, 99)
        esperado.append(resuelve(h, 25, 99))

    with tempfile.TemporaryDirectory() as tmp:
        prog = hacer(tmp)
        sal = subprocess.run([prog], input=('\n'.join(entrada) + '\n').encode('cp437'),
                             capture_output=True).stdout.decode().split('\n')
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
        err = sum(1 for e in esperado if len(e) == 2)
        otras = sum(1 for e in esperado if len(e) == 1)
        print('       (%d con número, %d con error, %d de texto o vacías)' % (len(esperado) - err - otras, err, otras))
    return 1 if mal else 0


if __name__ == '__main__':
    sys.setrecursionlimit(10000)
    sys.exit(main())
