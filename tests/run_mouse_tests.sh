#!/bin/bash
# Prueba del ratón: HSPRATON (INT 33h) con el ratón PS/2 de QEMU y, si se pasa
# "usb", con un ratón y un teclado USB a través de HSPUSB.SYS (xHCI).
# Mouse test: INT 33h driver with QEMU's PS/2 mouse (and USB HID with "usb").
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-ps2}
T=$(mktemp -d)
cp "$IMG" $T/f.img
EXTRA=""
if [ "$MODE" = usb ]; then
  printf 'DEVICE=A:\\DRIVERS\\HSPUSB.SYS\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  EXTRA="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,id=ukbd -device usb-mouse,bus=xhci.0,id=umouse"
fi
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
{
  sleep 6
  send "A:\\DRIVERS\\HSPRATON" 2
  send "BIN\\MOUSETST" 2
  sleep 1; mon "mouse_move 40 0"
  sleep 1; mon "mouse_move 0 24"
  sleep 1; mon "mouse_button 1"
  sleep 1; mon "mouse_button 0"
  sleep 1; mon "mouse_button 1"
  sleep 1; mon "mouse_button 0"
  sleep 1; mon "mouse_move -2000 -2000"
  sleep 1; mon "sendkey ret"
  sleep 2
  send "A:\\DRIVERS\\HSPRATON" 2
  sleep 1
} | timeout 40 qemu-system-i386 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait \
      -drive file=$T/f.img,format=raw,if=floppy $EXTRA -boot a -no-reboot > $T/log 2>&1
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== Hispa-DOS ratón ($MODE) =="
check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
if [ "$MODE" = ps2 ]; then check "Ratón PS/2 preparado."; fi
check "Ratón: botones 2"
check "Versión del driver: 8.20"
check "X=320 Y=100 B=0"
# el ratón se ha movido a la derecha y hacia abajo, y vuelve a la esquina
if grep -qE "^X=3[3-9][0-9] Y=100 B=0" $T/log.txt; then echo "  OK   movimiento horizontal"; else echo "  FAIL movimiento horizontal"; fail=1; fi
if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt && ! grep -qE "^X=3[3-9][0-9] Y=100 B=1" $T/log.txt; then echo "  OK   movimiento vertical"; else echo "  FAIL movimiento vertical"; fail=1; fi
check " B=1"
check "X=0 Y=0 B=0"
check "Pulsaciones del botón izquierdo: 2"
check "Llamadas a la rutina: 2"
check "HSPRATON ya está instalado."
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
