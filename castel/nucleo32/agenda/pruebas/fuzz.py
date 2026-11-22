#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""agenda/pruebas/fuzz.py AGENDA.BIN CORRE32 [VUELTAS] [SEMILLA]

(AGENDA) Estropea .ICS (y .VCF) a propósito y se los da a AGENDA /IMPORTA,
y lo importado, a /EXPORTA y /LISTA, en corre32: octetos cambiados, cortes,
trozos repetidos, renglones de 20 K, BEGIN sin END (y END sin BEGIN), mil
EXDATE, COUNT e INTERVAL enormes, BYDAY=99MO, fechas imposibles, zonas que
no existen, UTF-8 roto, ceros. Lo que no puede pasar: que el programa
falle (corre32 dice «ha fallado»), que no acabe (20 s) o que lo exportado
no se pueda volver a importar con las mismas citas.
"""
import os
import random
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))

MALOS = [
    b'RRULE:FREQ=DAILY;COUNT=999999999\r\n',
    b'RRULE:FREQ=YEARLY;BYMONTH=2;BYMONTHDAY=30\r\n',
    b'RRULE:FREQ=MONTHLY;BYDAY=99MO,-99FR;INTERVAL=0\r\n',
    b'RRULE:FREQ=WEEKLY;INTERVAL=4294967295;BYSETPOS=0,400,-400\r\n',
    b'RRULE:FREQ=SECONDLY;BYHOUR=99\r\n',
    b'RRULE:FREQ=YEARLY;BYYEARDAY=366,-366;BYWEEKNO=53\r\n',
    b'RRULE:;;;==;FREQ\r\n',
    b'DTSTART:20261399T256199Z\r\n',
    b'DTSTART;TZID="Marte/Olympus":20261124T100000\r\n',
    b'DTSTART;VALUE=DATE:0000\r\n',
    b'DTSTART:19000101\r\n',
    b'DTEND:20991231T235959Z\r\n',
    b'DURATION:P99999999W\r\n',
    b'DURATION:-PT5M\r\n',
    b'TRIGGER:-P9999999D\r\n',
    b'TRIGGER;VALUE=DATE-TIME:20000101T000000Z\r\n',
    b'EXDATE:' + b','.join(b'2026%02d%02dT100000Z' % (m, d) for m in range(1, 13) for d in range(1, 29)) + b'\r\n',
    b'SUMMARY:' + b'\xc3\xb1' * 5000 + b'\r\n',
    b'SUMMARY:\xff\xfe\xc3\x28\xe2\x82\r\n',
    b'LOCATION:' + b'\\' * 3000 + b'\r\n',
    b'CATEGORIES:' + b',' * 2000 + b'\r\n',
    b'X-' + b'A' * 20000 + b':b\r\n',
    b'BEGIN:VEVENT\r\n' * 50,
    b'END:VEVENT\r\nEND:VCALENDAR\r\nEND:VALARM\r\n',
    b'BEGIN:VTIMEZONE\r\nTZID:Raro\r\nBEGIN:DAYLIGHT\r\nTZOFFSETTO:+9999\r\nRRULE:FREQ=YEARLY;BYMONTH=13;BYDAY=9SU\r\nEND:DAYLIGHT\r\nEND:VTIMEZONE\r\n',
    b'RECURRENCE-ID:20261124T100000\r\n',
    b'\x00\x00\x00\r\n',
    b' continuacion sin renglon\r\n',
    b'BDAY:--1340\r\nBEGIN:VCARD\r\nFN:Sin fin\r\nBDAY:99999999\r\nEND:VCARD\r\n',
]


def corpus():
    base = []
    for n in os.listdir(AQUI):
        if n.lower().endswith(('.ics', '.vcf')):
            base.append(open(os.path.join(AQUI, n), 'rb').read())
    return base


def estropea(r, d):
    d = bytearray(d)
    for _ in range(r.randint(1, 6)):
        k = r.random()
        if k < 0.25 and d:
            for _ in range(r.randint(1, 20)):
                d[r.randrange(len(d))] = r.randrange(256)
        elif k < 0.35 and d:
            del d[r.randrange(len(d)):]
        elif k < 0.5 and d:
            a = r.randrange(len(d))
            b = min(len(d), a + r.randint(1, 400))
            d[a:a] = d[a:b] * r.randint(1, 8)
        elif k < 0.85:
            p = r.randrange(len(d) + 1)
            d[p:p] = r.choice(MALOS)
        else:
            d = d.replace(b'\r\n', b'\n') if r.random() < 0.5 else d.replace(b'\r\n', b'\r\n ')
    return bytes(d)


def corre(corre32, prog, carpeta, *args):
    try:
        p = subprocess.run([corre32, '-C', carpeta, prog, '/CARPETA=\\AG', '/AHORA=22/11/2026-10:00'] + list(args),
                           capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return None, b'(no acaba en 20 s)'
    return p.returncode, p.stdout + p.stderr


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    prog, corre32 = os.path.abspath(sys.argv[1]), sys.argv[2]
    vueltas = int(sys.argv[3]) if len(sys.argv) > 3 else 200
    r = random.Random(int(sys.argv[4]) if len(sys.argv) > 4 else 22)
    base = corpus()
    mal = 0
    with tempfile.TemporaryDirectory() as tmp:
        for i in range(vueltas):
            d = estropea(r, r.choice(base))
            for n in os.listdir(tmp):
                p = os.path.join(tmp, n)
                if os.path.isdir(p):
                    for m in os.listdir(p):
                        os.remove(os.path.join(p, m))
                    os.rmdir(p)
                else:
                    os.remove(p)
            open(os.path.join(tmp, 'X.ICS'), 'wb').write(d)
            for paso in (('/IMPORTA', 'X.ICS'), ('/EXPORTA', 'Y.ICS'), ('/LISTA', '20/11/2026', '30/11/2026'),
                         ('/AVISOS', '20/11/2026', '00:00', '30/11/2026', '00:00')):
                rc, sal = corre(corre32, prog, tmp, *paso)
                if rc is None or rc == 255 or b'ha fallado' in sal or b'ha muerto' in sal:
                    mal += 1
                    nombre = os.path.join(tempfile.gettempdir(), 'agenda-fuzz-%d.ics' % i)
                    open(nombre, 'wb').write(d)
                    print('MAL en la vuelta %d (%s): %s; el archivo, en %s' % (i, paso[0], sal[-200:].decode('latin-1', 'replace').strip(), nombre))
                    break
            else:
                # lo exportado, otra vez: las mismas citas
                rc1, s1 = corre(corre32, prog, tmp, '/IMPORTA', 'Y.ICS')
                if rc1 is None or rc1 == 255 or b'cambiadas' not in s1:
                    mal += 1
                    print('MAL en la vuelta %d: lo exportado no se vuelve a importar: %s' % (i, s1[-200:]))
                elif b'importadas 0,' not in s1:
                    mal += 1
                    print('MAL en la vuelta %d: lo exportado, importado otra vez, duplica: %s' % (i, s1[-200:]))
    if mal:
        print('MAL %d de %d vueltas' % (mal, vueltas))
        sys.exit(1)
    print('OK %d vueltas (%d ejecuciones) sin un fallo' % (vueltas, vueltas * 5))


if __name__ == '__main__':
    main()
