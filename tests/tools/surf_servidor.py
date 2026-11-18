#!/usr/bin/env python3
"""Servidor de pruebas del Navegador completo de Hispa-486 (NAVEGA2.EXE,
NetSurf: tests/run_navegador2_tests.sh, docs/486/NAVEGADOR2.md): una página
«moderna» como las de hoy (hoja de estilo de fuera con variables de CSS,
botones a lo Bootstrap con variables por clase, flexbox, un degradado de
fondo, letras de varios tamaños, una imagen PNG y otra JPEG), un formulario
GET, una redirección y una página larga para desplazarse. Por HTTP y por
HTTPS (el certificado «bueno» de tests/tools/nav_certs.sh).

uso: surf_servidor.py CARPETA PUERTO
  CARPETA: donde están los certificados y donde se deja pedidos.log (una
           línea por petición: método, ruta y User-Agent)
  PUERTO   HTTP; PUERTO+1 HTTPS
Solo escucha en 127.0.0.1 (QEMU lo ve como 10.0.2.2).
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import os, ssl, struct, sys, threading, zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

D, P = sys.argv[1], int(sys.argv[2])

def png(w, h, color):
    raw = b''.join(b'\0' + bytes(color) * w for _ in range(h))
    def ch(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    return b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b'')

# un JPEG de 8x8 gris claro (de base, hecho a mano: lo justo para stb_image)
def jpeg_gris():
    q = bytes([1] * 64)
    soi = b'\xff\xd8'
    dqt = b'\xff\xdb' + struct.pack('>H', 67) + b'\x00' + q
    sof = b'\xff\xc0' + struct.pack('>HBHHB', 11, 8, 8, 8, 1) + b'\x01\x11\x00'
    # Huffman: DC con un solo código (categoría 0), AC con un solo código (EOB)
    dht_dc = b'\xff\xc4' + struct.pack('>H', 20) + b'\x00' + bytes([1] + [0] * 15) + b'\x00'
    dht_ac = b'\xff\xc4' + struct.pack('>H', 20) + b'\x10' + bytes([1] + [0] * 15) + b'\x00'
    sos = b'\xff\xda' + struct.pack('>HB', 8, 1) + b'\x01\x00' + b'\x00\x3f\x00'
    datos = b'\x3f'           # DC=0 (código «0»), EOB (código «0»), relleno de unos
    return soi + dqt + sof + dht_dc + dht_ac + sos + datos + b'\xff\xd9'

CSS = """
:root { --tinta: #1d1d1b; --fondo: #f4efe4; --marca: #2f5d62; --aviso: #c0392b; --hueco: 12px;
        --letra: "DejaVu Sans", sans-serif; }
body { margin: 0; background: var(--fondo); color: var(--tinta); font-family: var(--letra); }
.cab { background: linear-gradient(90deg, #2f5d62, #44777c); color: #fff; padding: var(--hueco) 20px; }
.cab h1 { margin: 0; font-size: 28px; }
.fila { display: flex; gap: var(--hueco); padding: var(--hueco) 20px; }
.ficha { flex: 1; background: #fff; border: 2px solid var(--marca); padding: 8px; }
.btn { display: inline-block; padding: 6px 14px; color: var(--bs-btn-color); background-color: var(--bs-btn-bg);
       border: 1px solid var(--bs-btn-bg); }
.btn-peligro { --bs-btn-color: #fff; --bs-btn-bg: var(--aviso); }
.btn-marca { --bs-btn-color: #fff; --bs-btn-bg: #2f5d62; }
.grande { font-size: 40px; font-weight: bold; color: var(--aviso); }
"""

INICIO = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>Prueba moderna</title>
<link rel="stylesheet" href="/estilo.css"></head><body>
<div class="cab"><h1>Biblioteca de prueba</h1></div>
<div class="fila"><div class="ficha"><img src="/verde.png" width="40" height="30" alt="verde"> Ficha uno</div>
<div class="ficha">Ficha dos <img src="/gris.jpg" width="16" height="16" alt="gris"></div>
<div class="ficha">Ficha tres</div></div>
<p style="padding: 0 20px"><a class="btn btn-marca" href="/larga">Página larga</a>
<a class="btn btn-peligro" href="/vuelve">Redirección</a></p>
<form action="/busca" method="get" style="padding: 0 20px"><input name="q" value=""> <button type="submit">Buscar</button></form>
<p class="grande" style="padding: 0 20px">Ñandú 25 €</p>
</body></html>"""

class Pedido(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass
    def manda(self, cuerpo, tipo='text/html; charset=utf-8', codigo=200, extra=()):
        if isinstance(cuerpo, str):
            cuerpo = cuerpo.encode('utf-8')
        self.send_response(codigo)
        self.send_header('Content-Type', tipo)
        self.send_header('Content-Length', str(len(cuerpo)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(cuerpo)
    def do_GET(self):
        u = urlparse(self.path)
        with open(os.path.join(D, 'pedidos.log'), 'a') as f:
            f.write('GET %s %s\n' % (self.path, self.headers.get('User-Agent', '')))
        if u.path == '/':
            self.manda(INICIO)
        elif u.path == '/estilo.css':
            self.manda(CSS, 'text/css')
        elif u.path == '/verde.png':
            self.manda(png(40, 30, (40, 160, 60)), 'image/png')
        elif u.path == '/gris.jpg':
            self.manda(jpeg_gris(), 'image/jpeg')
        elif u.path == '/vuelve':
            self.manda('', codigo=302, extra=[('Location', '/llegada')])
        elif u.path == '/llegada':
            self.manda('<!DOCTYPE html><title>Llegada</title><body style="background:#2f5d62;color:#fff"><h1>Has llegado</h1></body>')
        elif u.path == '/busca':
            q = parse_qs(u.query).get('q', [''])[0]
            self.manda('<!DOCTYPE html><meta charset="utf-8"><title>Busca</title><h1>Buscado: %s</h1>' % q.replace('<', '&lt;'))
        elif u.path == '/larga':
            filas = ''.join('<p style="background:%s;margin:0;height:40px">Fila %d</p>' % ('#c0392b' if i == 59 else '#ffffff', i) for i in range(60))
            self.manda('<!DOCTYPE html><meta charset="utf-8"><title>Larga</title><body style="margin:0">' + filas + '</body>')
        else:
            self.manda('<h1>No existe</h1>', codigo=404)

def sirve(puerto, cert=None):
    s = ThreadingHTTPServer(('127.0.0.1', puerto), Pedido)
    if cert:
        c = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        c.load_cert_chain(os.path.join(D, cert))
        s.socket = c.wrap_socket(s.socket, server_side=True)
    threading.Thread(target=s.serve_forever, daemon=True).start()

sirve(P)
sirve(P + 1, 'bueno.pem')
print('listo', flush=True)
threading.Event().wait()
