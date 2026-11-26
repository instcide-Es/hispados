#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# =============================================================================
#  tools/remota_pentium.py - el cliente de la consola remota de Hispa-Pentium
#  («Dejar entrar a Claude»: SSHD /REMOTA, castel/nucleo32/ssh/remota.c)
#  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
#  La consola remota es SSH (cifrado) con un código de 6 cifras de un solo
#  uso como contraseña, que el dueño ve en la ventana de su equipo. Con
#  OpenSSH y una conexión maestra (ControlMaster), UN código vale para toda
#  la visita: las órdenes, leer los registros y copiar archivos van por la
#  misma conexión hasta «cerrar» (o hasta que el dueño cierre la ventana o
#  pulse «Echar»).
#
#  Uso:
#    remota_pentium.py abrir EQUIPO CODIGO [--puerto 2323] [--nombre toshiba]
#                      (EQUIPO: la IP, la de Tailscale 100.x o un nombre;
#                       --nombre: para recordar su llave aparte —detrás de un
#                       mismo router puede haber varios portátiles—)
#    remota_pentium.py orden "ver"          una orden de ORDENES, y su salida
#    remota_pentium.py consola              la Consola entera (con terminal)
#    remota_pentium.py leer RUTA [LOCAL]    trae un archivo (a la salida, o a LOCAL)
#    remota_pentium.py escribir LOCAL RUTA  lleva un archivo
#    remota_pentium.py registros [CARPETA]  trae WIFI.LOG, RED.LOG, ARRANQUE.TXT,
#                                           SDHCI.TXT, SDIO.TXT, BT.LOG y REMOTA.LOG
#    remota_pentium.py estado               ¿sigue abierta la visita?
#    remota_pentium.py cerrar               acaba la visita
#    remota_pentium.py consola486 IP CODIGO [ORDEN...]
#                      la CONSOLA de Hispa-DOS 386/486 (TCP 2323, sin cifrar):
#                      manda el código y las órdenes y enseña lo que sale
#
#  Lo suyo va en $REMOTA_DIR (si no, ~/.hispa_remota): la conexión maestra,
#  las llaves de los equipos (conocidos) y el último equipo abierto. Los
#  archivos de Hispa-Pentium salen en la página 437 (los registros, tal cual).
# =============================================================================
import os
import shlex
import socket
import subprocess
import sys
import time

DIR = os.environ.get('REMOTA_DIR') or os.path.expanduser('~/.hispa_remota')
REGISTROS = [r'\ARRANQUE.TXT', r'\HISPA\WIFI.LOG', r'\WIFI.LOG', r'\HISPA\RED.LOG', r'\RED.LOG',
             r'\SDHCI.TXT', r'\SDIO.TXT', r'\HISPA\BT.LOG', r'\HISPA\REMOTA.LOG', r'\HISPA\SSH\REMOTA.LOG']


def falla(t, codigo=1):
    sys.stderr.write('remota: %s\n' % t)
    sys.exit(codigo)


def prepara():
    os.makedirs(DIR, mode=0o700, exist_ok=True)


def ultimo():
    try:
        with open(os.path.join(DIR, 'ultimo')) as f:
            equipo, puerto, nombre = f.read().split()
        return equipo, int(puerto), nombre
    except (OSError, ValueError):
        falla('no hay ninguna visita abierta: «remota_pentium.py abrir EQUIPO CODIGO» primero')


def opciones(equipo, puerto, nombre):
    return ['-p', str(puerto), '-S', os.path.join(DIR, 'ctl-%s' % nombre),
            '-o', 'UserKnownHostsFile=' + os.path.join(DIR, 'conocidos'),
            '-o', 'HostKeyAlias=' + nombre, '-o', 'StrictHostKeyChecking=accept-new',
            '-o', 'UpdateHostKeys=no', '-o', 'LogLevel=ERROR']


def ssh(args, entrada=None, terminal=False, salida=None):
    equipo, puerto, nombre = ultimo()
    cmd = ['ssh'] + opciones(equipo, puerto, nombre) + (['-t'] if terminal else ['-T']) + ['claude@' + equipo] + args
    return subprocess.run(cmd, stdin=entrada, stdout=salida)


def esta_abierta(equipo, puerto, nombre):
    r = subprocess.run(['ssh'] + opciones(equipo, puerto, nombre) + ['-O', 'check', 'claude@' + equipo],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return r.returncode == 0


def abrir(a):
    if len(a) < 2:
        falla('uso: abrir EQUIPO CODIGO [--puerto N] [--nombre NOMBRE]')
    equipo, codigo = a[0], a[1].replace(' ', '').replace('-', '')
    puerto, nombre = 2323, None
    i = 2
    while i < len(a):
        if a[i] == '--puerto' and i + 1 < len(a):
            puerto = int(a[i + 1]); i += 2
        elif a[i] == '--nombre' and i + 1 < len(a):
            nombre = a[i + 1]; i += 2
        else:
            falla('no sé qué es «%s»' % a[i])
    if not (len(codigo) == 6 and codigo.isdigit()):
        falla('el código son 6 cifras (el de la ventana «Dejar entrar a Claude»)')
    nombre = nombre or ('%s_%d' % (equipo, puerto))
    prepara()
    if esta_abierta(equipo, puerto, nombre):
        print('remota: ya hay una visita abierta a %s (puerto %d)' % (equipo, puerto))
    else:
        pide = os.path.join(DIR, 'codigo.sh')
        with open(pide, 'w') as f:
            f.write('#!/bin/sh\nprintf "%s\\n" "$REMOTA_CODIGO"\n')
        os.chmod(pide, 0o700)
        ent = dict(os.environ, SSH_ASKPASS=pide, SSH_ASKPASS_REQUIRE='force', DISPLAY=os.environ.get('DISPLAY', ':0'),
                   REMOTA_CODIGO=codigo)
        cmd = ['ssh'] + opciones(equipo, puerto, nombre) + [
            '-M', '-f', '-N', '-o', 'ControlPersist=yes', '-o', 'ConnectTimeout=20',
            '-o', 'PreferredAuthentications=password', '-o', 'PubkeyAuthentication=no',
            '-o', 'NumberOfPasswordPrompts=1', '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=4',
            'claude@' + equipo]
        r = subprocess.run(cmd, env=ent, stdin=subprocess.DEVNULL, start_new_session=True)
        if r.returncode != 0:
            falla('no se ha podido entrar (¿el código, caducado o mal copiado? ¿la ventana, cerrada? '
                  '¿el puerto %d, sin abrir en el router?)' % puerto, 255)
    with open(os.path.join(DIR, 'ultimo'), 'w') as f:
        f.write('%s %d %s\n' % (equipo, puerto, nombre))
    h = subprocess.run(['ssh-keygen', '-l', '-F', nombre, '-f', os.path.join(DIR, 'conocidos')],
                       capture_output=True, text=True).stdout
    huella = [x for x in h.split() if x.startswith('SHA256:')]
    print('remota: dentro de %s (puerto %d). Huella del equipo: %s (la de su ventana)' %
          (equipo, puerto, huella[0] if huella else '?'))


def leer(ruta, destino=None):
    if destino:
        with open(destino, 'wb') as f:
            r = ssh(['remota:leer ' + ruta], salida=f)
        if r.returncode != 0:
            os.unlink(destino)
        return r.returncode
    sys.stdout.flush()
    return ssh(['remota:leer ' + ruta]).returncode


def escribir(local, ruta):
    with open(local, 'rb') as f:
        return ssh(['remota:escribir ' + ruta], entrada=f).returncode


def registros(carpeta):
    os.makedirs(carpeta, exist_ok=True)
    n = 0
    for r in REGISTROS:
        d = os.path.join(carpeta, r.strip('\\').replace('\\', '_'))
        with open(d, 'wb') as f:
            ok = ssh(['remota:leer ' + r], salida=f).returncode == 0
        if ok:
            n += 1
            print('  %-24s %8d octetos -> %s' % (r, os.path.getsize(d), d))
        else:
            os.unlink(d)
    print('remota: %d registros en %s' % (n, carpeta))
    return 0


def cerrar():
    equipo, puerto, nombre = ultimo()
    subprocess.run(['ssh'] + opciones(equipo, puerto, nombre) + ['-O', 'exit', 'claude@' + equipo],
                   stderr=subprocess.DEVNULL)
    print('remota: visita cerrada')
    return 0


def consola486(a):
    """La CONSOLA de Hispa-DOS 386/486: TCP en claro, código y órdenes."""
    if len(a) < 2:
        falla('uso: consola486 IP CODIGO [ORDEN...]')
    s = socket.create_connection((a[0], 2323), timeout=30)
    def hasta(texto, espera=20):
        b = b''
        fin = time.time() + espera
        while time.time() < fin and texto not in b:
            try:
                x = s.recv(4096)
            except socket.timeout:
                break
            if not x:
                break
            b += x
        return b
    sys.stdout.write(hasta(b'acceso:').decode('cp437', 'replace'))
    s.sendall(a[1].encode() + b'\r\n')
    sys.stdout.write(hasta(b'>').decode('cp437', 'replace'))
    for o in a[2:] + ['SALIR']:
        s.sendall(o.encode('cp437', 'replace') + b'\r\n')
        sys.stdout.write(hasta(b'>' if o != 'SALIR' else b'luego', 60).decode('cp437', 'replace'))
    s.close()
    print()
    return 0


def main(a):
    if not a or a[0] in ('-h', '--ayuda', 'ayuda'):
        print(__doc__ or open(__file__).read().split('# ====')[1])
        return 0
    o = a[0]
    if o == 'abrir':
        return abrir(a[1:])
    if o == 'orden':
        return ssh([' '.join(a[1:])]).returncode
    if o == 'consola':
        return ssh([], terminal=True).returncode
    if o == 'leer' and len(a) in (2, 3):
        return leer(a[1], a[2] if len(a) == 3 else None)
    if o == 'escribir' and len(a) == 3:
        return escribir(a[1], a[2])
    if o == 'registros':
        return registros(a[1] if len(a) > 1 else os.path.join(DIR, 'registros-' + time.strftime('%Y%m%d-%H%M%S')))
    if o == 'estado':
        e = ultimo()
        if esta_abierta(*e):
            print('remota: abierta (%s, puerto %d)' % e[:2]); return 0
        print('remota: no hay visita abierta (la última: %s, puerto %d)' % e[:2]); return 1
    if o == 'cerrar':
        return cerrar()
    if o == 'consola486':
        return consola486(a[1:])
    falla('no sé qué es «%s» (remota_pentium.py ayuda)' % o)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]) or 0)
