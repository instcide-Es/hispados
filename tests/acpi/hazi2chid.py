#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hazi2chid.py - (I2CHID) hace tests/acpi/i2chid-qemu.aml, una SSDT de prueba
# con dos touchpads HID sobre I2C de mentira, en AML escrito a mano (no hay
# iasl en el equipo): lo que miran acpi/pentium/i2chid.c (ACPI.BIN) y
# castel/pruebas_i2chid.sh (con acpi_sim y con QEMU, -acpitable). En ASL:
#
#   Scope (\_SB.PCI0) {
#     Device (I2C1) {
#       Name (_ADR, 0x00150001)
#       Name (SSCN, Package () {528, 640, 30})
#       Name (FMCN, Package () {128, 160, 30})
#       Device (TPD0) {                       // como un ELAN de ASUS
#         Name (_HID, "ELAN1200")
#         Name (_CID, "PNP0C50")
#         Method (_STA) { Return (0x0F) }
#         Name (_CRS, ResourceTemplate () {
#           I2cSerialBusV2 (0x15, ControllerInitiated, 400000, AddressingMode7Bit, "\_SB.PCI0.I2C1")
#           GpioInt (Level, ActiveLow, Exclusive, PullDefault, 0, "\_SB.GPI0") {55}
#         })
#         Method (_DSM, 4) {
#           If (LEqual (Arg0, ToUUID ("3CDFF6F7-4267-4555-AD05-B30A3D8938DE"))) {
#             If (LEqual (Arg2, Zero)) { Return (Buffer () {0x03}) }
#             If (LEqual (Arg2, One)) { Return (One) }
#           }
#           Return (Buffer () {0})
#         }
#       }
#       Device (TPD1) {                       // como las DSDT de Intel para quien no es Windows 8
#         Name (_HID, "SYNA2393")
#         Name (_CID, "PNP0C50")
#         Method (_STA) { Return (Zero) }
#         Name (SBFB, ResourceTemplate () {
#           I2cSerialBusV2 (0x2C, ControllerInitiated, 100000, AddressingMode7Bit, "\_SB.PCI0.I2C1") })
#         Name (SBFI, ResourceTemplate () { Interrupt (ResourceConsumer, Level, ActiveLow) {16} })
#         Method (_CRS) { Return (SBFI) }
#         Method (_DSM, 4) {
#           If (LEqual (Arg2, One)) { Return (0x20) }
#           Return (Buffer () {0})
#         }
#       }
#     }
#   }
import struct
import sys
import uuid


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


def entero(v):
    if v == 0:
        return b'\x00'
    if v == 1:
        return b'\x01'
    if v < 256:
        return b'\x0A' + bytes([v])
    if v < 65536:
        return b'\x0B' + struct.pack('<H', v)
    return b'\x0C' + struct.pack('<I', v)


def buffer(datos):
    return b'\x11' + pkg(entero(len(datos)) + datos)


def paquete(elems):
    return b'\x12' + pkg(bytes([len(elems)]) + b''.join(elems))


def metodo(n, args, cuerpo):
    return b'\x14' + pkg(seg(n) + bytes([args]) + cuerpo)


def dispositivo(n, cuerpo):
    return b'\x5B\x82' + pkg(seg(n) + cuerpo)


def si(pred, cuerpo):
    return b'\xA0' + pkg(pred + cuerpo)


def devuelve(t):
    return b'\xA4' + t


def igual(a, b):
    return b'\x93' + a + b


ARG0, ARG2 = b'\x68', b'\x6A'


def i2c(dirr, vel, fuente):
    f = fuente.encode() + b'\0'
    d = bytes([2, 0, 1, 0x02]) + struct.pack('<HBH', 0, 1, 6) + struct.pack('<IH', vel, dirr) + f
    return b'\x8E' + struct.pack('<H', len(d)) + d


def gpioint(pin, fuente):
    f = fuente.encode() + b'\0'
    pines = struct.pack('<H', pin)
    off_p = 23
    off_n = off_p + len(pines)
    off_v = off_n + len(f)
    d = bytes([1, 0]) + struct.pack('<HHBHH', 1, 0x02, 0, 0, 0) + struct.pack('<HBHHH', off_p, 0, off_n, off_v, 0) + pines + f
    return b'\x8C' + struct.pack('<H', len(d)) + d


def interrupcion(irq):
    d = bytes([0x01 | 0x04, 1]) + struct.pack('<I', irq)
    return b'\x89' + struct.pack('<H', len(d)) + d


def plantilla(*descs):
    return b''.join(descs) + b'\x79\x00'


GUID = uuid.UUID('3CDFF6F7-4267-4555-AD05-B30A3D8938DE').bytes_le

tpd0 = dispositivo('TPD0',
                   nombre('_HID', cadena('ELAN1200')) +
                   nombre('_CID', cadena('PNP0C50')) +
                   metodo('_STA', 0, devuelve(entero(0x0F))) +
                   nombre('_CRS', buffer(plantilla(i2c(0x15, 400000, '\\_SB.PCI0.I2C1'), gpioint(55, '\\_SB.GPI0')))) +
                   metodo('_DSM', 4,
                          si(igual(ARG0, buffer(GUID)),
                             si(igual(ARG2, entero(0)), devuelve(buffer(b'\x03'))) +
                             si(igual(ARG2, entero(1)), devuelve(entero(1)))) +
                          devuelve(buffer(b'\x00'))))
tpd1 = dispositivo('TPD1',
                   nombre('_HID', cadena('SYNA2393')) +
                   nombre('_CID', cadena('PNP0C50')) +
                   metodo('_STA', 0, devuelve(entero(0))) +
                   nombre('SBFB', buffer(plantilla(i2c(0x2C, 100000, '\\_SB.PCI0.I2C1')))) +
                   nombre('SBFI', buffer(plantilla(interrupcion(16)))) +
                   metodo('_CRS', 0, devuelve(seg('SBFI'))) +
                   metodo('_DSM', 4, si(igual(ARG2, entero(1)), devuelve(entero(0x20))) + devuelve(buffer(b'\x00'))))
i2c1 = dispositivo('I2C1',
                   nombre('_ADR', entero(0x00150001)) +
                   nombre('SSCN', paquete([entero(528), entero(640), entero(30)])) +
                   nombre('FMCN', paquete([entero(128), entero(160), entero(30)])) +
                   tpd0 + tpd1)
cuerpo = b'\x10' + pkg(b'\x5C\x2E' + seg('_SB') + seg('PCI0') + i2c1)

cab = bytearray(b'SSDT' + struct.pack('<I', 36 + len(cuerpo)) + bytes([2, 0]) + b'HISPA ' + b'I2CHIDPR' +
                struct.pack('<I', 1) + b'HSPA' + struct.pack('<I', 1))
t = cab + cuerpo
t[9] = (-sum(t)) & 0xFF
open(sys.argv[1] if len(sys.argv) > 1 else 'i2chid-qemu.aml', 'wb').write(bytes(t))
