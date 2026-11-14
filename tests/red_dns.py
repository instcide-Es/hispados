#!/usr/bin/env python3
# Hispa-DOS - servidor DNS falso para tests/run_red_tests.sh: contesta a las
# preguntas de tipo A de unos pocos nombres (todos a 10.0.2.2, la puerta de
# la red de usuario de QEMU) y «no existe» a lo demás. No sale a Internet.
#   red_dns.py dirección puerto
# Fake DNS server for the network tests.
import socket
import struct
import sys

NOMBRES = {b'prueba.hispados': '10.0.2.2', b'servidor.casa': '10.0.2.2'}


def nombre(d, i):
    partes = []
    while d[i]:
        n = d[i]
        partes.append(d[i + 1:i + 1 + n])
        i += 1 + n
    return b'.'.join(partes).lower(), i + 1


def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind((sys.argv[1], int(sys.argv[2])))
    while True:
        d, a = s.recvfrom(1500)
        try:
            ident = d[:2]
            n, fin = nombre(d, 12)
            q = d[12:fin + 4]
            ip = NOMBRES.get(n)
            if ip:
                cab = ident + b'\x81\x80' + struct.pack('>HHHH', 1, 1, 0, 0)
                resp = b'\xc0\x0c' + struct.pack('>HHIH', 1, 1, 60, 4) + socket.inet_aton(ip)
                s.sendto(cab + q + resp, a)
            else:
                cab = ident + b'\x81\x83' + struct.pack('>HHHH', 1, 0, 0, 0)
                s.sendto(cab + q, a)
            sys.stderr.write('dns: %s -> %s\n' % (n.decode(), ip))
        except Exception as e:  # pregunta mal formada: se ignora
            sys.stderr.write('dns: %r\n' % e)


main()
