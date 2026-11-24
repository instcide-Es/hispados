#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/servidor_cortes.py CARPETA [RAIZ]

(CORTES) Un servidor HTTP que corta las descargas a mitad, para probar que
Hispa-Pentium (DESCARGA, PAQUETES) se recupera: escucha en 127.0.0.1, en un
puerto libre que deja en CARPETA/puerto (desde QEMU, «-netdev user», es
10.0.2.2). Como nginx (actualiza.hispados.es), admite «Range: bytes=N-»
(206, con Content-Range) y «If-Range» (con su ETag: si no es la misma, 200
con todo). Sirve:

  /grande.bin      3.000.000 octetos al azar (siempre los mismos; también
                   en CARPETA/grande.bin, para compararlos)
  /pentium/...     los archivos de RAIZ (los paquetes y el catálogo)

y, delante del camino, cómo se porta (la primera vez que se pide ese
archivo entero; luego, bien):

  /calla/...       manda la mitad y se calla, sin cerrar (5 minutos)
  /cierra/...      manda la mitad y cierra (menos de lo que dijo)
  /rst/...         manda la mitad y corta con un RST
  /norango/...     manda la mitad y cierra; y no hace caso de Range (200 con
                   todo): hay que empezar de nuevo
  /lento/...       siempre, a unos 150 K/s (para cortar el enlace a mano o
                   pulsar «Cancelar» mientras tanto)
  /muylento/...    siempre, a unos 30 K/s
  /muere/...       (0.8.10) manda la mitad y cierra; al siguiente pedido (el
                   resto), 503; luego, bien: DESCARGAR no acaba, deja lo
                   bajado en NOMBRE.PAR y la vez siguiente sigue desde ahí

Los cortes, solo en lo que pasa de 100 K (no en el catálogo). Cada pedido
se apunta en CARPETA/pedidos: «camino rango» (el Range, o -), y en
CARPETA/servidos lo que se ha mandado de cada uno (con la hora, si el otro
lado ha cerrado o cortado). Lo pone en marcha
pruebas_cortes.sh (la parte 37) y lo para por su PID.
"""
import http.server
import os
import random
import select
import socket
import socketserver
import struct
import sys
import threading
import zlib
import time

CARPETA = sys.argv[1]
RAIZ = sys.argv[2] if len(sys.argv) > 2 else None
rnd = random.Random(20261122)
GRANDE = bytes(rnd.getrandbits(8) for _ in range(3000000))
open(os.path.join(CARPETA, 'grande.bin'), 'wb').write(GRANDE)
MODOS = ('calla', 'cierra', 'rst', 'norango', 'lento', 'muylento', 'muere')
muertes = {}
vistos = {}
cerrojo = threading.Lock()


def apunta(nombre, t):
    with cerrojo:
        with open(os.path.join(CARPETA, nombre), 'a') as f:
            f.write(t + '\n')


class Atiende(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def datos_de(self, c):
        if c == 'grande.bin':
            return GRANDE
        if RAIZ and c.startswith('pentium/') and '..' not in c:
            r = os.path.join(RAIZ, c[len('pentium/'):])
            if os.path.isfile(r):
                return open(r, 'rb').read()
        return None

    def do_GET(self):
        partes = self.path.split('?')[0].lstrip('/').split('/', 1)
        modo = None
        if len(partes) == 2 and partes[0] in MODOS:
            modo, c = partes
        else:
            c = '/'.join(partes)
        d = self.datos_de(c)
        rango = self.headers.get('Range')
        apunta('pedidos', '%s %s' % (self.path.split('?')[0], rango or '-'))
        if d is None:
            m = b'no existe\r\n'
            self.send_response(404)
            self.send_header('Content-Length', str(len(m)))
            self.end_headers()
            self.wfile.write(m)
            return
        if modo == 'muere':
            with cerrojo:
                k = muertes.get(c, 0)
                muertes[c] = k + 1
            if k == 1:
                m = b'ahora no\r\n'
                self.send_response(503)
                self.send_header('Content-Length', str(len(m)))
                self.end_headers()
                self.wfile.write(m)
                return
        etag = '"%08x-%x"' % (zlib.crc32(d), len(d))
        desde = 0
        if rango and modo != 'norango':
            si_rango = self.headers.get('If-Range')
            if rango.startswith('bytes=') and rango.endswith('-') and (si_rango is None or si_rango == etag):
                try:
                    desde = int(rango[6:-1])
                except ValueError:
                    desde = 0
        if desde >= len(d) and desde > 0:
            self.send_response(416)
            self.send_header('Content-Range', 'bytes */%d' % len(d))
            self.send_header('Content-Length', '0')
            self.end_headers()
            return
        if desde > 0:
            self.send_response(206)
            self.send_header('Content-Range', 'bytes %d-%d/%d' % (desde, len(d) - 1, len(d)))
        else:
            self.send_response(200)
        self.send_header('Content-Length', str(len(d) - desde))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('ETag', etag)
        self.send_header('Last-Modified', 'Sun, 22 Nov 2026 17:20:52 GMT')
        self.end_headers()
        cuerpo = d[desde:]
        clave = (modo, c)
        with cerrojo:
            primera = clave not in vistos
            vistos[clave] = vistos.get(clave, 0) + 1
        mandado = 0
        try:
            if modo in ('calla', 'cierra', 'rst', 'norango', 'muere') and primera and desde == 0 and len(cuerpo) > 100000:
                mitad = len(cuerpo) // 2
                for i in range(0, mitad, 16384):
                    self.wfile.write(cuerpo[i:min(i + 16384, mitad)])
                    mandado = min(i + 16384, mitad)
                self.wfile.flush()
                apunta('servidos', '%s %s %d corte' % (modo, c, mandado))
                if modo == 'calla':
                    # (callado, pero mirando si el otro cierra: para medir
                    # lo que tarda «Cancelar»)
                    fin = time.time() + 300
                    while time.time() < fin:
                        listos, _, _ = select.select([self.connection], [], [], 0.1)
                        if listos:
                            try:
                                m = self.connection.recv(4096)
                            except OSError:
                                m = b''
                            if not m:
                                apunta('servidos', '%s %s el cliente ha cerrado %.2f' % (modo, c, time.time()))
                                break
                elif modo == 'rst':
                    self.connection.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
                    self.connection.close()
                self.close_connection = True
                return
            paso, pausa = 16384, 0
            if modo == 'lento':
                paso, pausa = 8192, 0.05
            elif modo == 'muylento':
                paso, pausa = 4096, 0.13
            for i in range(0, len(cuerpo), paso):
                self.wfile.write(cuerpo[i:i + paso])
                mandado = min(i + paso, len(cuerpo))
                if pausa:
                    self.wfile.flush()
                    time.sleep(pausa)
            apunta('servidos', '%s %s %d desde %d' % (modo or 'bien', c, mandado, desde))
        except OSError:
            apunta('servidos', '%s %s %d cortada por el otro lado %.2f' % (modo or 'bien', c, mandado, time.time()))
            self.close_connection = True


class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


s = Servidor(('127.0.0.1', 0), Atiende)
open(os.path.join(CARPETA, 'puerto'), 'w').write(str(s.server_address[1]))
s.serve_forever()
