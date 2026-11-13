#!/usr/bin/env python3
"""Hace los sonidos propios de Hispa-DOS (no hay nada grabado ni de nadie:
todo sale de las fórmulas de este guion).

uso: mksample.py CARPETA

  HISPA.WAV    el sonido de prueba del Reproductor de sonido (SONIDOS.HVP):
               una melodía corta y alegre (un arpegio de do mayor, una frase
               y un acorde final) con timbre de teclado de láminas: cuatro
               armónicos, ataque rápido y caída exponencial. 22.050 Hz, 16
               bits, estéreo (los dos canales iguales). La primera nota es
               un do de 523 Hz que suena sola (la miden las pruebas).
  CAMPANA.WAV  la campanita que suena al cambiar el volumen en Hispa
               Ventanas: cuatro parciales que no son armónicos entre sí (como
               los de una campana) con caída exponencial, más rápida en los
               agudos. 0,3 s a 11.025 Hz, 8 bits, mono: unos 3 KB.
Los dos llevan la cabecera WAV corriente de 44 octetos (quien los lee en
Hispa Ventanas cuenta con ello).
Builds the project's own sound samples (pure synthesis, no recordings).
"""
import math, os, struct, sys


def wav(ruta, hz, bits, canales, muestras):
    """muestras: lista de valores de -1 a 1 (un canal; se repite en los demás)."""
    datos = bytearray()
    for v in muestras:
        v = max(-1.0, min(1.0, v))
        if bits == 8:
            uno = struct.pack('<B', int(round(128 + v * 127)))
        else:
            uno = struct.pack('<h', int(round(v * 32767)))
        datos += uno * canales
    alin = canales * bits // 8
    with open(ruta, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(datos)) + b'WAVEfmt ')
        f.write(struct.pack('<IHHIIHH', 16, 1, canales, hz, hz * alin, alin, bits))
        f.write(b'data' + struct.pack('<I', len(datos)))
        f.write(datos)


# ---- la melodía ----
DO5, RE5, MI5, FA5, SOL5, LA5, SI5, DO6 = 523.25, 587.33, 659.26, 698.46, 783.99, 880.0, 987.77, 1046.5
PASO = 0.17                             # segundos de una nota corta
MELODIA = (                             # (frecuencias, pasos)
    ((DO5,), 2), ((MI5,), 1), ((SOL5,), 1), ((DO6,), 2),            # el arpegio
    ((SOL5,), 1), ((LA5,), 1), ((SOL5,), 1), ((MI5,), 2),           # la frase
    ((SOL5,), 1), ((DO6,), 1), ((SI5,), 1), ((LA5,), 1), ((SOL5,), 2),
    ((MI5,), 1), ((FA5,), 1), ((RE5,), 2),
    ((DO5, MI5, SOL5, DO6), 5),                                     # el acorde
)
ARMONICOS = (1.0, 0.35, 0.15, 0.06)     # (con estos, la onda solo cruza el
                                        # cero dos veces por ciclo)


def nota(buf, hz, t0, frec, dura, fuerza):
    """Suma una nota: suena «dura» y se apaga en 30 ms (la primera, sola)."""
    n0 = int(t0 * hz)
    n = int((dura + 0.03) * hz)
    for i in range(n):
        if n0 + i >= len(buf):
            break
        t = i / hz
        env = min(1.0, t / 0.006) * math.exp(-t / 0.30)
        if t > dura:
            env *= max(0.0, 1 - (t - dura) / 0.03)
        v = 0.0
        for k, a in enumerate(ARMONICOS):
            v += a * math.exp(-t * k * 3) * math.sin(2 * math.pi * frec * (k + 1) * t)
        buf[n0 + i] += fuerza * env * v


def melodia(hz):
    total = sum(p for _, p in MELODIA) * PASO + 0.25
    buf = [0.0] * int(total * hz)
    t = 0.0
    for frecs, pasos in MELODIA:
        dura = pasos * PASO
        if len(frecs) > 1:
            dura += 0.2                 # el acorde final se deja sonar
        for f in frecs:
            nota(buf, hz, t, f, dura, 1.0 / len(frecs) ** 0.5)
        t += pasos * PASO
    pico = max(abs(v) for v in buf)
    return [v * 0.8 / pico for v in buf]


# ---- la campanita ----
def campana(hz, dura=0.3):
    parciales = ((880.0, 1.0, 0.085), (1765.0, 0.55, 0.065),    # (Hz, fuerza,
                 (2429.0, 0.40, 0.045), (3690.0, 0.22, 0.028))  # caída en s)
    n = int(dura * hz)
    buf = []
    for i in range(n):
        t = i / hz
        v = sum(a * math.exp(-t / tau) * math.sin(2 * math.pi * f * t) for f, a, tau in parciales)
        v *= min(1.0, t / 0.002)                    # sin chasquido al empezar
        v *= min(1.0, (dura - t) / 0.03)            # ni al acabar
        buf.append(v)
    pico = max(abs(v) for v in buf)
    return [v * 0.9 / pico for v in buf]


def main():
    carpeta = sys.argv[1] if len(sys.argv) > 1 else '.'
    os.makedirs(carpeta, exist_ok=True)
    # mono: los dos canales eran iguales (el Reproductor hace izquierda y
    # derecha al vuelo) y así ocupa la mitad (206 KB) en el disquete
    wav(os.path.join(carpeta, 'HISPA.WAV'), 22050, 16, 1, melodia(22050))
    wav(os.path.join(carpeta, 'CAMPANA.WAV'), 11025, 8, 1, campana(11025))


if __name__ == '__main__':
    main()
