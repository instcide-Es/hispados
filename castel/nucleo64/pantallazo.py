#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pantallazo.py - para las pruebas de Hispa-64 (pruebas.sh):
#   pantallazo.py captura MONITOR.sock SALIDA.ppm   pide a QEMU la pantalla (screendump) y lo apaga
#   pantallazo.py mira SALIDA.ppm SALIDA.png          cuenta los puntos de cada color, la pasa a PNG
#                                                    y dice «ancho alto amarillos grises verdes»
#   pantallazo.py orden MONITOR.sock ORDEN            le da una orden al monitor de QEMU (p. ej. «nmi»)
import socket
import struct
import sys
import time
import zlib


def captura(sock, ppm):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(5)
    time.sleep(0.3)
    s.sendall(f'screendump {ppm}\n'.encode())
    time.sleep(1.5)
    s.sendall(b'quit\n')
    time.sleep(0.5)
    s.close()


def orden(sock, o):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(5)
    time.sleep(0.3)
    s.sendall(f'{o}\n'.encode())
    time.sleep(0.5)
    s.close()


def lee_ppm(ppm):
    d = open(ppm, 'rb').read()
    campos = []
    i = 0
    while len(campos) < 4:
        while d[i:i + 1].isspace():
            i += 1
        j = i
        while not d[j:j + 1].isspace():
            j += 1
        campos.append(d[i:j])
        i = j
    i += 1
    assert campos[0] == b'P6'
    return int(campos[1]), int(campos[2]), d[i:]


def png(ancho, alto, px, sal):
    filas = b''.join(b'\0' + px[y * ancho * 3:(y + 1) * ancho * 3] for y in range(alto))

    def trozo(t, datos):
        return struct.pack('>I', len(datos)) + t + datos + struct.pack('>I', zlib.crc32(t + datos) & 0xFFFFFFFF)
    with open(sal, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', ancho, alto, 8, 2, 0, 0, 0))
                + trozo(b'IDAT', zlib.compress(filas, 6)) + trozo(b'IEND', b''))


def mira(ppm, sal):
    ancho, alto, px = lee_ppm(ppm)
    amarillos = grises = verdes = 0
    for k in range(0, ancho * alto * 3, 3):
        r, g, b = px[k], px[k + 1], px[k + 2]
        if r == 255 and g == 255 and b == 85:
            amarillos += 1
        elif r == 170 and g == 170 and b == 170:
            grises += 1
        elif r == 85 and g == 255 and b == 85:
            verdes += 1
    png(ancho, alto, px, sal)
    print(ancho, alto, amarillos, grises, verdes)


if __name__ == '__main__':
    if sys.argv[1] == 'captura':
        captura(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == 'orden':
        orden(sys.argv[2], sys.argv[3])
    else:
        mira(sys.argv[2], sys.argv[3])
