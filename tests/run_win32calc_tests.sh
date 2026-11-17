#!/bin/bash
# Prueba final de la fase 3c de Hispa-486 (docs/486/HISPA-486.md): una
# calculadora sencilla de Windows, hecha en C con MSVCRT.DLL, dentro de Hispa
# Ventanas. WINCALC.EXE (tests/progs/wincalc.c y wincalc.rc) lleva todo lo de
# la fase: KERNEL32 y MSVCRT (la coma flotante: strtod, sprintf, sqrt), los
# controles de serie (botones y una caja de texto), el menú (de los recursos,
# con una orden que se marca), un diálogo de plantilla y los textos de
# LoadStringA. Se maneja con el ratón y con el teclado y se miran los
# resultados en capturas:
#  - con el ratón: 78 + 5 = 83;
#  - con el teclado (las teclas suben a la ventana desde el botón con el
#    foco; Intro pulsa «=»; Esc borra): 12 × 3 = 36;
#  - escribiendo en la caja de texto: 364 / 4 = 91;
#  - el menú: «Dos decimales» se marca y cambia lo que se ve; «Raíz cuadrada»;
#  - dividir por cero: un MessageBoxA con el texto de los recursos, y «Error»;
#  - con decimales, por el teclado: 2,5 + 1 = 3,50;
#  - «Acerca de»: el diálogo de plantilla, con su texto hecho con sprintf;
#  - «Salir» y no queda memoria.
#  (GUARDAR=carpeta deja las capturas.)
# Final phase 3c test: a simple Windows calculator in C with MSVCRT.DLL.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
make -s build/WINCALC.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: la calculadora de Win32: sin gcc de 32 bits, se salta =="; exit 0; }
. tests/tools/win32_ventanas.sh
echo "== Hispa-486: una calculadora de Windows (C y MSVCRT) dentro de Hispa Ventanas =="
imagen WINCALC.EXE
# La ventana, en 60,50 (246x226); la barra de menús, en y = 75; su zona, en
# 62,95; la caja, en 72,103; los botones, en cuatro columnas (x = 97, 154,
# 211 y 268) y cuatro filas (y = 149, 183, 217 y 251): 789/ 456* 123- 0.=+
CAJA="68 98 256 132"
b() { local t; for t in "$@"; do case $t in
  7) clic 97 149;; 8) clic 154 149;; 9) clic 211 149;; /) clic 268 149;;
  4) clic 97 183;; 5) clic 154 183;; 6) clic 211 183;; x) clic 268 183;;
  1) clic 97 217;; 2) clic 154 217;; 3) clic 211 217;; -) clic 268 217;;
  0) clic 97 251;; .) clic 154 251;; =) clic 211 251;; +) clic 268 251;;
  esac; done; }
{ sleep 6; send 'VENTANAS' 9
  tecla right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5       # la memoria antes de nada
  tecla left ret; sleep 6; aparca; shot k1                                # la calculadora
  b 7 8 + 5; aparca; shot k2; b =; aparca; shot k3                          # con el ratón: 78 + 5 = 83
  tecla esc; sleep 0.5; shot k4                                             # Esc: borra
  tecla 1 2 kp_multiply 3; sleep 0.5; shot k5; tecla ret; sleep 1; shot k6  # con el teclado: 12 × 3 = 36
  clic 240 115; sleep 0.5; tecla 4; sleep 0.5; shot k7                      # se escribe en la caja: 364
  b / 4 =; aparca; shot k8                                                  # 364 / 4 = 91
  tecla alt-v; sleep 1; tecla d; sleep 1; shot v1                           # Ver, «Dos decimales»
  tecla alt-v; sleep 1.5; shot v2; tecla esc; sleep 0.5                     # y lleva su marca
  tecla alt-e; sleep 1; tecla r; sleep 1; shot v3                           # Edición, «Raíz cuadrada»
  b 5 / 0 =; sleep 1; aparca; shot z1                                       # dividir por cero: el aviso
  tecla ret; sleep 1.5; shot z2
  tecla 2 dot 5 kp_add 1; sleep 0.5; tecla ret; sleep 1; shot k9     # 2,5 + 1 = 3,50
  tecla alt-y; sleep 1; tecla a; sleep 2; shot a1                           # Ayuda, «Acerca de...»
  tecla ret; sleep 1.5; shot a2
  tecla alt-a; sleep 1; tecla s; sleep 2; aparca; shot x1                   # Archivo, «Salir»
  tecla right ret; sleep 5; aparca; shot m1
  mon quit; } | TMAX=240 run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- la calculadora: su menú (de los recursos), su caja y sus botones --"
chk $T/k1.ppm titulo 60 50 activa "Calculadora" busca 60 72 310 96 0 15 "rchivo" busca 60 72 310 96 0 15 "dición" \
  busca 60 72 310 96 0 15 "er" busca 60 72 310 96 0 15 "uda" busca $CAJA 0 9 "0" \
  busca 70 130 130 165 0 6 "7" busca 240 130 300 165 0 6 "/" busca 240 235 300 270 0 6 "+" busca 250 98 300 132 0 6 "C"
echo "-- con el ratón --"
chk $T/k2.ppm busca $CAJA 0 9 "5" nobusca $CAJA 0 9 "78"
chk $T/k3.ppm busca $CAJA 0 9 "83"
echo "-- con el teclado --"
chk $T/k4.ppm busca $CAJA 0 9 "0" nobusca $CAJA 0 9 "83"
chk $T/k5.ppm busca $CAJA 0 9 "3" nobusca $CAJA 0 9 "12"
chk $T/k6.ppm busca $CAJA 0 9 "36"
echo "-- escribiendo en la caja --"
chk $T/k7.ppm busca $CAJA 0 9 "36" nobusca $CAJA 0 9 "36 "
chk $T/k8.ppm busca $CAJA 0 9 "91" nobusca $CAJA 0 9 "364"
echo "-- el menú: una orden que se marca, y la raíz cuadrada --"
chk $T/v1.ppm busca $CAJA 0 9 "91.00"
chk $T/v2.ppm busca 200 90 420 130 9 12 "* " busca 200 90 420 130 9 12 "os decimales"    # (marcada: blanco sobre añil)
chk $T/v3.ppm busca $CAJA 0 9 "9.54"
echo "-- dividir por cero --"
chk $T/z1.ppm busca 200 240 600 320 0 8 "No se puede dividir por cero." titulo 60 50 no "Calculadora"
chk $T/z2.ppm titulo 60 50 activa "Calculadora" busca $CAJA 0 9 "Error"
chk $T/k9.ppm busca $CAJA 0 9 "3.50"
echo "-- «Acerca de»: un diálogo de plantilla --"
chk $T/a1.ppm titulo 248 224 activa "Acerca de la calculadora" busca 250 250 560 300 0 8 "Calculadora de Hispa-486." \
  busca 250 250 560 300 0 8 "Cuentas hechas: 4 (y 3.5)" busca 300 320 500 370 0 6 "Aceptar"
chk $T/a2.ppm titulo 60 50 activa "Calculadora" busca $CAJA 0 9 "3.50"
echo "-- «Salir» y no queda memoria --"
chk $T/x1.ppm titulo 60 330 activa "Win32" nobusca $CAJA 0 9 "3.50"
chk $T/m1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
fin
