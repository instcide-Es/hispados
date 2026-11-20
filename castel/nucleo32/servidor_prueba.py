#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/servidor_prueba.py CARPETA

Un servidor HTTP para probar la red de Hispa-Pentium (DESCARGAR) en QEMU:
escucha en 127.0.0.1, en un puerto libre que deja en CARPETA/puerto (el
programa, desde QEMU con «-netdev user», lo ve en 10.0.2.2) y deja en CARPETA
los archivos que sirve, para compararlos con lo descargado:

  /grande.bin    409.600 octetos al azar (siempre los mismos), con Content-Length
  /trozos.bin    150.001 octetos, en trozos («Transfer-Encoding: chunked»)
  /cierra.bin    70.000 octetos, sin largo: acaba cuando se cierra (HTTP/1.0)
  /ultima.txt    un texto corto
  /redirige      302, a /ultima.txt
  /lento.bin     dice que son 1.000.000 y manda 2.000; luego, nada (para Ctrl+C)
  lo demás       404, o, con RAIZ (un segundo argumento), el archivo de
                 RAIZ con ese camino (las actualizaciones: /pentium/...);
                 los pedidos de esos, uno por línea, en CARPETA/pedidos

Lo pone en marcha pruebas.sh (parte 11) y lo para por su PID.
"""
import http.server
import os
import random
import socketserver
import sys
import time

CARPETA = sys.argv[1]
RAIZ = sys.argv[2] if len(sys.argv) > 2 else None
rnd = random.Random(20261120)
ARCHIVOS = {
    'grande.bin': bytes(rnd.getrandbits(8) for _ in range(409600)),
    'trozos.bin': bytes(rnd.getrandbits(8) for _ in range(150001)),
    'cierra.bin': bytes(rnd.getrandbits(8) for _ in range(70000)),
    'ultima.txt': b'Hispa-Pentium 0.8: la red.\r\nversion=0.8\r\n',
}
for n, d in ARCHIVOS.items():
    open(os.path.join(CARPETA, n), 'wb').write(d)


class Atiende(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def do_GET(self):
        c = self.path.split('?')[0].lstrip('/')
        if c == 'redirige':
            self.send_response(302)
            self.send_header('Location', '/ultima.txt')
            self.send_header('Content-Length', '0')
            self.end_headers()
        elif c == 'trozos.bin':
            d = ARCHIVOS[c]
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            i = 0
            k = 1
            while i < len(d):
                t = d[i:i + k]
                self.wfile.write(b'%x;ext=1\r\n' % len(t) + t + b'\r\n')
                i += len(t)
                k = k * 3 % 9001 + 1
            self.wfile.write(b'0\r\nX-Fin: si\r\n\r\n')
        elif c == 'cierra.bin':
            self.protocol_version = 'HTTP/1.0'
            self.send_response(200)
            self.end_headers()
            self.wfile.write(ARCHIVOS[c])
            self.close_connection = True
        elif c == 'lento.bin':
            self.send_response(200)
            self.send_header('Content-Length', '1000000')
            self.end_headers()
            self.wfile.write(bytes(2000))
            self.wfile.flush()
            time.sleep(60)
        elif RAIZ and '..' not in c and os.path.isfile(os.path.join(RAIZ, c)):
            d = open(os.path.join(RAIZ, c), 'rb').read()
            with open(os.path.join(CARPETA, 'pedidos'), 'a') as f:
                f.write(c + '\n')
            self.send_response(200)
            self.send_header('Content-Length', str(len(d)))
            self.end_headers()
            self.wfile.write(d)
        elif c in ARCHIVOS:
            d = ARCHIVOS[c]
            self.send_response(200)
            self.send_header('Content-Length', str(len(d)))
            self.end_headers()
            self.wfile.write(d)
        else:
            d = b'no existe\r\n'
            self.send_response(404)
            self.send_header('Content-Length', str(len(d)))
            self.end_headers()
            self.wfile.write(d)


class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


s = Servidor(('127.0.0.1', 0), Atiende)
open(os.path.join(CARPETA, 'puerto'), 'w').write(str(s.server_address[1]))
s.serve_forever()
