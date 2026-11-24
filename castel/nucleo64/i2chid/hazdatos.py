#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hazdatos.py - Hispa-64, etapa 6: pasa los guiones de prueba de los
# descriptores HID (reales.txt, sint.txt y qemu.txt, de esta carpeta) a los
# octetos que lleva dentro prhid64.cas (entre «; <datos>» y «; </datos>»),
# para que el núcleo los analice al arrancar sin necesitar archivos.
#
#   hazdatos.py          rehace el trozo de prhid64.cas
#   hazdatos.py --mira   solo mira que esté al día (pruebas.sh): 0 si lo está
#
# Cada línea del guion es una orden (las de uhidprue.cas de Hispa-Pentium):
#   d HEX...   más octetos del descriptor de informe      ? analizarlo y decirlo
#   i HEX...   un informe del ratón                        - otro descriptor
#   a HEX...   un informe del puntero absoluto             m la orden del modo de entrada a 0
#   k HEX...   un informe del teclado                      l N los LED N (en decimal)
#   h HEX...   un descriptor HID de HID sobre I2C          o R O C I una orden (en decimal)
#   # ...      un comentario (no pasa)
# y en prhid64.cas va como [la letra][el largo, 2 octetos][los datos], con un 0 al final.
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
GUIONES = ['reales.txt', 'sint.txt', 'qemu.txt']
DESTINO = os.path.join(AQUI, '..', 'prhid64.cas')


def octetos():
    sal = bytearray()
    for g in GUIONES:
        sal += b'-\0\0'          # (cada guion, con un descriptor nuevo)
        for lin in open(os.path.join(AQUI, g), encoding='utf-8'):
            lin = lin.rstrip('\n').rstrip('\r')
            if not lin or lin[0] == '#':
                continue
            c = lin[0]
            resto = lin[1:].split()
            if c in 'diakh':
                datos = bytes(int(x, 16) for x in resto)
            elif c == 'o':
                datos = b''.join(int(x).to_bytes(2, 'little') for x in resto[:4])
            elif c == 'l':
                datos = int(resto[0]).to_bytes(2, 'little')
            elif c in '?-m':
                datos = b''
            else:
                sys.exit('%s: no sé qué es «%s»' % (g, lin))
            sal += bytes([ord(c)]) + len(datos).to_bytes(2, 'little') + datos
    return bytes(sal) + b'\0'


def trozo():
    o = octetos()
    lins = ['            ; <datos> (de i2chid/hazdatos.py: %d octetos; no se toca a mano)' % len(o)]
    for i in range(0, len(o), 16):
        lins.append('            db ' + ', '.join('0x%02X' % b for b in o[i:i + 16]))
    lins.append('            ; </datos>')
    return '\n'.join(lins) + '\n'


def main():
    texto = open(DESTINO, encoding='utf-8').read()
    a = texto.index('            ; <datos>')
    b = texto.index('            ; </datos>\n') + len('            ; </datos>\n')
    nuevo = texto[:a] + trozo() + texto[b:]
    if len(sys.argv) > 1 and sys.argv[1] == '--mira':
        sys.exit(0 if nuevo == texto else 1)
    if nuevo != texto:
        open(DESTINO, 'w', encoding='utf-8').write(nuevo)


main()
