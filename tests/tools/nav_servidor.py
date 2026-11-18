#!/usr/bin/env python3
"""Servidor de pruebas del Navegador y de la red de los programas Hispa32
(tests/run_navegador_tests.sh, tests/run_red32_tests.sh): una «biblioteca» de
mentira, parecida a la de ClicGest (entrada con galleta de sesión, lista en
una tabla, préstamo y devolución con POST y redirección), por HTTP y HTTPS.

uso: nav_servidor.py CARPETA PUERTO
  CARPETA: donde están los certificados (los hace la prueba con openssl) y
           donde se deja pedidos.log (una línea por petición)
  PUERTO   HTTP sin cifrar
  PUERTO+1 HTTPS, certificado bueno (bueno.pem: EC, firmado por la raíz de pruebas)
  PUERTO+2 HTTPS, certificado de otro nombre (otro.pem)
  PUERTO+3 HTTPS, certificado que no firma nadie conocido (suelto.pem)
  PUERTO+4 HTTPS, certificado caducado (caducado.pem)
  PUERTO+5 HTTPS, certificado RSA bueno (rsa.pem)
Solo escucha en 127.0.0.1 (QEMU lo ve como 10.0.2.2).
Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""
import os, random, ssl, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import nav_js    # [JS] tests/tools/nav_js.py

CARPETA, PUERTO = sys.argv[1], int(sys.argv[2])
FICHA = 'abc123'
SESION = 'S3S10N-de-prueba'
SOCIOS = {1: 'Pepe Formación', 2: 'María García', 3: 'Antonia Núñez'}
LIBROS = {1: ('El niño y la cigüeña', 'Rosa Peña'), 2: ('Canción «del» mar', 'Lluís Martí'),
          3: ('Recetas de la abuela', 'Pilar Esteve'), 4: ('Paseos por la sierra', 'Joaquín Sala')}
prestamos = {}          # libro -> (socio, días, nota)
aviso = ['']
cerrojo = threading.Lock()
random.seed(7)
BIN = bytes(random.getrandbits(8) for _ in range(150000))

CAB = ('<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8"><title>%s</title>'
       '<style>body{font-family:sans-serif}.oculto{display:none}.aviso{background:#fff3cd;color:#333}'
       '.fila{display:block}@media print{.pantalla{display:none}}</style>'
       '<script>var x = "<p>NO-DEBE-VERSE-4</p>";</script></head><body>\n')
PIE = '\n<p class="pantalla"><small>Servidor de pruebas de Hispa-486.</small></p></body></html>'


def pagina(titulo, cuerpo):
    return (CAB % titulo + cuerpo + PIE).encode('utf-8')


class Pedido(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'

    def log_message(self, *a):
        pass

    def apunta(self, cuerpo=''):
        with cerrojo, open(os.path.join(CARPETA, 'pedidos.log'), 'a', encoding='utf-8') as f:
            f.write('%s %s | %s | %s | %s | %s\n' % (self.command, self.path, self.headers.get('Cookie', '-'), cuerpo,
                                                     self.headers.get('User-Agent', '-'), self.headers.get('Accept-Language', '-')))

    def manda(self, datos, tipo='text/html; charset=UTF-8', estado=200, cab=(), trozos=False):
        self.send_response(estado)
        self.send_header('Content-Type', tipo)
        for k, v in cab:
            self.send_header(k, v)
        if trozos:      # a trozos (Transfer-Encoding: chunked), como da PHP el HTML
            self.send_header('Transfer-Encoding', 'chunked')
            self.end_headers()
            for i in range(0, len(datos), 700):
                t = datos[i:i + 700]
                self.wfile.write(b'%x\r\n' % len(t) + t + b'\r\n')
            self.wfile.write(b'0\r\n\r\n')
        else:
            self.send_header('Content-Length', str(len(datos)))
            self.end_headers()
            self.wfile.write(datos)

    def redirige(self, a, estado=302, cab=()):
        self.manda(b'', estado=estado, cab=(('Location', a),) + tuple(cab))

    def con_sesion(self):
        return ('SESION=' + SESION) in self.headers.get('Cookie', '')

    def do_GET(self):
        u = urlsplit(self.path)
        q = parse_qs(u.query)
        self.apunta()
        if u.path == '/':
            err = '<p class="aviso">Usuario o clave incorrectos.</p>' if 'error' in q else ''
            self.manda(pagina('Entrada - Biblioteca de pruebas',
                '<h1>Biblioteca de pruebas</h1>' + err + '<p>Escriba su usuario y su clave.</p>'
                '<form method="POST" action="/entrar"><input type="hidden" name="ficha" value="%s">'
                '<p><label for="u">Usuario</label> <input type="text" id="u" name="usuario" size="20" required></p>'
                '<p><label for="c">Clave</label> <input type="password" id="c" name="clave" size="20"></p>'
                '<p><input type="checkbox" name="recordar" value="1" checked> Recordarme</p>'
                '<p><button type="submit">Entrar</button></p></form>' % FICHA))
        elif u.path == '/biblioteca':
            if not self.con_sesion():
                return self.redirige('/?error=sesion')
            with cerrojo:
                av, aviso[0] = aviso[0], ''
                filas = ''.join('<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td></tr>' % (
                    n, t, a, ('Prestado a ' + SOCIOS[prestamos[n][0]]) if n in prestamos else 'En la biblioteca')
                    for n, (t, a) in LIBROS.items())
            self.manda(pagina('Libros - Biblioteca de pruebas',
                '<nav><a href="/biblioteca">Libros</a> <a href="/prestar">Prestar un libro</a> '
                '<a href="/devolver">Devolver</a></nav><h1>Libros</h1>'
                + ('<div class="aviso">%s</div>' % av if av else '') +
                '<div style="display:none">NO-DEBE-VERSE-1</div><p hidden>NO-DEBE-VERSE-2</p>'
                '<div class="d-none">NO-DEBE-VERSE-3</div><span class="oculto">NO-DEBE-VERSE-5</span>'
                '<table class="table"><thead><tr><th>N&ordm;</th><th>T&iacute;tulo</th><th>Autor</th><th>Estado</th></tr></thead>'
                '<tbody>' + filas + '</tbody></table>'
                '<details><summary>M&aacute;s datos</summary><p>Texto plegado: 4 libros &middot; &euro; &#8212; fin.</p></details>'
                '<ul><li>Uno</li><li>Dos <b>en negrita</b></li></ul>'), trozos=True)
        elif u.path == '/prestar':
            if not self.con_sesion():
                return self.redirige('/?error=sesion')
            if 'socio' in q:
                s = int(q['socio'][0])
                with cerrojo:
                    ops = ''.join('<option value="%d">%s</option>' % (n, t) for n, (t, a) in LIBROS.items() if n not in prestamos)
                return self.manda(pagina('Prestar - Biblioteca de pruebas',
                    '<h1>Prestar un libro</h1><p>Para %s</p><form method="post" action="/prestar">'
                    '<input type="hidden" name="ficha" value="%s"><input type="hidden" name="socio_id" value="%d">'
                    '<input type="hidden" name="marca" value="a&ntilde;o «€»">'
                    '<p><label>Libro</label> <select name="libro">%s</select></p>'
                    '<p><label>Nota</label><br><textarea name="nota" rows="3" cols="40"></textarea></p>'
                    '<p><input type="radio" name="dias" value="15"> 15 d&iacute;as '
                    '<input type="radio" name="dias" value="30" checked> 30 d&iacute;as</p>'
                    '<p><input type="checkbox" name="avisar" value="si"> Avisar al socio</p>'
                    '<p><button type="submit" name="accion" value="prestar">Prestar</button> '
                    '<button type="button" onclick="alert(1)">Solo JavaScript</button></p></form>' % (SOCIOS.get(s, '?'), FICHA, s, ops)))
            lista = ''
            if 'qs' in q:
                t = q['qs'][0].lower()
                lista = ''.join('<a class="fila" href="/prestar?socio=%d">%s &middot; n&ordm; %d</a>' % (n, nom, n)
                                for n, nom in SOCIOS.items() if t in nom.lower())
                lista = '<h3>Socios</h3>' + (lista or '<p>Nadie con ese nombre.</p>')
            self.manda(pagina('Prestar - Biblioteca de pruebas',
                '<h1>Prestar un libro</h1><form method="get" action="/prestar"><p>Nombre del socio '
                '<input type="search" name="qs" placeholder="Nombre o apellido"> <button type="submit">Buscar</button></p></form>' + lista))
        elif u.path == '/devolver':
            if not self.con_sesion():
                return self.redirige('/?error=sesion')
            with cerrojo:
                av, aviso[0] = aviso[0], ''
                fs = ''.join('<li>%s (%s) <form method="post" action="/devolver" onsubmit="return confirm(\'&iquest;Seguro que lo han devuelto?\')">'
                             '<input type="hidden" name="ficha" value="%s"><input type="hidden" name="libro" value="%d">'
                             '<select name="estado"><option value="bien" selected>Bien</option><option value="desgaste">Con desgaste</option></select> '
                             '<button type="submit">Devuelto</button></form></li>' % (LIBROS[n][0], SOCIOS[p[0]], FICHA, n)
                             for n, p in prestamos.items())
            self.manda(pagina('Devolver - Biblioteca de pruebas', '<h1>Devolver</h1>'
                + ('<div class="aviso">%s</div>' % av if av else '')
                + ('<ul>' + fs + '</ul>' if fs else '<p>No hay libros fuera.</p>') + '<p><a href="/biblioteca">Volver a los libros</a></p>'))
        elif u.path == '/enlaces':
            s = 'https://10.0.2.2:%d'
            ls = ((s % (PUERTO + 2) + '/', 'De otro nombre'), (s % (PUERTO + 3) + '/', 'Sin autoridad conocida'),
                  (s % (PUERTO + 4) + '/', 'Caducado'), (s % (PUERTO + 5) + '/texto.txt', 'Texto por RSA'),
                  ('/latin1', 'En Latin-1'), ('/grande', 'Grande'), ('/redir', 'Con redirecciones'),
                  ('/cierra', 'Sin longitud'), ('/noesta', 'No existe'), ('/archivo.bin', 'Un archivo'),
                  ('javascript:alert(1)', 'Solo JavaScript'))
            self.manda(pagina('Enlaces de prueba', ''.join('<p><a href="%s">%s</a></p>' % l for l in ls)))
        elif u.path == '/redir':
            self.redirige('/redir2')
        elif u.path == '/redir2':
            self.redirige('http://10.0.2.2:%d/final' % PUERTO, estado=301)
        elif u.path == '/final':
            self.manda(pagina('Final', '<p>Llegada tras dos redirecciones.</p>'))
        elif u.path == '/latin1':
            self.manda('<html><head><title>Página en Latin-1</title></head><body><p>Año, cigüeña y camión en ISO-8859-1.</p></body></html>'
                       .encode('latin-1'), tipo='text/html; charset=ISO-8859-1')
        elif u.path == '/grande':
            c = ''.join('<p>P&aacute;rrafo n&uacute;mero %d de la p&aacute;gina grande, con texto bastante para ocupar una l&iacute;nea '
                        'entera y algo m&aacute;s de la ventana del navegador.</p>' % n
                        for n in range(1, min(int(q.get('n', ['400'])[0]), 3000) + 1))
            self.manda(pagina('Página grande', '<h1>P&aacute;gina grande</h1>' + c + '<p id="fin">FIN-DE-LA-GRANDE</p>'), trozos=True)
        elif u.path == '/texto.txt':
            self.manda('Texto sin formato.\nSegunda línea: ñandú.\n'.encode('utf-8'), tipo='text/plain; charset=UTF-8')
        elif u.path == '/archivo.bin':
            self.manda(BIN, tipo='application/octet-stream')
        elif u.path == '/cierra':       # sin longitud: hasta que se cierra la conexión
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=UTF-8')
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(pagina('Sin longitud', '<p>Hasta el cierre de la conexi&oacute;n.</p>'))
            self.close_connection = True
        elif u.path.startswith('/js') and nav_js.atiende(self, u, q):    # [JS] las páginas con JavaScript
            pass
        else:
            self.manda(pagina('No existe', '<h1>404</h1><p>Aqu&iacute; no hay nada.</p>'), estado=404)

    def do_POST(self):
        n = int(self.headers.get('Content-Length', '0'))
        cuerpo = self.rfile.read(n).decode('ascii', 'replace')
        q = parse_qs(cuerpo, keep_blank_values=True)
        self.apunta(cuerpo)
        v = lambda k: q.get(k, [''])[0]
        if self.path.startswith('/js') and nav_js.atiende_post(self, self.path, cuerpo):    # [JS]
            return
        if self.path == '/entrar':
            if v('usuario') == 'maria' and v('clave') == 'secreto' and v('ficha') == FICHA:
                seguro = '; Secure' if isinstance(self.connection, ssl.SSLSocket) else ''
                return self.redirige('/biblioteca', cab=(('Set-Cookie', 'SESION=%s; Path=/; HttpOnly; SameSite=Lax%s' % (SESION, seguro)),
                                                         ('Set-Cookie', 'vieja=1; Path=/; Max-Age=0')))
            return self.redirige('/?error=1')
        if not self.con_sesion() or v('ficha') != FICHA:
            return self.redirige('/?error=sesion')
        if self.path == '/prestar':
            with cerrojo:
                lib, soc = int(v('libro') or 0), int(v('socio_id') or 0)
                if lib in LIBROS and soc in SOCIOS and lib not in prestamos:
                    prestamos[lib] = (soc, v('dias'), v('nota'))
                    aviso[0] = 'Hecho: prestado «%s» a %s (%s días%s). Nota: %s' % (
                        LIBROS[lib][0], SOCIOS[soc], v('dias'), ', con aviso' if v('avisar') == 'si' else '', v('nota') or '-')
                else:
                    aviso[0] = 'No se ha podido prestar.'
            return self.redirige('/biblioteca', estado=303)
        if self.path == '/devolver':
            with cerrojo:
                lib = int(v('libro') or 0)
                if lib in prestamos:
                    del prestamos[lib]
                    aviso[0] = 'Hecho: devuelto «%s» (%s).' % (LIBROS[lib][0], v('estado'))
            return self.redirige('/devolver')
        self.manda(b'', estado=404)


def sirve(puerto, cert=None):
    s = ThreadingHTTPServer(('127.0.0.1', puerto), Pedido)
    s.daemon_threads = True
    if cert:
        c = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        c.load_cert_chain(os.path.join(CARPETA, cert))
        if cert == 'rsa.pem':   # (el otro cifrado; los demás puertos eligen AES-256-GCM)
            c.set_ciphers('ECDHE-RSA-CHACHA20-POLY1305')
        s.socket = c.wrap_socket(s.socket, server_side=True)
    threading.Thread(target=s.serve_forever, daemon=True).start()


sirve(PUERTO)
for k, c in enumerate(('bueno.pem', 'otro.pem', 'suelto.pem', 'caducado.pem', 'rsa.pem')):
    if os.path.exists(os.path.join(CARPETA, c)):
        sirve(PUERTO + 1 + k, c)
print('listo', flush=True)
threading.Event().wait()
