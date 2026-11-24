#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/pantallazo.py - para las pruebas de Hispa-64 (pruebas.sh):
#   pantallazo.py captura MONITOR.sock SALIDA.ppm   pide a QEMU la pantalla (screendump) y lo apaga
#   pantallazo.py mira SALIDA.ppm SALIDA.png          cuenta los puntos de cada color, la pasa a PNG
#                                                    y dice «ancho alto amarillos grises verdes»
#   pantallazo.py orden MONITOR.sock ORDEN            le da una orden al monitor de QEMU (p. ej. «nmi»)
#   [etapa 8] el comparador de los pantallazos de las ventanas:
#   pantallazo.py foto MONITOR.sock SALIDA.ppm        un pantallazo, sin apagar
#   pantallazo.py cuenta A.ppm X Y AN AL RRGGBB [TOL] los puntos de ese color (con TOL de
#                                                    margen en cada canal) en el rectángulo
#   pantallazo.py oscuros A.ppm X Y AN AL [LUZ]       los puntos con menos luz que LUZ (de 0 a 255;
#                                                    100 si no se dice): la letra, las rayas
#   pantallazo.py difiere A.ppm B.ppm X Y AN AL       los puntos que cambian de A a B en el rectángulo
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


def rect(ancho, alto, x, y, an, al):
    for f in range(max(0, y), min(alto, y + al)):
        for c in range(max(0, x), min(ancho, x + an)):
            yield (f * ancho + c) * 3


def cuenta(ppm, x, y, an, al, color, tol=0):
    ancho, alto, px = lee_ppm(ppm)
    r0, g0, b0 = (color >> 16) & 255, (color >> 8) & 255, color & 255
    return sum(1 for k in rect(ancho, alto, x, y, an, al)
               if abs(px[k] - r0) <= tol and abs(px[k + 1] - g0) <= tol and abs(px[k + 2] - b0) <= tol)


def oscuros(ppm, x, y, an, al, luz=100):
    ancho, alto, px = lee_ppm(ppm)
    return sum(1 for k in rect(ancho, alto, x, y, an, al) if (px[k] * 3 + px[k + 1] * 6 + px[k + 2]) // 10 < luz)


def difiere(a, b, x, y, an, al):
    ancho, alto, pa = lee_ppm(a)
    _, _, pb = lee_ppm(b)
    return sum(1 for k in rect(ancho, alto, x, y, an, al) if pa[k:k + 3] != pb[k:k + 3])


def foto(sock, ppm):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(5)
    time.sleep(0.2)
    s.sendall(f'screendump {ppm}\n'.encode())
    time.sleep(1.5)
    s.close()


if __name__ == '__main__':
    n = [int(v) for v in sys.argv[3:7]] + ([int(sys.argv[7])] if sys.argv[1] == 'oscuros' and len(sys.argv) > 7 else []) if sys.argv[1] in ('cuenta', 'oscuros') else []
    if sys.argv[1] == 'cuenta':
        print(cuenta(sys.argv[2], n[0], n[1], n[2], n[3], int(sys.argv[7], 16), int(sys.argv[8]) if len(sys.argv) > 8 else 0))
    elif sys.argv[1] == 'oscuros':
        print(oscuros(sys.argv[2], *n))
    elif sys.argv[1] == 'difiere':
        print(difiere(sys.argv[2], sys.argv[3], *[int(v) for v in sys.argv[4:8]]))
    elif sys.argv[1] == 'foto':
        foto(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == 'captura':
        captura(sys.argv[2], sys.argv[3])
    elif sys.argv[1] == 'orden':
        orden(sys.argv[2], sys.argv[3])
    else:
        mira(sys.argv[2], sys.argv[3])
