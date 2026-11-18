#!/usr/bin/env python3
"""Arranca QEMU con la consola en el puerto serie y escribe las órdenes dadas,
de una en una, cada vez que sale el indicador del sistema.

  ordenes_drv.py registro "ORDEN 1" "ORDEN 2"... -- qemu...
"""
import os, select, subprocess, sys, time

def main():
    log = sys.argv[1]
    i = sys.argv.index('--')
    todo, qemu = sys.argv[2:i], sys.argv[i + 1:]
    p = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, bufsize=0)
    buf, pos, t0, fin = b'', 0, time.time(), None
    while time.time() - t0 < 180:
        if fin and time.time() > fin:
            break
        r, _, _ = select.select([p.stdout], [], [], 0.3)
        if r:
            d = os.read(p.stdout.fileno(), 4096)
            if not d:
                break
            buf += d
            continue
        if fin is None and time.time() - t0 > 5 and buf[pos:].rstrip().endswith(b'>'):
            if todo:
                time.sleep(0.3)
                p.stdin.write(todo.pop(0).encode('cp437') + b'\r')
                p.stdin.flush()
                pos = len(buf)
            else:
                fin = time.time() + 1.0
    p.terminate()
    p.wait()
    open(log, 'wb').write(buf)

if __name__ == '__main__':
    main()
