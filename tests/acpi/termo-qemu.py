#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# tests/acpi/termo-qemu.py SALIDA.aml
# (TERMO) La SSDT de las pruebas del Termómetro (castel/pruebas_termo.sh), para
# QEMU con -acpitable: una zona térmica, \_TZ.TZ01, cuyo _TMP lee la CMOS
# (el registro 63h: los grados; el monitor de QEMU lo cambia con
# «o /b 0x70 0x63» y «o /b 0x71 N»), con _CRT 100 °C y _PSV 90 °C; y un
# ventilador, \_SB.FAN0 (PNP0C0B), con _FST {0, 45 %, 2400 r.p.m.}.
# El AML, a mano (sin iasl).
import struct, sys

def pkglen(cuerpo):
    n = len(cuerpo) + 1
    if n < 64:
        return bytes([n]) + cuerpo
    n += 1
    assert n < 4096
    return bytes([0x40 | (n & 15), n >> 4]) + cuerpo

def word(v): return b'\x0b' + struct.pack('<H', v)
def byte(v): return b'\x0a' + bytes([v])

campos = b'\x5b\x80TCMS\x01' + byte(0x70) + byte(2)                       # OperationRegion (TCMS, SystemIO, 0x70, 2)
campos += b'\x5b\x81' + pkglen(b'TCMS\x01' + b'TIDX\x08' + b'TDAT\x08')     # Field (TCMS, ByteAcc) {TIDX, 8, TDAT, 8}
tmp = b'\x14' + pkglen(b'_TMP\x08' +                                        # Method (_TMP, 0, Serialized)
                       b'\x70' + byte(0x63) + b'TIDX' +                     #   Store (0x63, TIDX)
                       b'\xa4\x72\x77TDAT' + byte(10) + b'\x00' + word(2732) + b'\x00')   # Return ((TDAT * 10) + 2732)
zona = b'\x5b\x85' + pkglen(b'TZ01' + tmp + b'\x08_CRT' + word(3732) + b'\x08_PSV' + word(3632))
tz = b'\x10' + pkglen(b'\\_TZ_' + campos + zona)                             # Scope (\_TZ)
fan = b'\x5b\x82' + pkglen(b'FAN0' + b'\x08_HID\x0c' + bytes([0x41, 0xD0, 0x0C, 0x0B]) +
                          b'\x08_FST\x12' + pkglen(b'\x03\x00' + byte(45) + word(2400)))
sb = b'\x10' + pkglen(b'\\_SB_' + fan)
cuerpo = tz + sb
cab = bytearray(b'SSDT' + struct.pack('<I', 36 + len(cuerpo)) + b'\x02\x00' + b'HISPA ' + b'TERMPRUE' +
                struct.pack('<I', 1) + b'INTL' + struct.pack('<I', 0x20200925))
t = cab + cuerpo
t[9] = (-sum(t)) & 0xFF
open(sys.argv[1], 'wb').write(bytes(t))
