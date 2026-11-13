#!/usr/bin/env python3
"""Comprobaciones de Hispa-System para tests/run_hispasms_tests.sh.

  hispasms_check.py pantalla CAPTURA.ppm X Y ESCALA ESPERADO NOMBRE [X0 Y0 ANCHO ALTO]
      La imagen de la consola que hay en la captura de QEMU (su esquina en
      X,Y; cada punto, ESCALA x ESCALA) es la de ESPERADO: un PGM (o .pgm.gz)
      de 256x192 con los colores de la consola (0-63: dos bits de rojo, dos
      de verde y dos de azul), como los que escribe smsnativo. Cada color se
      pasa al más parecido de la paleta de Hispa Ventanas, igual que hace
      HISPASMS.HVP. Con X0 Y0 ANCHO ALTO, solo esa zona.
  hispasms_check.py iguales A.pgm[.gz] B.pgm[.gz] NOMBRE
  hispasms_check.py guion SALIDA TECLA...
      El guion de smsnativo que corresponde a esas teclas en el modo paso a
      paso (/P): 60 cuadros al abrir; cada tecla, 6 cuadros con su botón y 6
      sin él; «spc», 30 sin botones. Escribe cuántos cuadros son. Teclas:
      up down left right z (botón 1) x (botón 2) ret (pausa).
  hispasms_check.py suma ARCHIVO [DESDE CUANTOS]
      La suma (como la de HISPASMS.LOG) de un PGM o de un trozo de archivo.
  hispasms_check.py png ENTRADA.pgm[.gz] SALIDA.ppm
      La imagen de smsnativo con los colores de la consola, para verla.
  hispasms_check.py resultado MEMORIA [ram]
      El resultado que deja una ROM de prueba en la RAM del cartucho
      (8000h: fallos, DEh B0h 61h y el texto): «N ÚLTIMA LÍNEA». Con «ram»,
      en la de la consola (DF00h), para las que no la tienen.
Escribe «  OK   ...» o «  FAIL ...» y sale con 1 si falla.
"""
import gzip, os, re, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
from hispanes_check import paleta_hv, lee_ppm, suma, fin
MARGEN = 10
ALTO = 192
TECLAS = {'up': 0x01, 'down': 0x02, 'left': 0x04, 'right': 0x08, 'z': 0x10, 'x': 0x20, 'ret': 0x80}


def color_sms(c):
    """el color de la consola (6 bits) en rojo, verde y azul"""
    return (85 * (c & 3), 85 * ((c >> 2) & 3), 85 * ((c >> 4) & 3))


def en_pantalla():
    """cada color de la consola, como lo pone HISPASMS.HVP (paleta_haz)"""
    hv = paleta_hv()
    r = []
    for n in range(64):
        c = color_sms(n)
        mejor = min(range(256), key=lambda j: (sum((a - b) ** 2 for a, b in zip(c, hv[j])), j))
        r.append(hv[mejor])
    return r


def lee_pgm(ruta):
    d = (gzip.open if ruta.endswith('.gz') else open)(ruta, 'rb').read()
    m = re.match(rb'P5\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    if a[0] == 'pantalla':
        cap, x, y, esc, esp, nombre = a[1], int(a[2]), int(a[3]), int(a[4]), a[5], a[6]
        zona = [int(v) for v in a[7:11]] if len(a) >= 11 else [0, 0, 256, ALTO]
        try:
            w, h, d = lee_ppm(cap)
        except (OSError, AttributeError):
            fin(False, '%s: no hay captura' % nombre)
        _, _, e = lee_pgm(esp)
        col = en_pantalla()
        mal, primero = 0, None
        for ny in range(zona[1], zona[1] + zona[3]):
            for nx in range(zona[0], zona[0] + zona[2]):
                quiero = col[e[ny * 256 + nx] & 63]
                for dy in range(esc):
                    for dx in range(esc):
                        px, py = x + nx * esc + dx, y + ny * esc + dy
                        o = (py * w + px) * 3
                        hay = tuple(d[o:o + 3]) if px < w and py < h else (-1, -1, -1)
                        if len(hay) < 3 or any(abs(p - q) > MARGEN for p, q in zip(hay, quiero)):
                            mal += 1
                            if primero is None:
                                primero = (px, py, hay, quiero)
        if mal:
            fin(False, '%s: %d puntos distintos (el primero, en %d,%d: sale %s y se esperaba %s)'
                % (nombre, mal, primero[0], primero[1], '%02X%02X%02X' % tuple(max(0, v) for v in primero[2]),
                   '%02X%02X%02X' % primero[3]))
        fin(True, '%s (%d puntos de la consola, x%d)' % (nombre, zona[2] * zona[3], esc))
    elif a[0] == 'iguales':
        try:
            p, q = lee_pgm(a[1])[2], lee_pgm(a[2])[2]
        except OSError as err:
            fin(False, '%s: %s' % (a[3], err))
        n = sum(1 for i in range(min(len(p), len(q))) if p[i] != q[i])
        fin(n == 0 and len(p) == len(q), a[3] + ('' if n == 0 else ': %d puntos distintos' % n))
    elif a[0] == 'guion':
        d = bytearray([0] * 60)
        for t in a[2:]:
            if t == 'spc':
                d += bytes(30)
            else:
                d += bytes([TECLAS[t]]) * 6 + bytes(6)
        open(a[1], 'wb').write(d)
        print(len(d))
    elif a[0] == 'suma':
        if a[1].endswith('.pgm') or a[1].endswith('.gz'):
            print(suma(lee_pgm(a[1])[2]))
        else:
            d = open(a[1], 'rb').read()
            print(suma(d[int(a[2], 0):int(a[2], 0) + int(a[3], 0)] if len(a) > 3 else d))
    elif a[0] == 'png':
        w, h, d = lee_pgm(a[1])
        with open(a[2], 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (w, h))
            f.write(b''.join(bytes(color_sms(c & 63)) for c in d[:w * h]))
    elif a[0] == 'resultado':
        m = open(a[1], 'rb').read()[0x1F00 if a[2:] == ['ram'] else 0x2000:]
        if m[1:4] == b'\xde\xb0\x61':
            t = m[4:m.index(0, 4)].decode('latin-1').strip().split('\n')
            print('%d %s' % (m[0], t[-1]))
            if len(t) > 1:
                print('  (fallan: %s)' % ', '.join(t[:-1]), file=sys.stderr)
        else:
            print('255 (sin resultado)')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
