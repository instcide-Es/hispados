#!/bin/bash
# Pruebas de HSPUSB con cada clase de controladora USB (docs/USB.md). QEMU
# tiene las cuatro:
#   uhci      piix3-usb-uhci (USB 1.x de Intel: puertos de E/S)
#   ohci      pci-ohci (USB 1.x de los demás)
#   ehci      usb-ehci sola (USB 2.0 sin controladoras compañeras: solo
#             aparatos de alta velocidad; el concentrador de QEMU es de USB
#             1.1 y no se puede conectar, y el mando va a alta velocidad)
#   ehcicomp  ich9-usb-ehci1 con sus tres compañeras ich9-usb-uhci1..3, como
#             en un PC de 2002-2012: lo de alta velocidad se queda en el
#             EHCI y lo de USB 1.x se cede a las UHCI (bit Port Owner)
#   xhci      qemu-xhci (USB 3)
#   mezcla    las cuatro a la vez: teclado en la UHCI, ratón en la OHCI,
#             memoria en el EHCI y mando en el xHCI
#   aparcar   dos UHCI: quitar la memoria (su UHCI se queda sin nada y se
#             aparca) y ponerla en el otro puerto (se arranca con la lista
#             de tramas de reserva)
# Con cada una, lo mismo:
#   A) una memoria USB en un puerto raíz, con /INFO: leer, crear una
#      carpeta, copiar, borrar y listar; después fsck.fat y comparar;
#   C) lo mismo que A, pero en modo V86 (HSPEMM) y con HSPUSB cargado en
#      la memoria superior (DEVICEHIGH), y el teclado USB;
#   B) teclado en un puerto raíz (todo lo de «teclado» se escribe por él) y
#      un concentrador con ratón, memoria y mando detrás (en «ehci», todo
#      en puertos raíz; en «ehcicomp», teclado y memoria en el EHCI y el
#      concentrador, con ratón y mando, en una UHCI compañera): órdenes por
#      el teclado USB, memoria (leer, copiar, carpeta), ratón (HSPRATON y
#      MOUSETST) y mando (HSPJOY, INT 15h).
# Sin segundo argumento se hacen todas.
# HSPUSB tests on every USB host controller class QEMU provides.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
B=$(dirname "$IMG")
fail=0
T=""
QP=""
MP=""

check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; cfail=1; fi
}
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; cfail=1; }
veces() { grep -aoF -- "$1" $T/log 2>/dev/null | wc -l; }
espera() {  # texto [veces]
  local n=${2:-1} i=0
  while [ "$(veces "$1")" -lt "$n" ] && [ $i -lt 600 ]; do sleep 0.1; i=$((i + 1)); done
  [ $i -lt 600 ] || echo "  (no llega: $1)"
}
orden() {
  local n=$(veces 'A:\>')
  printf '%s\r' "$1" >&3
  espera 'A:\>' $((n + 1))
}
caso() {  # nombre, opciones de HSPUSB
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  mcopy -o -i $T/f.img $B/JOYTEST.COM ::BIN/
  printf 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS %s\r\n' "$2" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  printf '[mandos]\r\njuegos=no\r\n' > $T/NADA.INI
  mcopy -o -i $T/f.img $T/NADA.INI ::
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBDATA ::
  printf 'Hola desde la memoria USB\r\n' > $T/HOLA.TXT
  mcopy -i $T/usb.img $T/HOLA.TXT ::HOLA.TXT
  cfail=0
  echo "== Hispa-DOS USB, controladora $1 =="
}
arranca() {  # opciones de QEMU: la consola por el puerto serie
  local a=() x
  for x in "$@"; do a+=("${x//@T@/$T}"); done   # (@T@: la carpeta de este caso)
  mkfifo $T/in
  qemu-system-i386 -display none -serial stdio -monitor none \
      -drive file=$T/f.img,format=raw,if=floppy "${a[@]}" -boot a -no-reboot < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
  espera 'A:\>'
}
mando() {  # socket perfil órdenes...
  USBMANDO_VE=$T/log python3 tests/tools/usbmando.py "$@" &
  MP="$MP $!"
}
fin() {
  exec 3>&-
  kill $QP 2>/dev/null
  wait $QP $MP 2>/dev/null
  MP=""
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  "$@"
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}
# solo una parte: PARTE=A, PARTE=B o PARTE=C
PARTE=${PARTE:-ABC}
quiere() { [ "$MODE" = todo ] || [ "$MODE" = "$1" ]; }
STICK() { echo "-drive if=none,id=u,format=raw,file=@T@/usb.img -device usb-storage,bus=$1,drive=u,id=mem"; }

# ---- A) la memoria en un puerto raíz ----
c_disco() {
  check "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)"
  check "$CUENTA"
  check "$LINEA"
  check "08/06/50 -> disco"
  check "Disco USB: 32 MB en la unidad D:"
  check "Hola desde la memoria USB"
  check " Directorio de D:\\HISPA"
  check "EDITAR   COM"
  check "TECLADO  COM"
  if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat"; cat $T/fsck; fi
  if cmp -s <(mtype -i $T/usb.img ::HISPA/EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto"; else ko "copia de EDITAR.COM"; fi
  if mdir -i $T/usb.img ::HOLA.TXT > /dev/null 2>&1; then ko "HOLA.TXT no se borró"; else ok "HOLA.TXT borrado"; fi
}
disco() {  # nombre, bus, opciones de QEMU de la controladora
  case $PARTE in *A*) ;; *) return;; esac
  caso "$1: memoria en un puerto raíz" "/F /INFO"
  local bus=$2; shift 2
  # (la pausa de /INFO: 15 s)
  arranca "$@" $(STICK $bus)
  orden 'MOSTRAR D:\HOLA.TXT'
  orden 'CREARDIR D:\HISPA'
  orden 'COPIAR A:\HISPADOS\*.COM D:\HISPA'
  orden 'COPIAR A:\README.TXT D:\'
  orden 'BORRAR D:\HOLA.TXT'
  orden 'LISTAR D:\HISPA'
  fin c_disco
}

# ---- B) teclado, ratón, memoria y mando ----
c_todo() {
  check "Disco USB: 32 MB en la unidad D:"
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "Mando USB preparado: 054C:09CC, 6 ejes y 14 botones."
  # el teclado USB
  check "A:\\>VER"
  check "A:\\>ECHO Hola"
  n=$(grep -c "^Hola$" $T/log.txt)
  if [ "$n" -ge 2 ]; then ok "flecha arriba del teclado USB (historial)"; else ko "flecha arriba (historial): $n"; fi
  # la memoria
  check "Hola desde la memoria USB"
  check "NUEVA           <DIR>"
  if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat"; cat $T/fsck; fi
  if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto"; else ko "copia de EDITAR.COM"; fi
  # el ratón
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check "X=320 Y=100 B=0"
  if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt; then ok "movimiento del ratón USB"; else ko "movimiento del ratón USB"; fi
  check "Pulsaciones del botón izquierdo: 1"
  # el mando
  check "Mando 1: USB 054C:09CC (6 ejes, 14 botones)"
  check "INT15: botones=D0 X1=1 Y1=193 X2=193 Y2=193"
  check "Mando 1: tipo=1 6/14 ejes=0 128 128 128 0 0 cruz=0 botones=0002"
  [ -z "$ARBOL" ] || check "$ARBOL"
}
todo() {  # nombre, opciones de QEMU (controladora y aparatos)
  case $PARTE in *B*) ;; *) return;; esac
  caso "$1: teclado, ratón, memoria y mando" "/F /INFO"
  shift
  mando $T/redir ds4 espera "Mirando" wait .6 rep 0100808080280000
  Q="python3 tests/tools/qmp.py $T/qmp"
  arranca -qmp unix:$T/qmp,server,nowait -vga none -device VGA,id=vga0 \
      -chardev socket,id=rd,path=$T/redir,server=on,wait=off "$@"
  n=$(veces 'A:\>')
  $Q type vga0 "VER" key vga0 ret
  espera 'A:\>' $((n + 1))
  n=$(veces 'A:\>')
  $Q type vga0 "ECHO Hola" key vga0 ret wait 1 key vga0 up key vga0 ret
  espera 'A:\>' $((n + 2))
  orden 'MOSTRAR D:\HOLA.TXT'
  orden 'COPIAR A:\HISPADOS\EDITAR.COM D:\'
  orden 'CREARDIR D:\NUEVA'
  orden 'LISTAR D:\'
  orden 'A:\HISPADOS\HSPRATON'
  n=$(veces 'A:\>')
  printf 'BIN\\MOUSETST\r' >&3
  espera 'X=320 Y=100 B=0'
  $Q wait 1 move - 40 0 wait 1 move - 0 24 wait 1 btn - 1 wait 1 btn - 0 wait 1 key vga0 ret
  espera 'A:\>' $((n + 1))
  orden 'HSPJOY /I:A:\NADA.INI'
  orden 'MANDO'
  orden 'BIN\JOYTEST W 70'
  orden 'BIN\JOYTEST'
  fin c_todo
}

# ---- C) en modo V86 (HSPEMM) y cargado en la memoria superior ----
c_v86() {
  check "HSPEMM"
  check "Disco USB: 32 MB en la unidad D:"
  check "Teclado USB preparado."
  check "Hola desde la memoria USB"
  check "A:\\>ECHO Teclado USB va"
  if fsck.fat -n $T/usb.img > $T/fsck 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat"; cat $T/fsck; fi
  if cmp -s <(mtype -i $T/usb.img ::EDITAR.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM copiado intacto a la memoria"; else ko "copia de EDITAR.COM a la memoria"; fi
  if cmp -s <(mtype -i $T/f.img ::VUELTA.COM 2>/dev/null) $B/EDIT.COM; then ok "EDITAR.COM leído intacto de la memoria"; else ko "lectura de EDITAR.COM de la memoria"; fi
  # MEM /C: lo que ocupa el sistema en la memoria superior es HSPUSB
  n=$(sed -n '/memoria superior (UMB):/,$p' $T/log.txt | awk '$1 == "SISTEMA" { gsub(/\./, "", $2); print $2; exit }')
  if [ -n "$n" ] && [ "$n" -ge 20000 ]; then ok "HSPUSB cargado en la memoria superior ($n bytes)"; else ko "HSPUSB no está en la memoria superior ($n)"; fi
}
v86() {  # nombre, opciones de QEMU
  case $PARTE in *C*) ;; *) return;; esac
  caso "$1: modo V86 (HSPEMM) y DEVICEHIGH" "/F"
  shift
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=HIGH,UMB\r\nDEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS /F\r\n' > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  Q="python3 tests/tools/qmp.py $T/qmp"
  arranca -m 32 -net none -qmp unix:$T/qmp,server,nowait -vga none -device VGA,id=vga0 "$@"
  orden 'MEM /C'
  orden 'TYPE D:\HOLA.TXT'
  orden 'COPY A:\HISPADOS\EDITAR.COM D:\'
  orden 'COPY D:\EDITAR.COM A:\VUELTA.COM'
  n=$(veces 'A:\>')
  $Q type vga0 "ECHO Teclado USB va" key vga0 ret
  espera 'A:\>' $((n + 1))
  fin c_v86
}
KBD="usb-kbd,id=ukbd,display=vga0"

if quiere uhci; then
  HC="-device piix3-usb-uhci,id=hc"
  CUENTA="Controladores USB: UHCI 1, OHCI 0, EHCI 0, xHCI 0"
  LINEA="UHCI puertos con algo conectado: 1 (de 2)"
  disco uhci hc.0 $HC
  ARBOL="    2.2 FS 46F4:0001 08/06/50 -> disco"
  todo uhci $HC -device $KBD,bus=hc.0,port=1 -device usb-hub,bus=hc.0,port=2 \
      -device usb-mouse,bus=hc.0,port=2.1 $(STICK hc.0),port=2.2 -device usb-redir,chardev=rd,bus=hc.0,port=2.3
  v86 uhci $HC -device $KBD,bus=hc.0 $(STICK hc.0)
fi
if quiere ohci; then
  HC="-device pci-ohci,id=hc"
  CUENTA="Controladores USB: UHCI 0, OHCI 1, EHCI 0, xHCI 0"
  LINEA="OHCI puertos con algo conectado: 1 (de 3)"
  disco ohci hc.0 $HC
  ARBOL="    2.2 FS 46F4:0001 08/06/50 -> disco"
  todo ohci $HC -device $KBD,bus=hc.0,port=1 -device usb-hub,bus=hc.0,port=2 \
      -device usb-mouse,bus=hc.0,port=2.1 $(STICK hc.0),port=2.2 -device usb-redir,chardev=rd,bus=hc.0,port=2.3
  v86 ohci $HC -device $KBD,bus=hc.0 $(STICK hc.0)
fi
if quiere ehci; then
  HC="-device usb-ehci,id=hc"
  CUENTA="Controladores USB: UHCI 0, OHCI 0, EHCI 1, xHCI 0"
  LINEA="EHCI puertos con algo conectado: 1 (de 6)"
  disco ehci hc.0 $HC
  ARBOL="  2 HS 0627:0001 03/01/02 -> ratón"
  USBMANDO_VEL=2 todo ehci $HC -device $KBD,bus=hc.0,port=1 -device usb-mouse,bus=hc.0,port=2 \
      $(STICK hc.0),port=3 -device usb-redir,chardev=rd,bus=hc.0,port=4
  v86 ehci $HC -device $KBD,bus=hc.0 $(STICK hc.0)
fi
if quiere ehcicomp; then
  HC="-device ich9-usb-ehci1,id=hc,addr=1d.7,multifunction=on
      -device ich9-usb-uhci1,masterbus=hc.0,firstport=0,addr=1d.0,multifunction=on
      -device ich9-usb-uhci2,masterbus=hc.0,firstport=2,addr=1d.1,multifunction=on
      -device ich9-usb-uhci3,masterbus=hc.0,firstport=4,addr=1d.2,multifunction=on"
  CUENTA="Controladores USB: UHCI 3, OHCI 0, EHCI 1, xHCI 0"
  LINEA="EHCI puertos con algo conectado: 1 (de 6)"
  disco ehcicomp hc.0 $HC
  # el concentrador (USB 1.1) está en el puerto 3 del EHCI: cedido, sale en
  # el puerto 1 de la segunda UHCI
  ARBOL="    1.2 FS 054C:09CC 03/00/00 -> mando de juego"
  todo ehcicomp $HC -device $KBD,bus=hc.0,port=1 $(STICK hc.0),port=2 -device usb-hub,bus=hc.0,port=3 \
      -device usb-mouse,bus=hc.0,port=3.1,usb_version=1 -device usb-redir,chardev=rd,bus=hc.0,port=3.2
  # (el teclado, de USB 1.x: va a una UHCI compañera; la memoria, al EHCI)
  v86 ehcicomp $HC -device $KBD,bus=hc.0,usb_version=1 $(STICK hc.0)
fi
if quiere xhci; then
  HC="-device qemu-xhci,id=hc"
  CUENTA="Controladores USB: UHCI 0, OHCI 0, EHCI 0, xHCI 1"
  LINEA="xHCI puertos con algo conectado: 1 (de 8)"
  disco xhci hc.0 $HC
  ARBOL="    5.2 FS 46F4:0001 08/06/50 -> disco"
  todo xhci $HC -device $KBD,bus=hc.0,port=2 -device usb-hub,bus=hc.0,port=1 \
      -device usb-mouse,bus=hc.0,port=1.1 $(STICK hc.0),port=1.2 -device usb-redir,chardev=rd,bus=hc.0,port=1.3
  v86 xhci $HC -device $KBD,bus=hc.0 $(STICK hc.0)
fi

# ---- mezcla: las cuatro a la vez, cada aparato en una ----
if quiere mezcla; then
  ARBOL="Controladores USB: UHCI 1, OHCI 1, EHCI 1, xHCI 1"
  todo "las cuatro a la vez" -device qemu-xhci,id=hx -device usb-ehci,id=he -device piix3-usb-uhci,id=hu \
      -device pci-ohci,id=ho -device $KBD,bus=hu.0 -device usb-mouse,bus=ho.0 $(STICK he.0) \
      -device usb-redir,chardev=rd,bus=hx.0
fi

# ---- aparcar: una UHCI que se queda sin nada y vuelve a hacer falta ----
c_aparcar() {
  check "Teclado USB preparado."
  check "El volumen de la unidad D es MEMUNO"
  check "La unidad no está preparada"
  check "El volumen de la unidad D es MEMDOS"
  check "Hola desde la memoria MEMDOS"
  if mdir -i $T/DOS.img ::NUEVA > /dev/null 2>&1; then ok "carpeta NUEVA creada en la segunda memoria"; else ko "carpeta NUEVA en la segunda memoria"; fi
  for n in UNO DOS; do
    if fsck.fat -n $T/$n.img > /dev/null 2>&1; then ok "fsck.fat memoria $n"; else ko "fsck.fat memoria $n"; fi
  done
}
if quiere aparcar; then
  caso "UHCI: aparcar y volver a arrancar" "/F"
  for n in UNO DOS; do
    dd if=/dev/zero of=$T/$n.img bs=1M count=32 status=none
    mformat -i $T/$n.img -T 65536 -h 64 -s 32 -v MEM$n ::
    printf 'Hola desde la memoria MEM%s\r\n' $n > $T/HOLA.TXT
    mcopy -i $T/$n.img $T/HOLA.TXT ::HOLA.TXT
  done
  mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
  mkfifo $T/in
  qemu-system-i386 -display none -serial stdio -monitor unix:$T/mon,server,nowait \
      -drive file=$T/f.img,format=raw,if=floppy \
      -device piix3-usb-uhci,id=ha -device piix3-usb-uhci,id=hb \
      -device usb-kbd,bus=ha.0,port=1 \
      -drive if=none,id=u1,format=raw,file=$T/UNO.img -device usb-storage,bus=hb.0,port=1,drive=u1,id=m1 \
      -boot a -no-reboot < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
  espera 'A:\>'
  orden 'DIR D:'
  mon "device_del m1"; sleep 2
  n=$(veces 'A:\>')
  printf 'DIR D:\r' >&3
  espera "no est"; sleep 1
  printf 'd' >&3                       # «Descartar»
  espera 'A:\>' $((n + 1))
  mon "drive_add 0 if=none,id=u2,format=raw,file=$T/DOS.img"
  mon "device_add usb-storage,bus=hb.0,port=2,drive=u2,id=m2"; sleep 3
  orden 'DIR D:'
  orden 'TYPE D:HOLA.TXT'
  orden 'MD D:\NUEVA'
  fin c_aparcar
fi
exit $fail
