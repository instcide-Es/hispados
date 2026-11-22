#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/correo/pruebas/servidor_correo.py CARPETA [--grande N]

El servidor de correo de pruebas para Correo, el cliente de correo de
Hispa-Pentium: IMAP4rev1, SMTP, POP3 y la autoconfiguración por HTTP(S), todo
en un proceso, sin ningún servidor ni cuenta de verdad. Solo la biblioteca
estándar de Python 3 y openssl en la línea de órdenes.

Hace con openssl, en CARPETA, una autoridad raíz de prueba (RAIZ.CER, en DER;
también raiz.pem) y los certificados:
  bueno     ECDSA P-256, de la raíz de prueba, para «10.0.2.2»
            (SAN: DNS:10.0.2.2, IP:10.0.2.2, DNS:localhost, IP:127.0.0.1)
  otro      de la raíz de prueba, pero para «otro.ejemplo» (nombre equivocado)
  extrano   para «10.0.2.2», pero de otra raíz que no conoce nadie
TLS 1.2 como mínimo (y 1.3 también), que es lo que habla BearSSL.

Escucha en 127.0.0.1, en puertos libres (desde QEMU con «-netdev user», el
anfitrión es 10.0.2.2), y cuando todo escucha deja CARPETA/puertos
(«nombre=puerto» por línea):
  imaps              IMAP con TLS desde el principio (como el 993)
  imap               IMAP sin cifrar con STARTTLS (como el 143); antes de TLS
                     anuncia LOGINDISABLED y rechaza LOGIN y AUTHENTICATE
  imap_sin_starttls  IMAP sin cifrar que NO ofrece STARTTLS (el cliente debe
                     negarse a mandar la clave; si la manda, entra y queda
                     apuntado «LOGIN SIN CIFRAR»)
  imap_plano_local   IMAP sin cifrar sin STARTTLS que acepta LOGIN (para la
                     opción «permitir sin cifrar»; también lo apunta)
  imaps_otro         IMAP con TLS, certificado de otro nombre
  imaps_extrano      IMAP con TLS, certificado de una raíz desconocida
  smtps              SMTP con TLS desde el principio (como el 465)
  submission         SMTP con STARTTLS (como el 587); AUTH solo tras TLS
  pop3s              POP3 con TLS (como el 995)
  pop3               POP3 sin cifrar con STLS (como el 110)
  https, http        autoconfiguración (clientConfig 1.1, como la ISPDB)

Usuarios (todos @prueba.test; sin «@» también vale):
  ana@prueba.test     secreta1     buzón variado (haz_buzones.py)
  luis@prueba.test    clave-luis   casi vacío; recibe lo que se le envía
  grande@prueba.test  grande1      INBOX con N mensajes de relleno (--grande,
                                   10000 si no se dice), hechos al vuelo
El estado está en memoria: lo comparten las conexiones del mismo usuario y
dura lo que dura el proceso (lo de un SMTP entra en el INBOX y salta el IDLE).

Lo que llega por SMTP se guarda en CARPETA/enviados/0001.eml, 0002.eml...
(los octetos exactos, sin el relleno de puntos) y su sobre en 0001.env
(«MAIL FROM:<...>» y un «RCPT TO:<...>» por destinatario).

CARPETA/registro.txt: un renglón por orden recibida y por respuesta (las
respuestas, con «-> » delante), con el puerto y si iba cifrada:
  [imaps tls] a1 LOGIN ana@prueba.test ****
  [imaps tls] -> a1 OK [CAPABILITY ...] LOGIN hecho
Las claves NUNCA se apuntan (****). Si alguien manda una clave por una
conexión sin cifrar, se apunta un renglón con «LOGIN SIN CIFRAR».

Lo pone en marcha un guion de pruebas y lo para por su PID (SIGTERM: acaba
limpio, con 0).
"""
import argparse
import base64
import binascii
import datetime as dt
import email.utils
import http.server
import os
import quopri
import re
import select
import signal
import socket
import socketserver
import ssl
import subprocess
import sys
import threading
import traceback
import urllib.parse
import zlib

sys.dont_write_bytecode = True   # sin __pycache__ en el árbol
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import haz_buzones as hb  # noqa: E402

ARGS = argparse.ArgumentParser(description='Servidor de correo de pruebas de Hispa-Pentium')
ARGS.add_argument('carpeta')
ARGS.add_argument('--grande', type=int, default=10000, help='mensajes del INBOX de grande@prueba.test')
OPC = ARGS.parse_args()
CARPETA = os.path.abspath(OPC.carpeta)
os.makedirs(CARPETA, exist_ok=True)
ENVIADOS = os.path.join(CARPETA, 'enviados')
os.makedirs(ENVIADOS, exist_ok=True)
for viejo in ('puertos', 'puertos.tmp'):
    try:
        os.remove(os.path.join(CARPETA, viejo))
    except FileNotFoundError:
        pass

DOMINIO = 'prueba.test'
USUARIOS = {'ana@prueba.test': 'secreta1', 'luis@prueba.test': 'clave-luis', 'grande@prueba.test': 'grande1'}
TAMANO_MAX = 35882577
CERROJO = threading.RLock()
PUERTOS = {}


# ======================================================================
# el registro
# ======================================================================
REGISTRO = open(os.path.join(CARPETA, 'registro.txt'), 'a', encoding='utf-8', buffering=1)
CERROJO_REGISTRO = threading.Lock()


def apunta(conn, texto):
    if isinstance(texto, bytes):
        texto = texto.decode('utf-8', 'replace')
    texto = texto.replace('\r', '\\r').replace('\n', '\\n')
    if len(texto) > 500:
        texto = texto[:500] + '...'
    with CERROJO_REGISTRO:
        REGISTRO.write('%s %s\n' % (conn.etiqueta() if conn else '[servidor]', texto))


# ======================================================================
# los certificados (como en tls/servidor_https.py)
# ======================================================================
def ossl(*a):
    subprocess.run(['openssl'] + list(a), cwd=CARPETA, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def raiz(n, cn):
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', n + '.key')
    ossl('req', '-x509', '-new', '-key', n + '.key', '-sha256', '-days', '3650', '-subj', '/O=Hispa-DOS pruebas/CN=' + cn,
         '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', n + '.pem')


def hoja(n, ca, nombre, san):
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', n + '.key')
    ossl('req', '-new', '-key', n + '.key', '-subj', '/CN=' + nombre, '-out', n + '.csr')
    with open(os.path.join(CARPETA, n + '.ext'), 'w') as f:
        f.write('subjectAltName=%s\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\n'
                'extendedKeyUsage=serverAuth\n' % san)
    ossl('x509', '-req', '-in', n + '.csr', '-CA', ca + '.pem', '-CAkey', ca + '.key', '-CAcreateserial', '-days', '3650',
         '-sha256', '-extfile', n + '.ext', '-out', n + '.pem')


SAN_BUENO = 'DNS:10.0.2.2,IP:10.0.2.2,DNS:localhost,IP:127.0.0.1'
raiz('raiz', 'Raiz de prueba de Hispa-Pentium')
ossl('x509', '-in', 'raiz.pem', '-outform', 'der', '-out', 'RAIZ.CER')
raiz('otra', 'Raiz que no conoce nadie')
hoja('bueno', 'raiz', '10.0.2.2', SAN_BUENO)
hoja('otro', 'raiz', 'otro.ejemplo', 'DNS:otro.ejemplo')
hoja('extrano', 'otra', '10.0.2.2', SAN_BUENO)


def contexto(cert):
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    ctx.load_cert_chain(os.path.join(CARPETA, cert + '.pem'), os.path.join(CARPETA, cert + '.key'))
    return ctx


CTX = {n: contexto(n) for n in ('bueno', 'otro', 'extrano')}


# ======================================================================
# la conexión: renglones, octetos y STARTTLS
# ======================================================================
class Cortada(Exception):
    pass


class Conexion:
    def __init__(self, sock, nombre, tls):
        self.sock = sock
        self.nombre = nombre
        self.tls = tls
        self.buf = bytearray()

    def etiqueta(self):
        return '[%s %s]' % (self.nombre, 'tls' if self.tls else 'plano')

    def _recibe(self):
        try:
            d = self.sock.recv(262144)
        except (ssl.SSLError, OSError) as e:
            raise Cortada(str(e))
        if not d:
            raise Cortada('el otro lado ha cerrado')
        self.buf += d

    def linea_cruda(self, tope=64 << 20):
        """Un renglón con su final (\\n o \\r\\n)."""
        desde = 0
        while True:
            i = self.buf.find(b'\n', desde)
            if i >= 0:
                r = bytes(self.buf[:i + 1])
                del self.buf[:i + 1]
                return r
            desde = len(self.buf)
            if len(self.buf) > tope:
                raise Cortada('renglón demasiado largo')
            self._recibe()

    def linea(self, tope=64 << 20):
        r = self.linea_cruda(tope)
        return r[:-2] if r.endswith(b'\r\n') else r[:-1]

    def lee(self, n):
        while len(self.buf) < n:
            self._recibe()
        r = bytes(self.buf[:n])
        del self.buf[:n]
        return r

    def hay_datos(self, espera):
        if self.buf:
            return True
        if self.tls and self.sock.pending():
            return True
        try:
            r, _, _ = select.select([self.sock], [], [], espera)
        except (OSError, ValueError):
            raise Cortada('socket cerrado')
        return bool(r)

    def manda(self, d):
        try:
            self.sock.sendall(d)
        except (ssl.SSLError, OSError) as e:
            raise Cortada(str(e))

    def starttls(self, ctx):
        self.buf.clear()   # lo que viniera antes del saludo TLS, fuera (RFC 3207, 2595)
        self.sock.settimeout(30)
        self.sock = ctx.wrap_socket(self.sock, server_side=True)
        self.sock.settimeout(1800)
        self.tls = True


# ======================================================================
# nombres de carpeta en UTF-7 modificado (RFC 3501 5.1.3)
# ======================================================================
def utf7_cod(s):
    out, tramo = [], []

    def vacia():
        if tramo:
            b = ''.join(tramo).encode('utf-16-be')
            out.append('&' + base64.b64encode(b).decode().rstrip('=').replace('/', ',') + '-')
            del tramo[:]
    for ch in s:
        if 0x20 <= ord(ch) <= 0x7e:
            vacia()
            out.append('&-' if ch == '&' else ch)
        else:
            tramo.append(ch)
    vacia()
    return ''.join(out)


def utf7_dec(s):
    def uno(m):
        t = m.group(1)
        if not t:
            return '&'
        t = t.replace(',', '/')
        t += '=' * (-len(t) % 4)
        try:
            return base64.b64decode(t, validate=True).decode('utf-16-be')
        except (binascii.Error, UnicodeDecodeError):
            raise Malo('Nombre de carpeta mal codificado en UTF-7')
    if any(ord(c) > 0x7e for c in s):
        # UTF-8 crudo (no es lo correcto, pero algunos lo mandan): se acepta
        return s
    return re.sub(r'&([^-]*)-', uno, s)


# ======================================================================
# los mensajes por dentro (MIME), lo justo para IMAP
# ======================================================================
class Parte:
    __slots__ = ('cab', 'cuerpo', 'campos', 'tipo', 'subtipo', 'params', 'hijos', 'incrustado', 'sin_ct')

    def valor(self, nombre):
        for n, v, _ in self.campos:
            if n == nombre:
                return v
        return None


def separa(datos):
    if datos.startswith(b'\r\n'):
        return b'\r\n', datos[2:]
    if datos.startswith(b'\n'):
        return b'\n', datos[1:]
    i = datos.find(b'\r\n\r\n')
    j = datos.find(b'\n\n')
    if i >= 0 and (j < 0 or i <= j + 1):
        return datos[:i + 4], datos[i + 4:]
    if j >= 0:
        return datos[:j + 2], datos[j + 2:]
    return datos, b''


def campos_de(cab):
    campos = []
    for renglon in re.split(rb'(?<=\n)', cab):
        if not renglon or renglon in (b'\r\n', b'\n'):
            continue
        if renglon[:1] in (b' ', b'\t') and campos:
            n, v, crudo = campos[-1]
            campos[-1] = (n, v + ' ' + renglon.strip().decode('latin-1'), crudo + renglon)
            continue
        i = renglon.find(b':')
        if i <= 0:
            continue
        nombre = renglon[:i].strip().decode('latin-1').lower()
        campos.append((nombre, renglon[i + 1:].strip().decode('latin-1'), renglon))
    return [(n, v.strip(), c) for n, v, c in campos]


def parametros(texto):
    r = []
    for m in re.finditer(r';\s*([^=;\s]+)\s*=\s*("(?:[^"\\]|\\.)*"|[^;]*)', texto):
        v = m.group(2).strip()
        if v.startswith('"') and v.endswith('"') and len(v) >= 2:
            v = re.sub(r'\\(.)', r'\1', v[1:-1])
        r.append((m.group(1).lower(), v))
    return r


def analiza(datos, defecto=('text', 'plain'), nivel=0):
    p = Parte()
    p.cab, p.cuerpo = separa(datos)
    p.campos = campos_de(p.cab)
    p.hijos = []
    p.incrustado = None
    ct = p.valor('content-type')
    p.sin_ct = ct is None
    tipo, subtipo, params = defecto[0], defecto[1], []
    if ct:
        m = re.match(r'\s*([^/;\s]+)\s*/\s*([^;\s]+)', ct)
        if m:
            tipo, subtipo = m.group(1).lower(), m.group(2).lower()
            params = parametros(ct[m.end():])
        else:
            tipo, subtipo = 'text', 'plain'
    p.tipo, p.subtipo, p.params = tipo, subtipo, params
    if nivel > 30:
        p.tipo, p.subtipo = 'application', 'octet-stream'
        return p
    if tipo == 'multipart':
        frontera = dict(params).get('boundary')
        hijo = ('message', 'rfc822') if subtipo == 'digest' else ('text', 'plain')
        p.hijos = [analiza(t, hijo, nivel + 1) for t in trozos_multipart(p.cuerpo, frontera)]
        if not p.hijos:
            p.hijos = [analiza(b'\r\n', ('text', 'plain'), nivel + 1)]
    elif tipo == 'message' and subtipo in ('rfc822', 'global'):
        p.incrustado = analiza(p.cuerpo, nivel=nivel + 1)
    return p


def trozos_multipart(cuerpo, frontera):
    if not frontera:
        return []
    f = frontera.encode('latin-1')
    marcas = list(re.finditer(rb'(\r?\n|^)--' + re.escape(f) + rb'(--)?[ \t]*(?=\r?\n|$)', cuerpo))
    trozos = []
    for k, m in enumerate(marcas):
        if m.group(2):
            break
        ini = m.end()
        if cuerpo.startswith(b'\r\n', ini):
            ini += 2
        elif cuerpo.startswith(b'\n', ini):
            ini += 1
        fin = marcas[k + 1].start() if k + 1 < len(marcas) else len(cuerpo)
        trozos.append(cuerpo[ini:max(ini, fin)])
    return trozos


def cad(v):
    """Una cadena IMAP: NIL, entre comillas o literal."""
    if v is None:
        return b'NIL'
    if isinstance(v, str):
        v = v.encode('latin-1', 'replace')
    if len(v) < 1000 and all(0x20 <= c < 0x7f for c in v):
        return b'"' + v.replace(b'\\', b'\\\\').replace(b'"', b'\\"') + b'"'
    return b'{%d}\r\n' % len(v) + v


def lista_params(params):
    if not params:
        return b'NIL'
    return b'(' + b' '.join(cad(k.upper() if k in ('charset', 'boundary', 'name', 'format', 'delsp', 'type', 'filename') else k) + b' ' + cad(v)
                            for k, v in params) + b')'


def direcciones(valor):
    if not valor:
        return b'NIL'
    r = []
    try:
        pares = email.utils.getaddresses([valor])
    except Exception:
        pares = []
    for nombre, dire in pares:
        if not nombre and not dire:
            continue
        if '@' in dire:
            buzon, sitio = dire.rsplit('@', 1)
        else:
            buzon, sitio = dire, ''
        r.append(b'(' + cad(nombre or None) + b' NIL ' + cad(buzon) + b' ' + cad(sitio) + b')')
    return b'(' + b''.join(r) + b')' if r else b'NIL'


def sobre(p):
    v = p.valor
    de = direcciones(v('from'))
    return b'(' + b' '.join([
        cad(v('date')), cad(v('subject')), de,
        direcciones(v('sender')) if v('sender') else de,
        direcciones(v('reply-to')) if v('reply-to') else de,
        direcciones(v('to')), direcciones(v('cc')), direcciones(v('bcc')),
        cad(v('in-reply-to')), cad(v('message-id'))]) + b')'


def disposicion(p):
    d = p.valor('content-disposition')
    if not d:
        return b'NIL'
    m = re.match(r'\s*([^;\s]+)', d)
    if not m:
        return b'NIL'
    return b'(' + cad(m.group(1).upper()) + b' ' + lista_params(parametros(d[m.end():])) + b')'


def renglones(d):
    return d.count(b'\n') + (1 if d and not d.endswith(b'\n') else 0)


def estructura(p, ext=True):
    if p.tipo == 'multipart':
        s = b'(' + b''.join(estructura(h, ext) for h in p.hijos) + b' ' + cad(p.subtipo.upper())
        if ext:
            s += b' ' + lista_params(p.params) + b' ' + disposicion(p) + b' ' + cad(p.valor('content-language')) + b' ' + cad(p.valor('content-location'))
        return s + b')'
    params = p.params
    if p.sin_ct and p.tipo == 'text':
        params = [('charset', 'us-ascii')]
    cte = (p.valor('content-transfer-encoding') or '7bit').strip().upper()
    s = [cad(p.tipo.upper()), cad(p.subtipo.upper()), lista_params(params), cad(p.valor('content-id')),
         cad(p.valor('content-description')), cad(cte), b'%d' % len(p.cuerpo)]
    if p.tipo == 'text':
        s.append(b'%d' % renglones(p.cuerpo))
    elif p.incrustado is not None:
        s += [sobre(p.incrustado), estructura(p.incrustado, ext), b'%d' % renglones(p.cuerpo)]
    if ext:
        s += [cad(p.valor('content-md5')), disposicion(p), cad(p.valor('content-language')), cad(p.valor('content-location'))]
    return b'(' + b' '.join(s) + b')'


def campos_pedidos(cab, nombres, excepto):
    nombres = set(n.lower() for n in nombres)
    trozo = b''
    for n, _v, crudo in campos_de(cab):
        if (n in nombres) != excepto:
            trozo += crudo if crudo.endswith(b'\n') else crudo + b'\r\n'
    return trozo + b'\r\n'


def seccion(raiz, numeros, palabra, nombres):
    cur, como_parte = raiz, False
    for n in numeros:
        if como_parte and cur.incrustado is not None:
            cur = cur.incrustado
        if cur.tipo == 'multipart':
            if n < 1 or n > len(cur.hijos):
                return b''
            cur = cur.hijos[n - 1]
        elif n != 1:
            return b''
        como_parte = True
    if palabra == '':
        if not numeros:
            return None   # el mensaje entero
        if cur is raiz:
            return raiz.cuerpo
        return cur.cuerpo
    if palabra == 'MIME':
        return cur.cab if numeros else b''
    destino = raiz
    if numeros:
        if cur.incrustado is None:
            return b''
        destino = cur.incrustado
    if palabra == 'HEADER':
        return destino.cab
    if palabra == 'TEXT':
        return destino.cuerpo
    if palabra in ('HEADER.FIELDS', 'HEADER.FIELDS.NOT'):
        return campos_pedidos(destino.cab, nombres, palabra.endswith('.NOT'))
    return b''


# ---- para buscar: cabeceras RFC 2047 y cuerpos descodificados ----
def juego(nombre):
    nombre = (nombre or '').strip().strip('"').lower()
    if not nombre:
        return None
    try:
        import codecs
        return codecs.lookup(nombre).name
    except LookupError:
        return None


def texto_de(octetos, nombre_juego=None):
    j = juego(nombre_juego)
    if j:
        return octetos.decode(j, 'replace')
    try:
        return octetos.decode('utf-8')
    except UnicodeDecodeError:
        return octetos.decode('cp1252', 'replace')


def rfc2047(valor):
    crudo = valor.encode('latin-1', 'replace')
    valor = texto_de(crudo)
    patron = re.compile(r'=\?([^?\s]+)\?([bBqQ])\?([^?\s]*)\?=')
    out, pos, ultimo_fue_codificado = [], 0, False
    for m in patron.finditer(valor):
        entre = valor[pos:m.start()]
        try:
            cs = m.group(1).split('*')[0]
            if m.group(2) in 'bB':
                t = m.group(3)
                b = base64.b64decode(t + '=' * (-len(t) % 4), validate=True)
            else:
                b = re.sub(rb'=([0-9A-Fa-f]{2})', lambda x: bytes([int(x.group(1), 16)]), m.group(3).replace('_', ' ').encode('latin-1'))
            dec = texto_de(b, cs)
        except (binascii.Error, ValueError, UnicodeError):
            out.append(entre + m.group(0))
            pos = m.end()
            ultimo_fue_codificado = False
            continue
        if not (ultimo_fue_codificado and entre.strip() == ''):
            out.append(entre)
        out.append(dec)
        pos = m.end()
        ultimo_fue_codificado = True
    out.append(valor[pos:])
    return ''.join(out)


def descodifica_cuerpo(p):
    cte = (p.valor('content-transfer-encoding') or '').strip().lower()
    d = p.cuerpo
    try:
        if cte == 'quoted-printable':
            d = quopri.decodestring(d)
        elif cte == 'base64':
            limpio = re.sub(rb'[^A-Za-z0-9+/]', b'', d)
            limpio = limpio[:len(limpio) - len(limpio) % 4]
            d = base64.b64decode(limpio)
    except (binascii.Error, ValueError):
        pass
    return texto_de(d, dict(p.params).get('charset'))


def textos_cuerpo(p, out, nivel=0):
    if nivel > 30:
        return
    if p.tipo == 'multipart':
        for h in p.hijos:
            textos_cuerpo(h, out, nivel + 1)
    elif p.incrustado is not None:
        out.append(cabeceras_legibles(p.incrustado))
        textos_cuerpo(p.incrustado, out, nivel + 1)
    elif p.tipo == 'text':
        out.append(descodifica_cuerpo(p))


def cabeceras_legibles(p):
    return '\n'.join('%s: %s' % (n, rfc2047(v)) for n, v, _ in p.campos)


# ======================================================================
# el estado: cuentas, carpetas y mensajes
# ======================================================================
class Mensaje:
    __slots__ = ('uid', 'banderas', 'fecha', '_datos', 'indice', '_tam', '_parte', '_busca', 'mod', 'reciente')

    def __init__(self, uid, datos, banderas, fecha, indice=None):
        self.uid = uid
        self._datos = datos
        self.indice = indice
        self.banderas = set(banderas)
        self.fecha = fecha
        self._tam = len(datos) if datos is not None else None
        self._parte = None
        self._busca = None
        self.mod = 0
        self.reciente = True

    def datos(self):
        if self._datos is not None:
            return self._datos
        d = hb.mensaje_grande(self.indice)
        self._tam = len(d)
        return d

    def tam(self):
        if self._tam is None:
            self.datos()
        return self._tam

    def parte(self, datos=None):
        if self._parte is not None:
            return self._parte
        p = analiza(datos if datos is not None else self.datos())
        if self._datos is not None:
            self._parte = p
        return p

    def busca(self):
        """(cabeceras {nombre: [valores legibles]}, cuerpo legible), en minúsculas (casefold)."""
        if self._busca is not None:
            return self._busca
        p = self.parte()
        cabs = {}
        for n, v, _ in p.campos:
            cabs.setdefault(n, []).append(rfc2047(v).casefold())
        out = []
        textos_cuerpo(p, out)
        r = (cabs, '\n'.join(out).casefold())
        if self._datos is not None:
            self._busca = r
        return r


class Carpeta:
    def __init__(self, nombre, uso, uidvalidity):
        self.nombre = nombre
        self.uso = uso
        self.uidvalidity = uidvalidity
        self.mensajes = []
        self.por_uid = {}
        self.uidnext = 1
        self.version = 0
        self.mod = 0

    def agrega(self, datos, banderas, fecha, indice=None, reciente=True):
        m = Mensaje(self.uidnext, datos, banderas, fecha, indice)
        m.reciente = reciente
        self.uidnext += 1
        self.mensajes.append(m)
        self.por_uid[m.uid] = m
        self.mod += 1
        m.mod = self.mod
        self.version += 1
        return m

    def quita(self, uids):
        if not uids:
            return
        self.mensajes = [m for m in self.mensajes if m.uid not in uids]
        for u in uids:
            self.por_uid.pop(u, None)
        self.version += 1

    def cambia(self, m):
        self.mod += 1
        m.mod = self.mod
        self.version += 1


VECES_CREADA = {}


def uidvalidity_de(usuario, nombre):
    k = (usuario, nombre)
    VECES_CREADA[k] = VECES_CREADA.get(k, 0) + 1
    return ((zlib.crc32(('%s/%s' % k).encode('utf-8')) + 7919 * (VECES_CREADA[k] - 1)) & 0x7fffffff) or 1


class Cuenta:
    def __init__(self, usuario):
        self.usuario = usuario
        self.carpetas = {}
        self.suscritas = set()

    def crea(self, nombre, uso=None):
        c = Carpeta(nombre, uso, uidvalidity_de(self.usuario, nombre))
        self.carpetas[nombre] = c
        self.suscritas.add(nombre)
        return c

    def busca(self, nombre):
        return self.carpetas.get(nombre)

    def tiene_hijas(self, nombre):
        return any(n.startswith(nombre + '/') for n in self.carpetas)


def arma_cuentas():
    cuentas = {}
    for usuario, carpetas in hb.buzones().items():
        cu = Cuenta(usuario)
        for nombre, uso, mensajes in carpetas:
            c = cu.crea(nombre, uso)
            if mensajes is None:
                for i in range(1, OPC.grande + 1):
                    c.agrega(None, hb.banderas_grande(i), hb.fecha_grande(i), indice=i, reciente=False)
            else:
                for datos, banderas, fecha in mensajes:
                    c.agrega(datos, banderas, fecha, reciente=not banderas)
        cuentas[usuario] = cu
    return cuentas


CUENTAS = arma_cuentas()


def entra(usuario, clave):
    u = usuario.strip().lower()
    if '@' not in u:
        u += '@' + DOMINIO
    if USUARIOS.get(u) is not None and USUARIOS[u] == clave:
        return CUENTAS[u]
    return None


def nombre_usuario(usuario):
    u = usuario.strip().lower()
    return u if '@' in u else u + '@' + DOMINIO


def entrega_local(dire, datos):
    """Si «dire» es de un usuario de aquí, al INBOX (y salta su IDLE)."""
    u = dire.strip().lower()
    if u in CUENTAS:
        with CERROJO:
            CUENTAS[u].carpetas['INBOX'].agrega(datos, set(), ahora())
        return True
    return False


def ahora():
    return dt.datetime.now(dt.timezone.utc).astimezone().replace(microsecond=0)


MESES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec']


def fecha_imap(f):
    z = int(f.utcoffset().total_seconds() // 60)
    return '%02d-%s-%04d %02d:%02d:%02d %s%02d%02d' % (f.day, MESES[f.month - 1], f.year, f.hour, f.minute, f.second,
                                                      '+' if z >= 0 else '-', abs(z) // 60, abs(z) % 60)


def lee_fecha_imap(t):
    m = re.match(r'^\s*(\d{1,2})-([A-Za-z]{3})-(\d{4}) (\d\d):(\d\d):(\d\d) ([+-])(\d\d)(\d\d)$', t)
    if not m or m.group(2).capitalize() not in MESES:
        raise Malo('Fecha mal escrita (dd-Mmm-aaaa hh:mm:ss +zzzz)')
    z = dt.timedelta(hours=int(m.group(8)), minutes=int(m.group(9)))
    if m.group(7) == '-':
        z = -z
    try:
        return dt.datetime(int(m.group(3)), MESES.index(m.group(2).capitalize()) + 1, int(m.group(1)), int(m.group(4)),
                           int(m.group(5)), int(m.group(6)), tzinfo=dt.timezone(z))
    except ValueError:
        raise Malo('Fecha imposible')


def lee_dia(t):
    m = re.match(r'^(\d{1,2})-([A-Za-z]{3})-(\d{4})$', t)
    if not m or m.group(2).capitalize() not in MESES:
        raise Malo('Fecha mal escrita (dd-Mmm-aaaa)')
    try:
        return dt.date(int(m.group(3)), MESES.index(m.group(2).capitalize()) + 1, int(m.group(1)))
    except ValueError:
        raise Malo('Fecha imposible')


def comprime(uids):
    """[1,2,3,5] -> «1:3,5»"""
    r = []
    for u in uids:
        if r and u == r[-1][1] + 1:
            r[-1][1] = u
        else:
            r.append([u, u])
    return ','.join(str(a) if a == b else '%d:%d' % (a, b) for a, b in r)


# ======================================================================
# IMAP4rev1
# ======================================================================
class Malo(Exception):
    """BAD"""


class No(Exception):
    """NO"""


class Atomo(str):
    pass


class Cadena(bytes):
    pass


class Lector:
    """Las palabras de una orden IMAP (con sus literales ya leídos)."""

    def __init__(self, partes):
        self.partes = partes
        self.i = 0
        self.p = 0

    def _salta(self):
        l = self.partes[self.i]
        while self.p < len(l) and l[self.p] == 32:
            self.p += 1

    def fin(self):
        self._salta()
        return self.i >= len(self.partes) - 1 and self.p >= len(self.partes[self.i])

    def resto(self):
        r = []
        while not self.fin():
            r.append(self.token())
        return r

    def token(self):
        self._salta()
        l = self.partes[self.i]
        p = self.p
        if p >= len(l):
            raise Malo('Faltan argumentos')
        c = l[p]
        if c == 40:   # (
            self.p += 1
            lista = []
            while True:
                self._salta()
                l = self.partes[self.i]
                if self.p >= len(l):
                    if self.i < len(self.partes) - 1:
                        lista.append(self.token())
                        continue
                    raise Malo('Falta el paréntesis de cierre')
                if l[self.p] == 41:
                    self.p += 1
                    return lista
                lista.append(self.token())
        if c == 41:
            raise Malo('Paréntesis de cierre de más')
        if c == 34:   # "
            out = bytearray()
            p += 1
            while True:
                if p >= len(l):
                    raise Malo('Cadena sin cerrar')
                if l[p] == 92 and p + 1 < len(l):
                    out.append(l[p + 1])
                    p += 2
                    continue
                if l[p] == 34:
                    p += 1
                    break
                out.append(l[p])
                p += 1
            self.p = p
            return Cadena(bytes(out))
        if c == 123 and re.match(rb'\{\d+\+?\}$', l[p:]) and self.i + 1 < len(self.partes):
            d = self.partes[self.i + 1]
            self.i += 2
            self.p = 0
            return Cadena(d)
        prof = 0
        q = p
        while q < len(l):
            ch = l[q]
            if ch == 91:
                prof += 1
            elif ch == 93:
                prof = max(0, prof - 1)
            elif prof == 0 and ch in (32, 40, 41):
                break
            q += 1
        self.p = q
        return Atomo(l[p:q].decode('latin-1'))


def texto(t):
    """astring -> str (latin-1 para los octetos sin tocar)"""
    if isinstance(t, list):
        raise Malo('Se esperaba una cadena, no una lista')
    if isinstance(t, Cadena):
        return bytes(t).decode('utf-8', 'replace')
    return str(t)


BANDERAS_SISTEMA = {'\\seen': '\\Seen', '\\answered': '\\Answered', '\\flagged': '\\Flagged', '\\deleted': '\\Deleted',
                    '\\draft': '\\Draft', '\\recent': '\\Recent'}
BANDERAS_FIJAS = ['\\Answered', '\\Flagged', '\\Deleted', '\\Seen', '\\Draft', '$Forwarded']
ORDEN_USO = {'\\Sent': 1, '\\Drafts': 2, '\\Trash': 5, '\\Junk': 4, '\\Archive': 3}

ESTADOS = {
    'CAPABILITY': 0, 'NOOP': 0, 'LOGOUT': 0, 'ID': 0,
    'STARTTLS': 'sin', 'LOGIN': 'sin', 'AUTHENTICATE': 'sin',
    'SELECT': 1, 'EXAMINE': 1, 'CREATE': 1, 'DELETE': 1, 'RENAME': 1, 'SUBSCRIBE': 1, 'UNSUBSCRIBE': 1,
    'LIST': 1, 'LSUB': 1, 'STATUS': 1, 'APPEND': 1, 'NAMESPACE': 1, 'IDLE': 1, 'ENABLE': 1,
    'CHECK': 2, 'CLOSE': 2, 'UNSELECT': 2, 'EXPUNGE': 2, 'SEARCH': 2, 'FETCH': 2, 'STORE': 2, 'COPY': 2,
    'MOVE': 2, 'UID': 2,
}


class SesionImap:
    def __init__(self, conn, modo):
        self.c = conn
        self.modo = modo   # tls, starttls, sin_starttls, plano_local
        self.cuenta = None
        self.carpeta = None
        self.solo_lectura = False
        self.vista = []
        self.pos = {}
        self.conocidas = {}
        self.recientes = set()
        self.version_vista = -1
        self.mod_visto = 0

    # ---- lo que se manda ----
    def caps(self):
        c = 'IMAP4rev1 LITERAL+ SASL-IR IDLE UIDPLUS MOVE SPECIAL-USE NAMESPACE UNSELECT CHILDREN LIST-EXTENDED ID'
        if self.cuenta is None:
            if self.modo == 'starttls' and not self.c.tls:
                c += ' STARTTLS LOGINDISABLED'
            else:
                c += ' AUTH=PLAIN'
        return c

    def u(self, t):
        if isinstance(t, str):
            t = t.encode('utf-8')
        self.c.manda(b'* ' + t + b'\r\n')

    def final(self, tag, estado, t):
        apunta(self.c, '-> %s %s %s' % (tag, estado, t))
        self.c.manda(('%s %s %s\r\n' % (tag, estado, t)).encode('utf-8'))

    # ---- lo que llega ----
    def lee_orden(self):
        partes = []
        while True:
            l = self.c.linea()
            m = re.search(rb'\{(\d+)(\+?)\}$', l)
            if not m:
                partes.append(l)
                return partes
            n = int(m.group(1))
            if n > TAMANO_MAX * 2:
                if not m.group(2):
                    self.c.manda(b'* BAD Literal demasiado grande\r\n')
                raise Cortada('literal demasiado grande')
            partes.append(l)
            if not m.group(2):
                self.c.manda(b'+ Sigue\r\n')
            partes.append(self.c.lee(n))

    def corre(self):
        self.u('OK [CAPABILITY %s] Servidor de correo de pruebas de Hispa-Pentium listo' % self.caps())
        while True:
            partes = self.lee_orden()
            if not self.una(partes):
                return

    def registra_orden(self, partes, tag, orden):
        if orden in ('LOGIN', 'AUTHENTICATE'):
            try:
                lec = Lector(partes)
                lec.token()
                lec.token()
                primero = texto(lec.token())
            except Malo:
                primero = ''
            apunta(self.c, '%s %s %s ****' % (tag, orden, primero))
            return
        t = b''
        for k, x in enumerate(partes):
            t += x if k % 2 == 0 else b'<%d octetos>' % len(x)
        apunta(self.c, t)

    def una(self, partes):
        lec = Lector(partes)
        try:
            tag = lec.token()
            if not isinstance(tag, Atomo) or not tag or tag == '*' or tag == '+':
                raise Malo('')
            orden = lec.token()
            if not isinstance(orden, Atomo):
                raise Malo('')
        except Malo:
            apunta(self.c, b''.join(p for k, p in enumerate(partes) if k % 2 == 0))
            apunta(self.c, '-> * BAD Orden ilegible')
            self.c.manda(b'* BAD Orden ilegible\r\n')
            return True
        orden = orden.upper()
        self.registra_orden(partes, tag, orden)
        estado = ESTADOS.get(orden)
        try:
            if estado is None:
                raise Malo('Orden desconocida: %s' % orden)
            if estado == 'sin' and self.cuenta is not None:
                raise Malo('Ya has entrado')
            if estado in (1, 2) and self.cuenta is None:
                raise Malo('Primero hay que entrar (LOGIN)')
            if estado == 2 and self.carpeta is None:
                raise Malo('Primero hay que elegir carpeta (SELECT)')
            r = getattr(self, 'o_' + orden)(tag, lec)
            return r is not False
        except Malo as e:
            self.final(tag, 'BAD', str(e) or 'Orden mal escrita')
        except No as e:
            self.final(tag, 'NO', str(e))
        return True

    # ---- sin estado ----
    def o_CAPABILITY(self, tag, lec):
        self.u('CAPABILITY ' + self.caps())
        self.final(tag, 'OK', 'CAPABILITY hecho')

    def o_NOOP(self, tag, lec):
        self.sincroniza()
        self.final(tag, 'OK', 'NOOP hecho')

    def o_ID(self, tag, lec):
        self.u('ID ("name" "Servidor de pruebas de Hispa-Pentium" "version" "1.0")')
        self.final(tag, 'OK', 'ID hecho')

    def o_LOGOUT(self, tag, lec):
        self.u('BYE Hasta luego')
        self.final(tag, 'OK', 'LOGOUT hecho')
        return False

    def o_ENABLE(self, tag, lec):
        self.u('ENABLED')
        self.final(tag, 'OK', 'ENABLE hecho')

    def o_STARTTLS(self, tag, lec):
        if self.modo != 'starttls' or self.c.tls:
            raise Malo('STARTTLS no se ofrece aquí')
        self.final(tag, 'OK', 'Empieza TLS')
        try:
            self.c.starttls(CTX['bueno'])
        except (ssl.SSLError, OSError) as e:
            apunta(self.c, 'fallo TLS: %s' % e)
            raise Cortada('fallo TLS')
        apunta(self.c, 'TLS puesto (%s, %s)' % (self.c.sock.version(), self.c.sock.cipher()[0]))

    # ---- entrar ----
    def sin_cifrar(self, usuario):
        """Ha llegado una clave sin cifrar: se apunta. ¿Se deja entrar?"""
        if self.c.tls:
            return True
        apunta(self.c, 'LOGIN SIN CIFRAR (usuario %s)' % usuario)
        return self.modo != 'starttls'

    def entra(self, tag, usuario, clave):
        if not self.sin_cifrar(usuario):
            raise No('[PRIVACYREQUIRED] Antes hay que cifrar (STARTTLS)')
        cu = entra(usuario, clave)
        if cu is None:
            raise No('[AUTHENTICATIONFAILED] Usuario o clave incorrectos')
        self.cuenta = cu
        self.final(tag, 'OK', '[CAPABILITY %s] Dentro' % self.caps())

    def o_LOGIN(self, tag, lec):
        usuario = texto(lec.token())
        clave = texto(lec.token())
        if not lec.fin():
            raise Malo('Sobran argumentos')
        self.entra(tag, usuario, clave)

    def o_AUTHENTICATE(self, tag, lec):
        mec = texto(lec.token()).upper()
        if mec != 'PLAIN':
            raise No('Mecanismo no admitido (solo PLAIN)')
        if self.modo == 'starttls' and not self.c.tls and lec.fin():
            raise No('[PRIVACYREQUIRED] Antes hay que cifrar (STARTTLS)')
        if lec.fin():
            self.c.manda(b'+ \r\n')
            r = self.c.linea()
            apunta(self.c, '****')
            if r.strip() == b'*':
                raise Malo('AUTHENTICATE cancelado')
        else:
            r = texto(lec.token()).encode('latin-1', 'replace')
        if r.strip() == b'=':
            r = b''
        try:
            d = base64.b64decode(r.strip(), validate=True)
            _authz, usuario, clave = d.split(b'\x00', 2)
        except (binascii.Error, ValueError):
            raise Malo('PLAIN mal codificado')
        self.entra(tag, usuario.decode('utf-8', 'replace'), clave.decode('utf-8', 'replace'))

    # ---- carpetas ----
    def nombre(self, t):
        n = utf7_dec(texto(t))
        if n.endswith('/') and len(n) > 1:
            n = n.rstrip('/')
        if n.upper() == 'INBOX':
            return 'INBOX'
        if n.upper().startswith('INBOX/'):
            n = 'INBOX' + n[5:]
        return n

    def carpeta_de(self, t):
        n = self.nombre(t)
        c = self.cuenta.busca(n)
        if c is None:
            raise No('[NONEXISTENT] No existe la carpeta')
        return c

    def o_NAMESPACE(self, tag, lec):
        self.u('NAMESPACE (("" "/")) NIL NIL')
        self.final(tag, 'OK', 'NAMESPACE hecho')

    def atributos(self, nombre, c, especial=True, suscrita=False):
        a = ['\\HasChildren' if self.cuenta.tiene_hijas(nombre) else '\\HasNoChildren']
        if c is None:
            a.insert(0, '\\NonExistent' if not self.cuenta.tiene_hijas(nombre) else '\\Noselect')
        if especial and c is not None and c.uso:
            a.append(c.uso)
        if suscrita and nombre in self.cuenta.suscritas:
            a.append('\\Subscribed')
        return a

    def coincide(self, nombre_cod, patron, es_inbox):
        r = ''
        for ch in patron:
            r += '.*' if ch == '*' else '[^/]*' if ch == '%' else re.escape(ch)
        return re.fullmatch(r, nombre_cod, re.IGNORECASE if es_inbox else 0) is not None or \
            (es_inbox and re.fullmatch(r, nombre_cod, re.IGNORECASE) is not None)

    def nombres_ordenados(self, nombres):
        return sorted(nombres, key=lambda n: (n != 'INBOX', n.lower()))

    def o_LIST(self, tag, lec, lsub=False):
        toks = lec.resto()
        sel = []
        if toks and isinstance(toks[0], list):
            if lsub:
                raise Malo('LSUB no lleva opciones')
            sel = [str(x).upper() for x in toks[0]]
            toks = toks[1:]
        if len(toks) < 2:
            raise Malo('LIST referencia patrón')
        ref = texto(toks[0])
        pats = toks[1] if isinstance(toks[1], list) else [toks[1]]
        pats = [texto(p) for p in pats]
        ret = []
        if len(toks) >= 4 and str(toks[2]).upper() == 'RETURN' and isinstance(toks[3], list):
            ret = [str(x).upper() for x in toks[3]]
        elif len(toks) > 2:
            raise Malo('Sobran argumentos')
        palabra = 'LSUB' if lsub else 'LIST'
        if pats == [''] and not lsub:
            self.u('LIST (\\Noselect) "/" ""')
            self.final(tag, 'OK', 'LIST hecho')
            return
        if lsub or 'SUBSCRIBED' in sel:
            nombres = set(self.cuenta.suscritas)
        else:
            nombres = set(self.cuenta.carpetas)
        suscrita = 'SUBSCRIBED' in sel or 'SUBSCRIBED' in ret
        with CERROJO:
            for n in self.nombres_ordenados(nombres):
                cod = utf7_cod(n)
                if not any(self.coincide(cod, ref + p, n == 'INBOX') for p in pats):
                    continue
                c = self.cuenta.busca(n)
                if 'SPECIAL-USE' in sel and (c is None or not c.uso):
                    continue
                at = self.atributos(n, c, True, suscrita)
                self.u(b'%s (%s) "/" %s' % (palabra.encode(), ' '.join(at).encode(), cad(cod)))
        self.final(tag, 'OK', '%s hecho' % palabra)

    def o_LSUB(self, tag, lec):
        self.o_LIST(tag, lec, lsub=True)

    def o_STATUS(self, tag, lec):
        t = lec.token()
        que = lec.token()
        if not isinstance(que, list):
            raise Malo('STATUS carpeta (MESSAGES ...)')
        with CERROJO:
            c = self.carpeta_de(t)
            r = []
            for x in que:
                x = str(x).upper()
                if x == 'MESSAGES':
                    v = len(c.mensajes)
                elif x == 'RECENT':
                    v = sum(1 for m in c.mensajes if m.reciente)
                elif x == 'UIDNEXT':
                    v = c.uidnext
                elif x == 'UIDVALIDITY':
                    v = c.uidvalidity
                elif x == 'UNSEEN':
                    v = sum(1 for m in c.mensajes if '\\Seen' not in m.banderas)
                elif x == 'SIZE':
                    v = sum(m.tam() for m in c.mensajes)
                else:
                    raise Malo('STATUS no sabe %s' % x)
                r.append('%s %d' % (x, v))
        self.u(b'STATUS %s (%s)' % (cad(utf7_cod(c.nombre)), ' '.join(r).encode()))
        self.final(tag, 'OK', 'STATUS hecho')

    def o_CREATE(self, tag, lec):
        n = self.nombre(lec.token())
        if n == 'INBOX' or not n:
            raise No('[ALREADYEXISTS] Esa carpeta ya existe')
        with CERROJO:
            if self.cuenta.busca(n):
                raise No('[ALREADYEXISTS] Esa carpeta ya existe')
            trozos = n.split('/')
            for k in range(1, len(trozos) + 1):
                sub = '/'.join(trozos[:k])
                if sub and not self.cuenta.busca(sub):
                    self.cuenta.crea(sub)
        self.final(tag, 'OK', 'CREATE hecho')

    def o_DELETE(self, tag, lec):
        n = self.nombre(lec.token())
        with CERROJO:
            if n == 'INBOX':
                raise No('El INBOX no se puede borrar')
            c = self.cuenta.busca(n)
            if c is None:
                raise No('[NONEXISTENT] No existe la carpeta')
            if self.cuenta.tiene_hijas(n):
                raise No('[INUSE] La carpeta tiene carpetas dentro; bórralas antes')
            del self.cuenta.carpetas[n]
            self.cuenta.suscritas.discard(n)
            c.quita(set(m.uid for m in c.mensajes))
        if self.carpeta is c:
            self.carpeta = None
        self.final(tag, 'OK', 'DELETE hecho')

    def o_RENAME(self, tag, lec):
        a = self.nombre(lec.token())
        b = self.nombre(lec.token())
        with CERROJO:
            c = self.cuenta.busca(a)
            if c is None:
                raise No('[NONEXISTENT] No existe la carpeta')
            if self.cuenta.busca(b) or b == 'INBOX':
                raise No('[ALREADYEXISTS] Ya hay una carpeta con ese nombre')
            if a == 'INBOX':
                nueva = self.cuenta.crea(b)
                for m in list(c.mensajes):
                    nueva.agrega(m._datos, m.banderas, m.fecha, m.indice, reciente=False)
                c.quita(set(m.uid for m in c.mensajes))
            else:
                for n in sorted([n for n in self.cuenta.carpetas if n == a or n.startswith(a + '/')]):
                    nuevo = b + n[len(a):]
                    x = self.cuenta.carpetas.pop(n)
                    x.nombre = nuevo
                    self.cuenta.carpetas[nuevo] = x
                    if n in self.cuenta.suscritas:
                        self.cuenta.suscritas.discard(n)
                        self.cuenta.suscritas.add(nuevo)
            padre = b.rsplit('/', 1)[0] if '/' in b else None
            if padre and not self.cuenta.busca(padre):
                self.cuenta.crea(padre)
        self.final(tag, 'OK', 'RENAME hecho')

    def o_SUBSCRIBE(self, tag, lec):
        n = self.nombre(lec.token())
        with CERROJO:
            if not self.cuenta.busca(n):
                raise No('[NONEXISTENT] No existe la carpeta')
            self.cuenta.suscritas.add(n)
        self.final(tag, 'OK', 'SUBSCRIBE hecho')

    def o_UNSUBSCRIBE(self, tag, lec):
        n = self.nombre(lec.token())
        with CERROJO:
            self.cuenta.suscritas.discard(n)
        self.final(tag, 'OK', 'UNSUBSCRIBE hecho')

    def o_APPEND(self, tag, lec):
        n = self.nombre(lec.token())
        toks = lec.resto()
        if not toks or not isinstance(toks[-1], Cadena):
            raise Malo('APPEND carpeta [(banderas)] [fecha] {n}')
        datos = bytes(toks[-1])
        banderas, fecha = set(), None
        resto = toks[:-1]
        if resto and isinstance(resto[0], list):
            banderas = self.banderas(resto[0])
            resto = resto[1:]
        if resto:
            fecha = lee_fecha_imap(texto(resto[0]))
            resto = resto[1:]
        if resto:
            raise Malo('Sobran argumentos')
        if len(datos) > TAMANO_MAX:
            raise No('[TOOBIG] Mensaje demasiado grande')
        with CERROJO:
            c = self.cuenta.busca(n)
            if c is None:
                raise No('[TRYCREATE] No existe la carpeta')
            m = c.agrega(datos, banderas, fecha or ahora())
            uv = c.uidvalidity
        if self.carpeta is c:
            self.sincroniza()
        self.final(tag, 'OK', '[APPENDUID %d %d] APPEND hecho' % (uv, m.uid))

    def o_IDLE(self, tag, lec):
        self.c.manda(b'+ idling\r\n')
        apunta(self.c, '-> + idling')
        while True:
            if self.carpeta is not None:
                self.sincroniza()
            if self.c.hay_datos(0.1):
                l = self.c.linea()
                apunta(self.c, l)
                if l.strip().upper() == b'DONE':
                    break
                self.final(tag, 'BAD', 'Se esperaba DONE')
                return
        self.final(tag, 'OK', 'IDLE terminado')

    # ---- elegir carpeta ----
    def suelta(self):
        self.carpeta = None
        self.vista = []
        self.pos = {}
        self.conocidas = {}
        self.recientes = set()

    def rehace_pos(self):
        self.pos = {u: i + 1 for i, u in enumerate(self.vista)}

    def abre(self, tag, lec, solo_lectura):
        t = lec.token()
        self.suelta()
        with CERROJO:
            c = self.carpeta_de(t)
            self.carpeta = c
            self.solo_lectura = solo_lectura
            self.vista = [m.uid for m in c.mensajes]
            self.rehace_pos()
            self.conocidas = {m.uid: frozenset(m.banderas) for m in c.mensajes}
            if not solo_lectura:
                for m in c.mensajes:
                    if m.reciente:
                        self.recientes.add(m.uid)
                        m.reciente = False
            self.version_vista = c.version
            self.mod_visto = c.mod
            primera = next((i + 1 for i, m in enumerate(c.mensajes) if '\\Seen' not in m.banderas), None)
            existen = len(c.mensajes)
            uv, un = c.uidvalidity, c.uidnext
            otras = sorted(set(b for m in c.mensajes for b in m.banderas) - set(BANDERAS_FIJAS))
        fl = ' '.join(BANDERAS_FIJAS + otras)
        self.u('FLAGS (%s)' % fl)
        self.u('OK [PERMANENTFLAGS (%s)] Banderas que se guardan' % ('' if solo_lectura else fl + ' \\*'))
        self.u('%d EXISTS' % existen)
        self.u('%d RECENT' % len(self.recientes))
        if primera:
            self.u('OK [UNSEEN %d] La primera sin leer' % primera)
        self.u('OK [UIDVALIDITY %d] UID válidos' % uv)
        self.u('OK [UIDNEXT %d] El próximo UID' % un)
        self.final(tag, 'OK', '[%s] %s hecho' % ('READ-ONLY' if solo_lectura else 'READ-WRITE', 'EXAMINE' if solo_lectura else 'SELECT'))

    def o_SELECT(self, tag, lec):
        self.abre(tag, lec, False)

    def o_EXAMINE(self, tag, lec):
        self.abre(tag, lec, True)

    def o_CHECK(self, tag, lec):
        self.sincroniza()
        self.final(tag, 'OK', 'CHECK hecho')

    def borra_marcados(self, uids=None):
        with CERROJO:
            c = self.carpeta
            fuera = set(m.uid for m in c.mensajes if '\\Deleted' in m.banderas and (uids is None or m.uid in uids))
            c.quita(fuera)
        return fuera

    def o_CLOSE(self, tag, lec):
        if not self.solo_lectura:
            self.borra_marcados()
        self.suelta()
        self.final(tag, 'OK', 'CLOSE hecho')

    def o_UNSELECT(self, tag, lec):
        self.suelta()
        self.final(tag, 'OK', 'UNSELECT hecho')

    def o_EXPUNGE(self, tag, lec, uids=None):
        if self.solo_lectura:
            raise No('[READ-ONLY] La carpeta es de solo lectura')
        self.borra_marcados(uids)
        self.sincroniza()
        self.final(tag, 'OK', 'EXPUNGE hecho')

    # ---- lo que ha cambiado (otros clientes, SMTP) ----
    def sincroniza(self, expunges=True):
        c = self.carpeta
        if c is None:
            return
        salida = []
        with CERROJO:
            if c.version == self.version_vista:
                return
            hay = c.por_uid
            if expunges:
                quitados = False
                for i in range(len(self.vista) - 1, -1, -1):
                    if self.vista[i] not in hay:
                        salida.append(b'* %d EXPUNGE' % (i + 1))
                        self.conocidas.pop(self.vista[i], None)
                        self.recientes.discard(self.vista[i])
                        del self.vista[i]
                        quitados = True
                if quitados:
                    self.rehace_pos()
            ultimo = self.vista[-1] if self.vista else 0
            nuevos = [m for m in c.mensajes if m.uid > ultimo and m.uid not in self.pos]
            for m in nuevos:
                self.vista.append(m.uid)
                self.pos[m.uid] = len(self.vista)
                self.conocidas[m.uid] = frozenset(m.banderas)
                if m.reciente and not self.solo_lectura:
                    self.recientes.add(m.uid)
                    m.reciente = False
            if nuevos:
                salida.append(b'* %d EXISTS' % len(self.vista))
                salida.append(b'* %d RECENT' % len(self.recientes))
            if c.mod != self.mod_visto:
                for m in c.mensajes:
                    if m.mod > self.mod_visto and m.uid in self.pos:
                        b = frozenset(m.banderas)
                        if self.conocidas.get(m.uid) != b:
                            self.conocidas[m.uid] = b
                            salida.append(b'* %d FETCH (UID %d FLAGS (%s))' % (self.pos[m.uid], m.uid, self.banderas_texto(m).encode()))
                self.mod_visto = c.mod
            if expunges:
                self.version_vista = c.version
        if salida:
            self.c.manda(b'\r\n'.join(salida) + b'\r\n')

    # ---- conjuntos de mensajes ----
    def conjunto(self, t, uid):
        t = str(t)
        if not re.fullmatch(r'[0-9*:,]+', t):
            raise Malo('Conjunto de mensajes mal escrito')
        vista = self.vista
        if uid:
            maximo = vista[-1] if vista else 0
        else:
            maximo = len(vista)
        rangos = []
        for trozo in t.split(','):
            if not trozo:
                raise Malo('Conjunto de mensajes mal escrito')
            ab = trozo.split(':')
            if len(ab) > 2:
                raise Malo('Conjunto de mensajes mal escrito')
            vals = []
            for x in ab:
                if x == '*':
                    vals.append(maximo)
                elif x.isdigit() and int(x) > 0:
                    vals.append(int(x))
                else:
                    raise Malo('Número de mensaje mal escrito')
            a, b = min(vals), max(vals)
            if not uid and '*' not in ab and b > len(vista):
                raise Malo('Número de mensaje fuera de rango')
            rangos.append((a, b))
        if uid:
            if len(rangos) == 1:
                a, b = rangos[0]
                return [(i + 1, u) for i, u in enumerate(vista) if a <= u <= b]
            return [(i + 1, u) for i, u in enumerate(vista) if any(a <= u <= b for a, b in rangos)]
        sel = set()
        for a, b in rangos:
            if a == 0:
                continue
            sel.update(range(a, min(b, len(vista)) + 1))
        return [(s, vista[s - 1]) for s in sorted(sel)]

    def banderas_texto(self, m):
        b = sorted(m.banderas, key=lambda x: (not x.startswith('\\'), x))
        if m.uid in self.recientes:
            b.append('\\Recent')
        return ' '.join(b)

    def banderas(self, lista):
        r = set()
        for x in lista:
            x = texto(x)
            if x.startswith('\\'):
                if x.lower() not in BANDERAS_SISTEMA:
                    raise Malo('Bandera desconocida: %s' % x)
                if x.lower() == '\\recent':
                    continue
                r.add(BANDERAS_SISTEMA[x.lower()])
            elif x and re.fullmatch(r'[^\s(){%*"\\\]]+', x):
                r.add(x)
            else:
                raise Malo('Bandera mal escrita')
        return r

    # ---- FETCH ----
    def pide(self, t):
        if isinstance(t, list):
            r = []
            for x in t:
                r += self.pide(x)
            return r
        s = str(t)
        su = s.upper()
        if su == 'ALL':
            return [('FLAGS',), ('INTERNALDATE',), ('RFC822.SIZE',), ('ENVELOPE',)]
        if su == 'FAST':
            return [('FLAGS',), ('INTERNALDATE',), ('RFC822.SIZE',)]
        if su == 'FULL':
            return [('FLAGS',), ('INTERNALDATE',), ('RFC822.SIZE',), ('ENVELOPE',), ('BODY',)]
        if su in ('FLAGS', 'UID', 'RFC822.SIZE', 'INTERNALDATE', 'ENVELOPE', 'BODYSTRUCTURE', 'BODY', 'RFC822',
                  'RFC822.HEADER', 'RFC822.TEXT'):
            return [(su,)]
        m = re.fullmatch(r'(BODY(?:\.PEEK)?|BINARY(?:\.PEEK)?)\[([^\]]*)\](?:<(\d+)(?:\.(\d+))?>)?', s, re.IGNORECASE)
        if not m:
            raise Malo('FETCH no sabe %s' % s)
        peek = m.group(1).upper().endswith('.PEEK')
        sec = m.group(2)
        mm = re.fullmatch(r'((?:\d+\.)*\d+)?\.?([A-Za-z.]*)\s*(?:\((.*)\))?\s*', sec)
        if not mm:
            raise Malo('Sección mal escrita: %s' % sec)
        numeros = [int(x) for x in mm.group(1).split('.')] if mm.group(1) else []
        palabra = mm.group(2).upper()
        if palabra not in ('', 'HEADER', 'TEXT', 'MIME', 'HEADER.FIELDS', 'HEADER.FIELDS.NOT'):
            raise Malo('Sección desconocida: %s' % sec)
        nombres = (mm.group(3) or '').replace('"', ' ').split()
        if palabra.startswith('HEADER.FIELDS') and not nombres:
            raise Malo('HEADER.FIELDS sin la lista de cabeceras')
        if palabra == 'MIME' and not numeros:
            raise Malo('MIME necesita un número de parte')
        if palabra.startswith('HEADER.FIELDS'):
            eco = '%s%s (%s)' % (mm.group(1) + '.' if mm.group(1) else '', palabra, ' '.join(n.upper() for n in nombres))
        else:
            eco = '.'.join(([mm.group(1)] if mm.group(1) else []) + ([palabra] if palabra else []))
        ini = int(m.group(3)) if m.group(3) is not None else None
        largo = int(m.group(4)) if m.group(4) is not None else None
        return [('SECCION', peek, numeros, palabra, nombres, eco, ini, largo)]

    def respuesta_fetch(self, seq, m, pedidos, uid):
        trozos = []
        datos = None
        p = None

        def d():
            nonlocal datos
            if datos is None:
                datos = m.datos()
            return datos

        def pa():
            nonlocal p
            if p is None:
                p = m.parte(d())
            return p
        marca_visto = False
        for x in pedidos:
            k = x[0]
            if k == 'UID':
                continue
            if k == 'FLAGS':
                continue
            if k == 'RFC822.SIZE':
                trozos.append(b'RFC822.SIZE %d' % (m._tam if m._tam is not None else len(d())))
            elif k == 'INTERNALDATE':
                trozos.append(b'INTERNALDATE "%s"' % fecha_imap(m.fecha).encode())
            elif k == 'ENVELOPE':
                trozos.append(b'ENVELOPE ' + sobre(pa()))
            elif k == 'BODYSTRUCTURE':
                trozos.append(b'BODYSTRUCTURE ' + estructura(pa(), True))
            elif k == 'BODY':
                trozos.append(b'BODY ' + estructura(pa(), False))
            elif k == 'RFC822':
                trozos.append(b'RFC822 ' + cad_literal(d()))
                marca_visto = True
            elif k == 'RFC822.HEADER':
                trozos.append(b'RFC822.HEADER ' + cad_literal(pa().cab))
            elif k == 'RFC822.TEXT':
                trozos.append(b'RFC822.TEXT ' + cad_literal(pa().cuerpo))
                marca_visto = True
            elif k == 'SECCION':
                _, peek, numeros, palabra, nombres, eco, ini, largo = x
                if not numeros and palabra == '':
                    cont = d()
                elif not numeros and palabra != 'MIME' and p is None:
                    cab, cuerpo = separa(d())   # sin analizar el MIME entero
                    cont = cuerpo if palabra == 'TEXT' else cab if palabra == 'HEADER' else \
                        campos_pedidos(cab, nombres, palabra.endswith('.NOT'))
                else:
                    cont = seccion(pa(), numeros, palabra, nombres)
                    if cont is None:
                        cont = d()
                nombre_resp = b'BODY[' + eco.encode('latin-1') + b']'
                if ini is not None:
                    cont = cont[ini:ini + largo] if largo is not None else cont[ini:]
                    nombre_resp += b'<%d>' % ini
                trozos.append(nombre_resp + b' ' + cad_literal(cont))
                if not peek:
                    marca_visto = True
        pide_banderas = any(x[0] == 'FLAGS' for x in pedidos)
        if marca_visto and not self.solo_lectura and '\\Seen' not in m.banderas:
            with CERROJO:
                m.banderas.add('\\Seen')
                self.carpeta.cambia(m)
                self.conocidas[m.uid] = frozenset(m.banderas)
            pide_banderas = True
        cabeza = []
        if uid or any(x[0] == 'UID' for x in pedidos):
            cabeza.append(b'UID %d' % m.uid)
        if pide_banderas:
            cabeza.append(b'FLAGS (%s)' % self.banderas_texto(m).encode())
            self.conocidas[m.uid] = frozenset(m.banderas)
        return b'* %d FETCH (' % seq + b' '.join(cabeza + trozos) + b')\r\n'

    def o_FETCH(self, tag, lec, uid=False):
        conj = lec.token()
        pedidos = self.pide(lec.token())
        if not lec.fin():
            lec.resto()   # modificadores (CHANGEDSINCE...): no se usan
        pares = self.conjunto(conj, uid)
        salida = []
        tam = 0
        for seq, u in pares:
            m = self.carpeta.por_uid.get(u)
            if m is None:
                continue
            r = self.respuesta_fetch(seq, m, pedidos, uid)
            salida.append(r)
            tam += len(r)
            if tam > 1 << 20:
                self.c.manda(b''.join(salida))
                salida, tam = [], 0
        if salida:
            self.c.manda(b''.join(salida))
        self.sincroniza(expunges=uid)
        self.final(tag, 'OK', '%sFETCH hecho' % ('UID ' if uid else ''))

    # ---- STORE ----
    def o_STORE(self, tag, lec, uid=False):
        conj = lec.token()
        que = str(lec.token()).upper()
        resto = lec.resto()
        if len(resto) == 1 and isinstance(resto[0], list):
            lista = resto[0]
        else:
            lista = resto
        m = re.fullmatch(r'([+-]?)FLAGS(\.SILENT)?', que)
        if not m:
            raise Malo('STORE solo sabe FLAGS, +FLAGS y -FLAGS')
        nuevas = self.banderas(lista)
        if self.solo_lectura:
            raise No('[READ-ONLY] La carpeta es de solo lectura')
        pares = self.conjunto(conj, uid)
        salida = []
        with CERROJO:
            for seq, u in pares:
                msg = self.carpeta.por_uid.get(u)
                if msg is None:
                    continue
                antes = set(msg.banderas)
                if m.group(1) == '+':
                    msg.banderas |= nuevas
                elif m.group(1) == '-':
                    msg.banderas -= nuevas
                else:
                    msg.banderas = set(nuevas)
                if msg.banderas != antes:
                    self.carpeta.cambia(msg)
                self.conocidas[u] = frozenset(msg.banderas)
                if not m.group(2):
                    salida.append(b'* %d FETCH (%sFLAGS (%s))\r\n' % (seq, b'UID %d ' % u if uid else b'', self.banderas_texto(msg).encode()))
        if salida:
            self.c.manda(b''.join(salida))
        self.sincroniza(expunges=uid)
        self.final(tag, 'OK', '%sSTORE hecho' % ('UID ' if uid else ''))

    # ---- COPY / MOVE ----
    def copia(self, conj, destino_t, uid):
        pares = self.conjunto(conj, uid)
        with CERROJO:
            dest = self.cuenta.busca(self.nombre(destino_t))
            if dest is None:
                raise No('[TRYCREATE] No existe la carpeta de destino')
            de, a = [], []
            for _seq, u in pares:
                m = self.carpeta.por_uid.get(u)
                if m is None:
                    continue
                n = dest.agrega(m._datos, set(m.banderas), m.fecha, m.indice)
                de.append(u)
                a.append(n.uid)
        return dest, de, a

    def o_COPY(self, tag, lec, uid=False):
        conj = lec.token()
        dest_t = lec.token()
        dest, de, a = self.copia(conj, dest_t, uid)
        if dest is self.carpeta:
            self.sincroniza(expunges=uid)
        if de:
            self.final(tag, 'OK', '[COPYUID %d %s %s] COPY hecho' % (dest.uidvalidity, comprime(de), comprime(a)))
        else:
            self.final(tag, 'OK', 'COPY hecho (nada que copiar)')

    def o_MOVE(self, tag, lec, uid=False):
        if self.solo_lectura:
            raise No('[READ-ONLY] La carpeta es de solo lectura')
        conj = lec.token()
        dest_t = lec.token()
        dest, de, a = self.copia(conj, dest_t, uid)
        if de:
            self.u('OK [COPYUID %d %s %s] Movidos' % (dest.uidvalidity, comprime(de), comprime(a)))
            with CERROJO:
                self.carpeta.quita(set(de))
        self.sincroniza()
        self.final(tag, 'OK', '%sMOVE hecho' % ('UID ' if uid else ''))

    # ---- SEARCH ----
    def o_SEARCH(self, tag, lec, uid=False):
        toks = lec.resto()
        if toks and isinstance(toks[0], Atomo) and toks[0].upper() == 'RETURN':
            raise Malo('ESEARCH no se ofrece')
        if len(toks) >= 2 and isinstance(toks[0], Atomo) and toks[0].upper() == 'CHARSET':
            cs = texto(toks[1]).upper()
            if cs not in ('UTF-8', 'US-ASCII', 'UTF8'):
                raise No('[BADCHARSET (UTF-8 US-ASCII)] Juego de caracteres no admitido')
            toks = toks[2:]
        if not toks:
            raise Malo('SEARCH sin criterios')
        criterio = self.criterios(toks)
        res = []
        with CERROJO:
            carpeta = self.carpeta
            vista = list(self.vista)
        for i, u in enumerate(vista):
            m = carpeta.por_uid.get(u)
            if m is not None and criterio(m, i + 1):
                res.append(u if uid else i + 1)
        self.u('SEARCH' + ''.join(' %d' % x for x in res))
        self.sincroniza(expunges=uid)
        self.final(tag, 'OK', '%sSEARCH hecho' % ('UID ' if uid else ''))

    def criterios(self, toks):
        pila = list(toks)
        pila.reverse()
        fs = []
        while pila:
            fs.append(self.criterio(pila))
        return lambda m, s: all(f(m, s) for f in fs)

    def criterio(self, pila):
        t = pila.pop()

        def arg():
            if not pila:
                raise Malo('Falta un argumento en SEARCH')
            return pila.pop()

        def cadena():
            return texto(arg()).casefold()
        if isinstance(t, list):
            sub = self.criterios(t)
            return sub
        if isinstance(t, Cadena):
            raise Malo('Criterio de búsqueda desconocido')
        k = t.upper()
        bandera = {'ANSWERED': '\\Answered', 'DELETED': '\\Deleted', 'DRAFT': '\\Draft', 'FLAGGED': '\\Flagged', 'SEEN': '\\Seen'}
        if k == 'ALL':
            return lambda m, s: True
        if k in bandera:
            b = bandera[k]
            return lambda m, s: b in m.banderas
        if k.startswith('UN') and k[2:] in bandera:
            b = bandera[k[2:]]
            return lambda m, s: b not in m.banderas
        if k == 'NEW':
            return lambda m, s: m.uid in self.recientes and '\\Seen' not in m.banderas
        if k == 'OLD':
            return lambda m, s: m.uid not in self.recientes
        if k == 'RECENT':
            return lambda m, s: m.uid in self.recientes
        if k in ('KEYWORD', 'UNKEYWORD'):
            kw = texto(arg())
            si = k == 'KEYWORD'
            return lambda m, s: (kw in m.banderas) == si
        if k == 'NOT':
            f = self.criterio(pila)
            return lambda m, s: not f(m, s)
        if k == 'OR':
            a = self.criterio(pila)
            b = self.criterio(pila)
            return lambda m, s: a(m, s) or b(m, s)
        if k in ('FROM', 'TO', 'CC', 'BCC', 'SUBJECT'):
            v = cadena()
            n = k.lower()
            return lambda m, s: any(v in x for x in m.busca()[0].get(n, []))
        if k == 'HEADER':
            n = texto(arg()).lower()
            v = cadena()
            return lambda m, s: any(v in x for x in m.busca()[0].get(n, []))
        if k == 'BODY':
            v = cadena()
            return lambda m, s: v in m.busca()[1]
        if k == 'TEXT':
            v = cadena()
            return lambda m, s: v in m.busca()[1] or any(v in x for vs in m.busca()[0].values() for x in vs)
        if k in ('SINCE', 'BEFORE', 'ON'):
            dia = lee_dia(texto(arg()))
            if k == 'SINCE':
                return lambda m, s: m.fecha.date() >= dia
            if k == 'BEFORE':
                return lambda m, s: m.fecha.date() < dia
            return lambda m, s: m.fecha.date() == dia
        if k in ('SENTSINCE', 'SENTBEFORE', 'SENTON'):
            dia = lee_dia(texto(arg()))

            def enviado(m):
                v = m.parte().valor('date')
                try:
                    return email.utils.parsedate_to_datetime(v).date()
                except (TypeError, ValueError, IndexError):
                    return None

            def f(m, s, k=k):
                e = enviado(m)
                if e is None:
                    return False
                return e >= dia if k == 'SENTSINCE' else e < dia if k == 'SENTBEFORE' else e == dia
            return f
        if k in ('LARGER', 'SMALLER'):
            v = texto(arg())
            if not v.isdigit():
                raise Malo('LARGER/SMALLER llevan un número')
            n = int(v)
            if k == 'LARGER':
                return lambda m, s: m.tam() > n
            return lambda m, s: m.tam() < n
        if k == 'UID':
            uids = set(u for _s, u in self.conjunto(arg(), True))
            return lambda m, s: m.uid in uids
        if re.fullmatch(r'[0-9*:,]+', k):
            seqs = set(s for s, _u in self.conjunto(k, False))
            return lambda m, s: s in seqs
        raise Malo('Criterio de búsqueda desconocido: %s' % t)

    # ---- UID ----
    def o_UID(self, tag, lec):
        sub = str(lec.token()).upper()
        if sub == 'FETCH':
            return self.o_FETCH(tag, lec, uid=True)
        if sub == 'STORE':
            return self.o_STORE(tag, lec, uid=True)
        if sub == 'COPY':
            return self.o_COPY(tag, lec, uid=True)
        if sub == 'MOVE':
            return self.o_MOVE(tag, lec, uid=True)
        if sub == 'SEARCH':
            return self.o_SEARCH(tag, lec, uid=True)
        if sub == 'EXPUNGE':
            uids = set(u for _s, u in self.conjunto(lec.token(), True))
            return self.o_EXPUNGE(tag, lec, uids)
        raise Malo('UID no sabe %s' % sub)


def cad_literal(d):
    return b'{%d}\r\n' % len(d) + d


# ======================================================================
# SMTP (RFC 5321, 4954, 3207)
# ======================================================================
CERROJO_ENVIADOS = threading.Lock()


def guarda_enviado(remite, destinos, datos):
    with CERROJO_ENVIADOS:
        n = 1
        while os.path.exists(os.path.join(ENVIADOS, '%04d.eml' % n)):
            n += 1
        base = os.path.join(ENVIADOS, '%04d' % n)
        with open(base + '.env.tmp', 'wb') as f:
            f.write(('MAIL FROM:<%s>\r\n' % remite).encode('utf-8') + b''.join(('RCPT TO:<%s>\r\n' % d).encode('utf-8') for d in destinos))
        with open(base + '.eml.tmp', 'wb') as f:
            f.write(datos)
        os.rename(base + '.env.tmp', base + '.env')
        os.rename(base + '.eml.tmp', base + '.eml')
    return n


def atiende_smtp(conn, starttls):
    def r(t):
        apunta(conn, '-> ' + t.replace('\r\n', ' | '))
        conn.manda(t.encode('utf-8') + b'\r\n')

    r('220 prueba.test ESMTP Servidor de correo de pruebas de Hispa-Pentium')
    saludo = None
    usuario = None
    remite = None
    destinos = []

    def puede_auth():
        return conn.tls

    def credenciales(u, clave):
        nonlocal usuario
        if not conn.tls:
            apunta(conn, 'LOGIN SIN CIFRAR (usuario %s)' % u)
            r('538 5.7.11 Hay que cifrar antes (STARTTLS)')
            return
        if entra(u, clave):
            usuario = nombre_usuario(u)
            r('235 2.7.0 Dentro')
        else:
            r('535 5.7.8 Usuario o clave incorrectos')

    while True:
        l = conn.linea()
        texto_l = l.decode('utf-8', 'replace')
        palabra = texto_l.split(' ', 1)[0].upper()
        if palabra == 'AUTH':
            partes = texto_l.split()
            apunta(conn, 'AUTH %s%s' % (partes[1] if len(partes) > 1 else '', ' ****' if len(partes) > 2 else ''))
        else:
            apunta(conn, texto_l)
        if palabra in ('EHLO', 'HELO'):
            saludo = texto_l[5:].strip() or 'desconocido'
            remite, destinos = None, []
            if palabra == 'HELO':
                r('250 prueba.test te saluda, %s' % saludo)
                continue
            lineas = ['prueba.test te saluda, %s' % saludo, 'SIZE %d' % TAMANO_MAX, '8BITMIME', 'PIPELINING', 'ENHANCEDSTATUSCODES']
            if puede_auth():
                lineas.append('AUTH PLAIN LOGIN')
            if starttls and not conn.tls:
                lineas.append('STARTTLS')
            lineas.append('SMTPUTF8')
            r('\r\n'.join('250%s%s' % ('-' if k < len(lineas) - 1 else ' ', x) for k, x in enumerate(lineas)))
        elif palabra == 'STARTTLS':
            if not starttls or conn.tls:
                r('503 5.5.1 STARTTLS no se ofrece aquí')
                continue
            r('220 2.0.0 Empieza TLS')
            try:
                conn.starttls(CTX['bueno'])
            except (ssl.SSLError, OSError) as e:
                apunta(conn, 'fallo TLS: %s' % e)
                return
            apunta(conn, 'TLS puesto (%s, %s)' % (conn.sock.version(), conn.sock.cipher()[0]))
            saludo, usuario, remite, destinos = None, None, None, []
        elif palabra == 'AUTH':
            partes = texto_l.split()
            if saludo is None:
                r('503 5.5.1 Primero EHLO')
                continue
            if usuario:
                r('503 5.5.1 Ya has entrado')
                continue
            mec = partes[1].upper() if len(partes) > 1 else ''
            if not conn.tls and (len(partes) <= 2 or mec == 'LOGIN'):
                r('538 5.7.11 Hay que cifrar antes (STARTTLS)')
                continue
            if mec == 'PLAIN':
                if len(partes) > 2:
                    ir = partes[2]
                else:
                    r('334 ')
                    ir = conn.linea().decode('latin-1')
                    apunta(conn, '****')
                if ir.strip() == '*':
                    r('501 5.0.0 AUTH cancelado')
                    continue
                try:
                    _z, u, clave = base64.b64decode(ir.strip() if ir.strip() != '=' else '', validate=True).split(b'\x00', 2)
                except (binascii.Error, ValueError):
                    r('501 5.5.2 PLAIN mal codificado')
                    continue
                credenciales(u.decode('utf-8', 'replace'), clave.decode('utf-8', 'replace'))
            elif mec == 'LOGIN':
                try:
                    if len(partes) > 2:
                        u = base64.b64decode(partes[2], validate=True)
                    else:
                        r('334 VXNlcm5hbWU6')
                        lu = conn.linea().strip()
                        apunta(conn, lu.decode('latin-1'))
                        if lu == b'*':
                            r('501 5.0.0 AUTH cancelado')
                            continue
                        u = base64.b64decode(lu, validate=True)
                    r('334 UGFzc3dvcmQ6')
                    lc = conn.linea().strip()
                    apunta(conn, '****')
                    if lc == b'*':
                        r('501 5.0.0 AUTH cancelado')
                        continue
                    clave = base64.b64decode(lc, validate=True)
                except (binascii.Error, ValueError):
                    r('501 5.5.2 LOGIN mal codificado')
                    continue
                credenciales(u.decode('utf-8', 'replace'), clave.decode('utf-8', 'replace'))
            else:
                r('504 5.5.4 Mecanismo no admitido (PLAIN o LOGIN)')
        elif palabra == 'MAIL':
            m = re.match(r'(?i)^MAIL FROM:\s*<([^>]*)>(.*)$', texto_l)
            if not m:
                r('501 5.5.4 MAIL FROM:<dirección>')
                continue
            if saludo is None:
                r('503 5.5.1 Primero EHLO')
                continue
            if not usuario:
                r('530 5.7.0 Hay que entrar antes (AUTH)')
                continue
            if remite is not None:
                r('503 5.5.1 Ya hay un MAIL FROM')
                continue
            ms = re.search(r'(?i)\bSIZE=(\d+)', m.group(2))
            if ms and int(ms.group(1)) > TAMANO_MAX:
                r('552 5.3.4 Mensaje demasiado grande')
                continue
            remite, destinos = m.group(1), []
            r('250 2.1.0 Remitente aceptado')
        elif palabra == 'RCPT':
            m = re.match(r'(?i)^RCPT TO:\s*<([^>]+)>', texto_l)
            if not m:
                r('501 5.5.4 RCPT TO:<dirección>')
                continue
            if remite is None:
                r('503 5.5.1 Primero MAIL FROM')
                continue
            d = m.group(1).strip()
            if d.lower().endswith('@' + DOMINIO) and d.lower() not in CUENTAS:
                r('550 5.1.1 No existe ese buzón')
                continue
            destinos.append(d)
            r('250 2.1.5 Destinatario aceptado')
        elif palabra == 'DATA':
            if remite is None or not destinos:
                r('503 5.5.1 Primero MAIL FROM y RCPT TO')
                continue
            r('354 Manda el mensaje; acaba con <CRLF>.<CRLF>')
            trozos, total = [], 0
            while True:
                x = conn.linea_cruda()
                if x in (b'.\r\n', b'.\n'):
                    break
                if x.startswith(b'.'):
                    x = x[1:]
                total += len(x)
                if total <= TAMANO_MAX:
                    trozos.append(x)
            apunta(conn, '<%d octetos del mensaje>' % total)
            if total > TAMANO_MAX:
                r('552 5.3.4 Mensaje demasiado grande')
            else:
                datos = b''.join(trozos)
                n = guarda_enviado(remite, destinos, datos)
                for d in destinos:
                    entrega_local(d, datos)
                r('250 2.0.0 Aceptado como %04d' % n)
            remite, destinos = None, []
        elif palabra == 'RSET':
            remite, destinos = None, []
            r('250 2.0.0 Hecho')
        elif palabra == 'NOOP':
            r('250 2.0.0 Hecho')
        elif palabra == 'QUIT':
            r('221 2.0.0 Hasta luego')
            return
        elif palabra == 'VRFY':
            r('252 2.1.5 No se comprueba')
        else:
            r('500 5.5.2 Orden desconocida')


# ======================================================================
# POP3 (RFC 1939, 2449, 2595)
# ======================================================================
def atiende_pop3(conn, stls):
    def r(t):
        apunta(conn, '-> ' + t)
        conn.manda(t.encode('utf-8') + b'\r\n')

    def multi(datos):
        if not datos.endswith(b'\r\n'):
            datos += b'\r\n'
        datos = re.sub(rb'(^|\n)\.', rb'\1..', datos)
        conn.manda(datos + b'.\r\n')

    r('+OK Servidor POP3 de pruebas de Hispa-Pentium listo')
    usuario = None
    cuenta = None
    lista = []
    borrar = set()
    while True:
        l = conn.linea().decode('utf-8', 'replace')
        partes = l.split()
        orden = partes[0].upper() if partes else ''
        apunta(conn, 'PASS ****' if orden == 'PASS' else l)
        if orden == 'CAPA':
            r('+OK Lo que sé hacer')
            capas = ['USER', 'UIDL', 'TOP', 'RESP-CODES', 'PIPELINING']
            if stls and not conn.tls and cuenta is None:
                capas.append('STLS')
            conn.manda(('\r\n'.join(capas) + '\r\n.\r\n').encode())
        elif orden == 'QUIT':
            if cuenta is not None and borrar:
                with CERROJO:
                    cuenta.carpetas['INBOX'].quita(set(lista[i].uid for i in borrar))
            r('+OK Hasta luego')
            return
        elif orden == 'NOOP':
            r('+OK')
        elif cuenta is None:
            if orden == 'STLS':
                if not stls or conn.tls:
                    r('-ERR STLS no se ofrece aquí')
                    continue
                r('+OK Empieza TLS')
                try:
                    conn.starttls(CTX['bueno'])
                except (ssl.SSLError, OSError) as e:
                    apunta(conn, 'fallo TLS: %s' % e)
                    return
                apunta(conn, 'TLS puesto (%s, %s)' % (conn.sock.version(), conn.sock.cipher()[0]))
            elif orden == 'USER':
                if not conn.tls:
                    r('-ERR [AUTH] Hay que cifrar antes (STLS)')
                    usuario = partes[1] if len(partes) > 1 else ''
                    continue
                usuario = partes[1] if len(partes) > 1 else ''
                r('+OK Ahora la clave')
            elif orden == 'PASS':
                if usuario is None:
                    r('-ERR Primero USER')
                    continue
                if not conn.tls:
                    apunta(conn, 'LOGIN SIN CIFRAR (usuario %s)' % usuario)
                    r('-ERR [AUTH] Hay que cifrar antes (STLS)')
                    continue
                cu = entra(usuario, l[5:])
                if cu is None:
                    r('-ERR [AUTH] Usuario o clave incorrectos')
                    usuario = None
                    continue
                cuenta = cu
                with CERROJO:
                    lista = list(cu.carpetas['INBOX'].mensajes)
                r('+OK Dentro, %d mensajes' % len(lista))
            else:
                r('-ERR Primero USER y PASS')
        else:
            def num(k):
                if len(partes) <= k or not partes[k].isdigit():
                    return None
                n = int(partes[k])
                if n < 1 or n > len(lista) or (n - 1) in borrar:
                    return None
                return n
            if orden == 'STAT':
                vivos = [m for i, m in enumerate(lista) if i not in borrar]
                r('+OK %d %d' % (len(vivos), sum(m.tam() for m in vivos)))
            elif orden in ('LIST', 'UIDL'):
                def linea(i, m):
                    return '%d %s' % (i + 1, m.tam() if orden == 'LIST' else '%d.%d' % (cuenta.carpetas['INBOX'].uidvalidity, m.uid))
                if len(partes) > 1:
                    n = num(1)
                    if n is None:
                        r('-ERR No existe ese mensaje')
                    else:
                        r('+OK ' + linea(n - 1, lista[n - 1]))
                else:
                    r('+OK Lista')
                    conn.manda(''.join(linea(i, m) + '\r\n' for i, m in enumerate(lista) if i not in borrar).encode() + b'.\r\n')
            elif orden == 'RETR':
                n = num(1)
                if n is None:
                    r('-ERR No existe ese mensaje')
                    continue
                m = lista[n - 1]
                r('+OK %d octetos' % m.tam())
                multi(m.datos())
            elif orden == 'TOP':
                n = num(1)
                if n is None or len(partes) < 3 or not partes[2].isdigit():
                    r('-ERR TOP mensaje renglones')
                    continue
                cab, cuerpo = separa(lista[n - 1].datos())
                lineas = re.split(rb'(?<=\n)', cuerpo)[:int(partes[2])]
                r('+OK Cabeceras y %s renglones' % partes[2])
                multi(cab + b''.join(lineas))
            elif orden == 'DELE':
                n = num(1)
                if n is None:
                    r('-ERR No existe ese mensaje')
                    continue
                borrar.add(n - 1)
                r('+OK Marcado para borrar')
            elif orden == 'RSET':
                borrar.clear()
                r('+OK Nada marcado')
            else:
                r('-ERR Orden desconocida')


# ======================================================================
# autoconfiguración (clientConfig 1.1)
# ======================================================================
def xml_autoconfig():
    p = PUERTOS
    return ('''<?xml version="1.0" encoding="UTF-8"?>
<clientConfig version="1.1">
  <emailProvider id="prueba.test">
    <domain>prueba.test</domain>
    <displayName>Correo de pruebas de Hispa-Pentium</displayName>
    <displayShortName>Pruebas</displayShortName>
    <incomingServer type="imap">
      <hostname>10.0.2.2</hostname>
      <port>%d</port>
      <socketType>SSL</socketType>
      <username>%%EMAILADDRESS%%</username>
      <authentication>password-cleartext</authentication>
    </incomingServer>
    <incomingServer type="pop3">
      <hostname>10.0.2.2</hostname>
      <port>%d</port>
      <socketType>SSL</socketType>
      <username>%%EMAILADDRESS%%</username>
      <authentication>password-cleartext</authentication>
    </incomingServer>
    <outgoingServer type="smtp">
      <hostname>10.0.2.2</hostname>
      <port>%d</port>
      <socketType>SSL</socketType>
      <username>%%EMAILADDRESS%%</username>
      <authentication>password-cleartext</authentication>
    </outgoingServer>
    <outgoingServer type="smtp">
      <hostname>10.0.2.2</hostname>
      <port>%d</port>
      <socketType>STARTTLS</socketType>
      <username>%%EMAILADDRESS%%</username>
      <authentication>password-cleartext</authentication>
    </outgoingServer>
  </emailProvider>
</clientConfig>
''' % (p['imaps'], p['pop3s'], p['smtps'], p['submission'])).replace('\n', '\r\n').encode('utf-8')


class AtiendeHttp(http.server.BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'PruebasHispa/1.0'
    sys_version = ''

    def log_message(self, *a):
        pass

    def setup(self):
        try:
            if isinstance(self.request, ssl.SSLSocket):
                self.request.do_handshake()
        except (ssl.SSLError, OSError) as e:
            apunta(Conexion(None, self.server.nombre, True), 'fallo TLS: %s' % e)
        super().setup()

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        ruta = urllib.parse.urlsplit(self.path).path
        if ruta in ('/v1.1/prueba.test', '/.well-known/autoconfig/mail/config-v1.1.xml', '/mail/config-v1.1.xml'):
            codigo, d, tipo = 200, xml_autoconfig(), 'text/xml; charset=utf-8'
        else:
            codigo, d, tipo = 404, b'No existe\r\n', 'text/plain; charset=utf-8'
        apunta(Conexion(None, self.server.nombre, self.server.nombre == 'https'), '%s %s -> %d' % (self.command, self.path, codigo))
        self.send_response(codigo)
        self.send_header('Content-Type', tipo)
        self.send_header('Content-Length', str(len(d)))
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(d)


class ServidorHttp(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


# ======================================================================
# los servidores
# ======================================================================
class Manejador(socketserver.BaseRequestHandler):
    def handle(self):
        nombre, ctx, fn = self.server.info
        sock = self.request
        conn = Conexion(sock, nombre, ctx is not None)
        try:
            sock.settimeout(30)
            if ctx is not None:
                try:
                    sock = ctx.wrap_socket(sock, server_side=True)
                except (ssl.SSLError, OSError) as e:
                    apunta(conn, 'fallo TLS: %s' % e)
                    return
                conn.sock = sock
            sock.settimeout(1800)
            apunta(conn, 'conexión nueva desde %s:%d%s' % (self.client_address[0], self.client_address[1],
                                                         ' (%s, %s)' % (sock.version(), sock.cipher()[0]) if ctx else ''))
            fn(conn)
            apunta(conn, 'conexión acabada')
        except Cortada as e:
            apunta(conn, 'conexión cortada: %s' % e)
        except (socket.timeout, TimeoutError):
            apunta(conn, 'conexión cortada: tiempo agotado')
        except Exception:
            apunta(conn, 'ERROR del servidor: ' + traceback.format_exc().replace('\n', ' | '))
        finally:
            try:
                conn.sock.close()
            except OSError:
                pass


class ServidorTcp(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True
    request_queue_size = 64


SERVIDORES = []


def lanza(nombre, ctx, fn):
    s = ServidorTcp(('127.0.0.1', 0), Manejador)
    s.info = (nombre, ctx, fn)
    PUERTOS[nombre] = s.server_address[1]
    SERVIDORES.append(s)
    threading.Thread(target=s.serve_forever, daemon=True, name=nombre).start()


def lanza_http(nombre, ctx):
    s = ServidorHttp(('127.0.0.1', 0), AtiendeHttp)
    s.nombre = nombre
    if ctx:
        s.socket = ctx.wrap_socket(s.socket, server_side=True, do_handshake_on_connect=False)
    PUERTOS[nombre] = s.server_address[1]
    SERVIDORES.append(s)
    threading.Thread(target=s.serve_forever, daemon=True, name=nombre).start()


def imap(modo):
    return lambda conn: SesionImap(conn, modo).corre()


lanza('imaps', CTX['bueno'], imap('tls'))
lanza('imap', None, imap('starttls'))
lanza('imap_sin_starttls', None, imap('sin_starttls'))
lanza('imap_plano_local', None, imap('plano_local'))
lanza('imaps_otro', CTX['otro'], imap('tls'))
lanza('imaps_extrano', CTX['extrano'], imap('tls'))
lanza('smtps', CTX['bueno'], lambda conn: atiende_smtp(conn, False))
lanza('submission', None, lambda conn: atiende_smtp(conn, True))
lanza('pop3s', CTX['bueno'], lambda conn: atiende_pop3(conn, False))
lanza('pop3', None, lambda conn: atiende_pop3(conn, True))
lanza_http('https', CTX['bueno'])
lanza_http('http', None)

FIN = threading.Event()
signal.signal(signal.SIGTERM, lambda *a: FIN.set())
signal.signal(signal.SIGINT, lambda *a: FIN.set())
apunta(None, 'en marcha (pid %d): %s' % (os.getpid(), ' '.join('%s=%d' % kv for kv in PUERTOS.items())))
with open(os.path.join(CARPETA, 'puertos.tmp'), 'w') as f:
    f.write(''.join('%s=%d\n' % kv for kv in PUERTOS.items()))
os.rename(os.path.join(CARPETA, 'puertos.tmp'), os.path.join(CARPETA, 'puertos'))
while not FIN.wait(1):
    pass
for s in SERVIDORES:
    try:
        s.socket.close()
    except OSError:
        pass
apunta(None, 'parado')
REGISTRO.close()
sys.exit(0)
