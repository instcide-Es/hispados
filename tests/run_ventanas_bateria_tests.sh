#!/bin/bash
# [BATERÍA] [RELOJ] Prueba del indicador de batería y de la hora de Hispa
# Ventanas, en la barra superior entre el altavoz y «Salir». QEMU no simula una
# batería APM, así que se usa la opción oculta /BATnn[C] (depuración), que fija
# el nivel sin tocar la BIOS: /BAT85 = 85 %, /BAT100C = 100 % enchufada. La hora
# se saca de DOS (INT 21h 2Ch); el RTC se fija a las 23:58 para comprobarla.
# Se arranca sin /DEMO (Centro de programas), con HSPSND (el altavoz sale
# encendido), y se comprueba punto a punto (tests/tools/ventanas_check.py):
#  - el orden de izquierda a derecha: [altavoz] [pila + %] [HH:MM] [Salir];
#  - el relleno de la pila y su color según el nivel (verde >50, ocre 20-50,
#    rojo <20), el marco crema, el número y la hora «23:58»;
#  - enchufada/cargando: el rayo blanco sobre la pila;
#  - un sobremesa (sin /BAT, QEMU no da batería): solo la hora, sin pila;
#  - que el altavoz, corrido a la izquierda, sigue abriendo el volumen con un
#    clic (sus clics no se han roto).
# Deja capturas en build/ventanas_bateria_*.png.
# Hispa Ventanas test: battery level indicator (APM) and clock in the top bar.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
T=$(mktemp -d)
CHK="python3 tests/tools/ventanas_check.py"
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
chk() { $CHK "$@" || fail=1; }
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; sleep "${2:-0.3}"; }
shot() { sleep 0.4; mon "screendump $T/$1.ppm" 0.6; }
send() { printf '%s\r' "$1"; sleep "${2:-1.5}"; }
muestra() { [ -f $T/$1.ppm ] && convert $T/$1.ppm $B/ventanas_bateria_$2.png; }
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
# un arranque por escenario, con el reloj a las 23:58 (así la hora es estable)
run() {   # run ETIQUETA "ORDEN VENTANAS" [ordenes extra con el ratón...]
  cp "$IMG" $T/f.img
  printf "DEVICE=A:\\\\HISPADOS\\\\HSPXMS.SYS\r\nFILES=30\r\n" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  rm -f $T/mon; RX=400; RY=300
  { sleep 6
    send 'A:\HISPADOS\HSPDPMI' 2
    send 'A:\HISPADOS\HSPRATON' 2
    send 'A:\HISPADOS\HSPSND' 3
    send "VENTANAS $2" 7
    shot "$1"
    eval "${3:-:}"
    mon quit
  } | timeout 45 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -parallel none -m 32 -rtc base=2026-11-15T23:58:04 \
      -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
      -audiodev none,id=snd0 -device sb16,audiodev=snd0 > $T/raw 2>&1
  tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8 | grep -av '^audio:' > $T/log.txt
}

echo "== Hispa Ventanas: batería (APM) y hora, orden [altavoz][batería][hora][Salir] =="
# -- 85 %: verde, con la hora; y el altavoz sigue abriendo el volumen --
run v1 /BAT85 'clic 590 14; sleep 0.8; shot v1b'
chk $T/v1.ppm tam 800 600 \
  px 582 12 8 "altavoz encendido (crema), corrido a la izquierda" \
  px 603 12 1 "hueco de barra entre el altavoz y la pila" \
  px 608 12 8 "marco crema de la pila (lado izquierdo)" \
  px 614 12 11 "relleno verde al 85 %" \
  texto 636 5 8 1 fina "85%" \
  texto 676 5 8 1 fina "23:58" \
  texto 736 5 8 4 fina "Salir"
chk $T/v1b.ppm marco 550 34 236 146 activa titulo 550 34 activa "Volumen" \
  texto 568 63 0 8 fina "Volumen: 80 %"
muestra v1 85
# -- 15 %: rojo --
run v2 /BAT15
chk $T/v2.ppm px 610 12 4 "relleno rojo (terracota) al 15 %" px 608 12 8 "marco crema" \
  texto 636 5 8 1 fina "15%" texto 676 5 8 1 fina "23:58" texto 736 5 8 4 fina "Salir"
muestra v2 15
# -- 100 % enchufada: verde con el rayo blanco --
run v3 /BAT100C
chk $T/v3.ppm px 614 12 11 "relleno verde al 100 %" px 618 13 9 "rayo blanco (cargando)" \
  texto 636 5 8 1 fina "100%" texto 676 5 8 1 fina "23:58" px 582 12 8 "altavoz encendido"
muestra v3 100c
# -- sobremesa (sin /BAT: QEMU no da batería): solo la hora, sin pila --
run v4 ""
chk $T/v4.ppm px 608 12 1 "sin pila (zona de barra, no hay batería)" \
  px 636 5 1 "sin número de batería" \
  texto 676 5 8 1 fina "23:58" \
  px 582 12 1 "sin pila, el altavoz NO se queda a la izquierda (hueco cerrado)" \
  px 650 12 8 "altavoz encendido, pegado a la hora" texto 736 5 8 4 fina "Salir"
muestra v4 sobremesa
[ -n "${CAPTURAS:-}" ] && { for n in v1 v2 v3 v4; do [ -f $T/$n.ppm ] && convert $T/$n.ppm "$CAPTURAS/ventanas_bateria_$n.png"; done; }
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/Ejecutando/,$p' $T/log.txt; fi
rm -rf $T
exit $fail
