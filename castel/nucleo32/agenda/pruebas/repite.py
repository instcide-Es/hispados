#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""agenda/pruebas/repite.py AGENDA.BIN CORRE32 [N] [SEMILLA]

(AGENDA) Las repeticiones de la Agenda frente a python-dateutil (rrule): N
reglas al azar (y las de siempre de casos_fijos), cada una con su DTSTART
puesto en su primera vez (así RFC 5545 y dateutil dicen lo mismo), y, para
las que no tienen COUNT, también desde un día de más adelante (el salto sin
contar). Sin dateutil, solo los casos escritos a mano (casos.txt).
Dice «OK n casos» o lo que no casa, y acaba con 0 o 1.
"""
import os
import random
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta

AQUI = os.path.dirname(os.path.abspath(__file__))
DIAS = ['MO', 'TU', 'WE', 'TH', 'FR', 'SA', 'SU']

FIJOS = [
    'FREQ=DAILY;COUNT=10',
    'FREQ=DAILY;INTERVAL=3;COUNT=12',
    'FREQ=WEEKLY;BYDAY=MO,WE,FR;COUNT=15',
    'FREQ=WEEKLY;INTERVAL=2;BYDAY=TU,TH;COUNT=12',
    'FREQ=WEEKLY;INTERVAL=2;WKST=SU;BYDAY=TU,SU;COUNT=8',
    'FREQ=MONTHLY;BYMONTHDAY=31;COUNT=8',
    'FREQ=MONTHLY;BYMONTHDAY=-1;COUNT=13',
    'FREQ=MONTHLY;BYDAY=2TU;COUNT=12',
    'FREQ=MONTHLY;BYDAY=-1FR;COUNT=12',
    'FREQ=MONTHLY;BYDAY=MO,TU,WE,TH,FR;BYSETPOS=-1;COUNT=12',
    'FREQ=MONTHLY;BYDAY=FR;BYMONTHDAY=13;COUNT=5',
    'FREQ=YEARLY;COUNT=6',
    'FREQ=YEARLY;BYMONTH=2;BYMONTHDAY=29;COUNT=4',
    'FREQ=YEARLY;BYDAY=20MO;COUNT=5',
    'FREQ=YEARLY;BYMONTH=3;BYDAY=-1SU;COUNT=6',
    'FREQ=YEARLY;BYMONTH=10;BYDAY=-1SU;COUNT=6',
    'FREQ=YEARLY;BYYEARDAY=1,100,-1;COUNT=9',
    'FREQ=YEARLY;INTERVAL=4;BYMONTH=11;BYDAY=TU;BYMONTHDAY=2,3,4,5,6,7,8;COUNT=4',
    'FREQ=DAILY;BYMONTH=1;BYDAY=SA,SU;COUNT=20',
    'FREQ=WEEKLY;UNTIL=20270115T000000;BYDAY=MO',
    'FREQ=MONTHLY;INTERVAL=2;BYMONTHDAY=1,15;UNTIL=20271231',
]


def regla_al_azar(r):
    f = r.choice(['DAILY', 'WEEKLY', 'MONTHLY', 'YEARLY'])
    p = ['FREQ=' + f]
    if r.random() < 0.4:
        p.append('INTERVAL=%d' % r.randint(2, 5))
    if f == 'WEEKLY' and r.random() < 0.7:
        p.append('BYDAY=' + ','.join(sorted(r.sample(DIAS, r.randint(1, 4)), key=DIAS.index)))
    if f == 'WEEKLY' and r.random() < 0.2:
        p.append('WKST=' + r.choice(DIAS))
    if f == 'MONTHLY':
        k = r.random()
        if k < 0.35:
            p.append('BYMONTHDAY=' + ','.join(str(x) for x in r.sample([1, 2, 10, 15, 28, 29, 30, 31, -1, -2], r.randint(1, 3))))
        elif k < 0.75:
            p.append('BYDAY=%d%s' % (r.choice([1, 2, 3, 4, -1, -2]), r.choice(DIAS)))
        elif k < 0.9:
            p.append('BYDAY=' + ','.join(r.sample(DIAS[:5], r.randint(2, 5))))
            p.append('BYSETPOS=' + r.choice(['1', '-1', '2', '1,-1']))
    if f == 'YEARLY':
        k = r.random()
        if k < 0.3:
            p.append('BYMONTH=' + ','.join(str(x) for x in sorted(r.sample(range(1, 13), r.randint(1, 3)))))
        elif k < 0.55:
            p.append('BYMONTH=%d' % r.randint(1, 12))
            p.append('BYDAY=%d%s' % (r.choice([1, 2, 3, -1]), r.choice(DIAS)))
        elif k < 0.7:
            p.append('BYYEARDAY=' + ','.join(str(x) for x in r.sample([1, 32, 60, 100, 200, 365, -1, -10], 2)))
        elif k < 0.8:
            p.append('BYDAY=%d%s' % (r.choice([1, 10, 20, 52, -1, -5]), r.choice(DIAS)))
    if f == 'DAILY' and r.random() < 0.3:
        p.append('BYDAY=' + ','.join(r.sample(DIAS, 3)))
    if f == 'DAILY' and r.random() < 0.2:
        p.append('BYMONTH=' + str(r.randint(1, 12)))
    if r.random() < 0.6:
        p.append('COUNT=%d' % r.randint(1, 25))
    elif r.random() < 0.5:
        p.append('UNTIL=%04d%02d%02d' % (r.randint(2027, 2034), r.randint(1, 12), r.randint(1, 28)))
    return ';'.join(p)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    prog, corre = sys.argv[1], sys.argv[2]
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 300
    r = random.Random(int(sys.argv[4]) if len(sys.argv) > 4 else 2026)
    try:
        from dateutil.rrule import rrulestr
    except ImportError:
        rrulestr = None
    if os.environ.get('SIN_DATEUTIL'):
        rrulestr = None
    casos = []          # (dtstart, regla, n, desde, lo esperado)
    if rrulestr is None:
        print('(sin python-dateutil: solo casos.txt)')
        esperado, renglones = lee_fijos()
        casos = renglones
    else:
        reglas = FIJOS + [regla_al_azar(r) for _ in range(n)]
        for k, regla in enumerate(reglas):
            d0 = datetime(r.randint(2024, 2030), r.randint(1, 12), r.randint(1, 28), r.randint(0, 23), r.choice([0, 15, 30, 45]))
            try:
                primera = list(rrulestr(regla.replace('UNTIL=', 'UNTIL=') if 'UNTIL' not in regla or 'T' in regla.split('UNTIL=')[1][:9] else regla, dtstart=d0, cache=False, forceset=False)[:1])
            except Exception as e:
                print('regla mala (dateutil):', regla, e)
                continue
            if not primera:
                continue
            d0 = primera[0]
            reg = regla
            if 'UNTIL=' in reg and 'T' not in reg.split('UNTIL=')[1].split(';')[0]:
                # (UNTIL de fecha: hasta el final de ese día)
                u = reg.split('UNTIL=')[1].split(';')[0]
                reg_du = reg.replace('UNTIL=' + u, 'UNTIL=' + u + 'T235959')
            else:
                reg_du = reg
            # (la Agenda llega hasta el año 2200)
            veces = [v for v in rrulestr(reg_du, dtstart=d0)[:40] if v.year <= 2200]
            casos.append((d0, reg, 40, None, veces))
            if 'COUNT' not in reg and len(veces) > 0:
                # desde más adelante (el salto sin contar)
                desde = d0 + timedelta(days=r.randint(500, 3000))
                todas = [v for v in rrulestr(reg_du, dtstart=d0).between(datetime(desde.year, desde.month, desde.day), datetime(2199, 1, 1), inc=True)[:15] if v.year <= 2200]
                casos.append((d0, reg, 15, desde, todas))
    with tempfile.TemporaryDirectory() as tmp:
        ruta = os.path.join(tmp, 'CASOS.TXT')
        with open(ruta, 'w') as f:
            for d0, reg, k, desde, _ in casos:
                f.write('%s|%s|%d|%s\n' % (d0.strftime('%d/%m/%Y %H:%M'), reg, k, desde.strftime('%d/%m/%Y') if desde else ''))
        sal = subprocess.run([corre, '-C', tmp, prog, '/CASOS', 'CASOS.TXT'], capture_output=True, timeout=600).stdout.decode('latin-1')
    bloques = []
    act = None
    for l in sal.splitlines():
        if l.startswith('agenda: caso '):
            act = []
        elif l.startswith('agenda: vez ') and act is not None:
            act.append(datetime.strptime(l[12:], '%d/%m/%Y %H:%M'))
        elif l.startswith('agenda: fin') and act is not None:
            bloques.append(act)
            act = None
    mal = 0
    for i, (d0, reg, k, desde, esp) in enumerate(casos):
        dio = bloques[i] if i < len(bloques) else None
        if dio != esp[:k]:
            mal += 1
            if mal <= 8:
                print('NO CASA: %s DTSTART=%s desde=%s' % (reg, d0, desde))
                print('   dateutil: %s' % [v.strftime('%Y-%m-%d') for v in esp[:k]][:12])
                print('   agenda:   %s' % ([v.strftime('%Y-%m-%d') for v in dio][:12] if dio is not None else None))
    if mal:
        print('MAL %d de %d casos' % (mal, len(casos)))
        sys.exit(1)
    if rrulestr is None:
        print('OK %d casos (los escritos a mano, casos.txt)' % len(casos))
    else:
        print('OK %d casos (frente a python-dateutil %s)' % (len(casos), __import__('dateutil').__version__))


def lee_fijos():
    # casos.txt: «dd/mm/aaaa hh:mm|RRULE|n|desde|aaaa-mm-dd,aaaa-mm-dd,...» (escritos a mano)
    casos = []
    for l in open(os.path.join(AQUI, 'casos.txt'), encoding='utf-8'):
        l = l.strip()
        if not l or l.startswith('#'):
            continue
        d0, reg, k, desde, esp = l.split('|')
        d0 = datetime.strptime(d0, '%d/%m/%Y %H:%M')
        casos.append((d0, reg, int(k), datetime.strptime(desde, '%d/%m/%Y') if desde else None,
                      [datetime.strptime(x, '%Y-%m-%d').replace(hour=d0.hour, minute=d0.minute) for x in esp.split(',')]))
    return None, casos


if __name__ == '__main__':
    main()
