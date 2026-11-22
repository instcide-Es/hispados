#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""pruebas/fuzz_imap.py PROGRAMA VUELTAS [SEMILLA]

El fuzz del analizador de respuestas IMAP de Correo (imap.c): lo que diría un
servidor malo o roto. Hace respuestas de verdad (saludo, CAPABILITY, LIST con
SPECIAL-USE y UTF-7, STATUS, SELECT, SEARCH, FETCH con literales, cabeceras y
mensajes, APPEND, IDLE) y las estropea al azar (octetos cambiados, cortes,
literales que mienten —{4294967295}, {99999999}, {5} con 2 octetos—,
paréntesis sin fin, 50.000 paréntesis anidados, números enormes, átomos de
1 MB, comillas sin cerrar, \\ al final, NUL, renglones sin CRLF) y se las da a
«PROGRAMA /ORDEN imap-respuestas ARCHIVO» (Correo en Linux, con ASan y
UBSan: anfitrion.sh). Cada ejecución, con un tope de tiempo (lo cuadrático se
nota). Dice «  OK   ...» o «  FAIL ...»; lo que falla, en /tmp/correo-fuzz-imap.
"""
import os
import random
import subprocess
import sys
import tempfile

PROG, VUELTAS = sys.argv[1], int(sys.argv[2])
rnd = random.Random(int(sys.argv[3]) if len(sys.argv) > 3 else 20261122)
FALLOS = '/tmp/correo-fuzz-imap'
os.makedirs(FALLOS, exist_ok=True)


def literal(b):
    return b'{%d}\r\n' % len(b) + b


def base():
    cab = (b'From: Marta Gil <marta@ejemplo.es>\r\nSubject: =?UTF-8?Q?A=C3=B1o?=\r\nDate: Mon, 2 Mar 2026 10:00:00 +0100\r\n'
           b'Message-ID: <x@y>\r\nIn-Reply-To: <a@b>\r\nContent-Type: multipart/mixed; boundary=z\r\n\r\n')
    msg = cab + b'--z\r\nContent-Type: text/plain\r\n\r\nhola\r\n--z--\r\n'
    r = [b'* OK [CAPABILITY IMAP4rev1 IDLE UIDPLUS MOVE SPECIAL-USE LITERAL+ AUTH=PLAIN] listo\r\n',
         b'* CAPABILITY IMAP4rev1 IDLE UIDPLUS MOVE SPECIAL-USE LITERAL+ AUTH=PLAIN UNSELECT\r\n',
         b'c1 OK [CAPABILITY IMAP4rev1 IDLE] dentro\r\n',
         b'* LIST (\\HasNoChildren) "/" INBOX\r\n',
         b'* LIST (\\HasNoChildren \\Sent) "/" "Sent"\r\n',
         b'* LIST (\\HasChildren) "/" "Asociaci&APM-n"\r\n',
         b'* LIST (\\HasNoChildren) "/" "Asociaci&APM-n/Cuotas"\r\n',
         b'* LIST (\\Noselect) NIL ' + literal(b'Raro "con" comillas') + b'\r\n',
         b'* STATUS "INBOX" (MESSAGES 49 UNSEEN 34 UIDNEXT 50 UIDVALIDITY 1874738110)\r\n',
         b'* 49 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 1874738110] v\r\n* OK [UIDNEXT 50] n\r\n* OK [UNSEEN 3] u\r\n',
         b'* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n',
         b'* SEARCH 1 2 3 4 5 10 20 30 49\r\n',
         b'* 3 FETCH (UID 3 FLAGS (\\Seen \\Flagged) RFC822.SIZE 1234 INTERNALDATE "02-Mar-2026 10:00:00 +0100" '
         b'BODY[HEADER.FIELDS (FROM SUBJECT DATE MESSAGE-ID)] ' + literal(cab) + b')\r\n',
         b'* 4 FETCH (FLAGS (\\Seen) UID 4)\r\n',
         b'* 1 FETCH (UID 1 BODY[] ' + literal(msg) + b')\r\n',
         b'* 2 FETCH (UID 2 ENVELOPE ("Mon, 2 Mar 2026" "asunto" (("M" NIL "m" "e.es")) NIL NIL NIL NIL NIL NIL "<x@y>") '
         b'BODYSTRUCTURE ("text" "plain" ("charset" "utf-8") NIL NIL "7bit" 4 1 NIL NIL NIL) FLAGS ())\r\n',
         b'* 5 EXPUNGE\r\n',
         b'c9 OK [APPENDUID 1874738110 51] hecho\r\n',
         b'c10 OK [COPYUID 1874738110 3 7] movido\r\n',
         b'+ idling\r\n',
         b'* BYE adios\r\n']
    return b''.join(r)


BASE = base()


def estropea(d):
    d = bytearray(d)
    for _ in range(rnd.randint(1, 6)):
        k = rnd.randint(0, 13)
        if not d:
            d += b'*'
        i = rnd.randrange(len(d))
        if k == 0:
            d[i] = rnd.randrange(256)
        elif k == 1:
            del d[i:i + rnd.randint(1, 50)]
        elif k == 2:
            j = rnd.randrange(len(d))
            d[i:i] = d[j:j + rnd.randint(1, 200)]
        elif k == 3:
            d[i:i] = rnd.choice([b'{4294967295}\r\n', b'{99999999}\r\n', b'{5}\r\nab', b'{0}\r\n', b'{-1}\r\n', b'{12+}\r\n'])
        elif k == 4:
            d[i:i] = b'(' * rnd.choice([10, 1000, 50000])
        elif k == 5:
            d[i:i] = b'9' * rnd.randint(10, 40)
        elif k == 6:
            d[i:i] = b'A' * rnd.choice([1000, 100000, 1000000])
        elif k == 7:
            d[i:i] = b'"sin cerrar \\'
        elif k == 8:
            d[i:i] = b'\x00' * rnd.randint(1, 10)
        elif k == 9:
            d = d.replace(b'\r\n', b'\n') if rnd.random() < .5 else d.replace(b'\r\n', b'')
        elif k == 10:
            d[i:i] = b'* LIST (' + b'\\X ' * rnd.randint(1, 5000) + b') "/" a\r\n'
        elif k == 11:
            d[i:i] = b'* SEARCH ' + b' '.join(b'%d' % rnd.randint(0, 2 ** 33) for _ in range(rnd.randint(1, 3000))) + b'\r\n'
        elif k == 12:
            d[i:i] = b'* 1 FETCH (UID 1 BODY[] {%d}\r\n' % rnd.randint(0, 3000) + bytes(rnd.randrange(256) for _ in range(rnd.randint(0, 3000)))
        else:
            d = d[:i]
    return bytes(d)


ok = fallos = 0
with tempfile.TemporaryDirectory() as t:
    env = dict(os.environ, CORREO_DATOS=t, CORREO_CALLA='1', ASAN_OPTIONS='detect_leaks=1:abort_on_error=0',
               UBSAN_OPTIONS='halt_on_error=1')
    f = os.path.join(t, 'r.txt')
    for v in range(VUELTAS):
        d = BASE if v == 0 else estropea(BASE)
        open(f, 'wb').write(d)
        try:
            p = subprocess.run([PROG, '/ORDEN', 'imap-respuestas', f], env=env, capture_output=True, timeout=20)
            mal = p.returncode not in (0, 1) or b'Sanitizer' in p.stderr or b'runtime error' in p.stderr
            por = p.stderr[-400:].decode('utf-8', 'replace')
        except subprocess.TimeoutExpired:
            mal, por = True, 'tarda demasiado (¿cuadrático o un bucle?)'
        if mal:
            fallos += 1
            open(os.path.join(FALLOS, 'f%05d.txt' % v), 'wb').write(d)
            print('  FAIL fuzz IMAP vuelta %d: %s' % (v, por.strip().splitlines()[-1] if por.strip() else '?'))
        else:
            ok += 1
if not fallos:
    print('  OK   fuzz del analizador IMAP: %d respuestas de servidor estropeadas, sin un fallo (ASan y UBSan)' % ok)
sys.exit(1 if fallos else 0)
