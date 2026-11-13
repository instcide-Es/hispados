#!/bin/bash
# Prueba de los concentradores USB (hubs) con HSPUSB.SYS y del arranque desde
# una memoria USB servida por la BIOS:
#  1) teclado, ratón y memoria detrás de un concentrador: se teclea por el
#     teclado USB, se mueve el ratón USB y se lee y escribe la memoria;
#  2) dos concentradores encadenados, con /INFO (árbol de dispositivos);
#  3) quitar y poner la memoria que cuelga de un concentrador (y una petición
#     de control rechazada, STALL, no deja mudo al concentrador);
#  4) arranque desde la memoria USB (imagen usb/) con el CONFIG.SYS de serie:
#     HSPUSB no le quita el controlador a la BIOS, nada se cuelga y el
#     teclado USB (el de la BIOS) sigue funcionando;
#  5) lo mismo con /U: HSPUSB toma el controlador, atiende él el disco de
#     arranque (INT 13h) y el teclado que cuelga del concentrador.
#  6) equipo sin controlador de teclado (8042), como los Mac: las teclas del
#     teclado USB van al búfer de la BIOS, también con TECLADO SP (ñ, acentos).
# USB hub tests (QEMU usb-hub behind xHCI) and safe boot from a BIOS USB disk.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
USBIMG=${2:-build/hispados-usb.img}
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
nocheck() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin «$1»"; fi; }
tolog() { tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt; }
show() { if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi; }
mkstick() {    # fichero etiqueta
  dd if=/dev/zero of=$1 bs=1M count=32 status=none
  mformat -i $1 -T 65536 -h 64 -s 32 -v $2 ::
  printf 'Hola desde la memoria %s\r\n' $2 > $T/HOLA.TXT
  mcopy -i $1 $T/HOLA.TXT ::HOLA.TXT
}
floppy() {     # opciones de HSPUSB
  cp "$IMG" $T/f.img
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS %s\r\n' "$1" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
}

# ---- 1) teclado + ratón + memoria detrás de un concentrador ----
T=$(mktemp -d)
floppy "/F"
mkstick $T/usb.img USBHUB
Q="python3 tests/tools/qmp.py $T/qmp"
qemu-system-i386 -display none -serial file:$T/log -monitor none -parallel none \
    -qmp unix:$T/qmp,server,nowait -trace enable=usb_xhci_xfer_success,file=$T/trace \
    -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
    -device usb-hub,bus=xhci.0,port=1,id=hub1 \
    -device usb-kbd,bus=xhci.0,port=1.1,id=ukbd,display=vga0 \
    -device usb-mouse,bus=xhci.0,port=1.2,id=umouse \
    -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,port=1.3,drive=u \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 10
$Q type vga0 "VER" key vga0 ret wait 1 \
   type vga0 "MOSTRAR D:\\HOLA.TXT" key vga0 ret wait 2 \
   type vga0 "COPIAR A:\\HISPADOS\\EDITAR.COM D:\\" key vga0 ret wait 4 \
   type vga0 "CREARDIR D:\\NUEVA" key vga0 ret wait 2 \
   type vga0 "A:\\HISPADOS\\HSPRATON" key vga0 ret wait 2 \
   type vga0 "BIN\\MOUSETST" key vga0 ret wait 2 \
   move - 40 0 wait 1 move - 0 24 wait 1 \
   btn - 1 wait 1 btn - 0 wait 1 \
   key vga0 ret wait 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tolog
echo "== Hispa-DOS concentrador USB: teclado, ratón y memoria =="
check "Disco USB: 32 MB en la unidad D:"
check "Teclado USB preparado."
check "Ratón USB preparado."
check "A:\\>VER"
check "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)"
check "Hola desde la memoria USBHUB"
check "X=320 Y=100 B=0"
if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt; then ok "movimiento del ratón USB"; else ko "movimiento del ratón USB"; fi
check " B=1"
check "Pulsaciones del botón izquierdo: 1"
if grep -q "len 4" $T/trace; then ok "informes del ratón por el xHCI"; else ko "informes del ratón por el xHCI"; fi
if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria"; else ko "fsck.fat"; cat $T/fsck; fi
if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM) build/EDIT.COM; then ok "EDITAR.COM copiado intacto"; else ko "copia de EDITAR.COM"; fi
if mdir -i $T/usb.img ::NUEVA > /dev/null 2>&1; then ok "carpeta NUEVA creada"; else ko "carpeta NUEVA"; fi
show
rm -rf $T

# ---- 2) dos concentradores encadenados, /INFO ----
T=$(mktemp -d)
floppy "/F /INFO"
mkstick $T/usb.img USBDOS
{
  sleep 26
  for c in "MOSTRAR D:\\HOLA.TXT" "COPIAR A:\\HISPADOS\\*.COM D:\\" "LISTAR D:\\"; do
    printf '%s\r' "$c"; sleep 4
  done
  sleep 2
} | timeout 70 qemu-system-i386 -display none -serial stdio -monitor none \
      -device qemu-xhci,id=xhci \
      -device usb-hub,bus=xhci.0,port=1,id=hub1 \
      -device usb-hub,bus=xhci.0,port=1.2,id=hub2 \
      -device usb-kbd,bus=xhci.0,port=1.2.1 \
      -device usb-mouse,bus=xhci.0,port=1.2.3 \
      -drive if=none,id=u,format=raw,file=$T/usb.img -device usb-storage,bus=xhci.0,port=1.2.4,drive=u \
      -device usb-tablet,bus=xhci.0,port=1.4 \
      -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot > $T/log 2>&1
tolog
echo "== Hispa-DOS concentrador USB: dos niveles y /INFO =="
check "Controladores USB: UHCI 0, OHCI 0, EHCI 0, xHCI 1"
check "BIOS: suyo no, en marcha sí, arranque USB no"
check "  5 FS 0409:55AA 09/00/00 -> concentrador de 8 puertos"
check "    5.2 FS 0409:55AA 09/00/00 -> concentrador de 8 puertos"
check "      5.2.1 FS 0627:0001 03/01/01 -> teclado"
check "      5.2.3 FS 0627:0001 03/01/02 -> ratón"
check "      5.2.4 FS 46F4:0001 08/06/50 -> disco"
check "    5.4 FS 0627:0001 03/00/00 -> sin uso"
check "Disco USB: 32 MB en la unidad D:"
check "Teclado USB preparado."
check "Ratón USB preparado."
check "Hola desde la memoria USBDOS"
check "El volumen de la unidad D es USBDOS"
if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria"; else ko "fsck.fat"; cat $T/fsck; fi
if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM) build/EDIT.COM; then ok "EDITAR.COM copiado intacto"; else ko "copia de EDITAR.COM"; fi
show
rm -rf $T

# ---- 3) quitar y poner la memoria que cuelga del concentrador ----
# (con un HSPUSB que antes le hace al concentrador una petición que rechaza:
# el EP0 parado por el STALL tiene que reanudarse o nada de lo demás iría)
T=$(mktemp -d)
floppy "/F"
python3 tools/cp437.py drivers/hspusb.asm $T/hspusb.asm
if nasm -f bin -i build/gen/lib/ -i lib/ -DSTALL_TEST -o $T/HSPUSB.SYS $T/hspusb.asm; then
  mcopy -o -i $T/f.img $T/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
else
  ko "no se ensambla HSPUSB con STALL_TEST"
fi
mkstick $T/UNO.img MEMUNO
mkstick $T/DOS.img MEMDOS
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
typ() { python3 - "$1" <<'EOF' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1
import sys
m = {' ': 'spc', '.': 'dot', ':': 'shift-semicolon', '\\': 'backslash'}
for ch in sys.argv[1]:
    k = m.get(ch, ch.lower())
    if ch.isupper(): k = 'shift-' + k
    print('sendkey ' + k)
print('sendkey ret')
EOF
}
qemu-system-i386 -display none -serial file:$T/log -monitor unix:$T/mon,server,nowait -parallel none \
  -trace enable=usb_xhci_xfer_error,file=$T/trace \
  -device qemu-xhci,id=xhci -device usb-hub,bus=xhci.0,port=1,id=hub1 \
  -device usb-mouse,bus=xhci.0,port=1.1 \
  -drive if=none,id=u1,format=raw,file=$T/UNO.img \
  -device usb-storage,bus=xhci.0,port=1.2,drive=u1,id=m1 \
  -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 9
typ "DIR D:"; sleep 2
mon "device_del m1"; sleep 2
typ "DIR D:"; sleep 3
printf 'sendkey d\n' | socat - UNIX-CONNECT:$T/mon >/dev/null 2>&1; sleep 1   # «Descartar»
mon "drive_add 0 if=none,id=u2,format=raw,file=$T/DOS.img"
mon "device_add usb-storage,bus=xhci.0,port=1.3,drive=u2,id=m2"; sleep 3
typ "DIR D:"; sleep 3
typ "TYPE D:HOLA.TXT"; sleep 2
typ "MD D:\\NUEVA"; sleep 3
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tolog
echo "== Hispa-DOS concentrador USB: quitar y poner la memoria =="
if grep -q "usb_xhci_xfer_error" $T/trace; then ok "la petición rechazada (STALL) ha pasado por el xHCI"; else ko "no ha habido STALL"; fi
check "El volumen de la unidad D es MEMUNO"
check "La unidad no está preparada"
check "El volumen de la unidad D es MEMDOS"
check "Hola desde la memoria MEMDOS"
if mdir -i $T/DOS.img ::NUEVA > /dev/null 2>&1; then ok "carpeta NUEVA creada en la segunda memoria"; else ko "carpeta NUEVA en la segunda memoria"; fi
if mdir -i $T/UNO.img ::NUEVA > /dev/null 2>&1; then ko "NUEVA en la memoria equivocada"; else ok "la primera memoria no tiene NUEVA"; fi
for n in UNO DOS; do
  if fsck.fat -n $T/$n.img > /dev/null 2>&1; then ok "fsck.fat memoria $n"; else ko "fsck.fat memoria $n"; fi
done
show
rm -rf $T

# ---- 6) equipo sin controlador de teclado (8042), como los Mac ----
T=$(mktemp -d)
floppy "/F"
Q="python3 tests/tools/qmp.py $T/qmp"
qemu-system-i386 -machine pc,i8042=off -display none -serial file:$T/log -monitor none -parallel none \
    -qmp unix:$T/qmp,server,nowait \
    -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
    -device usb-hub,bus=xhci.0,port=1,id=hub1 \
    -device usb-kbd,bus=xhci.0,port=1.1,id=ukbd,display=vga0 \
    -device usb-mouse,bus=xhci.0,port=1.2,id=umouse \
    -drive file=$T/f.img,format=raw,if=floppy -boot a -no-reboot &
QP=$!
sleep 10
$Q type vga0 "VER" key vga0 ret wait 1 \
   type vga0 "ECHO Hola: 1-2, a.b" key vga0 ret wait 1 \
   key vga0 up key vga0 ret wait 1 \
   type vga0 "A:\\HISPADOS\\HSPRATON" key vga0 ret wait 2 \
   type vga0 "BIN\\MOUSETST" key vga0 ret wait 2 \
   move - 40 0 wait 1 move - 0 24 wait 1 \
   btn - 1 wait 1 btn - 0 wait 1 \
   key vga0 ret wait 2 \
   type vga0 "A:\\HISPADOS\\TECLADO SP" key vga0 ret wait 2 \
   type vga0 "ECHO a" key vga0 semicolon type vga0 "o" key vga0 ret wait 1 \
   type vga0 "ECHO " key vga0 apostrophe type vga0 "a" key vga0 ret wait 2
kill $QP 2>/dev/null; wait $QP 2>/dev/null
tolog
echo "== Hispa-DOS teclado y ratón USB sin controlador de teclado (8042) =="
check "Teclado USB preparado."
check "Sin controlador de teclado (8042): las teclas van al búfer de la BIOS."
check "A:\\>VER"
check "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)"
n=$(grep -c "^Hola: 1-2, a.b$" $T/log.txt)
if [ "$n" -ge 2 ]; then ok "mayúsculas, signos y flecha arriba (historial)"; else ko "mayúsculas, signos y flecha arriba: $n"; fi
check "X=320 Y=100 B=0"
if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt; then ok "movimiento del ratón USB"; else ko "movimiento del ratón USB"; fi
check "Pulsaciones del botón izquierdo: 1"
check "Teclado español instalado."
if grep -q "^año$" $T/log.txt; then ok "ñ con TECLADO SP"; else ko "ñ con TECLADO SP"; fi
if grep -q "^á$" $T/log.txt; then ok "tecla muerta (á) con TECLADO SP"; else ko "tecla muerta (á) con TECLADO SP"; fi
show
rm -rf $T

# ---- 4) y 5) arranque desde la memoria USB con teclado tras un concentrador ----
for MODE in serie U; do
  T=$(mktemp -d)
  cp "$USBIMG" $T/usb.img
  # (el menú de arranque de CONFIG.SYS, sin espera: opción «Normal»)
  tests/tools/sinmenu.sh $T/usb.img || fail=1
  # sin TECLADO SP: las teclas de QEMU son las del teclado americano
  printf '@ECHO OFF\r\nPROMPT $P$G\r\nPATH C:\\HISPADOS\r\n' > $T/AUTOEXEC.BAT
  mcopy -o -i $T/usb.img@@1048576 $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
  if [ $MODE = U ]; then
    printf 'DEVICE=C:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=C:\\HISPADOS\\HSPUSB.SYS /U\r\nFILES=30\r\nBUFFERS=20\r\n' > $T/CONFIG.SYS
    mcopy -o -i $T/usb.img@@1048576 $T/CONFIG.SYS ::CONFIG.SYS
  fi
  Q="python3 tests/tools/qmp.py $T/qmp"
  qemu-system-i386 -display none -serial file:$T/log -monitor none -parallel none \
      -qmp unix:$T/qmp,server,nowait \
      -vga none -device VGA,id=vga0 -device qemu-xhci,id=xhci \
      -device usb-hub,bus=xhci.0,port=1,id=hub1 \
      -device usb-kbd,bus=xhci.0,port=1.1,id=ukbd,display=vga0 \
      -device usb-mouse,bus=xhci.0,port=1.2,id=umouse \
      -drive if=none,id=usbd,format=raw,file=$T/usb.img \
      -device usb-storage,bus=xhci.0,port=2,drive=usbd,bootindex=0 -no-reboot &
  QP=$!
  sleep 10
  $Q type vga0 "VER" key vga0 ret wait 1 \
     type vga0 "COPY C:\\LEEME.TXT C:\\COPIA.TXT" key vga0 ret wait 3 \
     type vga0 "MD C:\\NUEVA" key vga0 ret wait 2 \
     type vga0 "COPY C:\\HISPADOS\\EDITAR.COM C:\\NUEVA" key vga0 ret wait 4 \
     type vga0 "DIR C:\\NUEVA" key vga0 ret wait 3
  kill $QP 2>/dev/null; wait $QP 2>/dev/null
  tolog
  dd if=$T/usb.img of=$T/p.img bs=1M skip=1 status=none
  if [ $MODE = serie ]; then
    echo "== Hispa-DOS arranque desde USB, CONFIG.SYS de serie (HSPUSB no toma el controlador) =="
    check "HSPUSB 0.1 - USB nativo de Hispa-DOS (xHCI)"
    check "HSPUSB no toma el controlador"
    nocheck "Teclado USB preparado."
  else
    echo "== Hispa-DOS arranque desde USB con /U (HSPUSB atiende el disco de arranque) =="
    check "Disco de arranque USB atendido por HSPUSB (INT 13h)."
    check "Teclado USB preparado."
    check "Ratón USB preparado."
    nocheck "Disco USB:"
  fi
  check "C:\\>VER"
  check "Hispa-DOS versión 0.9 (compatible con MS-DOS 6.22)"
  check " Directorio de C:\\NUEVA"
  check "EDITAR   COM"
  if fsck.fat -n $T/p.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria de arranque"; else ko "fsck.fat"; cat $T/fsck; fi
  if cmp -s <(mtype -i $T/p.img ::COPIA.TXT) <(mtype -i $T/p.img ::LEEME.TXT); then ok "COPIA.TXT igual que LEEME.TXT"; else ko "COPIA.TXT"; fi
  if cmp -s <(mtype -i $T/p.img ::NUEVA/EDITAR.COM) build/EDIT.COM; then ok "EDITAR.COM copiado intacto"; else ko "copia de EDITAR.COM"; fi
  show
  rm -rf $T
done
exit $fail
