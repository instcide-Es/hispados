#!/bin/bash
# Prueba de la fase 3d de Hispa-486 (docs/486/HISPA-486.md): el GDI de los
# programas de Windows 95/98 dentro de Hispa Ventanas (ventanas/win32g.inc,
# win32t.inc, win32b.inc, win32r.inc y win32ga.inc). WINGDI.EXE
# (tests/progs/wingdi.c) pinta una cuadrícula de figuras, letras, mapas de
# bits, DIB y recortes, y comprueba valores (GetTextMetricsA, CombineRgn,
# GetPixel, GetDIBits, las paletas...); aquí se miran los puntos de la captura.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 GDI (phase 3d) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINGDI.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: el GDI de Win32: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: el GDI de los programas de Windows (figuras, letras, mapas, regiones) =="
imagen WINGDI.EXE
{ sleep 6; send 'VENTANAS' 9
  doble 125 408; sleep 8; aparca; shot g1
  mon quit; } | TMAX=150 run
# La ventana, en 10,30; su zona empieza en 12,55. Las casillas, de 124 × 112
# desde 8,8 de la zona (en la pantalla: x = 20, 144, 268, 392, 516, 640 e
# y = 63, 175, 287, 399).
chk $T/g1.ppm titulo 10 30 activa "GDI de Hispa-486" \
  busca 12 505 600 535 232 231 "Comprobaciones: 12 de 12 bien."
echo "-- fila 0: rectángulo con pluma ancha, elipse, esquinas redondas, queso, cuerda y arco --"
chk $T/g1.ppm rgb 70 103 FF0000 "Rectangle: el pincel" rgb 19 103 0000FF "Rectangle: la pluma de 4" \
  rgb 194 103 00FF00 "Ellipse: dentro" rgb 146 65 FFFFFF "Ellipse: fuera de la esquina" rgb 194 63 000000 "Ellipse: el borde" \
  rgb 318 103 FFFF00 "RoundRect: dentro" rgb 270 65 FFFFFF "RoundRect: la esquina, fuera" rgb 271 103 FFFF00 "RoundRect: el lado, dentro" \
  rgb 467 83 FF00FF "Pie: el cuarto de arriba a la derecha" rgb 417 123 FFFFFF "Pie: lo demás, sin pintar" \
  rgb 566 83 00FFFF "Chord: la mitad de arriba" rgb 566 123 FFFFFF "Chord: la de abajo, sin pintar" \
  cuenta 640 63 100 42 FF0000 40 300 "Arc: la mitad de arriba, a rayas" cuenta 640 106 100 38 FF0000 0 0 "Arc: abajo, nada"
echo "-- fila 1: polígonos, rayados, ROP2 y bordes --"
chk $T/g1.ppm rgb 70 220 FFFFFF "Polygon ALTERNATE: el centro, hueco" rgb 194 220 0000FF "Polygon WINDING: el centro, lleno" \
  rgb 70 185 0000FF "Polygon: una punta" \
  cuenta 268 175 33 33 0000FF 100 200 "HS_HORIZONTAL" cuenta 302 215 33 33 0000FF 200 350 "HS_CROSS" \
  rgb 560 195 636363 "R2_XORPEN dos veces: como estaba" rgb 560 205 9A9A9A "R2_XORPEN una vez" \
  rgb 590 230 9A9A9A "InvertRect" rgb 646 190 FFFFFF "DrawEdge elevado: la luz" rgb 684 190 000000 "DrawEdge elevado: la sombra" \
  rgb 645 235 FF0000 "FrameRect"
echo "-- fila 2: letras de otros tamaños y DrawTextA --"
chk $T/g1.ppm cuenta 20 303 64 16 000000 60 400 "Negrita (letra de -13, peso 700)" \
  cuenta 20 327 200 36 0000FF 300 3000 "Grande (32, cursiva y subrayada)" \
  busca 380 287 600 305 232 231 "Una frase larga que" busca 380 303 600 321 232 231 "DrawTextA parte en varias" \
  busca 380 319 600 337 232 231 "líneas."
echo "-- fila 3: mapas de bits, máscaras, DIB y recortes --"
chk $T/g1.ppm rgb 52 431 FFFF00 "BitBlt de un mapa en memoria" rgb 22 401 0000FF "BitBlt: el fondo" \
  rgb 194 449 FFFF00 "StretchBlt al doble" rgb 146 401 0000FF "StretchBlt: el fondo" \
  rgb 300 431 FF0000 "SRCAND y SRCPAINT: el dibujo" rgb 270 401 00FF00 "SRCAND y SRCPAINT: lo de detrás" \
  rgb 404 411 FF0000 "StretchDIBits: arriba a la izquierda" rgb 476 411 00FF00 "StretchDIBits: arriba a la derecha" \
  rgb 404 483 0000FF "StretchDIBits: abajo a la izquierda" rgb 476 483 FFFF00 "StretchDIBits: abajo a la derecha" \
  rgb 517 410 0000FF "CreateDIBSection: el primer color" rgb 578 410 FF0000 "CreateDIBSection: el último" \
  rgb 548 431 0000FF "CreateDIBSection: lo pintado con GDI" \
  rgb 690 439 FFFFFF "el recorte: el agujero (ExcludeClipRect)" rgb 660 439 FF0000 "el recorte: dentro de la elipse" \
  rgb 642 401 FFFFFF "el recorte: fuera de la elipse"
fin
