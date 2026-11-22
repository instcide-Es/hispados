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
#        tipo=oficina                   (PROGRAMAS: de qué tipo es; los de TIPOS)
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
#        tipo=<nombre> <tipo>           (PROGRAMAS: una por paquete; un PAQUETES
#                                        de antes no la mira, y la línea de cada
#                                        paquete sigue como era)
#        sistema=<nombre>               (0.8.6: es del sistema; PAQUETES ACTUALIZA
#                                        lo instala si no está, salvo si se ha
#                                        quitado a propósito: QUITADOS.TXT)
#
#  (PROGRAMAS) El tipo: uno de los códigos de TIPOS (sistema, oficina,
#  internet...: docs/PENTIUM.md, sección 13, «El tipo de cada paquete»). Si
#  la cabecera no lo trae, se deduce del nombre (TIPO_POR_NOMBRE) o es
#  «otros». En el catálogo puede ir aunque el paquete no lo lleve dentro (así
#  no hay que rehacer un paquete que no cambia: clicgest).
#
#  Lo instalado, en C:\HISPA\PAQUETES\<NOMBRE>.PQI: la cabecera del paquete,
#  tal cual (con «registro» se saca de un .HPQ: así lo apunta INSTALAR).
#
#  Uso:
#    hpq.py crea SALIDA.HPQ --nombre N --version V [--descripcion D]
#               [--tipo T] [--necesita "a >= 1, b"] [--choca "c"] ORIGEN=\DESTINO ...
#    hpq.py catalogo CARPETA          (CATALOGO.TXT con los .HPQ de la carpeta)
#    hpq.py mira PAQUETE.HPQ          (lo que lleva, y comprueba sus sumas)
#    hpq.py registro PAQUETE.HPQ SALIDA.PQI
#    hpq.py sistema SALIDA CARPETA...  (los paquetes del sistema, con sus
#               registros .PQI, el catálogo y CARPETAS.TXT para INSTALAR, de
#               los archivos ya hechos que haya en esas carpetas: usb.sh)
#    hpq.py opcionales SALIDA CARPETA...  (los que no van con el sistema:
#               «navega», el Navegador, y «escudo», el antivirus; de lo que
#               deja su hacer.sh)
#    hpq.py linea PAQUETE.HPQ         (sus líneas para CATALOGO.TXT: la del
#               paquete y la de su tipo)
#    hpq.py tipos                     (PROGRAMAS: la tabla de los tipos)
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

# (PROGRAMAS) los tipos de los paquetes: el código (lo que va en «tipo=») y
# cómo se llama (la carpeta del menú Inicio, el apartado de «Programas
# instalados»). En este orden salen. «otros», si no se sabe.
TIPOS = [
    ('sistema', 'Sistema'),
    ('oficina', 'Oficina'),
    ('internet', 'Internet y red'),
    ('juegos', 'Juegos y emuladores'),
    ('multimedia', 'Multimedia'),
    ('seguridad', 'Seguridad'),
    ('windows', 'Programas de Windows'),
    ('herramientas', 'Herramientas'),
    ('desarrollo', 'Desarrollo'),
    ('terceros', 'Programas de terceros'),
    ('otros', 'Otros'),
]
# el tipo de los que no lo dicen (los paquetes hechos antes de la 0.8.5 y
# los de otros repositorios): lo mismo que sabe PAQUETES (upaquete.cas, tipo_por_nombre)
TIPO_POR_NOMBRE = {
    'base': 'sistema', 'paquetes': 'sistema', 'equipo': 'sistema', 'hvprun': 'sistema',
    'red': 'internet', 'tls': 'internet', 'navega': 'internet', 'ssh': 'internet',
    'tailscal': 'internet', 'wifi': 'internet',
    'bloc': 'oficina', 'oficina': 'oficina', 'pdf': 'oficina',
    'consola': 'herramientas', 'accesori': 'herramientas', 'comprim': 'herramientas',
    'ejemplos': 'desarrollo',
    'win32': 'windows',
    'escudo': 'seguridad', 'escbase': 'seguridad', 'escmes': 'seguridad', 'escdia': 'seguridad',
    'sonido': 'multimedia', 'mirador': 'multimedia', 'reproduc': 'multimedia',     # (REPRODUCTOR)
    'clicgest': 'terceros',
}


def tipo_valido(t):
    """El código de un tipo (también vale su nombre: «Internet y red»)."""
    t = (t or '').strip().lower()
    for c, n in TIPOS:
        if t == c or t == n.lower():
            return c
    falla('el tipo «%s» no vale: %s' % (t, ', '.join(c for c, _ in TIPOS)))


def tipo_de(nombre, campos=None):
    """El tipo de un paquete: el de su cabecera, o el de su nombre, u «otros»."""
    if campos and campos.get('tipo'):
        return tipo_valido(campos['tipo'])
    return TIPO_POR_NOMBRE.get(nombre, 'otros')

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
# salen ssh y tailscal; y, por la revisión de seguridad, paquetes 1.1 (la
# firma con S < L, los destinos) y tls 0.8.4 (TLS.HOR) —Tailscale: los nombres de
# paquete son de 8 letras, por el registro <NOMBRE>.PQI—)
VERSION_084 = '0.8.4'
# [WIN32] (21/11/2026) los programas de Windows: en el núcleo (los
# segmentos de Win32, las llamadas 180 a 182; el coprocesador de cada tarea
# es el de la 0.8.3), ORDENES (los .EXE) y Mi equipo (los abre); y el
# paquete win32 (1.0, que pide la base 0.8.4)
# (RUEDA) (21/11/2026) la rueda del ratón en los programas: el Bloc de notas
# (desplaza el texto sin mover el cursor), la Consola (el historial de lo que
# sube: rueda y Mayús+Re Pág) y Programas y actualizaciones (su lista) suben
# a la 0.8.4 / 1.1 (paquetes: con la revisión de seguridad). (Mi equipo y el Administrador de dispositivos —equipo
# 0.9— y MUESTRA —ejemplos 0.8.4— también la llevan: van en la 0.8.4, que aún
# no se ha publicado.) (MENÚ) el menú Inicio nuevo, los menús largos y los
# atajos de todo el sistema van en el núcleo (base 0.8.4, sin publicar).
VERSION_RUEDA = VERSION_084
# (WIFI) (21/11/2026) el Wi-Fi: el núcleo con el servicio Wi-Fi (la llamada
# 100, el icono con rayitas, Configuración > Red), RED.BIN con la tarjeta
# Wi-Fi (rwifi.cas) y el paquete wifi: WIFI.BIN, REDWIFI.BIN y el microcódigo
# de Broadcom con su licencia (aparte, sin tocar). Suben base y red
# (0.8.5, 21/11/2026, pentium-085: el menú Inicio, la rueda, «Programas
# instalados» (paquetes 1.2), la letra suavizada, la zona protegida, Escudo
# y el cortafuegos, el Wi-Fi y HispaGL (win32 1.1))
VERSION_085 = '0.8.5'
VERSION_WIFI = VERSION_085
# [GL] (21/11/2026) win32 1.1: con OpenGL (HispaGL: OPENGL32.DLL y GLU32.DLL,
# GLPRUEBA y ENGRANA; docs/486/OPENGL.md)
# (SONIDO, 21/11/2026) el sonido: en el núcleo (las llamadas 190 a 199,
# sonido.cas; el altavoz de la bandeja y Configuración > Sonido), y el
# paquete sonido (1.0): el servicio, SONIDO.BIN, y los sonidos del sistema;
# suben win32 (1.1: DirectSound y WinMM suenan) y hvprun (0.9: el sonido de
# los programas de Hispa Ventanas). Pide la base con las llamadas del sonido
# (la que salga con esta rama; ver docs/PENTIUM.md, «El sonido»)
VERSION_SONIDO = VERSION_085
# (RÁPIDO, 22/11/2026, escudo-rapido) la 0.8.6: fat.cas lee y escribe los
# sectores seguidos de una vez (antes, una orden al disco por sector), la
# memoria USB va de 16 K en 16 K (antes 4 K), PAQUETES 1.3 (el avance dentro
# de un paquete grande, «Cancelar», mira si cabe antes de bajar y borra lo
# bajado) y DESCARGAR /CANCELA=. Suben base, red y paquetes
VERSION_086 = '0.8.6'
SISTEMA = [
    ('base', VERSION_086, 'El sistema base: el núcleo, el intérprete de órdenes, el asistente, INSTALAR y el arranque', '', [
        ('ESCRIT8.BIN', r'\HISPA\NUCLEO.BIN'), ('ORDENES.BIN', r'\HISPA\ORDENES.BIN'),
        ('ASISTE.BIN', r'\HISPA\ASISTE.BIN'), ('INSTALAR.BIN', r'\HISPA\INSTALAR.BIN'), ('ACPI.BIN', r'\HISPA\ACPI.BIN'),
        ('LEEME.TXT', r'\HISPA\LEEME.TXT'), ('LEEME.TXT', r'\LEEME.TXT'), ('MANUAL.TXT', r'\HISPA\MANUAL.TXT'),
        ('BOOT.IMG', r'\HISPA\BOOT.IMG'), ('CORE.IMG', r'\HISPA\CORE.IMG')]),
    ('bloc', VERSION_085, 'El Bloc de notas', 'base >= 0.7', [('BLOC.BIN', r'\HISPA\BLOC.BIN')]),
    ('consola', VERSION_085, 'La Consola: el intérprete de órdenes en una ventana', 'base >= 0.7', [('CONSOLA.BIN', r'\HISPA\CONSOLA.BIN')]),
    # (explorador, dispositivos) el explorador de archivos, sus miniaturas y el Administrador de dispositivos
    ('equipo', '0.9.2', 'Mi equipo, el explorador de archivos (carpetas, vistas, copiar y pegar, la Papelera, buscar y las miniaturas) y el Administrador de dispositivos', 'base >= 0.8.4', [
        ('EQUIPO.BIN', r'\HISPA\EQUIPO.BIN'), ('MINIAT.BIN', r'\HISPA\MINIAT.BIN'), ('DISPOSIT.BIN', r'\HISPA\DISPOSIT.BIN')]),
    ('ejemplos', VERSION_085, 'Programas de ejemplo: HOLA, ECO, PRUEBA, BUCLE y MUESTRA (los controles de las ventanas)', 'base >= 0.8.4', [
        ('HOLA.BIN', r'\HISPA\HOLA.BIN'), ('ECO.BIN', r'\HISPA\ECO.BIN'),
        ('PRUEBA.BIN', r'\HISPA\PRUEBA.BIN'), ('BUCLE.BIN', r'\HISPA\BUCLE.BIN'), ('MUESTRA.BIN', r'\HISPA\MUESTRA.BIN')]),
    # (ACCESORIOS, 21/11/2026) la Calculadora, el Reloj y el Mapa de caracteres,
    # con sus accesos en Accesorios (tipo herramientas: con el hpq.py de la
    # 0.8.5, «--tipo herramientas»; su .MNU dice «tipo=Accesorios»)
    ('accesori', '1.0', 'Accesorios: la Calculadora (científica y de oficina: euros, IVA y cinta), el Reloj (calendario, alarma, cronómetro y cuenta atrás) y el Mapa de caracteres', 'base >= 0.8.4', [
        ('CALC.BIN', r'\HISPA\CALC.BIN'), ('RELOJ.BIN', r'\HISPA\RELOJ.BIN'), ('CARACTER.BIN', r'\HISPA\CARACTER.BIN'),
        ('CALC.MNU', r'\HISPA\MENU\CALC.MNU'), ('RELOJ.MNU', r'\HISPA\MENU\RELOJ.MNU'), ('CARACTER.MNU', r'\HISPA\MENU\CARACTER.MNU'),
        ('CALC.ICP', r'\HISPA\MENU\CALC.ICP'), ('RELOJ.ICP', r'\HISPA\MENU\RELOJ.ICP'), ('CARACT.ICP', r'\HISPA\MENU\CARACT.ICP')]),
    # (PROGRAMAS) la 1.2 (sobre la 1.1, la de la revisión de seguridad): los tipos,
    # REPARA, ARCHIVOS, SUELTOS, QUITA /DEPENDIENTES y la ventana unificada
    # «Programas y actualizaciones» (PROGRAMA.BIN, con los controles: base 0.8.2)
    ('paquetes', '1.3', 'Programas y actualizaciones: PAQUETES y su ventana (pendientes, al día, instaladas y sin instalar, por tipos)', 'base >= 0.8.2, nucleo >= 0.7', [
        ('PAQUETES.BIN', r'\HISPA\PAQUETES.BIN'), ('ACTUALIZ.BIN', r'\HISPA\ACTUALIZ.BIN'),
        ('PROGRAMA.BIN', r'\HISPA\PROGRAMA.BIN')]),
    ('red', VERSION_086, 'La red: el servicio de red (RED.BIN), IPCONFIG, PING y DESCARGAR', 'base >= ' + VERSION_085, [
        ('RED.BIN', r'\HISPA\RED.BIN'), ('IPCONFIG.BIN', r'\HISPA\IPCONFIG.BIN'),
        ('PING.BIN', r'\HISPA\PING.BIN'), ('DESCARGA.BIN', r'\HISPA\DESCARGA.BIN')]),
    ('tls', VERSION_085, 'HTTPS: el servicio TLS (TLS.BIN, con BearSSL) y la orden HTTPS', 'base >= 0.8.1, red >= 0.8.1', [
        ('TLS.BIN', r'\HISPA\TLS.BIN'), ('HTTPS.BIN', r'\HISPA\HTTPS.BIN')]),
    ('ssh', VERSION_084, 'SSH: entrar en otros equipos (SSH) y en este desde otros (SSHD, apagado hasta que se encienda)', 'base >= 0.8.4, red >= 0.8.4', [
        ('SSH.BIN', r'\HISPA\SSH.BIN'), ('SSHD.BIN', r'\HISPA\SSHD.BIN')]),
    ('tailscal', VERSION_084, 'Tailscale y WireGuard: la red privada de sus equipos, cada uno con su nombre y su IP 100.x', 'base >= 0.8.4, red >= 0.8.4, tls >= 0.8.1', [
        ('TAILSCAL.BIN', r'\HISPA\TAILSCAL.BIN'), ('WG.BIN', r'\HISPA\WG.BIN')]),
    # (WIFI) el Wi-Fi (castel/nucleo32/wifi/, docs/PENTIUM.md «El Wi-Fi»); el microcódigo, con su licencia
    ('wifi', VERSION_WIFI, 'El Wi-Fi: el servicio Wi-Fi (WIFI.BIN: Broadcom BCM43224/43225 y Atheros AR9285), su ventana (REDWIFI) y el microcódigo de Broadcom',
     'base >= ' + VERSION_WIFI + ', red >= ' + VERSION_WIFI, [
        ('WIFI.BIN', r'\HISPA\WIFI.BIN'), ('REDWIFI.BIN', r'\HISPA\REDWIFI.BIN'), ('WIFI.TXT', r'\HISPA\WIFI.TXT'),
        ('BCM43XX.FW', r'\HISPA\BCM43XX.FW'), ('BCM43XXH.FW', r'\HISPA\BCM43XXH.FW'), ('BCM43XX.TXT', r'\HISPA\BCM43XX.TXT')]),
    ('sonido', '1.0', 'El sonido: el servicio de sonido (HD Audio, AC\'97 y Sound Blaster 16; SONIDO.BIN y la orden SONIDO) y los sonidos del sistema',
     'base >= ' + VERSION_SONIDO, [
        ('SONIDO.BIN', r'\HISPA\SONIDO.BIN'), ('INICIO.WAV', r'\HISPA\INICIO.WAV'), ('APAGAR.WAV', r'\HISPA\APAGAR.WAV'),
        ('AVISO.WAV', r'\HISPA\AVISO.WAV'), ('ERROR.WAV', r'\HISPA\ERROR.WAV')]),
    ('hvprun', '0.9', 'HVPRUN: los programas de Hispa Ventanas dentro del escritorio (con su sonido)', 'base >= 0.7', [
        ('HVPRUN.BIN', r'\HISPA\HVPRUN.BIN')]),
    ('oficina', '1.0', 'La oficina de Hispa-386: Facturas, Pluma, Cuentas y Mirador', 'hvprun >= 0.7', [
        ('FACTURAS.HVP', r'\OFICINA\FACTURAS.HVP'), ('PLUMA.HVP', r'\OFICINA\PLUMA.HVP'),
        ('CUENTAS.HVP', r'\OFICINA\CUENTAS.HVP'), ('MIRADOR.HVP', r'\OFICINA\MIRADOR.HVP'),
        ('TIPOS.DAT', r'\OFICINA\TIPOS.DAT'), ('DICCIO.DAT', r'\OFICINA\DICCIO.DAT')]),
    # [WIN32] los programas de Windows de 32 bits y DirectX (docs/pentium/WIN32.md)
    ('win32', '1.1', 'Programas de Windows de 32 bits, DirectX (con sonido) y OpenGL: WIN32.BIN, sus DLL y los ejemplos (Naves, GLPRUEBA)', 'base >= ' + VERSION_084, [
        ('WIN32.BIN', r'\HISPA\WIN32.BIN'),
        ('MSVCRT.DLL', r'\WIN32\MSVCRT.DLL'), ('UCRT.DLL', r'\WIN32\UCRT.DLL'), ('COMCTL32.DLL', r'\WIN32\COMCTL32.DLL'),
        ('WINMM.DLL', r'\WIN32\WINMM.DLL'), ('DDRAW.DLL', r'\WIN32\DDRAW.DLL'), ('DSOUND.DLL', r'\WIN32\DSOUND.DLL'),
        ('DINPUT.DLL', r'\WIN32\DINPUT.DLL'), ('OLE32.DLL', r'\WIN32\OLE32.DLL'),
        ('OPENGL32.DLL', r'\WIN32\OPENGL32.DLL'), ('GLU32.DLL', r'\WIN32\GLU32.DLL'),   # [GL]
        ('CALC32.EXE', r'\HISPA32\CALC32.EXE'), ('HOLA32.EXE', r'\HISPA32\HOLA32.EXE'), ('CTL32.EXE', r'\HISPA32\CTL32.EXE'),
        ('MENU32.EXE', r'\HISPA32\MENU32.EXE'), ('AVISO32.EXE', r'\HISPA32\AVISO32.EXE'), ('NAVES.EXE', r'\HISPA32\NAVES.EXE'),
        ('GLPRUEBA.EXE', r'\HISPA32\GLPRUEBA.EXE'), ('ENGRANA.EXE', r'\HISPA32\ENGRANA.EXE'),   # [GL]
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


def cabecera(nombre, version, descripcion, necesita, choca, archivos, tipo=''):
    """archivos: [(destino, datos)]. Da la cabecera (en octetos)."""
    if not NOMBRE.match(nombre):
        falla('el nombre «%s» no vale (hasta 8 letras: a-z, 0-9, - y _)' % nombre)
    if not VERSION.match(version):
        falla('la versión «%s» no vale (números separados por puntos)' % version)
    if len(archivos) > MAX_ARCHIVOS:
        falla('demasiados archivos en un paquete (hasta %d)' % MAX_ARCHIVOS)
    vistos = set()
    lineas = [MAGIA, 'nombre=' + nombre, 'version=' + version,
              'descripcion=' + descripcion.replace('\n', ' ')[:200]]
    if tipo:
        lineas.append('tipo=' + tipo_valido(tipo))    # (PROGRAMAS)
    lineas += ['necesita=' + comprueba_lista(necesita, 'necesita'),
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


def haz_paquete(salida, nombre, version, descripcion, necesita, choca, archivos, tipo=''):
    c = cabecera(nombre, version, descripcion, necesita, choca, archivos, tipo)
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
    print('  tipo:     %s' % tipo_de(campos.get('nombre'), campos))
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
    tipos = {}
    for n in sorted(os.listdir(carpeta)):
        if not n.upper().endswith('.HPQ'):
            continue
        ruta = os.path.join(carpeta, n)
        campos, _, _, datos = lee_paquete(ruta)
        if n != n.upper() or not TROZO_83.match(n):
            falla('el paquete %s tiene que llamarse en 8+3 y en mayúsculas' % n)
        filas.append('paquete=%s %s %s %d %s %s' % (campos['nombre'], campos['version'], n, len(datos), sha(datos),
                                                    campos.get('descripcion', '')))
        if tipo_de(campos['nombre'], campos) != 'otros':
            tipos[campos['nombre']] = tipo_de(campos['nombre'], campos)
    # (PROGRAMAS) y el tipo de cada uno, aparte (un PAQUETES de antes no lo mira;
    # «otros», el de no saberlo, no se escribe)
    filas += ['tipo=%s %s' % (k, tipos[k]) for k in sorted(tipos)]
    # (0.8.6) los que son del sistema: un sistema que se pone al día
    # (PAQUETES ACTUALIZA) los instala también si no los tiene (los que salen
    # en una versión nueva: accesori, sonido, wifi, escudo...), salvo los que
    # se han quitado a propósito (QUITADOS.TXT); un PAQUETES de antes no mira
    # estas líneas
    hay = {x.split()[0][len('paquete='):] for x in filas if x.startswith('paquete=')}
    filas += ['sistema=%s' % n for n in sorted(DEL_SISTEMA) if n in hay]
    with open(os.path.join(carpeta, 'CATALOGO.TXT'), 'wb') as f:
        f.write(a437(MAGIA_CATALOGO) + b'\r\n')
        for x in filas:
            f.write(a437(x) + b'\r\n')
    return len([x for x in filas if x.startswith('paquete=')])


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
    ('navega', '0.2.1', 'El Navegador: la web de hoy (NetSurf con JavaScript), con pestañas y descargas',
     'base >= 0.8.3, red >= 0.8.1, tls >= 0.8.1', [
        ('NAVEGA/NAVEGA.BIN', r'\HISPA\NAVEGA\NAVEGA.BIN'), ('NAVEGA/NAVEGA.INI', r'\HISPA\NAVEGA\NAVEGA.INI'),
        ('NAVEGA/LEEME.TXT', r'\HISPA\NAVEGA\LEEME.TXT')]
     + [('NAVEGA/RECURSOS/' + n, '\\HISPA\\NAVEGA\\RECURSOS\\' + n) for n in NAVEGA_RECURSOS]
     + [('NAVEGA/FUENTES/' + n, '\\HISPA\\NAVEGA\\FUENTES\\' + n) for n in NAVEGA_FUENTES]
     + [('NAVEGA.MNU', r'\HISPA\MENU\NAVEGA.MNU'), ('NAVEGA.ICP', r'\HISPA\MENU\NAVEGA.ICP')]),
    # (ESCUDO) el antivirus (castel/nucleo32/escudo/paquete.sh). Sus firmas, aparte
    # (los paquetes «escbase», «escmes» y «escdia», que hace tools/escudo_firmas.py)
    ('escudo', '0.2.1', 'Escudo: el antivirus (con las firmas de ClamAV), para el equipo y para revisar memorias USB',
     'base >= 0.8.3, red >= 0.8.1, tls >= 0.8.1', [
        ('ESCUDO.BIN', r'\HISPA\ESCUDO.BIN'), ('ESCUDO/LEEME.TXT', r'\HISPA\ESCUDO\LEEME.TXT'),
        ('MENU/ESCUDO.MNU', r'\HISPA\MENU\ESCUDO.MNU'), ('MENU/ESCUDO.ICP', r'\HISPA\MENU\ESCUDO.ICP')]),
    # (PDF) Hispa-PDF (castel/nucleo32/pdf/paquete.sh): la orden PDF y su
    # ventana; del tipo «oficina» (en la 0.8.5: TIPOS de hpq.py y tipo_de)
    ('pdf', '1.0', 'Hispa-PDF: unir, separar, girar, ordenar, comprimir y cifrar PDF; PDF a imágenes y a texto; imágenes a PDF',
     'base >= 0.8.3', [
        ('PDF.BIN', r'\HISPA\PDF.BIN'), ('HISPAPDF.BIN', r'\HISPA\HISPAPDF.BIN'), ('PDF/LEEME.TXT', r'\HISPA\PDF\LEEME.TXT'),
        ('MENU/HISPAPDF.MNU', r'\HISPA\MENU\HISPAPDF.MNU'), ('MENU/HISPAPDF.ICP', r'\HISPA\MENU\HISPAPDF.ICP')]),
    # (ZIP) Comprimidos y las órdenes ZIP y UNZIP (castel/nucleo32/comprim/paquete.sh; tipo «herramientas»)
    ('comprim', '1.0', 'Comprimidos: los .ZIP como una carpeta (sacar, añadir y quitar, con contraseña) y las órdenes ZIP y UNZIP', 'base >= 0.8.4', [
        ('BIN/COMPRIM.BIN', r'\HISPA\COMPRIM.BIN'), ('BIN/ZIP.BIN', r'\HISPA\ZIP.BIN'), ('BIN/UNZIP.BIN', r'\HISPA\UNZIP.BIN'),
        ('COMPRIM.TXT', r'\HISPA\COMPRIM.TXT'), ('COMPRIM.MNU', r'\HISPA\MENU\COMPRIM.MNU'), ('COMPRIM.ICP', r'\HISPA\MENU\COMPRIM.ICP')]),
    # (MIRADOR) el Mirador, el visor de imágenes (castel/nucleo32/mirador/paquete.sh; tipo «multimedia»)
    ('mirador', '1.0', 'El Mirador: ver imágenes (BMP, PNG, JPG, GIF animados, ICO...), girar, diapositivas, pantalla completa y fondo de escritorio', 'base >= 0.8.6', [
        ('BIN/MIRADOR.BIN', r'\HISPA\MIRADOR.BIN'), ('MIRADOR.TXT', r'\HISPA\MIRADOR.TXT'),
        ('MIRADOR.MNU', r'\HISPA\MENU\MIRADOR.MNU'), ('MIRADOR.ICP', r'\HISPA\MENU\MIRADOR.ICP')]),
    # (REPRODUCTOR) el Reproductor de música (castel/nucleo32/reproductor/paquete.sh; tipo «multimedia»)
    ('reproduc', '1.0', 'El Reproductor: música (MP3, OGG, FLAC, WAV, MOD, XM, S3M, IT) y CD de música, con lista, ecualizador de 10 bandas y visualización',
     'base >= 0.8.6, sonido >= 1.0', [
        ('BIN/REPRODUC.BIN', r'\HISPA\REPRODUC.BIN'), ('REPRODUC.TXT', r'\HISPA\REPRODUC.TXT'),
        ('REPRODUC.MNU', r'\HISPA\MENU\REPRODUC.MNU'), ('REPRODUC.ICP', r'\HISPA\MENU\REPRODUC.ICP')]),
]

# (0.8.6) los que el catálogo marca del sistema («sistema=nombre»): los que
# van con él (SISTEMA) y Escudo, el antivirus (no sus firmas: las trae
# Escudo con «Actualizar las firmas»). No: los opcionales (navega, pdf,
# comprim, mirador, clicgest...)
DEL_SISTEMA = [n for n, *_ in SISTEMA] + ['escudo']


def opcionales(salida, carpetas, nombres=None):
    """Los paquetes de OPCIONALES (o los de «nombres») en SALIDA: da cuántos."""
    os.makedirs(salida, exist_ok=True)
    n = 0
    for nombre, version, descripcion, necesita, archivos in OPCIONALES:
        if nombres and nombre not in nombres:
            continue
        # (ESCUDO) los que no tienen ninguno de sus archivos en esas carpetas, no
        # se hacen (cada paquete.sh deja solo lo suyo); a medias, es un error
        if not nombres and not any(os.path.isfile(os.path.join(c, o)) for o, _ in archivos for c in carpetas):
            continue
        lista = [(d, open(busca(o, carpetas), 'rb').read()) for o, d in archivos]
        haz_paquete(os.path.join(salida, nombre.upper() + '.HPQ'), nombre, version, descripcion, necesita, '', lista,
                    tipo_de(nombre))
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
            # (y la de su tipo: PROGRAMAS)
            return b'\r\n'.join(x for x in f.read().split(b'\r\n')[1:3] if x)
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
        c = haz_paquete(os.path.join(salida, N + '.HPQ'), nombre, version, descripcion, necesita, '', lista,
                        tipo_de(nombre))
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
        op = {'--nombre': None, '--version': None, '--descripcion': '', '--necesita': '', '--choca': '', '--tipo': ''}
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
        haz_paquete(salida, op['--nombre'], op['--version'], op['--descripcion'], op['--necesita'], op['--choca'], archivos,
                    op['--tipo'])
        print('hpq: %s (%s %s, %d archivos, %d octetos)' % (salida, op['--nombre'], op['--version'], len(archivos), os.path.getsize(salida)))
        return 0
    if orden == 'catalogo':
        if len(a) > 4 and a[3] == '--clave':
            # Antes de reescribir CATALOGO.TXT: que se pueda firmar (módulo y
            # clave). Si no, un catálogo publicado se quedaba con la firma MALA
            # (22/11/2026: el cron de Escudo con el Python de cvdupdate).
            lee_privada(a[4])
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
        # (PDF) «opcionales SALIDA CARPETA nombre...»: solo esos paquetes
        cs = [x for x in a[3:] if os.path.isdir(x)]
        ns = [x for x in a[3:] if not os.path.isdir(x)]
        n = opcionales(a[2], cs, ns or None)
        print('hpq: %d paquetes opcionales en %s' % (n, a[2]))
        return 0
    if orden == 'linea':
        sys.stdout.buffer.write(linea_catalogo(a[2]) + b'\r\n')
        return 0
    if orden == 'tipos':
        for c, n in TIPOS:
            print('%-13s %s' % (c, n))
        return 0
    if orden == 'sistema':
        n = sistema(a[2], a[3:])
        print('hpq: %d paquetes del sistema en %s (con CATALOGO.TXT y sus registros en PQI)' % (n, a[2]))
        return 0
    falla('no sé qué es «%s»: crea, catalogo, mira, registro, sistema, opcionales, linea, clave, publica, firma o comprueba' % orden)


if __name__ == '__main__':
    sys.exit(principal(sys.argv))
