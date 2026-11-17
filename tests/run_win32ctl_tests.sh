#!/bin/bash
# Prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md), parte 2: los
# controles de serie de Windows (BUTTON, EDIT, STATIC, LISTBOX y COMBOBOX)
# como ventanas hijas, sobre los controles de Hispa Ventanas. WINCTL.EXE
# (tests/progs/winctl.c) escribe en un rótulo lo que le llega. Se mira en
# capturas, manejándolo con el ratón y el teclado:
#  - los controles salen con sus textos (CreateWindowExA con las clases de
#    serie, LB_ADDSTRING, CB_ADDSTRING, SetDlgItemInt, CheckRadioButton);
#  - escribir en la caja de texto: EN_CHANGE con su HWND y GetWindowTextLengthA;
#  - Tab pasa el foco al botón y el espacio lo pulsa: BN_CLICKED por
#    WM_COMMAND, GetDlgItemTextA y SetWindowTextA en un STATIC;
#  - la casilla y las opciones (IsDlgButtonChecked);
#  - la lista: LBN_SELCHANGE, LB_GETCURSEL, LB_GETTEXT y el doble clic;
#  - la lista desplegable: se abre, se elige y avisa (CBN_SELCHANGE);
#  - EnableWindow: un botón desactivado no atiende, y vuelve a atender;
#  - GetDlgItemInt y SetDlgItemInt; lo que el control con el foco no quiere
#    (un carácter) sube a la ventana, que lee la caja de varias líneas (con
#    sus CR LF) y dice quién tiene el foco (GetFocus, GetDlgCtrlID);
#  - al cerrar no queda memoria.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 standard controls (BUTTON, EDIT, STATIC, LISTBOX, COMBOBOX) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINCTL.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: los controles de Win32: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: los controles de serie de Win32 dentro de Hispa Ventanas =="
imagen WINCTL.EXE
# La ventana, en 40,40; su zona, en 42,65. Los rótulos de abajo (crema):
# el saludo, el estado y los cambios.
Z="45 215 530 290"
{ sleep 6; send 'VENTANAS' 9
  tecla right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5       # la memoria antes de nada
  tecla left ret; sleep 6; aparca; shot c1                                # la ventana y sus controles
  tecla x y; sleep 1; shot c2                                               # escribir en la caja (tiene el foco)
  tecla tab; sleep 0.5; tecla spc; sleep 1; shot c3                         # Tab: al botón; espacio: lo pulsa
  tecla tab tab; sleep 0.5; tecla spc; sleep 1; shot c4                     # la casilla, con el teclado
  clic 260 119; sleep 0.6; clic 340 87; aparca; shot c5                     # la opción «Dos» y Saludar, con el ratón
  clic 80 170; aparca; shot c6                                              # la lista: Verde
  doble 80 186; aparca; shot c7                                             # y doble clic en Azul
  clic 300 153; sleep 1.5; aparca; shot c8                                  # la lista desplegable, abierta
  clic 250 218; sleep 1; aparca; shot c9                                    # se elige «Grande»
  clic 440 87; sleep 1; clic 200 119; sleep 0.6; clic 340 87; aparca; shot c10   # Desactivar; «Uno»; Saludar no atiende
  clic 440 87; sleep 1; clic 340 87; aparca; shot c11                       # Activar; ahora sí
  clic 430 116; sleep 1; aparca; shot c12                                   # Doble: 21 -> 42
  tecla shift-1; sleep 1; shot c13                                          # un «!»: sube a la ventana
  clic 511 51; sleep 2; aparca; shot x1                                     # la X
  tecla right ret; sleep 5; aparca; shot m1
  mon quit; } | TMAX=200 run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- los controles, con sus textos --"
chk $T/c1.ppm titulo 40 40 activa "Controles de Win32" busca 45 68 130 100 0 8 "Nombre:" busca 120 68 290 100 0 9 "ispa" \
  busca 290 68 400 100 0 6 "Saludar" busca 390 68 530 100 0 6 "Desactivar" busca 60 105 200 135 0 8 "Mayúsculas" \
  busca 180 105 250 135 0 8 "Uno" busca 250 105 330 135 0 8 "Dos" busca 330 105 400 135 0 9 "21" \
  busca 45 140 200 215 0 9 "Rojo" busca 45 140 200 215 0 9 "Amarillo" busca 200 140 340 170 0 9 "Mediano" \
  busca 350 140 500 215 0 9 "una" busca 350 140 500 215 0 9 "dos" busca $Z 0 8 "Listo."
echo "-- el teclado: escribir, Tab y el espacio --"
chk $T/c2.ppm busca 120 68 290 100 0 9 "ispa" busca $Z 0 8 "Cambios: 2; la caja 101: 7 letras, 1 líneas"
chk $T/c3.ppm busca $Z 0 8 "Hola, xyHispa (7 letras, opción 1)"
chk $T/c4.ppm busca $Z 0 8 "Casilla 1, opciones 1 y 0"
echo "-- el ratón: opciones, botones, la lista y la lista desplegable --"
chk $T/c5.ppm busca $Z 0 8 "Hola, XYHISPA (7 letras, opción 2)" busca $Z 0 8 "Casilla 1, opciones 0 y 1"
chk $T/c6.ppm busca $Z 0 8 "Lista: Verde (1 de 4)"
chk $T/c7.ppm busca $Z 0 8 "Lista: Azul (2 de 4) doble"
chk $T/c8.ppm busca 200 165 340 235 0 8 "Pequeño" busca 200 165 340 235 0 8 "Mediano"
chk $T/c9.ppm busca $Z 0 8 "Combo: Grande (2 de 3) Grande" nobusca 200 165 340 235 0 8 "Pequeño"
echo "-- EnableWindow, GetDlgItemInt y lo que sube a la ventana --"
chk $T/c10.ppm busca $Z 0 8 "Hola, XYHISPA (7 letras, opción 2)" busca 390 68 530 100 0 6 "Activar" \
  busca $Z 0 8 "Casilla 1, opciones 1 y 0"
chk $T/c11.ppm busca $Z 0 8 "Hola, XYHISPA (7 letras, opción 1)" busca $Z 0 8 "Saludar: activo (botón 109)" \
  busca 390 68 530 100 0 6 "Desactivar"
chk $T/c12.ppm busca 330 105 400 135 0 9 "42" busca $Z 0 8 "Cambios: 3; la caja 112: 2 letras, 1 líneas"
chk $T/c13.ppm busca $Z 0 8 "Varias: [una<>dos] foco 113"
echo "-- al cerrar no queda memoria --"
chk $T/x1.ppm titulo 60 330 activa "Win32" nobusca 45 215 530 290 0 8 "Varias: [una<>dos] foco 113"
chk $T/m1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
fin
