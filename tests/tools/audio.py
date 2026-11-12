#!/usr/bin/env python3
"""Utilidades de audio para las pruebas de Hispa-DOS.

  audio.py make  fichero.wav hz ms rate bits canales   -> genera un tono
  audio.py check grabacion.wav [umbral]                -> tramos con sonido
                                                          y su frecuencia
"""
import sys, wave, struct, math

def make(path, hz, ms, rate, bits, ch):
    n = rate * ms // 1000
    w = wave.open(path, 'wb')
    w.setnchannels(ch); w.setsampwidth(bits // 8); w.setframerate(rate)
    frames = bytearray()
    for i in range(n):
        v = math.sin(2 * math.pi * hz * i / rate) * 0.6
        for _ in range(ch):
            if bits == 8:
                frames += struct.pack('<B', int(128 + v * 127))
            else:
                frames += struct.pack('<h', int(v * 32767))
    w.writeframes(bytes(frames)); w.close()

def check(path, umbral=1500):
    w = wave.open(path, 'rb')
    ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    if sw != 2:
        print("formato no esperado"); return
    vals = struct.unpack('<%dh' % (len(raw) // 2), raw)[::ch]   # canal izquierdo
    # tramos con sonido: ventanas de 10 ms con amplitud
    win = rate // 100
    active = []
    for i in range(0, len(vals) - win, win):
        seg = vals[i:i + win]
        active.append(max(abs(x) for x in seg) > umbral)
    segs, start = [], None
    for j, a in enumerate(active + [False]):
        if a and start is None:
            start = j
        if not a and start is not None:
            if j - start >= 10:       # al menos 100 ms
                segs.append((start, j))
            start = None
    print("grabación: %d Hz, %d canales, %.2f s" % (rate, ch, n / rate))
    for a, b in segs:
        # frecuencia por ventanas de 50 ms; se separa donde cambia más de un 10 %
        w5 = rate // 20
        runs = []
        for k in range(a * win, b * win - w5 + 1, w5):
            s = vals[k:k + w5]
            zc = sum(1 for q in range(1, len(s)) if (s[q - 1] < 0) != (s[q] < 0))
            hz = zc / 2 / (len(s) / rate)
            if runs and abs(hz - runs[-1][2] / runs[-1][3]) < 0.1 * max(hz, 1):
                runs[-1][1] = k + w5; runs[-1][2] += hz; runs[-1][3] += 1
            else:
                runs.append([k, k + w5, hz, 1])
        for r0, r1, tot, cnt in runs:
            if cnt < 2:
                continue
            seg = vals[r0:r1]
            print("tramo %.2f-%.2f s: %.0f Hz, pico %d" % (r0 / rate, r1 / rate, tot / cnt, max(abs(x) for x in seg)))

if __name__ == '__main__':
    if sys.argv[1] == 'make':
        make(sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7]))
    else:
        check(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1500)
