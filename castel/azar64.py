#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""azar64.py SEMILLA CUANTAS SALIDA.cas ESPERADO.txt
Como azar32.py, pero para 64 bits («modelo linux64», CASTEL64.md): escribe un
programa Castel con CUANTAS cuentas al azar que mezclan nat64 y ent64 con los
tipos de 32, 16 y 8 bits (variables, elementos, campos, punteros, parámetros,
todos los operadores, comparaciones, conversiones, asignaciones) y, aparte, lo
que tiene que escribir, calculado aquí con las reglas de CASTEL64.md: la
cuenta es de 64 bits si alguno de los dos es de 64 (si no, de 32, como en el
modelo plano), y con signo si alguno de los dos lleva signo; lo de 32 bits o
menos se ensancha a 64 con su signo si lo lleva. Lo usa pruebas64.sh.
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior."""
import random, sys
random.seed(int(sys.argv[1]))
N = int(sys.argv[2])
PLANO = True
ANCHO = {'nat8': 8, 'nat16': 16, 'ent16': 16, 'nat32': 32, 'ent32': 32, 'nat64': 64, 'ent64': 64}
VARS = {'a': ('nat16', 40000), 'b': ('nat16', 7), 'e': ('ent16', -100), 'c': ('nat8', 200),
        'la': ('nat32', 3000000000), 'lb': ('nat32', 70000), 'lc': ('nat32', 5), 'lz': ('nat32', 0xFFFFFFFF),
        'sa': ('ent32', -100000), 'sb': ('ent32', 123456789), 'sc': ('ent32', -1),
        'sd': ('ent32', -2147483648),
        'wa': ('nat64', 0x123456789ABCDEF0), 'wb': ('nat64', 5000000000), 'wc': ('nat64', 3),
        'wz': ('nat64', 0xFFFFFFFFFFFFFFFF), 'ia': ('ent64', -7000000000), 'ib': ('ent64', 99),
        'ic': ('ent64', -1), 'id': ('ent64', -0x8000000000000000)}
T32 = [11, 500000, 4000000000, 65536]
T64 = [-3, 0x7FFFFFFFFFFFFFFF, 10000000000, 1 << 40]
def con_signo(t):
    return t in ('ent16', 'ent32', 'ent64')
def es64(t):
    return t in ('nat64', 'ent64')
def es32(t):
    return t in ('nat32', 'ent32', 'lit32')
def es_lit(t):
    return t in ('lit', 'lit32')
def ancha(t):
    """¿Se hace en 32 bits la cuenta? (en el modelo plano, todas)"""
    return PLANO or es32(t)
def corta(t, x):
    return x & ((1 << ANCHO[t]) - 1)
def num(t, v):
    """El número que es (un entero de Python), dados sus bits y su tipo."""
    if es_lit(t):
        return v
    n = ANCHO[t]
    v &= (1 << n) - 1
    return v - (1 << n) if con_signo(t) and v >> (n - 1) else v
def tipo_res(ta, tb):
    s = con_signo(ta) or con_signo(tb)
    if es64(ta) or es64(tb):
        return 'ent64' if s else 'nat64'
    if ancha(ta) or ancha(tb):
        return 'ent32' if s else 'nat32'
    return 'ent16' if s else 'nat16'
def visto(ta, tb, va, vb):
    """Los dos operandos como los ve una comparación o una división: en 32 bits
    si alguno lo es (si no, en 16), y con signo si alguno lo lleva."""
    n = 64 if es64(ta) or es64(tb) else 32 if ancha(ta) or ancha(tb) else 16
    x, y = num(ta, va) & ((1 << n) - 1), num(tb, vb) & ((1 << n) - 1)
    if con_signo(ta) or con_signo(tb):
        x = x - (1 << n) if x >> (n - 1) else x
        y = y - (1 << n) if y >> (n - 1) else y
    return x, y
def par(s):
    return s if s.replace('_', 'a').isalnum() else f"({s})"
def hoja(prof):
    r = random.random()
    if r < 0.45:
        n = random.choice(list(VARS))
        return n, VARS[n][0], corta(VARS[n][0], VARS[n][1])
    if r < 0.55:
        v = random.choice([0, 1, 2, 3, 7, 8, 10, 16, 100, 255, 256, 300, 1000])
        return str(v), 'lit', v
    if r < 0.63:
        v = random.choice([65536, 100000, 1000000, 0x12345678, 4000000000, 2147483648])
        return (hex(v) if v == 0x12345678 else str(v)), 'lit32', v
    if r < 0.68 and prof > 0:
        s, t, v = expr(prof - 1)
        return f"t32[{par(s)} yb 3]", 'nat32', T32[num(t, v) & 3]
    if r < 0.72 and prof > 0:
        s, t, v = expr(prof - 1)
        return f"t64[{par(s)} yb 3]", 'ent64', corta('ent64', T64[num(t, v) & 3])
    if r < 0.86 and prof > 0:
        s, t, v = expr(prof - 1)
        k = random.random()
        if k < 0.2:
            return f"igual32({s})", 'nat32', corta('nat32', num(t, v))
        if k < 0.4:
            return f"igual_s32({s})", 'ent32', corta('ent32', num(t, v))
        if k < 0.55:
            return f"igual64({s})", 'nat64', corta('nat64', num(t, v))
        if k < 0.7:
            return f"igual_s64({s})", 'ent64', corta('ent64', num(t, v))
        s2, t2, v2 = expr(prof - 1)
        s3, t3, v3 = hoja(0)
        x = corta('nat8', num(t, v))
        n = num('ent32', corta('ent32', num(t2, v2)))
        y = corta('nat16', num(t3, v3))
        if es_lit(t):                       # una constante que no cabe no se deja pasar
            s = f"nat8({s})"
        if es_lit(t3):
            s3 = f"nat16({s3})"
        return f"tres({s}, {s2}, {s3})", 'ent32', corta('ent32', n + x - y)
    n = random.choice(['r.largo', 'r.con', 'valor(p)', 'p[1]', 'r.corto', 'r.ancho', 'pw[2]', 'valor(pw)'])
    return {'r.largo': ('r.largo', 'nat32', 77000), 'r.con': ('r.con', 'ent32', corta('ent32', -2)),
            'valor(p)': ('valor(p)', 'nat32', 11), 'p[1]': ('p[1]', 'nat32', 500000),
            'r.corto': ('r.corto', 'nat8', 9), 'r.ancho': ('r.ancho', 'nat64', 1 << 33),
            'pw[2]': ('pw[2]', 'ent64', corta('ent64', T64[2])),
            'valor(pw)': ('valor(pw)', 'ent64', corta('ent64', T64[0]))}[n]
def expr(prof):
    if prof == 0 or random.random() < 0.15:
        return hoja(prof)
    r = random.random()
    if r < 0.1:
        s, t, v = expr(prof - 1)
        if es_lit(t):
            return s, t, v
        if random.random() < 0.5:
            tr = 'ent64' if es64(t) else 'ent32' if ancha(t) else 'ent16'
            return f"-{par(s)}", tr, corta(tr, -num(t, v))
        tr = t if es32(t) or es64(t) else (('ent32' if t == 'ent16' else 'nat32') if PLANO else ('ent16' if t == 'ent16' else 'nat16'))
        return f"nob {par(s)}", tr, corta(tr, ~num(t, v))
    if r < 0.2:
        s, t, v = expr(prof - 1)
        if es_lit(t):
            return s, t, v
        k = random.choice(['nat8', 'nat16', 'ent16', 'nat32', 'ent32', 'nat64', 'ent64', 'nat64', 'ent64'])
        return f"{k}({s})", k, corta(k, num(t, v))
    sa, ta, va = expr(prof - 1)
    sb, tb, vb = expr(prof - 1)
    if es_lit(ta) and es_lit(tb):
        sb, tb, vb = hoja(0)
        while es_lit(tb):
            sb, tb, vb = hoja(0)
    op = random.choice(['+', '-', '*', 'yb', 'ob', 'xob', '+', '-', '*', 'div', 'mod', 'div', 'mod', '<<', '>>'])
    A, B = par(sa), par(sb)
    if op in ('+', '-', '*', 'yb', 'ob', 'xob'):
        x, y = num(ta, va), num(tb, vb)
        v = {'+': x + y, '-': x - y, '*': x * y, 'yb': x & y, 'ob': x | y, 'xob': x ^ y}[op]
        tr = tipo_res(ta, tb)
        return f"{A} {op} {B}", tr, corta(tr, v)
    if op in ('div', 'mod'):
        # el divisor, nunca cero. Si la cuenta es de 16 bits, pequeño y positivo
        # (para que no desborde con signo); si es de 32, lo que salga
        k = random.random()
        if k < 0.4 or es_lit(tb):
            vb = random.choice([1, 2, 3, 4, 7, 8, 10, 16, 256, 300, 65536, 100000, 1000000007])
            B, tb = str(vb), ('lit' if vb <= 0xFFFF else 'lit32')
        elif k < 0.7 or not (es32(tb) or es64(tb)):
            B, vb = f"({B} yb 127 ob 1)", (num(tb, vb) & 127) | 1
            tb = tipo_res(tb, 'lit')
        else:
            B, vb = f"({B} ob 1)", corta(tb, num(tb, vb) | 1)
        if es_lit(ta) and es_lit(tb):
            return hoja(0)
        x, y = visto(ta, tb, va, vb)
        if y == -1:                         # (el más negativo entre -1 no cabe: el procesador se para)
            B, tb, vb = '7', 'lit', 7
            x, y = visto(ta, tb, va, vb)
        q = abs(x) // abs(y) * (1 if (x < 0) == (y < 0) else -1)
        m = x - q * y
        tr = tipo_res(ta, tb)
        return f"{A} {op} {B}", tr, corta(tr, q if op == 'div' else m)
    # desplazamientos: manda el tipo de lo que se desplaza
    tope = 63 if es64(ta) else 31 if ancha(ta) else 15
    if random.random() < 0.6:
        n = random.randint(0, tope)
        B = str(n)
        if es_lit(ta):
            return hoja(0)
    else:
        n = num(tb, vb) & tope
        B = f"({B} yb {tope})"
    tr = ta if es64(ta) else ('ent32' if ta in ('ent32', 'ent16') else 'nat32') if ancha(ta) else ('ent16' if ta == 'ent16' else 'nat16')
    x = num(ta, va)
    if op == '<<':
        v = x << n
    elif con_signo(ta):
        v = x >> n
    else:
        v = corta(tr, x) >> n
    return f"{A} {op} {B}", tr, corta(tr, v)
lineas = []
esperado = []
i = 0
while i < N:
    s, t, v = expr(random.randint(1, 4))
    if es_lit(t):
        continue
    i += 1
    if random.random() < 0.3:
        s2, t2, v2 = expr(random.randint(1, 3))
        if es_lit(t2):
            s2, t2, v2 = 'lb - 2', 'nat32', 69998
        op = random.choice(['=', '<>', '<', '<=', '>', '>='])
        x, y = visto(t, t2, v, v2)
        res = {'=': x == y, '<>': x != y, '<': x < y, '<=': x <= y, '>': x > y, '>=': x >= y}[op]
        lineas.append(f"        escribir({s} {op} {s2}, \"\\n\")")
        esperado.append('cierto' if res else 'falso')
        lineas.append(f"        si {s} {op} {s2} entonces escribir(\"s\") sino escribir(\"n\") fin")
        lineas.append("        escribir(\"\\n\")")
        esperado.append('s' if res else 'n')
    else:
        lineas.append(f"        escribir({s}, \"\\n\")")
        esperado.append(str(num(t, v)))
    if random.random() < 0.4:
        # guardar en variables de cada tipo y enseñarlas
        k = random.choice([('x8', 'nat8'), ('x16', 'nat16'), ('xs', 'ent16'), ('x32', 'nat32'), ('xs32', 'ent32'),
                           ('x32', 'nat32'), ('xs32', 'ent32'), ('t32[b - 5]', 'nat32'), ('r.otro', 'ent32'),
                           ('q[lc - 4]', 'ent32'), ('x64', 'nat64'), ('xs64', 'ent64'), ('r.ancho', 'nat64'),
                           ('t64[b - 4]', 'ent64'), ('pw[1]', 'ent64')])
        lineas.append(f"        {k[0]} := {s}")
        lineas.append(f"        escribir({k[0]}, \"\\n\")")
        if k[0].startswith('t32'):
            lineas.append("        t32[2] := 4000000000")
        if k[0] in ('r.ancho', 't64[b - 4]', 'pw[1]'):
            lineas.append({'r.ancho': "        r.ancho := nat64(1) << 33", 't64[b - 4]': "        t64[3] := ent64(1) << 40",
                           'pw[1]': "        pw[1] := ent64(0x7FFFFFFF) << 32 ob 0xFFFFFFFF"}[k[0]])
        esperado.append(str(num(k[1], corta(k[1], num(t, v)))))
def inicial(t, v):
    if es64(t):                             # (los números escritos son de 32 bits como mucho)
        return ''
    return f" = {v}"
decl = ''.join(f"    variable {n}: {'byte' if t == 'nat8' else t}{inicial(t, v)}\n" for n, (t, v) in VARS.items())
def pon64(n, v):
    """La asignación de un número de 64 bits, a trozos de 32."""
    v &= (1 << 64) - 1
    alto, bajo = v >> 32, v & 0xFFFFFFFF
    t = VARS[n][0]
    return f"        {n} := {t}({alto}) << 32 ob {bajo}"
pone64 = '\n'.join(pon64(n, v) for n, (t, v) in VARS.items() if es64(t))
pone64 += '''
        t64[0] := ent64(-3)
        t64[1] := ent64(0x7FFFFFFF) << 32 ob 0xFFFFFFFF
        t64[2] := ent64(10000000) * 1000
        t64[3] := ent64(1) << 40
        r.ancho := nat64(1) << 33
        pw := dirección(t64[0])
'''
prog = f"""módulo Azar
    modelo linux64
    tipo Reg = estructura
        corto: byte
        largo: nat32
        con: ent32
        otro: ent32
        ancho: nat64
    fin
{decl}    variable t32: arreglo [4] de nat32 = (11, 500000, 4000000000, 65536)
    variable r: Reg
    variable p: puntero a nat32
    variable q: puntero a ent32
    variable dos: arreglo [2] de ent32
    variable t64: arreglo [4] de ent64
    variable pw: puntero a ent64
    función igual64(n: nat64): nat64
        devolver n
    fin
    función igual_s64(n: ent64): ent64
        variable local: ent64
        local := n
        devolver local
    fin
    función igual32(n: nat32): nat32
        devolver n
    fin
    función igual_s32(n: ent32): ent32
        variable local: ent32
        local := n
        devolver local
    fin
    función tres(x: byte, n: ent32, z: nat16): ent32
        devolver n + x - z
    fin
    función principal()
        variable x8: byte
        variable x16: nat16
        variable xs: ent16
        variable x32: nat32
        variable xs32: ent32
        variable x64: nat64
        variable xs64: ent64
""" + pone64 + """        r.corto := 9
        r.largo := 77000
        r.con := -2
        p := t32
        q := dos
""" + '\n'.join(lineas) + "\n    fin\nfin Azar.\n"
open(sys.argv[3], 'w', encoding='utf-8').write(prog)
open(sys.argv[4], 'w', encoding='utf-8').write('\n'.join(esperado) + '\n')
