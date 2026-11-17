#!/bin/bash
# Prueba de las teclas seguidas en Hispa Ventanas: cada tecla es para quien
# tiene el foco cuando le toca, no para quien lo tenía cuando se pulsó.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
#
# En un equipo lento (o muy cargado) las teclas que se pulsan seguidas llegan
# juntas en la misma vuelta del bucle de Ventanas. Antes, todas se apuntaban al
# control que tenía el foco en ese momento: con «Tab, Espacio» seguidos, el
# espacio actuaba sobre el control ANTERIOR al Tab (salió en la batería con 20
# pruebas a la vez: el Mezclador marcaba casillas que no eran). Ahora la tecla
# se dirige a quien tenga el foco al sacarla de la cola (msg_mirar).
#
# Se arranca VENTANAS /LENTO (opción oculta, para pruebas: cada vuelta tarda
# 1/3 s) y, en Mezclador > Avanzado, con el foco en «Ganancia de entrada», se
# mandan de golpe Espacio, Tab, Espacio: tiene que quedar la de entrada en x2
# y la de SALIDA en x2 (antes: la de entrada en x4 y la de salida sin tocar).
# Type-ahead test: keys go to the control that has the focus when dequeued.
set -u
cd "$(dirname "$0")/.."
. tests/tools/serie.sh
IMG=${1:-build/hispados.img}
CHK="python3 tests/tools/ventanas_check.py"
T=$(mktemp -d)
fail=0
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
tecla() { local k; for k in "$@"; do mon "sendkey $k" 0.5; done; }
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
doble() { ir $1 $2; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.1; mon "mouse_button 1" 0.1; mon "mouse_button 0" 0.8; }
# hasta NOMBRE comprobaciones...: repite la captura hasta que se cumplan (tope)
hasta() {
  local n=$1 i; shift
  for i in $(seq 1 60); do
    mon "screendump $T/$n.ppm" 0.6
    $CHK $T/$n.ppm "$@" > /dev/null 2>&1 && return 0
  done
  echo "  (no llegó a verse en pantalla lo esperado en la captura $n)" >&2
  return 1
}
DESDE=0
listo() { serie_indicador $T/raw $DESDE 300 120 >&2; }
send() { listo; DESDE=$(( $(serie_tam $T/raw) + ${#1} )); printf '%s\r' "$1"; }

cp "$IMG" $T/f.img
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nFILES=30\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
{ send 'KEYB SP'
  send 'A:\HISPADOS\HSPRATON'
  send 'A:\HISPADOS\HSPSND'
  send 'VENTANAS /LENTO'
  hasta e0 titulo 36 52 activa "Centro de programas"; sleep 1
  # doble clic en el altavoz de la barra: el Mezclador; Ver > Avanzado...
  doble 658 14
  ir 16 320
  hasta m1 titulo 86 81 activa "Mezclador"
  tecla alt-v; sleep 1; tecla down down ret
  hasta a1 titulo 126 141 activa "Avanzado" texto 198 181 0 6 fina "Ganancia de entrada: x1" \
    texto 202 213 0 6 fina "Ganancia de salida: x1"
  sleep 1
  # las tres teclas, de golpe (una sola conexión con el monitor de QEMU)
  printf 'sendkey spc\nsendkey tab\nsendkey spc\n' | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1
  sleep 3
  hasta a2 texto 202 213 0 6 fina "Ganancia de salida: x2"
  mon quit
} | timeout 600 qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
    -parallel none -m 32 -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
    -audiodev none,id=snd0 -vga std -device sb16,audiodev=snd0 > $T/raw 2>&1

echo "== Hispa Ventanas: teclas seguidas en un equipo lento (VENTANAS /LENTO) =="
chk $T/a1.ppm titulo 126 141 activa "Avanzado" texto 198 181 0 6 fina "Ganancia de entrada: x1"
# Espacio (entrada: x2), Tab (el foco, a la de salida), Espacio (salida: x2)
chk $T/a2.ppm texto 198 181 0 6 fina "Ganancia de entrada: x2" texto 202 213 0 6 fina "Ganancia de salida: x2"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | tail -20
  [ -f $T/a2.ppm ] && convert $T/a2.ppm "$(dirname "$IMG")/teclas_seguidas.png" 2>/dev/null
fi
rm -rf $T
exit $fail
