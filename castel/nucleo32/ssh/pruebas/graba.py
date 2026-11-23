#!/usr/bin/env python3
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# graba.py COLS FILAS SALIDA SEGUNDOS orden... [-- t:teclas ...]
# (TERMO) Pone en marcha la orden en un pseudoterminal de COLS x FILAS con
# TERM=xterm-256color y graba todo lo que escribe (para vt_prueba). Las
# teclas: «t:texto» a los t segundos (\e, \r, \x.. como en Python).
import os, pty, select, signal, struct, sys, time, fcntl, termios
cols, filas, salida, seg = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3], float(sys.argv[4])
resto = sys.argv[5:]
teclas = []
if '--' in resto:
    k = resto.index('--')
    teclas = [(float(a.split(':', 1)[0]), a.split(':', 1)[1].encode('latin-1', 'backslashreplace').decode('unicode_escape').encode('utf-8')) for a in resto[k + 1:]]
    resto = resto[:k]
pid, fd = pty.fork()
if pid == 0:
    os.environ['TERM'] = 'xterm-256color'
    os.environ['COLORTERM'] = 'truecolor'
    os.environ['LANG'] = 'C.UTF-8'
    os.environ['LC_ALL'] = 'C.UTF-8'
    os.execvp(resto[0], resto)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', filas, cols, 0, 0))
os.kill(pid, signal.SIGWINCH)
t0 = time.time()
datos = b''
while time.time() - t0 < seg:
    while teclas and time.time() - t0 >= teclas[0][0]:
        os.write(fd, teclas.pop(0)[1])
    r, _, _ = select.select([fd], [], [], 0.05)
    if r:
        try:
            b = os.read(fd, 65536)
        except OSError:
            break
        if not b:
            break
        datos += b
open(salida, 'wb').write(datos)
try:
    os.kill(pid, signal.SIGKILL)
    os.waitpid(pid, 0)
except (ProcessLookupError, ChildProcessError):
    pass
