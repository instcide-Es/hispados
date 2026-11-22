#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# sigue.py GRABADO.WAV REFERENCIA.WAV [--bloque MS]
# ¿Lo que QEMU ha grabado de la tarjeta (-audiodev wav) es la referencia
# (lo que hace el Reproductor en el anfitrión) entera y sin cortes? Busca
# dónde empieza la referencia en lo grabado y luego la sigue a trozos (de
# 100 ms): cada trozo tiene que estar donde tocaba (si se corta el sonido
# —la cola se queda vacía—, lo de detrás llega tarde: el desfase salta) y
# casar (la correlación). Dice:
#   empieza=N tramas=N trozos=N saltos=N peor_correlacion=X desfase_final=N snr=X
# (saltos: los trozos que han llegado más de 2 ms tarde que el anterior; snr,
# la de lo grabado frente a la referencia, con la ganancia que mejor casa,
# en dB). Sale con 0 siempre: quien lo llama mira los números.
import array, math, struct, sys


def lee(ruta):
    d = open(ruta, 'rb').read()
    p = 12
    canales = 2
    datos = b''
    while p + 8 <= len(d):
        k, n = d[p:p + 4], struct.unpack('<I', d[p + 4:p + 8])[0]
        if k == b'fmt ':
            canales = struct.unpack('<H', d[p + 10:p + 12])[0]
        elif k == b'data':
            datos = d[p + 8:p + 8 + n]
            break
        p += 8 + n + (n & 1)
    a = array.array('h')
    a.frombytes(datos[:len(datos) // 2 * 2])
    if canales == 2:
        a = a[0::2]     # (el lado izquierdo basta para seguirla)
    return a


def corr(a, i, b, j, n, paso=2):
    sab = saa = sbb = 0.0
    for k in range(0, n, paso):
        x, y = a[i + k], b[j + k]
        sab += x * y
        saa += x * x
        sbb += y * y
    return sab / math.sqrt(saa * sbb) if saa > 0 and sbb > 0 else 0.0


def main():
    g, r = lee(sys.argv[1]), lee(sys.argv[2])
    bloque = 4800
    if '--bloque' in sys.argv:
        bloque = int(sys.argv[sys.argv.index('--bloque') + 1]) * 48
    # dónde empieza: lo primero con sonido de la referencia, buscado en lo grabado
    j0 = 0
    while j0 < len(r) and abs(r[j0]) < 300:
        j0 += 1
    n0 = min(4800, len(r) - j0 - 1)
    mejor, cm = 0, -2.0
    # dónde puede empezar: donde lo grabado empieza a sonar tras un silencio
    # (de 50 ms o más), y por allí cerca, fino
    callado, candidatos = 2400, []
    for i in range(len(g)):
        if abs(g[i]) < 300:
            callado += 1
        else:
            if callado >= 2400:
                candidatos.append(i)
            callado = 0
    # (con un trozo largo —segundo y medio—: con uno corto, una nota de una
    # escala casa con otra parecida)
    largo = min(72000, len(r) - j0 - 1)
    for c0 in candidatos[:40]:
        for i in range(max(0, c0 - 400), min(len(g) - largo, c0 + 400), 8):
            c = corr(g, i, r, j0, largo, 24)
            if c > cm:
                cm, mejor = c, i
    m0 = mejor
    for i in range(max(0, m0 - 10), min(len(g) - largo, m0 + 10)):
        c = corr(g, i, r, j0, largo, 6)
        if c > cm or i == max(0, m0 - 10):
            if c > cm:
                cm, mejor = c, i
    desfase = mejor - j0
    if '--depura' in sys.argv:
        print('inicio: j0=%d mejor=%d corr=%.6f candidatos=%d' % (j0, mejor, cm, len(candidatos)), file=sys.stderr)
    saltos, peor, trozos = 0, 1.0, 0
    num = den = 0.0
    j = j0
    while j + bloque < len(r) and j + desfase + bloque + 2400 < len(g):
        # (los trozos en silencio no se pueden seguir: se saltan)
        if max(abs(x) for x in r[j:j + bloque:7]) < 300:
            j += bloque
            continue
        bc, bd = -2.0, desfase
        c = corr(g, j + desfase, r, j, bloque, 6) if j + desfase + bloque < len(g) else -2.0
        rango = range(desfase - 96, desfase + 2400, 2)
        if c > 0.9995:
            bc, rango = c, []
        for d in rango:
            if j + d < 0 or j + d + bloque >= len(g):
                continue
            c = corr(g, j + d, r, j, bloque, 6)
            if c > bc:
                bc, bd = c, d
            if c > 0.9999 and d >= desfase:
                break
        if bd > desfase + 96:
            saltos += 1
            if '--depura' in sys.argv:
                print('salto en %.3f s: %d tramas (corr %.5f)' % (j / 48000.0, bd - desfase, bc), file=sys.stderr)
        desfase = bd
        peor = min(peor, bc)
        trozos += 1
        for k in range(0, bloque, 5):
            num += r[j + k] * g[j + desfase + k]
            den += g[j + desfase + k] ** 2
        j += bloque
    gan = num / den if den else 1.0
    sen = ruido = 0.0
    desfase_final = desfase
    print('seguido=%d ' % (j - j0), end='')
    print('empieza=%d tramas=%d trozos=%d saltos=%d peor_correlacion=%.6f desfase_final=%d ganancia=%.3f' %
          (mejor - j0, len(r), trozos, saltos, peor, desfase_final - (mejor - j0), gan))


main()
