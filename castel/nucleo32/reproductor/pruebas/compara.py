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
    if '--desde-sonido' in args:
        args.remove('--desde-sonido')
    if '--ganancia' in args:
        args.remove('--ganancia')
    if '--sin-alinear' in args:
        args.remove('--sin-alinear')
        alinear = False
    ha, a = lee(args[0])
    hb, b = lee(args[1])
    if '--desde-sonido' in sys.argv:
        # (lo grabado por QEMU: se quita lo de delante hasta que suena, en los dos)
        def primero(x):
            i = 0
            while i < len(x) // 2 and abs(x[2 * i]) < 300 and abs(x[2 * i + 1]) < 300:
                i += 1
            return max(0, i - 200)
        a = a[2 * primero(a):]
        b = b[2 * primero(b):]
        # y lo de detrás del final de la referencia
        a = a[:len(b) + 2 * max_d]
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
        # (primero a grandes rasgos, con las dos diezmadas de 8 en 8 y un
        # segundo largo: así no se confunde un periodo de un tono con otro;
        # luego fino, punto a punto)
        def diezma(x, desde, n):
            return [sum(x[2 * (desde + 8 * k + j)] for j in range(8)) for k in range(n)]
        n8 = min(6000, (nb - ini) // 8 - 1, (na - ini - max_d) // 8 - 1)
        bd = diezma(b, ini, n8)
        mejor8, cmax = 0, -2
        for d8 in range(-(max_d // 8), max_d // 8 + 1):
            if ini + 8 * d8 < 0 or ini + 8 * d8 + 8 * n8 >= na:
                continue
            ad = diezma(a, ini + 8 * d8, n8)
            sab = sum(x * y for x, y in zip(ad, bd)); saa = sum(x * x for x in ad); sbb = sum(y * y for y in bd)
            c = sab / math.sqrt(saa * sbb) if saa and sbb else -2
            if c > cmax:
                cmax, mejor8 = c, d8
        cmax = -2
        for d in range(8 * mejor8 - 12, 8 * mejor8 + 13):
            if ini + d < 0 or ini + d + 16000 >= na or ini + 16000 >= nb:
                continue
            sab = saa = sbb = 0.0
            for i in range(0, 16000, 2):
                x, y = a[2 * (ini + d + i)], b[2 * (ini + i)]
                sab += x * y; saa += x * x; sbb += y * y
            c = sab / math.sqrt(saa * sbb) if saa and sbb else -2
            if c > cmax:
                cmax, mejor = c, d
    # la ganancia que mejor casa (lo grabado puede ir con otro volumen)
    g = 1.0
    if '--ganancia' in sys.argv:
        num = den = 0.0
        for i in range(max(0, -mejor), min(nb, na - mejor), 3):
            x = b[2 * i]; y = a[2 * (i + mejor)]
            num += x * y; den += y * y
        g = num / den if den else 1.0
        print('ganancia=%.4f (%.2f dB)' % (g, 20 * math.log10(abs(g) + 1e-12)))
    sen = ruido = 0.0
    dmax = 0
    n = 0
    for i in range(max(0, -mejor), min(nb, na - mejor)):
        for c in (0, 1):
            x = b[2 * i + c]
            y = a[2 * (i + mejor) + c] * g
            sen += x * x
            ruido += (x - y) ** 2
            dmax = max(dmax, abs(x - y))
            n += 1
    snr = 999.0 if ruido == 0 else 10 * math.log10(max(sen, 1) / ruido)
    print('hz=%d/%d tramas=%d/%d desfase=%d snr=%.1f dif_max=%d comunes=%d' % (ha, hb, na, nb, mejor, snr, int(dmax), n // 2))


main()
