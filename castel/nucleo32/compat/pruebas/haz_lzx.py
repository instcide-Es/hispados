#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""compat/pruebas/haz_lzx.py CARPETA

(COMPAT) Un CAB en LZX hecho aquí, para probar el descompresor del
Asistente (compat/lzx.c) sin ningún archivo de Microsoft: los
redistribuibles de verdad van en LZX:21 y con ellos se comprobó (fuera de
las pruebas) que lo sacado da la suma PE buena; aquí, un compresor LZX
sencillo y propio, con lo que tiene el formato: bloques literales (1),
alineados (2) y sin comprimir (3, uno de largo impar), un bloque que pasa de
un trozo de 32 K al siguiente, coincidencias cortas y de hasta 257, las tres
repetidas (R0, R1, R2), el pre-árbol con sus códigos 17, 18 y 19, los largos
como diferencia con los del bloque de antes, y la traducción E8 de Intel.
Deja en CARPETA PRUEBA.CAB, los archivos tal cual (para compararlos) y
nombres.txt.
"""
import heapq
import os
import random
import struct
import sys

VENT = 16
RANURAS = 32
N_PRINCIPAL = 256 + RANURAS * 8
TROZO = 32768
E8_TAM = 12000000

EXTRA = [0, 0, 0, 0] + [i // 2 for i in range(2, 34)] + [17] * 14
EXTRA = EXTRA[:51]
BASE = [0]
for k in range(1, 51):
    BASE.append(BASE[-1] + (1 << EXTRA[k - 1]))


class Bits:
    def __init__(self):
        self.palabras = bytearray()
        self.acc = 0
        self.n = 0

    def pon(self, v, n):
        for i in range(n - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1)
            self.n += 1
            if self.n == 16:
                self.palabras += struct.pack('<H', self.acc)
                self.acc = 0
                self.n = 0

    def alinea(self):
        if self.n:
            self.pon(0, 16 - self.n)

    def octetos(self, b):
        assert self.n == 0
        self.palabras += b


def largos(frec, maximo):
    """Los largos de Huffman (hasta «maximo» bits)."""
    f = list(frec)
    while True:
        hay = [(c, i) for i, c in enumerate(f) if c]
        if len(hay) < 2:
            lens = [0] * len(f)
            for _, i in hay:
                lens[i] = 1
            if len(hay) == 1:
                otro = 0 if hay[0][1] else 1
                lens[otro] = 1
            return lens
        h = [(c, k, [i]) for k, (c, i) in enumerate(hay)]
        heapq.heapify(h)
        lens = [0] * len(f)
        k = len(h)
        while len(h) > 1:
            a = heapq.heappop(h)
            b = heapq.heappop(h)
            for i in a[2] + b[2]:
                lens[i] += 1
            heapq.heappush(h, (a[0] + b[0], k, a[2] + b[2]))
            k += 1
        if max(lens) <= maximo:
            return lens
        f = [(c + 1) // 2 if c else 0 for c in f]


def codigos(lens):
    """Los códigos canónicos (por largo y, en cada largo, por símbolo)."""
    cod = [0] * len(lens)
    c = 0
    for l in range(1, 17):
        for s in range(len(lens)):
            if lens[s] == l:
                cod[s] = c
                c += 1
        c <<= 1
    return cod


def pon_largos(b, nuevos, viejos, de, a):
    """Los largos de [de, a) con el pre-árbol (como diferencia con los de antes)."""
    sims = []
    x = de
    while x < a:
        n = nuevos[x]
        if n == 0:
            y = x
            while y < a and nuevos[y] == 0 and y - x < 51:
                y += 1
            r = y - x
            if r >= 20:
                sims.append((18, r - 20, 5))
                x = y
                continue
            if r >= 4:
                sims.append((17, r - 4, 4))
                x = y
                continue
        y = x
        while y < a and nuevos[y] == n and y - x < 5:
            y += 1
        r = y - x
        z = (viejos[x] - n) % 17
        if r >= 4:
            sims.append((19, r - 4, 1, z))
            x = y
            continue
        sims.append((z,))
        x += 1
    fr = [0] * 20
    for s in sims:
        fr[s[0]] += 1
        if s[0] == 19:
            fr[s[3]] += 1
    pl = largos(fr, 15)
    pc = codigos(pl)
    for l in pl:
        b.pon(l, 4)
    for s in sims:
        b.pon(pc[s[0]], pl[s[0]])
        if s[0] in (17, 18):
            b.pon(s[1], s[2])
        elif s[0] == 19:
            b.pon(s[1], 1)
            b.pon(pc[s[3]], pl[s[3]])
    for x in range(de, a):
        viejos[x] = nuevos[x]


def e8(datos, pos0):
    """La traducción E8 al comprimir (la inversa de la del descompresor), trozo a trozo."""
    d = bytearray(datos)
    for t in range(0, len(d), TROZO):
        n = min(TROZO, len(d) - t)
        if n <= 6:
            continue
        i = 0
        cur = pos0 + t
        while i < n - 10:
            if d[t + i] != 0xE8:
                i += 1
                cur += 1
                continue
            i += 1
            r = struct.unpack_from('<i', d, t + i)[0]
            if -cur <= r < E8_TAM:
                a = r + cur if r < E8_TAM - cur else r - E8_TAM
                struct.pack_into('<i', d, t + i, a)
            i += 4
            cur += 5
    return bytes(d)


def fichas(d, de, a, reps):
    """LZ77 ávido dentro de [de, a), sin pasar de un trozo de 32 K a otro."""
    out = []
    tabla = {}
    i = de
    while i < a:
        fin_trozo = min(a, (i // TROZO + 1) * TROZO)
        mejor = (0, 0)
        if i + 2 < fin_trozo:
            # primero las repetidas (R0, R1, R2)
            for off in reps:
                if off <= i:
                    l = 0
                    while i + l < fin_trozo and l < 257 and d[i + l] == d[i + l - off]:
                        l += 1
                    if l >= 3 and l > mejor[0]:
                        mejor = (l, off)
            clave = d[i:i + 3]
            for p in reversed(tabla.get(clave, [])[-16:]):
                off = i - p
                if off > (1 << VENT) - 3:
                    continue
                l = 0
                while i + l < fin_trozo and l < 257 and d[i + l] == d[p + l]:
                    l += 1
                if l > mejor[0]:
                    mejor = (l, off)
        if mejor[0] >= 3:
            l, off = mejor
            out.append(('m', l, off))
            for k in range(l):
                if i + k + 3 <= len(d):
                    tabla.setdefault(d[i + k:i + k + 3], []).append(i + k)
            i += l
        else:
            out.append(('l', d[i]))
            if i + 3 <= len(d):
                tabla.setdefault(d[i:i + 3], []).append(i)
            i += 1
    return out


def comprime(datos, bloques):
    d = e8(datos, 0)
    b = Bits()
    trozos = []
    hecho_bits = 0
    b.pon(1, 1)
    b.pon(E8_TAM >> 16, 16)
    b.pon(E8_TAM & 0xFFFF, 16)
    viejos_p = [0] * N_PRINCIPAL
    viejos_l = [0] * 249
    reps = [1, 1, 1]
    pos = 0

    def cierra_trozo():
        b.alinea()
        trozos.append(bytes(b.palabras))
        b.palabras = bytearray()

    for clase, a in bloques:
        de = pos
        b.pon(clase, 3)
        largo = a - de
        b.pon(largo >> 8, 16)
        b.pon(largo & 0xFF, 8)
        if clase == 3:
            if b.n == 0:
                b.pon(0, 16)
            b.alinea()
            b.octetos(struct.pack('<III', *reps))
            for k in range(de, a):
                b.octetos(d[k:k + 1])
                if (k + 1) % TROZO == 0:
                    cierra_trozo()
            if largo & 1:
                b.octetos(b'\0')
            pos = a
            continue
        fs = fichas(d, de, a, reps)
        # las fichas, con las repetidas como el descompresor
        sims = []
        r = list(reps)
        for f in fs:
            if f[0] == 'l':
                sims.append(('l', f[1]))
                continue
            _, l, off = f
            if off == r[0]:
                ran = 0
            elif off == r[1]:
                ran = 1
                r[1] = r[0]
                r[0] = off
            elif off == r[2]:
                ran = 2
                r[2] = r[0]
                r[0] = off
            else:
                fo = off + 2
                ran = max(k for k in range(3, RANURAS) if BASE[k] <= fo)
                r[2] = r[1]
                r[1] = r[0]
                r[0] = off
            ex = 0 if ran < 3 else off + 2 - BASE[ran]
            sims.append(('m', l, ran, ex))
        fp = [0] * N_PRINCIPAL
        fl = [0] * 249
        fa = [0] * 8
        fp[0xE8] += 1          # (que la E8 tenga código: el descompresor empieza a traducir)
        for s in sims:
            if s[0] == 'l':
                fp[s[1]] += 1
            else:
                _, l, ran, ex = s
                cab = min(l - 2, 7)
                fp[256 + ran * 8 + cab] += 1
                if l - 2 >= 7:
                    fl[l - 2 - 7] += 1
                if clase == 2 and ran >= 3 and EXTRA[ran] >= 3:
                    fa[ex & 7] += 1
        lp = largos(fp, 16)
        ll = largos(fl, 16) if any(fl) else [0] * 249
        if clase == 2:
            la = largos([c + 1 for c in fa], 7)
            for x in la:
                b.pon(x, 3)
            ca = codigos(la)
        pon_largos(b, lp, viejos_p, 0, 256)
        pon_largos(b, lp, viejos_p, 256, N_PRINCIPAL)
        pon_largos(b, ll, viejos_l, 0, 249)
        cp = codigos(lp)
        cl = codigos(ll)
        p = de
        for s in sims:
            if s[0] == 'l':
                b.pon(cp[s[1]], lp[s[1]])
                p += 1
            else:
                _, l, ran, ex = s
                cab = min(l - 2, 7)
                sp = 256 + ran * 8 + cab
                b.pon(cp[sp], lp[sp])
                if l - 2 >= 7:
                    b.pon(cl[l - 9], ll[l - 9])
                if ran >= 3:
                    e = EXTRA[ran]
                    if clase == 2 and e >= 3:
                        if e > 3:
                            b.pon(ex >> 3, e - 3)
                        b.pon(ca[ex & 7], la[ex & 7])
                    elif e:
                        b.pon(ex, e)
                p += l
            if p % TROZO == 0 and p < len(d):
                cierra_trozo()
        reps = r
        pos = a
    if b.n or b.palabras:
        cierra_trozo()
    return trozos


def cab(archivos, trozos, total):
    nombres = b''.join(n.encode() + b'\0' for n, _ in archivos)
    coff_archivos = 36 + 8
    coff_datos = coff_archivos + sum(16 + len(n) + 1 for n, _ in archivos)
    datos = b''
    for k, t in enumerate(trozos):
        n = min(TROZO, total - k * TROZO)
        datos += struct.pack('<IHH', 0, len(t), n) + t
    h = struct.pack('<4sIIIIIBBHHHHH', b'MSCF', 0, coff_datos + len(datos), 0, coff_archivos, 0, 3, 1, 1, len(archivos), 0, 0x4321, 0)
    f = struct.pack('<IHH', coff_datos, len(trozos), 3 | (VENT << 8))
    fs = b''
    desde = 0
    for n, d in archivos:
        fs += struct.pack('<IIHHHH', len(d), desde, 0, 0x5B76, 0x6000, 0x20) + n.encode() + b'\0'
        desde += len(d)
    return h + f + fs + datos


def main():
    c = sys.argv[1]
    os.makedirs(c, exist_ok=True)
    rnd = random.Random(49)
    texto = ''.join('Linea %d del juego de mentira: el Asistente de compatibilidad de Hispa-Pentium.\r\n' % (i % 37) for i in range(900)).encode()
    binario = bytearray()
    for i in range(1700):
        binario += b'\x55\x8B\xEC\xE8' + struct.pack('<i', rnd.randrange(-50000, 50000)) + b'\x5D\xC3' + bytes(rnd.randrange(4) for _ in range(rnd.randrange(30)))
    binario += b'Z' * 3000 + bytes(range(256)) * 20
    al_azar = bytes(rnd.randrange(256) for _ in range(31001))
    archivos = [('TEXTO.TXT', texto), ('CODIGO.DLL', bytes(binario)), ('AZAR.BIN', al_azar)]
    todo = b''.join(d for _, d in archivos)
    n = len(todo)
    # los bloques: uno literal que pasa de un trozo al siguiente, uno sin comprimir de largo impar, uno alineado y otro sin comprimir al final
    bloques = [(1, 50000), (3, 51001), (2, min(n, 100000)), (1, min(n, 106000)), (3, n)]
    trozos = comprime(todo, bloques)
    with open(os.path.join(c, 'PRUEBA.CAB'), 'wb') as f:
        f.write(cab(archivos, trozos, n))
    for nom, d in archivos:
        with open(os.path.join(c, nom), 'wb') as f:
            f.write(d)
    with open(os.path.join(c, 'nombres.txt'), 'w') as f:
        f.write(' '.join(nom for nom, _ in archivos) + '\n')
    print('haz_lzx: %d octetos en %d trozos de LZX' % (n, len(trozos)))


if __name__ == '__main__':
    main()
