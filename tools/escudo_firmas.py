#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  Hispa-Pentium - tools/escudo_firmas.py: los paquetes de las firmas de Escudo
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  Escudo (castel/nucleo32/escudo) mira los archivos con las firmas de ClamAV
#  (de Cisco Talos). Los equipos con Hispa-Pentium NO las bajan del CDN de
#  ClamAV: su FAQ («Freshclam», docs.clamav.net) solo admite freshclam y
#  cvdupdate (las descargas con guiones se deniegan: 403/429), y muchos
#  equipos bajando cada uno 110 MB es lo que pide no hacer. Se hace como pide
#  ClamAV para muchos equipos: UN espejo propio en el VPS, puesto al día con
#  cvdupdate (/opt/cvdupdate, la base en /var/lib/escudo-espejo; una vez al
#  día), y de ahí este guion hace tres paquetes de Hispa-Pentium, que van con
#  el catálogo firmado (Ed25519) de http://actualiza.hispados.es/pentium/:
#
#    escbase  (BASE.ESC)  lo elegido de main.cvd: unos 20 MB; solo cambia
#                         cuando cambia main.cvd (la última, en diciembre de 2025)
#    escmes   (MES.ESC)   lo elegido de daily.cvd el día que se hace: unos
#                         19 MB; se rehace cada mes (o antes si el del día
#                         pasa de --max-dia)
#    escdia   (DIA.ESC)   lo nuevo de daily.cvd desde el del mes, y lo que
#                         ClamAV ha retirado desde entonces: de KB a pocos MB,
#                         cada día. Necesita «escmes = esa misma versión»
#  (Medido el 21/11/2026: main.cvd 63 y daily.cvd 28190; en el equipo, con
#  los tres, unos 12 MB de memoria.)
#
#  QUÉ SE ELIGE (las bases enteras son 3,2 millones de sumas y 400.000 firmas
#  del cuerpo: medio GB en memoria con el motor de antes):
#  - Las firmas del cuerpo y las lógicas de lo que llega en memorias USB y
#    descargas a un equipo de la asociación: Windows (Win.*), los documentos
#    (Doc, Xls, Ppt, Rtf, Pdf, Onenote, Hwp), los guiones (Js, Vbs, Ps1, Py,
#    Html, Php, Asp, Xml, Txt), Java, Lnk, Archive, Multios, Dos, Legacy y Emf.
#    Fuera: Andr, Unix, Osx/Macos, Ios, Symbos, Email, Img, Swf, Phishing...
#    (otras plataformas, o lo que no llega por una memoria). Y fuera también
#    lo que Escudo no sabe mirar (objetivos 2, 3, 4, 5, 6, 7, 9, 11; PCRE...).
#  - De cada subfirma hexadecimal sin comodines (y sin sitio fijo) de una
#    lógica, los 32 octetos seguidos que más dicen (no los primeros: muchas
#    empiezan con relleno). Las lógicas piden varias a la vez (casi todas,
#    cinco), así que no casan por casualidad; las del cuerpo (.ndb) van
#    enteras: una sola cadena recortada sí puede estar en un programa limpio
#    (probado: 32 octetos de una de Win.Trojan.Startpage están en gcc).
#  - Las sumas no gastan memoria en el equipo (van en el disco, por tamaño),
#    pero sí disco y descarga: van todas las de las plataformas elegidas
#    hasta --max-sumas (de 8 octetos cada una): primero las limpias, las del
#    día, las de archivo entero de main; luego las de sección PE de main de
#    las familias que siguen vivas en daily; y luego las demás.
#  En el equipo: en memoria, por cada firma del disco, solo sus
#  «disparadores» (12 octetos: 4 fijos y 2 de detrás); la firma se lee del
#  disco (bloques comprimidos de 64 K) solo si aparece uno (motor.c).
#
#  Uso:
#    escudo_firmas.py --cvd CARPETA --sal CARPETA [--estado CARPETA]
#                     [--nueva-base] [--max-sumas N] [--max-dia MB]
#                     [--publica CARPETA --clave PRIVADA] [--actualiza]
#      --cvd        el espejo de cvdupdate (main.cvd, daily.cvd)
#      --sal        dónde dejar ESCBASE.HPQ, ESCDIA.HPQ y CATALOGO.LIN
#      --estado     dónde guardar lo que lleva la base (BASE.LST.gz), para
#                   hacer el del día (de fábrica, --sal/estado)
#      --nueva-base rehace la base aunque no toque
#      --max-sumas  cuántas sumas en la base (de fábrica 1.500.000: 12 MB)
#      --max-dia    si el del día pasa de tantos MB, se rehace la base (4)
#      --publica    copia los dos paquetes a la carpeta del servidor y firma
#                   otra vez su catálogo con --clave (tools/hpq.py)
#      --actualiza  antes, «cvd update» (cvdupdate) si no se ha hecho en 20 h
#      --corpus     una carpeta de archivos limpios (de fábrica, los programas de
#                   Windows de Open Watcom: /opt/watcom/binnt y binnt64): los
#                   disparadores se eligen entre lo que no sale en ellos
#  Las licencias: las bases de ClamAV van con la GPL-2 (su COPYING, que va
#  dentro de cada .cvd y se copia en el paquete como COPYING.TXT); esto es un
#  cambio de formato y una selección de esas mismas firmas, y se reparte con
#  esa licencia y diciendo de dónde salen (ORIGEN.TXT).
# =============================================================================
import argparse
import gzip
import hashlib
import io
import os
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.dirname(AQUI)
HPQ = os.path.join(AQUI, 'hpq.py')
ESCUDO = os.path.join(RAIZ, 'castel', 'nucleo32', 'escudo')

PLATAFORMAS = {'Win', 'Doc', 'Xls', 'Ppt', 'Rtf', 'Pdf', 'Onenote', 'Hwp', 'Js', 'Vbs', 'Ps1', 'Py', 'Html', 'Php',
               'Asp', 'Xml', 'Txt', 'Java', 'Lnk', 'Archive', 'Multios', 'Dos', 'Legacy', 'Emf', 'Ole2', 'Heuristics'}
OBJETIVOS = {'0', '1', '10', '12'}      # cualquiera, PE, PDF y Java (motor.c: objetivo_vale)
CORTE = 32                              # octetos que se dejan de una subfirma hexadecimal sin comodines
DIAS_BASE = 30
# archivos limpios de Windows (los del compilador Open Watcom, en el VPS): los
# disparadores evitan lo que sale en ellos (si no, saltan en cualquier programa)
CORPUS = ['/opt/watcom/binnt', '/opt/watcom/binnt64']
SUMAS = ('hdb', 'hsb', 'mdb', 'msb')
HEX = re.compile(r'[0-9a-f]+')
SUB_SIMPLE = re.compile(r'(?:([^:]*):)?([0-9a-f]+)((?:::[wiaf]+)?)')


def falla(t):
    sys.exit('escudo_firmas: ' + t)


def cabecera(ruta):
    with open(ruta, 'rb') as f:
        c = f.read(512).decode('latin-1').rstrip(' \0\r\n').split(':')
    if len(c) < 9 or c[0] != 'ClamAV-VDB':
        falla('%s no es un .cvd de ClamAV' % ruta)
    return {'version': int(c[2]), 'firmas': int(c[3]), 'nivel': int(c[4]), 'segundos': int(c[8]), 'fecha': c[1]}


def busca(carpeta, base):
    for x in ('.cvd', '.cld'):
        r = os.path.join(carpeta, base + x)
        if os.path.isfile(r):
            return r
    return None


def saca(ruta):
    """Los archivos del .tar.gz de un .cvd (ya comprobado): {nombre: octetos}."""
    with open(ruta, 'rb') as f:
        f.seek(512)
        datos = f.read()
    modo = 'r:gz' if datos[:2] == b'\x1f\x8b' else 'r:'
    r = {}
    with tarfile.open(fileobj=io.BytesIO(datos), mode=modo) as t:
        for m in t.getmembers():
            if m.isfile() and '/' not in m.name and not m.name.startswith('.'):
                r[m.name] = t.extractfile(m).read()
    return r


def lineas(datos):
    for l in datos.decode('latin-1').split('\n'):
        l = l.rstrip('\r')
        if l and not l.startswith('#'):
            yield l


def plataforma(nombre):
    return nombre.split('.', 1)[0]


def ventana(h, n):
    """De una cadena hexadecimal larga, los «n» octetos seguidos que más dicen:
    los que más cambian de un octeto al siguiente (los ceros, los FF y lo que
    se repite, «-----», valen poco)."""
    b = bytes.fromhex(h)
    if len(b) <= n:
        return h
    peso = [0] + [0 if b[i] == b[i - 1] or b[i] in (0, 0xFF) else 1 if b[i] in (0x20, 0x90, 0xCC, 0x2D, 0x3D) else 3
                  for i in range(1, len(b))]
    mejor, donde, s = -1, 0, sum(peso[1:n])
    for i in range(len(b) - n + 1):
        if i:
            s += peso[i + n - 1] - peso[i]
        if s > mejor:
            mejor, donde = s, i
    return b[donde:donde + n].hex()


def corta(sub):
    m = SUB_SIMPLE.fullmatch(sub)
    if not m or m.group(1) is not None:      # (con su sitio delante: entera)
        return sub
    _, h, mod = m.groups()
    return ventana(h, CORTE) + mod


def elige_cuerpo(clase, l):
    """La línea (.ndb o .ldb) como va en Escudo, o None si no va."""
    if clase == 'ndb':
        p = l.split(':')
        if len(p) < 4 or plataforma(p[0]) not in PLATAFORMAS or p[1] not in OBJETIVOS:
            return None
        return l                            # (las del cuerpo, enteras: una sola cadena recortada da falsos avisos)
    p = l.split(';')
    if len(p) < 4 or plataforma(p[0]) not in PLATAFORMAS:
        return None
    m = re.search(r'Target:(\d+)', p[1])
    if (m.group(1) if m else '0') not in OBJETIVOS:
        return None
    if any('/' in s for s in p[3:]):       # PCRE: no
        return None
    p[1] = p[1].replace('Engine:51-255,', '').replace('Engine:81-255,', '')
    return ';'.join(p[:3] + [corta(s) for s in p[3:]])


def nombre_suma(clase, l):
    p = l.split(':')
    return p[2] if len(p) > 2 else ''


def familia(n):
    return re.sub(r'(-\d+){1,2}$', '', n)


def lee_todo(m, d):
    """Lo elegido de main (m) y daily (d): {clase: [(línea, de_main)]}."""
    r = {k: [] for k in ('hdb', 'hsb', 'mdb', 'msb', 'fp', 'sfp', 'ndb', 'ldb', 'ign2')}
    for de_main, arch in ((True, m), (False, d)):
        for nombre, datos in arch.items():
            if '.' not in nombre:
                continue
            clase = nombre.rsplit('.', 1)[1]
            if clase not in r:
                continue
            for l in lineas(datos):
                if clase in ('ndb', 'ldb'):
                    l = elige_cuerpo(clase, l)
                    if l is None:
                        continue
                elif clase in SUMAS and plataforma(nombre_suma(clase, l)) not in PLATAFORMAS:
                    continue
                r[clase].append((l, de_main))
    return r


def huella(clase, l):
    return hashlib.sha1((clase + '|' + l).encode('latin-1')).hexdigest()[:16]


def elige_sumas_main(todo, maximo):
    """Las sumas de main que van en la base, por orden de valor, hasta «maximo»:
    las de archivo entero; luego las de sección PE de las familias que siguen
    vivas en daily; luego las demás."""
    vivas = set()
    for clase in ('ndb', 'ldb'):
        for l, de_main in todo[clase]:
            if not de_main:
                vivas.add(familia(re.split('[:;]', l, 1)[0]))
    for clase in SUMAS:
        for l, de_main in todo[clase]:
            if not de_main:
                vivas.add(familia(nombre_suma(clase, l)))
    grupos = [[], [], []]
    for clase in SUMAS:
        for l, de_main in todo[clase]:
            if not de_main:
                continue
            if clase in ('hdb', 'hsb'):
                grupos[0].append((clase, l))
            elif familia(nombre_suma(clase, l)) in vivas:
                grupos[1].append((clase, l))
            else:
                grupos[2].append((clase, l))
    r = []
    for g in grupos:
        r += g[:max(0, maximo - len(r))]
    return r, sum(len(g) for g in grupos)


def escribe_bases(carpeta, por_clase):
    os.makedirs(carpeta, exist_ok=True)
    rutas = []
    for clase, ls in por_clase.items():
        if not ls:
            continue
        r = os.path.join(carpeta, 'ESCUDO.' + clase)
        with open(r, 'w', encoding='latin-1') as f:
            f.write('\n'.join(ls) + '\n')
        rutas.append(r)
    return rutas


def haz_esc(anf, rutas, salida, cm, cd, cb, fecha, corpus):
    args = [anf, '-o', salida, '-F', '-d', str(fecha), '-v', str(cm), str(cd), str(cb)]
    for c in corpus:
        args += ['-k', c]
    for r in rutas:
        args += ['-f', r]
    p = subprocess.run(args, capture_output=True, text=True, errors='replace')
    sys.stderr.write(p.stderr)
    if p.returncode or not os.path.isfile(salida):
        falla('no se ha podido hacer %s' % salida)
    # lo que gasta en el equipo: cargarlo con el mismo motor
    p = subprocess.run([anf, '-f', salida], capture_output=True, text=True, errors='replace')
    m = re.search(r'(\d+) K en memoria', p.stderr)
    return int(m.group(1)) if m else -1


def nombre_cuerpo(l):
    return re.split('[:;]', l, 1)[0]


def main():
    global CORTE
    a = argparse.ArgumentParser(description='Los paquetes de las firmas de Escudo (escbase, escmes y escdia).')
    a.add_argument('--cvd', required=True)
    a.add_argument('--sal', required=True)
    a.add_argument('--estado')
    a.add_argument('--nuevo-mes', action='store_true')
    a.add_argument('--max-sumas', type=int, default=600000)
    a.add_argument('--max-dia', type=float, default=3.0)
    a.add_argument('--publica')
    a.add_argument('--clave')
    a.add_argument('--actualiza', action='store_true')
    a.add_argument('--corpus', action='append', default=[])
    a.add_argument('--corte', type=int, default=16)
    o = a.parse_args()
    if not o.corpus:
        o.corpus = [c for c in CORPUS if os.path.isdir(c)]
    CORTE = o.corte
    if o.publica and not o.clave:
        falla('para --publica hace falta --clave (la privada de Hispa-Pentium)')
    estado = o.estado or os.path.join(o.sal, 'estado')
    os.makedirs(o.sal, exist_ok=True)
    os.makedirs(estado, exist_ok=True)
    if o.actualiza:
        marca = os.path.join(estado, 'actualizado')
        if not os.path.exists(marca) or time.time() - os.path.getmtime(marca) > 20 * 3600:
            cvd = shutil.which('cvd') or '/opt/cvdupdate/bin/cvd'
            if not os.path.exists(cvd):
                falla('no está cvdupdate (la herramienta de ClamAV para los espejos)')
            subprocess.run([cvd, 'update'], check=True)
            open(marca, 'w').close()
    main_cvd, daily_cvd = busca(o.cvd, 'main'), busca(o.cvd, 'daily')
    if not main_cvd or not daily_cvd:
        falla('no están main.cvd y daily.cvd en %s' % o.cvd)
    byte_cvd = busca(o.cvd, 'bytecode')
    cm, cd = cabecera(main_cvd), cabecera(daily_cvd)
    cb = cabecera(byte_cvd)['version'] if byte_cvd else 0
    hoy = time.strftime('%Y%m%d')
    with tempfile.TemporaryDirectory() as w:
        anf = os.path.join(w, 'escudo-anfitrion')
        p = subprocess.run(['bash', os.path.join(ESCUDO, 'hacer.sh'), '--anfitrion', anf], capture_output=True, text=True)
        if p.returncode or not os.path.exists(anf):
            falla('no se ha podido hacer escudo-anfitrion:\n' + p.stdout + p.stderr)
        for r in (main_cvd, daily_cvd):
            p = subprocess.run([anf, '-c', r], capture_output=True, text=True, errors='replace')
            print(p.stdout.strip())
            if p.returncode:
                falla('la firma digital de %s no es buena: no se usa' % r)
        m, d = saca(main_cvd), saca(daily_cvd)
        todo = lee_todo(m, d)
        lic = m.get('COPYING') or d.get('COPYING')
        if not lic:
            falla('no está el COPYING de ClamAV dentro de los .cvd')
        with open(os.path.join(w, 'COPYING.TXT'), 'wb') as f:
            f.write(lic.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
        paqs = []
        # ---- escbase: lo de main (se rehace solo cuando cambia main.cvd) ----
        vb = os.path.join(estado, 'BASE.V')
        base_v = int(open(vb).read().split()[0]) if os.path.exists(vb) else None
        if base_v != cm['version'] or not os.path.exists(os.path.join(o.sal, 'ESCBASE.HPQ')):
            sumas, total = elige_sumas_main(todo, o.max_sumas)
            pc = {k: [] for k in todo}
            for k, l in sumas:
                pc[k].append(l)
            for k in ('ndb', 'ldb', 'fp', 'sfp', 'ign2'):
                pc[k] = [l for l, de_main in todo[k] if de_main]
            rutas = escribe_bases(os.path.join(w, 'base'), pc)
            kb = haz_esc(anf, rutas, os.path.join(w, 'BASE.ESC'), cm['version'], 0, cb, hoy, o.corpus)
            print('escudo_firmas: escbase (main.cvd %d): %d de %d sumas, %d del cuerpo, %d lógicas; %d K en memoria en el equipo'
                  % (cm['version'], len(sumas), total, len(pc['ndb']), len(pc['ldb']), kb))
            paqs.append(('escbase', '%d' % cm['version'], 'Escudo: las firmas de ClamAV, la base (main.cvd %d)' % cm['version'],
                         'escudo >= 0.2', ['BASE.ESC', 'COPYING.TXT']))
            nueva_base = True
        else:
            nueva_base = False
        # ---- escmes: lo de daily el día que se hace (cada mes, o si el del día crece) ----
        de_daily = {k: [l for l, de_main in todo[k] if not de_main] for k in todo}
        lst = os.path.join(estado, 'MES.LST.gz')
        mes_v = None
        if os.path.exists(lst):
            with gzip.open(lst, 'rt') as f:
                cab = f.readline().split()
                mes_v = (int(cab[0]), cab[1])
                en_mes = set(x.strip() for x in f)
        rehacer = o.nuevo_mes or nueva_base or mes_v is None or \
            (time.mktime(time.strptime(hoy, '%Y%m%d')) - time.mktime(time.strptime(mes_v[1], '%Y%m%d'))) > DIAS_BASE * 86400
        if not rehacer:
            dia = {k: [l for l in de_daily[k] if huella(k, l) not in en_mes] for k in todo}
            vivos = set(nombre_cuerpo(l) for k in ('ndb', 'ldb') for l in de_daily[k])
            ret = os.path.join(estado, 'NOMBRES.gz')
            if os.path.exists(ret):
                with gzip.open(ret, 'rt') as f:
                    dia['ign2'] += [n.strip() for n in f if n.strip() and n.strip() not in vivos]
            tam = sum(len(l) + 1 for k in dia for l in dia[k])
            if tam > o.max_dia * 3 * (1 << 20):     # (en texto; comprimido, un tercio)
                print('escudo_firmas: lo del día ya pasa de %.1f MB: se rehace el del mes' % o.max_dia)
                rehacer = True
        if rehacer:
            rutas = escribe_bases(os.path.join(w, 'mes'), de_daily)
            km = haz_esc(anf, rutas, os.path.join(w, 'MES.ESC'), 0, cd['version'], cb, hoy, o.corpus)
            print('escudo_firmas: escmes (daily.cvd %d): %d sumas, %d del cuerpo, %d lógicas; %d K en memoria en el equipo'
                  % (cd['version'], sum(len(de_daily[k]) for k in SUMAS), len(de_daily['ndb']), len(de_daily['ldb']), km))
            with gzip.open(lst + '.nuevo', 'wt') as f:
                f.write('%d %s\n' % (cd['version'], hoy))
                for k in de_daily:
                    for l in de_daily[k]:
                        f.write(huella(k, l) + '\n')
            with gzip.open(os.path.join(estado, 'NOMBRES.gz.nuevo'), 'wt') as f:
                for k in ('ndb', 'ldb'):
                    for l in de_daily[k]:
                        f.write(nombre_cuerpo(l) + '\n')
            mes_v = (cd['version'], hoy)
            paqs.append(('escmes', '%d' % cd['version'], 'Escudo: las firmas de ClamAV del mes (daily.cvd %d)' % cd['version'],
                         'escudo >= 0.2, escbase', ['MES.ESC']))
            dia = {k: [] for k in todo}
        # ---- escdia: lo nuevo desde el del mes, y lo que ClamAV ha retirado ----
        rutas = escribe_bases(os.path.join(w, 'dia'), dia)
        kd = haz_esc(anf, rutas, os.path.join(w, 'DIA.ESC'), 0, cd['version'], cb, hoy, o.corpus)
        print('escudo_firmas: escdia (daily.cvd %d): %s; %d K en memoria con él' % (
            cd['version'], ', '.join('%d %s' % (len(v), k) for k, v in dia.items() if v) or 'nada nuevo', kd))
        with open(os.path.join(w, 'ORIGEN.TXT'), 'wb') as f:
            f.write(('Escudo: de donde salen sus firmas\r\n'
                     'Las firmas son las de ClamAV (Cisco Talos, https://www.clamav.net/), con\r\n'
                     'su licencia (GPL-2: COPYING.TXT, en el paquete escbase), elegidas\r\n'
                     '(Windows, documentos y guiones) y convertidas al indice de Escudo por\r\n'
                     'tools/escudo_firmas.py:\r\n'
                     '  BASE.ESC: main.cvd %d\r\n'
                     '  MES.ESC:  daily.cvd %d (del %s)\r\n'
                     '  DIA.ESC:  daily.cvd %d (%s): lo nuevo desde MES.ESC\r\n'
                     % (cm['version'], mes_v[0], mes_v[1], cd['version'], cd['fecha'])).encode('ascii', 'replace'))
        paqs.append(('escdia', '%d' % cd['version'], 'Escudo: las firmas de ClamAV del día (daily.cvd %d)' % cd['version'],
                     'escudo >= 0.2, escmes = %d' % mes_v[0], ['DIA.ESC', 'ORIGEN.TXT']))
        dest = '\\HISPA\\ESCUDO\\FIRMAS\\'
        for paq, ver, desc, nec, archivos in paqs:
            hpq = os.path.join(o.sal, paq.upper() + '.HPQ')
            subprocess.run([sys.executable, '-I', HPQ, 'crea', hpq, '--nombre', paq, '--version', ver, '--descripcion', desc,
                            '--necesita', nec] + ['%s=%s%s' % (os.path.join(w, x), dest, x) for x in archivos],
                           check=True, stdout=subprocess.DEVNULL)
            print('escudo_firmas: %s %s (%d octetos)' % (paq, ver, os.path.getsize(hpq)))
        if nueva_base:
            with open(vb, 'w') as f:
                f.write('%d\n' % cm['version'])
        if rehacer:
            os.replace(lst + '.nuevo', lst)
            os.replace(os.path.join(estado, 'NOMBRES.gz.nuevo'), os.path.join(estado, 'NOMBRES.gz'))
        with open(os.path.join(o.sal, 'CATALOGO.LIN'), 'wb') as f:
            for paq in ('ESCBASE', 'ESCMES', 'ESCDIA'):
                r = os.path.join(o.sal, paq + '.HPQ')
                if os.path.exists(r):
                    f.write(subprocess.run([sys.executable, '-I', HPQ, 'linea', r], check=True, capture_output=True).stdout)
        if o.publica:
            for paq in ('ESCBASE', 'ESCMES', 'ESCDIA'):
                shutil.copyfile(os.path.join(o.sal, paq + '.HPQ'), os.path.join(o.publica, paq + '.HPQ'))
            subprocess.run([sys.executable, '-I', HPQ, 'catalogo', o.publica, '--clave', o.clave], check=True)
            print('escudo_firmas: publicados en %s, con el catálogo firmado otra vez' % o.publica)


if __name__ == '__main__':
    main()
