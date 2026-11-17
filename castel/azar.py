#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""azar.py SEMILLA CUANTAS SALIDA.cas ESPERADO.txt
Cuentas al azar para probar el compilador: escribe un programa Castel con
CUANTAS expresiones (variables de todos los tipos, elementos, campos, punteros,
llamadas, todos los operadores, comparaciones, asignaciones) y, aparte, lo
que tiene que escribir, calculado aquí con las reglas de CASTEL.md (16 bits,
con signo si hay un ent16 por medio). Lo usa pruebas.sh con AZAR=n."""
import random, sys
random.seed(int(sys.argv[1]))
N = int(sys.argv[2])
VARS = {'a': ('nat16', 40000), 'b': ('nat16', 7), 'e': ('ent16', -100 & 0xFFFF), 'f': ('ent16', 1234),
        'c': ('nat8', 200), 'd': ('nat8', 3), 'g': ('nat16', 65535), 'h': ('ent16', -32768 & 0xFFFF)}
T16 = [11, 500, 65000, 3]
T8 = [0, 255, 16, 99]
def sg(v):
    v &= 0xFFFF
    return v - 0x10000 if v >= 0x8000 else v
def hoja(prof):
    r = random.random()
    if r < 0.45:
        n = random.choice(list(VARS))
        return n, VARS[n][0], VARS[n][1]
    if r < 0.6:
        v = random.choice([0, 1, 2, 3, 7, 8, 10, 16, 100, 255, 256, 300, 1000])
        return str(v), 'lit', v
    if r < 0.7 and prof > 0:
        s, t, v = expr(prof - 1)
        return f"t16[{par(s)} yb 3]", 'nat16', T16[v & 3]
    if r < 0.8 and prof > 0:
        s, t, v = expr(prof - 1)
        return f"t8[{par(s)} yb 3]", 'nat8', T8[v & 3]
    if r < 0.9 and prof > 0:
        s, t, v = expr(prof - 1)
        if random.random() < 0.5:
            return f"igual16({s})", 'nat16', v & 0xFFFF
        return f"igual_s({s})", 'ent16', v & 0xFFFF
    n = random.choice(['r.uno', 'r.dos', 'valor(p)', 'p[1]'])
    return {'r.uno': ('r.uno', 'nat8', 77), 'r.dos': ('r.dos', 'ent16', -2 & 0xFFFF),
            'valor(p)': ('valor(p)', 'nat16', 11), 'p[1]': ('p[1]', 'nat16', 500)}[n]
def par(s):
    return s if s.replace('_', 'a').isalnum() else f"({s})"
def tipo_res(ta, tb):
    return 'ent16' if 'ent16' in (ta, tb) else 'nat16'
def expr(prof):
    if prof == 0 or random.random() < 0.15:
        return hoja(prof)
    r = random.random()
    if r < 0.1:
        s, t, v = expr(prof - 1)
        if t == 'lit':
            return s, t, v
        if random.random() < 0.5:
            return f"-{par(s)}", 'ent16', -v & 0xFFFF
        return f"nob {par(s)}", ('ent16' if t == 'ent16' else 'nat16'), ~v & 0xFFFF
    if r < 0.18:
        s, t, v = expr(prof - 1)
        if t == 'lit':
            return s, t, v
        k = random.choice(['nat8', 'nat16', 'ent16'])
        return f"{k}({s})", k, v & (0xFF if k == 'nat8' else 0xFFFF)
    sa, ta, va = expr(prof - 1)
    sb, tb, vb = expr(prof - 1)
    if ta == 'lit' and tb == 'lit':
        sb, tb, vb = hoja(0)
        while tb == 'lit':
            sb, tb, vb = hoja(0)
    op = random.choice(['+', '-', '*', 'yb', 'ob', 'xob', '+', '-', 'div', 'mod', '<<', '>>'])
    A, B = par(sa), par(sb)
    if op in ('+', '-', '*', 'yb', 'ob', 'xob'):
        v = {'+': va + vb, '-': va - vb, '*': va * vb, 'yb': va & vb, 'ob': va | vb, 'xob': va ^ vb}[op]
        return f"{A} {op} {B}", tipo_res(ta, tb), v & 0xFFFF
    if op in ('div', 'mod'):
        # divisor: nunca cero, y pequeño y positivo para que no desborde con signo
        if random.random() < 0.5:
            vb = random.choice([1, 2, 3, 4, 7, 8, 10, 16, 256, 300])
            B, tb = str(vb), 'lit'
        else:
            B, tb, vb = f"({B} yb 127 ob 1)", (tb if tb in ('ent16', 'lit') else 'nat16'), (vb & 127) | 1
        if ta == 'lit' and tb == 'lit':
            return hoja(0)
        if 'ent16' in (ta, tb):
            x = sg(va)
            q = abs(x) // vb * (1 if x >= 0 else -1)
            m = x - q * vb
        else:
            x = va & 0xFFFF
            q, m = x // vb, x % vb
        return f"{A} {op} {B}", tipo_res(ta, tb), (q if op == 'div' else m) & 0xFFFF
    # desplazamientos
    if random.random() < 0.6:
        n = random.randint(0, 15)
        B = str(n)
    else:
        n = vb & 15
        B = f"({B} yb 15)"
    if ta == 'lit' and B.isdigit():
        return hoja(0)
    if op == '<<':
        v = va << n
    elif ta == 'ent16':
        v = sg(va) >> n
    else:
        v = (va & 0xFFFF) >> n
    return f"{A} {op} {B}", ('ent16' if ta == 'ent16' else 'nat16'), v & 0xFFFF
lineas = []
esperado = []
i = 0
while i < N:
    s, t, v = expr(random.randint(1, 4))
    if t == 'lit':
        continue
    i += 1
    if random.random() < 0.25:
        s2, t2, v2 = expr(random.randint(1, 3))
        if t2 == 'lit':
            t2, v2, s2 = 'nat16', 5, 'b - 2'
        op = random.choice(['=', '<>', '<', '<=', '>', '>='])
        if 'ent16' in (t, t2):
            x, y = sg(v), sg(v2)
        else:
            x, y = v & 0xFFFF, v2 & 0xFFFF
        res = {'=': x == y, '<>': x != y, '<': x < y, '<=': x <= y, '>': x > y, '>=': x >= y}[op]
        lineas.append(f"        escribir({s} {op} {s2}, \"\\n\")")
        esperado.append('cierto' if res else 'falso')
        # y como condición de un «si»
        lineas.append(f"        si {s} {op} {s2} entonces escribir(\"s\") sino escribir(\"n\") fin")
        lineas.append("        escribir(\"\\n\")")
        esperado.append('s' if res else 'n')
    else:
        lineas.append(f"        escribir({s}, \"\\n\")")
        esperado.append(str(sg(v) if t == 'ent16' else v & (0xFF if t == 'nat8' else 0xFFFF)))
    if random.random() < 0.3:
        # guardar en variables de cada tipo y enseñarlas
        k = random.choice([('x8', 'nat8'), ('x16', 'nat16'), ('xs', 'ent16'), ('t16[b - 5]', 'nat16x'), ('r.tres', 'nat16')])
        lineas.append(f"        {k[0]} := {s}")
        if k[1] == 'nat16x':
            lineas.append("        escribir(t16[2], \"\\n\")")
            lineas.append("        t16[2] := 65000")
        else:
            lineas.append(f"        escribir({k[0]}, \"\\n\")")
        esperado.append(str(sg(v) if k[1] == 'ent16' else v & (0xFF if k[1] == 'nat8' else 0xFFFF)))
decl = ''.join(f"    variable {n}: {'byte' if t == 'nat8' else t} = {sg(v) if t == 'ent16' else v}\n" for n, (t, v) in VARS.items())
prog = f"""módulo Azar
    tipo Reg = estructura
        uno: byte
        dos: ent16
        tres: nat16
    fin
{decl}    variable t16: arreglo [4] de nat16 = (11, 500, 65000, 3)
    variable t8: arreglo [4] de byte = (0, 255, 16, 99)
    variable r: Reg
    variable p: puntero a nat16
    función igual16(n: nat16): nat16
        devolver n
    fin
    función igual_s(n: ent16): ent16
        variable local: ent16
        local := n
        devolver local
    fin
    función principal()
        variable x8: byte
        variable x16: nat16
        variable xs: ent16
        r.uno := 77
        r.dos := -2
        p := t16
""" + '\n'.join(lineas) + "\n    fin\nfin Azar.\n"
open(sys.argv[3], 'w', encoding='utf-8').write(prog)
open(sys.argv[4], 'w', encoding='utf-8').write('\n'.join(esperado) + '\n')
