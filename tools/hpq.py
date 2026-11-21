#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  Hispa-Pentium - tools/hpq.py: hacer paquetes .HPQ y su catálogo
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  Un paquete .HPQ es lo que instala, actualiza y quita el programa PAQUETES
#  de Hispa-Pentium (castel/nucleo32/upaquete.cas). Formato (docs/PENTIUM.md,
#  sección 15):
#
#    una cabecera de texto (página 437, líneas acabadas en CR LF):
#        HPQ 1
#        nombre=oficina                 (hasta 8 letras: a-z, 0-9, «-» y «_»)
#        version=1.0                    (números separados por puntos)
#        descripcion=La oficina de Hispa-386
#        necesita=hvprun >= 0.7, nucleo >= 0.7     (puede ir vacío)
#        choca=                         (los que no pueden estar a la vez)
#        archivo=\OFICINA\PLUMA.HVP 45678 <SHA-256 en 64 cifras hex>
#        ...                            (uno por archivo, en el orden de los datos)
#        fin
#    y, detrás, los archivos tal cual, uno detrás de otro, en ese orden.
#
#  Los operadores de «necesita» y «choca»: >=, <=, =, >, < (o nada: cualquier
#  versión). «nucleo» es el núcleo que está en marcha (su versión la da él).
#  Las líneas que PAQUETES no conoce se dejan pasar (para lo que venga luego:
#  «firma=», por ejemplo, está reservada para la firma del paquete).
#
#  El catálogo (CATALOGO.TXT, al lado de los paquetes):
#        HPQ-CATALOGO 1
#        paquete=<nombre> <versión> <ARCHIVO.HPQ> <octetos> <SHA-256 del .HPQ> <descripción>
#
#  Lo instalado, en C:\HISPA\PAQUETES\<NOMBRE>.PQI: la cabecera del paquete,
#  tal cual (con «registro» se saca de un .HPQ: así lo apunta INSTALAR).
#
#  Uso:
#    hpq.py crea SALIDA.HPQ --nombre N --version V [--descripcion D]
#               [--necesita "a >= 1, b"] [--choca "c"] ORIGEN=\DESTINO ...
#    hpq.py catalogo CARPETA          (CATALOGO.TXT con los .HPQ de la carpeta)
#    hpq.py mira PAQUETE.HPQ          (lo que lleva, y comprueba sus sumas)
#    hpq.py registro PAQUETE.HPQ SALIDA.PQI
#    hpq.py sistema SALIDA CARPETA...  (los paquetes del sistema, con sus
#               registros .PQI, el catálogo y CARPETAS.TXT para INSTALAR, de
#               los archivos ya hechos que haya en esas carpetas: usb.sh)
#    hpq.py opcionales SALIDA CARPETA...  (los que no van con el sistema:
#               «navega», el Navegador; de lo que deja su hacer.sh)
#    hpq.py linea PAQUETE.HPQ         (su línea para CATALOGO.TXT)
#
#  La firma (Ed25519, RFC 8032): el catálogo lleva las sumas de todos los
#  paquetes, y cada paquete las de sus archivos; así que basta con firmar el
#  catálogo. La firma va al lado, en CATALOGO.FIR (una línea: «ed25519 » y
#  las 128 cifras hexadecimales de la firma). PAQUETES la comprueba con la
#  clave pública que lleva dentro (castel/nucleo32/clave.cas), y sin ella no
#  instala nada traído de Internet.
#    hpq.py clave PRIVADA             (hace una clave nueva: la privada, al
#               archivo PRIVADA, con permisos 600; dice la pública)
#    hpq.py publica PRIVADA [clave.cas]  (la pública; con clave.cas, el
#               módulo de Castel que la lleva)
#    hpq.py firma CARPETA PRIVADA     (CATALOGO.FIR del CATALOGO.TXT de ahí)
#    hpq.py catalogo CARPETA [--clave PRIVADA]   (y, con la clave, firmado)
#    hpq.py comprueba CARPETA PUBLICA (si CATALOGO.FIR es buena; PUBLICA: las
#               64 cifras, o un clave.cas)
#  La clave privada NO va en el repositorio (la de Hispa-Pentium está en
#  /root/.claude/Secretos/hispa-pentium-firma.key, en el VPS).
# =============================================================================

import hashlib
import os
import re
import sys

MAGIA = 'HPQ 1'
MAGIA_CATALOGO = 'HPQ-CATALOGO 1'
NOMBRE = re.compile(r'^[a-z0-9_-]{1,8}$')
VERSION = re.compile(r'^[0-9]+(\.[0-9]+)*$')
TROZO_83 = re.compile(r'^[A-Z0-9_$~!#%&@^(){}\'`-]{1,8}(\.[A-Z0-9_$~!#%&@^(){}\'`-]{1,3})?$')
LISTA = re.compile(r'^\s*([a-z0-9_-]{1,8})\s*(?:(>=|<=|=|>|<)\s*([0-9]+(?:\.[0-9]+)*))?\s*$')
MAX_ARCHIVOS = 64
MAX_CABECERA = 16384

# Los paquetes del sistema (hpq.py sistema): nombre, versión, descripción,
# lo que necesita y sus archivos (el archivo hecho y adónde va en el disco)
# (la 0.8.1, 20/11/2026: HTTPS —la llamada 99 del núcleo y el paquete tls— y
# cambiar la resolución desde Configuración; solo suben los paquetes que han
# cambiado: base, red y tls. Los demás siguen en la 0.8, la del CD)
VERSION_SISTEMA = '0.8.1'
# (la 0.8.2, 21/11/2026: los controles de las ventanas —Ctrl+Tab, Alt+letra
# y la llamada 81, maximizar— en el núcleo, y MUESTRA.BIN en los ejemplos;
# suben base y ejemplos; y el Administrador de dispositivos —las llamadas
# 155 y 156, ACPI.BIN que apunta lo de ACPI y DISPOSIT.BIN—)
VERSION_082 = '0.8.2'
# (la 0.8.3, 21/11/2026, todo junto: el núcleo con la rueda del ratón, el x87
# de cada programa y programas de hasta 8 MB, para el Navegador —el paquete
# «navega», OPCIONALES—, y el Administrador de dispositivos; suben base y equipo)
VERSION_BASE = '0.8.3'
VERSION_08 = '0.8'
# (la 0.8.4, 21/11/2026, tres ramas juntas en pentium-084: los CD y DVD
# —las llamadas 132 y 133, MONTAR y EXPULSAR—, SSH y Tailscale —TCP en
# escucha y la interfaz del túnel en RED.BIN, ORDENES /C, la marca del túnel
# en la bandeja y Configuración > «Red y Tailscale...»— y el explorador de
# archivos con el dock —las llamadas 170 y 171—; suben base, red, equipo
# (0.9, con MINIAT.BIN) y ejemplos (MUESTRA, con los controles nuevos), y
# salen ssh y tailscal —Tailscale: los nombres de
# paquete son de 8 letras, por el registro <NOMBRE>.PQI—)
VERSION_084 = '0.8.4'
# [WIN32] (21/11/2026) los programas de Windows: en el núcleo (los
# segmentos de Win32, las llamadas 180 a 182; el coprocesador de cada tarea
# es el de la 0.8.3), ORDENES (los .EXE) y Mi equipo (los abre); y el
# paquete win32 (1.0, que pide la base 0.8.4)
SISTEMA = [
    ('base', VERSION_084, 'El sistema base: el núcleo, el intérprete de órdenes, el asistente, INSTALAR y el arranque', '', [
        ('ESCRIT8.BIN', r'\HISPA\NUCLEO.BIN'), ('ORDENES.BIN', r'\HISPA\ORDENES.BIN'),
        ('ASISTE.BIN', r'\HISPA\ASISTE.BIN'), ('INSTALAR.BIN', r'\HISPA\INSTALAR.BIN'), ('ACPI.BIN', r'\HISPA\ACPI.BIN'),
        ('LEEME.TXT', r'\HISPA\LEEME.TXT'), ('LEEME.TXT', r'\LEEME.TXT'), ('MANUAL.TXT', r'\HISPA\MANUAL.TXT'),
        ('BOOT.IMG', r'\HISPA\BOOT.IMG'), ('CORE.IMG', r'\HISPA\CORE.IMG')]),
    ('bloc', VERSION_08, 'El Bloc de notas', 'base >= 0.7', [('BLOC.BIN', r'\HISPA\BLOC.BIN')]),
    ('consola', VERSION_08, 'La Consola: el intérprete de órdenes en una ventana', 'base >= 0.7', [('CONSOLA.BIN', r'\HISPA\CONSOLA.BIN')]),
    # (explorador, dispositivos) el explorador de archivos, sus miniaturas y el Administrador de dispositivos
    ('equipo', '0.9', 'Mi equipo, el explorador de archivos (carpetas, vistas, copiar y pegar, la Papelera, buscar y las miniaturas) y el Administrador de dispositivos', 'base >= 0.8.4', [
        ('EQUIPO.BIN', r'\HISPA\EQUIPO.BIN'), ('MINIAT.BIN', r'\HISPA\MINIAT.BIN'), ('DISPOSIT.BIN', r'\HISPA\DISPOSIT.BIN')]),
    ('ejemplos', VERSION_084, 'Programas de ejemplo: HOLA, ECO, PRUEBA, BUCLE y MUESTRA (los controles de las ventanas)', 'base >= 0.8.4', [
        ('HOLA.BIN', r'\HISPA\HOLA.BIN'), ('ECO.BIN', r'\HISPA\ECO.BIN'),
        ('PRUEBA.BIN', r'\HISPA\PRUEBA.BIN'), ('BUCLE.BIN', r'\HISPA\BUCLE.BIN'), ('MUESTRA.BIN', r'\HISPA\MUESTRA.BIN')]),
    ('paquetes', '1.0', 'Programas y actualizaciones: PAQUETES y su ventana', 'base >= 0.7, nucleo >= 0.7', [
        ('PAQUETES.BIN', r'\HISPA\PAQUETES.BIN'), ('ACTUALIZ.BIN', r'\HISPA\ACTUALIZ.BIN')]),
    ('red', VERSION_084, 'La red: el servicio de red (RED.BIN), IPCONFIG, PING y DESCARGAR', 'base >= 0.8.4', [
        ('RED.BIN', r'\HISPA\RED.BIN'), ('IPCONFIG.BIN', r'\HISPA\IPCONFIG.BIN'),
        ('PING.BIN', r'\HISPA\PING.BIN'), ('DESCARGA.BIN', r'\HISPA\DESCARGA.BIN')]),
    ('tls', VERSION_SISTEMA, 'HTTPS: el servicio TLS (TLS.BIN, con BearSSL) y la orden HTTPS', 'base >= 0.8.1, red >= 0.8.1', [
        ('TLS.BIN', r'\HISPA\TLS.BIN'), ('HTTPS.BIN', r'\HISPA\HTTPS.BIN')]),
    ('ssh', VERSION_084, 'SSH: entrar en otros equipos (SSH) y en este desde otros (SSHD, apagado hasta que se encienda)', 'base >= 0.8.4, red >= 0.8.4', [
        ('SSH.BIN', r'\HISPA\SSH.BIN'), ('SSHD.BIN', r'\HISPA\SSHD.BIN')]),
    ('tailscal', VERSION_084, 'Tailscale y WireGuard: la red privada de sus equipos, cada uno con su nombre y su IP 100.x', 'base >= 0.8.4, red >= 0.8.4, tls >= 0.8.1', [
        ('TAILSCAL.BIN', r'\HISPA\TAILSCAL.BIN'), ('WG.BIN', r'\HISPA\WG.BIN')]),
    ('hvprun', VERSION_08, 'HVPRUN: los programas de Hispa Ventanas dentro del escritorio', 'base >= 0.7', [
        ('HVPRUN.BIN', r'\HISPA\HVPRUN.BIN')]),
    ('oficina', '1.0', 'La oficina de Hispa-386: Facturas, Pluma, Cuentas y Mirador', 'hvprun >= 0.7', [
        ('FACTURAS.HVP', r'\OFICINA\FACTURAS.HVP'), ('PLUMA.HVP', r'\OFICINA\PLUMA.HVP'),
        ('CUENTAS.HVP', r'\OFICINA\CUENTAS.HVP'), ('MIRADOR.HVP', r'\OFICINA\MIRADOR.HVP'),
        ('TIPOS.DAT', r'\OFICINA\TIPOS.DAT'), ('DICCIO.DAT', r'\OFICINA\DICCIO.DAT')]),
    # [WIN32] los programas de Windows de 32 bits y DirectX (docs/pentium/WIN32.md)
    ('win32', '1.0', 'Programas de Windows de 32 bits y DirectX: WIN32.BIN, sus DLL y los ejemplos (Naves)', 'base >= ' + VERSION_084, [
        ('WIN32.BIN', r'\HISPA\WIN32.BIN'),
        ('MSVCRT.DLL', r'\WIN32\MSVCRT.DLL'), ('UCRT.DLL', r'\WIN32\UCRT.DLL'), ('COMCTL32.DLL', r'\WIN32\COMCTL32.DLL'),
        ('WINMM.DLL', r'\WIN32\WINMM.DLL'), ('DDRAW.DLL', r'\WIN32\DDRAW.DLL'), ('DSOUND.DLL', r'\WIN32\DSOUND.DLL'),
        ('DINPUT.DLL', r'\WIN32\DINPUT.DLL'), ('OLE32.DLL', r'\WIN32\OLE32.DLL'),
        ('CALC32.EXE', r'\HISPA32\CALC32.EXE'), ('HOLA32.EXE', r'\HISPA32\HOLA32.EXE'), ('CTL32.EXE', r'\HISPA32\CTL32.EXE'),
        ('MENU32.EXE', r'\HISPA32\MENU32.EXE'), ('AVISO32.EXE', r'\HISPA32\AVISO32.EXE'), ('NAVES.EXE', r'\HISPA32\NAVES.EXE'),
        ('NAVES.ICP', r'\HISPA32\NAVES.ICP'), ('NAVES.MNU', r'\HISPA\MENU\NAVES.MNU')]),
]


def falla(texto):
    sys.stderr.write('hpq: %s\n' % texto)
    sys.exit(1)


def a437(t):
    try:
        return t.encode('cp437')
    except UnicodeEncodeError:
        falla('«%s» lleva letras que no están en la página 437' % t)


def comprueba_lista(t, que):
    t = t.strip()
    if not t:
        return ''
    trozos = [x.strip() for x in t.split(',')]
    for x in trozos:
        m = LISTA.match(x)
        if not m:
            falla('«%s» (en %s) no vale: «nombre», o «nombre >= 1.2»' % (x, que))
    return ', '.join(trozos)


def comprueba_destino(d):
    if not d.startswith('\\') or d.endswith('\\'):
        falla('el destino «%s» tiene que ser una ruta entera: \\CARPETA\\ARCHIVO.EXT' % d)
    trozos = d[1:].split('\\')
    for t in trozos:
        if not TROZO_83.match(t):
            falla('el destino «%s» no es de nombres 8+3 en mayúsculas' % d)
    if d.upper().startswith('\\HISPA\\PAQUETES\\'):
        falla('\\HISPA\\PAQUETES es la base de datos de lo instalado: ahí no va un archivo de un paquete')
    if len(d) > 79:
        falla('el destino «%s» es demasiado largo' % d)
    return d


def sha(datos):
    return hashlib.sha256(datos).hexdigest()


def cabecera(nombre, version, descripcion, necesita, choca, archivos):
    """archivos: [(destino, datos)]. Da la cabecera (en octetos)."""
    if not NOMBRE.match(nombre):
        falla('el nombre «%s» no vale (hasta 8 letras: a-z, 0-9, - y _)' % nombre)
    if not VERSION.match(version):
        falla('la versión «%s» no vale (números separados por puntos)' % version)
    if len(archivos) > MAX_ARCHIVOS:
        falla('demasiados archivos en un paquete (hasta %d)' % MAX_ARCHIVOS)
    vistos = set()
    lineas = [MAGIA, 'nombre=' + nombre, 'version=' + version,
              'descripcion=' + descripcion.replace('\n', ' ')[:200],
              'necesita=' + comprueba_lista(necesita, 'necesita'),
              'choca=' + comprueba_lista(choca, 'choca')]
    for d, datos in archivos:
        comprueba_destino(d)
        if d.upper() in vistos:
            falla('el destino «%s» sale dos veces' % d)
        vistos.add(d.upper())
        lineas.append('archivo=%s %d %s' % (d, len(datos), sha(datos)))
    lineas.append('fin')
    c = b''.join(a437(x) + b'\r\n' for x in lineas)
    if len(c) > MAX_CABECERA:
        falla('la cabecera no cabe en %d octetos' % MAX_CABECERA)
    return c


def haz_paquete(salida, nombre, version, descripcion, necesita, choca, archivos):
    c = cabecera(nombre, version, descripcion, necesita, choca, archivos)
    with open(salida, 'wb') as f:
        f.write(c)
        for _, datos in archivos:
            f.write(datos)
    return c


def lee_paquete(ruta):
    """Da (campos, archivos [(destino, tamaño, sha)], largo de la cabecera, datos)."""
    datos = open(ruta, 'rb').read()
    fin = datos.find(b'\r\nfin\r\n')
    if not datos.startswith(MAGIA.encode() + b'\r\n') or fin < 0:
        falla('%s no es un paquete .HPQ' % ruta)
    largo = fin + 7
    campos = {}
    archivos = []
    for linea in datos[:fin].decode('cp437').split('\r\n')[1:]:
        k, _, v = linea.partition('=')
        if k == 'archivo':
            d, t, s = v.split(' ')
            archivos.append((d, int(t), s))
        else:
            campos[k] = v
    return campos, archivos, largo, datos


def mira(ruta):
    campos, archivos, largo, datos = lee_paquete(ruta)
    print('%s: %s %s — %s' % (os.path.basename(ruta), campos.get('nombre'), campos.get('version'), campos.get('descripcion')))
    print('  necesita: %s' % (campos.get('necesita') or '(nada)'))
    print('  choca:    %s' % (campos.get('choca') or '(nada)'))
    malos = 0
    p = largo
    for d, t, s in archivos:
        bien = sha(datos[p:p + t]) == s and p + t <= len(datos)
        malos += not bien
        print('  %-30s %9d  %s' % (d, t, 'bien' if bien else 'MAL (la suma no es la suya)'))
        p += t
    if p != len(datos):
        print('  el paquete mide %d octetos y tenía que medir %d' % (len(datos), p))
        malos += 1
    return 1 if malos else 0


def catalogo(carpeta):
    filas = []
    for n in sorted(os.listdir(carpeta)):
        if not n.upper().endswith('.HPQ'):
            continue
        ruta = os.path.join(carpeta, n)
        campos, _, _, datos = lee_paquete(ruta)
        if n != n.upper() or not TROZO_83.match(n):
            falla('el paquete %s tiene que llamarse en 8+3 y en mayúsculas' % n)
        filas.append('paquete=%s %s %s %d %s %s' % (campos['nombre'], campos['version'], n, len(datos), sha(datos),
                                                    campos.get('descripcion', '')))
    with open(os.path.join(carpeta, 'CATALOGO.TXT'), 'wb') as f:
        f.write(a437(MAGIA_CATALOGO) + b'\r\n')
        for x in filas:
            f.write(a437(x) + b'\r\n')
    return len(filas)


def busca(nombre, carpetas):
    for c in carpetas:
        r = os.path.join(c, nombre)
        if os.path.isfile(r):
            return r
    falla('no está %s en %s' % (nombre, ', '.join(carpetas)))


# Los paquetes que no van con el sistema (INSTALAR no los pone: se instalan
# desde «Programas y actualizaciones» o con PAQUETES INSTALA), hechos con
# «hpq.py opcionales SALIDA CARPETA...» de lo que deja su hacer.sh (los
# archivos se buscan por su ruta relativa en esas carpetas). Van en el
# catálogo de Internet con los del sistema (nucleo32/actualiza.sh).
NAVEGA_RECURSOS = ['DEFAULT.CSS', 'QUIRKS.CSS', 'ADBLOCK.CSS', 'INTERNAL.CSS', 'MESSAGES', 'BROTLI.DIC', 'INICIO.HTM']
NAVEGA_FUENTES = ['SANS.TTF', 'SANSN.TTF', 'SERIF.TTF', 'SERIFN.TTF', 'MONO.TTF', 'MONON.TTF']
OPCIONALES = [
    ('navega', '0.1', 'El Navegador: la web de hoy (NetSurf con JavaScript), con pestañas y descargas',
     'base >= 0.8.3, red >= 0.8.1, tls >= 0.8.1', [
        ('NAVEGA/NAVEGA.BIN', r'\HISPA\NAVEGA\NAVEGA.BIN'), ('NAVEGA/NAVEGA.INI', r'\HISPA\NAVEGA\NAVEGA.INI'),
        ('NAVEGA/LEEME.TXT', r'\HISPA\NAVEGA\LEEME.TXT')]
     + [('NAVEGA/RECURSOS/' + n, '\\HISPA\\NAVEGA\\RECURSOS\\' + n) for n in NAVEGA_RECURSOS]
     + [('NAVEGA/FUENTES/' + n, '\\HISPA\\NAVEGA\\FUENTES\\' + n) for n in NAVEGA_FUENTES]
     + [('NAVEGA.MNU', r'\HISPA\MENU\NAVEGA.MNU'), ('NAVEGA.ICP', r'\HISPA\MENU\NAVEGA.ICP')]),
]


def opcionales(salida, carpetas, nombres=None):
    """Los paquetes de OPCIONALES (o los de «nombres») en SALIDA: da cuántos."""
    os.makedirs(salida, exist_ok=True)
    n = 0
    for nombre, version, descripcion, necesita, archivos in OPCIONALES:
        if nombres and nombre not in nombres:
            continue
        lista = [(d, open(busca(o, carpetas), 'rb').read()) for o, d in archivos]
        haz_paquete(os.path.join(salida, nombre.upper() + '.HPQ'), nombre, version, descripcion, necesita, '', lista)
        n += 1
    return n


def linea_catalogo(ruta):
    """La línea del catálogo de un paquete (como la escribe «catalogo»)."""
    carpeta = os.path.dirname(os.path.abspath(ruta))
    tmp = os.path.join(carpeta, '.linea')
    os.makedirs(tmp, exist_ok=True)
    try:
        d = os.path.join(tmp, os.path.basename(ruta))
        if os.path.exists(d):
            os.remove(d)
        os.link(ruta, d)
        catalogo(tmp)
        with open(os.path.join(tmp, 'CATALOGO.TXT'), 'rb') as f:
            return f.read().split(b'\r\n')[1]
    finally:
        for x in os.listdir(tmp):
            os.remove(os.path.join(tmp, x))
        os.rmdir(tmp)


def sistema(salida, carpetas):
    """Los paquetes del sistema en SALIDA (con CATALOGO.TXT) y sus registros
    en SALIDA/PQI (con el CARPETAS.TXT que manda cada uno a \\HISPA\\PAQUETES)."""
    os.makedirs(os.path.join(salida, 'PQI'), exist_ok=True)
    mapa = []
    for nombre, version, descripcion, necesita, archivos in SISTEMA:
        lista = [(d, open(busca(o, carpetas), 'rb').read()) for o, d in archivos]
        N = nombre.upper()
        c = haz_paquete(os.path.join(salida, N + '.HPQ'), nombre, version, descripcion, necesita, '', lista)
        with open(os.path.join(salida, 'PQI', N + '.PQI'), 'wb') as f:
            f.write(c)
        mapa.append('%s.PQI=HISPA\\PAQUETES' % N)
    with open(os.path.join(salida, 'PQI', 'CARPETAS.TXT'), 'wb') as f:
        f.write(''.join(x + '\r\n' for x in mapa).encode())
    return catalogo(salida)


# -------------------------------------------------------------- la firma
def _ed():
    try:
        from cryptography.hazmat.primitives.asymmetric import ed25519
        from cryptography.hazmat.primitives import serialization
    except ImportError:
        falla('para firmar hace falta el módulo «cryptography» de Python (apt install python3-cryptography)')
    return ed25519, serialization


def lee_privada(ruta):
    ed25519, _ = _ed()
    t = open(ruta).read().strip()
    if not re.match(r'^[0-9a-f]{64}$', t):
        falla('%s no es una clave privada de hpq.py (64 cifras hexadecimales)' % ruta)
    return ed25519.Ed25519PrivateKey.from_private_bytes(bytes.fromhex(t))


def publica_de(priv):
    _, serialization = _ed()
    return priv.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def haz_clave(ruta):
    ed25519, serialization = _ed()
    if os.path.exists(ruta):
        falla('%s ya existe: no se pisa una clave' % ruta)
    k = ed25519.Ed25519PrivateKey.generate()
    semilla = k.private_bytes(serialization.Encoding.Raw, serialization.PrivateFormat.Raw, serialization.NoEncryption())
    fd = os.open(ruta, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, 'w') as f:
        f.write(semilla.hex() + '\n')
    return publica_de(k)


def modulo_clave(pub):
    filas = ',\n'.join('        ' + ', '.join('0x%02X' % x for x in pub[i:i + 16]) for i in (0, 16))
    return ('; =============================================================================\n'
            ';  clave.cas - la clave pública con la que PAQUETES comprueba la firma de los\n'
            ';  catálogos (CATALOGO.FIR: Ed25519). La hace «tools/hpq.py publica\n'
            ';  PRIVADA clave.cas»; la privada no está en el repositorio.\n'
            ';  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n'
            '; =============================================================================\n'
            'módulo Clave\n\n'
            '    público constante HUELLA = "%s"\n\n'
            '    público variable pública: arreglo [32] de byte = (\n%s)\n\n'
            'fin Clave.\n') % (pub.hex()[:16], filas)


def firma(carpeta, privada):
    k = lee_privada(privada)
    datos = open(os.path.join(carpeta, 'CATALOGO.TXT'), 'rb').read()
    f = k.sign(datos)
    with open(os.path.join(carpeta, 'CATALOGO.FIR'), 'wb') as s:
        s.write(b'ed25519 ' + f.hex().encode() + b'\r\n')


def comprueba_firma(carpeta, publica):
    ed25519, _ = _ed()
    if os.path.isfile(publica):
        t = open(publica, encoding='utf-8').read()
        b = re.findall(r'0x([0-9A-Fa-f]{2})', t.split('pública')[-1])[:32]
        pub = bytes(int(x, 16) for x in b)
    else:
        pub = bytes.fromhex(publica)
    datos = open(os.path.join(carpeta, 'CATALOGO.TXT'), 'rb').read()
    l = open(os.path.join(carpeta, 'CATALOGO.FIR'), 'rb').read().split()
    try:
        ed25519.Ed25519PublicKey.from_public_bytes(pub).verify(bytes.fromhex(l[1].decode()), datos)
        return True
    except Exception:
        return False


def principal(a):
    if len(a) < 2:
        print(__doc__ if __doc__ else 'uso: hpq.py crea|catalogo|mira|registro|sistema ...')
        sys.stderr.write('uso: hpq.py crea|catalogo|mira|registro|sistema ... (mire la cabecera de tools/hpq.py)\n')
        return 2
    orden = a[1]
    if orden == 'crea':
        salida = a[2]
        op = {'--nombre': None, '--version': None, '--descripcion': '', '--necesita': '', '--choca': ''}
        archivos = []
        i = 3
        while i < len(a):
            if a[i] in op:
                op[a[i]] = a[i + 1]
                i += 2
                continue
            o, sep, d = a[i].partition('=')
            if not sep:
                falla('«%s»: los archivos se dicen así: ORIGEN=\\DESTINO' % a[i])
            archivos.append((d, open(o, 'rb').read()))
            i += 1
        if not op['--nombre'] or not op['--version']:
            falla('faltan --nombre y --version')
        haz_paquete(salida, op['--nombre'], op['--version'], op['--descripcion'], op['--necesita'], op['--choca'], archivos)
        print('hpq: %s (%s %s, %d archivos, %d octetos)' % (salida, op['--nombre'], op['--version'], len(archivos), os.path.getsize(salida)))
        return 0
    if orden == 'catalogo':
        n = catalogo(a[2])
        print('hpq: %s con %d paquetes' % (os.path.join(a[2], 'CATALOGO.TXT'), n))
        if len(a) > 4 and a[3] == '--clave':
            firma(a[2], a[4])
            print('hpq: %s, firmado' % os.path.join(a[2], 'CATALOGO.FIR'))
        return 0
    if orden == 'clave':
        pub = haz_clave(a[2])
        print('hpq: clave privada en %s (no la enseñe ni la suba); la pública: %s' % (a[2], pub.hex()))
        return 0
    if orden == 'publica':
        pub = publica_de(lee_privada(a[2]))
        if len(a) > 3:
            with open(a[3], 'w', encoding='utf-8') as f:
                f.write(modulo_clave(pub))
        print(pub.hex())
        return 0
    if orden == 'firma':
        firma(a[2], a[3])
        print('hpq: %s, firmado' % os.path.join(a[2], 'CATALOGO.FIR'))
        return 0
    if orden == 'comprueba':
        bien = comprueba_firma(a[2], a[3])
        print('hpq: la firma de %s es %s' % (os.path.join(a[2], 'CATALOGO.TXT'), 'buena' if bien else 'MALA'))
        return 0 if bien else 1
    if orden == 'mira':
        return mira(a[2])
    if orden == 'registro':
        _, _, largo, datos = lee_paquete(a[2])
        with open(a[3], 'wb') as f:
            f.write(datos[:largo])
        return 0
    if orden == 'opcionales':
        n = opcionales(a[2], a[3:])
        print('hpq: %d paquetes opcionales en %s' % (n, a[2]))
        return 0
    if orden == 'linea':
        sys.stdout.buffer.write(linea_catalogo(a[2]) + b'\r\n')
        return 0
    if orden == 'sistema':
        n = sistema(a[2], a[3:])
        print('hpq: %d paquetes del sistema en %s (con CATALOGO.TXT y sus registros en PQI)' % (n, a[2]))
        return 0
    falla('no sé qué es «%s»: crea, catalogo, mira, registro, sistema, opcionales, linea, clave, publica, firma o comprueba' % orden)


if __name__ == '__main__':
    sys.exit(principal(sys.argv))
