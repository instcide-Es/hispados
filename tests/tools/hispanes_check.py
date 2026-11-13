#!/usr/bin/env python3
"""Comprobaciones de Hispa-Nes para tests/run_hispanes_tests.sh.

  hispanes_check.py pantalla CAPTURA.ppm X Y ESCALA ESPERADO NOMBRE [X0 Y0 ANCHO ALTO]
      La imagen de la consola que hay en la captura de QEMU (su esquina en
      X,Y; cada punto, ESCALA x ESCALA) es la de ESPERADO: un PGM (o .pgm.gz)
      de 256x240 con los colores de la consola (0-63), como los que escribe
      nesnativo. Cada color se pasa al más parecido de la paleta de Hispa
      Ventanas, igual que hace HISPANES.HVP. Con X0 Y0 ANCHO ALTO, solo esa
      zona (en puntos de la consola).
  hispanes_check.py iguales A.pgm[.gz] B.pgm[.gz] NOMBRE
      Las dos imágenes son la misma.
  hispanes_check.py guion SALIDA TECLA...
      El guion de botones de nesnativo que corresponde a esas teclas en el
      modo paso a paso (/P): 60 cuadros al abrir; cada tecla, 6 cuadros con
      su botón y 6 sin él; «spc», 30 sin botones. Escribe cuántos cuadros son.
  hispanes_check.py suma ARCHIVO [DESDE CUANTOS]
      La suma (como la de HISPANES.LOG) de un PGM o de un trozo de archivo.
  hispanes_check.py color N
      El color de la consola N (hex) en RRGGBB, como sale en la pantalla.
Escribe «  OK   ...» o «  FAIL ...» y sale con 1 si falla.
"""
import gzip, os, re, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(AQUI, '..', '..', 'tools'))
sys.path.insert(0, AQUI)
from hispanes_paleta import PALETA
from hispanes_guion import guion
MARGEN = 10
TECLAS = {'ret': 'T', 'x': 'A', 'z': 'B', 's': 'S', 'up': 'U', 'down': 'D', 'left': 'L', 'right': 'R'}


def paleta_hv():
    """los 256 colores de Hispa Ventanas: los 16 propios, el cubo y los grises"""
    cols = []
    dentro = False
    for l in open(os.path.join(AQUI, '..', '..', 'ventanas', 'datos.inc'), encoding='utf-8'):
        if l.startswith('paleta16:'):
            dentro = True
        elif dentro:
            m = re.match(r'\s+db\s+0x(\w+),\s*0x(\w+),\s*0x(\w+)', l)
            if m:
                cols.append(tuple(int(x, 16) for x in m.groups()))
            elif cols:
                break
    assert len(cols) == 16
    cols += [(51 * r, 51 * g, 51 * b) for r in range(6) for g in range(6) for b in range(6)]
    cols += [(11 * i,) * 3 for i in range(24)]
    return cols


def en_pantalla():
    """cada color de la consola, como lo pone HISPANES.HVP (paleta_haz)"""
    hv = paleta_hv()
    r = []
    for c in PALETA:
        mejor = min(range(256), key=lambda j: (sum((a - b) ** 2 for a, b in zip(c, hv[j])), j))
        r.append(hv[mejor])
    return r


def lee_pgm(ruta):
    d = (gzip.open if ruta.endswith('.gz') else open)(ruta, 'rb').read()
    m = re.match(rb'P5\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]


def lee_ppm(ruta):
    d = open(ruta, 'rb').read()
    m = re.match(rb'P6\s+(\d+)\s+(\d+)\s+(\d+)\s', d)
    return int(m.group(1)), int(m.group(2)), d[m.end():]


def suma(datos):
    h = 0
    for b in datos:
        h = (((h << 5) | (h >> 27)) + b) & 0xFFFFFFFF
    return '%08X' % h


def fin(ok, texto):
    print(('  OK   ' if ok else '  FAIL ') + texto)
    sys.exit(0 if ok else 1)


def main():
    a = sys.argv[1:]
    if a[0] == 'pantalla':
        cap, x, y, esc, esp, nombre = a[1], int(a[2]), int(a[3]), int(a[4]), a[5], a[6]
        zona = [int(v) for v in a[7:11]] if len(a) >= 11 else [0, 0, 256, 240]
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
        pasos = ['60']
        for t in a[2:]:
            pasos += ['30'] if t == 'spc' else [TECLAS[t] + ':6', '6']
        d = guion(pasos)
        open(a[1], 'wb').write(d)
        print(len(d))
    elif a[0] == 'suma':
        if a[1].endswith('.pgm') or a[1].endswith('.gz'):
            print(suma(lee_pgm(a[1])[2]))
        else:
            d = open(a[1], 'rb').read()
            print(suma(d[int(a[2], 0):int(a[2], 0) + int(a[3], 0)] if len(a) > 3 else d))
    elif a[0] == 'color':
        print('%02X%02X%02X' % en_pantalla()[int(a[1], 16)])
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main()
