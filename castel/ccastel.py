#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  ccastel.py - compilador de Castel (etapa 0, Fases 1, 3 y primera parte de la 4).
#  Castel: lenguaje de sistemas en español, familia Pascal/Oberon.
#  Este es el PRIMER compilador, escrito en Python en el PC: lee Castel
#  (docs/CASTEL.md) y SACA ENSAMBLADOR (NASM) para un .COM de DOS de 16 bits,
#  solo con instrucciones del 8086. Luego NASM remata.
#
#  uso:   python3 -I ccastel.py ENTRADA.cas [-o SALIDA.asm] [-I CARPETA]...
#         (sin -o, escribe ENTRADA.asm al lado; -I: dónde más buscar módulos)
#
#  Entiende: módulo con principal; varios archivos (importar, público); constante
#  (con expresiones), variable (globales con valor inicial), tipo; nat8/byte,
#  nat16, ent16, nat32, ent32 (32 bits en DX:AX), bool, arreglo
#  [N] de T, estructura, puntero a T; funciones con y sin retorno, parámetros
#  por valor y por referencia (ref); := ; si/sino; mientras; para (con paso);
#  según/caso; romper/seguir; devolver; ensamblador (con {nombres} de Castel);
#  aritmética, bits (yb ob xob nob << >>), comparaciones, lógicas; llamadas;
#  interrupción() con registros por nombre, acarreo(), reg_bx()...; y las
#  funciones de bajo nivel (puertos y memoria lejana).
#
#  Las partes: 1) léxico, 2) sintáctico (árbol), 3) tipos y comprobaciones,
#  4) generación de NASM, 5) los archivos (el programa y los módulos que importa:
#  todo acaba en un solo .asm). (Los saltos condicionales del 8086 solo llegan a
#  127 octetos: cuando no llegan, NASM pone él solo el salto contrario y un jmp.)
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# =============================================================================
import os
import re
import sys

PALABRAS = {
    'módulo', 'fin', 'constante', 'variable', 'función', 'devolver', 'tipo',
    'si', 'entonces', 'sino', 'mientras', 'hacer', 'para', 'desde', 'hasta',
    'paso', 'romper', 'seguir', 'ensamblador', 'y', 'o', 'no', 'div', 'mod',
    'cierto', 'falso', 'nulo', 'yb', 'ob', 'xob', 'nob', 'según', 'caso',
    'arreglo', 'estructura', 'puntero', 'público', 'importar',
}
REG16 = ('ax', 'bx', 'cx', 'dx', 'si', 'di')
REG8 = ('al', 'ah', 'bl', 'bh', 'cl', 'ch', 'dl', 'dh')
REGSEG = ('es', 'ds')
SALIDAS_INT = {'acarreo': 'ban', 'banderas': 'ban', 'reg_bx': 'bx', 'reg_cx': 'cx',
               'reg_dx': 'dx', 'reg_si': 'si', 'reg_di': 'di', 'reg_es': 'es'}


class ErrorCastel(Exception):
    def __init__(self, msg, pos=None):
        Exception.__init__(self, msg)
        self.msg = msg
        self.pos = pos
        self.archivo = None         # el archivo donde está el error (si se sabe)
        self.nota = None            # una línea más de explicación

    def __str__(self):
        if self.pos:
            return f"línea {self.pos[0]}, columna {self.pos[1]}: {self.msg}"
        return self.msg


def es_letra(c):
    return c.isalpha() or c == '_'


# =============================================================================
#  1. El lector (análisis léxico)
#  Cada pieza: (clase, valor, línea, columna). Clases: NL EOF NUM CAD ID PAL
#  OP ASM (las líneas crudas de un bloque «ensamblador»).
# =============================================================================
class Lexico:
    def __init__(self, texto):
        self.t = texto
        self.i = 0
        self.n = len(texto)
        self.linea = 1
        self.ini_linea = 0          # índice donde empieza la línea actual
        self.toks = []

    def pos(self, i=None):
        return (self.linea, (self.i if i is None else i) - self.ini_linea + 1)

    def error(self, msg, i=None):
        raise ErrorCastel(msg, self.pos(i))

    def lee(self):
        while self.i < self.n:
            c = self.t[self.i]
            if c == '\n':
                self.emite('NL', '\\n', self.i)
                self.i += 1
                self.linea += 1
                self.ini_linea = self.i
                continue
            if c in ' \t\r':
                self.i += 1
                continue
            if c == ';':                       # comentario de línea
                while self.i < self.n and self.t[self.i] != '\n':
                    self.i += 1
                continue
            if self.t.startswith('(*', self.i):
                self.comentario_bloque()
                continue
            if c == '"':
                self.cadena()
                continue
            if c == "'":
                self.caracter()
                continue
            if c.isdigit():
                self.numero()
                continue
            if es_letra(c):
                self.identificador()
                continue
            self.simbolo()
        self.emite('NL', '\\n', self.i)
        self.emite('EOF', 'el final del archivo', self.i)
        return self.toks

    def emite(self, clase, val, i):
        if clase == 'NL' and self.toks and self.toks[-1][0] == 'NL':
            return
        lin, col = self.pos(i)
        self.toks.append((clase, val, lin, col))

    def comentario_bloque(self):
        prof = 0
        ini = self.i
        pos0 = self.pos()
        while self.i < self.n:
            if self.t.startswith('(*', self.i):
                prof += 1
                self.i += 2
            elif self.t.startswith('*)', self.i):
                prof -= 1
                self.i += 2
                if prof == 0:
                    return
            else:
                if self.t[self.i] == '\n':
                    self.linea += 1
                    self.ini_linea = self.i + 1
                self.i += 1
        raise ErrorCastel("comentario de bloque «(*» sin cerrar", pos0)

    ESCAPES = {'n': 10, 'r': 13, 't': 9, '"': 34, '\\': 92, '0': 0, "'": 39, 'e': 27}

    def escape(self):
        # self.i está en la letra que sigue a la barra
        if self.i >= self.n:
            self.error("texto sin cerrar")
        e = self.t[self.i]
        if e == 'x':
            h = self.t[self.i + 1:self.i + 3]
            if len(h) != 2 or any(d not in '0123456789abcdefABCDEF' for d in h):
                self.error("«\\x» lleva dos cifras hexadecimales")
            self.i += 3
            return int(h, 16)
        if e not in self.ESCAPES:
            self.error(f"escape desconocido: «\\{e}»")
        self.i += 1
        return self.ESCAPES[e]

    def cadena(self):
        ini = self.i
        self.i += 1
        s = bytearray()
        while True:
            if self.i >= self.n or self.t[self.i] == '\n':
                self.error("texto sin cerrar (falta la comilla \")", ini)
            c = self.t[self.i]
            if c == '"':
                break
            if c == '\\':
                self.i += 1
                s.append(self.escape())
                continue
            try:
                s += c.encode('cp437')
            except UnicodeEncodeError:
                self.error(f"la letra «{c}» no está en la página 437")
            self.i += 1
        self.i += 1
        self.emite('CAD', bytes(s), ini)

    def caracter(self):
        ini = self.i
        self.i += 1
        if self.i >= self.n or self.t[self.i] == '\n':
            self.error("carácter sin cerrar (falta la comilla ')", ini)
        if self.t[self.i] == '\\':
            self.i += 1
            val = self.escape()
        else:
            try:
                b = self.t[self.i].encode('cp437')
            except UnicodeEncodeError:
                self.error(f"la letra «{self.t[self.i]}» no está en la página 437")
            val = b[0]
            self.i += 1
        if self.i >= self.n or self.t[self.i] != "'":
            self.error("carácter mal cerrado (falta la comilla ')", ini)
        self.i += 1
        self.emite('NUM', val, ini)

    def numero(self):
        j = self.i
        if self.t[self.i:self.i + 2] in ('0x', '0X'):
            self.i += 2
            k = self.i
            while self.i < self.n and self.t[self.i] in '0123456789abcdefABCDEF':
                self.i += 1
            if self.i == k:
                self.error("número hexadecimal sin cifras", j)
            v = int(self.t[k:self.i], 16)
        else:
            while self.i < self.n and self.t[self.i].isdigit():
                self.i += 1
            v = int(self.t[j:self.i])
        if self.i < self.n and (es_letra(self.t[self.i]) or self.t[self.i].isdigit()):
            self.error("número mal escrito", j)
        if v > 0xFFFFFFFF:
            self.error("el número no cabe en 32 bits", j)
        self.emite('NUM', v, j)

    def identificador(self):
        j = self.i
        while self.i < self.n and (es_letra(self.t[self.i]) or self.t[self.i].isdigit()):
            self.i += 1
        p = self.t[j:self.i]
        if p == 'ensamblador':
            self.emite('PAL', p, j)
            self.bloque_asm()
        elif p in PALABRAS:
            self.emite('PAL', p, j)
        else:
            self.emite('ID', p, j)

    def bloque_asm(self):
        # lo que sigue, tal cual, hasta una línea que sea solo «fin»
        while self.i < self.n and self.t[self.i] != '\n':
            if self.t[self.i] == ';':
                while self.i < self.n and self.t[self.i] != '\n':
                    self.i += 1
                break
            if self.t[self.i] not in ' \t\r':
                self.error("tras «ensamblador» se cambia de línea")
            self.i += 1
        pos0 = self.pos()
        lineas = []
        while True:
            if self.i >= self.n:
                raise ErrorCastel("bloque «ensamblador» sin «fin»", pos0)
            self.i += 1                         # el fin de línea
            self.linea += 1
            self.ini_linea = self.i
            k = self.t.find('\n', self.i)
            if k < 0:
                k = self.n
            ln = self.t[self.i:k].rstrip('\r')
            if ln.split(';')[0].strip() == 'fin':
                self.i += len(ln) - len(ln.lstrip())
                self.emite('ASM', lineas, self.i)
                self.emite('PAL', 'fin', self.i)
                self.i += 3
                return
            lineas.append((ln, self.linea))
            self.i = k

    def simbolo(self):
        par = self.t[self.i:self.i + 2]
        if par in (':=', '<>', '<=', '>=', '<<', '>>', '..'):
            self.emite('OP', par, self.i)
            self.i += 2
            return
        c = self.t[self.i]
        if c in '+-*()=<>,:.[]':
            self.emite('OP', c, self.i)
            self.i += 1
            return
        self.error(f"carácter inesperado: «{c}»")


# =============================================================================
#  2. La gramática (análisis sintáctico) -> árbol
# =============================================================================
class N:
    """Un nudo del árbol: k es la clase; pos, (línea, columna)."""
    def __init__(self, k, pos, **kw):
        self.k = k
        self.pos = pos
        self.__dict__.update(kw)


def recorre(x):
    """Todos los nudos que cuelgan de x (él incluido)."""
    if isinstance(x, N):
        yield x
        for v in x.__dict__.values():
            yield from recorre(v)
    elif isinstance(x, (list, tuple)):
        for v in x:
            yield from recorre(v)


NOMBRE_TOK = {'NL': 'el fin de la línea', 'EOF': 'el final del archivo'}


class Sintactico:
    def __init__(self, toks):
        self.toks = toks
        self.p = 0
        # los módulos de «importar» (estén donde estén): valen como Módulo.nombre
        self.importados = set()
        for i, t in enumerate(toks):
            if t[0] == 'PAL' and t[1] == 'importar':
                j = i + 1
                while j < len(toks) and toks[j][0] == 'ID':
                    self.importados.add(toks[j][1])
                    if toks[j + 1][0] != 'OP' or toks[j + 1][1] != ',':
                        break
                    j += 2

    def calificado(self):
        """¿Viene «Módulo.nombre», con Módulo importado?"""
        return self.es('ID') and self.tok()[1] in self.importados and self.es('OP', '.', 1) \
            and self.es('ID', None, 2)

    def tok(self, d=0):
        return self.toks[min(self.p + d, len(self.toks) - 1)]

    def pos(self):
        t = self.tok()
        return (t[2], t[3])

    def error(self, msg, pos=None):
        t = self.tok()
        if pos is None:
            esta = NOMBRE_TOK.get(t[0]) or (f"«{t[1].decode('cp437')}»" if t[0] == 'CAD' else f"«{t[1]}»")
            if t[0] == 'ASM':
                esta = 'un bloque de ensamblador'
            raise ErrorCastel(f"{msg} (está {esta})", (t[2], t[3]))
        raise ErrorCastel(msg, pos)

    def salta_nl(self):
        while self.tok()[0] == 'NL':
            self.p += 1

    def es(self, clase, val=None, d=0):
        t = self.tok(d)
        return t[0] == clase and (val is None or t[1] == val)

    def come(self, clase, val=None, que=None):
        if not self.es(clase, val):
            if clase == 'ID' and val is None and self.tok()[0] == 'PAL':
                self.error(f"«{self.tok()[1]}» es una palabra del lenguaje: no vale como nombre",
                           self.pos())
            if que is None:
                que = f"«{val}»" if val is not None else {'ID': 'un nombre', 'NUM': 'un número',
                                                           'NL': 'el fin de la línea'}.get(clase, clase)
            self.error(f"esperaba {que}")
        t = self.tok()
        self.p += 1
        return t

    def fin_sentencia(self):
        # una sentencia por línea (o seguida de lo que cierra el bloque)
        if not (self.es('NL') or self.es('EOF') or self.es('PAL', 'fin') or self.es('PAL', 'sino')
                or self.es('PAL', 'caso') or self.es_si_no()):
            self.error("sobra esto al final de la sentencia (va una por línea)")

    # ---- módulo ----
    def modulo(self):
        self.salta_nl()
        pos = self.pos()
        self.come('PAL', 'módulo')
        nombre = self.come('ID', que='el nombre del módulo')[1]
        self.nombre_modulo = nombre
        decls = []
        self.salta_nl()
        while not self.es('PAL', 'fin'):
            decls.append(self.declaracion_modulo())
            if self.es('ID', nombre):
                self.falta_fin()
            if not self.es('NL'):
                self.error("sobra esto al final de la declaración")
            self.salta_nl()
        self.come('PAL', 'fin')
        pf = self.pos()
        fin_nombre = self.come('ID', que='el nombre del módulo tras «fin»')[1]
        if fin_nombre != nombre:
            self.error(f"el módulo abre como «{nombre}» y cierra como «{fin_nombre}»", pf)
        if self.es('OP', '.'):
            self.p += 1
        self.salta_nl()
        if not self.es('EOF'):
            self.error("sobra texto después del final del módulo")
        return N('modulo', pos, nombre=nombre, decls=decls)

    def declaracion_modulo(self):
        if self.es('PAL', 'importar'):
            return self.importar()
        if self.es('PAL', 'público'):
            self.p += 1
            if not (self.es('PAL', 'constante') or self.es('PAL', 'variable') or self.es('PAL', 'función')
                    or self.es('PAL', 'tipo')):
                self.error("tras «público» va constante, variable, tipo o función")
            d = self.declaracion_modulo()
            d.publico = True
            return d
        if self.es('PAL', 'constante'):
            return self.constante()
        if self.es('PAL', 'variable'):
            return self.variable(True)
        if self.es('PAL', 'función'):
            return self.funcion()
        if self.es('PAL', 'tipo'):
            return self.decl_tipo()
        if self.es('PAL', 'ensamblador'):
            return self.ensamblador()
        if self.es('ID', self.nombre_modulo) or self.es('EOF'):
            self.falta_fin()
        self.error("esperaba constante, variable, tipo, función o ensamblador")

    def importar(self):
        pos = self.pos()
        self.come('PAL', 'importar')
        nombres = []
        while True:
            pn = self.pos()
            nombres.append((self.come('ID', que='el nombre de un módulo')[1], pn))
            if not self.es('OP', ','):
                break
            self.p += 1
        return N('importar', pos, nombres=nombres)

    def falta_fin(self):
        self.error("falta un «fin»: algún bloque de antes (si, mientras, para, según,"
                   " función...) se ha quedado sin cerrar", self.pos())

    def constante(self):
        pos = self.pos()
        self.come('PAL', 'constante')
        nombre = self.come('ID', que='el nombre de la constante')[1]
        self.come('OP', '=')
        return N('const', pos, nombre=nombre, e=self.expr())

    def decl_tipo(self):
        pos = self.pos()
        self.come('PAL', 'tipo')
        nombre = self.come('ID', que='el nombre del tipo')[1]
        self.come('OP', '=')
        return N('tipo', pos, nombre=nombre, t=self.tipo())

    def tipo(self):
        pos = self.pos()
        if self.es('PAL', 'arreglo'):
            self.p += 1
            self.come('OP', '[')
            n = self.expr()
            self.come('OP', ']')
            self.come('ID', 'de')
            return N('tarr', pos, n=n, base=self.tipo())
        if self.es('PAL', 'puntero'):
            self.p += 1
            self.come('ID', 'a')
            return N('tpunt', pos, base=self.tipo())
        if self.es('PAL', 'estructura'):
            self.p += 1
            campos = []
            self.salta_nl()
            while not self.es('PAL', 'fin'):
                nombres = [(self.come('ID', que='el nombre de un campo')[1], self.pos())]
                while self.es('OP', ','):
                    self.p += 1
                    nombres.append((self.come('ID', que='el nombre de un campo')[1], self.pos()))
                self.come('OP', ':')
                t = self.tipo()
                campos.append((nombres, t))
                self.salta_nl()
            self.come('PAL', 'fin')
            return N('test', pos, campos=campos)
        if self.calificado():
            nombre = f"{self.tok()[1]}.{self.tok(2)[1]}"
            self.p += 3
            return N('tnom', pos, nombre=nombre)
        if self.es('ID'):
            return N('tnom', pos, nombre=self.come('ID')[1])
        self.error("esperaba un tipo")

    def variable(self, global_):
        pos = self.pos()
        self.come('PAL', 'variable')
        nombres = [self.come('ID', que='el nombre de la variable')[1]]
        while self.es('OP', ','):
            self.p += 1
            nombres.append(self.come('ID', que='el nombre de la variable')[1])
        self.come('OP', ':')
        t = self.tipo()
        ini = None
        if self.es('OP', '='):
            self.p += 1
            ini = self.inicial()
        return N('var', pos, nombres=nombres, t=t, ini=ini, global_=global_)

    def inicial(self):
        # una expresión, o una lista entre paréntesis para un arreglo
        if self.es('OP', '('):
            marca = self.p
            pos = self.pos()
            self.p += 1
            self.salta_nl()
            lista = [self.expr()]
            self.salta_nl()
            while self.es('OP', ','):
                self.p += 1
                self.salta_nl()
                lista.append(self.expr())
                self.salta_nl()
            if self.es('OP', ')') and self.tok(1)[0] in ('NL', 'EOF') and len(lista) > 1:
                self.p += 1
                return N('lista', pos, elems=lista)
            self.p = marca
        return self.expr()

    def funcion(self):
        pos = self.pos()
        self.come('PAL', 'función')
        nombre = self.come('ID', que='el nombre de la función')[1]
        self.come('OP', '(')
        params = []
        if not self.es('OP', ')'):
            params.append(self.param())
            while self.es('OP', ','):
                self.p += 1
                params.append(self.param())
        self.come('OP', ')')
        t = None
        if self.es('OP', ':'):
            self.p += 1
            t = self.tipo()
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return N('func', pos, nombre=nombre, params=params, t=t, cuerpo=cuerpo)

    def param(self):
        pos = self.pos()
        ref = False
        if self.es('ID', 'ref') and self.es('ID', None, 1):
            self.p += 1
            ref = True
        nombre = self.come('ID', que='el nombre del parámetro')[1]
        self.come('OP', ':')
        return N('param', pos, nombre=nombre, t=self.tipo(), ref=ref)

    # ---- bloque de sentencias: hasta fin, sino, caso o «si no:» ----
    def es_si_no(self):
        return (self.es('PAL', 'si') and self.es('PAL', 'no', 1) and self.es('OP', ':', 2)) or \
               (self.es('PAL', 'sino') and self.es('OP', ':', 1))

    def bloque(self):
        sent = []
        self.salta_nl()
        while not (self.es('PAL', 'fin') or self.es('PAL', 'sino') or self.es('PAL', 'caso')
                   or self.es_si_no()):
            if self.es('EOF'):
                self.falta_fin()
            sent.append(self.sentencia())
            self.fin_sentencia()
            self.salta_nl()
        return sent

    def sentencia(self):
        pos = self.pos()
        if self.es('PAL', 'variable'):
            return self.variable(False)
        if self.es('PAL', 'constante'):
            return self.constante()
        if self.es('PAL', 'si'):
            return self.si()
        if self.es('PAL', 'mientras'):
            return self.mientras()
        if self.es('PAL', 'para'):
            return self.para()
        if self.es('PAL', 'según'):
            return self.segun()
        if self.es('PAL', 'devolver'):
            self.p += 1
            if self.es('NL') or self.es('PAL', 'fin') or self.es('PAL', 'sino'):
                return N('devolver', pos, e=None)
            return N('devolver', pos, e=self.expr())
        if self.es('PAL', 'romper'):
            self.p += 1
            return N('romper', pos)
        if self.es('PAL', 'seguir'):
            self.p += 1
            return N('seguir', pos)
        if self.es('PAL', 'ensamblador'):
            return self.ensamblador()
        if self.es('ID') or self.es('OP', '('):
            e = self.postfijo()
            if self.es('OP', ':='):
                self.p += 1
                return N('asig', pos, dest=e, e=self.expr())
            if e.k == 'llamada':
                return N('llamada_sent', pos, e=e)
            if self.es('OP', '='):
                self.error("para asignar se escribe «:=» («=» solo compara)", self.pos())
            self.error("esperaba «:=» o una llamada")
        self.error("sentencia no reconocida")

    def si(self):
        pos = self.pos()
        self.come('PAL', 'si')
        cond = self.expr()
        self.come('PAL', 'entonces')
        entonces = self.bloque()
        sino = None
        if self.es('PAL', 'sino') and not self.es('OP', ':', 1):
            self.p += 1
            if self.es('PAL', 'si'):
                return N('si', pos, cond=cond, entonces=entonces, sino=[self.si()])
            sino = self.bloque()
        self.come('PAL', 'fin')
        return N('si', pos, cond=cond, entonces=entonces, sino=sino)

    def mientras(self):
        pos = self.pos()
        self.come('PAL', 'mientras')
        cond = self.expr()
        self.come('PAL', 'hacer')
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return N('mientras', pos, cond=cond, cuerpo=cuerpo)

    def para(self):
        pos = self.pos()
        self.come('PAL', 'para')
        pv = self.pos()
        var = N('ref', pv, nombre=self.come('ID', que='la variable del «para»')[1])
        self.come('PAL', 'desde')
        desde = self.expr()
        self.come('PAL', 'hasta')
        hasta = self.expr()
        paso = None
        if self.es('PAL', 'paso'):
            self.p += 1
            paso = self.expr()
        self.come('PAL', 'hacer')
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return N('para', pos, var=var, desde=desde, hasta=hasta, paso=paso, cuerpo=cuerpo)

    def segun(self):
        pos = self.pos()
        self.come('PAL', 'según')
        e = self.expr()
        self.come('NL')
        self.salta_nl()
        casos = []
        otro = None
        while not self.es('PAL', 'fin'):
            if self.es_si_no():
                if otro is not None:
                    self.error("«si no:» repetido")
                self.p += 2 if self.es('PAL', 'si') else 1
                self.come('OP', ':')
                otro = self.bloque()
                continue
            pc = self.pos()
            self.come('PAL', 'caso', que='«caso», «si no:» o «fin»')
            if otro is not None:
                self.error("«si no:» va el último", pc)
            vals = []
            while True:
                a = self.expr()
                b = None
                if self.es('OP', '..'):
                    self.p += 1
                    b = self.expr()
                vals.append((a, b))
                if not self.es('OP', ','):
                    break
                self.p += 1
            self.come('OP', ':')
            casos.append(N('caso', pc, vals=vals, cuerpo=self.bloque()))
        self.come('PAL', 'fin')
        return N('segun', pos, e=e, casos=casos, otro=otro)

    def ensamblador(self):
        pos = self.pos()
        self.come('PAL', 'ensamblador')
        lineas = self.come('ASM')[1]
        self.come('PAL', 'fin')
        return N('asm', pos, lineas=lineas)

    def argumentos(self):
        self.come('OP', '(')
        args = []
        self.salta_nl()
        if not self.es('OP', ')'):
            while True:
                nombre = None
                if self.es('ID') and self.es('OP', ':', 1):
                    nombre = (self.tok()[1], self.pos())
                    self.p += 2
                args.append((nombre, self.expr()))
                self.salta_nl()
                if not self.es('OP', ','):
                    break
                self.p += 1
                self.salta_nl()
        self.come('OP', ')')
        return args

    # ---- expresiones. De más floja a más fuerte:
    #      o | y | no | comparaciones | + - ob xob | * div mod yb << >> | unarios
    def expr(self):
        izq = self.e_y()
        while self.es('PAL', 'o'):
            pos = self.pos()
            self.p += 1
            izq = N('bin', izq.pos, op='o', a=izq, b=self.e_y())
        return izq

    def e_y(self):
        izq = self.e_no()
        while self.es('PAL', 'y'):
            pos = self.pos()
            self.p += 1
            izq = N('bin', izq.pos, op='y', a=izq, b=self.e_no())
        return izq

    def e_no(self):
        if self.es('PAL', 'no'):
            pos = self.pos()
            self.p += 1
            return N('un', pos, op='no', x=self.e_no())
        return self.e_cmp()

    def e_cmp(self):
        izq = self.e_suma()
        if self.tok()[0] == 'OP' and self.tok()[1] in ('=', '<>', '<', '<=', '>', '>='):
            pos = self.pos()
            op = self.tok()[1]
            self.p += 1
            izq = N('bin', izq.pos, op=op, a=izq, b=self.e_suma())
            if self.tok()[0] == 'OP' and self.tok()[1] in ('=', '<>', '<', '<=', '>', '>='):
                self.error("las comparaciones no se encadenan: use «y»")
        return izq

    def e_suma(self):
        izq = self.e_mul()
        while (self.tok()[0] == 'OP' and self.tok()[1] in ('+', '-')) or \
                (self.tok()[0] == 'PAL' and self.tok()[1] in ('ob', 'xob')):
            pos = self.pos()
            op = self.tok()[1]
            self.p += 1
            izq = N('bin', izq.pos, op=op, a=izq, b=self.e_mul())
        return izq

    def e_mul(self):
        izq = self.e_unario()
        while (self.tok()[0] == 'OP' and self.tok()[1] in ('*', '<<', '>>')) or \
                (self.tok()[0] == 'PAL' and self.tok()[1] in ('div', 'mod', 'yb')):
            pos = self.pos()
            op = self.tok()[1]
            self.p += 1
            izq = N('bin', izq.pos, op=op, a=izq, b=self.e_unario())
        return izq

    def e_unario(self):
        pos = self.pos()
        if self.es('OP', '-'):
            self.p += 1
            return N('un', pos, op='-', x=self.e_unario())
        if self.es('PAL', 'nob'):
            self.p += 1
            return N('un', pos, op='nob', x=self.e_unario())
        return self.postfijo()

    def postfijo(self):
        e = self.primario()
        while True:
            pos = self.pos()
            if self.es('OP', '['):
                self.p += 1
                i = self.expr()
                self.come('OP', ']')
                e = N('indice', pos, obj=e, idx=i)
            elif self.es('OP', '.') and self.es('ID', None, 1):
                self.p += 1
                e = N('campo', pos, obj=e, nombre=self.come('ID')[1])
            else:
                return e

    def primario(self):
        t = self.tok()
        pos = self.pos()
        if t[0] == 'NUM':
            self.p += 1
            return N('num', pos, v=t[1])
        if t[0] == 'CAD':
            self.p += 1
            return N('cad', pos, s=t[1])
        if t[0] == 'PAL' and t[1] in ('cierto', 'falso'):
            self.p += 1
            return N('bool', pos, v=1 if t[1] == 'cierto' else 0)
        if t[0] == 'PAL' and t[1] == 'nulo':
            self.p += 1
            return N('nulo', pos)
        if t[0] == 'OP' and t[1] == '(':
            self.p += 1
            e = self.expr()
            self.come('OP', ')')
            return e
        if t[0] == 'ID' or (t[0] == 'PAL' and t[1] == 'puntero' and self.es('OP', '(', 1)):
            nombre = t[1]
            if self.calificado():               # Módulo.nombre
                nombre = f"{t[1]}.{self.tok(2)[1]}"
                self.p += 2
            self.p += 1
            if self.es('OP', '('):
                return N('llamada', pos, nombre=nombre, args=self.argumentos())
            return N('ref', pos, nombre=nombre)
        self.error("esperaba una expresión")


# =============================================================================
#  3. Los tipos
# =============================================================================
class Tipo:
    def __init__(self, k, tam, base=None, n=0, campos=None, nombre=None):
        self.k = k                  # nat8 nat16 ent16 bool lit punt arr est nada
        self.tam = tam
        self._base = base           # un Tipo, o una función que lo da (perezoso)
        self.n = n
        self.campos = campos        # nombre -> (desplazamiento, Tipo)
        self.nombre = nombre

    @property
    def base(self):
        if callable(self._base):
            self._base = self._base()
        return self._base

    def __str__(self):
        if self.k == 'punt':
            return 'puntero' if self._base is None else f'puntero a {self.base}'
        if self.k == 'arr':
            return f'arreglo [{self.n}] de {self.base}'
        if self.k == 'est':
            return self.nombre or 'estructura'
        return {'lit': 'número', 'lit32': 'número', 'nat8': 'byte', 'nada': 'nada'}.get(self.k, self.k)


T_NAT8 = Tipo('nat8', 1)
T_NAT16 = Tipo('nat16', 2)
T_ENT16 = Tipo('ent16', 2)
T_BOOL = Tipo('bool', 1)
T_NAT32 = Tipo('nat32', 4)
T_ENT32 = Tipo('ent32', 4)
T_LIT = Tipo('lit', 2)              # un número escrito tal cual: vale para todos
T_LIT32 = Tipo('lit32', 4)          # un número escrito que no cabe en 16 bits
T_NADA = Tipo('nada', 0)
T_PUNT_U = Tipo('punt', 2)          # puntero «universal»: nulo y puntero(n)
T_TEXTO = Tipo('punt', 2, T_NAT8)
BASICOS = {'nat8': T_NAT8, 'byte': T_NAT8, 'nat16': T_NAT16, 'ent16': T_ENT16, 'bool': T_BOOL,
           'nat32': T_NAT32, 'ent32': T_ENT32}
NUMERICOS = ('nat8', 'nat16', 'ent16', 'lit', 'nat32', 'ent32', 'lit32')
CONVERSIONES = ('nat8', 'byte', 'nat16', 'ent16', 'nat32', 'ent32')
MIN32 = -0x80000000
MAX32 = 0xFFFFFFFF


def igual(a, b):
    if a is b:
        return True
    if a.k != b.k:
        return False
    if a.k == 'punt':
        return a._base is None or b._base is None or igual(a.base, b.base)
    if a.k == 'arr':
        return a.n == b.n and igual(a.base, b.base)
    return a.k != 'est'


def es_num(t):
    return t.k in NUMERICOS


def es32(t):
    """¿Es un número de 32 bits (va en DX:AX)?"""
    return t.k in ('nat32', 'ent32', 'lit32')


def es_lit(t):
    return t.k in ('lit', 'lit32')


def cabe16(v):
    return -0x8000 <= v <= 0xFFFF


def tipo_lit(v):
    """El tipo de un número escrito: de 16 bits si cabe; si no, de 32."""
    return T_LIT if cabe16(v) else T_LIT32


def mitades(v):
    """Las dos palabras (baja, alta) de un número de 32 bits, como texto."""
    v &= 0xFFFFFFFF
    return str(v & 0xFFFF), str(v >> 16)


def mas2(r):
    """El lugar de la palabra alta de uno de 32 bits."""
    return (r[0], r[1], r[2] + 2)


class Modulo:
    """Un archivo fuente ya leído: su árbol y lo que declara."""
    def __init__(self, nombre, ruta, texto, arbol):
        self.nombre = nombre
        self.ruta = ruta
        self.texto = texto
        self.arbol = arbol
        self.importa = []           # los Modulo que importa, en el orden en que los nombra
        self.principal = False      # ¿es el programa (el archivo que se compila)?
        self.consts = {}            # nombre -> ('num', v) | ('bool', v) | ('cad', etiqueta)
        self.tipos = dict(BASICOS)
        self.globales = {}
        self.funcs = {}             # nombre -> N func (con .ptipos, .tret, .etq...)
        self.publicos = {}          # nombre -> (tabla, objeto): lo que ven los que lo importan
        self.ambiguos = {}          # nombre -> los módulos importados que lo traen (dos o más)

    def etiqueta(self, letra, nombre):
        """El nombre en NASM: F_nombre en el programa, F_Módulo__nombre en los demás."""
        return f'{letra}_{nombre}' if self.principal else f'{letra}_{self.nombre}__{nombre}'


class Trozo:
    """Un pedazo de lo que se genera (una función, un dato, una rutina de apoyo)
    con su etiqueta. Los que no son «fijos» solo van al programa si alguien los
    nombra: así un programa no carga con lo que no usa de los módulos."""
    def __init__(self, etq, lineas, fijo):
        self.etq = etq
        self.lineas = lineas
        self.vivo = fijo


class Var:
    def __init__(self, tipo, lugar=None, ref=False, off=0):
        self.tipo = tipo
        self.lugar = lugar          # (base, símbolo, desplazamiento)
        self.ref = ref              # parámetro por referencia: en [bp+off] va la dirección
        self.off = off


DIRECTIVA = {1: 'db ', 2: 'dw ', 4: 'dd '}


def potencia2(n):
    return n > 0 and n & (n - 1) == 0


def texto_lugar(r):
    base, sim, off = r
    s = base
    if sim:
        s = s + '+' + sim if s else sim
    if off or not s:
        s = s + (f'{off:+d}' if s else str(off))
    return s


# =============================================================================
#  4. Comprobaciones y generación de ensamblador NASM (.COM, solo 8086)
#
#  Convenios del código generado:
#   - Modelo «diminuto»: CS=DS=SS (un .COM). ES queda libre. DF=0 siempre.
#   - Una expresión deja su valor en AX (los de 8 bits, con AH=0); una de 32
#     bits, en DX:AX (gen_expr32; gen_expr da de ella la palabra baja). Evaluar
#     puede gastar AX, BX, CX, DX, SI, DI y ES; se conservan BP y SP.
#   - Los de 32 bits, en memoria: la palabra baja primero. Multiplicar, dividir
#     y desplazar con cuenta variable, con las rutinas castel_* (APOYO).
#   - Llamada: los argumentos se apilan de derecha a izquierda (cada uno una
#     palabra; los «ref», la dirección; los de 32 bits, dos: la alta primero);
#     la función los quita (ret n); el resultado vuelve en AX (DX:AX). Marco
#     con BP: parámetros en [bp+4...], locales en [bp-2...].
#   - Nombres en NASM: G_nombre (variable global), F_nombre (función),
#     T_n (textos), R_xx (registros que deja interrupción), L_n (saltos). Lo
#     de un módulo importado lleva su nombre: F_Módulo__nombre.
#   - Lo que se genera va en Trozos (cada función, cada dato, cada rutina de
#     apoyo); los de los módulos importados solo van al programa si alguien
#     los nombra (ver genera).
# =============================================================================
class Generador:
    def __init__(self, modulos):
        self.modulos = modulos      # en orden: cada uno, detrás de los que importa; el último, el programa
        self.lineas = []
        self.datos = []             # Trozos con valor inicial: van en el archivo
        self.bss = []               # Trozos sin él: detrás, a cero al arrancar
        self.cadenas = {}
        self.locales = {}
        self.nlbl = 0
        self.bucles = []            # (etiqueta de seguir, etiqueta de romper)
        self.ban_memoria = False    # ¿alguien lee la copia de las banderas (R_ban)?
        self.func = None
        self.principal = None       # la función principal() del programa
        self.usa(modulos[-1])

    # ---- utilidades ----
    def usa(self, m):
        """Desde ahora, los nombres son los del módulo m."""
        self.mod = m
        self.consts = m.consts
        self.tipos = m.tipos
        self.globales = m.globales
        self.funcs = m.funcs

    def en(self, m, f):
        """Hace f() con los nombres del módulo m y vuelve al de antes."""
        antes = self.mod
        self.usa(m)
        try:
            return f()
        finally:
            self.usa(antes)

    def error(self, msg, pos):
        e = ErrorCastel(msg, pos)
        e.archivo = self.mod.ruta
        raise e

    def no_esta(self, que, nombre, pos):
        """El error de un nombre que no se encuentra (con la pista, si es cosa de módulos)."""
        if nombre in self.mod.ambiguos:
            ms = self.mod.ambiguos[nombre]
            self.error(f"«{nombre}» está en los módulos {' y '.join(ms)}: escriba «{ms[0]}.{nombre}»"
                       f" o «{ms[1]}.{nombre}»", pos)
        if '.' in nombre:
            m, n = nombre.split('.', 1)
            self.error(f"el módulo {m} no tiene nada público que se llame «{n}»", pos)
        self.error(f"{que}: «{nombre}»", pos)

    def nueva(self):
        self.nlbl += 1
        return f"L_{self.nlbl}"

    def em(self, ln):
        self.lineas.append('    ' + ln)

    def et(self, lbl):
        self.lineas.append(lbl + ':')

    def etiqueta_cadena(self, s):
        if s not in self.cadenas:
            lbl = f"T_{len(self.cadenas) + 1}"
            self.cadenas[s] = lbl
            self.datos.append(Trozo(lbl, [f"{lbl}: db {','.join(str(b) for b in s + bytes([0]))}"], False))
        return self.cadenas[s]

    def declarado(self, nombre, pos, local=False):
        if nombre in PALABRAS or nombre in INTERNAS or nombre in BASICOS:
            self.error(f"«{nombre}» es un nombre del lenguaje", pos)
        if local:
            if nombre in self.locales:
                self.error(f"«{nombre}» ya está declarado en esta función", pos)
            return
        if nombre in self.consts or nombre in self.globales or nombre in self.funcs or nombre in self.tipos:
            self.error(f"«{nombre}» ya está declarado", pos)

    # ---- tipos escritos -> Tipo ----
    def resuelve(self, t):
        if t.k == 'tnom':
            if t.nombre not in self.tipos:
                self.no_esta("tipo desconocido", t.nombre, t.pos)
            return self.tipos[t.nombre]
        if t.k == 'tpunt':
            m = self.mod                        # lo apuntado se mira más tarde, pero en este módulo
            return Tipo('punt', 2, lambda: self.en(m, lambda: self.resuelve(t.base)))
        if t.k == 'tarr':
            n = self.const(t.n)
            if n is None:
                self.error("el tamaño de un arreglo es una constante", t.n.pos)
            if n < 1:
                self.error("un arreglo tiene al menos un elemento", t.n.pos)
            base = self.resuelve(t.base)
            if n * base.tam > 0xFFFF:
                self.error("el arreglo no cabe en un segmento", t.pos)
            return Tipo('arr', n * base.tam, base, n)
        if t.k == 'test':
            campos = {}
            off = 0
            for nombres, tc in t.campos:
                tt = self.resuelve(tc)
                for nm, pos in nombres:
                    if nm in campos:
                        self.error(f"campo repetido: «{nm}»", pos)
                    campos[nm] = (off, tt)
                    off += tt.tam
            if not campos:
                self.error("una estructura sin campos", t.pos)
            return Tipo('est', off, campos=campos)
        raise AssertionError(t.k)

    # ---- lo que declara un módulo (antes de generar nada) ----
    def declara(self, m):
        decls = m.arbol.decls
        propios = set()
        for d in decls:
            if d.k in ('const', 'tipo', 'func'):
                propios.add(d.nombre)
            elif d.k == 'var':
                propios.update(d.nombres)
        propios.update(x.nombre for x in recorre(decls) if x.k == 'const')
        # lo público de los módulos importados: siempre como Módulo.nombre y, si no
        # choca con nada (lo propio manda; lo que traen dos módulos, ninguno), a secas
        traidos = {}
        for imp in m.importa:
            for nombre, (tabla, obj) in imp.publicos.items():
                getattr(m, tabla)[f'{imp.nombre}.{nombre}'] = obj
                if nombre in propios:
                    continue
                if nombre in m.ambiguos:
                    m.ambiguos[nombre].append(imp.nombre)
                elif nombre in traidos:
                    otro, tabla2 = traidos.pop(nombre)
                    del getattr(m, tabla2)[nombre]
                    m.ambiguos[nombre] = [otro, imp.nombre]
                else:
                    getattr(m, tabla)[nombre] = obj
                    traidos[nombre] = (imp.nombre, tabla)

        def publica(d, tabla, nombre):
            if getattr(d, 'publico', False):
                m.publicos[nombre] = (tabla, getattr(m, tabla)[nombre])
        for d in decls:
            if d.k == 'const':
                self.decl_const(d)
                publica(d, 'consts', d.nombre)
            elif d.k == 'tipo':
                self.declarado(d.nombre, d.pos)
                t = self.resuelve(d.t)
                if t.k == 'est' and t.nombre is None:
                    t.nombre = d.nombre
                self.tipos[d.nombre] = t
                publica(d, 'tipos', d.nombre)
            elif d.k == 'var':
                self.decl_global(d)
                for nm in d.nombres:
                    publica(d, 'globales', nm)
            elif d.k == 'func':
                self.declarado(d.nombre, d.pos)
                d.mod = m
                d.etq = m.etiqueta('F', d.nombre)
                d.fin = m.etiqueta('X', d.nombre)
                self.funcs[d.nombre] = d
                publica(d, 'funcs', d.nombre)
        for d in decls:
            if d.k == 'func':
                d.tret = self.resuelve(d.t) if d.t else T_NADA
                if d.tret.k in ('arr', 'est'):
                    self.error("una función no devuelve arreglos ni estructuras (use un parámetro «ref»)", d.t.pos)
                d.ptipos = [self.resuelve(p.t) for p in d.params]
                d.poff = []                     # dónde cae cada parámetro: [bp+4], [bp+6]...
                off = 4
                for p, t in zip(d.params, d.ptipos):
                    if t.k in ('arr', 'est') and not p.ref:
                        self.error("los arreglos y las estructuras se pasan con «ref»", p.pos)
                    d.poff.append(off)
                    off += 4 if es32(t) and not p.ref else 2
                d.nbytes = off - 4

    # ---- el programa entero ----
    def genera(self):
        for m in self.modulos:
            self.usa(m)
            self.declara(m)
        prog = self.modulos[-1]
        self.usa(prog)
        pr = prog.funcs.get('principal')
        if pr is None or pr.mod is not prog:
            self.error("falta la función principal()", prog.arbol.pos)
        if pr.params:
            self.error("principal() no lleva parámetros", pr.pos)
        self.principal = pr
        codigo = []                             # primero el programa; detrás, los módulos
        for m in [prog] + self.modulos[:-1]:
            self.usa(m)
            for d in m.arbol.decls:
                if d.k == 'func':
                    self.lineas = []
                    self.gen_funcion(d)
                    codigo.append(Trozo(d.etq, self.lineas, m.principal))
                elif d.k == 'asm':
                    self.func = None
                    self.locales = {}
                    self.lineas = ['']
                    self.gen_asm(d)
                    codigo.append(Trozo(None, self.lineas, True))
        self.usa(prog)
        if not self.ban_memoria:
            # todos los acarreo() han salido con «jc»: la copia de las banderas sobra
            for t in codigo:
                c = t.lineas
                t.lineas = [ln for i, ln in enumerate(c)
                            if not (ln == '    pop word [R_ban]' and c[i - 1] == '    pushf')
                            and not (ln == '    pushf' and c[i + 1:i + 2] == ['    pop word [R_ban]'])]
        for r in sorted(set(SALIDAS_INT.values())):
            self.bss.append(Trozo(f'R_{r}', [f"R_{r}: resw 1"], False))
        apoyo = [Trozo(nombre, txt.strip('\n').split('\n'), False) for nombre, txt in APOYO.items()]
        # qué va al programa: lo fijo y, de lo demás, lo que alguien nombre
        todos = codigo + apoyo + self.datos + self.bss
        palabras = set()
        pendientes = [t for t in todos if t.vivo]
        while pendientes:
            for t in pendientes:
                palabras.update(re.findall(r'\w+', '\n'.join(t.lineas)))
            pendientes = [t for t in todos if not t.vivo and t.etq in palabras]
            for t in pendientes:
                t.vivo = True

        def vivos(lista):
            return [ln for t in lista if t.vivo for ln in t.lineas]
        bss = vivos(self.bss)
        cab = [
            '; -----------------------------------------------------------------',
            f'; Generado por ccastel desde el módulo {prog.nombre}. NO editar a mano.',
            '; Ensamblar:  nasm -f bin ESTE.asm -o PROGRAMA.COM',
            '; -----------------------------------------------------------------',
            'cpu 8086',
            'bits 16',
            'org 0x100',
            '    cld',
        ]
        if bss:
            cab += ['    mov di, _vacio', '    mov cx, _vacio_fin-_vacio', '    xor al, al', '    rep stosb']
        cab.append('    call F_principal')
        cab.append('    mov ah, 0x4C' if pr.tret is not T_NADA else '    mov ax, 0x4C00')
        cab.append('    int 0x21')
        sal = cab + vivos(codigo) + vivos(apoyo)
        datos = vivos(self.datos)
        if datos:
            sal += ['', '; ---- datos ----'] + datos
        if bss:
            sal += ['', '; ---- sin valor inicial: no van en el archivo; a cero al arrancar ----',
                    '_vacio:', '    absolute _vacio'] + bss + ['_vacio_fin:']
        if '_fin_programa' in palabras:
            sal.append('_fin_programa:')        # el primer octeto libre detrás del programa
        return '\n'.join(limpia(sal)) + '\n'

    def decl_const(self, d):
        self.declarado(d.nombre, d.pos)
        if d.e.k == 'cad':
            self.consts[d.nombre] = ('cad', self.etiqueta_cadena(d.e.s))
            return
        v = self.const(d.e)
        if v is None:
            self.error("una constante es un número, un texto o una cuenta con constantes", d.e.pos)
        self.rango(v, T_LIT, d.e.pos)
        self.consts[d.nombre] = ('bool' if self.tipo(d.e).k == 'bool' else 'num', v)

    def decl_global(self, d):
        t = self.resuelve(d.t)
        for nm in d.nombres:
            self.declarado(nm, d.pos)
            etq = self.mod.etiqueta('G', nm)
            self.globales[nm] = Var(t, ('', etq, 0))
            if d.ini is None:
                self.bss.append(Trozo(etq, [f"{etq}: resb {t.tam}"], self.mod.principal))
            else:
                self.datos.append(Trozo(etq, [f"{etq}: " + self.dato_inicial(t, d.ini)], self.mod.principal))

    def dato_inicial(self, t, ini):
        if t.k == 'arr':
            if ini.k == 'cad' or (ini.k == 'ref' and self.consts.get(ini.nombre, ('',))[0] == 'cad'):
                if t.base.tam != 1:
                    self.error("un texto solo inicia un arreglo de byte", ini.pos)
                if ini.k == 'cad':
                    s = ini.s
                else:
                    s = [k for k, v in self.cadenas.items() if v == self.consts[ini.nombre][1]][0]
                if len(s) + 1 > t.n:
                    self.error(f"el texto ({len(s)} letras y el 0 final) no cabe en el arreglo de {t.n}", ini.pos)
                octs = list(s) + [0] * (t.n - len(s))
                return 'db ' + ','.join(str(b) for b in octs)
            if ini.k != 'lista':
                self.error("un arreglo se inicia con una lista (a, b, c) o con un texto", ini.pos)
            if t.base.k in ('arr', 'est'):
                self.error("solo se inician arreglos de números, bool o punteros", ini.pos)
            if len(ini.elems) > t.n:
                self.error(f"sobran valores: el arreglo es de {t.n}", ini.elems[t.n].pos)
            vals = [self.valor_inicial(t.base, e) for e in ini.elems]
            vals += ['0'] * (t.n - len(vals))
            return DIRECTIVA[t.base.tam] + ','.join(vals)
        if t.k == 'est':
            self.error("las estructuras no llevan valor inicial (se rellenan campo a campo)", ini.pos)
        return DIRECTIVA[t.tam] + self.valor_inicial(t, ini)

    def valor_inicial(self, t, e):
        if e.k == 'lista':
            self.error("la lista de valores es para los arreglos", e.pos)
        self.compatible(t, e, "el valor inicial")
        if es32(t):
            c = self.const(e)
            if c is None:
                self.error("el valor inicial de una variable global tiene que ser constante", e.pos)
            return str(c & MAX32)
        im = self.inmediato(e)
        if im is None:
            self.error("el valor inicial de una variable global tiene que ser constante", e.pos)
        return im

    # ---- funciones ----
    def gen_funcion(self, d):
        self.func = d
        self.locales = {}
        self.fin_usado = False
        nparams = len(d.params)
        for i, (p, t) in enumerate(zip(d.params, d.ptipos)):
            self.declarado(p.nombre, p.pos, local=True)
            if p.ref:
                self.locales[p.nombre] = Var(t, None, ref=True, off=d.poff[i])
            else:
                self.locales[p.nombre] = Var(t, ('bp', '', d.poff[i]))
        marco = 0
        tipo_de = {}                            # el tipo de cada local (se usan antes de declararse)
        for s in recorre(d.cuerpo):
            if s.k == 'var' and not s.global_:
                s.tipo_res = self.resuelve(s.t)
                for nm in s.nombres:
                    tipo_de.setdefault(nm, s.tipo_res)
        for s in recorre(d.cuerpo):
            if s.k == 'var' and not s.global_:
                t = s.tipo_res
                for nm in s.nombres:
                    self.declarado(nm, s.pos, local=True)
                    marco += (t.tam + 1) & ~1
                    self.locales[nm] = Var(t, ('bp', '', -marco))
            elif s.k == 'para':
                # sitio para el tope, si no es constante (dos palabras si la variable es de 32 bits)
                nm = s.var.nombre
                v = self.locales.get(nm) or self.globales.get(nm)
                tv = tipo_de.get(nm) or (v.tipo if v else None)
                marco += 4 if tv is not None and es32(tv) else 2
                s.tope = ('bp', '', -marco)
        d.usa_int = {x.nombre for x in recorre(d.cuerpo) if x.k == 'llamada' and x.nombre in SALIDAS_INT}
        if d.tret is not T_NADA and not any(s.k in ('devolver', 'asm') for s in recorre(d.cuerpo)):
            self.error(f"{d.nombre}() devuelve un {d.tret}, pero no tiene ningún «devolver»", d.pos)
        con_marco = bool(marco or nparams or any(s.k == 'asm' for s in recorre(d.cuerpo)))
        self.con_marco = con_marco
        self.fin_func = d.fin
        self.lineas.append('')
        self.et(d.etq)
        if con_marco:
            self.em('push bp')
            self.em('mov bp, sp')
            if marco:
                self.em(f'sub sp, {marco}')
        self.gen_bloque(d.cuerpo, ultimo=True)
        if self.fin_usado:
            self.et(self.fin_func)
        if con_marco:
            if marco:
                self.em('mov sp, bp')
            self.em('pop bp')
        self.em(f'ret {d.nbytes}' if nparams else 'ret')
        self.func = None

    def gen_bloque(self, sents, ultimo=False):
        for i, s in enumerate(sents):
            self.gen_sent(s, ultimo and i == len(sents) - 1)

    # ---- nombres ----
    def var(self, nombre, pos):
        if nombre in self.locales:
            return self.locales[nombre]
        if nombre in self.globales:
            return self.globales[nombre]
        if nombre in self.consts:
            self.error(f"«{nombre}» es una constante, no una variable", pos)
        if nombre in self.funcs or nombre in INTERNAS:
            self.error(f"«{nombre}» es una función: se llama con paréntesis", pos)
        self.no_esta("nombre no declarado", nombre, pos)

    def es_var(self, nombre):
        return nombre in self.locales or nombre in self.globales

    def es_lugar(self, e):
        if e.k == 'ref':
            return self.es_var(e.nombre)
        if e.k in ('campo', 'indice'):
            return True
        return e.k == 'llamada' and e.nombre == 'valor'

    # ---- constantes: el valor de e si se sabe al compilar, o None ----
    def const(self, e):
        if not hasattr(e, '_c'):
            e._c = self._const(e)
        return e._c

    def _const(self, e):
        k = e.k
        if k in ('num', 'bool'):
            return e.v
        if k == 'ref':
            c = self.consts.get(e.nombre)
            if c and c[0] in ('num', 'bool') and not self.es_var(e.nombre):
                return c[1]
            return None
        if k == 'un':
            x = self.const(e.x)
            if x is None:
                return None
            self.comprueba(e)
            ancho = MAX32 if es32(self.tipo(e.x)) or not cabe16(x) else 0xFFFF
            v = {'-': -x, 'nob': ~x & ancho, 'no': 1 - x}[e.op]
            if not MIN32 <= v <= MAX32:
                self.error("la cuenta con constantes no cabe en 32 bits", e.pos)
            return v
        if k == 'bin':
            a, b = self.const(e.a), self.const(e.b)
            if a is None or b is None:
                return None
            self.comprueba(e)
            op = e.op
            if op in ('div', 'mod') and b == 0:
                self.error("división por cero", e.pos)
            # uno de 32 bits (o un número que no cabe en 16) se desplaza como uno de 32
            ta, tb = self.tipo(e.a), self.tipo(e.b)
            largo = es32(ta) or not cabe16(a)
            ancho = MAX32 if largo else 0xFFFF
            if op in ('<<', '>>') and not 0 <= b <= (31 if largo else 15):
                self.error(f"se desplaza de 0 a {31 if largo else 15} bits", e.b.pos)
            if op == 'div':
                q = abs(a) // abs(b)
                return q if (a < 0) == (b < 0) else -q
            if op == 'mod':
                r = abs(a) % abs(b)
                return r if a >= 0 else -r
            v = {'+': lambda: a + b, '-': lambda: a - b, '*': lambda: a * b,
                 'yb': lambda: a & b, 'ob': lambda: a | b, 'xob': lambda: a ^ b,
                 '<<': lambda: (a << b) & ancho, '>>': lambda: (a & ancho) >> b if a >= 0 else a >> b,
                 '=': lambda: int(a == b), '<>': lambda: int(a != b), '<': lambda: int(a < b),
                 '<=': lambda: int(a <= b), '>': lambda: int(a > b), '>=': lambda: int(a >= b),
                 'y': lambda: a & b, 'o': lambda: a | b}[op]()
            if es_num(ta) and es_num(tb) and not (es_lit(ta) and es_lit(tb)) \
                    and op not in ('=', '<>', '<', '<=', '>', '>='):
                # con un tipo por medio, la cuenta da la vuelta como al ejecutar
                if op not in ('<<', '>>'):
                    largo = es32(ta) or es32(tb)
                    signo = self.con_signo(ta, tb)
                else:
                    signo = ta.k in ('ent16', 'ent32')
                v &= MAX32 if largo else 0xFFFF
                if signo and v >> (31 if largo else 15):
                    v -= 1 << (32 if largo else 16)
            if not MIN32 <= v <= MAX32:
                self.error("la cuenta con constantes no cabe en 32 bits", e.pos)
            return v
        if k == 'llamada' and e.nombre == 'tamaño':
            return self.tipo_de_tamano(e).tam
        if k == 'llamada' and e.nombre in CONVERSIONES and len(e.args) == 1:
            x = self.const(e.args[0][1])
            if x is None:
                return None
            self.comprueba(e)
            if e.nombre in ('nat8', 'byte'):
                return x & 0xFF
            if e.nombre in ('nat32', 'ent32'):
                x &= MAX32
                return x - 0x100000000 if e.nombre == 'ent32' and x >= 0x80000000 else x
            x &= 0xFFFF
            return x - 0x10000 if e.nombre == 'ent16' and x >= 0x8000 else x
        return None

    def rango(self, v, t, pos):
        if t.k == 'nat8' and not -128 <= v <= 255:
            self.error(f"{v} no cabe en un byte (0 a 255)", pos)
        if t.k == 'nat16' and not -0x8000 <= v <= 0xFFFF:
            self.error(f"{v} no cabe en nat16 (0 a 65535)", pos)
        if t.k == 'ent16' and not -0x8000 <= v <= 0xFFFF:
            self.error(f"{v} no cabe en ent16 (-32768 a 32767)", pos)
        if t.k == 'nat32' and not MIN32 <= v <= MAX32:
            self.error(f"{v} no cabe en nat32 (0 a 4294967295)", pos)
        if t.k == 'ent32' and not MIN32 <= v <= MAX32:
            self.error(f"{v} no cabe en ent32 (-2147483648 a 2147483647)", pos)
        if es_lit(t) and not MIN32 <= v <= MAX32:
            self.error(f"{v} no cabe en 32 bits", pos)

    def tipo_de_tamano(self, e):
        if len(e.args) != 1 or e.args[0][0]:
            self.error("tamaño() lleva un tipo o una variable", e.pos)
        a = e.args[0][1]
        if a.k == 'ref' and a.nombre in self.tipos and not self.es_var(a.nombre):
            return self.tipos[a.nombre]
        if not self.es_lugar(a):
            self.error("tamaño() lleva un tipo o una variable", a.pos)
        return self.tipo(a)

    # ---- el tipo de una expresión (con sus comprobaciones) ----
    def tipo(self, e):
        if not hasattr(e, '_t'):
            e._tipando = True
            e._t = self._tipo(e)
        return e._t

    def comprueba(self, e):
        """Las comprobaciones de tipos de e, si no se están haciendo ya."""
        if not hasattr(e, '_t') and not getattr(e, '_tipando', False):
            self.tipo(e)

    def valor_tipo(self, e, que="aquí"):
        t = self.tipo(e)
        if t.k == 'nada':
            self.error("esa función no devuelve nada", e.pos)
        if t.k == 'est':
            self.error(f"una estructura entera no vale {que}: use sus campos", e.pos)
        return t

    def _tipo(self, e):
        k = e.k
        if k == 'num':
            return tipo_lit(e.v)
        if k == 'bool':
            return T_BOOL
        if k == 'nulo':
            return T_PUNT_U
        if k == 'cad':
            return T_TEXTO
        if k == 'ref':
            if self.es_var(e.nombre):
                return self.var(e.nombre, e.pos).tipo
            c = self.consts.get(e.nombre)
            if c:
                if c[0] == 'num':
                    return tipo_lit(c[1])
                return {'bool': T_BOOL, 'cad': T_TEXTO}[c[0]]
            if e.nombre in self.tipos:
                self.error(f"«{e.nombre}» es un tipo, no un valor", e.pos)
            self.var(e.nombre, e.pos)
        if k == 'un':
            t = self.valor_tipo(e.x)
            if e.op == 'no':
                if t.k != 'bool':
                    self.error(f"«no» niega un bool, no un {t} (para los bits, «nob»)", e.pos)
                return T_BOOL
            if not es_num(t):
                self.error(f"«{e.op}» es para números, no para {t}", e.pos)
            if es_lit(t):
                return tipo_lit(self.const(e))
            if e.op == '-':
                return T_ENT32 if es32(t) else T_ENT16
            if es32(t):
                return t
            return T_ENT16 if t.k == 'ent16' else T_NAT16
        if k == 'bin':
            return self.tipo_bin(e)
        if k == 'indice':
            to = self.valor_tipo(e.obj)
            ti = self.valor_tipo(e.idx)
            if to.k not in ('arr', 'punt'):
                self.error(f"solo se indexan los arreglos y los punteros, no un {to}", e.pos)
            if to.k == 'punt' and to._base is None:
                self.error("un puntero sin tipo no se puede indexar", e.pos)
            if not es_num(ti):
                self.error(f"el índice es un número, no un {ti}", e.idx.pos)
            c = self.const(e.idx)
            if to.k == 'arr' and c is not None and not 0 <= c < to.n:
                self.error(f"índice {c} fuera del arreglo (va de 0 a {to.n - 1})", e.idx.pos)
            return to.base
        if k == 'campo':
            to = self.tipo(e.obj)
            if to.k == 'punt' and to._base is not None and to.base.k == 'est':
                to = to.base
            if to.k != 'est':
                self.error(f"«.{e.nombre}»: lo de la izquierda no es una estructura, es un {to}", e.pos)
            if e.nombre not in to.campos:
                self.error(f"la estructura {to} no tiene el campo «{e.nombre}»", e.pos)
            return to.campos[e.nombre][1]
        if k == 'llamada':
            return self.tipo_llamada(e)
        raise AssertionError(k)

    def tipo_bin(self, e):
        op = e.op
        ta = self.valor_tipo(e.a)
        tb = self.valor_tipo(e.b)
        if op in ('y', 'o'):
            for x, t in ((e.a, ta), (e.b, tb)):
                if t.k != 'bool':
                    self.error(f"«{op}» une condiciones (bool), no un {t}"
                               f" (para los bits, «{op}b»)", x.pos)
            return T_BOOL
        if op in ('=', '<>', '<', '<=', '>', '>='):
            if es_num(ta) and es_num(tb):
                return T_BOOL
            if ta.k == 'bool' and tb.k == 'bool' and op in ('=', '<>'):
                return T_BOOL
            pa = ta.k in ('punt', 'arr')
            pb = tb.k in ('punt', 'arr')
            if pa and pb:
                return T_BOOL
            self.error(f"no se puede comparar un {ta} con un {tb}", e.pos)
        if op in ('+', '-') and ta.k == 'punt' and es_num(tb):
            if ta._base is None:
                self.error("a un puntero sin tipo no se le suma", e.pos)
            return ta
        if op == '+' and tb.k == 'punt' and es_num(ta):
            if tb._base is None:
                self.error("a un puntero sin tipo no se le suma", e.pos)
            return tb
        if op == '-' and ta.k == 'punt' and tb.k in ('punt', 'arr'):
            # (el nombre de un arreglo de byte vale como puntero a su primer elemento)
            if ta._base is None or tb._base is None or ta.base.tam != 1 or not igual(ta.base, tb.base):
                self.error("solo se restan dos punteros a byte", e.pos)
            return T_NAT16
        for x, t in ((e.a, ta), (e.b, tb)):
            if not es_num(t):
                if t.k == 'bool' and op in ('yb', 'ob', 'xob'):
                    self.error(f"«{op}» es para los bits de un número; con bool se usa «{op[:-1]}»", x.pos)
                self.error(f"«{op}» es para números, no para {t}", x.pos)
        if es_lit(ta) and es_lit(tb):
            return tipo_lit(self.const(e))
        if op in ('<<', '>>'):              # manda el que se desplaza
            if es32(ta):
                return T_ENT32 if ta.k == 'ent32' else T_NAT32
            return T_ENT16 if ta.k == 'ent16' else T_NAT16
        signo = 'ent16' in (ta.k, tb.k) or 'ent32' in (ta.k, tb.k)
        if es32(ta) or es32(tb):
            return T_ENT32 if signo else T_NAT32
        return T_ENT16 if signo else T_NAT16

    def con_signo(self, ta, tb):
        return 'ent16' in (ta.k, tb.k) or 'ent32' in (ta.k, tb.k) or (es_lit(ta) and es_lit(tb))

    def args_simples(self, e, n, nmax=None):
        if any(nm for nm, _ in e.args):
            self.error(f"{e.nombre}() no lleva argumentos con nombre", e.args[0][1].pos)
        if not (n <= len(e.args) <= (nmax or n)):
            cuantos = f"{n}" if not nmax else f"de {n} a {nmax}"
            self.error(f"{e.nombre}() lleva {cuantos} argumento{'s' if (nmax or n) != 1 else ''}"
                       f" y hay {len(e.args)}", e.pos)
        return [a for _, a in e.args]

    def numero(self, a, que):
        t = self.valor_tipo(a)
        if not es_num(t):
            self.error(f"{que} es un número, no un {t}", a.pos)

    def tipo_llamada(self, e):
        nm = e.nombre
        if self.es_var(nm):
            self.error(f"«{nm}» es una variable, no una función", e.pos)
        if nm in self.funcs:
            f = self.funcs[nm]
            args = self.args_simples(e, len(f.params))
            for a, p, t in zip(args, f.params, f.ptipos):
                if p.ref:
                    if not self.es_lugar(a):
                        self.error(f"el parámetro «{p.nombre}» es «ref»: hay que pasarle una variable", a.pos)
                    ta = self.tipo(a)
                    if not igual(ta, t) or (ta.k == 'punt' and (ta._base is None) != (t._base is None)):
                        self.error(f"el parámetro «{p.nombre}» es ref {t} y se le pasa un {ta}", a.pos)
                else:
                    self.compatible(t, a, f"el parámetro «{p.nombre}»")
            return f.tret
        if nm == 'escribir':
            if not e.args:
                self.error("escribir() lleva al menos un argumento", e.pos)
            for a in self.args_simples(e, 1, 99):
                t = self.valor_tipo(a)
                if not (es_num(t) or t.k == 'bool' or self.es_texto(t)):
                    self.error(f"escribir() no sabe enseñar un {t}", a.pos)
            return T_NADA
        if nm == 'escribir_car':
            self.numero(self.args_simples(e, 1)[0], "la letra")
            return T_NADA
        if nm in ('meter_puerto', 'leer_byte', 'leer_palabra', 'sacar_puerto', 'poner_byte', 'poner_palabra'):
            n = {'meter_puerto': 1, 'leer_byte': 2, 'leer_palabra': 2,
                 'sacar_puerto': 2, 'poner_byte': 3, 'poner_palabra': 3}[nm]
            for a in self.args_simples(e, n):
                self.numero(a, f"cada argumento de {nm}()")
            return {'meter_puerto': T_NAT8, 'leer_byte': T_NAT8, 'leer_palabra': T_NAT16}.get(nm, T_NADA)
        if nm == 'dirección':
            a = self.args_simples(e, 1)[0]
            if not self.es_lugar(a):
                if a.k == 'cad' or self.tipo(a) is T_TEXTO:
                    return T_TEXTO
                self.error("dirección() lleva una variable (o un elemento o un campo)", a.pos)
            t = self.tipo(a)
            return Tipo('punt', 2, t.base if t.k == 'arr' else t)
        if nm == 'valor':
            a = self.args_simples(e, 1)[0]
            t = self.valor_tipo(a)
            if t.k != 'punt':
                self.error(f"valor() lleva un puntero, no un {t}", a.pos)
            if t._base is None:
                self.error("valor() de un puntero sin tipo: asígnelo antes a un «puntero a ...»", a.pos)
            return t.base
        if nm == 'puntero':
            a = self.args_simples(e, 1)[0]
            t = self.valor_tipo(a)
            if not (es_num(t) or t.k in ('punt', 'arr')):
                self.error(f"puntero() lleva un número o un puntero, no un {t}", a.pos)
            return T_PUNT_U
        if nm in CONVERSIONES:
            a = self.args_simples(e, 1)[0]
            t = self.valor_tipo(a)
            if t.k == 'arr':
                self.error(f"{nm}() no convierte un arreglo", a.pos)
            return BASICOS[nm]
        if nm == 'fin_programa':
            self.args_simples(e, 0)
            return T_PUNT_U
        if nm == 'tamaño':
            self.tipo_de_tamano(e)
            return T_LIT
        if nm == 'interrupción':
            self.prepara_int(e)
            return T_NAT16
        if nm in SALIDAS_INT:
            self.args_simples(e, 0)
            return T_BOOL if nm == 'acarreo' else T_NAT16
        if nm == 'salir':
            for a in self.args_simples(e, 0, 1):
                self.numero(a, "el código de salida")
            return T_NADA
        if nm in self.tipos:
            self.error(f"«{nm}» es un tipo; solo convierten nat8(), nat16(), ent16(), nat32(), ent32() y puntero()", e.pos)
        self.no_esta("función no declarada", nm, e.pos)

    def es_texto(self, t):
        return t.k in ('punt', 'arr') and t._base is not None and t.base.k == 'nat8'

    def compatible(self, dest, e, que):
        """¿Se puede guardar e en algo de tipo dest? Si no, error."""
        t = self.valor_tipo(e)
        c = self.const(e)
        if dest.k in ('arr', 'est'):
            self.error(f"{que}: los arreglos y las estructuras no se copian enteros"
                       " (elemento a elemento o campo a campo)", e.pos)
        if es_num(dest):
            if self.es_texto(t) and (e.k == 'cad' or t is T_TEXTO):
                self.error(f"{que} es un número ({dest}) y se le da un texto", e.pos)
            if t.k in ('punt', 'arr'):
                self.error(f"{que} es un número ({dest}) y se le da un {t}"
                           " (para convertir, nat16(...))", e.pos)
            if t.k == 'bool':
                self.error(f"{que} es un número ({dest}) y se le da un bool", e.pos)
            if c is not None:
                self.rango(c, dest, e.pos)
            return
        if dest.k == 'bool':
            if t.k != 'bool':
                self.error(f"{que} es un bool y se le da un {t}", e.pos)
            return
        if dest.k == 'punt':
            if t.k == 'punt' and igual(dest, t):
                return
            if t.k == 'arr' and dest._base is not None and igual(dest.base, t.base):
                return
            if es_num(t) or t.k == 'bool':
                self.error(f"{que} es un {dest} y se le da un número"
                           " (para convertir, puntero(...); el puntero vacío es «nulo»)", e.pos)
            self.error(f"{que} es un {dest} y se le da un {t}", e.pos)
        raise AssertionError(dest.k)

    # ---- lugares: dónde vive una variable, un elemento o un campo ----
    # Un lugar es (base, símbolo, desplazamiento); la base: '' (dirección
    # fija), 'bp' (en el marco), 'bx' o 'bp+si' (calculada).
    def _lugar(self, e, emitir):
        k = e.k
        if k == 'ref':
            v = self.var(e.nombre, e.pos)
            if v.ref:
                if not emitir:
                    return None
                self.em(f'mov bx, [bp+{v.off}]')
                return ('bx', '', 0)
            return v.lugar
        if k == 'campo':
            to = self.tipo(e.obj)
            self.tipo(e)
            if to.k == 'punt':
                r = self.punt_a_bx(e.obj, emitir)
                est = to.base
            else:
                r = self._lugar(e.obj, emitir)
                est = to
            if r is None:
                return None
            return (r[0], r[1], r[2] + est.campos[e.nombre][0])
        if k == 'indice':
            obj, idx = e.obj, e.idx
            self.tipo(e)
        elif k == 'llamada' and e.nombre == 'valor':
            self.tipo(e)
            obj, idx = e.args[0][1], N('num', e.pos, v=0)
        else:
            self.error("aquí hace falta una variable, un elemento o un campo", e.pos)
        to = self.tipo(obj)
        tam = to.base.tam
        ci = self.const(idx)
        if to.k == 'arr':
            if ci is not None:
                r = self._lugar(obj, emitir)
                return r and (r[0], r[1], r[2] + ci * tam)
            if not emitir:
                return None
            if self.simple16(idx):
                r = self._lugar(obj, True)
                if r[0] == '':
                    self.carga_reg('bx', idx)
                    self.escala('bx', tam)
                    return ('bx', r[1], r[2])
                if r[0] == 'bp':
                    self.carga_reg('si', idx)
                    self.escala('si', tam)
                    return ('bp+si', '', r[2])
                r = self.a_bx(r)
                self.carga_reg('si', idx)
                self.escala('si', tam)
                self.em('add bx, si')
                return r
            self.gen_expr(idx)
            self.escala('ax', tam)
            r = self._lugar(obj, False)
            if r is not None and r[0] == '':
                self.em('mov bx, ax')
                return ('bx', r[1], r[2])
            if r is not None and r[0] == 'bp':
                self.em('mov si, ax')
                return ('bp+si', '', r[2])
            self.em('push ax')
            r = self.a_bx(self._lugar(obj, True))
            self.em('pop si')
            self.em('add bx, si')
            return r
        # puntero
        if ci is not None:
            r = self.punt_a_bx(obj, emitir)
            return r and ('bx', '', ci * tam)
        if not emitir:
            return None
        if self.simple16(idx):
            self.punt_a_bx(obj, True)
            self.carga_reg('si', idx)
            self.escala('si', tam)
        else:
            self.gen_expr(idx)
            self.escala('ax', tam)
            self.em('push ax')
            self.punt_a_bx(obj, True)
            self.em('pop si')
        self.em('add bx, si')
        return ('bx', '', 0)

    def punt_a_bx(self, obj, emitir):
        if not emitir:
            return None
        o = self.opnd(obj)
        if o and o[1] in (0, 2):
            self.em(f'mov bx, {o[0]}')
        else:
            self.gen_expr(obj)
            self.em('mov bx, ax')
        return ('bx', '', 0)

    def a_bx(self, r):
        if r[0] == 'bx':
            return r
        if r[0] == '':
            self.em(f'mov bx, {texto_lugar(r)}')
        else:
            self.em(f'lea bx, [{texto_lugar(r)}]')
        return ('bx', '', 0)

    def escala(self, reg, tam):
        if tam == 1:
            return
        if potencia2(tam) and tam <= 8:
            for _ in range(tam.bit_length() - 1):
                self.em(f'shl {reg}, 1')
        elif reg == 'ax':
            self.em(f'mov dx, {tam}')
            self.em('mul dx')
        else:
            self.em('push ax')
            self.em(f'mov ax, {tam}')
            self.em(f'mul {reg}')
            self.em(f'mov {reg}, ax')
            self.em('pop ax')

    def carga_reg(self, reg, e):
        self.em(f'mov {reg}, {self.opnd(e)[0]}')

    def lugar(self, e):
        return texto_lugar(self._lugar(e, True))

    def lugar_fijo(self, e):
        """El lugar de e si no hace falta código para llegar a él; si no, None."""
        if not self.es_lugar(e):
            return None
        r = self._lugar(e, False)
        return texto_lugar(r) if r else None

    def limpio(self, e):
        """¿Se llega al lugar de e sin gastar AX?"""
        k = e.k
        if k == 'ref':
            return True
        if k == 'campo':
            return self.simple16(e.obj) if self.tipo(e.obj).k == 'punt' else self.limpio(e.obj)
        if k == 'indice':
            obj, idx = e.obj, e.idx
        else:
            obj, idx = e.args[0][1], None
        if idx is not None and self.const(idx) is None and not self.simple16(idx):
            return False
        return self.simple16(obj) if self.tipo(obj).k == 'punt' else self.limpio(obj)

    # ---- operandos sencillos: lo que cabe en una instrucción sin calcular ----
    def inmediato(self, e):
        c = self.const(e)
        if c is not None:
            return str(c & 0xFFFF)
        if e.k == 'cad':
            return self.etiqueta_cadena(e.s)
        if e.k == 'nulo':
            return '0'
        if e.k == 'ref' and not self.es_var(e.nombre):
            c = self.consts.get(e.nombre)
            if c and c[0] == 'cad':
                return c[1]
            return None
        if e.k == 'llamada' and e.nombre == 'dirección':
            self.tipo(e)
            a = e.args[0][1]
            if not self.es_lugar(a):
                return self.inmediato(a)
            r = self._lugar(a, False)
            if r and r[0] == '':
                return texto_lugar(r)
            return None
        if e.k == 'llamada' and e.nombre == 'puntero':
            self.tipo(e)
            return self.inmediato(e.args[0][1])
        if self.es_lugar(e) and self.tipo(e).k == 'arr':
            r = self._lugar(e, False)
            if r and r[0] == '':
                return texto_lugar(r)
        return None

    def opnd(self, e):
        """(texto, tamaño): un inmediato (tamaño 0) o memoria directa (1 o 2)."""
        im = self.inmediato(e)
        if im is not None:
            return (im, 0)
        if self.es_lugar(e) and self.tipo(e).k not in ('arr', 'est'):
            r = self.lugar_fijo(e)
            if r:                               # de uno de 32 bits, su palabra baja
                return (f'[{r}]', min(self.tipo(e).tam, 2))
        return None

    def opnd32(self, e):
        """(baja, alta) si e vale tal cual como operando de 32 bits: una constante,
        una variable de 32 bits en sitio fijo o una natural de 16 (con 0 arriba)."""
        c = self.const(e)
        if c is not None:
            return mitades(c)
        self.tipo(e)
        while e.k == 'llamada' and e.nombre in ('nat32', 'ent32') and es32(self.tipo(e.args[0][1])):
            e = e.args[0][1]                    # nat32(x) con x ya de 32 bits: los mismos bits
        if self.es_lugar(e):
            t = self.tipo(e)
            if es32(t) or t.k == 'nat16':
                r = self._lugar(e, False)
                if r is not None:
                    return (f'[{texto_lugar(r)}]', f'[{texto_lugar(mas2(r))}]' if es32(t) else '0')
        return None

    def simple16(self, e):
        o = self.opnd(e)
        return bool(o) and o[1] in (0, 2)

    # ---- sentencias ----
    def gen_sent(self, s, ultimo=False):
        k = s.k
        if k == 'var':
            if s.ini is not None:
                if len(s.nombres) != 1 or s.tipo_res.k in ('arr', 'est'):
                    self.error("dentro de una función, el valor inicial es para una sola variable sencilla", s.ini.pos)
                self.gen_asig(N('ref', s.pos, nombre=s.nombres[0]), s.ini)
        elif k == 'const':
            self.decl_const(s)
        elif k == 'asig':
            self.gen_asig(s.dest, s.e)
        elif k == 'llamada_sent':
            self.tipo(s.e)
            if s.e.nombre not in self.funcs and s.e.nombre not in HACEN_ALGO:
                self.error(f"{s.e.nombre}() da un valor, y aquí se pierde: falta guardarlo o usarlo", s.e.pos)
            self.gen_llamada(s.e, sentencia=True)
        elif k == 'devolver':
            f = self.func
            if s.e is None:
                if f.tret is not T_NADA:
                    self.error(f"{f.nombre}() devuelve un {f.tret}: falta el valor", s.pos)
            else:
                if f.tret is T_NADA:
                    self.error(f"{f.nombre}() no devuelve nada (no lleva tipo)", s.e.pos)
                self.compatible(f.tret, s.e, "lo que se devuelve")
                if es32(f.tret):
                    self.gen_expr32(s.e)
                else:
                    self.gen_expr(s.e)
                    if f.tret.tam == 1 and self.tipo(s.e).tam != 1 and self.const(s.e) is None:
                        self.em('xor ah, ah')
            if not ultimo:
                self.fin_usado = True
                self.em(f'jmp {self.fin_func}')
        elif k == 'si':
            self.gen_si(s, ultimo)
        elif k == 'mientras':
            self.gen_mientras(s)
        elif k == 'para':
            self.gen_para(s)
        elif k == 'segun':
            self.gen_segun(s, ultimo)
        elif k == 'romper':
            if not self.bucles:
                self.error("«romper» fuera de un bucle", s.pos)
            self.em(f'jmp {self.bucles[-1][1]}')
        elif k == 'seguir':
            if not self.bucles:
                self.error("«seguir» fuera de un bucle", s.pos)
            self.em(f'jmp {self.bucles[-1][0]}')
        elif k == 'asm':
            self.gen_asm(s)
        else:
            raise AssertionError(k)

    def gen_asm(self, s):
        def cambia(m):
            nm = m.group(1)
            mas = int(m.group(2) or 0)          # {x+2}: dos octetos más allá (la palabra alta)
            if self.es_var(nm):
                v = self.var(nm, s.pos)
                if v.ref:
                    return f'[bp+{v.off}]'
                return f'[{texto_lugar((v.lugar[0], v.lugar[1], v.lugar[2] + mas))}]'
            if nm in self.consts:
                c = self.consts[nm]
                return c[1] if c[0] == 'cad' else str((c[1] + mas) & (0xFFFF if cabe16(c[1]) else MAX32))
            if nm in self.funcs:
                return self.funcs[nm].etq
            self.error(f"en el ensamblador, «{{{nm}}}» no es nada declarado en Castel",
                       (lin, ln.find('{' + nm) + 1))
        for ln, lin in s.lineas:
            codigo, punto, comentario = ln.partition(';')
            self.lineas.append(re.sub(r'\{([\w.]+)(?:\+(\d+))?\}', cambia, codigo) + punto + comentario)

    def tam_txt(self, t):
        return 'byte' if t.tam == 1 else 'word'

    def gen_asig(self, dest, e):
        if not self.es_lugar(dest):
            if dest.k == 'ref' and dest.nombre in self.consts:
                self.error(f"«{dest.nombre}» es una constante: no se le asigna", dest.pos)
            self.error("a la izquierda de «:=» va una variable, un elemento o un campo", dest.pos)
        td = self.tipo(dest)
        self.compatible(td, e, "el destino" if dest.k != 'ref' else f"«{dest.nombre}»")
        if es32(td):
            self.gen_asig32(dest, e)
            return
        tt = self.tam_txt(td)
        reg = 'al' if td.tam == 1 else 'ax'
        im = self.inmediato(e)
        fijo = self.lugar_fijo(dest)
        if im is not None:
            if td.tam == 1:
                im = str(int(im) & 0xFF)
            self.em(f'mov {tt} [{fijo or self.lugar(dest)}], {im}')
            return
        # x := x + n, x := x yb n...: sobre la propia memoria
        if fijo and e.k == 'bin' and e.op in ('+', '-', 'yb', 'ob', 'xob') and self.es_lugar(e.a) \
                and self.lugar_fijo(e.a) == fijo and self.const(e.b) is not None:
            self.tipo(e)
            n = self.const(e.b)
            if td.k == 'punt':
                n *= td.base.tam
            n &= 0xFF if td.tam == 1 else 0xFFFF
            if e.op in ('+', '-') and n == 1:
                self.em(f"{'inc' if e.op == '+' else 'dec'} {tt} [{fijo}]")
            else:
                ins = {'+': 'add', '-': 'sub', 'yb': 'and', 'ob': 'or', 'xob': 'xor'}[e.op]
                self.em(f'{ins} {tt} [{fijo}], {n}')
            return
        if td.tam == 1:
            self.gen_al(e)
        else:
            self.gen_expr(e)
        if fijo:
            self.em(f'mov [{fijo}], {reg}')
        elif self.limpio(dest):
            self.em(f'mov [{self.lugar(dest)}], {reg}')
        else:
            self.em('push ax')
            lug = self.lugar(dest)
            self.em('pop ax')
            self.em(f'mov [{lug}], {reg}')

    def gen_asig32(self, dest, e):
        """dest := e, con dest de 32 bits (dos palabras: la baja, primero)."""
        r = self._lugar(dest, False)
        c = self.const(e)
        if c is not None:
            if r is None:
                r = self._lugar(dest, True)
            baja, alta = mitades(c)
            self.em(f'mov word [{texto_lugar(r)}], {baja}')
            self.em(f'mov word [{texto_lugar(mas2(r))}], {alta}')
            return
        # x := x + n, x := x yb n...: sobre la propia memoria
        if r and e.k == 'bin' and e.op in ('+', '-', 'yb', 'ob', 'xob') and self.es_lugar(e.a) \
                and es32(self.tipo(e.a)) and self._lugar(e.a, False) == r and self.const(e.b) is not None:
            self.tipo(e)
            baja, alta = mitades(self.const(e.b))
            i1, i2 = self.INS32[e.op]
            self.em(f'{i1} word [{texto_lugar(r)}], {baja}')
            self.em(f'{i2} word [{texto_lugar(mas2(r))}], {alta}')
            return
        self.gen_expr32(e)
        if r is None:
            self.em('push dx')
            self.em('push ax')
            r = self._lugar(dest, True)
            self.em('pop ax')
            self.em('pop dx')
        self.em(f'mov [{texto_lugar(r)}], ax')
        self.em(f'mov [{texto_lugar(mas2(r))}], dx')

    INS32 = {'+': ('add', 'adc'), '-': ('sub', 'sbb'), 'yb': ('and', 'and'), 'ob': ('or', 'or'),
             'xob': ('xor', 'xor')}

    def acaba(self, sents):
        """¿El bloque termina saltando a otro sitio (no sigue por debajo)?"""
        if not sents:
            return False
        u = sents[-1]
        if u.k in ('devolver', 'romper', 'seguir'):
            return True
        return u.k == 'llamada_sent' and u.e.nombre == 'salir'

    def gen_si(self, s, ultimo=False):
        # «si c entonces romper fin»: un salto y ya
        if not s.sino and len(s.entonces) == 1 and s.entonces[0].k in ('romper', 'seguir') and self.bucles:
            self.salta(s.cond, self.bucles[-1][1 if s.entonces[0].k == 'romper' else 0], True)
            return
        l_sino = self.nueva()
        self.salta(s.cond, l_sino, False)
        self.gen_bloque(s.entonces, ultimo and not s.sino)
        if s.sino:
            l_fin = self.nueva()
            if not self.acaba(s.entonces):
                if ultimo:
                    self.fin_usado = True
                self.em(f'jmp {self.fin_func if ultimo else l_fin}')
            self.et(l_sino)
            self.gen_bloque(s.sino, ultimo)
            self.et(l_fin)
        else:
            self.et(l_sino)

    def gen_mientras(self, s):
        l_ini = self.nueva()
        l_fin = self.nueva()
        self.et(l_ini)
        self.salta(s.cond, l_fin, False)
        self.bucles.append((l_ini, l_fin))
        self.gen_bloque(s.cuerpo)
        self.bucles.pop()
        self.em(f'jmp {l_ini}')
        self.et(l_fin)

    def gen_para(self, s):
        tv = self.tipo(s.var)
        lug = self.lugar_fijo(s.var)
        if not es_num(tv) or lug is None:
            self.error("la variable del «para» es una variable numérica sencilla", s.var.pos)
        self.numero(s.hasta, "el final del «para»")
        paso = 1
        if s.paso is not None:
            paso = self.const(s.paso)
            if paso is None or not es_lit(self.tipo(s.paso)):
                self.error("el «paso» es un número constante", s.paso.pos)
            if paso == 0:
                self.error("el «paso» no puede ser 0", s.paso.pos)
            self.rango(abs(paso), tv, s.paso.pos)
        if es32(tv):
            self.gen_para32(s, tv, paso)
            return
        tt = self.tam_txt(tv)
        signo = tv.k == 'ent16'
        self.gen_asig(s.var, s.desde)
        ch = self.const(s.hasta)
        if ch is not None:
            self.rango(ch, tv, s.hasta.pos)
            ch &= 0xFF if tv.tam == 1 else 0xFFFF

            def compara():
                self.em(f'cmp {tt} [{lug}], {ch}')
        else:
            tope = texto_lugar(s.tope)
            self.gen_expr(s.hasta)
            self.em(f'mov [{tope}], ax')
            reg = 'al' if tv.tam == 1 else 'ax'

            def compara():
                self.em(f'mov {reg}, [{tope}]')
                self.em(f'cmp [{lug}], {reg}')
        l_cuerpo = self.nueva()
        l_seguir = self.nueva()
        l_fin = self.nueva()
        if paso > 0:
            pasado = 'g' if signo else 'a'
        else:
            pasado = 'l' if signo else 'b'
        compara()
        self.salto(pasado, l_fin)
        self.et(l_cuerpo)
        self.bucles.append((l_seguir, l_fin))
        self.gen_bloque(s.cuerpo)
        self.bucles.pop()
        self.et(l_seguir)
        if abs(paso) == 1:
            # se acaba al llegar justo al final: así no se desborda nunca
            compara()
            self.salto('e', l_fin)
            self.em(f"{'inc' if paso > 0 else 'dec'} {tt} [{lug}]")
            self.em(f'jmp {l_cuerpo}')
        else:
            self.em(f"{'add' if paso > 0 else 'sub'} {tt} [{lug}], {abs(paso)}")
            # si al dar el paso se sale del tipo, se acabó
            self.salto(('o' if signo else 'c'), l_fin)
            compara()
            self.salto('n' + pasado, l_cuerpo)
        self.et(l_fin)

    def gen_para32(self, s, tv, paso):
        """El «para» con una variable de 32 bits: lo mismo, a dos palabras."""
        r = self._lugar(s.var, False)
        baja, alta = texto_lugar(r), texto_lugar(mas2(r))
        cc = 'l' if tv.k == 'ent32' else 'b'
        self.gen_asig(s.var, s.desde)
        ch = self.const(s.hasta)
        if ch is not None:
            self.rango(ch, tv, s.hasta.pos)
            tb, ta = mitades(ch)
        else:
            tb, ta = f'[{texto_lugar(s.tope)}]', f'[{texto_lugar(mas2(s.tope))}]'
            self.gen_expr32(s.hasta)
            self.em(f'mov {tb}, ax')
            self.em(f'mov {ta}, dx')

        def pasado():
            # deja «menor» si la variable ya se ha pasado del final
            if paso > 0:                        # final < variable
                self.em(f'mov ax, {tb}')
                self.em(f'mov dx, {ta}')
                self.em(f'cmp ax, [{baja}]')
                self.em(f'sbb dx, [{alta}]')
            else:                               # variable < final
                self.em(f'mov ax, [{baja}]')
                self.em(f'mov dx, [{alta}]')
                self.em(f'cmp ax, {tb}')
                self.em(f'sbb dx, {ta}')
        l_cuerpo = self.nueva()
        l_seguir = self.nueva()
        l_fin = self.nueva()
        pasado()
        self.salto(cc, l_fin)
        self.et(l_cuerpo)
        self.bucles.append((l_seguir, l_fin))
        self.gen_bloque(s.cuerpo)
        self.bucles.pop()
        self.et(l_seguir)
        if abs(paso) == 1:
            l_no = self.nueva()
            self.em(f'mov ax, [{baja}]')
            self.em(f'cmp ax, {tb}')
            self.salto('ne', l_no)
            self.em(f'mov ax, [{alta}]')
            self.em(f'cmp ax, {ta}')
            self.salto('e', l_fin)
            self.et(l_no)
            self.em(f"{'add' if paso > 0 else 'sub'} word [{baja}], 1")
            self.em(f"{'adc' if paso > 0 else 'sbb'} word [{alta}], 0")
            self.em(f'jmp {l_cuerpo}')
        else:
            pb, pa = mitades(abs(paso))
            self.em(f"{'add' if paso > 0 else 'sub'} word [{baja}], {pb}")
            self.em(f"{'adc' if paso > 0 else 'sbb'} word [{alta}], {pa}")
            self.salto('o' if tv.k == 'ent32' else 'c', l_fin)
            pasado()
            self.salto('n' + cc, l_cuerpo)
        self.et(l_fin)

    def gen_segun(self, s, ultimo=False):
        t = self.valor_tipo(s.e)
        if not es_num(t):
            self.error(f"«según» elige por un número, no por un {t}", s.e.pos)
        if es32(t):
            self.error("«según» elige por un número de 16 bits: convierta antes con nat16(...)", s.e.pos)
        signo = t.k == 'ent16'
        self.gen_expr(s.e)
        l_fin = self.nueva()
        vistos = []
        ets = []
        for c in s.casos:
            l_c = self.nueva()
            ets.append(l_c)
            for a, b in c.vals:
                va = self.const(a)
                vb = va if b is None else self.const(b)
                if va is None or vb is None:
                    self.error("los valores de un «caso» son constantes", (a if va is None else b).pos)
                if not (cabe16(va) and cabe16(vb)):
                    self.error("el valor de un «caso» cabe en 16 bits", a.pos)
                if vb < va:
                    self.error("el intervalo del «caso» va de menor a mayor", a.pos)
                for x, y in vistos:
                    if va <= y and x <= vb:
                        self.error("ese valor ya está en otro «caso»", a.pos)
                vistos.append((va, vb))
                if va == vb:
                    self.em(f'cmp ax, {va & 0xFFFF}')
                    self.salto('e', l_c)
                else:
                    l_no = self.nueva()
                    self.em(f'cmp ax, {va & 0xFFFF}')
                    self.salto('l' if signo else 'b', l_no)
                    self.em(f'cmp ax, {vb & 0xFFFF}')
                    self.salto('le' if signo else 'be', l_c)
                    self.et(l_no)
        if s.otro:
            self.gen_bloque(s.otro)
        self.em(f'jmp {l_fin}')
        for c, l_c in zip(s.casos, ets):
            self.et(l_c)
            self.gen_bloque(c.cuerpo)
            if not self.acaba(c.cuerpo):
                self.em(f'jmp {l_fin}')
        self.et(l_fin)

    # ---- saltos según una condición ----
    def salto(self, cc, destino):
        self.em(f'j{cc} {destino}')

    INVERSA = {'e': 'ne', 'ne': 'e', 'l': 'ge', 'ge': 'l', 'g': 'le', 'le': 'g',
               'b': 'ae', 'ae': 'b', 'a': 'be', 'be': 'a', 'z': 'nz', 'nz': 'z',
               'c': 'nc', 'nc': 'c'}

    def salta(self, e, destino, si):
        """Salta a destino si la condición e vale «si»; si no, sigue."""
        t = self.valor_tipo(e)
        if t.k != 'bool':
            self.error(f"aquí va una condición (bool), no un {t}", e.pos)
        c = self.const(e)
        if c is not None:
            if bool(c) == si:
                self.em(f'jmp {destino}')
            return
        if e.k == 'un' and e.op == 'no':
            self.salta(e.x, destino, not si)
            return
        if e.k == 'bin' and e.op in ('y', 'o'):
            if (e.op == 'y') != si:             # «y» falso / «o» cierto: basta uno
                self.salta(e.a, destino, si)
                self.salta(e.b, destino, si)
            else:
                l = self.nueva()
                self.salta(e.a, l, not si)
                self.salta(e.b, destino, si)
                self.et(l)
            return
        if e.k == 'bin':
            cc = self.compara(e)
            self.salto(cc if si else self.INVERSA[cc], destino)
            return
        if e.k == 'llamada' and e.nombre == 'acarreo':
            self.usa_reg_int('ban')
            if self.banderas_vivas():
                self.salto('c' if si else 'nc', destino)
                return
            self.ban_memoria = True
            self.em('test byte [R_ban], 1')
            self.salto('nz' if si else 'z', destino)
            return
        o = self.opnd(e)
        if o and o[1] == 1:
            self.em(f'cmp byte {o[0]}, 0')
        else:
            self.gen_expr(e)
            self.em('or al, al')
        self.salto('nz' if si else 'z', destino)

    def banderas_vivas(self):
        """¿Siguen en el procesador las banderas de la última interrupción? Sí, si
        desde que se guardaron solo ha habido «mov» (que no las tocan) y ninguna
        etiqueta: entonces acarreo() es un «jc» y no hace falta mirar la copia."""
        for ln in reversed(self.lineas):
            if ln == '    pop word [R_ban]':
                return True
            if not ln.startswith('    mov '):
                return False
        return False

    def compara(self, e):
        """Deja las banderas de comparar e.a con e.b; da la condición del salto."""
        a, b, op = e.a, e.b, e.op
        ta, tb = self.tipo(a), self.tipo(b)
        signo = self.con_signo(ta, tb)
        if es_num(ta) and es_num(tb) and (es32(ta) or es32(tb)):
            return self.compara32(e, signo)
        if self.inmediato(a) is not None and self.inmediato(b) is None:
            a, b = b, a
            ta, tb = tb, ta
            op = {'<': '>', '>': '<', '<=': '>=', '>=': '<='}.get(op, op)
        cc = {'=': 'e', '<>': 'ne',
              '<': 'l' if signo else 'b', '<=': 'le' if signo else 'be',
              '>': 'g' if signo else 'a', '>=': 'ge' if signo else 'ae'}[op]
        ib = self.inmediato(b)
        ob = self.opnd(b)
        oa = self.opnd(a)
        # (x yb n) = 0: un «test» sobre la propia memoria
        if self.const(b) == 0 and op in ('=', '<>') and a.k == 'bin' and a.op == 'yb' \
                and self.const(a) is None and self.const(a.b) is not None \
                and self.es_lugar(a.a) and es_num(self.tipo(a.a)):
            self.tipo(a)
            n = self.const(a.b) & 0xFFFF
            tx = self.tipo(a.a)
            if tx.tam == 2 or n <= 255:
                self.em(f'test {self.tam_txt(tx)} [{self.lugar(a.a)}], {n}')
                return cc
        es8a = ta.tam == 1 and ta.k in ('nat8', 'bool')
        if ib is not None:
            cb = self.const(b)
            if self.es_lugar(a) and ta.k not in ('arr', 'est'):
                if ta.tam == 2 or (cb is not None and 0 <= cb <= 255):
                    self.em(f'cmp {self.tam_txt(ta)} [{self.lugar(a)}], {ib}')
                    return cc
            self.gen_expr(a)
            if cb == 0 and op in ('=', '<>'):
                self.em('or ax, ax')
            else:
                self.em(f'cmp ax, {ib}')
            return cc
        if ob and ob[1] == 2:
            self.gen_expr(a)
            self.em(f'cmp ax, {ob[0]}')
            return cc
        if ob and ob[1] == 1 and es8a:
            self.gen_al(a)
            self.em(f'cmp al, {ob[0]}')
            return cc
        if oa and oa[1] == 2:
            self.gen_expr(b)
            self.em(f'cmp {oa[0]}, ax')
            return cc
        self.gen_expr(a)
        self.em('push ax')
        self.gen_expr(b)
        self.em('pop cx')
        self.em('cmp cx, ax')
        return cc

    def compara32(self, e, signo):
        """Comparar dos números de 32 bits (los de 16 se ensanchan antes)."""
        a, b, op = e.a, e.b, e.op
        if op in ('=', '<>'):
            cc = 'e' if op == '=' else 'ne'
            if self.const(a) is not None and self.const(b) is None:
                a, b = b, a
            if self.const(b) == 0:
                self.gen_expr32(a)
                self.em('or ax, dx')
                return cc
            l = self.nueva()
            o = self.opnd32(b)
            if o:
                self.gen_expr32(a)
                self.em(f'cmp ax, {o[0]}')
                self.salto('ne', l)
                self.em(f'cmp dx, {o[1]}')
            else:
                self.dos32(a, b)
                self.em('cmp ax, bx')
                self.salto('ne', l)
                self.em('cmp dx, cx')
            self.et(l)
            return cc
        # a < b es «a - b pide prestado»; a > b es b < a; y los otros dos, lo contrario
        cc = ('l' if signo else 'b') if op in ('<', '>') else ('ge' if signo else 'ae')
        if op in ('<', '>='):                   # las banderas de a - b
            o = self.opnd32(b)
            if o:
                self.gen_expr32(a)
                self.em(f'cmp ax, {o[0]}')
                self.em(f'sbb dx, {o[1]}')
            else:
                self.dos32(a, b)
                self.em('cmp ax, bx')
                self.em('sbb dx, cx')
        else:                                   # las de b - a
            self.dos32(a, b)
            self.em('cmp bx, ax')
            self.em('sbb cx, dx')
        return cc

    # ---- expresiones: el valor, a AX ----
    def gen_al(self, e):
        """Como gen_expr, pero solo se promete AL (para guardar en un byte)."""
        t = self.valor_tipo(e)
        if self.es_lugar(e) and t.tam == 1 and self.inmediato(e) is None:
            self.em(f'mov al, [{self.lugar(e)}]')
        elif e.k == 'llamada' and e.nombre in ('leer_byte', 'meter_puerto'):
            self.gen_llamada(e, solo_al=True)
        else:
            self.gen_expr(e)

    def gen_expr(self, e):
        t = self.valor_tipo(e)
        im = self.inmediato(e)
        if im is not None:
            self.em('xor ax, ax' if im == '0' else f'mov ax, {im}')
            return
        k = e.k
        if self.es_lugar(e):
            if t.k == 'arr':
                self.em(f'lea ax, [{self.lugar(e)}]')
            elif t.tam == 1:
                self.em(f'mov al, [{self.lugar(e)}]')
                self.em('xor ah, ah')
            else:
                self.em(f'mov ax, [{self.lugar(e)}]')
            return
        if es32(t):                             # de uno de 32 bits, aquí vale la palabra baja
            self.gen_expr32(e)
            return
        if k == 'un':
            if e.op == 'no':
                self.gen_expr(e.x)
                self.em('xor al, 1')
            else:
                self.gen_expr(e.x)
                self.em('neg ax' if e.op == '-' else 'not ax')
            return
        if k == 'llamada':
            self.gen_llamada(e)
            return
        if k == 'bin':
            if t.k == 'bool':
                l_no = self.nueva()
                l_fin = self.nueva()
                self.salta(e, l_no, False)
                self.em('mov ax, 1')
                self.em(f'jmp {l_fin}')
                self.et(l_no)
                self.em('xor ax, ax')
                self.et(l_fin)
            else:
                self.gen_arit(e)
            return
        raise AssertionError(k)

    def carga32(self, c):
        baja, alta = mitades(c)
        self.em('xor ax, ax' if baja == '0' else f'mov ax, {baja}')
        self.em('xor dx, dx' if alta == '0' else f'mov dx, {alta}')

    def gen_expr32(self, e):
        """El valor de e como número de 32 bits, a DX:AX (DX, la palabra alta). Los
        de 16 y 8 bits se ensanchan: con el signo los ent16, con ceros los demás."""
        t = self.valor_tipo(e)
        c = self.const(e)
        if c is not None:
            self.carga32(c)
            return
        if not es32(t):
            self.gen_expr(e)
            self.em('cwd' if t.k == 'ent16' else 'xor dx, dx')
            return
        k = e.k
        if self.es_lugar(e):
            r = self._lugar(e, True)
            self.em(f'mov ax, [{texto_lugar(r)}]')
            self.em(f'mov dx, [{texto_lugar(mas2(r))}]')
        elif k == 'un':
            self.gen_expr32(e.x)
            if e.op == '-':
                self.em('neg dx')
                self.em('neg ax')
                self.em('sbb dx, 0')
            else:
                self.em('not ax')
                self.em('not dx')
        elif k == 'llamada' and e.nombre in ('nat32', 'ent32'):
            self.gen_expr32(e.args[0][1])
        elif k == 'llamada':
            self.gen_llamada(e)
        elif k == 'bin':
            self.gen_arit32(e)
        else:
            raise AssertionError(k)

    def dos32(self, a, b):
        """a -> DX:AX y b -> CX:BX."""
        o = self.opnd32(b)
        self.gen_expr32(a)
        if o:
            self.em('xor bx, bx' if o[0] == '0' else f'mov bx, {o[0]}')
            self.em('xor cx, cx' if o[1] == '0' else f'mov cx, {o[1]}')
        else:
            self.em('push dx')
            self.em('push ax')
            self.gen_expr32(b)
            self.em('mov bx, ax')
            self.em('mov cx, dx')
            self.em('pop ax')
            self.em('pop dx')

    def gen_arit32(self, e):
        """Una cuenta de 32 bits: el resultado, a DX:AX. Sumar, restar y los bits
        van a dos palabras con el acarreo; multiplicar, dividir y desplazar con una
        cuenta que no se sabe al compilar, con las rutinas de apoyo castel_*."""
        op, a, b = e.op, e.a, e.b
        ta, tb = self.tipo(a), self.tipo(b)
        cb = self.const(b)
        if op in ('<<', '>>'):
            rut = 'shl32' if op == '<<' else ('sar32' if ta.k == 'ent32' else 'shr32')
            if cb is not None:
                if not 0 <= cb <= 31:
                    self.error("se desplaza de 0 a 31 bits", b.pos)
                self.gen_expr32(a)
                self.desplaza32(rut, cb)
                return
            o = self.opnd(b)
            self.gen_expr32(a)
            if o and o[1] in (0, 2):
                self.em(f'mov cx, {o[0]}')
            else:
                self.em('push dx')
                self.em('push ax')
                self.gen_expr(b)
                self.em('mov cx, ax')
                self.em('pop ax')
                self.em('pop dx')
            self.em(f'call castel_{rut}')
            return
        if op in self.INS32:
            i1, i2 = self.INS32[op]
            o = self.opnd32(b)
            if o:
                self.gen_expr32(a)
                self.em(f'{i1} ax, {o[0]}')
                self.em(f'{i2} dx, {o[1]}')
            else:
                self.dos32(a, b)
                self.em(f'{i1} ax, bx')
                self.em(f'{i2} dx, cx')
            return
        if op == '*':
            self.dos32(a, b)
            self.em('call castel_mul32')
            return
        if op in ('div', 'mod'):
            if cb == 0:
                self.error("división por cero", b.pos)
            self.dos32(a, b)
            self.em(f"call castel_{'div32s' if self.con_signo(ta, tb) else 'div32u'}")
            if op == 'mod':
                self.em('mov ax, bx')
                self.em('mov dx, cx')
            return
        raise AssertionError(op)

    def desplaza32(self, rut, n):
        """DX:AX desplazado n bits (n, constante de 0 a 31)."""
        if n >= 16:                             # una palabra entera, y el resto en la otra
            if rut == 'shl32':
                self.em('mov dx, ax')
                self.em('xor ax, ax')
                reg, ins = 'dx', 'shl'
            else:
                self.em('mov ax, dx')
                self.em('cwd' if rut == 'sar32' else 'xor dx, dx')
                reg, ins = 'ax', 'sar' if rut == 'sar32' else 'shr'
            n -= 16
            if n == 1:
                self.em(f'{ins} {reg}, 1')
            elif n:
                self.em(f'mov cl, {n}')
                self.em(f'{ins} {reg}, cl')
            return
        par = {'shl32': ('shl ax, 1', 'rcl dx, 1'), 'shr32': ('shr dx, 1', 'rcr ax, 1'),
               'sar32': ('sar dx, 1', 'rcr ax, 1')}[rut]
        if n <= 2:
            for _ in range(n):
                self.em(par[0])
                self.em(par[1])
        else:
            l = self.nueva()
            self.em(f'mov cx, {n}')
            self.et(l)
            self.em(par[0])
            self.em(par[1])
            self.em(f'loop {l}')

    def a_cx(self, a, b):
        """a -> AX y b -> CX."""
        o = self.opnd(b)
        if o and o[1] in (0, 2):
            self.gen_expr(a)
            self.em(f'mov cx, {o[0]}')
        elif o:
            self.gen_expr(a)
            self.em(f'mov cl, {o[0]}')
            self.em('xor ch, ch')
        else:
            self.gen_expr(a)
            self.em('push ax')
            self.gen_expr(b)
            self.em('pop cx')
            self.em('xchg ax, cx')

    def gen_arit(self, e):
        op, a, b = e.op, e.a, e.b
        ta, tb = self.tipo(a), self.tipo(b)
        cb = self.const(b)
        signo = self.con_signo(ta, tb)
        # punteros: se avanza de elemento en elemento
        if ta.k == 'punt' or tb.k == 'punt':
            if ta.k == 'punt' and tb.k in ('punt', 'arr'):
                self.a_cx(a, b)
                self.em('sub ax, cx')
                return
            if tb.k == 'punt':
                a, b, ta, tb = b, a, tb, ta
                cb = self.const(b)
            tam = ta.base.tam
            if cb is not None:
                self.gen_expr(a)
                self.suma_const(op, cb * tam)
            else:
                self.gen_expr(b)
                self.escala('ax', tam)
                self.em('push ax')
                self.gen_expr(a)
                self.em('pop cx')
                self.em(f"{'add' if op == '+' else 'sub'} ax, cx")
            return
        if op in ('+', '-', 'yb', 'ob', 'xob'):
            ins = {'+': 'add', '-': 'sub', 'yb': 'and', 'ob': 'or', 'xob': 'xor'}[op]
            o = self.opnd(b)
            if cb is not None and op in ('+', '-'):
                self.gen_expr(a)
                self.suma_const(op, cb)
            elif o and o[1] in (0, 2):
                self.gen_expr(a)
                self.em(f'{ins} ax, {o[0]}')
            elif o:
                self.a_cx(a, b)
                self.em(f'{ins} ax, cx')
            else:
                self.gen_expr(a)
                self.em('push ax')
                self.gen_expr(b)
                self.em('pop cx')
                if op == '-':
                    self.em('xchg ax, cx')
                self.em(f'{ins} ax, cx')
            return
        if op == '*':
            if cb is not None and potencia2(cb):
                self.gen_expr(a)
                self.desplaza('shl', cb.bit_length() - 1)
                return
            o = self.opnd(b)
            if o and o[1] == 2:
                self.gen_expr(a)
                self.em(f'mul word {o[0]}')
            else:
                self.a_cx(a, b)
                self.em('mul cx')
            return
        if op in ('div', 'mod'):
            if cb == 0:
                self.error("división por cero", b.pos)
            if cb is not None and potencia2(cb) and not signo:
                self.gen_expr(a)
                if op == 'div':
                    self.desplaza('shr', cb.bit_length() - 1)
                else:
                    self.em(f'and ax, {cb - 1}')
                return
            self.a_cx(a, b)
            if signo:
                self.em('cwd')
                self.em('idiv cx')
            else:
                self.em('xor dx, dx')
                self.em('div cx')
            if op == 'mod':
                self.em('mov ax, dx')
            return
        if op in ('<<', '>>'):
            ins = 'shl' if op == '<<' else ('sar' if ta.k == 'ent16' else 'shr')
            if cb is not None:
                if not 0 <= cb <= 15:
                    self.error("se desplaza de 0 a 15 bits", b.pos)
                self.gen_expr(a)
                self.desplaza(ins, cb)
            else:
                self.a_cx(a, b)
                self.em(f'{ins} ax, cl')
            return
        raise AssertionError(op)

    def suma_const(self, op, n):
        n &= 0xFFFF
        if op == '-':
            n = -n & 0xFFFF
        if n == 0:
            return
        if n <= 2:
            for _ in range(n):
                self.em('inc ax')
        elif n >= 0xFFFE:
            for _ in range(0x10000 - n):
                self.em('dec ax')
        else:
            self.em(f'add ax, {n}')

    def desplaza(self, ins, n):
        if n <= 2:
            for _ in range(n):
                self.em(f'{ins} ax, 1')
        else:
            self.em(f'mov cl, {n}')
            self.em(f'{ins} ax, cl')

    # ---- llamadas ----
    def usa_apoyo(self, nombre):
        pass                                    # van al programa las que se nombran (ver genera)

    def usa_reg_int(self, r):
        pass

    def gen_llamada(self, e, sentencia=False, solo_al=False):
        nm = e.nombre
        args = [a for _, a in e.args]
        if nm in self.funcs:
            f = self.funcs[nm]
            if f is self.principal:
                self.error("principal() no se llama: es por donde se empieza", e.pos)
            for a, p in reversed(list(zip(args, f.params))):
                if p.ref:
                    r = self._lugar(a, False)
                    if r and r[0] == '':
                        self.em(f'mov ax, {texto_lugar(r)}')
                        self.em('push ax')
                    elif a.k == 'ref' and self.var(a.nombre, a.pos).ref:
                        self.em(f'push word [bp+{self.var(a.nombre, a.pos).off}]')
                    else:
                        self.em(f'lea ax, [{self.lugar(a)}]')
                        self.em('push ax')
                elif es32(self.tipo_param(f, p)):  # dos palabras: la alta, primero
                    o = self.opnd32(a)
                    if o and self.const(a) is None and o[1] != '0':
                        self.em(f'push word {o[1]}')
                        self.em(f'push word {o[0]}')
                    else:
                        self.gen_expr32(a)
                        self.em('push dx')
                        self.em('push ax')
                else:
                    o = self.opnd(a)
                    if o and o[1] == 2:
                        self.em(f'push word {o[0]}')
                    elif self.tipo_param(f, p).tam == 1:
                        self.gen_al(a)
                        self.em('push ax')
                    else:
                        self.gen_expr(a)
                        self.em('push ax')
            self.em(f'call {f.etq}')
            return
        if nm == 'escribir':
            for a in args:
                t = self.tipo(a)
                if self.es_texto(t):
                    o = self.opnd(a)
                    if o and o[1] in (0, 2):
                        self.em(f'mov si, {o[0]}')
                    else:
                        self.gen_expr(a)
                        self.em('mov si, ax')
                    self.usa_apoyo('escribir_cad')
                    self.em('call escribir_cad')
                elif t.k == 'bool':
                    l_no = self.nueva()
                    l_fin = self.nueva()
                    self.salta(a, l_no, False)
                    self.em(f"mov si, {self.etiqueta_cadena(b'cierto')}")
                    self.em(f'jmp {l_fin}')
                    self.et(l_no)
                    self.em(f"mov si, {self.etiqueta_cadena(b'falso')}")
                    self.et(l_fin)
                    self.usa_apoyo('escribir_cad')
                    self.em('call escribir_cad')
                elif es32(t):
                    self.gen_expr32(a)
                    c = self.const(a)
                    con_signo = t.k == 'ent32' or (c is not None and c < 0)
                    self.em(f"call {'escribir_num32' if con_signo else 'escribir_nat32'}")
                else:
                    self.gen_expr(a)
                    c = self.const(a)
                    con_signo = t.k == 'ent16' or (t.k == 'lit' and c is not None and c < 0)
                    rut = 'escribir_num' if con_signo else 'escribir_nat'
                    self.usa_apoyo(rut)
                    self.em(f'call {rut}')
            return
        if nm == 'escribir_car':
            self.carga8('dl', args[0])
            self.em('mov ah, 2')
            self.em('int 0x21')
            return
        if nm == 'salir':
            c = self.const(args[0]) if args else 0
            if c is not None:
                self.em(f'mov ax, {0x4C00 + (c & 0xFF)}')
            else:
                self.gen_al(args[0])
                self.em('mov ah, 0x4C')
            self.em('int 0x21')
            return
        if nm == 'meter_puerto':
            o = self.opnd(args[0])
            if o and o[1] in (0, 2):
                self.em(f'mov dx, {o[0]}')
            else:
                self.gen_expr(args[0])
                self.em('mov dx, ax')
            self.em('in al, dx')
            if not solo_al:
                self.em('xor ah, ah')
            return
        if nm == 'sacar_puerto':
            self.a_cx(args[1], args[0])         # dato -> AX, puerto -> CX
            self.em('mov dx, cx')
            self.em('out dx, al')
            return
        if nm in ('leer_byte', 'leer_palabra'):
            if self.simple16(args[1]):
                self.gen_expr(args[0])
                self.em('mov es, ax')
                donde = self.desp_es(args[1])
            else:
                self.a_cx(args[1], args[0])     # desplazamiento -> AX, segmento -> CX
                self.em('mov bx, ax')
                self.em('mov es, cx')
                donde = 'es:bx'
            if nm == 'leer_byte':
                self.em(f'mov al, [{donde}]')
                if not solo_al:
                    self.em('xor ah, ah')
            else:
                self.em(f'mov ax, [{donde}]')
            return
        if nm in ('poner_byte', 'poner_palabra'):
            tt = 'byte' if nm == 'poner_byte' else 'word'
            if self.simple16(args[1]) and self.opnd(args[2]):
                self.gen_expr(args[0])
                self.em('mov es, ax')
                donde = self.desp_es(args[1])
                o = self.opnd(args[2])
                if o[1] == 0:
                    self.em(f'mov {tt} [{donde}], {o[0]}')
                else:
                    self.gen_expr(args[2])
                    self.em(f"mov [{donde}], {'al' if tt == 'byte' else 'ax'}")
                return
            self.gen_expr(args[0])
            self.em('push ax')
            self.a_cx(args[2], args[1])         # dato -> AX, desplazamiento -> CX
            self.em('mov bx, cx')
            self.em('pop es')
            self.em('mov [es:bx], al' if nm == 'poner_byte' else 'mov [es:bx], ax')
            return
        if nm in ('dirección',):
            self.em(f'lea ax, [{self.lugar(args[0])}]')
            return
        if nm == 'puntero':
            self.gen_expr(args[0])
            return
        if nm in ('nat8', 'byte'):
            self.gen_expr(args[0])
            if self.tipo(args[0]).tam != 1:
                self.em('xor ah, ah')
            return
        if nm in ('nat16', 'ent16'):
            self.gen_expr(args[0])
            return
        if nm in ('nat32', 'ent32'):
            self.gen_expr32(args[0])
            return
        if nm == 'fin_programa':
            self.em('mov ax, _fin_programa')
            return
        if nm == 'interrupción':
            self.gen_int(e)
            return
        if nm in SALIDAS_INT:
            r = SALIDAS_INT[nm]
            self.usa_reg_int(r)
            if r == 'ban':
                self.ban_memoria = True
            self.em(f'mov ax, [R_{r}]')
            if nm == 'acarreo':
                self.em('and ax, 1')
            return
        raise AssertionError(nm)

    def desp_es(self, a):
        """Un desplazamiento sencillo dentro de ES: «es:N» o, cargándolo, «es:bx»."""
        o = self.opnd(a)
        if o[1] == 0:
            return f'es:{o[0]}'
        self.em(f'mov bx, {o[0]}')
        return 'es:bx'

    def tipo_param(self, f, p):
        return f.ptipos[f.params.index(p)]

    def carga8(self, reg, a):
        """El valor de a (su octeto bajo), al registro de 8 bits reg."""
        o = self.opnd(a)
        if o and o[1] == 0:
            c = self.const(a)
            self.em(f'mov {reg}, {c & 0xFF if c is not None else o[0]}')
        elif o:
            self.em(f'mov {reg}, {o[0]}')
        else:
            self.gen_al(a)
            if reg != 'al':
                self.em(f'mov {reg}, al')

    # interrupción(n, reg: valor, ...): carga los registros que se digan, llama
    # a la interrupción y devuelve AX. Lo que la función use después con
    # acarreo(), banderas(), reg_bx()... se guarda aquí en R_xx.
    def prepara_int(self, e):
        if not e.args or e.args[0][0]:
            self.error("interrupción(n, registro: valor, ...): falta el número", e.pos)
        n = self.const(e.args[0][1])
        if n is None or not 0 <= n <= 255:
            self.error("el número de la interrupción es una constante de 0 a 255", e.args[0][1].pos)
        vistos = {}
        for nombre, a in e.args[1:]:
            if not nombre:
                self.error("los registros se dan por su nombre: interrupción(0x21, ah: 2, dl: 'A')", a.pos)
            r, pos = nombre
            if r not in REG16 + REG8 + REGSEG:
                self.error(f"«{r}» no es un registro (ax bx cx dx si di, sus mitades, es, ds)", pos)
            entero = r[0] + 'x' if r in REG8 else r
            for otro in vistos:
                if otro == r or (entero == (otro[0] + 'x' if otro in REG8 else otro) and (r in REG16 or otro in REG16)):
                    self.error(f"el registro «{r}» ya está dado", pos)
            vistos[r] = a
            t = self.valor_tipo(a)
            if t.k == 'bool' or t.k == 'est':
                self.error(f"a un registro se le da un número o una dirección, no un {t}", a.pos)
            c = self.const(a)
            if c is not None and r in REG8 and not -128 <= c <= 255:
                self.error(f"{c} no cabe en «{r}» (8 bits)", a.pos)
        e.n_int = n
        e.regs = vistos

    def gen_int(self, e):
        n, regs = e.n_int, e.regs
        if 'ds' in regs:
            self.em('push ds')
            self.gen_expr(regs['ds'])
            self.em('push ax')
        directos = []
        apilados = []
        for r, a in regs.items():
            if r == 'ds':
                continue
            o = self.opnd(a)
            if o and (r in REG8 or o[1] in (0, 2)) and not (r == 'es' and o[1] == 0):
                directos.append((r, a, o))
            elif o and o[1] == 1 and r in ('ax', 'bx', 'cx', 'dx'):
                directos.append((r, a, o))
            else:
                apilados.append((r, a))
        mitades = [r[0] for r, _ in apilados if r in REG8]
        if len(mitades) != len(set(mitades)):
            self.error("las dos mitades de un registro con cuentas a la vez: calcule una antes en una variable", e.pos)
        for i, (r, a) in enumerate(apilados):
            if i < len(apilados) - 1:
                self.gen_expr(a)
                self.em('push ax')
            elif r in REG8:                     # el último se queda en AX: de ahí a su sitio
                self.gen_al(a)
                if r != 'al':
                    self.em(f'mov {r}, al')
            else:
                self.gen_expr(a)
                if r != 'ax':
                    self.em(f'mov {r}, ax')
        for r, a in reversed(apilados[:-1]):
            if r in REG8:
                self.em(f'pop {r[0]}x')
                if r[1] == 'h':
                    self.em(f'mov {r}, {r[0]}l')
            else:
                self.em(f'pop {r}')
        for r, a, o in directos:
            if r in REG8:
                self.carga8(r, a)
            elif o[1] == 1:
                self.em(f'mov {r[0]}l, {o[0]}')
                self.em(f'xor {r[0]}h, {r[0]}h')
            elif o[0] == '0' and r != 'es':
                self.em(f'xor {r}, {r}')
            else:
                self.em(f'mov {r}, {o[0]}')
        if 'ds' in regs:
            self.em('pop ds')
        protege_bp = n == 0x10                  # hay BIOS que estropean BP en la int 10h
        if protege_bp:
            self.em('push bp')
        self.em(f'int 0x{n:02X}')
        if protege_bp:
            self.em('pop bp')
        if 'ds' in regs:
            self.em('pop ds')
        guardar = {SALIDAS_INT[x] for x in self.func.usa_int}
        if 'ban' in guardar:
            self.usa_reg_int('ban')
            self.em('pushf')
            self.em('pop word [R_ban]')
        for r in ('bx', 'cx', 'dx', 'si', 'di', 'es'):
            if r in guardar:
                self.usa_reg_int(r)
                self.em(f'mov [R_{r}], {r}')


HACEN_ALGO = {'escribir', 'escribir_car', 'salir', 'sacar_puerto', 'poner_byte', 'poner_palabra',
              'interrupción'}
INTERNAS = {'escribir', 'escribir_car', 'meter_puerto', 'sacar_puerto', 'leer_byte', 'poner_byte',
            'leer_palabra', 'poner_palabra', 'dirección', 'valor', 'puntero', 'tamaño',
            'interrupción', 'salir', 'fin_programa'} | set(SALIDAS_INT)


def limpia(lineas):
    """Quita los «jmp X» que caen justo delante de «X:» y el código tras un
    salto que nadie puede alcanzar (hasta la etiqueta siguiente)."""
    sal = []
    for ln in lineas:
        sal.append(ln)
    cambio = True
    while cambio:
        cambio = False
        nuevo = []
        i = 0
        while i < len(sal):
            ln = sal[i]
            m = re.match(r'^    jmp (\w+)$', ln)
            if m:
                j = i + 1
                while j < len(sal) and re.match(r'^L_\d+:$', sal[j]):
                    if sal[j] == m.group(1) + ':':
                        break
                    j += 1
                if j < len(sal) and sal[j] == m.group(1) + ':':
                    cambio = True
                    i += 1
                    continue
                # tras un jmp, lo que haya hasta la etiqueta siguiente no se alcanza
                nuevo.append(ln)
                i += 1
                while i < len(sal) and re.match(r'^    (jmp|j\w+) L_\d+$', sal[i]):
                    cambio = True
                    i += 1
                continue
            # «jCC A / jmp B / A:»  ->  «jNCC B / A:»
            m = re.match(r'^    j(\w+) (L_\d+)$', ln)
            if m and m.group(1) in Generador.INVERSA and i + 2 < len(sal) \
                    and sal[i + 2] == m.group(2) + ':':
                m2 = re.match(r'^    jmp (\w+)$', sal[i + 1])
                if m2:
                    nuevo.append(f'    j{Generador.INVERSA[m.group(1)]} {m2.group(1)}')
                    i += 2
                    cambio = True
                    continue
            # «mov [X], ax / mov ax, [X]»: lo segundo sobra
            m = re.match(r'^    mov (\[[^\]]+\]), (ax|al)$', ln)
            if m and i + 1 < len(sal) and sal[i + 1] == f'    mov {m.group(2)}, {m.group(1)}':
                nuevo.append(ln)
                i += 2
                cambio = True
                continue
            nuevo.append(ln)
            i += 1
        sal = nuevo
    return sal


# Rutinas de apoyo en ensamblador: solo se añaden las que se usan.
APOYO = {
    'escribir_num': r"""
; escribir_num: enseña AX como entero con signo, en decimal
escribir_num:
    or ax, ax
    jns escribir_nat
    push ax
    mov dl, '-'
    mov ah, 2
    int 0x21
    pop ax
    neg ax
""",
    'escribir_nat': r"""
; escribir_nat: enseña AX como natural, en decimal
escribir_nat:
    xor cx, cx
    mov bx, 10
.cifra:
    xor dx, dx
    div bx
    push dx
    inc cx
    or ax, ax
    jnz .cifra
.sale:
    pop dx
    add dl, '0'
    mov ah, 2
    int 0x21
    loop .sale
    ret
""",
    'escribir_num32': r"""
; escribir_num32: enseña DX:AX como entero con signo de 32 bits, en decimal
escribir_num32:
    or dx, dx
    jns escribir_nat32
    push ax
    push dx
    mov dl, '-'
    mov ah, 2
    int 0x21
    pop dx
    pop ax
    neg dx
    neg ax
    sbb dx, 0
""",
    'escribir_nat32': r"""
; escribir_nat32: enseña DX:AX como natural de 32 bits, en decimal
escribir_nat32:
    xor cx, cx
    mov bx, 10
.cifra:
    mov si, ax
    mov ax, dx
    xor dx, dx
    div bx
    xchg ax, si
    div bx
    push dx
    inc cx
    mov dx, si
    mov si, ax
    or si, dx
    jnz .cifra
.sale:
    pop dx
    add dl, '0'
    mov ah, 2
    int 0x21
    loop .sale
    ret
""",
    'escribir_cad': r"""
; escribir_cad: enseña el texto acabado en 0 que hay en SI
escribir_cad:
    lodsb
    or al, al
    jz .fin
    mov dl, al
    mov ah, 2
    int 0x21
    jmp escribir_cad
.fin:
    ret
""",
}


APOYO.update({
    'castel_mul32': r"""
; castel_mul32: DX:AX = DX:AX * CX:BX (los 32 bits bajos: vale con signo y sin él)
castel_mul32:
    mov si, ax
    mov di, dx
    mul bx
    push ax
    push dx
    mov ax, si
    mul cx
    mov cx, ax
    mov ax, di
    mul bx
    add ax, cx
    pop dx
    add dx, ax
    pop ax
    ret
""",
    'castel_div32s': r"""
; castel_div32s: división con signo de DX:AX entre CX:BX. Cociente en DX:AX
; (hacia cero) y resto en CX:BX (con el signo del dividendo)
castel_div32s:
    mov si, dx
    xor si, cx
    push si
    push dx
    or dx, dx
    jns .a
    neg dx
    neg ax
    sbb dx, 0
.a:
    or cx, cx
    jns .b
    neg cx
    neg bx
    sbb cx, 0
.b:
    call castel_div32u
    pop si
    or si, si
    jns .c
    neg cx
    neg bx
    sbb cx, 0
.c:
    pop si
    or si, si
    jns .d
    neg dx
    neg ax
    sbb dx, 0
.d:
    ret
""",
    'castel_div32u': r"""
; castel_div32u: división sin signo de DX:AX entre CX:BX. Cociente en DX:AX y
; resto en CX:BX. (Entre cero salta la interrupción 0, como con «div».)
castel_div32u:
    or cx, cx
    jnz .largo
    mov cx, ax
    mov ax, dx
    xor dx, dx
    div bx
    xchg ax, cx
    div bx
    mov bx, dx
    mov dx, cx
    xor cx, cx
    ret
.largo:
    push bp
    mov bp, 32
    xor si, si
    xor di, di
.bit:
    shl ax, 1
    rcl dx, 1
    rcl si, 1
    rcl di, 1
    jc .resta
    cmp di, cx
    jb .no
    ja .resta
    cmp si, bx
    jb .no
.resta:
    sub si, bx
    sbb di, cx
    inc ax
.no:
    dec bp
    jnz .bit
    mov bx, si
    mov cx, di
    pop bp
    ret
""",
    'castel_shl32': r"""
; castel_shl32: DX:AX desplazado CX bits a la izquierda
castel_shl32:
    cmp cx, 32
    jbe .cabe
    mov cx, 32
.cabe:
    jcxz .fin
.otro:
    shl ax, 1
    rcl dx, 1
    loop .otro
.fin:
    ret
""",
    'castel_shr32': r"""
; castel_shr32: DX:AX desplazado CX bits a la derecha, metiendo ceros
castel_shr32:
    cmp cx, 32
    jbe .cabe
    mov cx, 32
.cabe:
    jcxz .fin
.otro:
    shr dx, 1
    rcr ax, 1
    loop .otro
.fin:
    ret
""",
    'castel_sar32': r"""
; castel_sar32: DX:AX desplazado CX bits a la derecha, conservando el signo
castel_sar32:
    cmp cx, 32
    jbe .cabe
    mov cx, 32
.cabe:
    jcxz .fin
.otro:
    sar dx, 1
    rcr ax, 1
    loop .otro
.fin:
    ret
""",
})


# =============================================================================
#  5. Los archivos: el programa y los módulos que importa
# =============================================================================
class Cargador:
    """Lee el archivo que se compila y, detrás, los módulos que importa (y los que
    importan estos). «importar Textos» busca textos.cas (o Textos.cas) en la carpeta
    del que lo importa, en las de -I y en biblioteca/, al lado del compilador."""
    def __init__(self, rutas):
        self.rutas = rutas
        self.mods = {}              # nombre -> Modulo
        self.orden = []             # cada módulo, detrás de los que importa
        self.en_curso = []          # los que se están leyendo (para ver los círculos)
        self.textos = {}            # ruta -> texto (para señalar los errores)

    def error(self, msg, pos, m, nota=None):
        e = ErrorCastel(msg, pos)
        e.archivo = m.ruta
        e.nota = nota
        raise e

    def busca(self, nombre, m, pos):
        """El archivo del módulo que se importa: (ruta, árbol). Un archivo que se
        llame así pero sea un programa (con principal()) no vale: se sigue buscando."""
        carpetas = [os.path.dirname(m.ruta) or '.'] + self.rutas + \
                   [os.path.join(os.path.dirname(os.path.abspath(__file__)), 'biblioteca')]
        programas = []
        for c in carpetas:
            for n in dict.fromkeys((nombre + '.cas', nombre.lower() + '.cas')):
                r = os.path.join(c, n)
                if not os.path.isfile(r):
                    continue
                arbol = self.lee(r)
                if any(d.k == 'func' and d.nombre == 'principal' for d in arbol.decls):
                    programas.append(r)
                    continue
                if arbol.nombre != nombre:
                    self.error(f"el archivo {n} es el módulo «{arbol.nombre}», no «{nombre}»", pos, m)
                return r, arbol
        nota = "se busca en: " + ', '.join(carpetas)
        if programas:
            nota += "; " + ', '.join(programas) + " es un programa (tiene principal()), no un módulo"
        self.error(f"no encuentro el módulo «{nombre}»: falta el archivo {nombre.lower()}.cas", pos, m, nota)

    def lee(self, ruta):
        try:
            with open(ruta, encoding='utf-8') as f:
                texto = f.read()
        except (OSError, UnicodeDecodeError) as e:
            raise ErrorCastel(f"no puedo leer {ruta}: {e}")
        self.textos[ruta] = texto
        try:
            return Sintactico(Lexico(texto).lee()).modulo()
        except ErrorCastel as e:
            e.archivo = ruta
            raise

    def carga(self, ruta, arbol=None):
        if arbol is None:
            arbol = self.lee(ruta)
        m = Modulo(arbol.nombre, ruta, self.textos[ruta], arbol)
        self.en_curso.append(m.nombre)
        for d in arbol.decls:
            if d.k != 'importar':
                continue
            for nombre, pos in d.nombres:
                if nombre in self.en_curso:
                    self.error("los módulos se importan en círculo: "
                               + ' -> '.join(self.en_curso[self.en_curso.index(nombre):] + [nombre]), pos, m)
                if any(x.nombre == nombre for x in m.importa):
                    continue
                otro = self.mods.get(nombre)
                if otro is None:
                    otro = self.carga(*self.busca(nombre, m, pos))
                m.importa.append(otro)
        self.en_curso.pop()
        self.mods[m.nombre] = m
        self.orden.append(m)
        return m


def compila(ruta, rutas=()):
    """Compila el programa de ese archivo (con sus módulos): da el ensamblador."""
    c = Cargador(list(rutas))
    try:
        c.carga(ruta).principal = True
        return Generador(c.orden).genera()
    except ErrorCastel as e:
        e.textos = c.textos
        raise


def main():
    args = sys.argv[1:]
    entrada = None
    salida = None
    rutas = []
    i = 0
    while i < len(args):
        if args[i] in ('-o', '-I') and i + 1 < len(args):
            if args[i] == '-o':
                salida = args[i + 1]
            else:
                rutas.append(args[i + 1])
            i += 2
        elif args[i].startswith('-I') and len(args[i]) > 2:
            rutas.append(args[i][2:])
            i += 1
        elif entrada is None and not args[i].startswith('-'):
            entrada = args[i]
            i += 1
        else:
            entrada = None
            break
    if entrada is None:
        print("uso: ccastel.py ENTRADA.cas [-o SALIDA.asm] [-I CARPETA_DE_MÓDULOS]...", file=sys.stderr)
        return 2
    if salida is None:
        salida = entrada.rsplit('.', 1)[0] + '.asm'
    try:
        asm = compila(entrada, rutas)
    except ErrorCastel as e:
        archivo = e.archivo or entrada
        if e.pos:
            print(f"{archivo}:{e.pos[0]}:{e.pos[1]}: error: {e.msg}", file=sys.stderr)
            lineas = getattr(e, 'textos', {}).get(archivo, '').split('\n')
            if 0 < e.pos[0] <= len(lineas):
                ln = lineas[e.pos[0] - 1].replace('\t', ' ')
                print('    ' + ln, file=sys.stderr)
                print('    ' + ' ' * (e.pos[1] - 1) + '^', file=sys.stderr)
        elif e.archivo:
            print(f"{archivo}: error: {e.msg}", file=sys.stderr)
        else:
            print(f"ccastel: {e.msg}", file=sys.stderr)
        if e.nota:
            print(f"    ({e.nota})", file=sys.stderr)
        return 1
    with open(salida, 'w', encoding='utf-8') as f:
        f.write(asm)
    print(f"ccastel: {entrada} -> {salida}  ({len(asm.splitlines())} líneas de ensamblador)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
