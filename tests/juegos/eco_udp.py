#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# tests/juegos/eco_udp.py CARPETA PUERTO_FUERA: el otro lado de REDUDP
# (JUEGOS): un eco de UDP (en un puerto libre, que deja en CARPETA/puerto)
# y un «cliente» que manda al servidor de Hispa-Pentium (por el puerto de
# QEMU llevado al 27015), cada medio segundo hasta que contesta, y apunta
# lo que contesta en CARPETA/respuesta.
import os, socket, sys, time
d, fuera = sys.argv[1], int(sys.argv[2])
e = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
e.bind(('127.0.0.1', 0))
e.settimeout(0.2)
open(os.path.join(d, 'puerto'), 'w').write(str(e.getsockname()[1]))
c = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
c.settimeout(0.2)
fin = time.time() + 240
otra = 0
hecho = False
while time.time() < fin:
    try:
        datos, quien = e.recvfrom(2000)
        e.sendto(b'eco: ' + datos, quien)
    except socket.timeout:
        pass
    if not hecho and time.time() >= otra:
        otra = time.time() + 0.5
        c.sendto(b'hola servidor', ('127.0.0.1', fuera))
    if not hecho:
        try:
            r, _ = c.recvfrom(2000)
            open(os.path.join(d, 'respuesta'), 'wb').write(r)
            hecho = True
        except (socket.timeout, ConnectionRefusedError):
            pass
