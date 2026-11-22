#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""nucleo32/correo/pruebas/prueba_servidor.py [--grande N]

Prueba rápida, desde Linux, del servidor de correo de pruebas
(servidor_correo.py): lo arranca en un subproceso con una carpeta temporal,
lo prueba con imaplib, smtplib, poplib y ssl (con RAIZ.CER como única
autoridad) y lo para por su PID. Un renglón por prueba:
  «  OK   texto» o «  FAIL texto»; acaba con 0 si todo va bien.
"""
import base64
import imaplib
import os
import poplib
import re
import shutil
import signal
import smtplib
import socket
import ssl
import subprocess
import sys
import tempfile
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
GRANDE = 10000
if '--grande' in sys.argv:
    GRANDE = int(sys.argv[sys.argv.index('--grande') + 1])
FALLOS = 0


def ok(si, texto):
    global FALLOS
    if si:
        print('  OK   ' + texto)
    else:
        FALLOS += 1
        print('  FAIL ' + texto)
    sys.stdout.flush()


def prueba(texto):
    """Decorador: si la prueba revienta, FAIL con la excepción."""
    def envuelve(f):
        try:
            f()
        except Exception as e:
            ok(False, '%s (%s: %s)' % (texto, type(e).__name__, e))
        return f
    return envuelve


CARPETA = tempfile.mkdtemp(prefix='correo_pruebas_')
proc = subprocess.Popen([sys.executable, '-I', os.path.join(AQUI, 'servidor_correo.py'), CARPETA, '--grande', str(GRANDE)],
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
P = {}
t0 = time.time()
while time.time() - t0 < 60:
    if os.path.exists(os.path.join(CARPETA, 'puertos')):
        for renglon in open(os.path.join(CARPETA, 'puertos')):
            n, v = renglon.strip().split('=')
            P[n] = int(v)
        break
    if proc.poll() is not None:
        break
    time.sleep(0.05)
ok(bool(P), 'el servidor arranca y deja «puertos» (%.1f s, %d puertos)' % (time.time() - t0, len(P)))
if not P:
    print(proc.stdout.read().decode('utf-8', 'replace'))
    sys.exit(1)

CTX = ssl.create_default_context(cadata=open(os.path.join(CARPETA, 'RAIZ.CER'), 'rb').read())
H = '127.0.0.1'


def registro():
    return open(os.path.join(CARPETA, 'registro.txt'), encoding='utf-8').read()


ok(set(P) == {'imaps', 'imap', 'imap_sin_starttls', 'imap_plano_local', 'imaps_otro', 'imaps_extrano', 'smtps', 'submission',
              'pop3s', 'pop3', 'https', 'http'}, 'están todos los puertos: ' + ' '.join(sorted(P)))


def uid_de(M, asunto):
    typ, d = M.uid('SEARCH', 'SUBJECT', '"%s"' % asunto)
    l = d[0].split()
    return l[0].decode() if l else None


@prueba('imaps: LOGIN')
def _():
    M = imaplib.IMAP4_SSL(H, P['imaps'], ssl_context=CTX)
    typ, _d = M.login('ana@prueba.test', 'secreta1')
    ok(typ == 'OK', 'imaps: LOGIN de ana con TLS (%s)' % M.sock.version())
    M.logout()


@prueba('imaps: clave mala')
def _():
    M = imaplib.IMAP4_SSL(H, P['imaps'], ssl_context=CTX)
    try:
        M.login('ana', 'mala')
        ok(False, 'imaps: clave mala rechazada')
    except imaplib.IMAP4.error as e:
        ok('AUTHENTICATIONFAILED' in str(e), 'imaps: clave mala rechazada ([AUTHENTICATIONFAILED])')
    typ, _d = M.authenticate('PLAIN', lambda _r: b'\x00ana\x00secreta1')
    ok(typ == 'OK', 'imaps: AUTHENTICATE PLAIN sin @ («ana»)')
    M.logout()


@prueba('imap: STARTTLS')
def _():
    M = imaplib.IMAP4(H, P['imap'])
    ok('LOGINDISABLED' in M.capabilities and 'STARTTLS' in M.capabilities, 'imap: sin cifrar anuncia STARTTLS y LOGINDISABLED')
    try:
        M.login('ana@prueba.test', 'secreta1')
        ok(False, 'imap: LOGIN sin cifrar rechazado')
    except imaplib.IMAP4.error as e:
        ok('PRIVACYREQUIRED' in str(e), 'imap: LOGIN sin cifrar rechazado ([PRIVACYREQUIRED])')
    ok('LOGIN SIN CIFRAR' in registro(), 'registro: apunta «LOGIN SIN CIFRAR»')
    M.logout()
    M = imaplib.IMAP4(H, P['imap'])
    M.starttls(ssl_context=CTX)
    typ, _d = M.login('ana@prueba.test', 'secreta1')
    ok(typ == 'OK', 'imap: STARTTLS y LOGIN')
    M.logout()


@prueba('imap_sin_starttls')
def _():
    M = imaplib.IMAP4(H, P['imap_sin_starttls'])
    ok('STARTTLS' not in M.capabilities, 'imap_sin_starttls: no ofrece STARTTLS')
    M.logout()
    M = imaplib.IMAP4(H, P['imap_plano_local'])
    antes = registro().count('[imap_plano_local plano] LOGIN SIN CIFRAR')
    typ, _d = M.login('luis', 'clave-luis')
    ok(typ == 'OK' and registro().count('[imap_plano_local plano] LOGIN SIN CIFRAR') == antes + 1,
       'imap_plano_local: LOGIN sin cifrar entra y queda apuntado')
    M.logout()


M = imaplib.IMAP4_SSL(H, P['imaps'], ssl_context=CTX)
M.login('ana@prueba.test', 'secreta1')


@prueba('LIST')
def _():
    typ, d = M.list('""', '*')
    t = b'\n'.join(d).decode()
    ok(typ == 'OK' and '\\Sent' in t and '\\Drafts' in t and '\\Trash' in t and '\\Junk' in t and '\\Archive' in t,
       'LIST "" "*" con SPECIAL-USE (%d carpetas)' % len(d))
    ok('"Asociaci&APM-n/Cuotas"' in t and '(\\HasChildren) "/" "Asociaci&APM-n"' in t, 'LIST: «Asociación/Cuotas» en UTF-7 y \\HasChildren')
    typ, d = M._simple_command('LIST', '(SPECIAL-USE)', '""', '"*"')
    typ, d = M._untagged_response(typ, d, 'LIST')
    ok(typ == 'OK' and len(d) == 5, 'LIST (SPECIAL-USE) "" "*": 5 carpetas')
    typ, d = M.list('""', '%')
    ok(typ == 'OK' and not any(b'Cuotas' in x for x in d), 'LIST "" "%" no entra en las hijas')
    typ, d = M.status('"Asociaci&APM-n/Cuotas"', '(MESSAGES UNSEEN UIDNEXT UIDVALIDITY RECENT)')
    ok(typ == 'OK' and b'MESSAGES 2' in d[0] and b'UNSEEN 1' in d[0], 'STATUS de la carpeta UTF-7: %s' % d[0].decode())


@prueba('SELECT')
def _():
    typ, d = M.select('INBOX')
    ok(typ == 'OK' and int(d[0]) == 49, 'SELECT INBOX: %s mensajes' % d[0].decode())
    ok('UIDVALIDITY' in M.untagged_responses and 'UIDNEXT' in M.untagged_responses, 'SELECT: UIDVALIDITY y UIDNEXT')


@prueba('UID FETCH de cabeceras')
def _():
    typ, d = M.uid('FETCH', '1:*', '(UID FLAGS RFC822.SIZE BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID)])')
    n = sum(1 for x in d if isinstance(x, tuple))
    ok(typ == 'OK' and n == 49, 'UID FETCH 1:* de cabeceras: %d' % n)
    typ, d = M.uid('FETCH', '1:*', '(ENVELOPE BODYSTRUCTURE)')
    ok(typ == 'OK' and len(d) >= 49, 'UID FETCH 1:* (ENVELOPE BODYSTRUCTURE)')
    u = uid_de(M, 'Caso 26')
    typ, d = M.uid('FETCH', u, '(BODY.PEEK[2.HEADER] BODY.PEEK[2.TEXT] BODY.PEEK[1.MIME] BODY.PEEK[]<0.20>)')
    t = b''.join(x[1] for x in d if isinstance(x, tuple))
    ok(typ == 'OK' and b'Acta de la asamblea' in t and b'25 \xe2\x82\xac' in t, 'FETCH de las partes del message/rfc822 (2.HEADER, 2.TEXT)')
    typ, d = M.uid('FETCH', u, '(FLAGS)')
    ok(b'\\Seen' not in d[0], 'BODY.PEEK no pone \\Seen')
    typ, d = M.uid('FETCH', u, '(BODY[TEXT])')
    ok(b'\\Seen' in b''.join(x[0] if isinstance(x, tuple) else x for x in d), 'BODY[TEXT] pone \\Seen')


@prueba('SEARCH')
def _():
    u2 = uid_de(M, 'Caso 02')
    u3 = uid_de(M, 'Caso 03')
    M.literal = 'año'.encode('utf-8')
    typ, d = M.uid('SEARCH', 'CHARSET', 'UTF-8', 'BODY')
    l = d[0].decode().split()
    ok(typ == 'OK' and u2 in l and u3 in l, 'UID SEARCH CHARSET UTF-8 BODY «año» encuentra el QP y el ISO-8859-1 (%d)' % len(l))
    M.literal = 'ñandú'.encode('utf-8')
    typ, d = M.uid('SEARCH', 'CHARSET', 'UTF-8', 'SUBJECT')
    ok(typ == 'OK' and uid_de(M, 'Caso 08') in d[0].decode().split(), 'SEARCH SUBJECT «ñandú» (RFC 2047 Q)')
    typ, d = M.search(None, 'OR', 'FLAGGED', '(FROM "jose" NOT SEEN)', 'SINCE', '1-Mar-2026')
    ok(typ == 'OK' and len(d[0].split()) >= 2, 'SEARCH OR / NOT / paréntesis / SINCE: %s' % d[0].decode())


@prueba('STORE')
def _():
    u = uid_de(M, 'Caso 01')
    typ, d = M.uid('STORE', u, '+FLAGS', '(\\Flagged $Forwarded)')
    ok(typ == 'OK' and b'\\Flagged' in d[0] and b'$Forwarded' in d[0], 'UID STORE +FLAGS (\\Flagged $Forwarded)')
    typ, d = M.uid('STORE', u, '-FLAGS.SILENT', '(\\Flagged)')
    typ, d = M.uid('FETCH', u, '(FLAGS)')
    ok(b'\\Flagged' not in d[0], 'UID STORE -FLAGS.SILENT')


@prueba('MOVE')
def _():
    u = uid_de(M, 'Caso 20')
    typ, d = M.uid('MOVE', u, 'Archive')
    ok(typ == 'OK' and 'COPYUID' in str(M.untagged_responses.get('OK', b'')) + str(d), 'UID MOVE a Archive con [COPYUID]')
    typ, d = M.status('Archive', '(MESSAGES)')
    ok(b'MESSAGES 3' in d[0], 'MOVE: Archive tiene 3')


@prueba('APPEND')
def _():
    msg = b'From: ana@prueba.test\r\nTo: marta@ejemplo.es\r\nSubject: Borrador de la prueba\r\n\r\nHola.\r\n'
    typ, d = M.append('Drafts', '(\\Draft \\Seen)', '"22-Nov-2026 10:00:00 +0100"', msg)
    ok(typ == 'OK' and b'APPENDUID' in d[0], 'APPEND a Drafts con [APPENDUID]: %s' % d[0].decode())
    typ, d = M.append('NoExiste', None, None, msg)
    ok(typ == 'NO' and b'TRYCREATE' in d[0], 'APPEND a una carpeta que no existe: [TRYCREATE]')


ENVIADO_IDLE = b'From: luis@prueba.test\r\nTo: ana@prueba.test\r\nSubject: Para el IDLE\r\n\r\nHola, Ana.\r\n.empieza por punto\r\n..y por dos\r\n'


class Crudo:
    """Un cliente IMAP a mano (para IDLE)."""

    def __init__(self, puerto):
        self.s = CTX.wrap_socket(socket.create_connection((H, puerto)), server_hostname=H)
        self.f = self.s.makefile('rb')
        self.f.readline()
        self.n = 0

    def orden(self, t):
        self.n += 1
        tag = 'c%d' % self.n
        self.s.sendall(('%s %s\r\n' % (tag, t)).encode())
        r = []
        while True:
            l = self.f.readline()
            r.append(l)
            if l.startswith(tag.encode() + b' '):
                return r


@prueba('IDLE')
def _():
    c = Crudo(P['imaps'])
    c.orden('LOGIN ana secreta1')
    r = c.orden('SELECT INBOX')
    existen = int([l for l in r if l.endswith(b'EXISTS\r\n')][0].split()[1])
    c.s.sendall(b'i1 IDLE\r\n')
    ok(c.f.readline().startswith(b'+ '), 'IDLE: «+ idling»')
    t = time.time()
    s = smtplib.SMTP_SSL(H, P['smtps'], context=CTX)
    s.login('luis@prueba.test', 'clave-luis')
    s.sendmail('luis@prueba.test', ['ana@prueba.test'], ENVIADO_IDLE)   # smtplib pone el relleno de puntos
    s.quit()
    c.s.settimeout(5)
    l = c.f.readline()
    ok(l == b'* %d EXISTS\r\n' % (existen + 1), 'IDLE: «%s» al enviar por smtps a ana (%.2f s)' % (l.decode().strip(), time.time() - t))
    c.s.sendall(b'DONE\r\n')
    while True:
        l = c.f.readline()
        if l.startswith(b'i1 '):
            break
    ok(l.startswith(b'i1 OK'), 'IDLE: DONE')
    eml = open(os.path.join(CARPETA, 'enviados', '0001.eml'), 'rb').read()
    ok(eml == ENVIADO_IDLE, 'smtps: guardado en enviados/0001.eml, exacto (sin el relleno de puntos)')
    env = open(os.path.join(CARPETA, 'enviados', '0001.env'), 'rb').read()
    ok(env == b'MAIL FROM:<luis@prueba.test>\r\nRCPT TO:<ana@prueba.test>\r\n', 'smtps: el sobre en 0001.env')
    c.orden('LOGOUT')


@prueba('POP3S')
def _():
    p = poplib.POP3_SSL(H, P['pop3s'], context=CTX)
    p.user('ana@prueba.test')
    p.pass_('secreta1')
    n, tam = p.stat()
    r, lineas, _o = p.retr(1)
    ok(n >= 49 and b'Caso 01' in b'\n'.join(lineas), 'POP3S: STAT (%d mensajes, %d octetos) y RETR 1' % (n, tam))
    r, lineas, _o = p.top(2, 0)
    ok(any(l.startswith(b'Subject:') for l in lineas), 'POP3S: TOP 2 0')
    ok(len(p.uidl()[1]) == n, 'POP3S: UIDL')
    p.quit()
    p = poplib.POP3(H, P['pop3'])
    ok('STLS' in p.capa(), 'pop3: CAPA con STLS')
    p.stls(context=CTX)
    p.user('luis')
    p.pass_('clave-luis')
    ok(p.stat()[0] >= 1, 'pop3: STLS y entrar')
    p.quit()


@prueba('submission')
def _():
    s = smtplib.SMTP(H, P['submission'])
    s.ehlo()
    ok(s.has_extn('starttls') and not s.has_extn('auth'), 'submission: antes de TLS, STARTTLS sí y AUTH no')
    try:
        s.sendmail('ana@prueba.test', ['luis@prueba.test'], b'Subject: x\r\n\r\nx\r\n')
        ok(False, 'submission: sin AUTH, 530')
    except smtplib.SMTPSenderRefused as e:
        ok(e.smtp_code == 530, 'submission: sin AUTH, 530')
    s.starttls(context=CTX)
    s.ehlo()
    ok(s.has_extn('auth') and 'LOGIN' in s.esmtp_features['auth'], 'submission: tras STARTTLS, AUTH PLAIN LOGIN')
    s.user, s.password = 'ana@prueba.test', 'secreta1'
    code, _r = s.auth('LOGIN', s.auth_login, initial_response_ok=False)
    ok(code == 235, 'submission: AUTH LOGIN')
    msg = b'From: ana@prueba.test\r\nTo: luis@prueba.test\r\nSubject: Por submission\r\n\r\nHola, Luis.\r\n'
    s.sendmail('ana@prueba.test', ['luis@prueba.test', 'marta@ejemplo.es'], msg)
    try:
        s.sendmail('ana@prueba.test', ['nadie@prueba.test'], msg)
        ok(False, 'submission: destinatario local que no existe, 550')
    except smtplib.SMTPRecipientsRefused as e:
        ok(list(e.recipients.values())[0][0] == 550, 'submission: destinatario local que no existe, 550')
    s.quit()
    ok(open(os.path.join(CARPETA, 'enviados', '0002.eml'), 'rb').read() == msg, 'submission: enviados/0002.eml exacto')
    L = imaplib.IMAP4_SSL(H, P['imaps'], ssl_context=CTX)
    L.login('luis', 'clave-luis')
    typ, d = L.select('INBOX', readonly=True)
    ok(int(d[0]) == 2, 'submission: entregado en el INBOX de luis')
    L.logout()


@prueba('certificados')
def _():
    for nombre, esperado in (('imaps_otro', 'mismatch'), ('imaps_extrano', 'unable to get local issuer')):
        try:
            CTX.wrap_socket(socket.create_connection((H, P[nombre])), server_hostname=H).close()
            ok(False, '%s: la verificación falla' % nombre)
        except ssl.SSLCertVerificationError as e:
            ok(esperado in str(e).lower() or esperado in str(e.verify_message).lower(), '%s: la verificación falla (%s)' % (nombre, e.verify_message))
    s = CTX.wrap_socket(socket.create_connection((H, P['imaps'])), server_hostname='localhost')
    ok(True, 'imaps: el certificado vale para «localhost» (%s)' % s.version())
    s.close()
    c12 = ssl.create_default_context(cadata=open(os.path.join(CARPETA, 'RAIZ.CER'), 'rb').read())
    c12.maximum_version = ssl.TLSVersion.TLSv1_2
    c12.set_ciphers('ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256')
    s = c12.wrap_socket(socket.create_connection((H, P['smtps'])), server_hostname=H)
    ok(s.version() == 'TLSv1.2', 'smtps: TLS 1.2 con %s (lo de BearSSL)' % s.cipher()[0])
    s.close()


@prueba('autoconfiguración')
def _():
    import http.client
    c = http.client.HTTPSConnection(H, P['https'], context=CTX)
    c.request('GET', '/v1.1/prueba.test')
    r = c.getresponse()
    d = r.read().decode()
    ok(r.status == 200 and '<port>%d</port>' % P['imaps'] in d and 'STARTTLS' in d, 'https: /v1.1/prueba.test')
    c.request('GET', '/mail/config-v1.1.xml?emailaddress=ana%40prueba.test')
    r = c.getresponse()
    r.read()
    ok(r.status == 200, 'https: /mail/config-v1.1.xml?emailaddress=')
    c = http.client.HTTPConnection(H, P['http'])
    c.request('GET', '/.well-known/autoconfig/mail/config-v1.1.xml')
    r = c.getresponse()
    r.read()
    c.request('GET', '/otra')
    r2 = c.getresponse()
    r2.read()
    ok(r.status == 200 and r2.status == 404, 'http: .well-known (200) y lo demás (404)')


@prueba('grande')
def _():
    G = imaplib.IMAP4_SSL(H, P['imaps'], ssl_context=CTX)
    G.login('grande', 'grande1')
    t = time.time()
    typ, d = G.select('INBOX')
    n = int(d[0])
    total = 0
    for a in range(1, n + 1, 500):
        typ, d = G.fetch('%d:%d' % (a, min(n, a + 499)), '(UID FLAGS RFC822.SIZE BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])')
        total += sum(1 for x in d if isinstance(x, tuple))
    tc = time.time() - t
    ok(n == GRANDE and total == n, 'grande: cabeceras de los %d en tramos de 500: %.1f s' % (total, tc))
    typ, d = G.uid('SEARCH', 'UNSEEN')
    ok(typ == 'OK' and 0.2 * n < len(d[0].split()) < 0.4 * n, 'grande: UID SEARCH UNSEEN: %d sin leer' % len(d[0].split()))
    typ, d = G.uid('FETCH', '%d:*' % (n + 1000), '(UID)')
    ok(typ == 'OK' and len(d) == 1 and b'UID %d' % n in d[0], 'grande: UID FETCH n+1000:* da el último')
    G.logout()


M.logout()


@prueba('registro')
def _():
    r = registro()
    ok('[imaps tls] a1 LOGIN ana@prueba.test ****' in r or re.search(r'\[imaps tls\] \w+ LOGIN ana@prueba.test \*\*\*\*', r),
       'registro: «[imaps tls] TAG LOGIN ana@prueba.test ****»')
    ok(not any(c in r for c in ('secreta1', 'clave-luis', 'grande1', base64.b64encode(b'\x00ana\x00secreta1').decode(),
                                base64.b64encode(b'secreta1').decode())), 'registro: ninguna clave')
    ok('ERROR del servidor' not in r, 'registro: sin errores del servidor')
    if 'ERROR del servidor' in r:
        for l in r.splitlines():
            if 'ERROR del servidor' in l:
                print('       ' + l[:2000])


os.kill(proc.pid, signal.SIGTERM)
try:
    rc = proc.wait(10)
except subprocess.TimeoutExpired:
    proc.kill()
    rc = None
ok(rc == 0, 'el servidor acaba limpio con SIGTERM (%s)' % rc)
salida = proc.stdout.read().decode('utf-8', 'replace').strip()
if salida:
    print(salida)
shutil.rmtree(CARPETA, ignore_errors=True)
print('%s: %d fallos' % ('BIEN' if not FALLOS else 'MAL', FALLOS))
sys.exit(1 if FALLOS else 0)
