#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# tests/movil_ncm.py hacer|mirar MOVIL.IN [MOVIL.OUT]
# (MÓVIL) La prueba de la parte de paquetes del móvil por USB de
# Hispa-Pentium (castel/nucleo32/rmovil.cas, con UMOVILP.BIN) con datos
# grabados: lo que QEMU no tiene (CDC-NCM: su usb-net solo da RNDIS y
# CDC-ECM) y lo que su RNDIS no hace (varias tramas en una transferencia).
# Los NTB16 son como los arman f_ncm.c de Linux (el NDP16 al final, detrás
# de los datagramas, alineados a 4) y los de otros (el NDP16 delante, dos
# NDP encadenados, la firma «NCM1»); los RNDIS, como los junta un Android
# (con el relleno hasta 8). «hacer» deja MOVIL.IN; «mirar» compara MOVIL.OUT
# con lo esperado y dice cada caso (OK/FAIL); sale con 1 si alguno falla.
import random
import struct
import sys

random.seed(28)


def trama(n, k):
    # una trama de Ethernet de n octetos (destino, origen, tipo y datos)
    return bytes([0x02, 0, 0, 0, 0, k]) + bytes([0x02, 0x11, 0x22, 0x33, 0x44, 0x55]) + b'\x08\x00' + bytes(random.getrandbits(8) for _ in range(n - 14))


def al(x, a):
    return (x + a - 1) // a * a


def ntb_linux(dgs):
    # NTH16, los datagramas (alineados a 4) y el NDP16 al final (alineado a 4)
    b = bytearray(12)
    idx = []
    for d in dgs:
        while len(b) % 4:
            b.append(0)
        idx.append((len(b), len(d)))
        b += d
    while len(b) % 4:
        b.append(0)
    ndp = len(b)
    n = len(idx)
    b += b'NCM0' + struct.pack('<HH', 8 + 4 * (n + 1), 0)
    for o, l in idx:
        b += struct.pack('<HH', o, l)
    b += struct.pack('<HH', 0, 0)
    b[0:12] = b'NCMH' + struct.pack('<HHHH', 12, 7, len(b), ndp)
    return bytes(b)


def ntb_delante(dgs, firma=b'NCM0'):
    n = len(dgs)
    ndp = 12
    o = al(ndp + 8 + 4 * (n + 1), 4)
    cuerpo = bytearray()
    idx = []
    for d in dgs:
        pos = o + len(cuerpo)
        while pos % 4:
            cuerpo.append(0)
            pos += 1
        idx.append((pos, len(d)))
        cuerpo += d
    b = bytearray(b'NCMH' + struct.pack('<HHHH', 12, 8, o + len(cuerpo), ndp))
    b += firma + struct.pack('<HH', 8 + 4 * (n + 1), 0)
    for x, l in idx:
        b += struct.pack('<HH', x, l)
    b += struct.pack('<HH', 0, 0)
    while len(b) < o:
        b.append(0)
    b += cuerpo
    return bytes(b)


def ntb_dos_ndp(d1, d2):
    # dos NDP16 encadenados (wNextNdpIndex), uno con cada datagrama
    b = bytearray(12)
    b += d1
    while len(b) % 4:
        b.append(0)
    o2 = len(b)
    b += d2
    while len(b) % 4:
        b.append(0)
    ndp1 = len(b)
    ndp2 = ndp1 + 16
    b += b'NCM0' + struct.pack('<HH', 16, ndp2) + struct.pack('<HHHH', 12, len(d1), 0, 0)
    b += b'NCM0' + struct.pack('<HH', 16, 0) + struct.pack('<HHHH', o2, len(d2), 0, 0)
    b[0:12] = b'NCMH' + struct.pack('<HHHH', 12, 9, len(b), ndp1)
    return bytes(b)


def rndis(dgs, relleno=8):
    b = bytearray()
    for d in dgs:
        l = al(44 + len(d), relleno)
        b += struct.pack('<11I', 1, l, 36, len(d), 0, 0, 0, 0, 0, 0, 0) + d + bytes(l - 44 - len(d))
    return bytes(b)


def casos():
    t = [trama(n, k) for k, n in enumerate([60, 1514, 342, 98, 1200, 64, 590, 1514, 1514, 1514])]
    c = []
    # (nombre, clase, datos, tramas esperadas, tiradas esperadas)
    c.append(("NCM: un NTB16 como los de f_ncm de Linux (tres datagramas, el NDP16 al final)", 2, ntb_linux(t[0:3]), t[0:3], 0))
    c.append(("NCM: el NDP16 delante, detrás de la cabecera (dos datagramas)", 2, ntb_delante(t[3:5]), t[3:5], 0))
    c.append(("NCM: la firma «NCM1» (con CRC: los datos, igual)", 2, ntb_delante(t[5:6], b'NCM1'), t[5:6], 0))
    c.append(("NCM: dos NDP16 encadenados", 2, ntb_dos_ndp(t[6], t[1]), [t[6], t[1]], 0))
    c.append(("NCM: diez datagramas en 16 K (tres de 1514 y más)", 2, ntb_linux(t), t, 0))
    malo = bytearray(ntb_linux(t[0:2]))
    malo[0:4] = b'XXXX'
    c.append(("NCM: una cabecera que no es NTH16 se tira entera", 2, bytes(malo), [], 1))
    fuera = bytearray(ntb_delante(t[3:4]))
    struct.pack_into('<H', fuera, 12 + 10, 4000)     # el largo del datagrama, fuera del NTB
    c.append(("NCM: un datagrama que se sale del NTB se tira (y lo demás sigue)", 2, bytes(fuera), [], 1))
    c.append(("RNDIS: tres paquetes en una transferencia, rellenos hasta 8 (como los junta Android)", 1, rndis(t[0:3]), t[0:3], 0))
    c.append(("RNDIS: un paquete solo, sin relleno (como QEMU)", 1, rndis(t[3:4], 1), t[3:4], 0))
    c.append(("RNDIS: lo que no es REMOTE_NDIS_PACKET_MSG se tira", 1, struct.pack('<3I', 7, 20, 0) + bytes(40), [], 1))
    # armar: (nombre, clase, divisor, resto, alineación, paquete, trama)
    a = []
    a.append(("NCM, armar: NTH16, NDP16 en 12 y el datagrama en 28 (divisor 4)", 4, 4, 0, 4, 512, t[3]))
    a.append(("NCM, armar: con resto 2 (la cabecera IP alineada a 4): el datagrama en 30", 4, 4, 2, 4, 512, t[0]))
    a.append(("NCM, armar: un NTB que es múltiplo del paquete (512) lleva un octeto de más", 4, 4, 0, 4, 512, trama(484, 9)))
    a.append(("RNDIS, armar: REMOTE_NDIS_PACKET_MSG de 44 octetos y la trama", 3, 0, 0, 0, 64, t[2]))
    a.append(("RNDIS, armar: 44 + 20 = 64 (el paquete): un octeto de más", 3, 0, 0, 0, 64, trama(20, 10)))
    return c, a


def esperado_arma(clase, div, resto, alin, paquete, d):
    if clase == 4:
        ndp = al(12, alin)
        o = ndp + 16
        while o % div != resto:
            o += 1
        k = o + len(d)
        b = bytearray(b'NCMH' + struct.pack('<HHHH', 12, 0, k, ndp))
        while len(b) < ndp:
            b.append(0)
        b += b'NCM0' + struct.pack('<HHHHHH', 16, 0, o, len(d), 0, 0)
        while len(b) < o:
            b.append(0)
        b += d
    else:
        b = bytearray(struct.pack('<11I', 1, 44 + len(d), 36, len(d), 0, 0, 0, 0, 0, 0, 0) + d)
    if paquete and len(b) % paquete == 0:
        b.append(0)
    return bytes(b)


def hacer(ruta):
    c, a = casos()
    b = bytearray()
    for _, clase, datos, _, _ in c:
        b += bytes([clase]) + struct.pack('<H', len(datos)) + datos
    for _, clase, div, resto, alin, paquete, d in a:
        b += bytes([clase]) + struct.pack('<HHHHH', div, resto, alin, paquete, len(d)) + d
    open(ruta, 'wb').write(b)


def mirar(ruta):
    c, a = casos()
    s = open(ruta, 'rb').read()
    p = 0
    mal = 0
    for nombre, _, _, tramas, tiradas in c:
        n, ti = struct.unpack_from('<HH', s, p)
        p += 4
        vistas = []
        for _ in range(n):
            (l,) = struct.unpack_from('<H', s, p)
            vistas.append(s[p + 2:p + 2 + l])
            p += 2 + l
        if vistas == tramas and ti == tiradas:
            print("  OK   " + nombre)
        else:
            mal += 1
            print("  FAIL %s (%d tramas, %d tiradas; se esperaban %d y %d)" % (nombre, n, ti, len(tramas), tiradas))
    for nombre, clase, div, resto, alin, paquete, d in a:
        (k,) = struct.unpack_from('<H', s, p)
        visto = s[p + 2:p + 2 + k]
        p += 2 + k
        e = esperado_arma(clase, div, resto, alin, paquete, d)
        # (el número de secuencia del NTH16 no se mira)
        if clase == 4 and len(visto) >= 8:
            visto = visto[:6] + b'\0\0' + visto[8:]
        if visto == e:
            print("  OK   " + nombre)
        else:
            mal += 1
            print("  FAIL %s (%d octetos; se esperaban %d)" % (nombre, k, len(e)))
    return 1 if mal else 0


if __name__ == '__main__':
    if sys.argv[1] == 'hacer':
        hacer(sys.argv[2])
    else:
        sys.exit(mirar(sys.argv[2]))
