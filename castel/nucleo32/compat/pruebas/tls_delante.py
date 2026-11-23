#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""tls_delante.py CARPETA PUERTO_PHP

El mismo de las pruebas del Termómetro (rama termometro: termo/pruebas/tls_delante.py).

(COMPAT) Un HTTPS de prueba delante del receptor de los informes del
Asistente (tools/compat-subida/informe.php, con «php -S 127.0.0.1:PUERTO_PHP»), como lo
pondría nginx, y de lo que imita a download.microsoft.com: hace con openssl
una raíz de prueba (CARPETA/RAIZ.CER, en DER,
para \\RAICES) y un certificado para 10.0.2.2 (lo que es el
anfitrión desde QEMU), escucha en 127.0.0.1 (el puerto, en CARPETA/puerto) y
pasa cada conexión, ya sin cifrar, al PHP. Nada sale de esta máquina.
"""
import os
import socket
import ssl
import subprocess
import sys
import threading

C, PP = sys.argv[1], int(sys.argv[2])
os.makedirs(C, exist_ok=True)


def ossl(*a):
    subprocess.run(['openssl'] + list(a), cwd=C, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'raiz.key')
ossl('req', '-x509', '-new', '-key', 'raiz.key', '-sha256', '-days', '3650', '-subj', '/O=Hispa-DOS pruebas/CN=Raiz del Asistente',
     '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', 'raiz.pem')
ossl('x509', '-in', 'raiz.pem', '-outform', 'DER', '-out', 'RAIZ.CER')
ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'srv.key')
ossl('req', '-new', '-key', 'srv.key', '-subj', '/CN=10.0.2.2', '-out', 'srv.csr')
open(os.path.join(C, 'srv.ext'), 'w').write('subjectAltName=DNS:10.0.2.2,IP:10.0.2.2\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n')
ossl('x509', '-req', '-in', 'srv.csr', '-CA', 'raiz.pem', '-CAkey', 'raiz.key', '-CAcreateserial', '-days', '3650', '-sha256',
     '-extfile', 'srv.ext', '-out', 'srv.pem')
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(os.path.join(C, 'srv.pem'), os.path.join(C, 'srv.key'))
ls = socket.socket()
ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(('127.0.0.1', 0))
ls.listen(8)
open(os.path.join(C, 'puerto'), 'w').write(str(ls.getsockname()[1]))
registro = open(os.path.join(C, 'tls.txt'), 'a')


def pasa(a, b):
    try:
        while True:
            d = a.recv(65536)
            if not d:
                break
            b.sendall(d)
    except OSError:
        pass
    try:
        b.shutdown(socket.SHUT_WR)
    except OSError:
        pass


def atiende(c):
    try:
        s = ctx.wrap_socket(c, server_side=True)
        registro.write('conexión TLS: %s %s\n' % (s.version(), s.cipher()[0]))
        registro.flush()
        p = socket.create_connection(('127.0.0.1', PP))
        h = threading.Thread(target=pasa, args=(p, s), daemon=True)
        h.start()
        pasa(s, p)
        h.join(60)
        s.close()
        p.close()
    except (OSError, ssl.SSLError) as e:
        registro.write('fallo: %s\n' % e)
        registro.flush()


while True:
    c, _ = ls.accept()
    threading.Thread(target=atiende, args=(c,), daemon=True).start()
