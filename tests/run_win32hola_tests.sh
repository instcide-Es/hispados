#!/bin/bash
# Prueba de la fase 3b de Hispa-486 (docs/486/HISPA-486.md): el «Hola, mundo»
# de ventana de Win32 dentro de Hispa Ventanas (ventanas/win32u.inc).
# WINHOLA.EXE (tests/progs/winhola.c) registra una clase, crea su ventana y
# pinta en WM_PAINT. Se mira en capturas:
#  - la ventana sale con su título y su texto (TextOutA), el rectángulo rojo
#    con su borde y su raya y el azul con el texto centrado; su zona mide lo
#    que dice WM_SIZE; el temporizador vence; SendMessageA anidado en
#    WM_CREATE devuelve lo suyo;
#  - un clic cambia el número (WM_LBUTTONDOWN -> InvalidateRect -> WM_PAINT),
#    y un doble clic cuenta dos (la clase no tiene CS_DBLCLKS);
#  - las teclas: WM_CHAR con su WM_KEYUP, F1 por WM_KEYDOWN y la «t», que
#    cambia el título (GetWindowTextA y SetWindowTextA);
#  - el botón derecho abre un MessageBoxA cuya dueña es la ventana: mientras
#    está, la ventana no atiende clics;
#  - moverla y cambiarle el tamaño la repinta (WM_SIZE llega desde el sistema:
#    en diferido);
#  - la X la cierra (WM_CLOSE -> DefWindowProcA -> WM_DESTROY ->
#    PostQuitMessage) y la tarea termina sin dejar memoria;
#  - dos copias a la vez, cada una con su cuenta; el Administrador de tareas
#    las ve, le pide a una que cierre (y cierra) y termina a la fuerza la otra
#    (que tiene un cuadro abierto); la memoria queda como al principio.
# La fase 3a (tests/run_win32gui_tests.sh) se pasa aparte.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 «Hello, world» window inside Hispa Ventanas (phase 3b) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/WINHOLA.EXE > /dev/null 2>&1 || {
  echo "== Hispa-486: el «Hola» de ventana de Win32: sin gcc de 32 bits, se salta =="; exit 0; }
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.3; done; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
RX=400; RY=300
ir() {
  local dx=$(( $1 - RX )) dy=$(( $2 - RY )) sx sy
  while [ $dx -ne 0 ] || [ $dy -ne 0 ]; do
    sx=$dx; [ $sx -gt 60 ] && sx=60; [ $sx -lt -60 ] && sx=-60
    sy=$dy; [ $sy -gt 60 ] && sy=60; [ $sy -lt -60 ] && sy=-60
    mon "mouse_move $sx $sy" 0.07
    dx=$(( dx - sx )); dy=$(( dy - sy ))
  done
  RX=$1; RY=$2; sleep 0.3
}
clic() { ir $1 $2; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; }
clicder() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
doble() { ir $1 $2; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.6; }
arrastra() { ir $1 $2; mon "mouse_button 1" 0.4; ir $3 $4; mon "mouse_button 0" 0.8; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-170} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}

echo "== Hispa-486: el «Hola, mundo» de ventana de Win32 dentro de Hispa Ventanas =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/WINHOLA.EXE ::BIN/WINHOLA.EXE || ko "no cabe WINHOLA.EXE en el disco"
# un grupo «Win32», abierto, con los accesos de la prueba (tres iconos en
# fila: sus nombres, en x = 125, 229 y 333 de la pantalla, y = 371)
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Win32' 'Ventana=60,260,600,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Hola' 'Icono=4' 'Programa=A:\BIN\WINHOLA.EXE' \
  '[Acceso]' 'Nombre=Bloc' 'Icono=1' 'Programa=:bloc' \
  '[Acceso]' 'Nombre=Tareas' 'Icono=2' 'Programa=A:\HISPADOS\PROGS\TAREAS.HVP' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
cp $T/f.img $T/f0.img

aparca() { ir 16 590; }
MEMZ="180 418 450 54"      # la zona de la memoria en la ventana del Administrador de tareas
N=16; B_=231               # negro y blanco del cubo de colores (el texto y el fondo de la ventana)

# ---- 1.ª sesión: una ventana ----
# La ventana sale (CW_USEDEFAULT) en 48,52 y mide 400x240: su zona empieza en
# 50,77 y mide 396x213.
{ sleep 6; send 'VENTANAS' 9
  tecla right right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5   # la memoria antes de nada
  tecla left left ret; sleep 4; aparca; shot h1                            # el «Hola»
  clic 200 250; aparca; shot h2                                              # un clic
  doble 200 250; aparca; shot h3                                             # y dos seguidos
  tecla a; sleep 0.5; tecla b; sleep 1; shot t1                              # letras
  tecla f1; sleep 1; shot t2                                                 # una tecla que no escribe
  tecla t; sleep 1; shot t3                                                  # el título
  clicder 200 250; sleep 1.5; aparca; shot c1                                # el cuadro, con dueña
  clic 100 250; aparca; shot c2                                              # (la dueña no atiende)
  tecla ret; sleep 1.5; shot c3
  arrastra 150 62 462 300; aparca; shot v1                                   # moverla: a 360,290
  arrastra 758 528 778 558; aparca; shot v2                                  # más grande: 419x269
  clic 500 500; aparca; shot v3                                              # y sigue contando
  clic 769 299; sleep 1.5; aparca; shot x1                                   # la X
  tecla right right ret; sleep 5; aparca; shot m1                            # la memoria, después
  mon quit; } | run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 418 640 436 0 8 "Libre:" \
  busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- WINHOLA.EXE: la ventana, su texto y sus dibujos --"
chk $T/h1.ppm titulo 48 52 activa "Hola desde Win32" texto 60 87 $N $B_ fina "¡Hola, mundo de Win32!" \
  texto 60 107 $N $B_ fina "Clics: 0" texto 60 127 $N $B_ fina "Zona: 396x213" \
  texto 60 147 $N $B_ fina "Temporizador: bien" texto 60 167 $N $B_ fina "Enviado: 12  Tecla: -" \
  texto 60 187 28 $B_ fina "Todo bien. BEEF z4000000000" \
  rgb 300 87 000000 "borde del rectángulo" rgb 320 95 FF0000 "rectángulo rojo" rgb 320 112 FFFFFF "raya blanca" \
  rgb 429 136 000000 "esquina del borde" rgb 430 137 FFFFFF "fuera del rectángulo" \
  rgb 305 150 0000FF "rectángulo azul" rgb 429 186 0000FF "su esquina" texto 341 159 $B_ 21 fina "Centro" \
  rgb 440 280 FFFFFF "fondo blanco"
echo "-- los clics y las teclas --"
chk $T/h2.ppm texto 60 107 $N $B_ fina "Clics: 1" texto 60 87 $N $B_ fina "¡Hola, mundo de Win32!"
chk $T/h3.ppm texto 60 107 $N $B_ fina "Clics: 3"
chk $T/t1.ppm texto 60 167 $N $B_ fina "Enviado: 12  Tecla: b (1)"
chk $T/t2.ppm texto 60 167 $N $B_ fina "Enviado: 12  Tecla: F1"
chk $T/t3.ppm titulo 48 52 activa "Hola desde Win32 (título)" texto 60 167 $N $B_ fina "Enviado: 12  Tecla: t (3)"
echo "-- MessageBoxA con dueña --"
chk $T/c1.ppm busca 200 200 600 330 0 8 "Lleva 3 clics." busca 200 200 600 330 0 8 "Punto: 150, 173" \
  busca 300 300 500 370 0 6 "Aceptar" titulo 48 52 no "Hola desde Win32 (título)"
chk $T/c2.ppm busca 200 200 600 330 0 8 "Lleva 3 clics." texto 60 107 $N $B_ fina "Clics: 3"
chk $T/c3.ppm titulo 48 52 activa "Hola desde Win32 (título)" nobusca 200 200 600 330 0 8 "Lleva 3 clics." \
  texto 60 107 $N $B_ fina "Clics: 3"
echo "-- moverla y cambiarle el tamaño la repinta --"
chk $T/v1.ppm titulo 360 290 activa "Hola desde Win32 (título)" texto 372 325 $N $B_ fina "¡Hola, mundo de Win32!" \
  texto 372 345 $N $B_ fina "Clics: 3" texto 372 365 $N $B_ fina "Zona: 396x213" rgb 632 333 FF0000 "rectángulo rojo"
chk $T/v2.ppm titulo 360 290 activa "Hola desde Win32 (título)" texto 372 365 $N $B_ fina "Zona: 415x242" \
  texto 372 345 $N $B_ fina "Clics: 3" rgb 632 333 FF0000 "rectángulo rojo" rgb 770 550 FFFFFF "fondo de lo nuevo"
chk $T/v3.ppm texto 372 345 $N $B_ fina "Clics: 4"
echo "-- la X la cierra y no queda memoria --"
chk $T/x1.ppm titulo 60 260 activa "Win32" nobusca 300 280 790 590 $N $B_ "Clics: 4"
chk $T/m1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "4 programas en marcha" \
  igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi

# ---- 2.ª sesión: dos copias y el Administrador de tareas ----
# La primera sale en 48,52 y se lleva a 360,290; la segunda, en 72,76.
echo "-- dos copias a la vez --"
f1=$fail
cp $T/f0.img $T/f.img
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 9
  tecla right right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5
  tecla left left ret; sleep 4
  clic 200 250; clic 210 250                                                 # la primera: dos clics
  arrastra 150 62 462 300; aparca
  doble 125 375; sleep 4; aparca; shot d1                                    # la segunda
  clic 200 280; aparca; shot d2                                              # un clic en la segunda
  clic 600 500; aparca; shot d3                                              # y otro en la primera (pasa delante)
  clicder 600 500; sleep 1.5                                                 # la primera, con su cuadro abierto
  tecla ctrl-alt-delete; sleep 5; aparca; shot k1                            # el Administrador de tareas
  tecla down down down down; sleep 1; tecla delete; sleep 1.5; shot k2       # a la segunda se le pide que cierre
  tecla ret; sleep 4; shot k3
  tecla up; sleep 0.5; tecla delete; sleep 1.5; tecla ret; sleep 6; shot k4  # la primera no puede (tiene el cuadro):
  tecla ret; sleep 4; shot k5                                                # a la fuerza
  tecla alt-f4; sleep 1.5; shot k6
  mon quit; } | TMAX=200 run
chk $T/d1.ppm titulo 72 76 activa "Hola desde Win32" texto 84 131 $N $B_ fina "Clics: 0" \
  rgb 700 340 FF0000 "la primera, detrás"
chk $T/d2.ppm texto 84 131 $N $B_ fina "Clics: 1" rgb 700 340 FF0000 "la primera, detrás"
chk $T/d3.ppm titulo 360 290 activa "Hola desde Win32" texto 372 345 $N $B_ fina "Clics: 3" \
  texto 84 131 $N $B_ fina "Clics: 1"
chk $T/k1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 180 640 330 0 8 "Hola desde Win32" \
  busca 176 480 640 498 0 6 "6 programas en marcha" cambia $MEMZ $T/m0.ppm 20
echo "-- el Administrador de tareas: pedirle que cierre y terminarlo a la fuerza --"
chk $T/k2.ppm busca 150 250 650 330 0 8 "¿Terminar «Hola desde Win32»?"
chk $T/k3.ppm titulo 170 80 activa "Administrador de tareas" busca 176 480 640 498 0 6 "5 programas en marcha" \
  busca 176 180 640 330 0 8 "Hola desde Win32"
chk $T/k4.ppm busca 130 250 680 330 0 8 "«Hola desde Win32» no se cierra."
chk $T/k5.ppm titulo 170 80 activa "Administrador de tareas" nobusca 176 180 640 330 0 8 "Hola desde Win32" \
  busca 176 480 640 498 0 6 "4 programas en marcha" igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
chk $T/k6.ppm titulo 60 260 activa "Win32" nobusca 100 200 790 590 0 8 "Lleva 3 clics." \
  nobusca 300 280 790 590 $N $B_ "Clics: 3"

[ -n "${GUARDAR:-}" ] && { for f in $T/*.ppm; do cp $f "$GUARDAR"/s2_$(basename $f); done; } 2>/dev/null
if [ $fail -ne $f1 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi
rm -rf $T
exit $fail
