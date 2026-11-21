#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/navega/servidor_pruebas.py CARPETA

El sitio de prueba del Navegador de Hispa-Pentium (NAVEGA.BIN), para
castel/pruebas.sh («el Navegador») sin depender de internet: el mismo sitio
por HTTP y por HTTPS. Hace con openssl, en CARPETA, una autoridad raíz de
prueba (RAIZ.CER, en DER: va al disco de prueba, en \\RAICES, y la conoce
TLS.BIN) y el certificado ECDSA de «10.0.2.2» (desde QEMU con «-netdev user»,
esta máquina); escucha en 127.0.0.1 y deja los puertos en CARPETA/puertos
(«http=...» y «https=...»). Apunta cada pedido en CARPETA/pedidos.txt (una
línea: método, camino, User-Agent, Cookie, X-Prueba y el cuerpo), para ver
lo que manda el Navegador.

Lo que sirve:
  /              la página: una hoja de fuera (/estilo.css, con variables),
                 un PNG y un JPEG, un formulario POST, un enlace, una caja que
                 pinta el JavaScript, otra que pinta tras setTimeout, otra
                 tras un fetch (si le llegan las dos galletas: la del servidor
                 y la de document.cookie) y una página larga (para la rueda)
  /estilo.css    las reglas (colores en variables de CSS, flexbox, esquinas)
  /rojo.png, /azul.jpg   las imágenes (de un color, 80 x 60)
  /api           (fetch) {"galleta": "si"} si le llegan sesion=abc y desdejs=1
  /eco           (POST) la página con lo que se ha mandado
  /pagina2.html  otra página (un bloque de color)
  /fijo.html     position: fixed (cabecera y barra lateral, con un enlace
                 abajo) y sticky, en una página larga
  /descarga      un archivo de 50.000 octetos «Informe anual 2026.zip»
                 (Content-Disposition: attachment)
Lo pone en marcha pruebas.sh y lo para por su PID.
"""
import http.server
import json
import os
import random
import socketserver
import ssl
import subprocess
import sys
import threading
import urllib.parse
import zlib
import struct

CARPETA = sys.argv[1]
rnd = random.Random(20261121)
DESCARGA = bytes(rnd.getrandbits(8) for _ in range(50000))


def png(r, g, b, an=80, al=60):
    fila = b'\0' + bytes([r, g, b]) * an
    def trozo(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', an, al, 8, 2, 0, 0, 0))
            + trozo(b'IDAT', zlib.compress(fila * al)) + trozo(b'IEND', b''))


def jpeg(color, an=80, al=60):
    """un JPEG de un color: con ImageMagick (convert), que está en el VPS"""
    r = subprocess.run(['convert', '-size', '%dx%d' % (an, al), 'xc:#%02x%02x%02x' % color, '-quality', '95', 'jpeg:-'],
                       capture_output=True, check=True)
    return r.stdout


ROJO = png(0xD0, 0x20, 0x20)
AZUL = jpeg((0x20, 0x40, 0xD0))

ESTILO = b""":root { --verde: #2e8b57; --borde: #14294a; }
body { margin: 0; font-family: sans-serif; background: #ffffff; color: #14294a; }
#cabecera { background: var(--verde); color: #fff; height: 60px; font-size: 28px; padding: 0 0 0 16px; }
.fila { display: flex; gap: 10px; padding: 10px; }
.caja { width: 90px; height: 60px; border-radius: 8px; border: 2px solid var(--borde); }
#caja-js { background: #808080; }
#caja-tiempo { background: #808080; }
#caja-fetch { background: #808080; }
form { padding: 0 10px; }
input[type=text] { width: 200px; height: 28px; }
#largo { height: 3000px; background: #f8f8ff; }
#pie { height: 80px; background: #8a2be2; color: #fff; }
"""

INDICE = """<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><title>Prueba del Navegador</title>
<link rel="stylesheet" href="/estilo.css"></head>
<body>
<div id="cabecera">Prueba del Navegador</div>
<div class="fila">
<div class="caja" id="caja-js"></div><div class="caja" id="caja-tiempo"></div><div class="caja" id="caja-fetch"></div>
<img src="/rojo.png" width="80" height="60" alt="rojo"><img src="/azul.jpg" width="80" height="60" alt="azul">
</div>
<form action="/eco" method="post"><input type="text" name="nombre" id="nombre"> <input type="submit" value="Enviar"></form>
<p><a id="enlace" href="/pagina2.html">La segunda página</a> · <a href="/descarga">El informe</a></p>
<div id="largo">Una página larga, para la rueda del ratón.</div>
<div id="pie">El pie de la página</div>
<script>
document.cookie = 'desdejs=1; path=/';
var c = document.getElementById('caja-js');
c.style.backgroundColor = '#c71585';
c.textContent = 'JS';
setTimeout(function () { document.getElementById('caja-tiempo').style.backgroundColor = '#1e90ff'; }, 200);
var n = parseInt(localStorage.getItem('visitas') || '0', 10) + 1;
localStorage.setItem('visitas', String(n));
fetch('/api?visitas=' + n, { headers: { 'X-Prueba': 'fetch' } }).then(r => r.json()).then(j => {
  if (j.galleta === 'si') document.getElementById('caja-fetch').style.backgroundColor = '#ff8c00';
});
</script>
</body></html>
""".encode()

# position: fixed y sticky: una cabecera fija (roja), una barra lateral fija
# como la de ClicGest (azul: height 100vh, overflow-y auto, z-index 100), con un
# enlace abajo del todo, y un bloque sticky (verde) en el contenido, que es largo
FIJO = """<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><title>Fijos</title><style>
body { margin: 0; font-family: sans-serif; }
#cabecera { position: fixed; top: 0; left: 0; right: 0; height: 40px; background: #e03030; color: #fff; z-index: 10; }
.lateral { position: fixed; top: 0; left: 0; width: 120px; height: 100vh; overflow-y: auto; background: #305080; color: #fff; z-index: 100; }
.lateral a { color: #fff; display: block; padding: 8px; }
#abajo { position: absolute; bottom: 10px; left: 0; }
main { margin-left: 120px; padding-top: 40px; }
#pegado { position: sticky; top: 40px; height: 30px; background: #20a060; }
.banda { height: 200px; }
.par { background: #f0f0c0; } .impar { background: #c0d8f0; }
</style></head><body>
<div id="cabecera">Cabecera fija</div>
<aside class="lateral"><a href="/pagina2.html">Uno</a><a href="/pagina2.html">Dos</a><a id="abajo" href="/eco?lateral">Abajo</a></aside>
<main><div class="banda par">arriba</div><div id="pegado">sticky</div>
""" + ''.join('<div class="banda %s">banda %d</div>' % ('par' if k % 2 else 'impar', k) for k in range(15)) + """
</main></body></html>
"""

PAGINA2 = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>La segunda</title></head>
<body style="margin:0"><div style="background:#008b8b;height:200px;color:#fff;font-size:30px">La segunda página</div></body></html>
""".encode()


class Pedidos(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def apunta(self, cuerpo=b''):
        with open(os.path.join(CARPETA, 'pedidos.txt'), 'a', encoding='utf-8') as f:
            f.write('%s %s | %s | %s | %s | %s\n' % (self.command, self.path, self.headers.get('User-Agent', ''),
                                                    self.headers.get('Cookie', ''), self.headers.get('X-Prueba', ''),
                                                    cuerpo.decode('utf-8', 'replace')))

    def da(self, codigo, tipo, datos, mas=()):
        self.send_response(codigo)
        self.send_header('Content-Type', tipo)
        self.send_header('Content-Length', str(len(datos)))
        for k, v in mas:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(datos)

    def do_GET(self):
        self.apunta()
        camino = self.path.split('?')[0]
        if camino == '/':
            self.da(200, 'text/html; charset=utf-8', INDICE, [('Set-Cookie', 'sesion=abc; Path=/; HttpOnly'),
                                                               ('Set-Cookie', 'recuerda=1; Path=/; Max-Age=86400')])
        elif camino == '/estilo.css':
            self.da(200, 'text/css', ESTILO)
        elif camino == '/rojo.png':
            self.da(200, 'image/png', ROJO)
        elif camino == '/azul.jpg':
            self.da(200, 'image/jpeg', AZUL)
        elif camino == '/api':
            c = self.headers.get('Cookie', '')
            si = 'sesion=abc' in c and 'desdejs=1' in c
            self.da(200, 'application/json', json.dumps({'galleta': 'si' if si else 'no'}).encode())
        elif camino == '/fijo.html':
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            pag = FIJO
            if 'y' in q:            # (para comparar con Chrome: ya desplazada)
                pag = pag.replace('</body>', '<script>window.scrollTo(0, %d)</script></body>' % int(q['y'][0]))
            self.da(200, 'text/html; charset=utf-8', pag.encode())
        elif camino == '/pagina2.html':
            self.da(200, 'text/html; charset=utf-8', PAGINA2)
        elif camino == '/descarga':
            self.da(200, 'application/zip', DESCARGA, [('Content-Disposition', 'attachment; filename="Informe anual 2026.zip"')])
        else:
            self.da(404, 'text/plain', b'no esta\n')

    def do_POST(self):
        n = int(self.headers.get('Content-Length', '0'))
        cuerpo = self.rfile.read(n)
        self.apunta(cuerpo)
        if self.path == '/eco':
            campos = urllib.parse.parse_qs(cuerpo.decode())
            nombre = campos.get('nombre', [''])[0]
            t = ('<!DOCTYPE html><html><head><meta charset="utf-8"><title>Eco</title></head><body style="margin:0">'
                 '<div style="background:#daa520;height:120px;font-size:24px">Hola, %s</div></body></html>' % nombre)
            self.da(200, 'text/html; charset=utf-8', t.encode())
        else:
            self.da(404, 'text/plain', b'no esta\n')


class Servidor(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def openssl(*a):
    subprocess.run(['openssl'] + list(a), cwd=CARPETA, check=True, capture_output=True)


def certificados():
    openssl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'raiz.key')
    openssl('req', '-x509', '-new', '-key', 'raiz.key', '-days', '3650', '-subj', '/CN=Raiz de prueba del Navegador',
            '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', 'raiz.pem')
    openssl('x509', '-in', 'raiz.pem', '-outform', 'DER', '-out', 'RAIZ.CER')
    openssl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'srv.key')
    openssl('req', '-new', '-key', 'srv.key', '-subj', '/CN=10.0.2.2', '-out', 'srv.csr')
    with open(os.path.join(CARPETA, 'ext.cnf'), 'w') as f:
        f.write('subjectAltName=DNS:10.0.2.2,IP:10.0.2.2\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n')
    openssl('x509', '-req', '-in', 'srv.csr', '-CA', 'raiz.pem', '-CAkey', 'raiz.key', '-CAcreateserial', '-days', '825',
            '-extfile', 'ext.cnf', '-out', 'srv.pem')


def principal():
    os.makedirs(CARPETA, exist_ok=True)
    with open(os.path.join(CARPETA, 'descarga.bin'), 'wb') as f:
        f.write(DESCARGA)                       # (para comparar lo descargado)
    certificados()
    http_s = Servidor(('127.0.0.1', 0), Pedidos)
    https_s = Servidor(('127.0.0.1', 0), Pedidos)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.path.join(CARPETA, 'srv.pem'), os.path.join(CARPETA, 'srv.key'))
    https_s.socket = ctx.wrap_socket(https_s.socket, server_side=True)
    for s in (http_s, https_s):
        threading.Thread(target=s.serve_forever, daemon=True).start()
    with open(os.path.join(CARPETA, 'puertos.tmp'), 'w') as f:
        f.write('http=%d\nhttps=%d\n' % (http_s.server_address[1], https_s.server_address[1]))
    os.rename(os.path.join(CARPETA, 'puertos.tmp'), os.path.join(CARPETA, 'puertos'))
    threading.Event().wait()


if __name__ == '__main__':
    principal()
