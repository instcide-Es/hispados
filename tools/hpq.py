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
#        serie=<N>                      (USUARIOS, 22/11/2026: la pone «firma», detrás
#                                        de la primera línea, y la sube cada vez:
#                                        PAQUETES no usa un catálogo de Internet con
#                                        la serie más baja que la última que vio,
#                                        contra la vuelta atrás a versiones viejas)
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
#    hpq.py firma CARPETA PRIVADA [--serie N]   (CATALOGO.FIR del CATALOGO.TXT de
#               ahí; antes le pone «serie=N»: la de ahora más uno, y como poco
#               los segundos desde 1970, así sube aunque el catálogo se rehaga;
#               --serie, para las pruebas)
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
import time

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
    'tailscal': 'internet', 'wifi': 'internet', 'correo': 'internet',  # (CORREO)
    'bloc': 'oficina', 'oficina': 'oficina', 'pdf': 'oficina', 'pluma': 'oficina', 'agenda': 'oficina',   # (PLUMA32: pluma) (AGENDA)
    'cuentas': 'oficina',   # (CUENTAS32)
    'consola': 'herramientas', 'accesori': 'herramientas', 'comprim': 'herramientas', 'termo': 'herramientas', 'compat': 'herramientas',   # (TERMO) termo (COMPAT)
    'ejemplos': 'desarrollo',
    'win32': 'windows',
    'escudo': 'seguridad', 'escbase': 'seguridad', 'escmes': 'seguridad', 'escdia': 'seguridad',
    'sonido': 'multimedia', 'mirador': 'multimedia', 'reproduc': 'multimedia', 'pintura': 'multimedia',     # (REPRODUCTOR) reproduc, (OFICINA2) pintura
    'juegos': 'juegos',     # (JUEGOS2)
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
# (USUARIOS, 22/11/2026, rama usuarios) la 0.8.7: los usuarios con contraseña
# (la pantalla de entrada, \USUARIOS, Configuración > Usuarios: USUARIOS.BIN;
# las contraseñas, con Argon2id: ARGON2.BIN) y el pozo del azar del núcleo
# (las llamadas 200 a 210) en la base; SSHD con los usuarios (ssh); el azar del
# núcleo en TLS.BIN (tls), WIFI.BIN (wifi), SSH, WG y TAILSCAL (hispac/hispa.c:
# ssh y tailscal); PAQUETES 1.4 (la serie del catálogo, solo el administrador instala)
# (PANEL, 22/11/2026, rama panel-control) la 0.8.7: el Panel de control
# (Configuración con sus iconos; PANEL.BIN: la fecha y la hora, el ratón, el
# teclado, la red, la energía y la personalización; las llamadas 174 y 175,
# PANEL.INI) y la hora por Internet (HORARED.BIN, NTP); ACPI.BIN con el botón
# de encendido. Suben base y tls (TLS.BIN no se fía de un reloj de antes de TLS.HOR)
VERSION_087 = '0.8.7'
# (0.8.8, 22/11/2026, pentium-088: la Agenda —el vigilante de los avisos y
# la próxima cita en el reloj de la bandeja, de cada usuario—, Cuentas 2 y
# Pintura 2 —la 2.0 del sistema—: en el núcleo, el escritorio, Mi equipo
# (exoper) y el menú Inicio). Sube la base
VERSION_088 = '0.8.8'
# (CORTES, 22/11/2026, rama red-cortes) los cortes de la red (docs/PENTIUM.md,
# 14 tervicies): TCP con tope y keepalive, la red que cambia (DHCP), DESCARGAR que
# sigue donde iba (Range), «Cancelar» al momento y «Forzar el cierre» al
# apagar. Suben base (el núcleo: el cuadro de apagar y la llamada 11 con ECX =
# 2), red, tls (HTTPS reintenta), wifi (REDWIFI /SIMCORTE) y paquetes (1.5)
VERSION_CORTES = '0.8.8'
# (0.8.9, 23/11/2026, pentium-089: el arranque UEFI de 64 y 32 bits, el
# Termómetro —las llamadas 220 a 225—, el Asistente de compatibilidad, el
# touchpad y los punteros). UNA sola versión de la base: esta; las de abajo
# (TOUCHPAD y PUNTEROS) son la misma. Con Usuario.VERSIÓN_TEXTO («0.8.9»)
VERSION_089 = '0.8.9'
# (TOUCHPAD, 22/11/2026, rama touchpad) el touchpad: Synaptics en modo
# absoluto (touchpad.cas: el borde derecho para desplazar, tocar para hacer
# clic, dos dedos, la palma, al escribir), ALPS y Elantech dichos, Panel de
# control > Ratón > Touchpad (las claves 23 a 31 de la 174) y RATON.BIN
# (RATON /TOUCHPAD graba lo que manda). Sube la base
VERSION_TOUCHPAD = VERSION_089
# (PUNTEROS, 22/11/2026, rama punteros) los punteros del ratón: en el núcleo
# las 13 formas (cursores.cas), la llamada 177 y PUNTERO.INI; en el Panel de
# control, Ratón > Puntero (PANEL.BIN). La base sube al juntarla (0.8.9: la
# pone quien coordina); con ella, los programas que piden su puntero: bloc
# 1.0.1, pintura 2.0.1 (sin la flecha doble dibujada: pide esta base),
# pluma 2.0.2, correo 1.0.3 (la 1.0.2 ya estaba publicada) y navega 0.2.3 (con una base de antes, la
# llamada 177 no hace nada y siguen yendo)
VERSION_PUNTEROS = VERSION_089
# (0.8.10, 24/11/2026, pentium-0810: el Termómetro del taller —la batería que
# se relee, SMI_COUNT, Machine Check, las zonas PCI de 64 bits movidas debajo
# de los 4 GB, el traspaso del EHCI, sin8042—, los touchpads HID sobre I2C
# —la llamada 159—, la RTL8111/8168, el Wi-Fi de Intel 7265 (etapa 1) y el
# eMMC y las SD por SDHCI —la llamada 160, INSTALAR en ellas—). Con
# Usuario.VERSIÓN_TEXTO («0.8.10»). Suben base, red (rrtl.cas, RED.LOG),
# wifi (la 7265), paquetes 1.7 (el GRUB.CFG de UEFI) y termo 1.1
# (TERMO 1.1.1, 25/11/2026: el paquete termo, sin cambiar la base: varias
# pruebas en el mismo arranque, la pantalla y INFORME.TXT; termo/paquete.sh)
VERSION_0810 = '0.8.10'
# (0.8.11, 25/11/2026, rama sin-admin: el HP Mini de la asociación con solo
# «Biblio2», normal, y ningún administrador) la pantalla de entrada ofrece
# crear un administrador (o hacerlo a uno con contraseña) cuando no hay
# ninguno, en la pantalla de verdad; el último administrador no se quita ni
# se degrada; un usuario normal pone al día lo instalado con el catálogo
# firmado (la llamada 200, 6); lo que lanza ACTUALIZ /BANDEJA es del que ha
# entrado; y el diagnóstico del Wi-Fi en WIFI.LOG sin «Registro=SI». Suben
# base, paquetes 1.8, red (RED.BIN: «/OTRA» al relanzar WIFI.BIN) y wifi
VERSION_0811 = '0.8.11'
SISTEMA = [
    ('base', VERSION_0811, 'El sistema base: el núcleo, el intérprete de órdenes, los usuarios, el asistente, INSTALAR, el arranque, el Panel de control y la hora por Internet', '', [
        ('ESCRIT8.BIN', r'\HISPA\NUCLEO.BIN'), ('ORDENES.BIN', r'\HISPA\ORDENES.BIN'), ('RATON.BIN', r'\HISPA\RATON.BIN'),
        ('USUARIOS.BIN', r'\HISPA\USUARIOS.BIN'), ('ARGON2.BIN', r'\HISPA\ARGON2.BIN'),
        ('PANEL.BIN', r'\HISPA\PANEL.BIN'), ('HORARED.BIN', r'\HISPA\HORARED.BIN'),
        ('ASISTE.BIN', r'\HISPA\ASISTE.BIN'), ('INSTALAR.BIN', r'\HISPA\INSTALAR.BIN'), ('ACPI.BIN', r'\HISPA\ACPI.BIN'),
        ('LEEME.TXT', r'\HISPA\LEEME.TXT'), ('LEEME.TXT', r'\LEEME.TXT'), ('MANUAL.TXT', r'\HISPA\MANUAL.TXT'),
        ('BOOT.IMG', r'\HISPA\BOOT.IMG'), ('CORE.IMG', r'\HISPA\CORE.IMG')]),
    ('bloc', '1.0.1', 'El Bloc de notas: pestañas, buscar y reemplazar (con expresiones), colores según el lenguaje, UTF-8 y comparar', 'base >= ' + VERSION_087, [('BLOC.BIN', r'\HISPA\BLOC.BIN')]),    # (BLOC) la 1.0: el Bloc al nivel de un editor serio
    ('consola', VERSION_085, 'La Consola: el intérprete de órdenes en una ventana', 'base >= 0.7', [('CONSOLA.BIN', r'\HISPA\CONSOLA.BIN')]),
    # (explorador, dispositivos) el explorador de archivos, sus miniaturas y el Administrador de dispositivos
    # (REPRODUCTOR) la 0.9.3: la música (.MP3, .OGG, .FLAC, .WAV, .CUE...) se abre con el Reproductor si está
    # (0.8.8) la 0.9.4: una sola «Editar con Pintura» (Pintura 2 o, sin ella, la de la oficina)
    # (0.8.9) 0.9.5: (COMPAT) «Comprobar compatibilidad» en el menú de un .EXE o de una carpeta
    ('equipo', '0.9.5', 'Mi equipo, el explorador de archivos (carpetas, vistas, copiar y pegar, la Papelera, buscar y las miniaturas) y el Administrador de dispositivos', 'base >= 0.8.4', [
        ('EQUIPO.BIN', r'\HISPA\EQUIPO.BIN'), ('MINIAT.BIN', r'\HISPA\MINIAT.BIN'), ('DISPOSIT.BIN', r'\HISPA\DISPOSIT.BIN')]),
    ('ejemplos', VERSION_085, 'Programas de ejemplo: HOLA, ECO, PRUEBA, BUCLE y MUESTRA (los controles de las ventanas)', 'base >= 0.8.4', [
        ('HOLA.BIN', r'\HISPA\HOLA.BIN'), ('ECO.BIN', r'\HISPA\ECO.BIN'),
        ('PRUEBA.BIN', r'\HISPA\PRUEBA.BIN'), ('BUCLE.BIN', r'\HISPA\BUCLE.BIN'), ('MUESTRA.BIN', r'\HISPA\MUESTRA.BIN')]),
    # (ACCESORIOS, 21/11/2026) la Calculadora, el Reloj y el Mapa de caracteres,
    # con sus accesos en Accesorios (tipo herramientas: con el hpq.py de la
    # 0.8.5, «--tipo herramientas»; su .MNU dice «tipo=Accesorios»)
    # (AGENDA) la 1.1: el Reloj, con los festivos de festivos.cas (los de la
    # comunidad y los locales) y dos clics en un día abren la Agenda
    ('accesori', '1.1', 'Accesorios: la Calculadora (científica y de oficina: euros, IVA y cinta), el Reloj (calendario con festivos, alarma, cronómetro y cuenta atrás) y el Mapa de caracteres', 'base >= 0.8.4', [
        ('CALC.BIN', r'\HISPA\CALC.BIN'), ('RELOJ.BIN', r'\HISPA\RELOJ.BIN'), ('CARACTER.BIN', r'\HISPA\CARACTER.BIN'),
        ('CALC.MNU', r'\HISPA\MENU\CALC.MNU'), ('RELOJ.MNU', r'\HISPA\MENU\RELOJ.MNU'), ('CARACTER.MNU', r'\HISPA\MENU\CARACTER.MNU'),
        ('CALC.ICP', r'\HISPA\MENU\CALC.ICP'), ('RELOJ.ICP', r'\HISPA\MENU\RELOJ.ICP'), ('CARACT.ICP', r'\HISPA\MENU\CARACT.ICP')]),
    # (PROGRAMAS) la 1.2 (sobre la 1.1, la de la revisión de seguridad): los tipos,
    # REPARA, ARCHIVOS, SUELTOS, QUITA /DEPENDIENTES y la ventana unificada
    # «Programas y actualizaciones» (PROGRAMA.BIN, con los controles: base 0.8.2)
    # (0.8.9) 1.6: (COMPAT) los ganchos del Asistente al instalar o actualizar win32 (PAQUETES y ACTUALIZ /BANDEJA)
    # (0.8.10) 1.7: (SDHCI, UEFI) el «núcleo de antes» también en un GRUB.CFG con multiboot2 (lo instalado en SD/eMMC por UEFI)
    # (0.8.11) 1.8: (SIN-ADMIN) un usuario normal pone al día lo instalado con el catálogo firmado (sin /FORZAR)
    ('paquetes', '1.8', 'Programas y actualizaciones: PAQUETES y su ventana (pendientes, al día, instaladas y sin instalar, por tipos)', 'base >= 0.8.2, nucleo >= 0.7', [
        ('PAQUETES.BIN', r'\HISPA\PAQUETES.BIN'), ('ACTUALIZ.BIN', r'\HISPA\ACTUALIZ.BIN'),
        ('PROGRAMA.BIN', r'\HISPA\PROGRAMA.BIN')]),
    # (JUEGOS) la 0.8.7: UDP sin esperar (rpila.cas: un juego lo pregunta en cada cuadro)
    # (0.8.10) la RTL8111/8168 (rrtl.cas) y RED.LOG con Registro=SI
    # (0.8.11) RED.BIN: WIFI.BIN otra vez en el mismo arranque, con «/OTRA» (WIFI.LOG sigue)
    ('red', VERSION_0811, 'La red: el servicio de red (RED.BIN), IPCONFIG, PING y DESCARGAR', 'base >= ' + VERSION_085, [
        ('RED.BIN', r'\HISPA\RED.BIN'), ('IPCONFIG.BIN', r'\HISPA\IPCONFIG.BIN'),
        ('PING.BIN', r'\HISPA\PING.BIN'), ('DESCARGA.BIN', r'\HISPA\DESCARGA.BIN')]),
    ('tls', VERSION_CORTES, 'HTTPS: el servicio TLS (TLS.BIN, con BearSSL) y la orden HTTPS', 'base >= 0.8.1, red >= 0.8.1', [
        ('TLS.BIN', r'\HISPA\TLS.BIN'), ('HTTPS.BIN', r'\HISPA\HTTPS.BIN')]),
    # (0.8.9) SSH con el terminal nuevo en su ventana (ssh/vt.c: xterm-256color, el tamaño con window-change; el del Termómetro)
    ('ssh', VERSION_089, 'SSH: entrar en otros equipos (SSH) y en este desde otros (SSHD, con los usuarios del equipo; apagado hasta que se encienda)', 'base >= ' + VERSION_087 + ', red >= 0.8.4', [
        ('SSH.BIN', r'\HISPA\SSH.BIN'), ('SSHD.BIN', r'\HISPA\SSHD.BIN')]),
    ('tailscal', '0.8.9', 'Tailscale y WireGuard: la red privada de sus equipos, cada uno con su nombre y su IP 100.x', 'base >= 0.8.4, red >= 0.8.4, tls >= 0.8.1', [
        ('TAILSCAL.BIN', r'\HISPA\TAILSCAL.BIN'), ('WG.BIN', r'\HISPA\WG.BIN')]),
    # (WIFI) el Wi-Fi (castel/nucleo32/wifi/, docs/PENTIUM.md «El Wi-Fi»); el microcódigo, con su licencia
    # (0.8.10) (INTEL) la Intel 7265, etapa 1: encuentra las redes (sin el firmware de Intel, que no va en el paquete)
    # (0.8.11) (SIN-ADMIN) el diagnóstico en WIFI.LOG sin Registro=SI, y la ventana dice dónde está
    ('wifi', VERSION_0811, 'El Wi-Fi: el servicio Wi-Fi (WIFI.BIN: Broadcom BCM43224/43225 y Atheros AR9285), su ventana (REDWIFI) y el microcódigo de Broadcom',
     'base >= ' + VERSION_WIFI + ', red >= ' + VERSION_WIFI, [
        ('WIFI.BIN', r'\HISPA\WIFI.BIN'), ('REDWIFI.BIN', r'\HISPA\REDWIFI.BIN'), ('WIFI.TXT', r'\HISPA\WIFI.TXT'),
        ('BCM43XX.FW', r'\HISPA\BCM43XX.FW'), ('BCM43XXH.FW', r'\HISPA\BCM43XXH.FW'), ('BCM43XX.TXT', r'\HISPA\BCM43XX.TXT')]),
    ('sonido', '1.0', 'El sonido: el servicio de sonido (HD Audio, AC\'97 y Sound Blaster 16; SONIDO.BIN y la orden SONIDO) y los sonidos del sistema',
     'base >= ' + VERSION_SONIDO, [
        ('SONIDO.BIN', r'\HISPA\SONIDO.BIN'), ('INICIO.WAV', r'\HISPA\INICIO.WAV'), ('APAGAR.WAV', r'\HISPA\APAGAR.WAV'),
        ('AVISO.WAV', r'\HISPA\AVISO.WAV'), ('ERROR.WAV', r'\HISPA\ERROR.WAV')]),
    # (OFICINA2) hvprun 0.10: la rueda, Alt+letra, los títulos largos (con más
    # de 63 letras el núcleo no hacía la ventana), el módulo en la unidad del
    # sistema y la carpeta de los documentos (castel/nucleo32/hvprun/poficina.inc)
    ('hvprun', '0.10', 'HVPRUN: los programas de Hispa Ventanas dentro del escritorio (con su sonido, la rueda y Alt+letra)', 'base >= 0.7', [
        ('HVPRUN.BIN', r'\HISPA\HVPRUN.BIN')]),
    # (OFICINA2) la oficina 1.1: con Datos, el Lector, Publicaciones y
    # Presentaciones, y los tipos de letra en todos sus tamaños (TIPOS12/24/32)
    ('oficina', '1.1', 'La oficina de Hispa-386: Pluma, Cuentas, Facturas, Datos, Presentaciones, Publicaciones y Lector', 'hvprun >= 0.10', [
        ('FACTURAS.HVP', r'\OFICINA\FACTURAS.HVP'), ('PLUMA.HVP', r'\OFICINA\PLUMA.HVP'),
        ('CUENTAS.HVP', r'\OFICINA\CUENTAS.HVP'), ('MIRADOR.HVP', r'\OFICINA\MIRADOR.HVP'),
        ('DATOS.HVP', r'\OFICINA\DATOS.HVP'), ('LECTOR.HVP', r'\OFICINA\LECTOR.HVP'),
        ('PUBLICA.HVP', r'\OFICINA\PUBLICA.HVP'), ('PRESENTA.HVP', r'\OFICINA\PRESENTA.HVP'),
        ('TIPOS.DAT', r'\OFICINA\TIPOS.DAT'), ('TIPOS12.DAT', r'\OFICINA\TIPOS12.DAT'),
        ('TIPOS24.DAT', r'\OFICINA\TIPOS24.DAT'), ('TIPOS32.DAT', r'\OFICINA\TIPOS32.DAT'),
        ('DICCIO.DAT', r'\OFICINA\DICCIO.DAT')]),
    # (OFICINA2) Pintura, aparte (tipo multimedia, como el Mirador). (PINTURA32,
    # 0.8.8) La 2.0 es Pintura 2, el programa de dibujo de 32 bits
    # (castel/nucleo32/oficina/pintura): sustituye a la 1.0 (el PINTURA.HVP de
    # Hispa-386, en HVPRUN) y sigue siendo del sistema. Pide base 0.8.6 (la
    # llamada 172, el fondo de escritorio). Con ella, el menú (PINTURA.MNU,
    # tipo=Multimedia) y Mi equipo («Editar con Pintura», los .PCX) la usan
    ('pintura', '2.0.1', 'Pintura 2: dibujar y retocar imágenes en color verdadero (PNG, JPG, BMP, GIF, PCX, ICO, TGA; a PDF; fondo de escritorio)', 'base >= ' + VERSION_PUNTEROS, [
        ('PINTURA.BIN', r'\HISPA\PINTURA.BIN'), ('PINTURA.TXT', r'\HISPA\PINTURA.TXT'),
        ('PINTURA.MNU', r'\HISPA\MENU\PINTURA.MNU'), ('PINTURA.ICP', r'\HISPA\MENU\PINTURA.ICP')]),
    # [WIN32] los programas de Windows de 32 bits y DirectX (docs/pentium/WIN32.md)
    # (JUEGOS, 22/11/2026) win32 1.2: lo que pide un juego (hilos de verdad, SEH,
    # el registro, MSVCRT de VC6, el ratón de los juegos, UDP, la mezcla en su
    # hilo, el CD de sonido; WIN32 COMPRUEBA y WIN32.LOG). Vale con la base de
    # antes (sin las llamadas 183 a 186 hace lo de siempre); con todo, la base
    # que las lleve (la rama win32-juegos)
    # (HL, 22/11/2026) win32 1.3: lo que pedía Half-Life 1 (las DLL donde las
    # busca Windows, WSOCK32 por número, mmio, el mezclador, WININET, la
    # seguridad de ADVAPI32...; WIN32 COMPRUEBA /CON y /MAQUINA). Los mismos archivos
    ('win32', '1.3', 'Programas de Windows de 32 bits y juegos: hilos, SEH, registro, DirectX (con sonido y CD), OpenGL, WININET; WIN32.BIN, sus DLL y los ejemplos', 'base >= ' + VERSION_084, [
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
    ('navega', '0.2.3', 'El Navegador: la web de hoy (NetSurf con JavaScript), con pestañas y descargas',
     'base >= 0.8.3, red >= 0.8.1, tls >= 0.8.1', [
        ('NAVEGA/NAVEGA.BIN', r'\HISPA\NAVEGA\NAVEGA.BIN'), ('NAVEGA/NAVEGA.INI', r'\HISPA\NAVEGA\NAVEGA.INI'),
        ('NAVEGA/LEEME.TXT', r'\HISPA\NAVEGA\LEEME.TXT')]
     + [('NAVEGA/RECURSOS/' + n, '\\HISPA\\NAVEGA\\RECURSOS\\' + n) for n in NAVEGA_RECURSOS]
     + [('NAVEGA/FUENTES/' + n, '\\HISPA\\NAVEGA\\FUENTES\\' + n) for n in NAVEGA_FUENTES]
     + [('NAVEGA.MNU', r'\HISPA\MENU\NAVEGA.MNU'), ('NAVEGA.ICP', r'\HISPA\MENU\NAVEGA.ICP')]),
    # (ESCUDO) el antivirus (castel/nucleo32/escudo/paquete.sh). Sus firmas, aparte
    # (los paquetes «escbase», «escmes» y «escdia», que hace tools/escudo_firmas.py)
    # (0.8.10) 0.2.2: sin la lista de paquetes, otra vez a los 10 s, y el aviso en corto (el HP Mini)
    ('escudo', '0.2.2', 'Escudo: el antivirus (con las firmas de ClamAV), para el equipo y para revisar memorias USB',
     'base >= 0.8.3, red >= 0.8.1, tls >= 0.8.1', [
        ('ESCUDO.BIN', r'\HISPA\ESCUDO.BIN'), ('ESCUDO/LEEME.TXT', r'\HISPA\ESCUDO\LEEME.TXT'),
        ('MENU/ESCUDO.MNU', r'\HISPA\MENU\ESCUDO.MNU'), ('MENU/ESCUDO.ICP', r'\HISPA\MENU\ESCUDO.ICP')]),
    # (PDF) Hispa-PDF (castel/nucleo32/pdf/paquete.sh): la orden PDF y su
    # ventana; del tipo «oficina» (en la 0.8.5: TIPOS de hpq.py y tipo_de)
    ('pdf', '1.0', 'Hispa-PDF: unir, separar, girar, ordenar, comprimir y cifrar PDF; PDF a imágenes y a texto; imágenes a PDF',
     'base >= 0.8.3', [
        ('PDF.BIN', r'\HISPA\PDF.BIN'), ('HISPAPDF.BIN', r'\HISPA\HISPAPDF.BIN'), ('PDF/LEEME.TXT', r'\HISPA\PDF\LEEME.TXT'),
        ('MENU/HISPAPDF.MNU', r'\HISPA\MENU\HISPAPDF.MNU'), ('MENU/HISPAPDF.ICP', r'\HISPA\MENU\HISPAPDF.ICP')]),
    # (PLUMA32) Pluma 2, el procesador de textos de 32 bits (castel/nucleo32/oficina/pluma/paquete.sh;
    # tipo «oficina»): con él instalado, el menú y Mi equipo lo usan en lugar del Pluma de Hispa-386
    # (0.8.8) la 2.0.1: la misma Pluma con la biblioteca común de la oficina de
    # Cuentas 2 y Pintura 2 (memchr, strrchr, F11 y F12, las imágenes TGA)
    ('pluma', '2.0.2', 'Pluma 2: el procesador de textos de 32 bits (.PLU, .ODT, .DOCX, .RTF y .TXT; exporta a PDF; ortografía)',
     'base >= 0.8.3', [
        ('PLUMA.BIN', r'\HISPA\PLUMA.BIN'), ('PLUMA/LEEME.TXT', r'\HISPA\PLUMA\LEEME.TXT'),
        ('PLUMA/DICCIO.DAT', r'\HISPA\PLUMA\DICCIO.DAT'),
        ('MENU/PLUMA.MNU', r'\HISPA\MENU\PLUMA.MNU'), ('MENU/PLUMA.ICP', r'\HISPA\MENU\PLUMA.ICP')]),
    # (CUENTAS32) Cuentas 2, la hoja de cálculo de 32 bits (castel/nucleo32/oficina/cuentas/paquete.sh;
    # tipo «oficina»): con ella instalada, el menú y Mi equipo la usan en lugar del Cuentas de Hispa-386
    ('cuentas', '2.0', 'Cuentas 2: la hoja de cálculo de 32 bits (.HOJ, .XLSX, .ODS y .CSV; gráficos; exporta a PDF)',
     'base >= 0.8.3', [
        ('CUENTAS.BIN', r'\HISPA\CUENTAS.BIN'), ('CUENTAS/LEEME.TXT', r'\HISPA\CUENTAS\LEEME.TXT'),
        ('MENU/CUENTAS.MNU', r'\HISPA\MENU\CUENTAS.MNU'), ('MENU/CUENTAS.ICP', r'\HISPA\MENU\CUENTAS.ICP')]),
    # (ZIP) Comprimidos y las órdenes ZIP y UNZIP (castel/nucleo32/comprim/paquete.sh; tipo «herramientas»)
    ('comprim', '1.0.1', 'Comprimidos: los .ZIP como una carpeta (sacar, añadir y quitar, con contraseña) y las órdenes ZIP y UNZIP', 'base >= 0.8.4', [
        ('BIN/COMPRIM.BIN', r'\HISPA\COMPRIM.BIN'), ('BIN/ZIP.BIN', r'\HISPA\ZIP.BIN'), ('BIN/UNZIP.BIN', r'\HISPA\UNZIP.BIN'),
        ('COMPRIM.TXT', r'\HISPA\COMPRIM.TXT'), ('COMPRIM.MNU', r'\HISPA\MENU\COMPRIM.MNU'), ('COMPRIM.ICP', r'\HISPA\MENU\COMPRIM.ICP')]),
    # (MIRADOR) el Mirador, el visor de imágenes (castel/nucleo32/mirador/paquete.sh; tipo «multimedia»)
    # (AGENDA) la Agenda (castel/nucleo32/agenda/paquete.sh; tipo «oficina»)
    # (0.8.8) pide la base 0.8.8: el escritorio lanza su vigilante (AGENDA
    # /VIGILAR) al entrar cada usuario y dice la próxima cita en el reloj
    ('agenda', '1.0', 'Agenda: citas, tareas y cumpleaños con avisos (aunque esté cerrada), festivos de España y de cada comunidad, repeticiones e importar y exportar .ICS',
     'base >= ' + VERSION_088, [
        ('AGENDA.BIN', r'\HISPA\AGENDA.BIN'), ('AGENDA/LEEME.TXT', r'\HISPA\AGENDA\LEEME.TXT'),
        ('MENU/AGENDA.MNU', r'\HISPA\MENU\AGENDA.MNU'), ('MENU/AGENDA.ICP', r'\HISPA\MENU\AGENDA.ICP')]),
    ('mirador', '1.0.1', 'El Mirador: ver imágenes (BMP, PNG, JPG, GIF animados, ICO...), girar, diapositivas, pantalla completa y fondo de escritorio', 'base >= 0.8.6', [
        ('BIN/MIRADOR.BIN', r'\HISPA\MIRADOR.BIN'), ('MIRADOR.TXT', r'\HISPA\MIRADOR.TXT'),
        ('MIRADOR.MNU', r'\HISPA\MENU\MIRADOR.MNU'), ('MIRADOR.ICP', r'\HISPA\MENU\MIRADOR.ICP')]),
    # (REPRODUCTOR) el Reproductor de música (castel/nucleo32/reproductor/paquete.sh; tipo «multimedia»)
    ('reproduc', '1.0', 'El Reproductor: música (MP3, OGG, FLAC, WAV, MOD, XM, S3M, IT) y CD de música, con lista, ecualizador de 10 bandas y visualización',
     'base >= ' + VERSION_087 + ', sonido >= 1.0', [
        ('BIN/REPRODUC.BIN', r'\HISPA\REPRODUC.BIN'), ('REPRODUC.TXT', r'\HISPA\REPRODUC.TXT'),
        ('REPRODUC.MNU', r'\HISPA\MENU\REPRODUC.MNU'), ('REPRODUC.ICP', r'\HISPA\MENU\REPRODUC.ICP')]),
    # (JUEGOS2) los juegos (castel/nucleo32/juegos/paquete.sh; tipo «juegos»)
    ('juegos', '1.0', 'Los juegos: Hispa-Pinball (La Feria), Bloques, Rompemuros, Culebra, Campo de minas, Solitario, Tragabolas y Almacén', 'base >= 0.8.4',
     [(o + '/' + j + '.' + e if o else j + '.' + e, '\\HISPA\\' + d + '\\' + j + '.' + e)
      for j in ('BLOQUES', 'ROMPEMUR', 'PINBALL', 'CULEBRA', 'MINAS', 'SOLITARI', 'TRAGABOL', 'ALMACEN')
      for o, d, e in (('BIN', 'JUEGOS', 'BIN'), ('', 'MENU', 'MNU'), ('', 'MENU', 'ICP'))]
     + [('JUEGOS.TXT', r'\HISPA\JUEGOS\LEEME.TXT')]),
    # (CORREO) Correo, el cliente de correo (castel/nucleo32/correo/paquete.sh; tipo «internet»). El
    # secreto de \HISPA\CORREO y el STARTTLS llegan con el base y el tls de la 0.8.7
    # (PARPADEO) 1.0.1: sin parpadeo (el lienzo propio, un vuelco; nada por el ratón que solo pasa); comprim y mirador 1.0.1, tampoco repintan por él
    # (CORREO) 1.0.2: dos clics (o Intro) en la lista, el mensaje a toda la ventana, con sus botones; Esc vuelve
    # (PUNTEROS, 0.8.9) 1.0.3: pide su puntero (la mano en los enlaces, la «I» al escribir; la llamada 177)
    ('correo', '1.0.3', 'Correo: el cliente de correo (IMAP, POP3 y SMTP, siempre cifrado; HTML sin scripts, adjuntos por Escudo, sin conexión)',
     'base >= ' + VERSION_087 + ', red >= 0.8.1, tls >= ' + VERSION_087, [
        ('CORREO.BIN', r'\HISPA\CORREO.BIN'), ('CORREO.TXT', r'\HISPA\CORREO.TXT'),
        ('CORREO.MNU', r'\HISPA\MENU\CORREO.MNU'), ('CORREO.ICP', r'\HISPA\MENU\CORREO.ICP')]),
    # (COMPAT) el Asistente de compatibilidad (castel/nucleo32/compat/paquete.sh; tipo «herramientas»): qué le
    # falta a un programa de Windows, el informe (con permiso), el aviso al cambiar win32 y los componentes
    # de Microsoft (la lista REDIST.LST: se bajan de Microsoft, no van aquí)
    ('compat', '1.0', 'Asistente de compatibilidad: qué le falta a un programa de Windows, explicado; avisa cuando ya está y pone los componentes de Microsoft',
     'base >= 0.8.4, win32 >= 1.0', [
        ('BIN/COMPAT.BIN', r'\HISPA\COMPAT.BIN'), ('COMPAT/REDIST.LST', r'\HISPA\COMPAT\REDIST.LST'),
        ('COMPAT/COMPAT.INI', r'\HISPA\COMPAT\COMPAT.INI'), ('COMPAT/LEEME.TXT', r'\HISPA\COMPAT\LEEME.TXT'),
        ('COMPAT.MNU', r'\HISPA\MENU\COMPAT.MNU'), ('COMPAT.ICP', r'\HISPA\MENU\COMPAT.ICP')]),
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


def pon_serie(carpeta, serie=None):
    """(USUARIOS) «serie=N» en CATALOGO.TXT, detrás de la primera línea: N, la
    que se diga o la de antes más uno (y, como poco, los segundos desde 1970:
    sube aunque «catalogo» haya rehecho el archivo sin ella). Da N."""
    ruta = os.path.join(carpeta, 'CATALOGO.TXT')
    filas = open(ruta, 'rb').read().split(b'\r\n')
    vieja = 0
    for x in filas:
        if x.startswith(b'serie='):
            try:
                vieja = int(x[6:].decode())
            except ValueError:
                pass
    filas = [x for x in filas if not x.startswith(b'serie=')]
    if serie is None:
        serie = max(vieja + 1, int(time.time()))
    if serie < 1 or serie > 0xFFFFFFFF:
        falla('la serie %d no vale (de 1 a 4294967295)' % serie)
    filas.insert(1, b'serie=%d' % serie)
    with open(ruta, 'wb') as f:
        f.write(b'\r\n'.join(filas))
    return serie


def firma(carpeta, privada, serie=None):
    k = lee_privada(privada)
    pon_serie(carpeta, serie)
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
            firma(a[2], a[4], int(a[6]) if len(a) > 6 and a[5] == '--serie' else None)
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
        firma(a[2], a[3], int(a[5]) if len(a) > 5 and a[4] == '--serie' else None)
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
