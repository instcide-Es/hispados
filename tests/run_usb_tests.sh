#!/bin/bash
# Prueba de la imagen USB: arranque desde una memoria USB (xHCI) con un disco
# duro vacío, INSTALL, y arranque posterior solo desde el disco duro. También
# comprueba que INSTALL no toca un disco ya instalado si se cancela ([C]) o si
# se elige la instalación nueva ([N]) y luego se contesta que no al aviso.
# (Desde la 0.9.12 INSTALAR pregunta A/N/C en un disco que ya tiene Hispa-DOS;
# la prueba seguía contestando «N» como cuando la pregunta era de sí o no, y
# eso ahora es «Instalación nueva»: se quedaba esperando la confirmación.)
# USB image test: boot from USB, install to the hard disk, boot from the disk.
set -u
cd "$(dirname "$0")/.."
USBIMG=${1:-build/hispados-usb.img}
T=$(mktemp -d)
cp "$USBIMG" $T/usb.img
# (el menú de arranque de CONFIG.SYS, sin espera: opción «Normal»)
tests/tools/sinmenu.sh $T/usb.img || exit 1
dd if=/dev/zero of=$T/hdd.img bs=1M count=64 status=none
USBDEV="-device qemu-xhci,id=xhci -drive if=none,id=usbd,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,drive=usbd,bootindex=0"
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
# 1) arrancar desde USB e instalar
{
  sleep 6
  send "VER"
  send "DIR" 1.5
  send "INSTALL" 4
  send "S" 4                # crear la partición con todo el disco
  send "S" 6                # darle formato e instalar
  sleep 12
  send "DIR D:\\" 2
} | timeout 60 qemu-system-i386 -display none -serial stdio -monitor none $USBDEV \
      -drive if=none,id=hd,format=raw,file=$T/hdd.img -device ide-hd,drive=hd,bootindex=1 -no-reboot > $T/log1 2>&1
# 2) arrancar solo desde el disco duro
{
  sleep 5
  send "VER"
  send "DIR" 2
  send "INSTALL" 2
} | timeout 25 qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/hdd.img,format=raw,if=ide -boot c -no-reboot > $T/log2 2>&1
# 3) volver a arrancar desde USB: el disco ya tiene particiones -> no se toca
cp $T/hdd.img $T/hdd_antes.img
{
  sleep 6
  send "INSTALL" 4
  send "C" 3                # ya instalado en D:: [C] Cancelar, no se toca nada
  send "INSTALL" 3
  send "N" 3                # [N] Instalación nueva (formatearía)...
  send "N" 2                # ...pero a «¿Continuar (S/N)?» se contesta que no
  send "VER" 2
} | timeout 32 qemu-system-i386 -display none -serial stdio -monitor none $USBDEV \
      -drive if=none,id=hd,format=raw,file=$T/hdd.img -device ide-hd,drive=hd,bootindex=1 -no-reboot > $T/log3 2>&1
for i in 1 2 3; do tr -d '\r' < $T/log$i | iconv -f CP437 -t UTF-8 > $T/l$i.txt; done
fail=0
check() {
  if grep -qF -- "$2" "$1"; then echo "  OK   $2"; else echo "  FAIL $2"; fail=1; fi
}
echo "== Hispa-DOS USB image tests =="
check $T/l1.txt "Hispa-DOS 0.9.24 - núcleo compatible con MS-DOS 6.22"
check $T/l1.txt "HSPXMS 0.1: gestor XMS 3.0"
check $T/l1.txt "Bienvenido a Hispa-DOS"
check $T/l1.txt " El volumen de la unidad C es HISPADOS"
check $T/l1.txt "El disco duro no tiene particiones."
check $T/l1.txt "Partición DOS primaria creada."
check $T/l1.txt "Sistema transferido"
check $T/l1.txt "Hispa-DOS esta instalado."
check $T/l1.txt " Directorio de D:\\"
echo "-- arranque desde el disco duro --"
check $T/l2.txt "Hispa-DOS 0.9.24 - núcleo compatible con MS-DOS 6.22"
check $T/l2.txt "Bienvenido a Hispa-DOS"
check $T/l2.txt " Directorio de C:\\"
check $T/l2.txt "HISPADOS"
echo "-- INSTALL con el disco ya ocupado --"
check $T/l3.txt "Unidad para Hispa-DOS: D: (disco 2, partición 1"
check $T/l3.txt "[A] Actualizar (conserva tus datos y ajustes)"
check $T/l3.txt "Elige una opción y pulsa Intro (A/N/C): C"
n=$(grep -cF "No se ha instalado nada." $T/l3.txt)
if [ "$n" -ge 1 ]; then echo "  OK   [C] Cancelar: No se ha instalado nada."; else echo "  FAIL [C] Cancelar: No se ha instalado nada."; fail=1; fi
check $T/l3.txt "Se dará formato y se borrará TODO lo que tenga"
if [ "$n" -eq 2 ]; then echo "  OK   [N] y después «N»: No se ha instalado nada."; else echo "  FAIL [N] y después «N»: No se ha instalado nada. (sale $n veces)"; fail=1; fi
check $T/l3.txt "Hispa-DOS versión 0.9.24"
if cmp -s $T/hdd.img $T/hdd_antes.img; then echo "  OK   el disco duro no ha cambiado"; else echo "  FAIL el disco duro ha cambiado"; fail=1; fi
P=$(mktemp --suffix=.img)
dd if=$T/hdd.img of=$P bs=512 skip=63 status=none
if fsck.fat -n $P > $T/fsck 2>&1; then echo "  OK   fsck.fat del disco instalado"; else echo "  FAIL fsck.fat"; cat $T/fsck; fail=1; fi
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for i in 1 2 3; do echo "--- arranque $i ---"; cat $T/l$i.txt; done; fi
rm -rf $T $P
exit $fail
