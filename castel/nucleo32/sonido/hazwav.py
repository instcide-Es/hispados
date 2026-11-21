#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# (Los sonidos que hace, además, se dan al dominio público: CC0 1.0.)
# nucleo32/sonido/hazwav.py CARPETA
# (SONIDO) Hace los sonidos del sistema de Hispa-Pentium, sintetizados aquí
# (nada de fuera): WAV de 22.050 Hz, 16 bits, mono, cortos.
#   INICIO.WAV  do, mi, sol y do (523, 659, 784 y 1047 Hz), de campana, 1,4 s
#   APAGAR.WAV  sol, mi y do, bajando, 1,1 s
#   AVISO.WAV   «din-don»: la y fa (880 y 698 Hz), 0,6 s
#   ERROR.WAV   dos golpes graves (mi, 165 Hz, con armónicos), 0,45 s
# El mismo programa da siempre los mismos octetos.
import math
import struct
import sys

HZ = 22050


def campana(f, t0, dura, fuerza=1.0):
    """una nota de campana: el tono, su octava y un poco de quinta, con su caída"""
    return (f, t0, dura, fuerza)


def hace(notas, largo, timbre="campana"):
    n = int(largo * HZ)
    s = [0.0] * n
    for f, t0, dura, fuerza in notas:
        i0 = int(t0 * HZ)
        for k in range(int(dura * HZ)):
            i = i0 + k
            if i >= n:
                break
            t = k / HZ
            ataque = min(1.0, t / 0.006)
            if timbre == "campana":
                caida = math.exp(-t * 4.0)
                v = (math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * 2 * f * t) * math.exp(-t * 6)
                     + 0.12 * math.sin(2 * math.pi * 3 * f * t) * math.exp(-t * 9))
            else:
                caida = math.exp(-t * 9.0)
                v = sum(math.sin(2 * math.pi * f * h * t) / h for h in (1, 2, 3, 4, 5))
            s[i] += fuerza * ataque * caida * v
    pico = max(abs(x) for x in s) or 1
    # (la cola, sin chasquido)
    for k in range(min(n, int(0.03 * HZ))):
        s[n - 1 - k] *= k / (0.03 * HZ)
    return [int(x / pico * 0.62 * 32767) for x in s]


def escribe(ruta, m):
    datos = struct.pack("<%dh" % len(m), *m)
    with open(ruta, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, HZ, HZ * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(datos)) + datos)


def principal():
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    escribe(d + "/INICIO.WAV", hace([campana(523.25, 0.0, 1.4), campana(659.26, 0.16, 1.24), campana(783.99, 0.32, 1.08),
                                     campana(1046.5, 0.48, 0.92, 0.8)], 1.4))
    escribe(d + "/APAGAR.WAV", hace([campana(783.99, 0.0, 1.1), campana(659.26, 0.2, 0.9), campana(523.25, 0.4, 0.7)], 1.1))
    escribe(d + "/AVISO.WAV", hace([campana(880.0, 0.0, 0.6), campana(698.46, 0.17, 0.43)], 0.6))
    escribe(d + "/ERROR.WAV", hace([(164.81, 0.0, 0.2, 1.0), (164.81, 0.22, 0.23, 1.0)], 0.45, "grave"))


principal()
