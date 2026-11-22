#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/ntp_prueba.py PUERTO:HORA [PUERTO:HORA...]
# (NTP) Servidores NTP de mentira para las pruebas de HORA.BIN y del Panel
# de control (castel/pruebas.sh, la parte 34): cada uno escucha en su
# puerto UDP (en QEMU, «10.0.2.2:PUERTO») y contesta como un servidor de
# verdad (estrato 2, modo 4, con el «originate» del que pregunta), con la
# hora que se le diga:
#   0          la de verdad (la de esta máquina, en UTC)
#   +N / -N    la de verdad más (o menos) N segundos: el que miente
#   AAAA-MM-DD esa fecha (a las 12:00 UTC): el que miente mucho
#   callado    no contesta
# Lo que le preguntan lo dice por la salida («ntp: PUERTO ...»).
import socket, select, struct, sys, time, calendar

DESDE_1900 = 2208988800

def hora_de(modo):
    if modo == 'callado':
        return None
    if modo == '0':
        return time.time()
    if modo[0] in '+-':
        return time.time() + int(modo)
    return calendar.timegm(time.strptime(modo, '%Y-%m-%d')) + 12 * 3600

def a_ntp(t):
    s = int(t)
    return struct.pack('>II', (s + DESDE_1900) & 0xFFFFFFFF, int((t - s) * 2**32) & 0xFFFFFFFF)

socks = {}
for a in sys.argv[1:]:
    p, modo = a.split(':', 1)
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(('127.0.0.1', int(p)))
    socks[s] = (int(p), modo)
print('ntp: escuchando en', ', '.join('%d (%s)' % v for v in socks.values()), flush=True)
while True:
    listos, _, _ = select.select(list(socks), [], [])
    for s in listos:
        datos, de = s.recvfrom(512)
        p, modo = socks[s]
        t = hora_de(modo)
        if len(datos) < 48 or t is None:
            print('ntp: %d: pregunta de %s:%d, no contesto' % (p, de[0], de[1]), flush=True)
            continue
        llega = a_ntp(t)
        r = bytes([0x24, 2, 6, 0xEC]) + struct.pack('>II', 0, 0) + b'HISP' + llega + datos[40:48] + llega + a_ntp(hora_de(modo))
        s.sendto(r, de)
        print('ntp: %d: contesto a %s:%d con %s UTC (%s)' % (p, de[0], de[1], time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(t)), modo), flush=True)
