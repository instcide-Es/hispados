#!/usr/bin/env python3
"""Genera lib/hspopl_tab.inc: las tablas de la síntesis FM (OPL2) de HSPSND.

  python3 tools/mkopl.py > lib/hspopl_tab.inc

Salen de las fórmulas que describen el chip (no se copian de ningún otro
programa):
  - seno en logaritmo: un cuarto de onda de 256 puntos,
        -log2(sin((i + 0,5) * pi / 512)) * 256
  - exponencial: (2 ^ (i / 256)) * 1024, ya doblada (salida de 13 bits);
    se consulta con el índice al revés (255 - fracción) y se desplaza a la
    derecha la parte entera del logaritmo
  - atenuación por altura (KSL): 6 dB por octava, en pasos de 0,1875 dB
    para los cuatro bits altos de la frecuencia: techo(32 + 8 * log2(i))
  - multiplicadores de frecuencia (por dos: el 0 es medio)
"""
import math

def words(name, vals, per=8):
    out = []
    for k in range(0, len(vals), per):
        lab = name if k == 0 else ''
        out.append("%-11s dw %s" % (lab, ", ".join("%d" % v for v in vals[k:k + per])))
    return "\n".join(out)

logsin = [round(-math.log2(math.sin((i + 0.5) * math.pi / 512)) * 256) for i in range(256)]
expw = [(round((2 ** (i / 256) - 1) * 1024) + 1024) * 2 for i in range(256)]
ksl = [0] + [math.ceil(32 + 8 * math.log2(i) - 1e-9) for i in range(1, 16)]
mult = [1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30]

print("; hspopl_tab.inc - tablas de la FM. Generado por tools/mkopl.py: no editar.")
print(words("fm_logsin", logsin))
print(words("fm_exp", expw))
print("fm_kslrom   db " + ", ".join(str(v) for v in ksl))
print("fm_mult     db " + ", ".join(str(v) for v in mult))
