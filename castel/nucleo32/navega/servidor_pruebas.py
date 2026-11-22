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
  /panel         (navega 0.2.2) como el modo teléfono de ClicGest: con la
                 galleta vista=movil (o, sin galleta, un User-Agent con
                 «Mobile»), un guion pone la clase «cg-movil» en <html> y la
                 hoja manda: la barra de abajo fija (bottom: 0) con «Más», que
                 abre con showModal() un <dialog> pegado abajo (inset: auto 0
                 0 0; max-height: 86dvh; overflow-y: auto) con un <details>
                 cerrado y, al final, «Ver versión de ordenador»; si no, la de
                 ordenador, con «Ver versión móvil»
  /vista?modo=movil|pc&volver=/panel   pone la galleta vista (un año,
                 HttpOnly) y vuelve con un 302, como /vista de ClicGest
  /portada       como la cabecera de clicgest.es con menos de 1200 puntos de
                 ancho (navbar-expand-xl de Bootstrap): el menú plegado
                 (.collapse sin .show: display: none), el botón de las tres
                 rayas, que es un SVG en un data: (navbar-toggler-icon: sin
                 SVG no se veía), y dentro «Acceder» (a /login)
  /login         una página de un color
  /svg.html      imágenes SVG: un <img> de /icono.svg (un cuadrado relleno
                 de rgb(200, 30, 120) en un viewBox) y otro en un data: (tres
                 rayas con stroke en rgba(), como el de Bootstrap), a 90 x 90
  /ua            una página que dice (con el color) si el User-Agent de la
                 petición y el de navigator.userAgent llevan «Mobile»
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

# (navega 0.2.2) el modo teléfono de ClicGest, con lo que lo hace difícil:
# la barra de abajo y la hoja «Más» son position: fixed con bottom: 0 (NetSurf
# las ponía al final de la página: no se veían), la hoja con inset (que libcss
# no conocía), max-height en dvh y overflow-y: auto, un <details> cerrado con
# display: grid dentro (salía desplegado) y la salida al final de la hoja
PANEL_MOVIL = """<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><title>Panel (movil)</title>
<script>document.documentElement.className += ' cg-movil cg-vista-fijada';</script><style>
body { margin: 0; font-family: sans-serif; background: #f4f6f8; }
.cabecera { background: #0f172a; color: #fff; height: 50px; font-size: 22px; }
.relleno { height: 220px; margin: 10px; background: #dfe6ee; }
html.cg-movil body { padding-bottom: 60px; }
html.cg-movil .pestanas { position: fixed; left: 0; right: 0; bottom: 0; z-index: 1030; display: flex; background: #0b6e4f; height: 50px; }
html.cg-movil .pestana { flex: 1 1 0; color: #fff; text-align: center; padding-top: 14px; border: 0; background: none; font-size: 16px; }
html.cg-movil #mas { background: #f59e0b; color: #000; }
html.cg-movil dialog.hoja { position: fixed; inset: auto 0 0 0; margin: 0; width: 100%; max-width: 100%; max-height: 86dvh;
  padding: 0; border: 0; background: #fff3c4; overflow-y: auto; z-index: 1045; }
html.cg-movil dialog.hoja:not([open]) { display: none; }
html.cg-movil .lista { display: grid; grid-template-columns: 1fr 1fr; }
html.cg-movil .item { display: flex; min-height: 52px; align-items: center; padding: 0 12px; color: #14294a; }
html.cg-movil .plegado .item { background: #ffd0d0; }
html.cg-movil #ordenador { background: #c0306a; color: #fff; min-height: 60px; }
</style></head><body>
<div class="cabecera">Panel (version movil)</div>
""" + ''.join('<div class="relleno">bloque %d</div>' % k for k in range(8)) + """
<nav class="pestanas"><a class="pestana" href="/panel">Inicio</a><a class="pestana" href="/panel">Socios</a>
<button type="button" class="pestana" id="mas">Mas</button></nav>
<dialog class="hoja" id="hoja">
<div class="lista">""" + ''.join('<a class="item" href="/panel">Seccion %d</a>' % k for k in range(12)) + """</div>
<details class="plegado"><summary class="item">Papeles, banco y mas</summary>
<div class="lista">""" + ''.join('<a class="item" href="/panel">Escondida %d</a>' % k for k in range(10)) + """</div></details>
<div class="lista"><a class="item" href="/panel">Ajustes</a><a class="item" href="/panel">Seguridad</a></div>
<a class="item" id="ordenador" href="/vista?modo=pc&amp;volver=/panel">Ver version de ordenador</a>
</dialog>
<script>
var hoja = document.getElementById('hoja');
document.getElementById('mas').addEventListener('click', function (e) {
  e.preventDefault();
  if (typeof hoja.showModal === 'function') hoja.showModal(); else hoja.setAttribute('open', '');
});
</script>
</body></html>
"""

PANEL_PC = """<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><title>Panel (ordenador)</title><style>
body { margin: 0; font-family: sans-serif; }
.cabecera { background: #1f4e79; color: #fff; height: 60px; font-size: 24px; }
#movil { display: block; background: #2e8b57; color: #fff; height: 50px; width: 300px; font-size: 18px; }
</style></head><body>
<div class="cabecera">Panel (version de ordenador)</div>
<a id="movil" href="/vista?modo=movil&amp;volver=/panel">Ver version movil</a>
</body></html>
"""

PORTADA = """<!DOCTYPE html>
<html lang="es"><head><meta charset="utf-8"><title>Portada</title><style>
body { margin: 0; font-family: sans-serif; }
.nav { position: fixed; top: 0; left: 0; right: 0; background: #fff; z-index: 1030; border-bottom: 1px solid #ccc; }
.barra { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; padding: 8px 16px; }
.marca { font-size: 22px; color: #166534; }
.toggler { padding: 4px 12px; background: transparent; border: 0; }
.toggler-icon { display: inline-block; width: 1.5em; height: 1.5em; vertical-align: middle; background-repeat: no-repeat;
  background-position: center; background-size: 100%;
  background-image: url("data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 30'%3e%3cpath stroke='rgba%2833, 37, 41, 0.75%29' stroke-linecap='round' stroke-miterlimit='10' stroke-width='2' d='M4 7h22M4 15h22M4 23h22'/%3e%3c/svg%3e"); }
.collapse:not(.show) { display: none; }
.menu { flex-basis: 100%; }
.menu a { display: block; padding: 8px 0; color: #14294a; }
#acceder { display: inline-block; background: #1f6feb; color: #fff; padding: 10px 20px; }
.heroe { margin-top: 60px; height: 900px; background: #0b1020; color: #fff; font-size: 30px; }
</style></head><body>
<nav class="nav"><div class="barra"><a class="marca" href="/portada">ClicGest</a>
<button class="toggler" type="button" id="toggler" aria-label="Abrir el menu"><span class="toggler-icon"></span></button>
<div class="collapse menu" id="navmenu"><a href="#funciones">Funciones</a><a href="#precios">Precios</a>
<a id="acceder" href="/login">Acceder</a></div></div></nav>
<section class="heroe">El software todo en uno para tu asociacion</section>
<script>
document.getElementById('toggler').addEventListener('click', function () {
  document.getElementById('navmenu').classList.toggle('show');
});
</script>
</body></html>
"""

LOGIN = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>Entrar</title></head>
<body style="margin:0"><div style="background:#6a5acd;height:200px;color:#fff;font-size:30px">Entrar</div></body></html>
"""

ICONO_SVG = (b"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 30'>"
             b"<rect x='3' y='3' width='24' height='24' fill='rgb(200, 30, 120)'/></svg>")

SVG_PAGINA = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>SVG</title></head>
<body style="margin:0;background:#fff"><img src="/icono.svg" width="90" height="90" alt="cuadrado">
<img width="90" height="90" alt="rayas" src="data:image/svg+xml,%3csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 30 30'%3e%3cpath stroke='rgba%2820, 40, 200, 0.9%29' stroke-linecap='round' stroke-width='4' d='M4 7h22M4 15h22M4 23h22'/%3e%3c/svg%3e">
</body></html>
"""

UA_PAGINA = """<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><title>User-Agent</title></head>
<body style="margin:0"><div style="background:%s;height:120px;color:#fff;font-size:24px">%s</div>
<div id="js" style="background:#808080;height:80px"></div>
<script>document.getElementById('js').style.backgroundColor = /Mobile/.test(navigator.userAgent) ? '#c71585' : '#1e90ff';</script>
</body></html>
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
        elif camino == '/panel':
            galleta = self.headers.get('Cookie', '')
            vista = ''
            for trozo in galleta.split(';'):
                k, _, v = trozo.strip().partition('=')
                if k == 'vista':
                    vista = v
            if vista not in ('movil', 'pc'):
                vista = 'movil' if 'Mobile' in self.headers.get('User-Agent', '') else 'pc'
            self.da(200, 'text/html; charset=utf-8', (PANEL_MOVIL if vista == 'movil' else PANEL_PC).encode(),
                    [('Cache-Control', 'no-store')])
        elif camino == '/vista':
            q = urllib.parse.parse_qs(urllib.parse.urlparse(self.path).query)
            modo = q.get('modo', [''])[0]
            volver = q.get('volver', ['/panel'])[0]
            if not volver.startswith('/') or volver.startswith('//'):
                volver = '/panel'
            if modo in ('movil', 'pc'):
                g = 'vista=%s; Path=/; Max-Age=31536000; HttpOnly; SameSite=Lax' % modo
            else:
                g = 'vista=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax'
            self.da(302, 'text/plain', b'', [('Set-Cookie', g), ('Location', volver)])
        elif camino == '/portada':
            self.da(200, 'text/html; charset=utf-8', PORTADA.encode())
        elif camino == '/login':
            self.da(200, 'text/html; charset=utf-8', LOGIN.encode())
        elif camino == '/svg.html':
            self.da(200, 'text/html; charset=utf-8', SVG_PAGINA.encode())
        elif camino == '/icono.svg':
            self.da(200, 'image/svg+xml', ICONO_SVG)
        elif camino == '/ua':
            movil = 'Mobile' in self.headers.get('User-Agent', '')
            self.da(200, 'text/html; charset=utf-8',
                    (UA_PAGINA % (('#daa520', 'movil') if movil else ('#008b8b', 'escritorio'))).encode(),
                    [('Cache-Control', 'no-store')])
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
