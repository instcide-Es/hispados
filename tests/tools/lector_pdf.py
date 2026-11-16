#!/usr/bin/env python3
"""Prueba del núcleo de Lector contra los PDF de tests/tools/hazpdf.py.

uso: lector_pdf.py [-v] [CASO...] [--azar N] [--de-verdad CARPETA]

Ensambla el núcleo (tests/tools/lector_nucleo.py), hace cada PDF de hazpdf.py,
se lo da y compara lo que el núcleo dice con lo que hazpdf.py sabe que ha
metido: páginas, tamaño, avisos, cada trozo de texto en su sitio, con su
tamaño, su tipo y su ancho, las rayas, los rectángulos, las imágenes y el
texto en orden de lectura. Después, con --azar N (por defecto 400), estropea
N veces octetos al azar de esos PDF, los corta y los mezcla: lo que diga,
pero sin colgarse ni caerse. Con --de-verdad CARPETA compara el texto de los
PDF de esa carpeta (hechos por otros programas) con el que saca «pdftotext».
"""
import os, random, re, subprocess, sys, tempfile, unicodedata

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hazpdf
from lector_nucleo import Nucleo, monta

fallos = 0
total = 0


def res(ok, texto):
    global fallos, total
    total += 1
    if not ok:
        fallos += 1
        print('  FAIL ' + texto)
    elif VER:
        print('  OK   ' + texto)


def cerca(a, b, tol=0.6):
    return abs(a - b) <= tol


def comprueba(nombre, n, esperado, hallados):
    if n.colgado or n.codigo != 0:
        res(False, '%s: el núcleo %s' % (nombre, 'se ha colgado' if n.colgado else 'se ha caído (salida %d)' % n.codigo))
        return
    for e in esperado:
        c = e[0]
        pg = n.pags[e[1]] if c not in ('error', 'paginas', 'reparado', 'muchos', 'titulo', 'autor', 'halla') and e[1] < len(n.pags) else None
        d = '%s: %s' % (nombre, ' '.join(str(x) if not isinstance(x, float) else '%.1f' % x for x in e))
        if c == 'error':
            res(n.err == e[1], d + ' (da %d)' % n.err)
        elif c == 'paginas':
            res(n.npag == e[1], d + ' (tiene %d)' % n.npag)
        elif c == 'reparado':
            res(n.reparado == e[1], d + ' (es %d)' % n.reparado)
        elif c == 'muchos':
            res(n.muchos == e[1], d + ' (es %d)' % n.muchos)
        elif c == 'titulo':
            res(n.titulo == e[1], d + ' (es «%s»)' % n.titulo)
        elif c == 'autor':
            res(n.autor == e[1], d + ' (es «%s»)' % n.autor)
        elif c == 'halla':
            v = hallados.get(e[1])
            cuantas = len(v.pags[e[2]].halla) if v and e[2] < len(v.pags) else -1
            res(cuantas == e[3], d + ' (lo halla %d veces)' % cuantas)
        elif pg is None:
            res(False, d + ' (no hay esa página)')
        elif c == 'tam':
            res(cerca(pg.an, e[2], 0.1) and cerca(pg.al, e[3], 0.1), d + ' (es %.2f x %.2f)' % (pg.an, pg.al))
        elif c == 'aviso':
            res(pg.avisos & e[2] == e[2], d + ' (lleva %d)' % pg.avisos)
        elif c == 'sin_aviso':
            res(pg.avisos & e[2] == 0, d + ' (lleva %d)' % pg.avisos)
        elif c == 'texto':
            t, x, y, tam = e[2:6]
            cand = [k for k in pg.textos() if k[8] == t]
            bien = [k for k in cand if cerca(k[1], x) and cerca(k[2], y) and cerca(k[4], tam, 0.1)
                    and (len(e) < 7 or e[6] is None or k[6] == e[6]) and (len(e) < 8 or cerca(k[3], e[7], 0.5))]
            mas = ''
            if not bien:
                mas = ' (hay: %s)' % ('; '.join('x=%.2f y=%.2f tam=%.2f letra=%d ancho=%.2f' % (k[1], k[2], k[4], k[6], k[3]) for k in cand) or
                                      'ninguno con ese texto; los que hay: ' + ' | '.join(k[8] for k in pg.textos())[:300])
            res(bool(bien), d + mas)
        elif c == 'contiene':
            res(any(e[2] in k[8] for k in pg.textos()), d)
        elif c == 'no_texto':
            res(not any(e[2] in k[8] for k in pg.textos()), d)
        elif c == 'lineas':
            l = pg.texto.split('\r\n')
            if l and l[-1] == '':
                l.pop()
            res(l == e[2], d[:60] + '...' + ('' if l == e[2] else '\n       sale:    %r\n       esperado: %r' % (l, e[2])))
        elif c == 'raya':
            x0, y0, x1, y1 = e[2:6]
            bien = [k for k in pg.de('L') if cerca(k[1], x0) and cerca(k[2], y0) and cerca(k[3], x1) and cerca(k[4], y1)
                    and (len(e) < 7 or e[6] is None or cerca(k[8], e[6], 0.05)) and (len(e) < 8 or k[5] == e[7])]
            res(bool(bien), d + ('' if bien else ' (hay: %s)' % '; '.join('%.1f,%.1f-%.1f,%.1f g=%.2f c=%d' % (k[1], k[2], k[3], k[4], k[8], k[5]) for k in pg.de('L'))[:400]))
        elif c in ('rect', 'imagen'):
            x, y, an, al = e[2:6]
            cl = 'R' if c == 'rect' else 'I'
            bien = [k for k in pg.de(cl) if cerca(k[1], x) and cerca(k[2], y) and cerca(k[3], an) and cerca(k[4], al)
                    and (len(e) < 7 or k[5] == e[6])]
            res(bool(bien), d + ('' if bien else ' (hay: %s)' % '; '.join('%.1f,%.1f %.1fx%.1f c=%d' % k[1:6] for k in pg.de(cl))[:400]))
        elif c == 'ilegible':
            res(any(cerca(k[1], e[2]) and cerca(k[2], e[3]) for k in pg.de('X')), d)
        elif c == 'figura':
            res(any(len(k[8][0]) == e[2] for k in pg.de('G')), d + ' (hay: %s)' % [[len(t) for t in k[8]] for k in pg.de('G')])
        elif c == 'ncosas':
            res(len(pg.de(e[2])) == e[3], d + ' (hay %d)' % len(pg.de(e[2])))
        else:
            raise SystemExit('comprobación desconocida: ' + c)


def llano(t):
    """Un texto, como lo dejaría Lector: en la 437 y sin lo que no existe."""
    t = t.replace('’', "'").replace('‘', "'").replace('“', '"').replace('”', '"')
    t = t.replace('ﬁ', 'fi').replace('ﬂ', 'fl').replace('–', '-').replace('—', '-').replace('€', 'EUR').replace('…', '...')
    s = ''
    for ch in t:
        try:
            ch.encode('cp437')
            s += ch
        except UnicodeEncodeError:
            d = unicodedata.normalize('NFKD', ch)
            s += d[0] if d[0].isascii() else '?'
    return s


def palabras(t):
    return [p for p in re.split(r'\s+', t) if p]


def main():
    global VER
    arg = sys.argv[1:]
    VER = '-v' in arg
    azar = 400
    de_verdad = None
    solo = []
    i = 0
    while i < len(arg):
        if arg[i] == '--azar':
            azar = int(arg[i + 1]); i += 1
        elif arg[i] == '--de-verdad':
            de_verdad = arg[i + 1]; i += 1
        elif arg[i] != '-v':
            solo.append(arg[i])
        i += 1
    with tempfile.TemporaryDirectory() as tmp:
        prog = monta(tmp)
        casos = hazpdf.casos()
        for nombre, datos, esperado in casos:
            if solo and nombre not in solo:
                continue
            ruta = os.path.join(tmp, nombre + '.PDF')
            open(ruta, 'wb').write(datos)
            n = Nucleo(prog, ruta)
            hallados = {b: Nucleo(prog, ruta, b) for b in {e[1] for e in esperado if e[0] == 'halla'}}
            antes = fallos
            comprueba(nombre, n, esperado, hallados)
            if fallos == antes and not VER:
                print('  OK   %s: %d comprobaciones' % (nombre, len(esperado)))
        if not solo and azar:
            # ---- estropeados: que no se cuelgue ni se caiga ----
            rnd = random.Random(1611)
            buenos = [d for _, d, _ in casos if 300 < len(d) < 60000 and d.lstrip().startswith(b'%PDF')]
            malos = 0
            ruta = os.path.join(tmp, 'AZAR.PDF')
            for k in range(azar):
                d = bytearray(rnd.choice(buenos))
                clase = rnd.randrange(6)
                if clase == 0:                              # octetos cambiados
                    for _ in range(rnd.randint(1, 20)):
                        d[rnd.randrange(len(d))] = rnd.randrange(256)
                elif clase == 1:                            # cortado
                    d = d[:rnd.randrange(len(d))]
                elif clase == 2:                            # un trozo quitado
                    a = rnd.randrange(len(d)); d = d[:a] + d[a + rnd.randint(1, 200):]
                elif clase == 3:                            # un trozo repetido o cambiado de sitio
                    a = rnd.randrange(len(d)); b = rnd.randrange(len(d)); t = d[a:a + rnd.randint(1, 300)]
                    d = d[:b] + t + d[b:]
                elif clase == 4:                            # números cambiados (largos, direcciones, referencias)
                    t = bytes(d)
                    cifras = [m.start() for m in re.finditer(rb'\d', t)]
                    for _ in range(rnd.randint(1, 12)):
                        d[rnd.choice(cifras)] = rnd.choice(b'0123456789')
                else:                                       # palabras clave cambiadas
                    t = bytes(d)
                    barras = [m.start() for m in re.finditer(rb'[/<>\[\]()]', t)]
                    for _ in range(rnd.randint(1, 8)):
                        d[rnd.choice(barras)] = rnd.choice(b'/<>[]() R')
                open(ruta, 'wb').write(bytes(d))
                n = Nucleo(prog, ruta, 'a', tiempo=20)
                if n.colgado or n.codigo != 0:
                    malos += 1
                    if malos <= 5:
                        guarda = os.path.join(os.environ.get('LECTOR_MALOS', '/tmp'), 'lector_malo_%d.pdf' % k)
                        open(guarda, 'wb').write(bytes(d))
                        print('  FAIL estropeado %d (clase %d): %s; guardado en %s' % (
                            k, clase, 'se cuelga' if n.colgado else 'se cae (salida %d)' % n.codigo, guarda))
            res(malos == 0, '%d PDF estropeados al azar: %d cuelgues o caídas' % (azar, malos))
            if malos == 0 and not VER:
                print('  OK   %d PDF estropeados al azar: ni un cuelgue ni una caída' % azar)
        if de_verdad:
            for f in sorted(os.listdir(de_verdad)):
                if not f.lower().endswith('.pdf'):
                    continue
                ruta = os.path.join(de_verdad, f)
                n = Nucleo(prog, ruta)
                if n.colgado or n.codigo != 0 or n.err != 0:
                    res(False, '%s: no se abre (error %d, salida %d)' % (f, n.err, n.codigo))
                    continue
                ref = subprocess.run(['pdftotext', '-enc', 'UTF-8', ruta, '-'], stdout=subprocess.PIPE).stdout.decode('utf-8', 'replace')
                pags = ref.split('\f')
                mio = palabras(' '.join(pg.texto for pg in n.pags))
                suyo = palabras(llano(ref))
                faltan = [p for p in suyo if p not in set(mio)]
                sobran = [p for p in mio if p not in set(suyo)]
                bien = len(faltan) <= len(suyo) * 0.02 and len(sobran) <= len(mio) * 0.02
                paginas = len([x for x in pags if x.strip()]) if len(pags) > 1 else 1
                res(bien and n.npag >= 1, '%s (de verdad): %d páginas, %d palabras; faltan %d y sobran %d respecto a pdftotext%s' % (
                    f, n.npag, len(suyo), len(faltan), len(sobran),
                    '' if bien else '\n       faltan: %s\n       sobran: %s' % (faltan[:30], sobran[:30])))
                if bien and not VER:
                    print('  OK   %s (de verdad): %d páginas, %d palabras como las de pdftotext (faltan %d, sobran %d); avisos %s' % (
                        f, n.npag, len(suyo), len(faltan), len(sobran), sorted({pg.avisos for pg in n.pags})))
    print('  %s núcleo de Lector: %d comprobaciones, %d fallos' % ('OK  ' if not fallos else 'FAIL', total, fallos))
    sys.exit(1 if fallos else 0)


if __name__ == '__main__':
    main()
