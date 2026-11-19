#!/bin/bash
# Prueba de la fase 3a de Hispa-486 (docs/486/HISPA-486.md): programas Win32
# de VENTANAS dentro de Hispa Ventanas (ventanas/win32.inc).
#  - WINGUI.EXE (tests/progs/wingui.c, subsistema 2), abierto desde un acceso
#    directo del Centro: su MessageBoxA sale como un cuadro de Hispa Ventanas
#    con «Sí» y «No»; Esc no lo cierra (hay que contestar); «Sí» y sale el
#    segundo cuadro con lo contestado, la línea de órdenes (el programa y sus
#    parámetros) y «Todo bien.»; Aceptar y la tarea termina. Otra vez,
#    contestando «No». Abierto varias veces, no se pierde memoria.
#  - WINFALTA.EXE: llama a una función que no hay (SetWindowsHookA): sale
#    «Hispa-486: falta la función SetWindowsHookA (USER32.dll)», el programa
#    termina y Hispa Ventanas sigue.
#  - Con uno preguntando se abren el Bloc de notas y otro WINGUI (dos a la
#    vez); el Administrador de tareas lo lista, lo termina a la fuerza y la
#    memoria libre queda como antes de abrir ninguno.
#  - HOLAC.EXE (de consola, subsistema 3) sigue saliendo a Hispa-DOS, donde lo
#    ejecuta HW32, y se vuelve a Hispa Ventanas.
#  (GUARDAR=carpeta deja las capturas.)
# Win32 GUI programs inside Hispa Ventanas (phase 3a) test.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
make -s $B/WINGUI.EXE $B/WINFALTA.EXE $B/HOLAC.EXE $B/HW32.COM > /dev/null 2>&1 || {
  echo "== Hispa-486: Win32 de ventanas: sin gcc de 32 bits, se salta =="; exit 0; }
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
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-150} qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}

echo "== Hispa-486: programas Win32 de ventanas dentro de Hispa Ventanas =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in WINGUI.EXE WINFALTA.EXE HOLAC.EXE HW32.COM; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::BIN/$f || ko "no cabe $f en el disco"
done
# un grupo «Win32», abierto, con los accesos de la prueba (cinco iconos en
# fila: sus nombres, en x = 125, 229, 333, 437 y 541 de la pantalla, y = 371)
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Win32' 'Ventana=60,260,600,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Ventanas' 'Icono=4' 'Programa=A:\BIN\WINGUI.EXE' 'Parametros=uno dos' \
  '[Acceso]' 'Nombre=Falta' 'Icono=4' 'Programa=A:\BIN\WINFALTA.EXE' \
  '[Acceso]' 'Nombre=Consola' 'Icono=6' 'Programa=A:\BIN\HOLAC.EXE' 'Parametros=tres' 'Volver=1' \
  '[Acceso]' 'Nombre=Bloc' 'Icono=1' 'Programa=:bloc' \
  '[Acceso]' 'Nombre=Tareas' 'Icono=2' 'Programa=A:\HISPADOS\PROGS\TAREAS.HVP' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI

aparca() { ir 16 590; }
MEMZ="180 418 450 54"      # la zona de la memoria en la ventana del Administrador de tareas

# ---- 1.ª sesión: los programas de ventanas ----
{ sleep 6; send 'VENTANAS' 9
  # la memoria antes de nada: el Administrador de tareas (el 5.º acceso)
  tecla right right right right ret; sleep 5; aparca; shot m0; tecla alt-f4; sleep 1.5
  # WINGUI: la pregunta; Esc no la cierra; Sí; lo contestado; Aceptar
  tecla left left left left ret; sleep 2; shot g1
  tecla esc; sleep 1; shot g2
  tecla ret; sleep 2; shot g3
  tecla ret; sleep 2; shot g4
  # otra vez, contestando «No» (Tab e Intro)
  tecla ret; sleep 2; tecla tab ret; sleep 2; shot g5
  tecla ret; sleep 1.5
  # WINFALTA: pregunta, aviso, y la función que falta
  tecla right ret; sleep 2; tecla ret; sleep 2; shot f1
  tecla ret; sleep 2; shot f2
  tecla ret; sleep 2; shot f3
  # con WINGUI preguntando, se abre el Bloc de notas (las demás tareas siguen)
  tecla left ret; sleep 2; doble 437 375; sleep 2; aparca; shot b1
  tecla alt-f4; sleep 1.5; shot b2
  # y otro WINGUI a la vez (dos programas de Windows, cada uno con su cuadro)
  doble 125 375; sleep 2; aparca; shot d1
  tecla ret; sleep 2; tecla ret; sleep 2; shot d2          # el segundo: Sí y Aceptar
  # con el primero aún preguntando: el Administrador de tareas lo termina
  doble 541 375; sleep 5; aparca; shot k1
  tecla down down down; sleep 1; tecla delete; sleep 1.5; shot k2; tecla ret; sleep 5; shot k2b   # no se cierra (hay que contestar):
  tecla ret; sleep 4; shot k3                                       # a la fuerza
  tecla alt-f4; sleep 1.5; shot k4
  mon quit; } | run
chk $T/m0.ppm titulo 170 80 activa "Administrador de tareas" busca 176 418 640 436 0 8 "Libre:" \
  busca 176 480 640 498 0 6 "4 programas en marcha"
echo "-- WINGUI.EXE: MessageBoxA es un cuadro de Hispa Ventanas --"
chk $T/g1.ppm titulo 288 234 activa "Prueba de Win32" busca 280 240 520 320 0 8 "¿Le gusta Hispa-486?" \
  busca 280 240 520 320 0 8 "Conteste sí o no." busca 300 320 500 350 0 6 "Sí" busca 300 320 500 350 0 6 "No" \
  nobusca 280 320 620 350 0 6 "Cancelar"
chk $T/g2.ppm igual 284 230 240 140 $T/g1.ppm 284 230 0        # Esc: hay que contestar
chk $T/g3.ppm titulo 274 225 activa "Prueba de Win32" busca 270 250 530 330 0 8 "Ha dicho que sí." \
  busca 270 250 530 330 0 8 "A:\\BIN\\WINGUI.EXE uno dos" busca 270 250 530 330 0 8 "Todo bien." \
  busca 300 330 500 360 0 6 "Aceptar"
chk $T/g4.ppm titulo 60 260 activa "Win32" nobusca 100 200 700 400 0 8 "Todo bien."
chk $T/g5.ppm titulo 274 225 activa "Prueba de Win32" busca 270 250 530 330 0 8 "Ha dicho que no." \
  busca 270 250 530 330 0 8 "Todo bien."
echo "-- WINFALTA.EXE: una función que falta --"
chk $T/f1.ppm titulo 288 243 activa "Error" busca 280 260 520 320 0 8 "Ahora, una que falta."
chk $T/f2.ppm titulo 150 243 activa "Hispa-486" \
  busca 100 270 700 310 0 8 "Hispa-486: falta la función SetWindowsHookA (USER32.dll)"
chk $T/f3.ppm titulo 60 260 activa "Win32" nobusca 100 200 700 400 0 8 "ESTO NO DEBE SALIR" \
  nobusca 100 200 700 400 0 8 "Hispa-486: falta la función SetWindowsHookA (USER32.dll)"
echo "-- con un programa de Windows abierto, lo demás sigue --"
chk $T/b1.ppm titulo 150 90 activa "Bloc de notas - (sin nombre)"
chk $T/b2.ppm titulo 60 260 activa "Win32" titulo 288 234 no "Prueba de Win32"
chk $T/d1.ppm titulo 288 234 activa "Prueba de Win32" busca 280 240 520 320 0 8 "¿Le gusta Hispa-486?"
chk $T/d2.ppm titulo 288 234 no "Prueba de Win32"               # el primero sigue ahí
echo "-- el Administrador de tareas lo ve y lo termina; no se pierde memoria --"
chk $T/k1.ppm titulo 170 80 activa "Administrador de tareas" busca 176 232 640 252 0 8 "Prueba de Win32" \
  busca 176 480 640 498 0 6 "5 programas en marcha" cambia $MEMZ $T/m0.ppm 20
chk $T/k2.ppm busca 150 250 650 330 0 8 "¿Terminar «Prueba de Win32»?"
chk $T/k2b.ppm busca 130 250 680 330 0 8 "«Prueba de Win32» no se cierra."
chk $T/k3.ppm titulo 170 80 activa "Administrador de tareas" nobusca 176 180 640 300 0 8 "Prueba de Win32" \
  busca 176 480 640 498 0 6 "4 programas en marcha" igual $MEMZ $T/m0.ppm ${MEMZ% * *} 0
chk $T/k4.ppm nobusca 250 200 560 400 0 8 "¿Le gusta Hispa-486?" nobusca 250 200 560 400 0 8 "Conteste sí o no."

# ---- 2.ª sesión: un programa de Windows de consola sale a Hispa-DOS (HW32) y se vuelve ----
echo "-- HOLAC.EXE (de consola) sale a Hispa-DOS y lo ejecuta HW32 --"
RX=400; RY=300
{ sleep 6; send 'VENTANAS' 9
  tecla right right ret; sleep 14; aparca; shot c1
  tecla left left ret; sleep 2; shot c2; tecla ret; sleep 2; tecla ret; sleep 1.5   # y WINGUI sigue yendo
  clic 475 65; sleep 1; tecla ret; sleep 4                  # cerrar el Centro: «Volver a Hispa-DOS»
  send 'ECHO TRAS-SALIR' 2; } | TMAX=90 run
check "Hola desde C (gcc) en Hispa-486"
check 'Linea: A:\BIN\HOLAC.EXE tres'
check "Suma: 704982704"
chk $T/c1.ppm titulo 60 260 activa "Win32"                    # de vuelta en Hispa Ventanas
chk $T/c2.ppm titulo 288 234 activa "Prueba de Win32"
check "Hispa Ventanas ha terminado."
if sed -n '/Hispa Ventanas ha terminado/,$p' $T/log.txt | grep -qF 'TRAS-SALIR'; then ok "después, el intérprete de órdenes sigue"; else ko "sin el intérprete tras salir"; fi
if grep -q 'HW32: hace falta HSPDPMI' $T/log.txt; then ko "HW32 se ha quedado sin HSPDPMI"; fi

[ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -30; fi
rm -rf $T
exit $fail
