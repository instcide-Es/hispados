#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# haz_tapa.py SALIDA - [Hispa-64, etapa 11 a] hace tapa-qemu.aml (lo de
# tapa-qemu.asl) a mano, sin iasl, como haz_brillo.py: la tapa de un portátil
# de mentira (PNP0C0D) para QEMU, cuyo _LID lee la CMOS 63h (0: abierta;
# otro: cerrada), y tres métodos que hacen lo que haría el firmware de un
# portátil al cerrar la tapa o al pulsar Fn y las teclas del brillo:
#   \_SB_.HTAV   Notify (\_SB_.HTAP, 0x80)   la tapa ha cambiado
#   \_SB_.HBSU   Notify (\_SB_.HLCD, 0x86)   Fn + subir el brillo
#   \_SB_.HBBA   Notify (\_SB_.HLCD, 0x87)   Fn + bajar el brillo
# (HLCD es el de brillo-qemu.aml.)
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


def bits(n):
    # la longitud de un campo (en bits): el número tal cual, en el formato de PkgLength
    if n < 0x40:
        return bytes([n])
    return bytes([0x40 | (n & 0xF), n >> 4])


def paquete(op, cuerpo):
    return op + pkglen(len(cuerpo)) + cuerpo


def metodo(nombre, cuerpo):
    return paquete(b'\x14', nombre + b'\x00' + cuerpo)


SB = b'\\\x2E_SB_'
region = b'\x5B\x80' + b'TCMS' + b'\x01' + b'\x0A\x70' + b'\x0A\x02'
campo = paquete(b'\x5B\x81', b'TCMS' + b'\x01' + b'TIDX' + bits(8) + b'TDAT' + bits(8))
indice = paquete(b'\x5B\x86', b'TIDX' + b'TDAT' + b'\x01' + b'\x00' + bits(0x63 * 8) + b'TAPA' + bits(8))
lid = metodo(b'_LID', b'\xA4' + b'\x93' + b'TAPA' + b'\x00')        # Return (LEqual (TAPA, Zero))
tapa = paquete(b'\x5B\x82', b'HTAP' + b'\x08_HID' + b'\x0C\x41\xD0\x0C\x0D' + lid)   # EisaId ("PNP0C0D")
avisa = metodo(b'HTAV', b'\x86' + SB + b'HTAP' + b'\x0A\x80')
sube = metodo(b'HBSU', b'\x86' + SB + b'HLCD' + b'\x0A\x86')
baja = metodo(b'HBBA', b'\x86' + SB + b'HLCD' + b'\x0A\x87')
cuerpo = paquete(b'\x10', b'\\_SB_' + region + campo + indice + tapa + avisa + sube + baja)
largo = 36 + len(cuerpo)
cab = bytearray(b'SSDT' + largo.to_bytes(4, 'little') + b'\x01\x00' + b'HISPA ' + b'TAPA    ' + (1).to_bytes(4, 'little') + b'HISP' + (1).to_bytes(4, 'little'))
t = cab + cuerpo
t[9] = (-sum(t)) & 0xFF
open(sys.argv[1], 'wb').write(bytes(t))
