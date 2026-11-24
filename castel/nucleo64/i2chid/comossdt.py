#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# comossdt.py - Hispa-64, etapa 6 (pruebas de I2C-HID): pasa una tabla de
# ACPI de un equipo de verdad (la DSDT del ASUS GL553VD, 189 KB) a SSDT de
# 64 KB como mucho, para dárselas a QEMU con «-acpitable file=...» (no deja
# tablas más grandes) al lado de las suyas (una segunda DSDT no la querría
# nadie: \_S5 y lo demás, de la de QEMU). Se parte por los términos de lo
# alto de la tabla (y, si un Scope no cabe, en varios Scope con el mismo
# nombre): el espacio de nombres que sale es el mismo. Cada trozo, con la
# firma «SSDT» y su suma rehecha.
#   comossdt.py ORIGEN PREFIJO   ->  PREFIJO1.aml, PREFIJO2.aml...
import struct
import sys

MAXIMO = 60000
d = b''


def pkglen(p):
    b = d[p]
    n = b >> 6
    if n == 0:
        return b & 0x3F, n + 1
    v = b & 0x0F
    for i in range(n):
        v |= d[p + 1 + i] << (4 + 8 * i)
    return v, n + 1


def cod_pkglen(n):
    # el cuerpo mide n: la longitud (que se cuenta a sí misma) en 2 o 3 octetos
    for k in (2, 3, 4):
        t = n + k
        if t < 1 << (4 + 8 * (k - 1)):
            out = [((k - 1) << 6) | (t & 0x0F)]
            for i in range(k - 1):
                out.append((t >> (4 + 8 * i)) & 0xFF)
            return bytes(out)
    raise ValueError('demasiado grande')


def nombre(p):
    while d[p] in (0x5C, 0x5E):
        p += 1
    b = d[p]
    if b == 0:
        return p + 1
    if b == 0x2E:
        return p + 9
    if b == 0x2F:
        return p + 2 + 4 * d[p + 1]
    return p + 4


METODOS = {}
OPS1 = {0x70: 2, 0x71: 1, 0x72: 3, 0x73: 3, 0x74: 3, 0x75: 1, 0x76: 1, 0x77: 3, 0x78: 4, 0x79: 3, 0x7A: 3,
        0x7B: 3, 0x7C: 3, 0x7D: 3, 0x7E: 3, 0x7F: 3, 0x80: 2, 0x81: 2, 0x82: 2, 0x83: 1, 0x84: 3, 0x85: 3,
        0x86: 2, 0x87: 1, 0x88: 3, 0x8A: 3, 0x8B: 3, 0x8C: 3, 0x8D: 3, 0x8E: 1, 0x8F: 3, 0x90: 2, 0x91: 2,
        0x92: 1, 0x93: 2, 0x94: 2, 0x95: 2, 0x96: 2, 0x97: 2, 0x98: 2, 0x99: 2, 0x9C: 3, 0x9D: 2, 0x9E: 4,
        0xA4: 1, 0xA5: 0, 0x9F: 0, 0xA3: 0, 0xCC: 0}
EXT = {0x12: 2, 0x13: 4, 0x21: 1, 0x22: 1, 0x24: 1, 0x25: 2, 0x26: 1, 0x27: 1, 0x28: 2, 0x29: 2, 0x30: 0,
       0x31: 0, 0x33: 0, 0x1F: 6, 0x20: 2, 0x2A: 1}


def salta(p):
    b = d[p]
    if b in (0, 1, 0xFF) or 0x60 <= b <= 0x6E:
        return p + 1
    if b in (0x0A, 0x0B, 0x0C, 0x0E):
        return p + {0x0A: 2, 0x0B: 3, 0x0C: 5, 0x0E: 9}[b]
    if b == 0x0D:
        return d.index(0, p + 1) + 1
    if b in (0x11, 0x12, 0x13, 0xA0, 0xA1, 0xA2):
        return p + 1 + pkglen(p + 1)[0]
    if b == 0x89:
        q = salta(p + 1)
        q = salta(q + 1)
        q = salta(q + 1)
        return salta(q)
    if b == 0x5B:
        c = d[p + 1]
        if c == 0x23:
            return salta(p + 2) + 2
        if c == 0x32:
            return salta(p + 7)
        q = p + 2
        for _ in range(EXT[c]):
            q = salta(q)
        return q
    if b in (0x5C, 0x5E, 0x2E, 0x2F, 0x5F) or 0x41 <= b <= 0x5A:
        q = nombre(p)
        ult = d[q - 4:q]
        for _ in range(METODOS.get(ult, 0)):
            q = salta(q)
        return q
    q = p + 1
    for _ in range(OPS1[b]):
        q = salta(q)
    return q


def termino(p):
    """dónde acaba el término de lo alto que empieza en p"""
    b = d[p]
    if b in (0x10, 0x14, 0xA0, 0xA1, 0xA2):
        return p + 1 + pkglen(p + 1)[0]
    if b == 0x5B:
        c = d[p + 1]
        if c in (0x82, 0x83, 0x84, 0x85, 0x81, 0x86, 0x87):
            return p + 2 + pkglen(p + 2)[0]
        if c == 0x80:
            return salta(salta(nombre(p + 2) + 1))
        if c == 0x01:
            return nombre(p + 2) + 1
        if c == 0x02:
            return nombre(p + 2)
        if c == 0x88:
            return salta(salta(salta(nombre(p + 2))))
        return salta(p)
    if b == 0x08:
        return salta(nombre(p + 1))
    if b == 0x06:
        return nombre(nombre(p + 1))
    if b == 0x15:
        return nombre(p + 1) + 2
    return salta(p)


def metodos(ini, fin):
    # los Method de todo (para saltar sus llamadas): una pasada a ojo
    p = ini
    while p < fin:
        if d[p] == 0x14:
            try:
                n, k = pkglen(p + 1)
                q = p + 1 + k
                if p + 1 + n <= fin:
                    e = nombre(q)
                    METODOS.setdefault(d[e - 4:e], d[e] & 7)
            except (IndexError, ValueError):
                pass
        p += 1


def trozos(ini, fin):
    """los términos de [ini, fin), en grupos que caben; un Scope que no cabe, partido"""
    grupos, ahora = [], b''
    p = ini
    while p < fin:
        e = termino(p)
        t = d[p:e]
        if len(t) > MAXIMO and d[p] == 0x10:
            n, k = pkglen(p + 1)
            q = nombre(p + 1 + k)
            nom = d[p + 1 + k:q]
            for g in trozos(q, e):
                t2 = b'\x10' + cod_pkglen(len(nom) + len(g)) + nom + g
                if len(ahora) + len(t2) > MAXIMO and ahora:
                    grupos.append(ahora)
                    ahora = b''
                ahora += t2
        else:
            if len(t) > MAXIMO:
                sys.exit('un término de %d octetos no cabe' % len(t))
            if len(ahora) + len(t) > MAXIMO and ahora:
                grupos.append(ahora)
                ahora = b''
            ahora += t
        p = e
    if ahora:
        grupos.append(ahora)
    return grupos


def main():
    global d
    d = open(sys.argv[1], 'rb').read()
    largo = struct.unpack_from('<I', d, 4)[0]
    metodos(36, largo)
    for i, g in enumerate(trozos(36, largo)):
        t = bytearray(b'SSDT' + struct.pack('<I', 36 + len(g)) + d[8:36]) + g
        t[9] = 0
        t[9] = (-sum(t)) & 0xFF
        open('%s%d.aml' % (sys.argv[2], i + 1), 'wb').write(bytes(t))


main()
