#!/bin/bash
# Prueba del escritorio de Hispa-486 de Hispa Ventanas (ventanas/barra.inc;
# docs/VENTANAS.md, «El escritorio de Hispa-486»): la barra de tareas de
# abajo y el menú Inicio.
#  1.ª sesión, VENTANAS /486 en 800x600: el escritorio empieza despejado (sin
#     el Centro de programas ni la barra de arriba), con la barra de tareas
#     abajo (Inicio, la hora); el menú Inicio con el ratón (los grupos del
#     Centro y las órdenes fijas; al parar el ratón en un grupo sale su
#     submenú) y un programa abierto desde él (la Calculadora), con su botón
#     en la barra; el menú con el teclado (Ctrl+Esc, flechas, Intro, Esc) y
#     otro programa (el Bloc de notas); un clic en el botón trae la ventana
#     delante, otro la minimiza (va a su botón, no a un icono del escritorio)
#     y otro la recupera; maximizar no tapa la barra; el botón derecho en la
#     barra (cascada) y en un botón (el menú de la ventana); «Centro de
#     programas» desde Inicio, y cerrarlo lo esconde; «Salir» desde Inicio.
#  2.ª sesión, 640x480 en VGA de 16 colores, sin /486: el ajuste es la clave
#     «Escritorio=486» de VENTANAS.INI. El menú cabe; un programa de Windows
#     (WINCALC.EXE, si hay gcc de 32 bits) sale en la barra como los demás;
#     Configuración guarda VENTANAS.INI y conserva la clave.
#  3.ª sesión, sin el ajuste: el escritorio de Hispa-386 es el de siempre (la
#     barra arriba, el Centro abierto, nada abajo) y Ctrl+Esc no hace nada.
#  (GUARDAR=carpeta deja las capturas.)
# Hispa-486 desktop test: bottom taskbar and Start menu.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
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
doble() { ir $1 $2; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.6; }
derecho() { ir $1 $2; mon "mouse_button 2" 0.25; mon "mouse_button 0" 0.5; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-170} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
ini() { MTOOLS_SKIP_CHECK=1 mtype -i $T/f.img ::HISPADOS/VENTANAS.INI 2>/dev/null | tr -d '\r' | iconv -f CP437 -t UTF-8 > $T/ven.txt; }

echo "== Hispa Ventanas: el escritorio de Hispa-486 (barra de tareas y menú Inicio) =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
cp $T/f.img $T/base.img

# ---- 1.ª: VENTANAS /486, 800x600 ----
# La barra: de y = 572 a 599 (el filete ocre, arriba); «Inicio», en 4,577;
# los botones de las ventanas, desde x = 88, de 160 de ancho como mucho.
# El menú Inicio: en 2,322, de 258 de ancho; el submenú, a su derecha.
# La Calculadora nace en 500,60 y el Bloc de notas en 150,90.
BY=588
{ sleep 6; send 'VENTANAS /486' 10
  ir 300 200; shot a1                               # el escritorio, despejado
  clic 30 $BY; sleep 1; ir 300 $BY; ir 300 200; shot a2     # Inicio, con el ratón (que sale por la barra)
  ir 300 383; ir 200 383; sleep 1.2; shot a3        # parado en «Accesorios»: su submenú
  clic 320 406; sleep 3; ir 300 200; shot a4        # «Calculadora»
  tecla ctrl-esc; sleep 1; shot a5                  # Inicio, con el teclado
  tecla down; sleep 0.3; tecla right; sleep 1; shot a6      # «Accesorios» y dentro
  tecla left; sleep 0.6; shot a7                    # fuera del submenú
  tecla esc; sleep 0.8; shot a8                     # Esc: se cierra
  tecla ctrl-esc; sleep 1; tecla down right; sleep 0.6; tecla ret; sleep 3; ir 300 200; shot a9   # «Bloc de notas»
  clic 150 $BY; sleep 1; ir 300 200; shot b1        # el botón de la Calculadora: delante
  clic 150 $BY; sleep 1; ir 300 200; shot b2        # otra vez: se minimiza (a su botón)
  clic 150 $BY; sleep 1; ir 300 200; shot b3        # y otra: se recupera
  clic 310 $BY; sleep 1; doble 400 102; sleep 1; ir 300 200; shot b4    # el Bloc de notas, maximizado
  derecho 600 $BY; sleep 1; shot b5                 # el botón derecho en la barra
  tecla a; sleep 1.5; ir 300 400; shot b6           # «Ventanas en cascada»
  derecho 150 $BY; sleep 1; shot b7                 # el botón derecho en un botón: el menú de la ventana
  tecla esc; sleep 0.6
  clic 30 $BY; sleep 1; ir 600 300; mon "mouse_button 1" 0.25; mon "mouse_button 0" 0.5; sleep 0.8; shot b8   # un clic fuera cierra el menú
  tecla ctrl-esc; sleep 0.8; tecla c c; sleep 0.4; tecla ret; sleep 2; ir 300 500; shot c1   # «Centro de programas»
  tecla alt-f4; sleep 1.5; shot c2                  # cerrarlo: se esconde (no se sale)
  tecla ctrl-esc; sleep 0.8; tecla up; sleep 0.4; tecla ret; sleep 2; shot c3   # «Salir de Hispa Ventanas...»
  tecla ret; sleep 4
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=200 run
echo "-- el escritorio de Hispa-486: despejado, con la barra de tareas abajo --"
chk $T/a1.ppm tam 800 600 px 400 572 10 "el filete de la barra de tareas" px 84 590 1 "la barra de tareas" \
  texto 26 578 8 4 negrita "Inicio" nopx 400 10 1 "sin barra arriba" nopx 400 26 10 "sin el filete de arriba" \
  nobusca 0 0 500 300 1 6 "Centro de programas" px 40 100 2 "el fondo, donde estaba el Centro" \
  busca 730 572 790 584 8 1 ":"
echo "-- el menú Inicio, con el ratón --"
chk $T/a2.ppm texto 26 578 0 10 negrita "Inicio" texto 29 326 8 1 negrita "Hispa Ventanas" \
  busca 20 340 60 372 0 8 "Principal" busca 20 360 60 396 0 8 "Accesorios" busca 20 380 60 420 0 8 "Juegos" \
  busca 20 430 60 470 0 8 "Configuración" busca 20 450 60 492 0 8 "Administrador de archivos" \
  busca 20 470 60 514 0 8 "Administrador de tareas" busca 20 490 60 536 0 8 "Centro de programas" \
  busca 20 520 60 566 0 8 "Salir de Hispa Ventanas..."
chk $T/a3.ppm busca 20 360 60 396 9 12 "Accesorios" busca 270 370 320 400 0 8 "Bloc de notas" \
  busca 270 390 320 420 0 8 "Calculadora" busca 270 410 320 450 0 8 "Reloj"
chk $T/a4.ppm titulo 500 60 activa "Calculadora" busca 100 574 130 582 0 8 "Calculadora" \
  nobusca 20 340 60 372 0 8 "Principal" texto 26 578 8 4 negrita "Inicio"
echo "-- el menú Inicio, con el teclado --"
chk $T/a5.ppm busca 20 340 60 372 9 12 "Principal" busca 20 360 60 396 0 8 "Accesorios"
chk $T/a6.ppm busca 270 370 320 400 9 12 "Bloc de notas" busca 270 390 320 420 0 8 "Calculadora"
chk $T/a7.ppm nobusca 270 370 320 400 9 12 "Bloc de notas" busca 20 360 60 396 9 12 "Accesorios"
chk $T/a8.ppm nobusca 20 360 60 396 9 12 "Accesorios" nobusca 20 340 60 372 0 8 "Principal" titulo 500 60 activa "Calculadora"
chk $T/a9.ppm titulo 150 90 activa "Bloc de notas - (sin nombre)" busca 260 574 290 582 0 8 "Bloc de notas - " \
  busca 100 574 130 582 0 6 "Calculadora"
echo "-- los botones de las ventanas: delante, minimizar y recuperar --"
chk $T/b1.ppm titulo 500 60 activa "Calculadora" busca 100 574 130 582 0 8 "Calculadora" busca 260 574 290 582 0 6 "Bloc de notas - "
chk $T/b2.ppm nobusca 480 40 560 80 8 4 "Calculadora" busca 100 574 130 582 0 7 "Calculadora" \
  titulo 150 90 activa "Bloc de notas - (sin nombre)" nobusca 0 520 400 572 8 2 "Calculadora" px 40 550 2 "sin iconos de lo minimizado"
chk $T/b3.ppm titulo 500 60 activa "Calculadora" busca 100 574 130 582 0 8 "Calculadora"
echo "-- maximizar deja sitio a la barra --"
chk $T/b4.ppm marco 0 0 800 572 activa titulo 0 0 activa "Bloc de notas - (sin nombre)" \
  px 400 572 10 "el filete de la barra de tareas" px 84 590 1 "la barra de tareas" texto 26 578 8 4 negrita "Inicio" \
  busca 260 574 290 582 0 8 "Bloc de notas - "
echo "-- el botón derecho en la barra y en un botón --"
chk $T/b5.ppm busca 360 440 420 572 0 8 "Ventanas en c" busca 360 440 420 572 0 8 "Ventanas en m" \
  busca 360 440 420 572 0 8 "Administrador de " busca 360 440 420 572 0 8 "onfiguración..."
chk $T/b6.ppm titulo 34 34 activa "Bloc de notas - (sin nombre)" titulo 8 8 no "Calculadora"
chk $T/b7.ppm busca 140 440 200 572 0 8 "Mi" busca 140 440 200 572 0 8 "errar"
chk $T/b8.ppm nobusca 20 340 60 372 0 8 "Principal" texto 26 578 8 4 negrita "Inicio"
echo "-- el Centro de programas, desde Inicio; cerrarlo lo esconde --"
chk $T/c1.ppm titulo 36 52 activa "Centro de programas" busca 420 574 450 582 0 8 "Centro de progra"
chk $T/c2.ppm nobusca 0 40 200 70 8 4 "Centro de programas" nobusca 420 574 450 582 0 8 "Centro de progra" \
  px 84 590 1 "la barra de tareas sigue (no se ha salido)"
echo "-- «Salir», desde Inicio --"
chk $T/c3.ppm titulo 212 157 activa "Salir de Hispa Ventanas"
check "Hispa Ventanas ha terminado."
if sed -n '/Hispa Ventanas ha terminado/,$p' $T/log.txt | grep -qF 'TRAS-SALIR'; then ok "después, el intérprete de órdenes sigue"; else ko "sin el intérprete tras salir"; fi
[ -n "${GUARDAR:-}" ] && for f in $T/[abc][0-9].ppm; do cp $f "$GUARDAR"/barra_$(basename $f); done 2>/dev/null

# ---- 2.ª: 640x480, VGA de 16 colores, con la clave de VENTANAS.INI ----
cp $T/base.img $T/f.img
MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/HISPA.WAV 2>/dev/null   # (sitio para Win32: aquí no suena)
W32=0
if make -s build/WINCALC.EXE build/MSVCRT.DLL build/HW32.COM > /dev/null 2>&1; then
  W32=1
  for f in HW32.COM MSVCRT.DLL; do MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img build/$f ::HISPADOS/$f || W32=0; done
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img build/WINCALC.EXE ::BIN/WINCALC.EXE || W32=0
fi
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Windows' 'Ventana=60,230,400,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Cuentas de Windows' 'Icono=4' 'Programa=A:\BIN\WINCALC.EXE' \
  '[Grupo]' 'Nombre=Principal' \
  '[Acceso]' 'Nombre=Configuración' 'Icono=5' 'Programa=A:\HISPADOS\CONFIG.HVP' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
printf '%s\r\n' '; ajustes de prueba' '[Pantalla]' 'Dibujo=rombos' '' '[Escritorio]' 'Escritorio = 486' > $T/VENTANAS.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/VENTANAS.INI ::HISPADOS/VENTANAS.INI
# La barra: de y = 452 a 479; «Inicio», en 4,457. La calculadora de Windows
# nace en 60,50. Configuración: el segundo dibujo del fondo («liso»), en
# 416,155; «Guardar», en 446,430.
RX=320; RY=240; BY=468
{ sleep 6; send 'VENTANAS /VGA' 10
  ir 300 20; shot v1                                # el grupo que estaba abierto no se abre
  tecla ctrl-esc; sleep 1; shot v2                  # el menú cabe encima de la barra
  if [ $W32 = 1 ]; then
    tecla right; sleep 0.8; tecla ret; sleep 7; ir 300 20; shot v3      # el programa de Windows
    clic 150 $BY; sleep 1; ir 300 20; shot v4       # su botón: se minimiza
    clic 150 $BY; sleep 1; ir 300 20; shot v5       # y vuelve
    tecla alt-f4; sleep 2
  else
    tecla esc; sleep 0.8
  fi
  tecla ctrl-esc; sleep 0.8; tecla c; sleep 0.4; tecla ret; sleep 4; ir 300 20; shot v6   # Configuración
  clic 416 155; sleep 1; clic 446 430; sleep 2.5; ir 300 20; shot v7    # otro dibujo, y «Guardar»
  tecla ctrl-esc; sleep 0.8; tecla up; sleep 0.4; tecla ret; sleep 2; tecla ret; sleep 4
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=200 run
echo "-- 640x480 en VGA de 16 colores, con «Escritorio=486» en VENTANAS.INI --"
chk $T/v1.ppm tam 640 480 px 320 452 10 "el filete de la barra de tareas" px 320 470 1 "la barra de tareas" \
  texto 26 458 8 4 negrita "Inicio" nopx 320 10 1 "sin barra arriba" \
  nobusca 0 200 400 260 1 6 "Windows" nobusca 0 0 500 100 1 6 "Centro de programas"
chk $T/v2.ppm busca 20 200 60 452 9 12 "Windows" busca 20 200 60 452 0 8 "Principal" \
  busca 20 200 60 452 0 8 "Configuración" busca 20 200 60 452 0 8 "Salir de Hispa Ventanas..." \
  px 320 470 1 "la barra de tareas, sin tapar"
if [ $W32 = 1 ]; then
  echo "-- un programa de Windows, en la barra como cualquier ventana --"
  chk $T/v3.ppm titulo 60 50 activa "Calculadora" busca 100 454 130 462 0 8 "Calculadora"
  chk $T/v4.ppm nobusca 60 40 120 70 8 4 "Calculadora" busca 100 454 130 462 0 7 "Calculadora"
  chk $T/v5.ppm titulo 60 50 activa "Calculadora" busca 100 454 130 462 0 8 "Calculadora"
else
  echo "  (sin gcc de 32 bits: se salta el programa de Windows)"
fi
echo "-- Configuración guarda VENTANAS.INI y conserva la clave --"
chk $T/v6.ppm busca 100 454 130 462 0 8 "Configuración" px 320 470 1 "la barra de tareas, sin tapar"
ini
if grep -qx 'Dibujo=liso' $T/ven.txt; then ok "VENTANAS.INI: Configuración ha guardado el dibujo nuevo"; else ko "VENTANAS.INI sin Dibujo=liso"; fi
if grep -qx 'Escritorio=486' $T/ven.txt; then ok "VENTANAS.INI: sigue «Escritorio=486»"; else ko "VENTANAS.INI ha perdido «Escritorio=486»"; fi
if grep -qx '\[Escritorio\]' $T/ven.txt; then ok "VENTANAS.INI: con su sección [Escritorio]"; else ko "VENTANAS.INI sin [Escritorio]"; fi
check "Hispa Ventanas ha terminado."
[ $fail -ne 0 ] && cat $T/ven.txt
[ -n "${GUARDAR:-}" ] && for f in $T/v[0-9].ppm; do cp $f "$GUARDAR"/barra_$(basename $f); done 2>/dev/null

# ---- 3.ª: sin el ajuste, el escritorio de Hispa-386 de siempre ----
cp $T/base.img $T/f.img
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 10
  ir 300 400; shot h1
  tecla ctrl-esc; sleep 1; shot h2                  # Ctrl+Esc: aquí no hay menú Inicio
  clic 420 300; sleep 1; ir 300 400; shot h3        # ni el fondo hace nada nuevo
  tecla alt-f4; sleep 1.5; tecla ret; sleep 4       # cerrar el Centro: el cuadro de salir
  send 'ECHO TRAS-SALIR' 2
  mon quit; } | TMAX=120 run
echo "-- sin el ajuste: el escritorio de Hispa-386, como siempre --"
chk $T/h1.ppm tam 800 600 px 400 10 1 "la barra de arriba" px 400 26 10 "y su filete" texto 12 5 8 1 negrita "Hispa Ventanas" \
  titulo 36 52 activa "Centro de programas" nopx 400 572 10 "sin barra de tareas" nopx 400 590 1 "sin barra de tareas" \
  nopx 30 588 4 "sin botón Inicio"
chk $T/h2.ppm igual 0 30 800 570 $T/h1.ppm 0 30 0
chk $T/h3.ppm igual 0 30 800 570 $T/h1.ppm 0 30 0
check "Hispa Ventanas ha terminado."
ini
if [ -s $T/ven.txt ] && grep -q 'Escritorio' $T/ven.txt; then ko "VENTANAS.INI de Hispa-386 con la clave Escritorio"; else ok "VENTANAS.INI de Hispa-386, sin la clave Escritorio"; fi
[ -n "${GUARDAR:-}" ] && for f in $T/h[0-9].ppm; do cp $f "$GUARDAR"/barra_$(basename $f); done 2>/dev/null

if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi
rm -rf $T
exit $fail
