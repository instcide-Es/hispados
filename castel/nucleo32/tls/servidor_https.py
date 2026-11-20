#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/tls/servidor_https.py CARPETA

Los servidores HTTPS para probar el servicio TLS de Hispa-Pentium (TLS.BIN,
DESCARGAR https://, HTTPS.BIN y la biblioteca Https) en QEMU sin depender de
internet. Hace con openssl, en CARPETA, una autoridad raíz de prueba
(RAIZ.CER, en DER: va al disco de prueba, en \\RAICES) y los certificados de
los servidores; escucha en 127.0.0.1 (desde QEMU con «-netdev user», en
10.0.2.2) y deja los puertos en CARPETA/puertos («nombre=puerto» por línea):

  bueno     ECDSA P-256, firmado por la raíz de prueba, para «10.0.2.2»
  rsa       RSA 2048, firmado por la raíz de prueba, para «10.0.2.2»
  otro      firmado por la raíz de prueba, pero para «otro.ejemplo»
  extrano   firmado por otra raíz, que no conoce nadie
  hora      HTTP sin cifrar, solo para la cabecera Date (TLS.INI: hora=)

Lo que sirven (los cuatro de HTTPS, lo mismo):
  /grande.bin    409.600 octetos al azar (los de servidor_prueba.py), con Content-Length
  /trozos.bin    150.001 octetos, en trozos (chunked)
  /ultima.txt    un texto corto
  /redirige      302, a /ultima.txt;  /lejos  301, a https://10.0.2.2:PUERTO/ultima.txt
  /galleta       pone dos galletas (Set-Cookie);  /mira  dice las que le llegan
  /eco           (POST o GET) dice el método, las cabeceras X-... y el cuerpo
  lo demás       404
Lo pone en marcha pruebas.sh y lo para por su PID.
"""
import http.server
import os
import random
import socketserver
import ssl
import subprocess
import sys
import threading

CARPETA = sys.argv[1]
rnd = random.Random(20261120)
ARCHIVOS = {
    'grande.bin': bytes(rnd.getrandbits(8) for _ in range(409600)),
    'trozos.bin': bytes(rnd.getrandbits(8) for _ in range(150001)),
    'ultima.txt': b'Hispa-Pentium 0.8: la red, cifrada.\r\nversion=0.8\r\n',
}
for n, d in ARCHIVOS.items():
    open(os.path.join(CARPETA, n), 'wb').write(d)


def ossl(*a):
    subprocess.run(['openssl'] + list(a), cwd=CARPETA, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def raiz(n, cn):
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', n + '.key')
    ossl('req', '-x509', '-new', '-key', n + '.key', '-sha256', '-days', '3650', '-subj', '/O=Hispa-DOS pruebas/CN=' + cn,
         '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', n + '.pem')


def hoja(n, ca, nombre, rsa=False):
    if rsa:
        ossl('genrsa', '-out', n + '.key', '2048')
    else:
        ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', n + '.key')
    ossl('req', '-new', '-key', n + '.key', '-subj', '/CN=' + nombre, '-out', n + '.csr')
    open(os.path.join(CARPETA, n + '.ext'), 'w').write(
        'subjectAltName=DNS:%s%s\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\n' % (nombre, ',IP:' + nombre if nombre[0].isdigit() else ''))
    ossl('x509', '-req', '-in', n + '.csr', '-CA', ca + '.pem', '-CAkey', ca + '.key', '-CAcreateserial', '-days', '3650',
         '-sha256', '-extfile', n + '.ext', '-out', n + '.pem')


raiz('raiz', 'Raiz de prueba de Hispa-Pentium')
ossl('x509', '-in', 'raiz.pem', '-outform', 'der', '-out', 'RAIZ.CER')
raiz('otra', 'Raiz que no conoce nadie')
hoja('bueno', 'raiz', '10.0.2.2')
hoja('rsa', 'raiz', '10.0.2.2', rsa=True)
hoja('otro', 'raiz', 'otro.ejemplo')
hoja('extrano', 'otra', '10.0.2.2')

PUERTOS = {}


class Atiende(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def setup(self):
        try:
            if isinstance(self.request, ssl.SSLSocket):
                self.request.do_handshake()
        except (ssl.SSLError, OSError):
            pass
        super().setup()

    def manda(self, codigo, d, cab=()):
        self.send_response(codigo)
        for k, v in cab:
            self.send_header(k, v)
        self.send_header('Content-Length', str(len(d)))
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(d)

    def do_HEAD(self):
        self.manda(200, b'')

    def do_POST(self):
        self.do_GET()

    def do_GET(self):
        c = self.path.split('?')[0].lstrip('/')
        cuerpo = b''
        n = int(self.headers.get('Content-Length', '0') or 0)
        if n:
            cuerpo = self.rfile.read(n)
        if c == 'redirige':
            self.manda(302, b'', [('Location', '/ultima.txt')])
        elif c == 'lejos':
            self.manda(301, b'', [('Location', 'https://10.0.2.2:%d/ultima.txt' % PUERTOS['bueno'])])
        elif c == 'galleta':
            self.send_response(200)
            self.send_header('Set-Cookie', 'sesion=abc123; Path=/; HttpOnly')
            self.send_header('Set-Cookie', 'idioma=es; Path=/')
            d = b'galletas puestas\r\n'
            self.send_header('Content-Length', str(len(d)))
            self.end_headers()
            self.wfile.write(d)
        elif c == 'mira':
            self.manda(200, ('galletas=%s\r\n' % self.headers.get('Cookie', '')).encode())
        elif c == 'eco':
            x = ''.join('%s=%s\r\n' % (k.lower(), v) for k, v in self.headers.items() if k.lower().startswith('x-'))
            d = ('metodo=%s\r\nlargo=%d\r\n%stipo=%s\r\ncuerpo=' % (self.command, len(cuerpo), x, self.headers.get('Content-Type', ''))).encode() + cuerpo
            self.manda(200, d)
        elif c == 'trozos.bin':
            d = ARCHIVOS[c]
            self.send_response(200)
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            i, k = 0, 1
            while i < len(d):
                t = d[i:i + k]
                self.wfile.write(b'%x;ext=1\r\n' % len(t) + t + b'\r\n')
                i += len(t)
                k = k * 3 % 9001 + 1
            self.wfile.write(b'0\r\nX-Fin: si\r\n\r\n')
        elif c in ARCHIVOS:
            self.manda(200, ARCHIVOS[c])
        else:
            self.manda(404, b'no existe\r\n')


class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True


def lanza(nombre, cert):
    s = Servidor(('127.0.0.1', 0), Atiende)
    if cert:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(os.path.join(CARPETA, cert + '.pem'), os.path.join(CARPETA, cert + '.key'))
        s.socket = ctx.wrap_socket(s.socket, server_side=True, do_handshake_on_connect=False)
    PUERTOS[nombre] = s.server_address[1]
    threading.Thread(target=s.serve_forever, daemon=True).start()


for n in ('bueno', 'rsa', 'otro', 'extrano'):
    lanza(n, n)
lanza('hora', None)
open(os.path.join(CARPETA, 'puertos.tmp'), 'w').write(''.join('%s=%d\n' % kv for kv in PUERTOS.items()))
os.rename(os.path.join(CARPETA, 'puertos.tmp'), os.path.join(CARPETA, 'puertos'))
threading.Event().wait()
