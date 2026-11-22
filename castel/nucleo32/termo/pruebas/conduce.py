#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
"""termo/pruebas/conduce.py CARPETA GUION -- orden de QEMU...

(TERMO) Lleva una máquina QEMU para las pruebas del Termómetro que no caben en
una lista de órdenes a tiempo fijo (castel/pruebas_termo.sh): espera a que el
puerto serie o un archivo digan algo, saca la pantalla, busca en ella la
ventana del terminal (por su fondo) y la arrastra por la esquina. El GUION es
un archivo con un paso por línea:

  serie TEXTO SEGUNDOS     espera a que el puerto serie diga TEXTO
  archivo RUTA TEXTO SEG   espera a que RUTA tenga TEXTO
  pausa SEGUNDOS
  tecla NOMBRE...          sendkey de QEMU (e, t, up, ctrl-c, shift-tab...)
  captura NOMBRE           la pantalla, en CARPETA/NOMBRE.ppm
  encoge RGB DX DY         busca el rectángulo de ese fondo (el terminal) y
                           arrastra su esquina de abajo a la derecha DX, DY
  clic X Y                 un clic en ese punto (el botón de maximizar)
  doble X Y                dos clics en ese punto
  monitor ORDEN            una orden cualquiera del monitor

Lo que pasa, en CARPETA/conduce.txt; el puerto serie, en CARPETA/serie.txt.
Acaba la máquina al final (su PID, nada más). Sale con 0 si todos los pasos
se han cumplido; si no, con 1 (y dice cuál).
"""
import os
import socket
import subprocess
import sys
import time

carpeta, guion = sys.argv[1], sys.argv[2]
orden_qemu = sys.argv[sys.argv.index('--') + 1:]
os.makedirs(carpeta, exist_ok=True)
mon = os.path.join(carpeta, 'mon.sock')
serie = os.path.join(carpeta, 'serie.txt')
for f in (mon, serie):
    if os.path.exists(f):
        os.remove(f)
diario = open(os.path.join(carpeta, 'conduce.txt'), 'w')


def apunta(t):
    diario.write('%7.1f %s\n' % (time.time() - t0, t))
    diario.flush()


t0 = time.time()
q = subprocess.Popen(orden_qemu + ['-monitor', 'unix:%s,server,nowait' % mon, '-serial', 'file:%s' % serie],
                     stdout=subprocess.DEVNULL, stderr=open(os.path.join(carpeta, 'qemu.err'), 'w'))


def monitor(o):
    for _ in range(50):
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(mon)
            break
        except OSError:
            time.sleep(0.1)
    else:
        return ''
    s.settimeout(5)
    r = b''
    try:
        r += s.recv(4096)
        s.sendall(o.encode() + b'\n')
        time.sleep(0.15)
        r += s.recv(65536)
    except OSError:
        pass
    s.close()
    return r.decode('latin-1')


def lee(ruta):
    try:
        return open(ruta, 'rb').read().decode('cp437', 'replace')
    except OSError:
        return ''


def ppm(ruta):
    d = open(ruta, 'rb').read()
    # P6\nan al\n255\n
    partes = d.split(b'\n', 3)
    an, al = map(int, partes[1].split())
    return an, al, partes[3]


# la flecha: el núcleo la pone en el centro; se lleva a la esquina y se cuenta desde ahí
rx = ry = None


def ir(x, y):
    global rx, ry
    if rx is None:
        for _ in range(60):
            monitor('mouse_move -60 -60')
            time.sleep(0.02)
        rx = ry = 0
    dx, dy = x - rx, y - ry
    while dx or dy:
        sx = max(-40, min(40, dx))
        sy = max(-40, min(40, dy))
        monitor('mouse_move %d %d' % (sx, sy))
        time.sleep(0.04)
        dx -= sx
        dy -= sy
    rx, ry = x, y
    time.sleep(0.3)


fallo = None
for linea in open(guion, encoding='utf-8'):
    linea = linea.rstrip('\n')
    if not linea.strip() or linea.startswith('#'):
        continue
    p = linea.split(' ')
    apunta(linea)
    if q.poll() is not None:
        fallo = 'QEMU ha acabado antes de tiempo'
        break
    if p[0] == 'serie':
        texto, seg = ' '.join(p[1:-1]), float(p[-1])
        fin = time.time() + seg
        while time.time() < fin and texto not in lee(serie):
            time.sleep(0.5)
        if texto not in lee(serie):
            fallo = 'el puerto serie no ha dicho «%s» en %s s' % (texto, p[-1])
            break
    elif p[0] == 'archivo':
        ruta, texto, seg = p[1], ' '.join(p[2:-1]), float(p[-1])
        fin = time.time() + seg
        while time.time() < fin and texto not in lee(ruta):
            time.sleep(0.5)
        if texto not in lee(ruta):
            fallo = '%s no dice «%s» en %s s' % (ruta, texto, p[-1])
            break
    elif p[0] == 'pausa':
        time.sleep(float(p[1]))
    elif p[0] == 'tecla':
        for t in p[1:]:
            monitor('sendkey ' + t)
            time.sleep(0.25)
    elif p[0] == 'captura':
        monitor('screendump %s' % os.path.join(carpeta, p[1] + '.ppm'))
        time.sleep(0.5)
    elif p[0] == 'clic':
        ir(int(p[1]), int(p[2]))
        monitor('mouse_button 1')
        monitor('mouse_button 0')
        time.sleep(0.5)
    elif p[0] == 'doble':
        ir(int(p[1]), int(p[2]))
        # (los dos clics por la misma conexión del monitor: si no, van muy separados)
        so = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        so.connect(mon)
        so.settimeout(2)
        try:
            so.recv(4096)
        except OSError:
            pass
        for o in ('mouse_button 1', 'mouse_button 0', 'mouse_button 1', 'mouse_button 0'):
            so.sendall(o.encode() + b'\n')
            time.sleep(0.04)
        so.close()
        time.sleep(0.5)
    elif p[0] == 'monitor':
        monitor(' '.join(p[1:]))
    elif p[0] == 'encoge':
        rgb = bytes.fromhex(p[1])
        dx, dy = int(p[2]), int(p[3])
        f = os.path.join(carpeta, 'encoge.ppm')
        monitor('screendump %s' % f)
        time.sleep(0.8)
        an, al, d = ppm(f)
        xs, ys = [], []
        for y in range(0, al, 2):
            fila = d[y * an * 3:(y + 1) * an * 3]
            k = fila.find(rgb * 4)
            while k >= 0 and k % 3:
                k = fila.find(rgb * 4, k + 1)
            if k >= 0:
                j = fila.rfind(rgb)
                while j >= 0 and j % 3:
                    j = fila.rfind(rgb, 0, j)
                xs += [k // 3, j // 3]
                ys.append(y)
        if not xs:
            fallo = 'no se encuentra el terminal (el fondo %s) en la pantalla' % p[1]
            break
        x1, y1 = max(xs), max(ys)
        apunta('el terminal: hasta %d,%d (pantalla de %d x %d); su esquina, a %d,%d' % (x1, y1, an, al, x1 + 4 + dx, y1 + 4 + dy))
        ir(x1 + 4, y1 + 4)
        monitor('mouse_button 1')
        time.sleep(0.3)
        ir(x1 + 4 + dx, y1 + 4 + dy)
        monitor('mouse_button 0')
        time.sleep(0.5)
    else:
        fallo = 'paso desconocido: ' + linea
        break
apunta('fin: ' + (fallo or 'todo cumplido'))
if q.poll() is None:
    q.kill()
    q.wait()
sys.exit(1 if fallo else 0)
