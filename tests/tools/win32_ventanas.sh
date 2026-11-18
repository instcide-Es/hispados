# Lo común de las pruebas de la fase 3c de Hispa-486 (programas Win32 de
# ventanas dentro de Hispa Ventanas): se incluye con «.» desde
# tests/run_win32ctl_tests.sh, run_win32menu_tests.sh y run_win32calc_tests.sh.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#   imagen PROGRAMA.EXE [otro archivo de build para \BIN...]
#       prepara $T/f.img: HW32.COM y MSVCRT.DLL en \HISPADOS, el programa (y
#       lo demás) en \BIN y un grupo «Win32», abierto (en 60,330), con dos
#       accesos: el programa y el Administrador de tareas
#   run     arranca QEMU con lo que se le escribe por la entrada (send, tecla,
#           clic, shot...); la consola queda en $T/log.txt
#   chk     tests/tools/ventanas_check.py
T=$(mktemp -d)
fail=0
B=build
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
doble() { ir $1 $2; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.12; mon "mouse_button 1" 0.12; mon "mouse_button 0" 0.6; }
aparca() { ir 16 590; }
chk() { python3 -I tests/tools/ventanas_check.py "$@" || fail=1; }
run() {
  rm -f $T/mon
  timeout ${TMAX:-170} qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 16 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 > $T/log.txt
}
imagen() {
  local f prog=$1
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  MTOOLS_SKIP_CHECK=1 mdel -i $T/f.img ::HISPADOS/HISPA.WAV 2>/dev/null   # (sitio: aquí no suena)
  for f in HW32.COM MSVCRT.DLL; do
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::HISPADOS/$f || ko "no cabe $f en el disco"
  done
  for f in "$@"; do
    MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $B/$f ::BIN/$f || ko "no cabe $f en el disco"
  done
  printf '%s\r\n' '[Centro]' 'Ventana=36,52,452,190' 'Activo=0' 'SinRepetidos=1' 'TareasPrincipal=1' \
    '[Grupo]' 'Nombre=Win32' 'Ventana=60,330,600,200' 'Estado=1' \
    '[Acceso]' 'Nombre=Prueba' 'Icono=4' "Programa=A:\\BIN\\$prog" \
    '[Acceso]' 'Nombre=Tareas' 'Icono=2' 'Programa=A:\HISPADOS\PROGS\TAREAS.HVP' \
    | iconv -f UTF-8 -t CP437 > $T/CENTRO.INI
  MTOOLS_SKIP_CHECK=1 mcopy -o -i $T/f.img $T/CENTRO.INI ::HISPADOS/CENTRO.INI
}
# la zona de la memoria en la ventana del Administrador de tareas, y los
# colores: negro y blanco del cubo (el texto y el fondo de una ventana de
# Windows), y la tinta sobre crema o blanco (los controles)
MEMZ="180 418 450 54"
N=16; B_=231
fin() {
  [ -n "${GUARDAR:-}" ] && cp $T/*.ppm "$GUARDAR"/ 2>/dev/null
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/VENTANAS/,$p' $T/log.txt | tail -20; fi
  rm -rf $T
  exit $fail
}
