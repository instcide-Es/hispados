#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  ccastel.py - compilador minimo de Castel (Fase 1, etapa 0).
#  Castel: lenguaje de sistemas en espanol, familia Pascal/Oberon.
#  Este es el PRIMER compilador, escrito en Python en el PC: lee el
#  subconjunto minimo de CASTEL.md y SACA ENSAMBLADOR (NASM) para un .COM de
#  DOS de 16 bits. No es bonito ni rapido: solo correcto. Luego NASM remata.
#
#  uso:   python3 -I ccastel.py ENTRADA.cas [-o SALIDA.asm]
#         (sin -o, escribe ENTRADA.asm al lado)
#
#  Subconjunto (CASTEL.md seccion 8): modulo con principal; constante y
#  variable; tipos nat8/nat16/ent16/byte/bool; funciones con y sin retorno;
#  :=; si/sino/fin; mientras/fin; para/desde/hasta/hacer/fin; romper/seguir;
#  devolver; ensamblador/fin; aritmetica (+ - * div mod), comparaciones,
#  logicas (y o no); llamadas; funciones de bajo nivel (meter_puerto,
#  sacar_puerto, leer_byte, poner_byte, escribir); comentarios ; y (* *).
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# =============================================================================
import sys

# ---- palabras clave (acentuadas, como en CASTEL.md; se aceptan asi) --------
PALABRAS = {
    'módulo', 'fin', 'constante', 'variable', 'función', 'devolver',
    'si', 'entonces', 'sino', 'mientras', 'hacer', 'para', 'desde', 'hasta',
    'paso', 'romper', 'seguir', 'ensamblador', 'y', 'o', 'no', 'div', 'mod',
    'cierto', 'falso',
}
TIPOS = {'nat8', 'nat16', 'ent16', 'byte', 'bool'}
BUILTINS = {'escribir', 'meter_puerto', 'sacar_puerto', 'leer_byte', 'poner_byte'}


class ErrorCastel(Exception):
    pass


def es_letra(c):
    # letra ASCII, vocal acentuada/dieresis, n~, o _
    return c.isalpha() or c == '_' or c in 'áéíóúÁÉÍÓÚñÑüÜ'


# =============================================================================
#  1. El lector (analisis lexico)
# =============================================================================
class Lexico:
    def __init__(self, texto):
        self.t = texto
        self.i = 0
        self.n = len(texto)
        self.linea = 1
        self.toks = []

    def error(self, msg):
        raise ErrorCastel(f"linea {self.linea}: {msg}")

    def lee(self):
        while self.i < self.n:
            c = self.t[self.i]
            if c == '\n':
                self.emite('NL', '\\n')
                self.i += 1
                self.linea += 1
                continue
            if c in ' \t\r':
                self.i += 1
                continue
            if c == ';':                       # comentario de linea
                while self.i < self.n and self.t[self.i] != '\n':
                    self.i += 1
                continue
            if c == '(' and self.i + 1 < self.n and self.t[self.i + 1] == '*':
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
        self.emite('EOF', '')
        return self.toks

    def emite(self, tipo, val):
        # colapsar NL repetidos (lineas en blanco) en uno solo
        if tipo == 'NL' and self.toks and self.toks[-1][0] == 'NL':
            return
        self.toks.append((tipo, val, self.linea))

    def comentario_bloque(self):
        prof = 0
        while self.i < self.n:
            if self.t[self.i:self.i + 2] == '(*':
                prof += 1
                self.i += 2
            elif self.t[self.i:self.i + 2] == '*)':
                prof -= 1
                self.i += 2
                if prof == 0:
                    return
            else:
                if self.t[self.i] == '\n':
                    self.linea += 1
                self.i += 1
        self.error("comentario de bloque (* sin cerrar")

    def cadena(self):
        self.i += 1
        s = []
        while self.i < self.n and self.t[self.i] != '"':
            c = self.t[self.i]
            if c == '\\':
                self.i += 1
                e = self.t[self.i] if self.i < self.n else ''
                s.append({'n': '\n', 't': '\t', '"': '"', '\\': '\\', '0': '\0'}.get(e, e))
            elif c == '\n':
                self.error("texto sin cerrar (falta la comilla \")")
            else:
                s.append(c)
            self.i += 1
        if self.i >= self.n:
            self.error("texto sin cerrar (falta la comilla \")")
        self.i += 1
        self.emite('CAD', ''.join(s))

    def caracter(self):
        self.i += 1
        if self.t[self.i] == '\\':
            self.i += 1
            e = self.t[self.i]
            val = {'n': 10, 't': 9, '"': 34, '\\': 92, '0': 0, "'": 39}.get(e, ord(e))
            self.i += 1
        else:
            val = ord(self.t[self.i])
            self.i += 1
        if self.i >= self.n or self.t[self.i] != "'":
            self.error("caracter mal cerrado (falta ')")
        self.i += 1
        self.emite('NUM', val)

    def numero(self):
        j = self.i
        if self.t[self.i:self.i + 2] in ('0x', '0X'):
            self.i += 2
            k = self.i
            while self.i < self.n and self.t[self.i] in '0123456789abcdefABCDEF':
                self.i += 1
            if self.i == k:
                self.error("numero hexadecimal sin digitos")
            self.emite('NUM', int(self.t[k:self.i], 16))
            return
        while self.i < self.n and self.t[self.i].isdigit():
            self.i += 1
        self.emite('NUM', int(self.t[j:self.i]))

    def identificador(self):
        j = self.i
        while self.i < self.n and (es_letra(self.t[self.i]) or self.t[self.i].isdigit()):
            self.i += 1
        p = self.t[j:self.i]
        if p in PALABRAS:
            self.emite('PAL', p)
        else:
            self.emite('ID', p)

    def simbolo(self):
        dobles = {':=', '<>', '<=', '>='}
        par = self.t[self.i:self.i + 2]
        if par in dobles:
            self.emite('OP', par)
            self.i += 2
            return
        c = self.t[self.i]
        if c in '+-*()=<>,:.':
            self.emite('OP', c)
            self.i += 1
            return
        self.error(f"caracter inesperado: {c!r}")


# =============================================================================
#  2. La gramatica (analisis sintactico) -> arbol
#  Nodos como tuplas: el primer elemento es la etiqueta.
# =============================================================================
class Sintactico:
    def __init__(self, toks):
        self.toks = toks
        self.p = 0

    def tok(self):
        return self.toks[self.p]

    def error(self, msg):
        t = self.tok()
        raise ErrorCastel(f"linea {t[2]}: {msg} (esta '{t[1]}')")

    def salta_nl(self):
        while self.tok()[0] == 'NL':
            self.p += 1

    def come(self, tipo=None, val=None):
        t = self.tok()
        if tipo and t[0] != tipo:
            self.error(f"esperaba {tipo}")
        if val is not None and t[1] != val:
            self.error(f"esperaba '{val}'")
        self.p += 1
        return t

    def es(self, tipo, val=None):
        t = self.tok()
        return t[0] == tipo and (val is None or t[1] == val)

    # ---- modulo ----
    def modulo(self):
        self.salta_nl()
        self.come('PAL', 'módulo')
        nombre = self.come('ID')[1]
        decls = []
        self.salta_nl()
        while not self.es('PAL', 'fin'):
            decls.append(self.declaracion_modulo())
            self.salta_nl()
        self.come('PAL', 'fin')
        fin_nombre = self.come('ID')[1]
        if fin_nombre != nombre:
            self.error(f"el modulo abre '{nombre}' y cierra '{fin_nombre}'")
        if self.es('OP', '.'):
            self.come('OP', '.')
        return ('modulo', nombre, decls)

    def declaracion_modulo(self):
        if self.es('PAL', 'constante'):
            return self.constante()
        if self.es('PAL', 'variable'):
            return self.variable(global_=True)
        if self.es('PAL', 'función'):
            return self.funcion()
        self.error("esperaba constante, variable o funcion")

    def constante(self):
        self.come('PAL', 'constante')
        nombre = self.come('ID')[1]
        self.come('OP', '=')
        t = self.tok()
        if t[0] == 'NUM':
            self.p += 1
            return ('const', nombre, ('num', t[1]))
        if t[0] == 'CAD':
            self.p += 1
            return ('const', nombre, ('cad', t[1]))
        if t[0] == 'OP' and t[1] == '-':
            self.p += 1
            v = self.come('NUM')[1]
            return ('const', nombre, ('num', -v))
        self.error("una constante es un numero o un texto")

    def variable(self, global_=False):
        self.come('PAL', 'variable')
        nombres = [self.come('ID')[1]]
        while self.es('OP', ','):
            self.come('OP', ',')
            nombres.append(self.come('ID')[1])
        self.come('OP', ':')
        tipo = self.come()[1]
        if tipo not in TIPOS:
            self.error("tipo desconocido")
        return ('var', nombres, tipo, global_)

    def funcion(self):
        self.come('PAL', 'función')
        nombre = self.come('ID')[1]
        self.come('OP', '(')
        params = []
        if not self.es('OP', ')'):
            params.append(self.param())
            while self.es('OP', ','):
                self.come('OP', ',')
                params.append(self.param())
        self.come('OP', ')')
        tipo = None
        if self.es('OP', ':'):
            self.come('OP', ':')
            tipo = self.come()[1]
            if tipo not in TIPOS:
                self.error("tipo de retorno desconocido")
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return ('func', nombre, params, tipo, cuerpo)

    def param(self):
        nombre = self.come('ID')[1]
        self.come('OP', ':')
        tipo = self.come()[1]
        if tipo not in TIPOS:
            self.error("tipo de parametro desconocido")
        return (nombre, tipo)

    # ---- bloque de sentencias (hasta 'fin' o 'sino') ----
    def bloque(self):
        sent = []
        self.salta_nl()
        while not (self.es('PAL', 'fin') or self.es('PAL', 'sino')):
            sent.append(self.sentencia())
            self.salta_nl()
        return sent

    def sentencia(self):
        if self.es('PAL', 'variable'):
            return self.variable(global_=False)
        if self.es('PAL', 'constante'):
            return self.constante()
        if self.es('PAL', 'si'):
            return self.si()
        if self.es('PAL', 'mientras'):
            return self.mientras()
        if self.es('PAL', 'para'):
            return self.para()
        if self.es('PAL', 'devolver'):
            self.come('PAL', 'devolver')
            if self.tok()[0] in ('NL',) or self.es('PAL', 'fin'):
                return ('devolver', None)
            return ('devolver', self.expr())
        if self.es('PAL', 'romper'):
            self.come('PAL', 'romper')
            return ('romper',)
        if self.es('PAL', 'seguir'):
            self.come('PAL', 'seguir')
            return ('seguir',)
        if self.es('PAL', 'ensamblador'):
            return self.ensamblador()
        # asignacion o llamada
        if self.es('ID'):
            nombre = self.come('ID')[1]
            if self.es('OP', ':='):
                self.come('OP', ':=')
                return ('asig', nombre, self.expr())
            if self.es('OP', '('):
                args = self.argumentos()
                return ('llamada_sent', nombre, args)
            self.error("esperaba ':=' o '(' tras el nombre")
        self.error("sentencia no reconocida")

    def si(self):
        self.come('PAL', 'si')
        cond = self.expr()
        self.come('PAL', 'entonces')
        entonces = self.bloque()
        sino = None
        if self.es('PAL', 'sino'):
            self.come('PAL', 'sino')
            if self.es('PAL', 'si'):
                sino = [self.si()]      # 'sino si' encadenado
                return ('si', cond, entonces, sino)
            sino = self.bloque()
        self.come('PAL', 'fin')
        return ('si', cond, entonces, sino)

    def mientras(self):
        self.come('PAL', 'mientras')
        cond = self.expr()
        self.come('PAL', 'hacer')
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return ('mientras', cond, cuerpo)

    def para(self):
        self.come('PAL', 'para')
        var = self.come('ID')[1]
        self.come('PAL', 'desde')
        desde = self.expr()
        self.come('PAL', 'hasta')
        hasta = self.expr()
        self.come('PAL', 'hacer')
        cuerpo = self.bloque()
        self.come('PAL', 'fin')
        return ('para', var, desde, hasta, cuerpo)

    def ensamblador(self):
        # tomamos las lineas crudas hasta 'fin'. Trabajamos sobre el texto por
        # tokens: cada NL marca linea; juntamos los tokens de cada linea tal cual.
        self.come('PAL', 'ensamblador')
        lineas = []
        actual = []
        self.salta_nl()
        while not self.es('PAL', 'fin'):
            t = self.tok()
            if t[0] == 'NL':
                if actual:
                    lineas.append(' '.join(actual))
                    actual = []
                self.p += 1
            elif t[0] == 'EOF':
                self.error("bloque ensamblador sin 'fin'")
            else:
                actual.append(str(t[1]))
                self.p += 1
        if actual:
            lineas.append(' '.join(actual))
        self.come('PAL', 'fin')
        return ('asm', lineas)

    def argumentos(self):
        self.come('OP', '(')
        args = []
        if not self.es('OP', ')'):
            args.append(self.expr())
            while self.es('OP', ','):
                self.come('OP', ',')
                args.append(self.expr())
        self.come('OP', ')')
        return args

    # ---- expresiones (precedencia de CASTEL.md seccion 6) ----
    def expr(self):
        return self.e_o()

    def e_o(self):
        izq = self.e_y()
        while self.es('PAL', 'o'):
            self.come('PAL', 'o')
            izq = ('bin', 'o', izq, self.e_y())
        return izq

    def e_y(self):
        izq = self.e_no()
        while self.es('PAL', 'y'):
            self.come('PAL', 'y')
            izq = ('bin', 'y', izq, self.e_no())
        return izq

    def e_no(self):
        if self.es('PAL', 'no'):
            self.come('PAL', 'no')
            return ('un', 'no', self.e_no())
        return self.e_cmp()

    def e_cmp(self):
        izq = self.e_suma()
        if self.tok()[0] == 'OP' and self.tok()[1] in ('=', '<>', '<', '<=', '>', '>='):
            op = self.come('OP')[1]
            izq = ('bin', op, izq, self.e_suma())
        return izq

    def e_suma(self):
        izq = self.e_mul()
        while self.tok()[0] == 'OP' and self.tok()[1] in ('+', '-'):
            op = self.come('OP')[1]
            izq = ('bin', op, izq, self.e_mul())
        return izq

    def e_mul(self):
        izq = self.e_unario()
        while (self.tok()[0] == 'OP' and self.tok()[1] == '*') or self.es('PAL', 'div') or self.es('PAL', 'mod'):
            op = self.come()[1]
            izq = ('bin', op, izq, self.e_unario())
        return izq

    def e_unario(self):
        if self.tok()[0] == 'OP' and self.tok()[1] == '-':
            self.come('OP', '-')
            return ('un', '-', self.e_unario())
        return self.primario()

    def primario(self):
        t = self.tok()
        if t[0] == 'NUM':
            self.p += 1
            return ('num', t[1])
        if t[0] == 'CAD':
            self.p += 1
            return ('cad', t[1])
        if t[0] == 'PAL' and t[1] in ('cierto', 'falso'):
            self.p += 1
            return ('num', 1 if t[1] == 'cierto' else 0)
        if t[0] == 'OP' and t[1] == '(':
            self.come('OP', '(')
            e = self.expr()
            self.come('OP', ')')
            return e
        if t[0] == 'ID':
            self.p += 1
            if self.es('OP', '('):
                return ('llamada', t[1], self.argumentos())
            return ('ref', t[1])
        self.error("esperaba una expresion")


# =============================================================================
#  3/4. Comprobaciones minimas + generacion de ensamblador NASM (.COM)
# =============================================================================
class Generador:
    def __init__(self, arbol):
        self.arbol = arbol
        self.lineas = []          # codigo
        self.datos = []           # variables globales y cadenas
        self.cadenas = {}         # texto -> etiqueta
        self.consts = {}          # nombre -> ('num',v) | ('cad',s)
        self.globales = {}        # nombre -> etiqueta
        self.funcs = {}           # nombre -> (params, tipo)
        self.nlbl = 0
        self.ncad = 0
        self.loops = []           # (etiqueta_continuar, etiqueta_romper)

    def nuevo_lbl(self, base='L'):
        self.nlbl += 1
        return f".{base}{self.nlbl}"

    def etiqueta_cadena(self, s):
        if s in self.cadenas:
            return self.cadenas[s]
        self.ncad += 1
        lbl = f"S_{self.ncad}"
        self.cadenas[s] = lbl
        octetos = ','.join(str(b) for b in s.encode('cp437', 'replace')) or '0'
        self.datos.append(f"{lbl}: db {octetos},0")
        return lbl

    def em(self, ln):
        self.lineas.append('    ' + ln if not ln.endswith(':') and not ln.startswith(';') else ln)

    def error(self, msg):
        raise ErrorCastel(msg)

    def genera(self):
        _, nombre, decls = self.arbol
        # recoger constantes, globales y firmas de funciones primero
        for d in decls:
            if d[0] == 'const':
                self.consts[d[1]] = d[2]
            elif d[0] == 'var':
                for nm in d[1]:
                    self.globales[nm] = f"G_{nm}"
                    self.datos.append(f"G_{nm}: dw 0")
            elif d[0] == 'func':
                self.funcs[d[1]] = (d[2], d[3])
        if 'principal' not in self.funcs:
            self.error("falta la funcion principal()")
        # cabecera .COM
        cab = [
            '; -----------------------------------------------------------------',
            f'; Generado por ccastel desde el modulo {nombre}. NO editar a mano.',
            '; Ensamblar:  nasm -f bin ESTE.asm -o PROGRAMA.COM',
            '; -----------------------------------------------------------------',
            'cpu 8086',
            'bits 16',
            'org 0x100',
            '    call F_principal',
            '    mov ax, 0x4C00',
            '    int 0x21',
        ]
        self.lineas = cab
        # funciones del usuario
        for d in decls:
            if d[0] == 'func':
                self.gen_funcion(d)
        # rutinas de apoyo + datos
        self.lineas.append('')
        self.lineas.append('; ---- rutinas de apoyo ----')
        self.lineas.extend(APOYO.splitlines())
        self.lineas.append('')
        self.lineas.append('; ---- datos ----')
        self.lineas.extend(self.datos)
        return '\n'.join(self.lineas) + '\n'

    def gen_funcion(self, d):
        _, nombre, params, tipo, cuerpo = d
        locales = {}
        offset = [0]

        # reunir variables locales (declaradas en el cuerpo) para el marco
        def recoge(sents):
            for s in sents:
                if s[0] == 'var':
                    for nm in s[1]:
                        offset[0] += 2
                        locales[nm] = -offset[0]
                elif s[0] == 'si':
                    recoge(s[2])
                    if s[3]:
                        recoge(s[3])
                elif s[0] in ('mientras',):
                    recoge(s[2])
                elif s[0] == 'para':
                    recoge(s[4])
        recoge(cuerpo)

        self.cur_params = {}
        for i, (nm, _tp) in enumerate(params):
            self.cur_params[nm] = 4 + 2 * i      # [bp+4] primer parametro
        self.cur_locales = locales
        self.cur_fin = f".fin_{nombre}"

        self.lineas.append('')
        self.lineas.append(f'F_{nombre}:')
        self.em('push bp')
        self.em('mov bp, sp')
        if offset[0]:
            self.em(f'sub sp, {offset[0]}')
        for s in cuerpo:
            self.gen_sent(s)
        self.lineas.append(f'{self.cur_fin}:')
        self.em('mov sp, bp')
        self.em('pop bp')
        self.em('ret')

    # ---- localizar una variable (parametro, local o global) ----
    def lugar(self, nombre):
        if nombre in self.cur_params:
            return f"[bp+{self.cur_params[nombre]}]"
        if nombre in self.cur_locales:
            return f"[bp{self.cur_locales[nombre]}]"
        if nombre in self.globales:
            return f"[{self.globales[nombre]}]"
        return None

    # ---- sentencias ----
    def gen_sent(self, s):
        k = s[0]
        if k == 'var':
            return                      # el espacio ya esta en el marco
        if k == 'const':
            self.consts[s[1]] = s[2]
            return
        if k == 'asig':
            lug = self.lugar(s[1])
            if lug is None:
                self.error(f"variable no declarada: {s[1]}")
            self.gen_expr(s[2])
            self.em(f'mov word {lug}, ax')
            return
        if k == 'llamada_sent':
            self.gen_llamada(s[1], s[2], como_sent=True)
            return
        if k == 'devolver':
            if s[1] is not None:
                self.gen_expr(s[1])
            self.em(f'jmp {self.cur_fin}')
            return
        if k == 'si':
            self.gen_si(s)
            return
        if k == 'mientras':
            self.gen_mientras(s)
            return
        if k == 'para':
            self.gen_para(s)
            return
        if k == 'romper':
            if not self.loops:
                self.error("'romper' fuera de un bucle")
            self.em(f'jmp {self.loops[-1][1]}')
            return
        if k == 'seguir':
            if not self.loops:
                self.error("'seguir' fuera de un bucle")
            self.em(f'jmp {self.loops[-1][0]}')
            return
        if k == 'asm':
            for ln in s[1]:
                self.lineas.append('    ' + ln)
            return
        self.error(f"sentencia no soportada: {k}")

    def gen_si(self, s):
        _, cond, entonces, sino = s
        l_sino = self.nuevo_lbl('sino')
        l_fin = self.nuevo_lbl('finsi')
        self.gen_expr(cond)
        self.em('cmp ax, 0')
        self.em(f'je {l_sino}')
        for x in entonces:
            self.gen_sent(x)
        self.em(f'jmp {l_fin}')
        self.lineas.append(f'{l_sino}:')
        if sino:
            for x in sino:
                self.gen_sent(x)
        self.lineas.append(f'{l_fin}:')

    def gen_mientras(self, s):
        _, cond, cuerpo = s
        l_ini = self.nuevo_lbl('mie')
        l_fin = self.nuevo_lbl('finmie')
        self.lineas.append(f'{l_ini}:')
        self.gen_expr(cond)
        self.em('cmp ax, 0')
        self.em(f'je {l_fin}')
        self.loops.append((l_ini, l_fin))
        for x in cuerpo:
            self.gen_sent(x)
        self.loops.pop()
        self.em(f'jmp {l_ini}')
        self.lineas.append(f'{l_fin}:')

    # gen_para se define limpio mas abajo: Generador.gen_para = _gen_para_limpio


# El 'para': tope del bucle guardado en la pila (evaluado una sola vez), leido
# con un peek (pop/push) en cada vuelta. Comparacion con signo (ent16).
def _gen_para_limpio(self, s):
    _, var, desde, hasta, cuerpo = s
    lug = self.lugar(var)
    if lug is None:
        self.error(f"la variable del 'para' no esta declarada: {var}")
    l_cond = self.nuevo_lbl('para')
    l_cont = self.nuevo_lbl('paracont')
    l_fin = self.nuevo_lbl('finpara')
    self.gen_expr(desde)
    self.em(f'mov word {lug}, ax')
    self.gen_expr(hasta)
    self.em('push ax')                 # tope en la cima de la pila -> [bp? ] no; usamos cx guardado
    # Guardamos el tope en la pila y lo leemos con un registro cada vuelta.
    self.lineas.append(f'{l_cond}:')
    self.em('pop cx')                  # recupera tope
    self.em('push cx')                 # y lo deja otra vez (lo necesitamos la proxima vuelta)
    self.em(f'mov ax, {lug}')
    self.em('cmp ax, cx')
    self.em(f'jg {l_fin}')             # signed: si v > tope, fin
    self.loops.append((l_cont, l_fin))
    for x in cuerpo:
        self.gen_sent(x)
    self.loops.pop()
    self.lineas.append(f'{l_cont}:')
    self.em(f'inc word {lug}')
    self.em(f'jmp {l_cond}')
    self.lineas.append(f'{l_fin}:')
    self.em('pop cx')                  # quita el tope de la pila


Generador.gen_para = _gen_para_limpio


def _gen_expr(self, e):
    k = e[0]
    if k == 'num':
        self.em(f'mov ax, {e[1] & 0xFFFF}')
        return
    if k == 'cad':
        lbl = self.etiqueta_cadena(e[1])
        self.em(f'mov ax, {lbl}')
        return
    if k == 'ref':
        if e[1] in self.consts:
            c = self.consts[e[1]]
            if c[0] == 'num':
                self.em(f'mov ax, {c[1] & 0xFFFF}')
            else:
                self.em(f'mov ax, {self.etiqueta_cadena(c[1])}')
            return
        lug = self.lugar(e[1])
        if lug is None:
            self.error(f"nombre no declarado: {e[1]}")
        self.em(f'mov ax, {lug}')
        return
    if k == 'un':
        if e[1] == '-':
            self.gen_expr(e[2])
            self.em('neg ax')
            return
        if e[1] == 'no':
            self.gen_expr(e[2])
            self.em('xor ax, 1')
            return
    if k == 'llamada':
        self.gen_llamada(e[1], e[2], como_sent=False)
        return
    if k == 'bin':
        return self.gen_bin(e)
    self.error(f"expresion no soportada: {k}")


def _gen_bin(self, e):
    _, op, izq, der = e
    self.gen_expr(izq)
    self.em('push ax')
    self.gen_expr(der)
    self.em('mov cx, ax')
    self.em('pop ax')               # ax=izq, cx=der
    if op == '+':
        self.em('add ax, cx')
    elif op == '-':
        self.em('sub ax, cx')
    elif op == '*':
        self.em('imul cx')          # dx:ax = ax*cx ; nos quedamos con ax
    elif op == 'div':
        self.em('cwd')
        self.em('idiv cx')          # ax = cociente
    elif op == 'mod':
        self.em('cwd')
        self.em('idiv cx')
        self.em('mov ax, dx')       # dx = resto
    elif op == 'y':
        self.em('and ax, cx')
    elif op == 'o':
        self.em('or ax, cx')
    elif op in ('=', '<>', '<', '<=', '>', '>='):
        salto = {'=': 'je', '<>': 'jne', '<': 'jl', '<=': 'jle', '>': 'jg', '>=': 'jge'}[op]
        l1 = self.nuevo_lbl('cmp')
        l2 = self.nuevo_lbl('cmpf')
        self.em('cmp ax, cx')
        self.em(f'{salto} {l1}')
        self.em('mov ax, 0')
        self.em(f'jmp {l2}')
        self.lineas.append(f'{l1}:')
        self.em('mov ax, 1')
        self.lineas.append(f'{l2}:')
    else:
        self.error(f"operador no soportado: {op}")


def _gen_llamada(self, nombre, args, como_sent):
    # funciones de bajo nivel (magicas)
    if nombre == 'escribir':
        if len(args) != 1:
            self.error("escribir() lleva un argumento")
        a = args[0]
        es_texto = a[0] == 'cad' or (a[0] == 'ref' and a[1] in self.consts and self.consts[a[1]][0] == 'cad')
        self.gen_expr(a)                 # direccion (cad) o valor (num) en ax
        if es_texto:
            self.em('mov dx, ax')
            self.em('call escribir_cad')
        else:
            self.em('call escribir_num')
        return
    if nombre == 'meter_puerto':
        self.gen_expr(args[0])
        self.em('mov dx, ax')
        self.em('in al, dx')
        self.em('mov ah, 0')
        return
    if nombre == 'sacar_puerto':
        self.gen_expr(args[0])
        self.em('push ax')
        self.gen_expr(args[1])
        self.em('mov cx, ax')            # dato
        self.em('pop dx')                # puerto
        self.em('mov al, cl')
        self.em('out dx, al')
        return
    if nombre == 'leer_byte':
        self.gen_expr(args[0])
        self.em('push ax')
        self.gen_expr(args[1])
        self.em('mov bx, ax')
        self.em('pop ax')
        self.em('mov es, ax')
        self.em('mov al, [es:bx]')
        self.em('mov ah, 0')
        return
    if nombre == 'poner_byte':
        self.gen_expr(args[0])           # seg
        self.em('push ax')
        self.gen_expr(args[1])           # desp
        self.em('push ax')
        self.gen_expr(args[2])           # dato
        self.em('mov cx, ax')
        self.em('pop bx')
        self.em('pop ax')
        self.em('mov es, ax')
        self.em('mov al, cl')
        self.em('mov [es:bx], al')
        return
    # funcion del usuario
    if nombre not in self.funcs:
        self.error(f"funcion no declarada: {nombre}")
    params, tipo = self.funcs[nombre]
    if len(args) != len(params):
        self.error(f"{nombre}() espera {len(params)} argumentos, recibe {len(args)}")
    for a in reversed(args):             # derecha a izquierda
        self.gen_expr(a)
        self.em('push ax')
    self.em(f'call F_{nombre}')
    if params:
        self.em(f'add sp, {2 * len(params)}')


Generador.gen_expr = _gen_expr
Generador.gen_bin = _gen_bin
Generador.gen_llamada = _gen_llamada


# Rutinas de apoyo en ensamblador (impresion por DOS int 21h).
APOYO = r"""
; escribir_cad: imprime la cadena 0-terminada en DS:DX (int 21h/02h por caracter)
escribir_cad:
    push ax
    push dx
    push si
    mov si, dx
.ec_l:
    mov al, [si]
    cmp al, 0
    je .ec_fin
    mov dl, al
    mov ah, 2
    int 0x21
    inc si
    jmp .ec_l
.ec_fin:
    pop si
    pop dx
    pop ax
    ret

; escribir_num: imprime AX como entero con signo en decimal
escribir_num:
    push ax
    push bx
    push cx
    push dx
    mov cx, 0
    cmp ax, 0
    jge .en_pos
    push ax
    mov dl, '-'
    mov ah, 2
    int 0x21
    pop ax
    neg ax
.en_pos:
    mov bx, 10
.en_div:
    xor dx, dx
    div bx                 ; dx:ax / 10 -> ax cociente, dx resto
    add dl, '0'
    push dx
    inc cx
    cmp ax, 0
    jne .en_div
.en_imp:
    pop dx
    mov ah, 2
    int 0x21
    loop .en_imp
    pop dx
    pop cx
    pop bx
    pop ax
    ret
"""


def compila(texto):
    toks = Lexico(texto).lee()
    arbol = Sintactico(toks).modulo()
    return Generador(arbol).genera()


def main():
    args = sys.argv[1:]
    if not args:
        print("uso: ccastel.py ENTRADA.cas [-o SALIDA.asm]", file=sys.stderr)
        return 2
    entrada = args[0]
    salida = None
    if '-o' in args:
        salida = args[args.index('-o') + 1]
    if salida is None:
        salida = entrada.rsplit('.', 1)[0] + '.asm'
    try:
        with open(entrada, encoding='utf-8') as f:
            texto = f.read()
        asm = compila(texto)
    except ErrorCastel as e:
        print(f"ccastel: error: {e}", file=sys.stderr)
        return 1
    with open(salida, 'w', encoding='utf-8') as f:
        f.write(asm)
    print(f"ccastel: {entrada} -> {salida}  ({len(asm.splitlines())} lineas de ensamblador)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
