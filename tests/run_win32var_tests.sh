#!/bin/bash
# Prueba de la fase 3d de Hispa-486 (docs/486/HISPA-486.md): lo de USER32 y
# COMDLG32 de los programas de Windows 95/98 además de pintar
# (ventanas/win32x.inc). WINVAR.EXE (tests/progs/winvar.c y winvar.rc), en el
# escritorio de Hispa-486 (VENTANAS /486, con la barra de tareas):
#  - su icono (de los recursos, el de su clase) en su botón de la barra, y
#    dibujado con DrawIcon y DrawIconEx; el de serie; un mapa de los recursos;
#  - el cursor de su clase (de los recursos) sobre su zona;
#  - la barra de desplazamiento (WS_VSCROLL): sus flechas mueven las líneas;
#  - el caret detrás de lo que se escribe;
#  - los aceleradores de los recursos: F5 y Mayús+F5 (teclas virtuales con y
#    sin Mayús.), Ctrl+C y Ctrl+V (el portapapeles), Ctrl+L (un carácter de
#    control: TrackPopupMenu, que devuelve la orden) y Ctrl+O (GetOpenFileNameA,
#    con el diálogo de archivos de Hispa Ventanas);
#  - del menú, ChooseColorA y ChooseFontA.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 USER32/COMDLG32 extras (phase 3d) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINVAR.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: USER32 y COMDLG32 de la fase 3d: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: barras, caret, aceleradores, iconos, portapapeles y diálogos comunes =="
imagen WINVAR.EXE
# La ventana, en 20,40 (600x400); su zona empieza en 22,86 (la x y la y de lo
# que pinta, más eso). La barra vertical, a la derecha: su flecha de abajo en
# 610,430 y la pista, debajo del pulgar, en 610,300. El botón de la barra de
# tareas, desde x = 88 (su icono, en 93..108, 578..593).
Z="22 86 300 300"
{ sleep 6; send 'VENTANAS /486' 10
  tecla ctrl-esc; sleep 1; tecla right; sleep 0.8; tecla ret; sleep 8; ir 200 200; shot v1
  tecla a b c; sleep 1; ir 200 200; shot v2
  tecla ctrl-c; sleep 0.5; tecla ctrl-v; sleep 1; shot v3
  tecla f5 f5 shift-f5; sleep 1; shot v4
  tecla ctrl-l; sleep 1.5; shot v5; tecla down down ret; sleep 1.5; shot v6
  clic 610 430; clic 610 430; clic 610 430; sleep 1; ir 200 200; shot s1     # las flechas de la barra
  clic 610 300; sleep 1; ir 200 200; shot s2                              # la pista: una página
  tecla ctrl-o; sleep 2; shot v7; tecla w i n v a r dot e x e; sleep 0.5; tecla ret; sleep 1.5; shot v8
  tecla alt-v; sleep 0.8; tecla c; sleep 1.5; shot v9; tecla up up up up up up; sleep 0.5; tecla ret; sleep 1.5; shot v10
  tecla alt-v; sleep 0.8; tecla l; sleep 1.5; shot v11; tecla down down down; sleep 0.5; tecla ret; sleep 1.5; shot v12
  tecla alt-a; sleep 0.8; tecla s; sleep 2; shot v13
  mon quit; } | TMAX=240 run
echo "-- la ventana: su icono (de los recursos) dibujado y en la barra de tareas, el de serie y un mapa --"
chk $T/v1.ppm titulo 20 40 activa "Varios de USER32" busca $Z 232 231 "Línea 1" busca $Z 232 231 "Línea 10" \
  rgb 338 110 FF0000 "DrawIcon: el rojo de dentro" rgb 338 97 0000FF "DrawIcon: el borde azul" \
  rgb 323 95 FFFFFF "DrawIcon: lo transparente" rgb 434 126 FF0000 "DrawIconEx a 64x64" \
  rgb 323 170 00FF00 "LoadBitmapA: verde" rgb 327 170 FFFF00 "LoadBitmapA: amarillo" \
  px 100 586 4 "el icono en su botón de la barra de tareas (a 16 colores)" \
  rgb 203 203 000000 "el cursor de su clase (de los recursos)" \
  rgb 86 290 000000 "el caret" rgb 88 290 FFFFFF "(solo dos puntos de ancho)"
echo "-- se escribe; el portapapeles; los aceleradores --"
chk $T/v2.ppm busca 22 280 300 310 232 231 "Texto: abc" rgb 110 290 000000 "el caret, detrás"
chk $T/v3.ppm busca 22 300 500 330 232 231 "Pegado: abc"
chk $T/v4.ppm busca 22 300 500 330 232 231 "F5: 2  Mayús+F5: 1"
chk $T/v5.ppm busca 150 150 450 300 0 8 "Segunda"
chk $T/v6.ppm busca 22 300 500 330 232 231 "Menú: 2002"
echo "-- la barra de desplazamiento --"
chk $T/s1.ppm busca 22 86 300 106 232 231 "Línea 4" nobusca 22 86 300 106 232 231 "Línea 1 "
chk $T/s2.ppm busca 22 86 300 106 232 231 "Línea 14"
echo "-- GetOpenFileNameA, ChooseColorA y ChooseFontA --"
chk $T/v7.ppm busca 100 140 700 400 0 8 "Nombre del archivo:" busca 100 140 700 420 0 9 "[BIN]"
chk $T/v8.ppm busca 22 320 500 350 232 231 "Archivo: A:\\winvar.exe  (0)"
chk $T/v9.ppm busca 100 140 700 420 0 6 "Aceptar" busca 100 140 700 420 9 12 "Blanco"
chk $T/v10.ppm rgb 332 216 FF0000 "el color elegido (rojo)"
chk $T/v12.ppm cuenta 22 354 300 30 000000 200 4000 "la letra elegida (14 puntos)"
fin
