#!/bin/bash
# Prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md), partes 3 y 4:
# los menús de Windows sobre la barra de menús de Hispa Ventanas, y los
# recursos (.rsrc) y los diálogos de plantilla. WINMENU.EXE
# (tests/progs/winmenu.c y winmenu.rc) escribe lo que pasa en su ventana. Se
# mira en capturas, con el teclado y con el ratón:
#  - la barra hecha con CreateMenu, CreatePopupMenu y AppendMenuA (y puesta en
#    CreateWindowExA): Alt+letra la abre, y elegir da WM_COMMAND;
#  - CheckMenuItem pone la marca (y GetMenuState la dice); la orden en gris
#    (MF_GRAYED) no se elige hasta que EnableMenuItem la activa;
#  - una orden suelta en la barra; SetMenu con otro menú, y con ninguno (la
#    zona de la ventana crece);
#  - los recursos: LoadMenuA (por nombre, con su marca), LoadStringA (dos
#    bloques de textos, con acentos), LoadIconA (el que está y el que no) y
#    FindResourceA, LoadResource, LockResource y SizeofResource;
#  - DialogBoxParamA: el diálogo modal sale centrado con sus controles;
#    WM_INITDIALOG los rellena; Intro es el botón por defecto y Esc cancela;
#    EndDialog da el resultado; mientras, su dueña no atiende;
#  - CreateDialogParamA: el diálogo que no es modal convive con la ventana;
#  - al cerrar no queda memoria.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 menus, resources and template dialogs test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINMENU.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: los menús y los recursos de Win32: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: los menús, los recursos y los diálogos de Win32 dentro de Hispa Ventanas =="
imagen WINMENU.EXE
# La ventana, en 40,40 (500x170); su barra de menús, en y = 65; su zona, en
# 42,85. Lo que escribe: Z. Los desplegables salen debajo de la barra: D.
Z="45 88 540 190"
BARRA="40 62 300 86"
D="40 84 300 200"
Z2="45 68 540 190"         # (sin barra de menús, la zona empieza 20 puntos antes)
{ sleep 6; send 'VENTANAS' 9
  tecla right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5       # la memoria antes de nada
  tecla left ret; sleep 6; aparca; shot n1                                # la ventana con su barra
  tecla alt-a; sleep 1.5; shot n2                                           # «Archivo», abierto
  tecla ret; sleep 1; shot n3                                               # Contar
  tecla alt-a; sleep 1; tecla g; sleep 1; shot n3b                          # la de gris no se elige
  tecla esc; sleep 0.5; tecla alt-a; sleep 1; tecla m; sleep 1; tecla alt-a; sleep 1.5; shot n4   # Marcada: se ve la marca
  tecla esc; sleep 0.5; tecla alt-v; sleep 1; tecla a; sleep 1; tecla alt-a; sleep 1; tecla g; sleep 1; shot n5   # se activa la gris, y se elige
  clic 190 75; sleep 1.5; clic 180 98; sleep 2; aparca; shot a1             # «Acerca» (una orden en la barra): su diálogo
  clic 100 150; sleep 1; shot a2                                            # (la dueña no atiende)
  tecla ret; sleep 1.5; aparca; shot a3
  clic 135 75; sleep 1.5; clic 150 118; sleep 1.5; aparca; shot r1          # Ver, «Menú de los recursos», con el ratón
  tecla alt-r; sleep 1.5; shot r2
  tecla m; sleep 2; shot d1                                                 # el diálogo modal
  tecla a b; sleep 0.5; tecla tab; sleep 0.3; tecla spc; sleep 0.5; shot d2 # se escribe, y se quita la casilla
  tecla ret; sleep 1.5; shot d3                                             # Intro: Aceptar
  tecla alt-r; sleep 1; tecla m; sleep 2; tecla esc; sleep 1.5; shot d4     # otra vez: Esc cancela
  tecla alt-r; sleep 1; tecla m; sleep 2; clic 330 352; sleep 1.5; aparca; shot d5   # y Aceptar con el ratón
  tecla alt-r; sleep 1; tecla a; sleep 2; shot p1                           # el que no es modal
  tecla spc; sleep 1; shot p2                                               # su botón «Sumar»
  clic 300 180; sleep 1; tecla alt-r; sleep 1; tecla t; sleep 1; shot t1    # la ventana sigue: un texto
  tecla alt-r; sleep 1; tecla t; sleep 1; shot t2                           # y otro (del segundo bloque)
  clic 450 297; sleep 1.5; aparca; shot p3                                  # «Cerrar» lo destruye
  clic 300 180; sleep 1; tecla alt-r; sleep 1; tecla n; sleep 1.5; shot r3  # el menú de antes
  clic 135 75; sleep 1.5; clic 150 138; sleep 1.5; aparca; shot s1          # «Sin menú»
  clic 528 51; sleep 2; aparca; shot x1                                     # la X
  tecla right ret; sleep 5; aparca; shot m1
  mon quit; } | TMAX=240 run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- la barra de menús hecha con CreateMenu y AppendMenuA --"
chk $T/n1.ppm titulo 40 40 activa "Menús y recursos" busca $BARRA 0 15 "rchivo" busca $BARRA 0 15 "er" \
  busca $BARRA 0 15 "erca" busca $Z $N $B_ "Orden: 0  Cuenta: 0  Sumas: 0" \
  busca $Z $N $B_ "Marca: 0 (estado 0)  Gris: 1  Archivo: 5" busca $Z $N $B_ "Zona: 123"
chk $T/n2.ppm busca $D 0 8 "arcada" busca $D 7 8 "ris" busca $D 0 8 "alir" nobusca $D 0 8 "ris"
chk $T/n3.ppm busca $Z $N $B_ "Orden: 101  Cuenta: 1  Sumas: 0"
chk $T/n3b.ppm busca $D 7 8 "ris" busca $D 0 8 "alir"       # (sigue abierto: si se hubiera elegido, luego habría 2001)
echo "-- CheckMenuItem y EnableMenuItem --"
chk $T/n4.ppm busca $D 0 8 "* " busca $D 0 8 "arcada" busca $D 7 8 "ris"
chk $T/n5.ppm busca $Z $N $B_ "Orden: 103  Cuenta: 1001  Sumas: 0" busca $Z $N $B_ "Marca: 1 (estado 8)  Gris: 0"
echo "-- una orden en la barra y un diálogo de plantilla (DialogBoxParamA) --"
chk $T/a1.ppm titulo 248 226 activa "Acerca de" busca 250 250 550 300 0 8 "Menús y recursos de Win32," \
  busca 250 250 550 300 0 8 "dentro de Hispa Ventanas." busca 300 320 500 370 0 6 "Aceptar" \
  titulo 40 40 no "Menús y recursos"
chk $T/a2.ppm titulo 248 226 activa "Acerca de" titulo 40 40 no "Menús y recursos"
chk $T/a3.ppm titulo 40 40 activa "Menús y recursos" busca $Z $N $B_ "Diálogo: 101 «Hispa»" \
  nobusca 250 250 550 300 0 8 "dentro de Hispa Ventanas."
echo "-- los recursos: LoadMenuA, LoadIconA y FindResourceA --"
chk $T/r1.ppm busca $BARRA 0 15 "ecursos" nobusca $BARRA 0 15 "rchivo" busca $Z $N $B_ "Orden: 106" \
  busca $Z $N $B_ "Icono: 1 0  Menú 1: 16 octetos, 0  Zona: 123" busca $Z $N $B_ "Archivo: 5"
chk $T/r2.ppm busca $D 0 8 "  Diálogo " busca $D 0 8 "parte" busca $D 0 8 "* Menú de a"
echo "-- el diálogo modal: WM_INITDIALOG, sus controles, Intro, Esc y EndDialog --"
chk $T/d1.ppm titulo 228 210 activa "Datos" busca 230 235 580 280 0 8 "Nombre:" busca 320 235 580 280 0 9 "ispa" \
  busca 230 280 580 320 0 8 "Con saludo" busca 250 330 580 380 0 6 "Aceptar" busca 250 330 580 380 0 6 "Cancelar"
chk $T/d2.ppm busca 320 235 580 280 0 9 "ispa" titulo 228 210 activa "Datos"
chk $T/d3.ppm titulo 40 40 activa "Menús y recursos" busca $Z $N $B_ "Diálogo: 1 «abHispa»"
chk $T/d4.ppm titulo 40 40 activa "Menús y recursos" busca $Z $N $B_ "Diálogo: 0 «abHispa»"
chk $T/d5.ppm titulo 40 40 activa "Menús y recursos" busca $Z $N $B_ "Diálogo: 11 «abHispa»"
echo "-- el diálogo que no es modal (CreateDialogParamA) y LoadStringA --"
chk $T/p1.ppm titulo 288 242 activa "Aparte" busca 290 265 520 310 0 6 "Sumar" busca 290 265 520 310 0 6 "Cerrar" \
  busca 290 310 520 350 0 8 "no modal"
chk $T/p2.ppm busca $Z $N $B_ "Orden: 202  Cuenta: 1001  Sumas: 1"
chk $T/t1.ppm titulo 40 40 activa "Menús y recursos" busca $Z $N $B_ "Texto: El diecisiete: ñandú, camión" \
  titulo 288 242 no "Aparte"
chk $T/t2.ppm busca $Z $N $B_ "Texto: Hispa-486 lee sus textos"
chk $T/p3.ppm nobusca 290 265 520 310 0 6 "Sumar" busca $Z $N $B_ "Sumas: 1"
echo "-- SetMenu: el menú de antes, y ninguno --"
chk $T/r3.ppm busca $BARRA 0 15 "rchivo" nobusca $BARRA 0 15 "ecursos" busca $Z $N $B_ "Orden: 204"
chk $T/s1.ppm nobusca $BARRA 0 15 "rchivo" busca $Z2 $N $B_ "Orden: 107" busca $Z2 $N $B_ "Zona: 143"
echo "-- al cerrar no queda memoria --"
chk $T/x1.ppm titulo 60 330 activa "Win32" nobusca $Z2 $N $B_ "Orden: 107"
chk $T/m1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
fin
