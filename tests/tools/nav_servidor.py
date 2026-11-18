#!/usr/bin/env python3
"""Servidor de pruebas del Navegador y de la red de los programas Hispa32
(tests/run_navegador_tests.sh, tests/run_red32_tests.sh): una «biblioteca» de
mentira, parecida a la de ClicGest (entrada con galleta de sesión, lista en
una tabla, préstamo y devolución con POST y redirección), por HTTP y HTTPS.
Y lo que se añadió para usar ClicGest entero: la galleta de «recordarme»
(que se guarda en el disco: Max-Age y Expires), imágenes (PNG, GIF, JPEG,
data:, una rota y un SVG que no se puede ver), subir un archivo
(multipart/form-data, con required y pattern), una descarga (un PDF con
Content-Disposition) y <meta http-equiv=refresh>.

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
import base64, binascii, hashlib, os, random, ssl, struct, sys, threading, zlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
sys.path.insert(0, os.path.dirname(os.path.abspath(globals().get('__file__', 'tests/tools/nav_servidor.py'))))
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
RECUERDA = 'R3CU3RDA'


# ---- imágenes hechas aquí (sin Pillow): PNG, GIF; el JPEG lo hizo Pillow ----
def png(w, h, color, alfa=False):
    """un PNG liso (RGB, o RGBA con la mitad derecha transparente)"""
    def trozo(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', binascii.crc32(t + d) & 0xFFFFFFFF)
    filas = b''
    for y in range(h):
        if alfa:
            filas += b'\0' + b''.join(bytes(color) + (b'\xff' if x < w // 2 else b'\0') for x in range(w))
        else:
            filas += b'\0' + bytes(color) * w
    return (b'\x89PNG\r\n\x1a\n' + trozo(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6 if alfa else 2, 0, 0, 0))
            + trozo(b'IDAT', zlib.compress(filas)) + trozo(b'IEND', b''))


def gif(w, h, color):
    """un GIF liso de 2 colores: LZW de 9 bits, con «borrar» cada poco (sin tabla que crezca)"""
    pal = bytes(color) + b'\xff\xff\xff' + b'\0' * 762
    cods, bits, nb, sal = [], 0, 0, bytearray()
    for i in range(w * h):
        if i % 200 == 0:
            cods.append(256)
        cods.append(0)
    cods.append(257)
    for c in cods:
        bits |= c << nb
        nb += 9
        while nb >= 8:
            sal.append(bits & 255)
            bits >>= 8
            nb -= 8
    if nb:
        sal.append(bits & 255)
    bloques = b''.join(bytes([len(sal[i:i + 255])]) + bytes(sal[i:i + 255]) for i in range(0, len(sal), 255))
    return (b'GIF89a' + struct.pack('<HHBBB', w, h, 0xF7, 0, 0) + pal + b',' + struct.pack('<HHHHB', 0, 0, w, h, 0)
            + b'\x08' + bloques + b'\0;')


TAPA_JPG = base64.b64decode('''/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAUDBAQEAwUEBAQFBQUGBwwIBwcHBw8LCwkMEQ8SEhEPERETFhwXExQaFRERGCEYGh0dHx8fExciJCIeJBweHx7/2wBDAQUFBQcGBw4ICA4eFBEUHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh4eHh7/wAARCAFoAPADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDgqKKK/LT8ZCiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooA9t/4UJ/1Nf8A5Tv/ALZR/wAKE/6mv/ynf/bK9tor8j/1nzT/AJ+/+Sx/yP2b/VPKf+fX/k0v8zxL/hQn/U1/+U7/AO2Uf8KE/wCpr/8AKd/9sr22ij/WfNP+fv8A5LH/ACD/AFTyn/n1/wCTS/zPEv8AhQn/AFNf/lO/+2Uf8KE/6mv/AMp3/wBsr22ij/WfNP8An7/5LH/IP9U8p/59f+TS/wAzxL/hQn/U1/8AlO/+2Uf8KE/6mv8A8p3/ANsr22ij/WfNP+fv/ksf8g/1Tyn/AJ9f+TS/zPEv+FCf9TX/AOU7/wC2Uf8AChP+pr/8p3/2yvbaKP8AWfNP+fv/AJLH/IP9U8p/59f+TS/zPEv+FCf9TX/5Tv8A7ZR/woT/AKmv/wAp3/2yvbaKP9Z80/5+/wDksf8AIP8AVPKf+fX/AJNL/M8S/wCFCf8AU1/+U7/7ZR/woT/qa/8Aynf/AGyvbaKP9Z80/wCfv/ksf8g/1Tyn/n1/5NL/ADPEv+FCf9TX/wCU7/7ZR/woT/qa/wDynf8A2yvbaKP9Z80/5+/+Sx/yD/VPKf8An1/5NL/M8S/4UJ/1Nf8A5Tv/ALZR/wAKE/6mv/ynf/bK9too/wBZ80/5+/8Aksf8g/1Tyn/n1/5NL/M8S/4UJ/1Nf/lO/wDtlH/ChP8Aqa//ACnf/bK9too/1nzT/n7/AOSx/wAg/wBU8p/59f8Ak0v8zxL/AIUJ/wBTX/5Tv/tlH/ChP+pr/wDKd/8AbK9too/1nzT/AJ+/+Sx/yD/VPKf+fX/k0v8AM8S/4UJ/1Nf/AJTv/tlH/ChP+pr/APKd/wDbK9too/1nzT/n7/5LH/IP9U8p/wCfX/k0v8zxL/hQn/U1/wDlO/8AtlH/AAoT/qa//Kd/9sr22ij/AFnzT/n7/wCSx/yD/VPKf+fX/k0v8zxL/hQn/U1/+U7/AO2Uf8KE/wCpr/8AKd/9sr22ij/WfNP+fv8A5LH/ACD/AFTyn/n1/wCTS/zPEv8AhQn/AFNf/lO/+2Uf8KE/6mv/AMp3/wBsr22ij/WfNP8An7/5LH/IP9U8p/59f+TS/wAwooorwD6IKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigAooooAKKKKACiiigD/2Q==''')   # 240x360: arriba verde (0,153,0), abajo amarillo (Pillow, calidad 85)
LOGO_PNG = png(96, 36, (255, 0, 0))           # rojo, con width y height en la página
AZUL_GIF = gif(40, 40, (0, 0, 255))           # azul, sin medidas en la página
ICONO_PNG = png(16, 16, (255, 0, 255), alfa=True)   # magenta y transparente: un icono en la línea
DATA_PNG = base64.b64encode(png(8, 8, (0, 255, 255))).decode()
PDF = (b'%PDF-1.4\n1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n'
       b'3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 300 200]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>endobj\n'
       b'4 0 obj<</Length 52>>stream\nBT /F1 18 Tf 20 120 Td (RECIBO DE PRUEBA 42) Tj ET\nendstream endobj\n'
       b'5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\ntrailer<</Root 1 0 R>>\n%%EOF\n')
subida = {}             # la portada subida: nombre, tipo, datos

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
            f.write('%s %s | %s | %s | %s | %s | %s\n' % (self.command, self.path, self.headers.get('Cookie', '-'), cuerpo,
                                                          self.headers.get('User-Agent', '-'), self.headers.get('Accept-Language', '-'),
                                                          self.headers.get('Accept', '-')))

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
        c = self.headers.get('Cookie', '')
        return ('SESION=' + SESION) in c or ('RECUERDA=' + RECUERDA) in c

    def do_GET(self):
        u = urlsplit(self.path)
        q = parse_qs(u.query)
        self.apunta()
        if u.path == '/' and ('RECUERDA=' + RECUERDA) in self.headers.get('Cookie', ''):
            return self.redirige('/biblioteca')      # «recordarme»: dentro sin escribir la clave
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
                '<a href="/devolver">Devolver</a> <a href="/imagenes">Im&aacute;genes</a> <a href="/portada">Portada</a></nav><h1>Libros</h1>'
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
        elif u.path == '/imagenes':
            self.manda(pagina('Imágenes de prueba',
                '<style>.portada{width:56px;height:80px;border:1px solid #ccc}</style><h1>Im&aacute;genes</h1>'
                '<p>Logo: <img src="/logo.png" alt="Logo" width="96" height="36"></p>'
                '<p><a href="/final"><img src="/tapa.jpg" class="portada" alt=""></a> Una portada con enlace</p>'
                '<p><img src="/azul.gif" alt="Azul"> sin medidas</p>'
                '<p>Icono <img src="/icono.png" width="16" height="16" alt="i"> en la l&iacute;nea y '
                '<img src="data:image/png;base64,' + DATA_PNG + '" alt="dentro"> dentro de la p&aacute;gina.</p>'
                '<p>Rota: <img src="/rota.png" alt="ROTA"> y un dibujo: <img src="/dibujo.svg" alt="SVG"></p>'
                '<p>FIN-IMAGENES</p>'))
        elif u.path in ('/logo.png', '/tapa.jpg', '/azul.gif', '/icono.png', '/subida.img'):
            if u.path == '/subida.img':
                if not subida:
                    return self.manda(b'', estado=404)
                return self.manda(subida['datos'], tipo=subida['tipo'])
            d = {'/logo.png': (LOGO_PNG, 'image/png'), '/tapa.jpg': (TAPA_JPG, 'image/jpeg'),
                 '/azul.gif': (AZUL_GIF, 'image/gif'), '/icono.png': (ICONO_PNG, 'image/png')}[u.path]
            self.manda(d[0], tipo=d[1], cab=(('Cache-Control', 'max-age=2592000'),))
        elif u.path == '/dibujo.svg':
            self.manda(b'<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20"><rect width="20" height="20"/></svg>', tipo='image/svg+xml')
        elif u.path == '/portada':
            if not self.con_sesion():
                return self.redirige('/?error=sesion')
            hecho = ('<div class="aviso">Subida: %s (%d octetos, %s).</div><p><img src="/subida.img" alt="la subida"></p>'
                     % (subida['nombre'], len(subida['datos']), subida['tipo'])) if 'hecho' in q and subida else ''
            self.manda(pagina('Portada - Biblioteca de pruebas', '<h1>Portada del libro</h1>' + hecho +
                '<form method="post" action="/portada" enctype="multipart/form-data">'
                '<input type="hidden" name="ficha" value="%s">'
                '<p><label for="portada">Portada</label> <input type="file" id="portada" name="portada" accept="image/jpeg,image/png" required></p>'
                '<p><label for="titulo">T&iacute;tulo</label> <input type="text" id="titulo" name="titulo" pattern="[A-Za-z ]{3,}" title="Solo letras, tres o m&aacute;s." size="20"></p>'
                '<p><button type="submit">Subir</button></p></form>'
                '<p><a href="/recibo.pdf">Recibo en PDF</a> &middot; <a href="/refresca">Se refresca solo</a></p>' % FICHA))
        elif u.path == '/recibo.pdf':
            self.manda(PDF, tipo='application/pdf', cab=(('Content-Disposition', 'attachment; filename="Recibo 2026-11.pdf"'),))
        elif u.path == '/refresca':
            self.manda(pagina('Se refresca', '<meta http-equiv="refresh" content="1; url=/final"><p>Espere un momento...</p>'))
        elif u.path == '/redir':
            self.redirige('/redir2')
        elif u.path == '/redir2':
            self.redirige('http://10.0.2.2:%d/final' % PUERTO, estado=301)
        elif u.path == '/final':
            self.manda(pagina('Final', '<p>Llegada tras dos redirecciones.</p>'
                              # [GLIFOS] las letras que la 437 no tiene (en UTF-8 y con entidades)
                              '<p>Últimos 33,00 € - &Agrave;ngel, Òscar, &Iacute;ñigo, Ïsona, Ëlia</p>'
                              '<p>São João: Ã Õ Â Ê Ô Û Î È Ì Ù &Uacute; &Oacute; - Œuvre, cœur &copy; &reg; &euro;&hellip;</p>'))
        elif u.path == '/latin1':
            self.manda('<html><head><title>Página en Latin-1</title></head><body><p>Año, cigüeña y camión en ISO-8859-1.</p></body></html>'
                       .encode('latin-1'), tipo='text/html; charset=ISO-8859-1')
        elif u.path == '/grande':
            c = ''.join('<p>P&aacute;rrafo n&uacute;mero %d de la p&aacute;gina grande, con texto bastante para ocupar una l&iacute;nea '
                        'entera y algo m&aacute;s de la ventana del navegador.</p>' % n
                        for n in range(1, min(int(q.get('n', ['400'])[0]), 3000) + 1))
            self.manda(pagina('Página grande', '<h1>P&aacute;gina grande</h1>' + c + '<p id="fin">FIN-DE-LA-GRANDE</p>'), trozos=True)
        elif u.path == '/larga':        # [RUEDA] tests/run_rueda_tests.sh: una lista larga y mucho texto
            ops = ''.join('<option value="%d">Opci&oacute;n %02d</option>' % (n, n) for n in range(1, 41))
            c = ''.join('<p>L&iacute;nea larga %03d.</p>' % n for n in range(1, 121))
            self.manda(pagina('Lista larga', '<p>Elija: <select name="n">%s</select></p>%s' % (ops, c)))
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
        crudo = self.rfile.read(n)
        tipo = self.headers.get('Content-Type', '')
        if tipo.startswith('multipart/form-data'):
            # las partes, una a una: los campos y el archivo (lo que se apunta
            # del archivo: nombre, tipo, octetos y el principio de su SHA-1)
            frontera = tipo.split('boundary=', 1)[1].strip('"').encode()
            q, apunte = {}, []
            for parte in crudo.split(b'--' + frontera)[1:]:
                if parte.startswith(b'--'):
                    break
                cab, _, datos = parte[2:].partition(b'\r\n\r\n')
                datos = datos[:-2] if datos.endswith(b'\r\n') else datos
                cab = cab.decode('utf-8', 'replace')
                nombre = cab.split('name="', 1)[1].split('"', 1)[0]
                if 'filename="' in cab:
                    fn = cab.split('filename="', 1)[1].split('"', 1)[0]
                    ct = cab.split('Content-Type:', 1)[1].strip() if 'Content-Type:' in cab else '?'
                    q[nombre] = [(fn, ct, datos)]
                    apunte.append('%s=%s:%s:%d:%s' % (nombre, fn, ct, len(datos), hashlib.sha1(datos).hexdigest()[:8]))
                else:
                    q[nombre] = [datos.decode('utf-8', 'replace')]
                    apunte.append('%s=%s' % (nombre, q[nombre][0]))
            cuerpo = '&'.join(apunte)
            self.apunta('[multipart] ' + cuerpo)
        else:
            cuerpo = crudo.decode('ascii', 'replace')
            q = parse_qs(cuerpo, keep_blank_values=True)
            self.apunta(cuerpo)
        v = lambda k: q.get(k, [''])[0]
        if self.path.startswith('/js') and nav_js.atiende_post(self, self.path, cuerpo):    # [JS]
            return
        if self.path == '/entrar':
            if v('usuario') == 'maria' and v('clave') == 'secreto' and v('ficha') == FICHA:
                seguro = '; Secure' if isinstance(self.connection, ssl.SSLSocket) else ''
                cab = (('Set-Cookie', 'SESION=%s; Path=/; HttpOnly; SameSite=Lax%s' % (SESION, seguro)),
                       ('Set-Cookie', 'vieja=1; Path=/; Max-Age=0'),
                       ('Set-Cookie', 'caduca=1; Path=/; Expires=Thu, 01 Jan 2015 00:00:00 GMT'))
                if v('recordar') == '1':        # «recordarme»: una galleta que dura 180 días, y otra con Expires
                    cab += (('Set-Cookie', 'RECUERDA=%s; Path=/; Max-Age=15552000; HttpOnly; SameSite=Lax%s' % (RECUERDA, seguro)),
                            ('Set-Cookie', 'PREF=azul; Path=/; Expires=Wed, 21 Oct 2037 07:28:00 GMT'))
                return self.redirige('/biblioteca', cab=cab)
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
        if self.path == '/portada':
            if 'portada' in q and isinstance(q['portada'][0], tuple) and q['portada'][0][2]:
                fn, ct, datos = q['portada'][0]
                subida.update(nombre=fn, tipo=ct, datos=datos)
                return self.redirige('/portada?hecho=1', estado=303)
            return self.redirige('/portada?falta=1', estado=303)
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
