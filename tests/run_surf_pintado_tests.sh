#!/bin/bash
# Prueba del pintado del Navegador completo (NAVEGA2.EXE: NetSurf con lo de
# w32/surf), en el anfitrión, sin QEMU ni Internet: el mismo NetSurf
# compilado para Linux (build/anf/surfppm, w32/surf/anfitrion.c) pinta
# tests/surf/pagina.html en un PPM y se miran los puntos:
#  - @font-face: un icono de letra (U+E000, un cuadrado) con TTF, WOFF (que se
#    prefiere a WOFF2 si la hoja da a elegir) y WOFF2 (Brotli), con ::before;
#  - CSS grid: repeat(3, 1fr) con gap, y repeat(auto-fill, minmax(230px,
#    1fr)) con grid-column: 1 / -1;
#  - flexbox con justify-content: space-between;
#  - border-radius (la esquina, del fondo de la página) y box-shadow;
#  - un campo con min-height: 48px y box-sizing: border-box (48 de alto,
#    como Chrome) y su texto de ayuda (placeholder).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
echo "== Hispa-486: el pintado del Navegador completo (NetSurf en el anfitrión) =="
if ! make -s build/anf/surfppm > /tmp/surf_pintado_make.$$ 2>&1; then
  if grep -q "error" /tmp/surf_pintado_make.$$; then cat /tmp/surf_pintado_make.$$; rm -f /tmp/surf_pintado_make.$$; echo "  FAIL no compila surfppm"; exit 1; fi
  echo "  (no se puede compilar en el anfitrión: se salta)"; rm -f /tmp/surf_pintado_make.$$; exit 0
fi
rm -f /tmp/surf_pintado_make.$$
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
t0=$(date +%s%N)
if ! SURF_GALLETAS=$T/galletas build/anf/surfppm -h 520 -o $T/p.ppm tests/surf/pagina.html; then
  echo "  FAIL surfppm no pinta la página"; exit 1
fi
echo "  (pintada en $(( ($(date +%s%N) - t0) / 1000000 )) ms)"
python3 -I - $T/p.ppm <<'EOP'
import sys
d = open(sys.argv[1], 'rb').read()
t = d.split(b'\n', 3)
w, h = map(int, t[1].split())
px = t[3]
fallo = 0
def p(x, y):
    o = (y * w + x) * 3
    return tuple(px[o:o + 3])
def cerca(c, r, tol=40):
    return all(abs(a - b) <= tol for a, b in zip(c, r))
def chk(nombre, ok):
    global fallo
    print('  %s %s' % ('OK  ' if ok else 'FAIL', nombre))
    if not ok: fallo = 1
NEGRO, BLANCO, ROJO, AZUL, VERDE, MAG, CIAN = (0,0,0), (255,255,255), (255,0,0), (0,0,255), (0,170,0), (255,0,255), (0,255,255)
# @font-face: el cuadrado de U+E000, de x+4 a x+36 y de y=8 a y=40
chk('icono de letra TTF (@font-face, ::before)', cerca(p(20, 30), NEGRO) and cerca(p(20, 4), BLANCO))
chk('icono de letra WOFF', cerca(p(120, 30), NEGRO))
chk('icono de letra WOFF2 (Brotli)', cerca(p(220, 30), NEGRO))
chk('entre los iconos, nada', cerca(p(70, 30), BLANCO) and cerca(p(170, 30), BLANCO))
# grid: 3 columnas de (780 - 40) / 3 con 20 de hueco
chk('grid: tres columnas (1fr) en una fila', cerca(p(100, 85), ROJO) and cerca(p(390, 85), ROJO) and cerca(p(700, 85), ROJO))
chk('grid: el hueco (gap: 20px)', cerca(p(256, 85), BLANCO) and cerca(p(522, 85), BLANCO) and not cerca(p(240, 85), BLANCO))
# flex: space-between
chk('flex: justify-content: space-between', cerca(p(50, 125), AZUL) and cerca(p(390, 125), AZUL) and cerca(p(730, 125), AZUL) and cerca(p(200, 125), BLANCO) and cerca(p(600, 125), BLANCO))
# border-radius: la caja de y=150 a 250, x de 0 a 200, radio 30
chk('border-radius: la esquina, sin pintar', cerca(p(2, 152), BLANCO) and cerca(p(197, 247), BLANCO))
chk('border-radius: lo de dentro, pintado', cerca(p(100, 200), VERDE) and cerca(p(2, 200), VERDE) and cerca(p(30, 152), VERDE))
# box-shadow: 0 10px 0 negro bajo la caja (y de 260 a 310)
chk('box-shadow: la sombra bajo la caja', cerca(p(100, 315), NEGRO) and cerca(p(100, 285), (238, 238, 238)) and cerca(p(100, 325), BLANCO))
# el campo: borde magenta de 2; su alto, 48 (min-height con border-box)
filas = [y for y in range(330, 420) if cerca(p(150, y), MAG)]
alto = (filas[-1] - filas[0] + 1) if filas else 0
chk('campo con min-height: 48px y border-box: %d de alto' % alto, alto == 48)
gris = [x for x in range(12, 120) for y in range(filas[0] + 10, filas[0] + 38) if filas and 60 < p(x, y)[0] < 200] if filas else []
chk('texto de ayuda (placeholder) en gris', len(gris) > 30)
# grid auto-fill: 3 columnas de 230 o más; el último elemento, de lado a lado
yy = (filas[-1] if filas else 400) + 20
chk('grid auto-fill minmax(230px, 1fr): 3 por fila', cerca(p(100, yy), CIAN) and cerca(p(400, yy), CIAN) and cerca(p(700, yy), CIAN) and cerca(p(258, yy), BLANCO))
chk('grid-column: 1 / -1 (de lado a lado)', cerca(p(10, yy + 60), CIAN) and cerca(p(770, yy + 60), CIAN))
sys.exit(fallo)
EOP
r=$?
# position: fixed y sticky, con la página desplazada 300 puntos (-y): la
# cabecera fija y la barra lateral fija (de height: 100vh: hasta abajo) en su
# sitio; el sticky, pegado bajo la cabecera (la barra lateral de ClicGest se
# cortaba al desplazar)
if build/anf/surfppm -w 746 -h 420 -y 300 -o $T/f.ppm tests/surf/fijo.html > /dev/null 2>&1; then
  python3 -I - $T/f.ppm <<'EOP' || r=1
import sys
d = open(sys.argv[1], 'rb').read()
t = d.split(b'\n', 3)
w, h = map(int, t[1].split())
px = t[3]
def p(x, y):
    o = (y * w + x) * 3
    return tuple(px[o:o + 3])
def cerca(c, r, tol=40):
    return all(abs(a - b) <= tol for a, b in zip(c, r))
fallo = 0
def chk(nombre, ok):
    global fallo
    print('  %s %s' % ('OK  ' if ok else 'FAIL', nombre))
    if not ok: fallo = 1
chk('position: fixed desplazada: la cabecera, arriba', cerca(p(400, 20), (255, 0, 0)))
chk('position: fixed desplazada: la barra lateral, entera (100vh: de arriba abajo)', cerca(p(60, 5), (0, 0, 255)) and cerca(p(60, 419), (0, 0, 255)))
chk('position: sticky: pegado bajo la cabecera', cerca(p(400, 55), (0, 170, 0)) and cerca(p(400, 90), (255, 255, 255)))
sys.exit(fallo)
EOP
else echo "  FAIL surfppm no pinta tests/surf/fijo.html"; r=1; fi
# (navega 0.2.2) el modo teléfono de ClicGest (tests/surf/movil.html): la
# barra de abajo, fija con bottom: 0, abajo de la ventana; la hoja «Más» (un
# <dialog open> fijo con inset: auto 0 0 0 y max-height: 80dvh) pegada abajo,
# con su <details> cerrado; los clics, para la hoja de encima (también con la
# página desplazada) y no para los enlaces de debajo; el icono SVG en un data:
if build/anf/surfppm -w 600 -h 400 -o $T/m.ppm -enlace 100 380 -enlace 100 300 -enlace 100 50 tests/surf/movil.html > $T/m.txt 2>&1 \
   && build/anf/surfppm -w 600 -h 400 -y 300 -o $T/m2.ppm -enlace 100 380 -enlace 100 250 tests/surf/movil.html > $T/m2.txt 2>&1 \
   && build/anf/surfppm -w 600 -h 230 -o $T/m3.ppm tests/surf/movil.html > /dev/null 2>&1; then
  python3 -I - $T/m.ppm $T/m.txt $T/m2.txt $T/m3.ppm <<'EOP' || r=1
import sys
d = open(sys.argv[1], 'rb').read()
t = d.split(b'\n', 3)
w, h = map(int, t[1].split())
px = t[3]
def p(x, y):
    o = (y * w + x) * 3
    return tuple(px[o:o + 3])
def cerca(c, r, tol=12):
    return all(abs(a - b) <= tol for a, b in zip(c, r))
enl = {}
for f in sys.argv[2:4]:
    for l in open(f):
        if l.startswith('enlace '):
            _, x, y, u = l.split()
            enl[(f == sys.argv[3], int(x), int(y))] = u.split('/')[-1]
fallo = 0
def chk(nombre, ok):
    global fallo
    print('  %s %s' % ('OK  ' if ok else 'FAIL', nombre))
    if not ok: fallo = 1
chk('inset: auto 0 0 0: la hoja, pegada abajo (200 de alto: de 200 a 400)', cerca(p(500, 197), (221, 221, 221)) and cerca(p(500, 203), (255, 243, 196)) and cerca(p(500, 399), (192, 48, 106)))
d3 = open(sys.argv[4], 'rb').read().split(b'\n', 3)[3]
def p3(x, y):
    o = (y * w + x) * 3
    return tuple(d3[o:o + 3])
chk('max-height: 80dvh: con 230 de alto, la hoja mide 184 (de 46 abajo)', cerca(p3(500, 43), (221, 221, 221)) and cerca(p3(500, 49), (255, 243, 196)))
chk('<details> cerrado: se ve su <summary>, no lo de dentro (aunque sea display: grid)', cerca(p(300, 340), (255, 208, 208)) and not cerca(p(300, 300), (255, 208, 208)))
chk('el icono SVG (data:, trazos con rgba()), pintado', cerca(p(580, 16), (33, 37, 41), 30) and cerca(p(580, 24), (33, 37, 41), 30) and cerca(p(580, 20), (221, 221, 221)))
chk('el clic en la hoja de encima: «Ver versión de ordenador», no el enlace de debajo', enl.get((False, 100, 380)) == 'vista?modo=pc')
chk('el clic en la rejilla de la hoja: su enlace', enl.get((False, 100, 300)) == 'seccion4')
chk('el clic fuera de la hoja: el enlace de la página', enl.get((False, 100, 50)) == 'fondo0')
chk('con la página desplazada (300), lo fijo sigue llevándose el clic donde se ve', enl.get((True, 100, 380)) == 'vista?modo=pc' and enl.get((True, 100, 250)) == 'seccion2')
sys.exit(fallo)
EOP
else echo "  FAIL surfppm no pinta tests/surf/movil.html"; r=1; fi
[ -n "${GUARDAR:-}" ] && { mkdir -p "$GUARDAR"; python3 -I tests/tools/ppm2png.py $T/p.ppm "$GUARDAR"/pintado.png; }
exit $r
