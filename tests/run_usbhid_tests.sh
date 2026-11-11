#!/bin/bash
# Prueba del teclado y el ratón USB con HSPUSB.SYS (xHCI de QEMU): todo se
# teclea por el teclado USB y el ratón se mueve por el ratón USB (QMP).
# USB keyboard and mouse test (input sent only to the USB devices).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf 'DEVICE=A:\\DRIVERS\\HSPUSB.SYS\r\n' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
Q="python3 tests/tools/qmp.py $T/qmp"
qemu-system-i386 -display none -serial file:$T/log -monitor none -parallel none \
    -qmp unix:$T/qmp,server,nowait -trace enable=usb_xhci_xfer_success,file=$T/trace \
    -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0,id=ukbd,display=vga0 \
    -device usb-mouse,bus=xhci.0,id=umouse \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 8
$Q type vga0 "VER" key vga0 ret wait 1 \
   type vga0 "ECHO Hola" key vga0 ret wait 1 \
   key vga0 up key vga0 ret wait 1 \
   type vga0 "A:\\DRIVERS\\HSPRATON" key vga0 ret wait 2 \
   type vga0 "BIN\\MOUSETST" key vga0 ret wait 2 \
   move - 40 0 wait 1 move - 0 24 wait 1 \
   btn - 1 wait 1 btn - 0 wait 1 \
   key vga0 ret wait 2
kill $QP 2>/dev/null
wait $QP 2>/dev/null
tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
fail=0
check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; fail=1; fi
}
echo "== Hispa-DOS teclado y ratón USB =="
check "HSPUSB 0.1 - USB nativo de Hispa-DOS (xHCI)"
check "Teclado USB preparado."
check "Ratón USB preparado."
check "A:\\>VER"
check "Hispa-DOS versión 0.2 (compatible con MS-DOS 6.22)"
check "A:\\>ECHO Hola"
# la flecha arriba (tecla extendida) repite la orden: dos "Hola"
n=$(grep -c "^Hola$" $T/log.txt)
if [ "$n" -ge 2 ]; then echo "  OK   flecha arriba (historial)"; else echo "  FAIL flecha arriba (historial): $n"; fail=1; fi
check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
check "X=320 Y=100 B=0"
if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt; then echo "  OK   movimiento del ratón USB"; else echo "  FAIL movimiento del ratón USB"; fail=1; fi
check " B=1"
check "Pulsaciones del botón izquierdo: 1"
# los informes del ratón (4 bytes) han pasado por nuestro xHCI, no por el PS/2
if grep -q "len 4" $T/trace; then echo "  OK   informes del ratón USB por el xHCI"; else echo "  FAIL informes del ratón USB por el xHCI"; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
rm -rf $T
exit $fail
