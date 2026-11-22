#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# fuzz_formulas.py SALIDA [N]: un guion de CUENTAS --guion con N fórmulas
# hechas al azar (bien y mal escritas, metidas unas en otras, con trozos
# rotos y octetos sueltos) para el analizador y el evaluador de Cuentas 2:
# con ASan y UBSan no debe quejarse nada, y lo que se lee bien tiene que
# escribirse y volver a leerse igual («ida»).
import sys, random

sal, n = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 2000
R = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 7)
FUN = ['SUMA', 'SI', 'BUSCARV', 'INDICE', 'COINCIDIR', 'TEXTO', 'FECHA', 'PAGO', 'TASA', 'TIR', 'CONCATENAR', 'EXTRAE',
       'SUMAR.SI', 'CONTAR.SI', 'REDONDEAR', 'DESREF', 'INDIRECTO', 'SUBTOTALES', 'Y', 'O', 'NO', 'MAX', 'MEDIANA',
       'DIAS.LAB', 'SUSTITUIR', 'REPETIR', 'ELEGIR', 'AÑO', 'SIFECHA', 'VNA', 'SUMAPRODUCTO', 'K.ESIMO.MAYOR', 'FOO',
       'sum', 'vlookup', '_xlfn.CONCAT', 'NUMERO.ROMANO', 'DIRECCION', 'SI.ERROR', 'TIPO', 'COMBINAT', 'FACT']
OPS = ['+', '-', '*', '/', '^', '&', '=', '<>', '<', '>', '<=', '>=', ':', '%', ' ']


def ref():
    c = R.choice(['A', 'B', 'Z', 'AA', 'IV', 'IW', 'XFD', '$A', 'c'])
    f = R.choice(['1', '2', '10', '65536', '65537', '0', '$3', '999999'])
    r = c + f
    if R.random() < 0.3:
        r += ':' + R.choice(['B9', '$C$10', 'IV65536', 'A', '5'])
    if R.random() < 0.15:
        r = R.choice(['Hoja1!', "'Mi hoja'!", 'Nada!', "'a''b'!", 'Hoja2!']) + r
    if R.random() < 0.05:
        r = R.choice(['A:A', '1:1', '$B:$D', '3:7', 'A:', ':1'])
    return r


def expr(h=0):
    k = R.random()
    if h > 6 or k < 0.25:
        return R.choice([ref(), str(R.randint(-9, 99999)), '1,5', '"tex""to"', 'VERDADERO', '#¡DIV/0!', '#N/A',
                         '0,000001', '1E+300', '"', 'IVA', '()', '', 'ñ', '1,2,3', '"€"'])
    if k < 0.55:
        f = R.choice(FUN)
        args = [expr(h + 1) for _ in range(R.randint(0, 4))]
        return f + '(' + R.choice([';', ';', ',', ';;']).join(args) + R.choice([')', ')', ')', ''])
    if k < 0.8:
        return expr(h + 1) + R.choice(OPS) + expr(h + 1)
    if k < 0.9:
        return R.choice(['-', '+', '--']) + expr(h + 1)
    return '(' + expr(h + 1) + R.choice([')', ')', ''])


L = ['pon B9 3', 'pon C10 4', 'hoja Mi hoja', 'pon A1 2', 'hoja Hoja1', 'nombre IVA =Hoja1!$B$9']
for i in range(n):
    f = expr()
    if R.random() < 0.1:
        # octetos rotos
        b = bytearray(f.encode('utf-8', 'replace'))
        for _ in range(R.randint(1, 3)):
            if b:
                b[R.randrange(len(b))] = R.randrange(1, 256) if R.random() < 0.5 else ord(R.choice('()";:!$#'))
        f = b.decode('utf-8', 'replace').replace('\n', ' ').replace('\r', ' ')
    if R.random() < 0.02:
        f = '(' * R.randint(150, 400) + '1' + ')' * R.randint(150, 400)
    if R.random() < 0.02:
        f = '+'.join(['A1'] * R.randint(500, 3000))
    c = 'D%d' % (i % 500 + 1)
    L.append('prueba %s =%s' % (c, f))
    L.append('ida %s' % c)
with open(sal, 'w', encoding='utf-8') as fh:
    fh.write('\n'.join(L) + '\n')
