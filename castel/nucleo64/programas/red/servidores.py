#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/red/servidores.py - Hispa-64, etapa 9: los servidores
# de prueba de los programas de red (DESCARGA, NTP, ECO), para arrancarlos
# en el anfitrión (solo la biblioteca estándar de Python 3):
#   servidores.py http PUERTO ARCHIVO
#       HTTP/1.1: ARCHIVO en cualquier ruta, con Content-Length; /trozos, lo
#       mismo con «Transfer-Encoding: chunked»; /redir, 302 a /; /redir5 y
#       /redir6, 5 y 6 redirecciones seguidas; /no, 404
#   servidores.py https PUERTO ARCHIVO CERT.pem CLAVE.pem
#       lo mismo, con TLS
#   servidores.py ntp PUERTO DESFASE_S
#       SNTP (RFC 4330): contesta con la hora del anfitrión más DESFASE_S
#       segundos (con decimales; negativo, atrasada)
#   servidores.py eco PUERTO
#       eco por TCP (cada conexión, en un hilo)
#   servidores.py certs CARPETA NOMBRE [ec|rsa]
#       con openssl: una autoridad de prueba (ca.pem, ca.der, ca.key) y un
#       certificado de servidor para NOMBRE (DNS en el SAN; servidor.pem,
#       servidor.key), ECDSA P-256 (ec, por omisión) o RSA 2048, válidos 10
#       años desde ayer
# PUERTO 0: el que dé el sistema. Cada servidor escribe «listo PUERTO» (el
# de verdad) al arrancar y se para solo a los 600 s.
import os
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from socketserver import StreamRequestHandler, ThreadingTCPServer

VIDA_S = 600


def listo(puerto):
    print("listo %d" % puerto, flush=True)


def para_solo():
    t = threading.Timer(VIDA_S, lambda: os._exit(0))
    t.daemon = True
    t.start()


def http(puerto, archivo, cert=None, clave=None):
    with open(archivo, "rb") as f:
        datos = f.read()

    class Pedido(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        def redirige(self, a):
            self.send_response(302)
            self.send_header("Location", a)
            self.send_header("Content-Length", "0")
            self.end_headers()

        def do_GET(self):
            ruta = self.path.split("?")[0]
            if ruta == "/redir":
                return self.redirige("/")
            if ruta.startswith("/redir") and ruta[6:].isdigit():
                n = int(ruta[6:])
                return self.redirige("/redir%d" % (n - 1) if n > 1 else "/")
            if ruta == "/no":
                cuerpo = b"no hay\n"
                self.send_response(404)
                self.send_header("Content-Length", str(len(cuerpo)))
                self.end_headers()
                self.wfile.write(cuerpo)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            if ruta == "/trozos":
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                i, tam = 0, 1
                while i < len(datos):
                    t = datos[i:i + tam]
                    self.wfile.write(b"%x\r\n" % len(t) + t + b"\r\n")
                    i += len(t)
                    tam = min(tam * 3 + 7, 40000)
                self.wfile.write(b"0\r\n\r\n")
            else:
                self.send_header("Content-Length", str(len(datos)))
                self.end_headers()
                self.wfile.write(datos)
            self.wfile.flush()

    s = ThreadingHTTPServer(("0.0.0.0", puerto), Pedido)
    s.daemon_threads = True
    if cert:
        c = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        c.load_cert_chain(cert, clave)
        s.socket = c.wrap_socket(s.socket, server_side=True)
    listo(s.server_address[1])
    para_solo()
    s.serve_forever()


DE_1900 = 2208988800


def marca(t):
    s = int(t)
    return struct.pack("!II", (s + DE_1900) & 0xffffffff, int((t - s) * 4294967296) & 0xffffffff)


def ntp(puerto, desfase):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("0.0.0.0", puerto))
    listo(s.getsockname()[1])
    para_solo()
    while True:
        p, quien = s.recvfrom(512)
        llega = time.time() + desfase
        if len(p) < 48 or (p[0] & 7) != 3:
            continue
        vn = (p[0] >> 3) & 7
        r = bytes([(0 << 6) | (vn << 3) | 4, 2, p[2], 0xec])       # sin aviso, servidor, estrato 2
        r += struct.pack("!II", 0, 0)                               # retardo y dispersión raíz
        r += b"HISP"                                                # el de referencia
        r += marca(llega - 10)                                      # la última puesta en hora
        r += p[40:48]                                               # el de salida del cliente
        r += marca(llega)
        r += marca(time.time() + desfase)
        s.sendto(r, quien)


def eco(puerto):
    class Eco(StreamRequestHandler):
        def handle(self):
            while True:
                d = self.request.recv(65536)
                if not d:
                    break
                self.request.sendall(d)

    ThreadingTCPServer.allow_reuse_address = True
    ThreadingTCPServer.daemon_threads = True
    s = ThreadingTCPServer(("0.0.0.0", puerto), Eco)
    listo(s.server_address[1])
    para_solo()
    s.serve_forever()


def openssl(*a):
    subprocess.run(["/usr/bin/openssl"] + list(a), check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def certs(carpeta, nombre, tipo="ec"):
    os.makedirs(carpeta, exist_ok=True)
    c = lambda n: os.path.join(carpeta, n)
    clave = ["-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256"] if tipo == "ec" else ["-newkey", "rsa:2048"]
    desde = time.strftime("%Y%m%d%H%M%SZ", time.gmtime(time.time() - 86400))
    hasta = time.strftime("%Y%m%d%H%M%SZ", time.gmtime(time.time() - 86400 + 3650 * 86400))
    with tempfile.TemporaryDirectory() as t:
        # «openssl ca» deja poner el principio de la validez (ayer: por si el
        # reloj del que comprueba va un poco atrasado); x509 -req, no
        open(os.path.join(t, "index.txt"), "w").close()
        with open(os.path.join(t, "serial"), "w") as f:
            f.write("%016X\n" % int(time.time() * 1000))
        cfg = os.path.join(t, "ca.cnf")
        with open(cfg, "w") as f:
            f.write("""[ ca ]
default_ca = prueba
[ prueba ]
dir = %s
database = $dir/index.txt
serial = $dir/serial
new_certs_dir = $dir
default_md = sha256
policy = libre
unique_subject = no
copy_extensions = none
[ libre ]
commonName = supplied
[ ext_ca ]
basicConstraints = critical,CA:TRUE
keyUsage = critical,keyCertSign,cRLSign
subjectKeyIdentifier = hash
[ ext_servidor ]
basicConstraints = critical,CA:FALSE
keyUsage = critical,digitalSignature,keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = DNS:%s
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid
""" % (t, nombre))
        openssl("req", "-new", *clave, "-nodes", "-keyout", c("ca.key"), "-out", os.path.join(t, "ca.csr"),
                "-subj", "/CN=Hispa-64 autoridad de prueba")
        openssl("ca", "-batch", "-config", cfg, "-selfsign", "-keyfile", c("ca.key"), "-in", os.path.join(t, "ca.csr"),
                "-out", c("ca.pem"), "-startdate", desde, "-enddate", hasta, "-extensions", "ext_ca", "-notext")
        openssl("x509", "-in", c("ca.pem"), "-outform", "DER", "-out", c("ca.der"))
        openssl("req", "-new", *clave, "-nodes", "-keyout", c("servidor.key"), "-out", os.path.join(t, "s.csr"),
                "-subj", "/CN=" + nombre)
        openssl("ca", "-batch", "-config", cfg, "-cert", c("ca.pem"), "-keyfile", c("ca.key"), "-in", os.path.join(t, "s.csr"),
                "-out", c("servidor.pem"), "-startdate", desde, "-enddate", hasta, "-extensions", "ext_servidor", "-notext")
    print("certs: %s: ca.pem, ca.der, ca.key, servidor.pem, servidor.key (%s, para «%s», de %s a %s)"
          % (carpeta, "ECDSA P-256" if tipo == "ec" else "RSA 2048", nombre, desde, hasta), flush=True)


def main(a):
    try:
        if len(a) >= 3 and a[0] == "http":
            return http(int(a[1]), a[2])
        if len(a) >= 5 and a[0] == "https":
            return http(int(a[1]), a[2], a[3], a[4])
        if len(a) >= 3 and a[0] == "ntp":
            return ntp(int(a[1]), float(a[2]))
        if len(a) >= 2 and a[0] == "eco":
            return eco(int(a[1]))
        if len(a) >= 3 and a[0] == "certs":
            return certs(a[1], a[2], a[3] if len(a) >= 4 else "ec")
    except subprocess.CalledProcessError as e:
        sys.exit("servidores.py: openssl falla: %s" % e.stderr.decode(errors="replace").strip())
    sys.exit("uso: servidores.py http PUERTO ARCHIVO | https PUERTO ARCHIVO CERT.pem CLAVE.pem |\n"
             "     ntp PUERTO DESFASE_S | eco PUERTO | certs CARPETA NOMBRE [ec|rsa]")


if __name__ == "__main__":
    main(sys.argv[1:])
