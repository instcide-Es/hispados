#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hazsdhci.py DIRECCIÓN SALIDA.aml - (SDHCI) una SSDT de prueba con una
# controladora SD de «modo ACPI» de mentira, en AML escrito a mano (no hay
# iasl en el equipo): lo que miran acpi/pentium/sdhci.c (ACPI.BIN) y
# castel/pruebas_sdhci.sh con QEMU (-acpitable). La dirección es la de la
# sdhci-pci de QEMU (su registro base 0, el que pone SeaBIOS): así el núcleo,
# con «sdhcisinpci», la usa como si solo la dijera ACPI, igual que en una
# tableta Bay Trail en modo ACPI. En ASL:
#
#   Scope (\_SB) {
#     Device (SDHB) {                         // como la microSD de un Bay Trail
#       Name (_HID, "80860F16")
#       Name (_CID, "PNP0D40")
#       Name (_UID, One)
#       Name (BADR, DIRECCIÓN)                // (en las de verdad, de la GNVS)
#       Name (PSOK, Zero)
#       Name (RBUF, ResourceTemplate () {
#         Memory32Fixed (ReadWrite, 0x00000000, 0x00001000)
#         Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive) {46}
#       })
#       Method (_CRS) {
#         CreateDWordField (RBUF, 4, B0BA)
#         Store (BADR, B0BA)
#         Return (RBUF)
#       }
#       Method (_STA) { Return (0x0F) }
#       Method (_PS0) { Store (One, PSOK) }
#     }
#     Device (SDHC) {                         // la SDIO del Wi-Fi: no se toca
#       Name (_HID, "INT33BB")
#       Name (_UID, 2)
#       Name (_CRS, ResourceTemplate () { Memory32Fixed (ReadWrite, 0xFEB00000, 0x00001000) })
#     }
#   }
import struct
import sys


def pkg(cuerpo):
    n = len(cuerpo)
    if n + 1 <= 63:
        return bytes([n + 1]) + cuerpo
    if n + 2 < 1 << 12:
        t = n + 2
        return bytes([0x40 | (t & 0xF), t >> 4]) + cuerpo
    t = n + 3
    return bytes([0x80 | (t & 0xF), (t >> 4) & 0xFF, t >> 12]) + cuerpo


def seg(n):
    return n.ljust(4, '_').encode()


def nombre(n, v):
    return b'\x08' + seg(n) + v


def cadena(s):
    return b'\x0D' + s.encode() + b'\0'


def dword(v):
    return b'\x0C' + struct.pack('<I', v)


def byte_(v):
    return b'\x0A' + bytes([v])


def buffer(datos):
    return b'\x11' + pkg(byte_(len(datos)) + datos)


def metodo(n, cuerpo, args=0):
    return b'\x14' + pkg(seg(n) + bytes([args]) + cuerpo)


def dispositivo(n, cuerpo):
    return b'\x5B\x82' + pkg(seg(n) + cuerpo)


def plantilla(*descr):
    return buffer(b''.join(descr) + b'\x79\x00')


def mem32fija(base, largo):
    return b'\x86\x09\x00\x01' + struct.pack('<II', base, largo)


def interrupcion(n):
    return b'\x89\x06\x00\x05\x01' + struct.pack('<I', n)


def main():
    base = int(sys.argv[1], 16)
    sdhb = dispositivo('SDHB',
                       nombre('_HID', cadena('80860F16')) + nombre('_CID', cadena('PNP0D40')) + nombre('_UID', b'\x01')
                       + nombre('BADR', dword(base)) + nombre('PSOK', b'\x00')
                       + nombre('RBUF', plantilla(mem32fija(0, 0x1000), interrupcion(46)))
                       + metodo('_CRS', b'\x8A' + seg('RBUF') + byte_(4) + seg('B0BA')
                                + b'\x70' + seg('BADR') + seg('B0BA') + b'\xA4' + seg('RBUF'))
                       + metodo('_STA', b'\xA4' + byte_(0x0F))
                       + metodo('_PS0', b'\x70\x01' + seg('PSOK')))
    sdhc = dispositivo('SDHC',
                       nombre('_HID', cadena('INT33BB')) + nombre('_UID', byte_(2))
                       + nombre('_CRS', plantilla(mem32fija(0xFEB00000, 0x1000))))
    cuerpo = b'\x10' + pkg(b'\x5C' + seg('_SB') + sdhb + sdhc)
    cab = b'SSDT' + struct.pack('<I', 36 + len(cuerpo)) + bytes([2, 0]) + b'HISPA ' + b'SDHCIPRU' + struct.pack('<I', 1) \
        + b'INTL' + struct.pack('<I', 0x20260101)
    t = bytearray(cab + cuerpo)
    t[9] = (-sum(t)) & 0xFF
    open(sys.argv[2], 'wb').write(bytes(t))


main()
