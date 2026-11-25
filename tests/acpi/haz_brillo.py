#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_brillo.py SALIDA - hace brillo-qemu.aml (lo de brillo-qemu.asl) a mano,
# sin iasl: el AML de cada cosa, con sus longitudes de paquete (PkgLength).
import sys

def pkglen(n):
    # la longitud cuenta sus propios octetos
    if n + 1 < 0x40:
        return bytes([n + 1])
    t = n + 2
    if t < 0x1000:
        return bytes([0x40 | (t & 0xF), t >> 4])
    t = n + 3
    return bytes([0x80 | (t & 0xF), (t >> 4) & 0xFF, t >> 12])

def paquete(op, cuerpo):
    return op + pkglen(len(cuerpo)) + cuerpo

def entero(v):
    if v == 0:
        return b'\x00'
    if v == 1:
        return b'\x01'
    if v < 256:
        return bytes([0x0A, v])
    return bytes([0x0B, v & 0xFF, v >> 8])

niveles = [100, 40, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100]
bcl = paquete(b'\x14', b'_BCL' + b'\x00' + b'\xA4' + paquete(b'\x12', bytes([len(niveles)]) + b''.join(entero(v) for v in niveles)))
bcm = paquete(b'\x14', b'_BCM' + b'\x01' + b'\x70\x68' + b'BRIL')
bqc = paquete(b'\x14', b'_BQC' + b'\x00' + b'\xA4' + b'BRIL')
disp = paquete(b'\x5B\x82', b'HLCD' + b'\x08_ADR' + entero(0x400) + b'\x08BRIL' + entero(0x32) + bcl + bcm + bqc)
cuerpo = paquete(b'\x10', b'\\_SB_' + disp)
largo = 36 + len(cuerpo)
cab = bytearray(b'SSDT' + largo.to_bytes(4, 'little') + b'\x01\x00' + b'HISPA ' + b'BRILLO  ' + (1).to_bytes(4, 'little') + b'HISP' + (1).to_bytes(4, 'little'))
t = cab + cuerpo
t[9] = (-sum(t)) & 0xFF
open(sys.argv[1], 'wb').write(bytes(t))
