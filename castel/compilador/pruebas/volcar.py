#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# volcar.py - la pareja de volcar.cas: carga un programa de Castel con las
# clases de ccastel.py (Cargador, Sintactico, Lexico) y vuelca su árbol en el
# mismo texto que saca VOLCAR.EXE (el frente escrito en Castel) dentro de
# Hispa-8086. Si los dos textos coinciden, los dos frentes entienden lo mismo.
#   python3 -I volcar.py PROGRAMA.cas [-I CARPETA]... > SALIDA.txt
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
import ccastel as C

OPS = {':=': 1, '<>': 2, '<=': 3, '>=': 4, '<<': 5, '>>': 6, '..': 7, '+': 8, '-': 9, '*': 10,
       '=': 13, '<': 14, '>': 15, 'y': 60, 'o': 61, 'no': 62, 'div': 63, 'mod': 64, 'yb': 68,
       'ob': 69, 'xob': 70, 'nob': 71}


def pos(n):
    return f"{n.pos[0]}:{min(n.pos[1], 255)}"


def pos2(p):
    return f"{p[0]}:{min(p[1], 255)}"


def lista(ns):
    return ''.join(' ' + v(x) for x in ns)


def bloque(ns):
    return ' [' + lista(ns or []) + ']'


def nombres(ns):
    return ''.join(f" (n {nm} {pos2(p)})" for nm, p in ns)


def pub(n):
    return ' p' if getattr(n, 'publico', False) else ' -'


def v(n):
    if n is None:
        return '-'
    k = n.k
    a = f"({k} {pos(n)}"
    if k == 'num':
        return a + f" {n.v})"
    if k == 'bool':
        return a + f" {n.v})"
    if k == 'nulo':
        return a + ")"
    if k == 'cad':
        return a + " " + n.s.hex() + ")"
    if k == 'ref':
        return a + f" {n.nombre})"
    if k == 'un':
        return a + f" {OPS[n.op]} {v(n.x)})"
    if k == 'bin':
        return a + f" {OPS[n.op]} {v(n.a)} {v(n.b)})"
    if k == 'indice':
        return a + f" {v(n.obj)} {v(n.idx)})"
    if k == 'campo':
        return a + f" {v(n.obj)} {n.nombre})"
    if k == 'llamada':
        s = a + f" {n.nombre}"
        for nm, e in n.args:
            s += f" (arg {nm[0]} {pos2(nm[1])} {v(e)})" if nm else f" (arg - {v(e)})"
        return s + ")"
    if k == 'lista':
        return a + lista(n.elems) + ")"
    if k == 'var':
        return a + (' g' if n.global_ else ' l') + pub(n) + ''.join(' ' + x for x in n.nombres) \
            + f" : {v(n.t)} {v(n.ini)})"
    if k == 'const':
        return a + pub(n) + f" {n.nombre} {v(n.e)})"
    if k == 'asig':
        return a + f" {v(n.dest)} {v(n.e)})"
    if k == 'llamada_sent':
        return a + f" {v(n.e)})"
    if k == 'devolver':
        return a + f" {v(n.e)})"
    if k == 'si':
        return a + f" {v(n.cond)}" + bloque(n.entonces) + bloque(n.sino) + ")"
    if k == 'mientras':
        return a + f" {v(n.cond)}" + bloque(n.cuerpo) + ")"
    if k == 'para':
        return a + f" {v(n.var)} {v(n.desde)} {v(n.hasta)} {v(n.paso)}" + bloque(n.cuerpo) + ")"
    if k == 'segun':
        return a + f" {v(n.e)}" + lista(n.casos) + " otro" + bloque(n.otro) + ")"
    if k == 'caso':
        return a + ''.join(f" (val {v(x)} {v(y)})" for x, y in n.vals) + bloque(n.cuerpo) + ")"
    if k in ('romper', 'seguir', 'modelo'):
        return a + ")"
    if k == 'asm':
        texto = ''.join(ln + '\n' for ln, _ in n.lineas).encode('utf-8')
        return a + f" {n.lineas[0][1] if n.lineas else '-'} " + texto.hex() + ")"
    if k == 'func':
        s = a + pub(n) + f" {n.nombre}"
        for p in n.params:
            s += f" (param {pos(p)} {'r' if p.ref else 'v'} {p.nombre} {v(p.t)})"
        return s + f" : {v(n.t)}" + bloque(n.cuerpo) + ")"
    if k == 'tipo':
        return a + pub(n) + f" {n.nombre} {v(n.t)})"
    if k == 'importar':
        return a + nombres(n.nombres) + ")"
    if k == 'tarr':
        return a + f" {v(n.n)} {v(n.base)})"
    if k == 'tpunt':
        return a + f" {v(n.base)})"
    if k == 'test':
        return a + ''.join(" (campos" + nombres(ns) + f" {v(t)})" for ns, t in n.campos) + ")"
    if k == 'tnom':
        return a + f" {n.nombre})"
    raise AssertionError(k)


def main():
    args = sys.argv[1:]
    rutas = []
    while '-I' in args:
        i = args.index('-I')
        rutas.append(args[i + 1])
        del args[i:i + 2]
    c = C.Cargador(rutas)
    try:
        c.carga(args[0])
    except C.ErrorCastel as e:
        if e.pos:
            print(f"{e.archivo or args[0]}:{e.pos[0]}:{e.pos[1]}: error: {e.msg}")
        else:
            print(f"ccastel: {e.msg}")
        return 1
    sal = sys.stdout
    for m in c.orden:
        sal.write(f"== {m.nombre} importa:" + ''.join(' ' + x.nombre for x in m.importa) + "\n")
        sal.write(f"(modulo {pos(m.arbol)} {m.nombre}\n")
        consts = []
        for d in m.arbol.decls:
            sal.write(v(d) + "\n")
            if d.k == 'func':
                consts += [x.nombre for x in C.recorre(d.cuerpo) if x.k == 'const']
        sal.write("(consts" + ''.join(' ' + x for x in consts) + "))\n")
    return 0


if __name__ == '__main__':
    sys.exit(main())
