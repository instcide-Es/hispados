#!/bin/bash
# Prueba de la fase 3a de Hispa-486 (docs/486/HISPA-486.md): programas Win32
# de VENTANAS dentro de Hispa Ventanas (ventanas/win32.inc).
#  - WINGUI.EXE (tests/progs/wingui.c, subsistema 2), abierto desde un acceso
#    directo del Centro: su MessageBoxA sale como un cuadro de Hispa Ventanas
#    con «Sí» y «No»; Esc no lo cierra (hay que contestar); «Sí» y sale el
#    segundo cuadro con lo contestado, la línea de órdenes (el programa y sus
#    parámetros) y «Todo bien.»; Aceptar y la tarea termina. Otra vez,
#    contestando «No». Abierto varias veces, no se pierde memoria.
#  - WINFALTA.EXE: llama a una función que no hay (CreateWindowExA): sale
#    «Hispa-486: falta la función CreateWindowExA (USER32.dll)», el programa
#    termina y Hispa Ventanas sigue.
#  - Dos a la vez (cada uno con su cuadro), y el Bloc de notas sigue
#    funcionando con un programa de Windows abierto.
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
  timeout ${TMAX:-150} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
# la memoria libre que dice DPMI (el primer número de la información de 0500h)
# la apunta el Administrador de tareas; aquí se mira desde fuera: XMSTEST
# antes de entrar y después de salir de Hispa Ventanas tiene que dar lo mismo

echo "== Hispa-486: programas Win32 de ventanas dentro de Hispa Ventanas =="
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in WINGUI.EXE WINFALTA.EXE HOLAC.EXE HW32.COM; do
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::BIN/$f || ko "no cabe $f en el disco"
done
# un grupo «Win32», abierto, con los accesos de la prueba (cuatro iconos en
# fila: x = 90, 190, 290 y 390 dentro del grupo)
printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
  '[Grupo]' 'Nombre=Win32' 'Ventana=60,260,470,200' 'Estado=1' \
  '[Acceso]' 'Nombre=Ventanas' 'Icono=4' 'Programa=A:\BIN\WINGUI.EXE' 'Parametros=uno dos' \
  '[Acceso]' 'Nombre=Falta' 'Icono=4' 'Programa=A:\BIN\WINFALTA.EXE' \
  '[Acceso]' 'Nombre=Consola' 'Icono=6' 'Programa=A:\BIN\HOLAC.EXE' 'Parametros=tres' 'Volver=1' \
  '[Acceso]' 'Nombre=Bloc' 'Icono=1' 'Programa=:bloc' \
  | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI

if [ -n "${EXPLORA:-}" ]; then
  { sleep 6; send 'VENTANAS' 9; tecla ret; sleep 2; shot e2; tecla esc; sleep 1; shot e3; tecla ret; sleep 2; shot e4; tecla ret; sleep 2; shot e5; mon quit; } | run
  tail -5 $T/log.txt; cp $T/e?.ppm "$EXPLORA"/; rm -rf $T; exit 0
fi
rm -rf $T
exit $fail
