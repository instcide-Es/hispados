#!/usr/bin/env python3
"""Envía entradas a un dispositivo concreto de QEMU por QMP (input-send-event).

uso: qmp.py SOCKET ORDEN...
  type DISPOSITIVO TEXTO   teclea el texto (letras, cifras, espacio, . \\ :)
  key DISPOSITIVO QCODE    pulsa y suelta una tecla (ret, esc, f2...)
  combo DISPOSITIVO A+B    pulsa varias a la vez (ctrl+backspace, alt+left...)
  move DISPOSITIVO DX DY   movimiento relativo del ratón
  btn DISPOSITIVO 0|1      botón izquierdo soltado / pulsado
  down DISPOSITIVO QCODE   aprieta una tecla y la deja apretada (ctrl...)
  up DISPOSITIVO QCODE     la suelta
  (DISPOSITIVO "-": sin dispositivo, al que QEMU tenga activo)
  wait SEGUNDOS
Sends input events to a specific QEMU device through QMP.
"""
import json, socket, sys, time

QC = {' ': 'spc', '.': 'dot', '\\': 'backslash', ':': ('shift', 'semicolon'),
      '-': 'minus', '/': 'slash', ',': 'comma'}


def main():
    s = socket.socket(socket.AF_UNIX)
    s.connect(sys.argv[1])
    f = s.makefile('rw')
    f.readline()

    def cmd(c, args=None):
        m = {'execute': c}
        if args is not None:
            m['arguments'] = args
        f.write(json.dumps(m) + '\n')
        f.flush()
        while True:
            r = json.loads(f.readline())
            if 'return' in r or 'error' in r:
                return r

    def ev(dev, events):
        args = {'events': events}
        if dev != '-':
            args['device'] = dev
        cmd('input-send-event', args)

    def key(dev, code, down):
        ev(dev, [{'type': 'key', 'data': {'down': down, 'key': {'type': 'qcode', 'data': code}}}])

    def tap(dev, codes):
        for c in codes:
            key(dev, c, True)
            time.sleep(0.12)
        for c in reversed(codes):
            key(dev, c, False)
            time.sleep(0.12)

    cmd('qmp_capabilities')
    a = sys.argv[2:]
    i = 0
    while i < len(a):
        op = a[i]
        if op == 'type':
            dev, text = a[i + 1], a[i + 2]
            for ch in text:
                q = QC.get(ch)
                if q is None:
                    q = ('shift', ch.lower()) if ch.isupper() else ch
                tap(dev, list(q) if isinstance(q, tuple) else [q])
            i += 3
        elif op == 'key':
            tap(a[i + 1], [a[i + 2]])
            i += 3
        elif op in ('down', 'up'):
            key(a[i + 1], a[i + 2], op == 'down')
            time.sleep(0.12)
            i += 3
        elif op == 'combo':
            tap(a[i + 1], a[i + 2].split('+'))
            i += 3
        elif op == 'move':
            ev(a[i + 1], [{'type': 'rel', 'data': {'axis': 'x', 'value': int(a[i + 2])}},
                          {'type': 'rel', 'data': {'axis': 'y', 'value': int(a[i + 3])}}])
            i += 4
        elif op == 'btn':
            ev(a[i + 1], [{'type': 'btn', 'data': {'down': a[i + 2] == '1', 'button': 'left'}}])
            i += 3
        elif op == 'wait':
            time.sleep(float(a[i + 1]))
            i += 2
        else:
            raise SystemExit('orden desconocida: ' + op)


if __name__ == '__main__':
    main()
