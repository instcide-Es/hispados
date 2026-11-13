#!/usr/bin/env python3
"""Hace de persona en las pruebas de AUDICION: arranca QEMU con la consola en
el puerto serie, escribe la orden y contesta a cada pregunta cuando aparece.

  audicion_drv.py registro "respuestas" [--cmd ORDEN] [--pre ORDEN]...
                  [--nada] [--post ORDEN]... -- qemu...

--nada: no se escribe ninguna orden (AUDICION la lanza el arranque: la opción
«Prueba» del menú); se contesta desde el principio.
--post: órdenes que se escriben, de una en una, en cada indicador del sistema
que sale después del informe (para ver lo que queda tras AUDICION).

respuestas: reglas separadas por «;»:
  default=S      lo que se contesta si ninguna otra regla vale
  P03=N          a la pregunta número 3
  P05=RS         a la 5: primero R (repetir) y luego S
  /texto/=N      a las preguntas que contienen ese texto
  P07=E          Esc
Se aplica la primera regla que vale: primero las de número, después las de
texto (en el orden dado) y al final «default».
"""
import os, re, select, subprocess, sys, time

def main():
    log, spec = sys.argv[1], sys.argv[2]
    rest = sys.argv[3:]
    cmd, pre, post, nada = 'AUDICION', [], [], False
    while rest and rest[0] != '--':
        if rest[0] == '--nada':
            nada = True
            rest = rest[1:]
            continue
        if rest[0] == '--cmd':
            cmd = rest[1]
        elif rest[0] == '--pre':
            pre.append(rest[1])
        elif rest[0] == '--post':
            post.append(rest[1])
        rest = rest[2:]
    qemu = rest[1:]
    bynum, bytext, default = {}, [], 'S'
    for r in filter(None, spec.split(';')):
        k, v = r.rsplit('=', 1)
        if k == 'default':
            default = v
        elif k.startswith('/'):
            bytext.append((k.strip('/'), v))
        else:
            bynum[int(k[1:])] = v
    p = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, bufsize=0)
    buf = b''
    pos = 0                 # hasta dónde se ha mirado
    used = {}               # letras ya gastadas de cada pregunta
    stage = 1 if nada else 0
    t0 = time.time()
    tend = None
    todo = pre + [cmd]
    after = None            # tras el informe: dónde empieza lo nuevo

    def send(b):
        p.stdin.write(b)
        p.stdin.flush()

    while time.time() - t0 < 300:
        if tend and time.time() > tend:
            break
        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            d = os.read(p.stdout.fileno(), 4096)
            if not d:
                break
            buf += d
        elif stage == 0 and time.time() - t0 > 5 and buf.rstrip().endswith(b'>'):
            # el indicador del sistema: las órdenes, de una en una
            if todo:
                time.sleep(0.3)
                send(todo.pop(0).encode('cp437') + b'\r')
                pos = len(buf)
                if not todo:
                    stage = 1
            continue
        if stage == 0:
            continue
        if stage == 2:
            # después del informe: las órdenes de --post en cada indicador
            if not r and buf[after:].rstrip().endswith(b'>'):
                if post:
                    time.sleep(0.3)
                    send(post.pop(0).encode('cp437') + b'\r')
                    after = len(buf)
                else:
                    tend = time.time() + 1.5
                    stage = 3
            continue
        if stage == 3:
            continue
        new = buf[pos:]
        if b'Pulse una tecla para empezar' in new:
            pos = len(buf)
            time.sleep(0.2)
            send(b' ')
            continue
        m = None
        for m in re.finditer(rb'\((S/N/R|S/N)\) $', new):
            pass
        if m and not r:
            line = new[:m.end()].split(b'\n')[-1].decode('cp437')
            pos = len(buf)
            if 'Continuar de todos modos' in line:
                send(b'S')
                continue
            qm = re.search(r'P(\d\d)\. ', line)
            n = int(qm.group(1)) if qm else used.get('last', 0)
            used['last'] = n
            ans = bynum.get(n)
            if ans is None:
                for t, v in bytext:
                    if t in line:
                        ans = v
                        break
            if ans is None:
                ans = default
            i = used.get(n, 0)
            ch = ans[min(i, len(ans) - 1)]
            used[n] = i + 1
            time.sleep(0.15)
            send(b'\x1b' if ch == 'E' else ch.encode())
            continue
        if tend is None and (b'Informe guardado' in new or b'NO se ha podido guardar' in new):
            if post:
                stage = 2
                after = len(buf)
            else:
                tend = time.time() + 2.5
    p.terminate()
    p.wait()
    open(log, 'wb').write(buf)

if __name__ == '__main__':
    main()
