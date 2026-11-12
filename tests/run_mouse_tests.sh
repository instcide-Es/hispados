#!/bin/bash
# Prueba del ratón: HSPRATON (INT 33h) con el ratón PS/2 de QEMU, movido y
# pulsado desde el monitor (mouse_move, mouse_button) y leído con MOUSETST
# (INT 33h AX=3):
#   bios      vía BIOS (INT 15h), la automática cuando la BIOS ofrece ratón
#   directo   vía directa por el 8042 (/DIRECTO), con IntelliMouse y /INFO
#   simple    vía directa sin sondeos (/DIRECTO /SIMPLE): paquetes de 3 bytes
#   reserva   la BIOS no ofrece ratón (NOC2): la automática pasa a la directa
#   teclado   vía directa + KEYB SP: se teclea con el ratón moviéndose y no
#             se pierde ni se cambia ninguna tecla
#   sin8042   PC sin controlador 8042: se avisa y no se cuelga
#   imgusb    arranque de la imagen USB (su AUTOEXEC.BAT carga HSPRATON)
#   usb       ratón y teclado USB a través de HSPUSB.SYS (xHCI)
# Sin segundo argumento se hacen todas menos «usb».
# Mouse test: INT 33h driver, BIOS and direct 8042 paths, under QEMU.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
USBIMG=${USBIMG:-build/hispados-usb.img}
fail=0
T=""
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
keys() { for x in "$@"; do mon "sendkey $x"; sleep 0.08; done; }
check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; cfail=1; fi
}
nocheck() {
  if grep -qF -- "$1" $T/log.txt; then echo "  FAIL no debía salir: $1"; cfail=1; else echo "  OK   (no sale) $1"; fi
}
# mover y pulsar con MOUSETST en marcha
gestos() {
  sleep 1; mon "mouse_move 40 0"
  sleep 1; mon "mouse_move 0 24"
  sleep 1; mon "mouse_button 1"
  sleep 1; mon "mouse_button 0"
  sleep 1; mon "mouse_button 1"
  sleep 1; mon "mouse_button 0"
  sleep 1; mon "mouse_move -2000 -2000"
  sleep 1; mon "sendkey ret"
  sleep 2
}
# lo que MOUSETST tiene que haber visto por INT 33h
check_gestos() {
  check "Ratón: botones 2"
  check "Registros conservados"
  check "Versión del driver: 8.20"
  check "X=320 Y=100 B=0"
  # el ratón se ha movido a la derecha y hacia abajo, y vuelve a la esquina
  if grep -qE "^X=3[3-9][0-9] Y=100 B=0" $T/log.txt; then echo "  OK   movimiento horizontal"; else echo "  FAIL movimiento horizontal"; cfail=1; fi
  if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt && ! grep -qE "^X=3[3-9][0-9] Y=100 B=1" $T/log.txt; then echo "  OK   movimiento vertical"; else echo "  FAIL movimiento vertical"; cfail=1; fi
  check " B=1"
  check "X=0 Y=0 B=0"
  check "Pulsaciones del botón izquierdo: 2"
  check "Llamadas a la rutina: 2"
}
# arrancar QEMU con la entrada de la función $1 por el puerto serie
qemu() {  # guion segundos [opciones de QEMU]
  local guion=$1 secs=$2; shift 2
  $guion | timeout $secs qemu-system-i386 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait \
      -drive file=$T/f.img,format=raw,if=floppy "$@" -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
}
caso() {  # nombre: prepara el directorio y la imagen
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  cfail=0
  echo "== Hispa-DOS ratón ($1) =="
}
fin() {
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}

# ---- vía BIOS (automática) ----
g_bios() {
  sleep 6
  send "A:\\HISPADOS\\HSPRATON" 2
  send "BIN\\MOUSETST" 2
  gestos
  send "A:\\HISPADOS\\HSPRATON" 2
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  send "A:\\HISPADOS\\HSPRATON /NADA" 2
  sleep 1
}
t_bios() {
  caso "vía BIOS"
  qemu g_bios 45
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check "Vía BIOS (INT 15h). Ratón PS/2 preparado."
  check_gestos
  check "HSPRATON ya está instalado."
  check "INT 15h C2xxh: bien"
  check "Directa: no probada"
  check "En uso:  vía BIOS, Ratón PS/2, paquetes de 3 bytes"
  # los paquetes de los gestos están contados; ninguna IRQ 12 nuestra
  if grep -qE "IRQ12=0  bytes=0  paquetes=[1-9][0-9]*  descartes=" $T/log.txt; then echo "  OK   contadores de la vía BIOS"; else echo "  FAIL contadores de la vía BIOS"; cfail=1; fi
  check "Uso: HSPRATON [/BIOS | /DIRECTO] [/SIMPLE] [/INFO]"
  fin
}

# ---- vía directa (8042), con IntelliMouse y /INFO ----
g_directo() {
  sleep 6
  send "A:\\HISPADOS\\HSPRATON /directo /INFO" 2
  sleep 3
  send "BIN\\MOUSETST" 2
  gestos
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  send "VER" 2
  sleep 1
}
t_directo() {
  caso "vía directa, 8042"
  qemu g_directo 50
  check "Vía directa (8042, IRQ 12). Ratón PS/2 con rueda (IntelliMouse) preparado."
  check "INT 15h C2xxh: no probada"
  check "Directa: bien"
  check "reinicio=FA AA 00  id=03"
  check "prueba aux=00"
  check "En uso:  vía directa, Ratón PS/2 con rueda (IntelliMouse), paquetes de 4 bytes"
  nocheck "SIN IRQ 12"
  check_gestos
  check "HSPRATON ya está instalado."
  # con el driver cargado, /INFO enseña los contadores: IRQ 12, bytes y
  # paquetes de los gestos, y ninguno descartado
  if grep -qE "IRQ12=[1-9][0-9]*  bytes=[1-9][0-9]*  paquetes=[1-9][0-9]*  descartes=0  teclado=0  reinicios=0" $T/log.txt; then echo "  OK   contadores de la vía directa"; else echo "  FAIL contadores de la vía directa"; cfail=1; fi
  # el teclado sigue (la orden VER llega por el puerto serie; el eco, por la consola)
  check "Hispa-DOS versión"
  fin
}

# ---- vía directa sin sondeos: paquetes de 3 bytes ----
g_simple() {
  sleep 6
  send "A:\\HISPADOS\\HSPRATON /DIRECTO /SIMPLE" 2
  sleep 3
  send "BIN\\MOUSETST" 2
  gestos
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  sleep 1
}
t_simple() {
  caso "vía directa, /SIMPLE"
  qemu g_simple 50
  check "Vía directa (8042, IRQ 12). Ratón PS/2 preparado."
  check_gestos
  check "En uso:  vía directa, Ratón PS/2, paquetes de 3 bytes"
  check "id=--  identif=-- -- --"
  if grep -qE "paquetes=[1-9][0-9]*  descartes=0 " $T/log.txt; then echo "  OK   ningún paquete descartado"; else echo "  FAIL ningún paquete descartado"; cfail=1; fi
  fin
}

# ---- la BIOS no ofrece ratón: la vía automática pasa a la directa ----
g_reserva() {
  sleep 6
  send "BIN\\NOC2" 2
  send "A:\\HISPADOS\\HSPRATON /BIOS" 2
  sleep 1
}
g_reserva2() {
  sleep 6
  send "BIN\\NOC2" 2
  send "A:\\HISPADOS\\HSPRATON" 2
  sleep 3
  send "BIN\\MOUSETST" 2
  gestos
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  sleep 1
}
t_reserva() {
  caso "BIOS sin ratón, /BIOS"
  qemu g_reserva 20
  check "NOC2: la BIOS ya no ofrece el ratón PS/2."
  check "La BIOS no ofrece ratón PS/2 (INT 15h C205h, AH=86h)."
  check "No hay ratón PS/2: se usará un ratón USB (HSPUSB.SYS) si lo hay."
  nocheck "Vía directa"
  fin
  caso "BIOS sin ratón, automática"
  qemu g_reserva2 50
  check "Vía directa (8042, IRQ 12). Ratón PS/2 con rueda (IntelliMouse) preparado."
  check_gestos
  check "INT 15h C2xxh: fallo en C205h, AH=86h"
  check "Directa: bien"
  fin
}

# ---- teclado español con el ratón moviéndose (vía directa) ----
g_teclado() {
  sleep 6
  send "A:\\HISPADOS\\HSPRATON $TECL_VIA" 2
  sleep 3
  send "KEYB SP" 2
  sleep 2
  # el ratón no para mientras se teclea por el teclado PS/2 (sendkey)
  ( for i in $(seq 1 90); do mon "mouse_move 3 1"; mon "mouse_move -3 -1"; sleep 0.05; done ) &
  keys shift-r shift-e shift-m spc a b c d e f g h i j k l m n o p q r s t u v w x y z \
       0 1 2 3 4 5 6 7 8 9 spc semicolon apostrophe a shift-semicolon backslash ret
  wait
  sleep 1
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  sleep 1
}
t_teclado() {
  TECL_VIA=${TECL_VIA-/DIRECTO}
  caso "teclado con el ratón moviéndose"
  qemu g_teclado 60
  check "Vía directa (8042, IRQ 12). Ratón PS/2 con rueda (IntelliMouse) preparado."
  check "Teclado español instalado."
  check "REM abcdefghijklmnopqrstuvwxyz0123456789 ñáÑç"
  if grep -qE "paquetes=[1-9][0-9]+  descartes=0 " $T/log.txt; then echo "  OK   paquetes del ratón mientras se tecleaba, ninguno roto"; else echo "  FAIL paquetes del ratón mientras se tecleaba"; cfail=1; fi
  fin
}

# ---- PC sin controlador 8042 ----
g_sin8042() {
  sleep 6
  send "BIN\\NOC2" 2
  send "A:\\HISPADOS\\HSPRATON /INFO" 2
  sleep 6
  send "VER" 2
  sleep 1
}
t_sin8042() {
  caso "PC sin 8042"
  qemu g_sin8042 30 -machine pc,i8042=off
  check "Directa: este PC no tiene controlador 8042"
  check "Vía directa: este PC no tiene controlador 8042."
  check "No hay ratón PS/2: se usará un ratón USB (HSPUSB.SYS) si lo hay."
  check "Hispa-DOS versión"
  fin
}

# ---- imagen USB: arranque como en un portátil sin disquetera ----
g_imgusb() {
  sleep 9
  send "MOUSETST" 2
  gestos
  send "HSPRATON /INFO" 2
  sleep 1
}
t_imgusb() {
  T=$(mktemp -d)
  cfail=0
  echo "== Hispa-DOS ratón (imagen USB, AUTOEXEC.BAT) =="
  if [ ! -f "$USBIMG" ]; then echo "  FAIL falta $USBIMG (make usb)"; fail=1; rm -rf $T; return; fi
  cp "$USBIMG" $T/usb.img
  mcopy -o -i $T/usb.img@@$((2048*512)) build/MOUSETST.COM ::HISPADOS/MOUSETST.COM || cfail=1
  g_imgusb | timeout 50 qemu-system-i386 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait -device qemu-xhci,id=xhci \
      -drive if=none,id=usbd,format=raw,file=$T/usb.img \
      -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0 -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check "Ratón PS/2 preparado."
  check_gestos
  check "En uso:  vía BIOS, Ratón PS/2, paquetes de 3 bytes"
  fin
}

# ---- ratón y teclado USB con HSPUSB.SYS ----
g_usb() {
  sleep 6
  send "A:\\HISPADOS\\HSPRATON" 2
  send "BIN\\MOUSETST" 2
  gestos
  send "A:\\HISPADOS\\HSPRATON" 2
  sleep 1
}
t_usb() {
  caso "usb"
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  qemu g_usb 40 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,id=ukbd -device usb-mouse,bus=xhci.0,id=umouse
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check_gestos
  check "HSPRATON ya está instalado."
  fin
}

case "$MODE" in
  ps2|bios) t_bios ;;
  directo)  t_directo ;;
  simple)   t_simple ;;
  reserva)  t_reserva ;;
  teclado)  t_teclado ;;
  sin8042)  t_sin8042 ;;
  imgusb)   t_imgusb ;;
  usb)      t_usb ;;
  todo)     t_bios; t_directo; t_simple; t_reserva; t_teclado; t_sin8042; t_imgusb ;;
  *)        echo "modo desconocido: $MODE"; exit 2 ;;
esac
exit $fail
