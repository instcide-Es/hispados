#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/sonido/oye.py GRABACION.WAV [HZ,HZ...]
# (SONIDO) Para las pruebas del sonido de Hispa-Pentium (castel/pruebas.sh,
# la parte 23): lo que QEMU ha grabado de la tarjeta (-audiodev wav), en
# ráfagas (sonidos separados por un cuarto de segundo de silencio). Por cada
# una, una línea:
#   ráfaga N INICIO FIN DURA HZ PICO_IZQ PICO_DER CORTES [HZ=NIVEL...]
# (los tiempos en milisegundos; HZ, la frecuencia de su primera décima de
# segundo, por los pasos por cero; los cortes, silencios de 3 ms o más
# dentro de la ráfaga: la tarjeta se ha quedado sin datos; y, para cada
# frecuencia pedida, su nivel mayor en una décima de segundo de la ráfaga,
# por el algoritmo de Goertzel, en la escala de las muestras).
import math
import struct
import sys
import wave


def principal():
    w = wave.open(sys.argv[1], 'rb')
    ch, hz, n = w.getnchannels(), w.getframerate(), w.getnframes()
    d = w.readframes(n)
    v = struct.unpack('<%dh' % (len(d) // 2), d)
    izq, der = v[0::ch], v[1::ch] if ch > 1 else v[0::ch]
    pedidas = [float(x) for x in sys.argv[2].split(',')] if len(sys.argv) > 2 else []
    ven = hz // 1000 * 3                       # ventanas de 3 ms
    activa = []
    for i in range(0, len(izq) - ven, ven):
        activa.append(max(max(abs(x) for x in izq[i:i + ven]), max(abs(x) for x in der[i:i + ven])) > 200)
    rafagas, ini, ultima = [], None, None
    hueco = 250 // 3
    for k, a in enumerate(activa + [False] * (hueco + 1)):
        if a:
            if ini is None:
                ini = k
            ultima = k
        elif ini is not None and k - ultima > hueco:
            rafagas.append((ini, ultima + 1))
            ini = None
    for num, (a, b) in enumerate(rafagas):
        i0, i1 = a * ven, b * ven
        cortes = 0
        k = a
        while k < b:
            if not activa[k]:
                j = k
                while j < b and not activa[j]:
                    j += 1
                cortes += 1
                k = j
            else:
                k += 1
        trozo = izq[i0:min(i1, i0 + hz // 10)]
        if max((abs(x) for x in trozo), default=0) < max((abs(x) for x in der[i0:min(i1, i0 + hz // 10)]), default=0):
            trozo = der[i0:min(i1, i0 + hz // 10)]
        pasos = sum(1 for q in range(1, len(trozo)) if (trozo[q - 1] < 0) != (trozo[q] < 0))
        f0 = pasos / 2 / (len(trozo) / hz) if trozo else 0
        pi = max((abs(x) for x in izq[i0:i1]), default=0)
        pd = max((abs(x) for x in der[i0:i1]), default=0)
        extra = []
        if pedidas:
            # (el mayor de las ventanas de 100 ms de la ráfaga)
            mejor = dict((f, 0) for f in pedidas)
            paso = hz // 10
            for m0 in range(i0, max(i0 + 1, i1 - paso), paso):
                s = [(izq[q] + der[q]) / 2 for q in range(m0, min(m0 + paso, i1))]
                for f in pedidas:
                    c = 2 * math.cos(2 * math.pi * f / hz)
                    s1 = s2 = 0.0
                    for x in s:
                        s1, s2 = x + c * s1 - s2, s1
                    pot = s1 * s1 + s2 * s2 - c * s1 * s2
                    mejor[f] = max(mejor[f], int(2 * math.sqrt(max(pot, 0)) / max(len(s), 1)))
            extra = ['%d=%d' % (f, mejor[f]) for f in pedidas]
        print('ráfaga %d %d %d %d %d %d %d %d %s' % (num, i0 * 1000 // hz, i1 * 1000 // hz, (i1 - i0) * 1000 // hz,
                                                     f0, pi, pd, cortes, ' '.join(extra)))


principal()
