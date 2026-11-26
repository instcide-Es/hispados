#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""tls_delante.py CARPETA PUERTO_PHP

(TERMO) Un HTTPS de prueba delante del receptor de «Enviar a Carlos»
(tools/termo-subida/subir.php, con «php -S 127.0.0.1:PUERTO_PHP»), como lo
pondría nginx: hace con openssl una raíz de prueba (CARPETA/RAIZ.CER, en DER,
para \\RAICES de la memoria) y un certificado para 10.0.2.2 (lo que es el
anfitrión desde QEMU), escucha en 127.0.0.1 (el puerto, en CARPETA/puerto) y
pasa cada conexión, ya sin cifrar, al PHP. Nada sale de esta máquina.
Con TLS_CADENA=le, una cadena como la de Let's Encrypt de hoy (ECDSA:
hoja <- YE1 <- Root YE cruzada por X2 <- X2 cruzada por X1), con Root YE de
ancla (RAIZ.CER).
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


CADENA_LE = os.environ.get('TLS_CADENA') == 'le'
open(os.path.join(C, 'srv.ext'), 'w').write('subjectAltName=DNS:10.0.2.2,IP:10.0.2.2\nbasicConstraints=CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\n')
if not CADENA_LE:
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'raiz.key')
    ossl('req', '-x509', '-new', '-key', 'raiz.key', '-sha256', '-days', '3650', '-subj', '/O=Hispa-DOS pruebas/CN=Raiz del Termometro',
         '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', 'raiz.pem')
    ossl('x509', '-in', 'raiz.pem', '-outform', 'DER', '-out', 'RAIZ.CER')
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'srv.key')
    ossl('req', '-new', '-key', 'srv.key', '-subj', '/CN=10.0.2.2', '-out', 'srv.csr')
    ossl('x509', '-req', '-in', 'srv.csr', '-CA', 'raiz.pem', '-CAkey', 'raiz.key', '-CAcreateserial', '-days', '3650', '-sha256',
         '-extfile', 'srv.ext', '-out', 'srv.pem')
    cadena = 'srv.pem'
else:
    # (TERMO 1.2.1) TLS_CADENA=le: una cadena como la de Let's Encrypt de
    # noviembre de 2026 para hispados.es (ECDSA): la hoja (P-256) <- YE1
    # (P-384, SHA-384) <- Root YE cruzada por X2 <- X2 cruzada por X1 (RSA).
    # El ancla, en \RAICES, solo Root YE (la autofirmada); las cruzadas van
    # en la cadena, como las manda el servidor de verdad
    ca = open(os.path.join(C, 'ca.ext'), 'w')
    ca.write('basicConstraints=critical,CA:TRUE\nkeyUsage=critical,keyCertSign,cRLSign\n')
    ca.close()
    open(os.path.join(C, 'int.ext'), 'w').write('basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,digitalSignature,keyCertSign,cRLSign\nextendedKeyUsage=serverAuth\n')
    ossl('genrsa', '-out', 'x1.key', '2048')
    ossl('req', '-x509', '-new', '-key', 'x1.key', '-sha256', '-days', '3650', '-subj', '/C=US/O=ISRG pruebas/CN=Root X1 prueba',
         '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', 'x1.pem')
    ossl('ecparam', '-name', 'secp384r1', '-genkey', '-noout', '-out', 'x2.key')
    ossl('req', '-new', '-key', 'x2.key', '-subj', '/C=US/O=ISRG pruebas/CN=Root X2 prueba', '-out', 'x2.csr')
    ossl('x509', '-req', '-in', 'x2.csr', '-CA', 'x1.pem', '-CAkey', 'x1.key', '-CAcreateserial', '-days', '3650', '-sha256', '-extfile', 'ca.ext', '-out', 'x2-cruzada.pem')
    ossl('ecparam', '-name', 'secp384r1', '-genkey', '-noout', '-out', 'ye.key')
    ossl('req', '-x509', '-new', '-key', 'ye.key', '-sha384', '-days', '3650', '-subj', '/C=US/O=ISRG pruebas/CN=Root YE prueba',
         '-addext', 'basicConstraints=critical,CA:TRUE', '-addext', 'keyUsage=critical,keyCertSign,cRLSign', '-out', 'ye.pem')
    ossl('x509', '-in', 'ye.pem', '-outform', 'DER', '-out', 'RAIZ.CER')
    ossl('req', '-new', '-key', 'ye.key', '-subj', '/C=US/O=ISRG pruebas/CN=Root YE prueba', '-out', 'ye.csr')
    ossl('x509', '-req', '-in', 'ye.csr', '-CA', 'x2-cruzada.pem', '-CAkey', 'x2.key', '-CAcreateserial', '-days', '3650', '-sha384', '-extfile', 'ca.ext', '-out', 'ye-cruzada.pem')
    ossl('ecparam', '-name', 'secp384r1', '-genkey', '-noout', '-out', 'ye1.key')
    ossl('req', '-new', '-key', 'ye1.key', '-subj', '/C=US/O=Let\'s Encrypt pruebas/CN=YE1', '-out', 'ye1.csr')
    ossl('x509', '-req', '-in', 'ye1.csr', '-CA', 'ye.pem', '-CAkey', 'ye.key', '-CAcreateserial', '-days', '3650', '-sha384', '-extfile', 'int.ext', '-out', 'ye1.pem')
    ossl('ecparam', '-name', 'prime256v1', '-genkey', '-noout', '-out', 'srv.key')
    ossl('req', '-new', '-key', 'srv.key', '-subj', '/CN=10.0.2.2', '-out', 'srv.csr')
    ossl('x509', '-req', '-in', 'srv.csr', '-CA', 'ye1.pem', '-CAkey', 'ye1.key', '-CAcreateserial', '-days', '90', '-sha384',
         '-extfile', 'srv.ext', '-out', 'srv.pem')
    with open(os.path.join(C, 'cadena.pem'), 'w') as f:
        for n in ('srv.pem', 'ye1.pem', 'ye-cruzada.pem', 'x2-cruzada.pem'):
            f.write(open(os.path.join(C, n)).read())
    cadena = 'cadena.pem'
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
ctx.minimum_version = ssl.TLSVersion.TLSv1_2
ctx.load_cert_chain(os.path.join(C, cadena), os.path.join(C, 'srv.key'))
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
