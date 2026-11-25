#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hazsdhci.py DIRECCIÓN SALIDA.aml [sdio] - (SDHCI) una SSDT de prueba con una
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
#     Device (SDHC) {                         // la SDIO del Wi-Fi
#       Name (_HID, "INT33BB")
#       Name (_UID, 2)
#       Name (_CRS, ResourceTemplate () { Memory32Fixed (ReadWrite, 0xFEB00000, 0x00001000) })
#     }
#   }
# (RTL8723BS) Con «sdio», además, como en una tableta Bay Trail: la
# controladora GPIO SUS (INT33FC _UID 3) con la patilla 26 (WL_EN) en una
# OperationRegion GeneralPurposeIo, el _PS0 de la SDIO que la baja, espera
# 20 ms y la sube, y su hijo, el chip (OBDA8723), con otro _PS0 que la sube:
#     Device (GPO2) {
#       Name (_HID, "INT33FC") Name (_UID, 3)
#       Name (_CRS, ResourceTemplate () { Memory32Fixed (ReadWrite, 0xFED0E000, 0x1000) })
#       OperationRegion (GPOP, GeneralPurposeIo, 0, 1)
#       Field (GPOP, ByteAcc, NoLock, Preserve) {
#         Connection (GpioIo (Exclusive, PullDefault, 0, 0, IoRestrictionOutputOnly, "\\_SB.GPO2") {26}),
#         WFD3, 1 }
#     }
#     (en SDHC) Name (_CRS, ... Memory32Fixed (0xFEB00000, 0x1000), Memory32Fixed (0xFEB01000, 0x1000))
#               Method (_PS0) { Store (Zero, \_SB.GPO2.WFD3)  Sleep (20)  Store (One, \_SB.GPO2.WFD3) }
#               Device (RTLW) { Name (_HID, "OBDA8723") Name (_ADR, One) Method (_PS0) { Store (One, \_SB.GPO2.WFD3) } }
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


def gpioio(pin, fuente):
    # el descriptor GpioIo (8Ch): la tabla de patillas en 23, la fuente detrás
    f = fuente.encode() + b'\0'
    largo = 23 + 2 + len(f)
    d = bytes([0x8C]) + struct.pack('<H', largo - 3) + bytes([1, 1]) + struct.pack('<HH', 1, 2) + bytes([0]) \
        + struct.pack('<HHH', 0, 0, 23) + bytes([0]) + struct.pack('<HHH', 25, largo, 0) + struct.pack('<H', pin) + f
    return d


def i2c(direccion, hz, fuente):
    # (0.8.12, «fichaequipo») el descriptor I2cSerialBus (8Eh): consumidor, 7 bits
    f = fuente.encode() + b'\0'
    cuerpo = bytes([1, 0, 1, 0x02]) + struct.pack('<H', 0) + bytes([1]) + struct.pack('<H', 6) \
        + struct.pack('<IH', hz, direccion) + f
    return bytes([0x8E]) + struct.pack('<H', len(cuerpo)) + cuerpo


def ruta_wfd3():
    return b'\x5C\x2F\x03' + seg('_SB') + seg('GPO2') + seg('WFD3')


def main():
    base = int(sys.argv[1], 16)
    con_sdio = len(sys.argv) > 3 and sys.argv[3] == 'sdio'
    sdhb = dispositivo('SDHB',
                       nombre('_HID', cadena('80860F16')) + nombre('_CID', cadena('PNP0D40')) + nombre('_UID', b'\x01')
                       + nombre('BADR', dword(base)) + nombre('PSOK', b'\x00')
                       + nombre('RBUF', plantilla(mem32fija(0, 0x1000), interrupcion(46)))
                       + metodo('_CRS', b'\x8A' + seg('RBUF') + byte_(4) + seg('B0BA')
                                + b'\x70' + seg('BADR') + seg('B0BA') + b'\xA4' + seg('RBUF'))
                       + metodo('_STA', b'\xA4' + byte_(0x0F))
                       + metodo('_PS0', b'\x70\x01' + seg('PSOK')))
    if not con_sdio:
        sdhc = dispositivo('SDHC',
                           nombre('_HID', cadena('INT33BB')) + nombre('_UID', byte_(2))
                           + nombre('_CRS', plantilla(mem32fija(0xFEB00000, 0x1000))))
        gpo2 = b''
    else:
        campo = b'\x5B\x81' + pkg(seg('GPOP') + b'\x01' + b'\x02' + plantilla(gpioio(26, '\\_SB.GPO2')) + seg('WFD3') + b'\x01')
        gpo2 = dispositivo('GPO2',
                           nombre('_HID', cadena('INT33FC')) + nombre('_UID', byte_(3))
                           + nombre('_CRS', plantilla(mem32fija(0xFED0E000, 0x1000)))
                           + b'\x5B\x80' + seg('GPOP') + b'\x08' + b'\x00' + b'\x01'
                           + campo)
        rtlw = dispositivo('RTLW', nombre('_HID', cadena('OBDA8723')) + nombre('_ADR', b'\x01')
                           + metodo('_PS0', b'\x70\x01' + ruta_wfd3()))
        sdhc = dispositivo('SDHC',
                           nombre('_HID', cadena('INT33BB')) + nombre('_UID', byte_(2))
                           + nombre('_CRS', plantilla(mem32fija(0xFEB00000, 0x1000), mem32fija(0xFEB01000, 0x1000)))
                           + metodo('_PS0', b'\x70\x00' + ruta_wfd3() + b'\x5B\x22' + byte_(20) + b'\x70\x01' + ruta_wfd3())
                           + rtlw)
    i2c5 = b''
    if con_sdio:
        # (0.8.12, «fichaequipo») una I2C de Bay Trail con el acelerómetro
        # (BOSC0200) y la pantalla táctil (MSSL1680, _STA 0; sin el _CID PNP0C50, para que no la tome por un
        # touchpad I2C-HID): lo que lista
        # ACPI.BIN en ACPI.TXT para saber qué chips lleva la tableta
        accl = dispositivo('ACCL', nombre('_HID', cadena('BOSC0200')) + nombre('_CRS', plantilla(i2c(0x18, 400000, '\\_SB.I2C5'), interrupcion(70)))
                           + metodo('_STA', b'\xA4' + byte_(0x0F)))
        tcsc = dispositivo('TCSC', nombre('_HID', cadena('MSSL1680'))
                           + nombre('_CRS', plantilla(i2c(0x40, 400000, '\\_SB.I2C5')))
                           + metodo('_STA', b'\xA4' + byte_(0x00)))
        i2c5 = dispositivo('I2C5', nombre('_HID', cadena('80860F41')) + nombre('_UID', byte_(5))
                           + nombre('_CRS', plantilla(mem32fija(0xFEB10000, 0x1000))) + accl + tcsc)
    cuerpo = b'\x10' + pkg(b'\x5C' + seg('_SB') + gpo2 + sdhb + sdhc + i2c5)
    cab = b'SSDT' + struct.pack('<I', 36 + len(cuerpo)) + bytes([2, 0]) + b'HISPA ' + b'SDHCIPRU' + struct.pack('<I', 1) \
        + b'INTL' + struct.pack('<I', 0x20260101)
    t = bytearray(cab + cuerpo)
    t[9] = (-sum(t)) & 0xFF
    open(sys.argv[2], 'wb').write(bytes(t))


main()
