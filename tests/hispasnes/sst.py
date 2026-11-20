#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""Pasa las pruebas «de un solo paso» (SingleStepTests, MIT) a binario y las
corre en el núcleo de Hispa-Snes (snesnativo -c / -s).

uso: sst.py SNESNATIVO DIR_65816 DIR_SPC700 [MAX_POR_ARCHIVO]

Cada archivo .json trae casos de una instrucción: el estado de antes (los
registros y los octetos de memoria que toca), el de después y los ciclos.
tests/thirdparty/fetch_snes.sh baja solo el principio de cada archivo: aquí
se toman los casos enteros que haya. El binario, para snesnativo:
  65816: por caso, 10 dd (pc s p a x y dbr d pbr e), dd n, n x (dd dir, dd
         valor), lo mismo del final, y dd ciclos
  SPC700: 6 dd (pc a x y sp psw), dd n, n x (dd, dd), lo del final, dd ciclos
snesnativo dice en la salida normal los que no salen igual («MAL n ...») y
al final cuántos van bien. Las pruebas de MVN y MVP mueven muchos octetos de
una vez: se corren hasta acabar y sin mirar los ciclos.
"""
import json, os, struct, subprocess, sys, tempfile

R65 = ('pc', 's', 'p', 'a', 'x', 'y', 'dbr', 'd', 'pbr', 'e')
RSPC = ('pc', 'a', 'x', 'y', 'sp', 'psw')


def casos(ruta, maximo):
    out = []
    with open(ruta, encoding='utf-8') as f:
        for linea in f:
            linea = linea.strip().lstrip('[').rstrip(',').rstrip(']')
            if not linea.startswith('{'):
                continue
            try:
                out.append(json.loads(linea))
            except ValueError:
                break                   # (el último, cortado)
            if len(out) >= maximo:
                break
    if not out:                         # (todo en una línea: hasta donde se pueda)
        txt = open(ruta, encoding='utf-8').read()
        dec = json.JSONDecoder()
        i = txt.find('{')
        while i >= 0 and len(out) < maximo:
            try:
                o, j = dec.raw_decode(txt, i)
            except ValueError:
                break
            out.append(o)
            i = txt.find('{', j)
    return out


def estado(st, regs):
    b = b''.join(struct.pack('<I', st[r]) for r in regs)
    b += struct.pack('<I', len(st['ram']))
    for a, v in st['ram']:
        b += struct.pack('<II', a, v)
    return b


def binario(lista, regs):
    b = b''
    for c in lista:
        b += estado(c['initial'], regs) + estado(c['final'], regs) + struct.pack('<I', len(c['cycles']))
    return b


def corre(exe, modo, ruta, lista, regs, nombre):
    with tempfile.NamedTemporaryFile(delete=False) as t:
        t.write(binario(lista, regs))
    try:
        r = subprocess.run([exe, modo, t.name], stdout=subprocess.PIPE, timeout=120)
    finally:
        os.unlink(t.name)
    sal = r.stdout.decode('latin-1').splitlines()
    malos = [l for l in sal if l.startswith('MAL')]
    ciclos = [l for l in sal if l.startswith('CICLOS')]
    return malos, ciclos


def main():
    exe, d65, dspc = sys.argv[1:4]
    maximo = int(sys.argv[4]) if len(sys.argv) > 4 else 100
    total = mal = malc = 0
    resumen = []
    for d, modo, regs, pat in ((d65, '-c', R65, ('%02x.e.json', '%02x.n.json')),
                               (dspc, '-s', RSPC, ('%02x.json',))):
        if not os.path.isdir(d):
            continue
        for op in range(256):
            for p in pat:
                ruta = os.path.join(d, p % op)
                if not os.path.isfile(ruta):
                    continue
                lista = casos(ruta, maximo)
                if not lista:
                    continue
                m, c = corre(exe, modo, ruta, lista, regs, p % op)
                total += len(lista)
                mal += len(m)
                malc += len(c)
                if m or c:
                    resumen.append('%s %s: %d mal, %d con otros ciclos; %s' % (
                        modo, p % op, len(m), len(c), (m + c)[0]))
    for l in resumen:
        print(l)
    print('casos: %d, mal: %d, con otros ciclos: %d' % (total, mal, malc))
    sys.exit(1 if mal else 0)


main()
