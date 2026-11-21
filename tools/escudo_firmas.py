#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  Hispa-Pentium - tools/escudo_firmas.py: los paquetes de las firmas de Escudo
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  Escudo (castel/nucleo32/escudo) mira los archivos con las firmas de ClamAV
#  (de Cisco Talos). Los equipos con Hispa-Pentium NO las bajan del CDN de
#  ClamAV: su FAQ («Freshclam», docs.clamav.net) dice que las únicas formas
#  admitidas de bajarlas son freshclam y cvdupdate, que las descargas con
#  guiones (wget, curl...) se deniegan y que a quien baja demasiado se le
#  corta (403/429); y el CDN ya rechaza a un cliente que no es uno de esos
#  (probado el 21/11/2026: 403). Así que se hace como pide ClamAV para muchos
#  equipos: UN espejo propio, en el VPS, puesto al día con cvdupdate (su
#  herramienta: una comprobación al día como mucho, con su User-Agent), y
#  de ahí, este guion hace dos paquetes de Hispa-Pentium, firmados con el
#  catálogo (Ed25519) como todos, que los equipos traen de
#  http://actualiza.hispados.es/pentium/ con PAQUETES:
#
#    escbase  (BASE.ESC)  lo de main.cvd: cambia poco (meses)
#    escdia   (DIA.ESC)   lo de daily.cvd: cada día
#
#  FIRMAS.ESC es el índice propio de Escudo (el formato, en motor.c): las
#  sumas por tamaño (solo se lee del disco la página que hace falta), los
#  nombres aparte y las firmas del cuerpo (.ndb y .ldb) en texto. Lo hace el
#  mismo motor que va en Hispa-Pentium, compilado para Linux
#  (escudo-anfitrion: nucleo32/escudo/hacer.sh --anfitrion), después de
#  comprobar la firma digital de cada .cvd.
#
#  Uso:
#    escudo_firmas.py --cvd CARPETA [--sal CARPETA] [--publica CARPETA --clave PRIVADA]
#                     [--limite-base MB] [--limite-dia MB] [--actualiza]
#      --cvd        dónde están main.cvd y daily.cvd (el espejo de cvdupdate:
#                   ~/.cvdupdate/database, o lo que diga «cvd config show»)
#      --sal        dónde dejar ESCBASE.HPQ, ESCDIA.HPQ y CATALOGO.LIN
#      --publica    la carpeta del servidor de actualizaciones: pone ahí los
#                   dos paquetes (en lugar de los de antes) y firma otra vez
#                   su catálogo con --clave (tools/hpq.py)
#      --limite-*   lo más que mide cada .ESC (si no cabe, se dejan primero
#                   las sumas de main: ver LEEME de Escudo); 0, sin límite
#      --actualiza  antes, «cvd update» (cvdupdate) si no se ha hecho en las
#                   últimas 20 horas
#  Las licencias: las bases de ClamAV van con la GPL-2 (su COPYING, que va
#  dentro de cada .cvd y se copia en el paquete como COPYING.TXT); lo que hace
#  este guion es un cambio de formato de esas mismas firmas, y se reparte con
#  esa licencia y diciendo de dónde salen (ORIGEN.TXT).
# =============================================================================
import argparse
import io
import os
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


def copying(ruta):
    """El COPYING de dentro del .cvd (la licencia de las firmas)."""
    with open(ruta, 'rb') as f:
        f.seek(512)
        datos = f.read()
    modo = 'r:gz' if datos[:2] == b'\x1f\x8b' else 'r:'
    with tarfile.open(fileobj=io.BytesIO(datos), mode=modo) as t:
        for m in t.getmembers():
            if m.name == 'COPYING' and m.isfile():
                return t.extractfile(m).read()
    return None


def main():
    a = argparse.ArgumentParser(description='Los paquetes de las firmas de Escudo (escbase y escdia).')
    a.add_argument('--cvd', required=True)
    a.add_argument('--sal')
    a.add_argument('--publica')
    a.add_argument('--clave')
    a.add_argument('--limite-base', type=int, default=48)
    a.add_argument('--limite-dia', type=int, default=24)
    a.add_argument('--actualiza', action='store_true')
    o = a.parse_args()
    if not o.sal and not o.publica:
        falla('diga --sal o --publica')
    if o.publica and not o.clave:
        falla('para --publica hace falta --clave (la privada de Hispa-Pentium)')
    if o.actualiza:
        marca = os.path.join(o.cvd, '.escudo_actualizado')
        if not os.path.exists(marca) or time.time() - os.path.getmtime(marca) > 20 * 3600:
            if shutil.which('cvd') is None:
                falla('no está cvdupdate («pip install cvdupdate»: la herramienta de ClamAV para los espejos)')
            subprocess.run(['cvd', 'update'], check=True)
            open(marca, 'w').close()
    main_cvd, daily_cvd = busca(o.cvd, 'main'), busca(o.cvd, 'daily')
    if not main_cvd or not daily_cvd:
        falla('no están main.cvd y daily.cvd en %s' % o.cvd)
    cm, cd = cabecera(main_cvd), cabecera(daily_cvd)
    with tempfile.TemporaryDirectory() as w:
        anf = os.path.join(w, 'escudo-anfitrion')
        subprocess.run(['bash', os.path.join(ESCUDO, 'hacer.sh'), '--anfitrion', anf], check=True)
        # la firma digital de ClamAV de cada uno (si no es buena, no se sigue)
        for r in (main_cvd, daily_cvd):
            p = subprocess.run([anf, '-c', r], capture_output=True, text=True)
            print(p.stdout.strip())
            if p.returncode:
                falla('la firma digital de %s no es buena: no se usa' % r)
        # los dos .ESC: con todo cargado (así las .fp y las .ign2 de daily
        # valen también para main), cada uno con lo suyo
        for origen, nombre, limite in (('main', 'BASE.ESC', o.limite_base), ('dia', 'DIA.ESC', o.limite_dia)):
            r = subprocess.run([anf, '-f', main_cvd, '-f', daily_cvd, '-o', os.path.join(w, nombre), '-F', '-O', origen,
                                '-l', str(limite)], capture_output=True, text=True)
            sys.stderr.write(r.stderr)
            if r.returncode:
                falla('no se ha podido hacer %s' % nombre)
        lic = copying(main_cvd) or copying(daily_cvd)
        if not lic:
            falla('no está el COPYING de ClamAV dentro de los .cvd')
        with open(os.path.join(w, 'COPYING.TXT'), 'wb') as f:
            f.write(lic.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
        hoy = time.strftime('%d/%m/%Y')
        with open(os.path.join(w, 'ORIGEN.TXT'), 'wb') as f:
            f.write(('Escudo: de donde salen sus firmas\r\n'
                     'Las firmas son las de ClamAV (Cisco Talos, https://www.clamav.net/), con\r\n'
                     'su licencia (GPL-2: COPYING.TXT). Convertidas al indice de Escudo\r\n'
                     '(BASE.ESC y DIA.ESC) por tools/escudo_firmas.py el %s:\r\n'
                     '  main.cvd version %d (%d firmas, %s)\r\n'
                     '  daily.cvd version %d (%d firmas, %s)\r\n'
                     % (hoy, cm['version'], cm['firmas'], cm['fecha'], cd['version'], cd['firmas'], cd['fecha'])).encode('ascii', 'replace'))
        sal = o.sal or os.path.join(w, 'paq')
        os.makedirs(sal, exist_ok=True)
        dest = '\\HISPA\\ESCUDO\\FIRMAS\\'
        for paq, ver, desc, archivos in (
                ('escbase', '%d' % cm['version'], 'Escudo: las firmas de ClamAV, la base (main.cvd %d)' % cm['version'],
                 ['BASE.ESC', 'COPYING.TXT']),
                ('escdia', '%d' % cd['version'], 'Escudo: las firmas de ClamAV del día (daily.cvd %d)' % cd['version'],
                 ['DIA.ESC', 'ORIGEN.TXT'])):
            hpq = os.path.join(sal, paq.upper() + '.HPQ')
            subprocess.run([sys.executable, '-I', HPQ, 'crea', hpq, '--nombre', paq, '--version', ver, '--descripcion', desc,
                            '--necesita', 'escudo >= 0.1'] + ['%s=%s%s' % (os.path.join(w, x), dest, x) for x in archivos],
                           check=True, stdout=subprocess.DEVNULL)
            print('escudo_firmas: %s %s (%d octetos)' % (paq, ver, os.path.getsize(hpq)))
        if o.sal:
            with open(os.path.join(o.sal, 'CATALOGO.LIN'), 'wb') as f:
                for paq in ('ESCBASE', 'ESCDIA'):
                    f.write(subprocess.run([sys.executable, '-I', HPQ, 'linea', os.path.join(o.sal, paq + '.HPQ')],
                                           check=True, capture_output=True).stdout)
        if o.publica:
            for paq in ('ESCBASE', 'ESCDIA'):
                shutil.copyfile(os.path.join(sal, paq + '.HPQ'), os.path.join(o.publica, paq + '.HPQ'))
            subprocess.run([sys.executable, '-I', HPQ, 'catalogo', o.publica, '--clave', o.clave], check=True)
            print('escudo_firmas: publicados en %s, con el catálogo firmado otra vez' % o.publica)


if __name__ == '__main__':
    main()
