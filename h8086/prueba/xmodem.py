#!/usr/bin/env python3
"""El otro lado de PASAR, para la prueba: habla XMODEM por un zócalo de Unix
(el puerto serie de QEMU).

uso: xmodem.py ZÓCALO enviar ARCHIVO     manda ARCHIVO (PASAR RECIBIR lo espera)
     xmodem.py ZÓCALO recibir ARCHIVO    recibe en ARCHIVO (PASAR ENVIAR lo manda)
     ... [suma]                          con la suma de 8 bits en vez del CRC

Es XMODEM de libro, hecho aparte del de PASAR (en ensamblador): si los dos se
entienden, es que los dos cumplen el protocolo.

Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import socket, sys, time

SOH, EOT, ACK, NAK, CAN = 1, 4, 6, 0x15, 0x18


def crc16(d):
    c = 0
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


class Linea:
    def __init__(self, ruta):
        self.s = socket.socket(socket.AF_UNIX)
        for _ in range(50):
            try:
                self.s.connect(ruta)
                break
            except OSError:
                time.sleep(0.1)

    def lee(self, plazo):
        self.s.settimeout(plazo)
        try:
            d = self.s.recv(1)
        except socket.timeout:
            return None
        return d[0] if d else None

    def pon(self, d):
        self.s.sendall(bytes(d))


def enviar(l, datos):
    # el que recibe empieza: «C» (CRC) o NAK (suma)
    con_crc = None
    for _ in range(60):
        c = l.lee(1)
        if c == ord('C'):
            con_crc = True
            break
        if c == NAK:
            con_crc = False
            break
    if con_crc is None:
        return 'el otro lado no pide nada'
    n = 1
    for i in range(0, len(datos), 128):
        b = datos[i:i + 128].ljust(128, b'\x1a')
        cola = crc16(b).to_bytes(2, 'big') if con_crc else bytes([sum(b) & 255])
        for _ in range(10):
            l.pon(bytes([SOH, n, 255 - n]) + b + cola)
            c = l.lee(10)
            if c == ACK:
                break
            if c == CAN:
                return 'cancelado'
        else:
            return 'el bloque %d no se da por bueno' % n
        n = (n + 1) & 255
    for _ in range(5):
        l.pon([EOT])
        if l.lee(5) == ACK:
            return None
    return 'no se da por bueno el final'


def recibir(l, con_crc=True):
    datos, n = b'', 1
    l.pon(b'C' if con_crc else [NAK])
    while True:
        c = l.lee(10)
        if c is None:
            return None, 'el otro lado no manda nada'
        if c == EOT:
            l.pon([ACK])
            return datos.rstrip(b'\x1a'), None
        if c == CAN:
            return None, 'cancelado'
        if c != SOH:
            continue
        cab = [l.lee(2), l.lee(2)]
        b = bytes(l.lee(2) or 0 for _ in range(128))
        if con_crc:
            bien = ((l.lee(2) or 0) << 8 | (l.lee(2) or 0)) == crc16(b)
        else:
            bien = l.lee(2) == sum(b) & 255
        if not bien or cab[0] is None or cab[0] + cab[1] != 255:
            l.pon([NAK])
            continue
        if cab[0] == n:
            datos += b
            n = (n + 1) & 255
        l.pon([ACK])


def main():
    l = Linea(sys.argv[1])
    suma = 'suma' in sys.argv[4:]
    if sys.argv[2] == 'enviar':
        err = enviar(l, open(sys.argv[3], 'rb').read())
    else:
        d, err = recibir(l, not suma)
        if err is None:
            open(sys.argv[3], 'wb').write(d)
    if err:
        print('xmodem.py:', err)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
