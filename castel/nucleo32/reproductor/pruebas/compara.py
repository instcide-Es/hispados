#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compara.py A.WAV B.WAV [--desfase N] [--sin-alinear]
# Compara dos WAV de 16 bits (lo del Reproductor y la referencia): los
# alinea (busca el desfase que mejor casa, hasta ±N tramas), y dice el
# desfase, la relación señal/ruido (SNR, en dB), la diferencia más grande y
# los largos. Sale con 0 siempre: quien lo llama mira los números.
import struct, sys, array, math


def lee(ruta):
    d = open(ruta, 'rb').read()
    assert d[:4] == b'RIFF' and d[8:12] == b'WAVE', ruta
    p = 12
    hz = canales = bits = None
    datos = b''
    while p + 8 <= len(d):
        k, n = d[p:p + 4], struct.unpack('<I', d[p + 4:p + 8])[0]
        if k == b'fmt ':
            _, canales, hz, _, _, bits = struct.unpack('<HHIIHH', d[p + 8:p + 24])
        elif k == b'data':
            datos = d[p + 8:p + 8 + n]
            break
        p += 8 + n + (n & 1)
    assert bits == 16, ruta
    a = array.array('h')
    a.frombytes(datos[:len(datos) // 2 * 2])
    if canales == 1:
        b = array.array('h')
        for x in a:
            b.append(x)
            b.append(x)
        a = b
    return hz, a


def main():
    args = sys.argv[1:]
    max_d = 3000
    alinear = True
    if '--desfase' in args:
        i = args.index('--desfase')
        max_d = int(args[i + 1])
        del args[i:i + 2]
    if '--sin-alinear' in args:
        args.remove('--sin-alinear')
        alinear = False
    ha, a = lee(args[0])
    hb, b = lee(args[1])
    na, nb = len(a) // 2, len(b) // 2
    # el desfase: con el lado izquierdo, por trozos de 4096 tramas desde la
    # primera parte con sonido
    mejor = 0
    if alinear:
        ini = 0
        while ini < min(na, nb) - 1 and abs(b[2 * ini]) < 200:
            ini += 1
        ini = min(ini + 2000, max(0, nb - 5000))
        L = min(4096, nb - ini - 1)
        def ssd(d):
            if ini + d < 0 or ini + d + L >= na:
                return 1e300
            return sum((a[2 * (ini + d + i)] - b[2 * (ini + i)]) ** 2 for i in range(0, L, 8))
        todos = [(ssd(d), d) for d in range(-max_d, max_d + 1)]
        minimo = min(t[0] for t in todos)
        # (de los que casan casi igual de bien, el más cercano: con un tono
        # puro casan todos los periodos)
        mejor = min((abs(d), d) for s, d in todos if s <= minimo * 1.05 + 1)[1]
    sen = ruido = 0.0
    dmax = 0
    n = 0
    for i in range(max(0, -mejor), min(nb, na - mejor)):
        for c in (0, 1):
            x = b[2 * i + c]
            y = a[2 * (i + mejor) + c]
            sen += x * x
            ruido += (x - y) ** 2
            dmax = max(dmax, abs(x - y))
            n += 1
    snr = 999.0 if ruido == 0 else 10 * math.log10(max(sen, 1) / ruido)
    print('hz=%d/%d tramas=%d/%d desfase=%d snr=%.1f dif_max=%d comunes=%d' % (ha, hb, na, nb, mejor, snr, dmax, n // 2))


main()
