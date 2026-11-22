#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# fuzz.py CUENTAS CARPETA VUELTAS: estropea a propósito los libros de prueba
# de formatos.sh (los de haz_ref.py y los del guion, en CARPETA) y los pasa
# por los lectores de Cuentas 2 («CUENTAS --convierte X Y», compilado con
# ASan y UBSan: lee, calcula y vuelve a escribir en .HOJ, .XLSX u .ODS).
# A los .XLSX y .ODS: octetos del ZIP, cortes, el XML de dentro estropeado
# (octetos, cortes, trozos repetidos, anidado hondo, números enormes en las
# repeticiones, las unidas, los índices de estilos y de textos compartidos,
# fórmulas compartidas sin maestra, rutas que se salen, partes que faltan) y
# «bombas» de descompresión. A los .HOJ y .CSV: octetos, líneas repetidas,
# campos cambiados, números enormes, líneas largas, comillas sin cerrar.
# Si algo falla (un error de ASan o UBSan, una señal o se cuelga más de 30
# segundos), deja el archivo en CARPETA/fallo-N.* y acaba con 1.
import os, random, subprocess, sys, zipfile, io, re

cuentas, carpeta, vueltas = sys.argv[1], sys.argv[2], int(sys.argv[3])
rnd = random.Random(int(os.environ.get('SEMILLA', '2026')))
orig = {}
for n in ('excel.xlsx', 'e1904.xlsx', 'libre.ods', 'hoj386.hoj', 'g.hoj', 'g.xlsx', 'g.ods', 'pc.csv', 'coma.csv', 'tab.csv'):
    p = os.path.join(carpeta, n)
    if os.path.exists(p):
        orig[n] = open(p, 'rb').read()
nombres = sorted(orig)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:exitcode=99', UBSAN_OPTIONS='print_stacktrace=1:exitcode=98')

def partes(d):
    z = zipfile.ZipFile(io.BytesIO(d))
    return [(i.filename, z.read(i.filename)) for i in z.infolist()]

def zipea(ps):
    b = io.BytesIO()
    with zipfile.ZipFile(b, 'w') as z:
        for n, d in ps:
            z.writestr(zipfile.ZipInfo(n) if n == 'mimetype' else n, d,
                       compress_type=zipfile.ZIP_STORED if n == 'mimetype' else zipfile.ZIP_DEFLATED)
    return b.getvalue()

def octetos(d, n):
    d = bytearray(d)
    for _ in range(n):
        if d:
            d[rnd.randrange(len(d))] = rnd.randrange(256)
    return bytes(d)

ENORMES = [b'999999999', b'4294967295', b'2147483648', b'-1', b'0', b'1e308', b'nan', b'65537', b'257', b'1048577']
def xml(x):
    x = bytearray(x)
    k = rnd.randrange(14)
    if k == 0 and x:
        return octetos(x, rnd.randint(1, 30))
    if k == 1 and x:
        return bytes(x[:rnd.randrange(len(x))])
    if k == 2 and x:
        a = rnd.randrange(len(x)); b = min(len(x), a + rnd.randint(1, 3000))
        return bytes(x[:b] + x[a:b] * rnd.randint(2, 30) + x[b:])
    if k == 3:
        i = x.find(b'>', len(x) // 3)
        return bytes(x[:i + 1] + b'<a>' * 20000 + x[i + 1:]) if i > 0 else bytes(x)
    if k in (4, 5, 6, 7):
        # los números de dentro de los atributos, enormes o raros
        ms = list(re.finditer(rb'="(-?[0-9][0-9.]*)"', bytes(x)))
        for m in rnd.sample(ms, min(len(ms), rnd.randint(1, 8))):
            s, e = m.span(1)
            x[s:e] = rnd.choice(ENORMES)
            break
        return bytes(x)
    if k == 8:
        return bytes(x).replace(b'<f>', b'<f t="shared" si="7">', 1).replace(b'si="0"', b'si="99999"')
    if k == 9:
        return bytes(x).replace(b'Target="', b'Target="../../../../', 1)
    if k == 10:
        return bytes(x).replace(b'of:=', b'of:=' + b'(' * rnd.randint(100, 5000), 1)
    if k == 11:
        return bytes(x).replace(b'<v>', b'<v>' + rnd.choice(ENORMES), 3)
    if k == 12:
        return bytes(x).replace(b'ref="', b'ref="' + rnd.choice([b'XFD1048576:', b'A0:', b'ZZZZ9', b'$$$', b'A1:B']), 2)
    return bytes(x).replace(b'&', b'&#xFFFFFFF;', 2).replace(b'<!', b'<!DOCTYPE x [<!ENTITY a "aaaa">]><!', 1)

def zip_malo(d):
    k = rnd.randrange(6)
    if k == 0:
        return octetos(d, rnd.randint(1, 40))
    if k == 1:
        return d[:rnd.randrange(len(d))]
    try:
        ps = partes(d)
    except Exception:
        return d
    if k == 2 and ps:
        del ps[rnd.randrange(len(ps))]
        return zipea(ps)
    if k == 3:
        # una bomba: un archivo de dentro muy grande de ceros
        ps.append(('xl/bomba.xml', b'\0' * (rnd.randint(1, 80) * 1024 * 1024)))
        return zipea(ps)
    ps = [(n, xml(c) if n.endswith(('.xml', '.rels')) and rnd.random() < 0.5 else c) for n, c in ps]
    return zipea(ps)

def texto_malo(d):
    k = rnd.randrange(8)
    if k == 0:
        return octetos(d, rnd.randint(1, 30))
    if k == 1:
        return d[:rnd.randrange(len(d) + 1)]
    ls = d.split(b'\n')
    if k == 2 and ls:
        i = rnd.randrange(len(ls)); return b'\n'.join(ls[:i] + [ls[i]] * rnd.randint(2, 500) + ls[i:])
    if k == 3:
        for _ in range(rnd.randint(1, 10)):
            i = rnd.randrange(len(ls))
            c = ls[i].split(b'\t' if b'\t' in ls[i] else b' ')
            if c:
                c[rnd.randrange(len(c))] = rnd.choice(ENORMES + [b'', b'ZZZ99999', b'A1:IV65536', b'=' * 3000, b'"', b'\\'])
                ls[i] = (b'\t' if b'\t' in ls[i] else b' ').join(c)
        return b'\n'.join(ls)
    if k == 4:
        i = rnd.randrange(len(ls)); ls[i] = ls[i] + b'x' * rnd.randint(1000, 200000); return b'\n'.join(ls)
    if k == 5:
        return d.replace(b'"', b'', 1) + b'"' + b'a;' * 50000
    if k == 6:
        return d + b'\r\nC A1 0 =' + b'SUMA(' * 3000 + b'\r\n2C\tB1\t1\tf\t' + b'(' * 4000 + b'\r\n'
    return b'\n'.join(rnd.sample(ls, len(ls)))

fallos = leidos = 0
for v in range(vueltas):
    n = nombres[v % len(nombres)]
    d = orig[n]
    malo = zip_malo(d) if n.endswith(('.xlsx', '.ods')) else texto_malo(d)
    ext = n.rsplit('.', 1)[1]
    ent = os.path.join(carpeta, 'fz.' + ext)
    open(ent, 'wb').write(malo)
    sal = os.path.join(carpeta, 'fz_sal.' + rnd.choice(['hoj', 'xlsx', 'ods', 'csv']))
    try:
        r = subprocess.run([cuentas, '--convierte', ent, sal], capture_output=True, timeout=30, env=env)
        mal = r.returncode in (98, 99) or r.returncode < 0 or b'Sanitizer' in r.stderr or b'runtime error' in r.stderr
        por = r.stderr.decode('utf-8', 'replace')[-600:]
        leidos += r.returncode == 0
    except subprocess.TimeoutExpired:
        mal, por = True, 'se cuelga (más de 30 s)'
    if mal:
        fallos += 1
        f = os.path.join(carpeta, 'fallo-%d.%s' % (fallos, ext))
        open(f, 'wb').write(malo)
        print('fallo %d (%s, vuelta %d): %s' % (fallos, n, v, por))
        if fallos >= 5:
            break
print('%d vueltas (%d se han podido leer y guardar; los demás, con su aviso), %d fallos' % (v + 1, leidos, fallos))
sys.exit(1 if fallos else 0)
