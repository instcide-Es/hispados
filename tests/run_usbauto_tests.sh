#!/bin/bash
# [USB-AUTO] Pruebas de HSPUSB sin /F en los equipos sin ratón PS/2, del
# traspaso de los puertos de Intel (EHCI -> xHCI), de los ratones que se
# leen según su descriptor de informe y de los receptores de Bluetooth que
# se hacen pasar por teclado o ratón (docs/USB.md):
#   raton     RATONTST: el análisis del descriptor de informe de un ratón
#             (lib/hidraton.inc) con descriptores de varias clases
#   sin8042   PC sin 8042 (como un MacBook arrancado en modo BIOS): HSPUSB
#             sin opciones toma el USB él solo; teclado (al búfer de la BIOS)
#             y ratón (HSPRATON, MOUSETST) USB en el xHCI
#   ehci      lo mismo con un EHCI
#   descr     sin8042 con un HSPUSB que lee el ratón según su descriptor
#             (HR_FORZAR), no con los informes de arranque
#   arranque  arranque de la imagen USB (CONFIG.SYS «Normal», HSPUSB sin
#             opciones) en un PC sin 8042: toma también la memoria de
#             arranque (como /U), y el ratón y el teclado USB van
#   con8042   con 8042 y ratón PS/2: sin /F no se toca nada (como siempre)
#   sinaux    con 8042 pero sin ratón PS/2 (SIN_PS2=1): se toma como con /F
#   intel     el traspaso de puertos de Intel (INTEL_FAKE: QEMU no tiene esos
#             registros) con un EHCI y un xHCI: /INFO lo dice y todo va
#   proxy     receptores de Bluetooth (PROXY_TEST: los aparatos de QEMU
#             hacen de receptor): se dejan para el final y se usan si no
#             hay otro teclado ni ratón
# Sin segundo argumento se hacen todas.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
MODE=${2:-todo}
B=$(dirname "$IMG")
USBIMG=${USBIMG:-$B/hispados-usb.img}
fail=0
T=""
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon > /dev/null 2>&1; }
send() { sleep "${2:-1.5}"; printf '%s\r' "$1"; }
# teclear por el teclado USB (sendkey del monitor: va al teclado de QEMU)
teclea() {
  python3 - "$1" <<'EOF' | while read -r k; do mon "sendkey $k"; sleep 0.12; done
import sys
m = {' ': 'spc', '.': 'dot', '\\': 'backslash', '\r': 'ret'}
for ch in sys.argv[1]:
    print(m.get(ch, ch))
EOF
}
check() {
  if grep -qF -- "$1" $T/log.txt; then echo "  OK   $1"; else echo "  FAIL $1"; cfail=1; fi
}
nocheck() {
  if grep -qF -- "$1" $T/log.txt; then echo "  FAIL no debía salir: $1"; cfail=1; else echo "  OK   (no sale) $1"; fi
}
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
check_gestos() {
  check "X=320 Y=100 B=0"
  if grep -qE "^X=3[3-9][0-9] Y=100 B=0" $T/log.txt; then echo "  OK   movimiento horizontal"; else echo "  FAIL movimiento horizontal"; cfail=1; fi
  if grep -qE "^X=3[3-9][0-9] Y=1[0-9][0-9] B=0" $T/log.txt && ! grep -qE "^X=3[3-9][0-9] Y=100 B=1" $T/log.txt; then echo "  OK   movimiento vertical"; else echo "  FAIL movimiento vertical"; cfail=1; fi
  check " B=1"
  check "X=0 Y=0 B=0"
  check "Pulsaciones del botón izquierdo: 2"
}
caso() {  # nombre [opciones de HSPUSB] [variante -D...]
  T=$(mktemp -d)
  cp "$IMG" $T/f.img
  cfail=0
  echo "== Hispa-DOS USB automático ($1) =="
  printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPUSB.SYS %s\r\n' "${2:-}" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  if [ -n "${3:-}" ]; then
    python3 tools/cp437.py drivers/hspusb.asm $T/hspusb.asm
    if nasm -f bin -i $B/gen/lib/ -i lib/ $3 -o $T/HSPUSB.SYS $T/hspusb.asm; then
      mcopy -o -i $T/f.img $T/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
    else
      echo "  FAIL no se ensambla HSPUSB con $3"; cfail=1
    fi
  fi
}
qemu() {  # guion segundos [opciones de QEMU]
  local guion=$1 secs=$2; shift 2
  $guion | timeout $secs qemu-system-i386 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait \
      -drive file=$T/f.img,format=raw,if=floppy "$@" -boot a -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
}
fin() {
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  [ $cfail -ne 0 ] && fail=1
  rm -rf $T
}
quiere() { [ "$MODE" = todo ] || [ "$MODE" = "$1" ]; }

# ---- RATONTST: el descriptor de informe de los ratones ----
g_raton() {
  sleep 5
  send "BIN\\RATONTST" 2
  sleep 1
}
if quiere raton; then
  caso "RATONTST"
  mcopy -o -i $T/f.img $B/RATONTST.COM ::BIN/
  qemu g_raton 20
  check "Raton QEMU: id=-1 x=8/8 y=16/8 botones=0,1,2"
  check "  b=1 dx=5 dy=-5"
  check "Panel con identificador: id=2 x=8/8 y=16/8 botones=0,-1,-1"
  check "  b=1 dx=3 dy=-2"
  check "Receptor 12 bits: id=2 x=16/12 y=28/12 botones=0,1,2"
  check "  b=2 dx=-3 dy=4"
  n=$(grep -c "^  otro informe" $T/log.txt)
  if [ "$n" -eq 2 ]; then echo "  OK   informes de otro identificador descartados"; else echo "  FAIL informes de otro identificador: $n"; cfail=1; fi
  check "Tableta: no es un raton (sin X e Y relativas)"
  check "Raton 16 bits: id=-1 x=8/16 y=24/16 botones=0,1,2"
  check "  b=4 dx=300 dy=-100"
  fin
fi

# ---- sin 8042: teclado y ratón USB sin /F ----
g_usbauto() {
  sleep ${ESPERA:-7}
  teclea "rem usb sin 8042"$'\r'
  send "A:\\HISPADOS\\HSPRATON" 2
  send "BIN\\MOUSETST" 2
  gestos
  sleep 1
}
c_usbauto() {  # controladora (para el mensaje)
  check "este equipo no tiene controlador de teclado"
  check "(8042) ni ratón PS/2: HSPUSB lleva el USB (teclado, ratón y panel)."
  nocheck "La BIOS está usando el controlador USB"
  check "Teclado USB preparado."
  check "Sin controlador de teclado (8042): las teclas van al búfer de la BIOS."
  check "Ratón USB preparado."
  check "USB: teclado por $1, ratón por $1."
  check "A:\\>rem usb sin 8042"
  check "HSPRATON 0.1 - ratón de Hispa-DOS (INT 33h)"
  check_gestos
}
if quiere sin8042; then
  caso "sin 8042, xHCI"
  qemu g_usbauto 60 -machine pc,i8042=off -device qemu-xhci,id=hc \
      -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0
  c_usbauto "xHCI 1B36:000D"
  fin
fi
if quiere ehci; then
  caso "sin 8042, EHCI"
  qemu g_usbauto 60 -machine pc,i8042=off -device usb-ehci,id=hc \
      -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0
  c_usbauto "EHCI 8086:24CD"
  fin
fi
if quiere descr; then
  caso "sin 8042, ratón según su descriptor" "/INFO" "-DHR_FORZAR"
  ESPERA=24 qemu g_usbauto 75 -machine pc,i8042=off -device qemu-xhci,id=hc \
      -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0
  check "Ratón: informes según su descriptor (no de arranque); identificador 0"
  c_usbauto "xHCI 1B36:000D"
  fin
fi

# ---- arranque desde la memoria USB, sin 8042 (como en el MacBook) ----
g_arranque() {
  sleep 12
  teclea "rem tecla usb"$'\r'
  send "DIR C:\\HISPADOS\\HSPUSB.SYS" 2
  send "MOUSETST" 2
  gestos
  sleep 1
}
if quiere arranque; then
  T=$(mktemp -d)
  cfail=0
  echo "== Hispa-DOS USB automático (arranque desde la memoria USB, sin 8042) =="
  cp "$USBIMG" $T/usb.img
  tests/tools/sinmenu.sh $T/usb.img || cfail=1
  mcopy -o -i $T/usb.img@@$((2048*512)) $B/MOUSETST.COM ::HISPADOS/MOUSETST.COM || cfail=1
  g_arranque | timeout 70 qemu-system-i386 -display none -serial stdio \
      -monitor unix:$T/mon,server,nowait -machine pc,i8042=off -device qemu-xhci,id=hc \
      -drive if=none,id=usbd,format=raw,file=$T/usb.img \
      -device usb-storage,bus=hc.0,drive=usbd,bootindex=0 \
      -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0 -no-reboot > $T/log 2>&1
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 > $T/log.txt
  check "(8042) ni ratón PS/2: HSPUSB lleva el USB (teclado, ratón y panel)."
  check "También atiende la memoria USB de arranque (como con /U)."
  check "Disco de arranque USB atendido por HSPUSB (INT 13h)."
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "Bienvenido a Hispa-DOS"
  check "rem tecla usb"
  check "HSPUSB   SYS"
  check_gestos
  fin
fi

# ---- con 8042 y ratón PS/2: sin /F, como siempre ----
g_con() {
  sleep 6
  send "VER" 2
}
if quiere con8042; then
  caso "con 8042 y ratón PS/2"
  qemu g_con 25 -device qemu-xhci,id=hc -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0
  check "La BIOS está usando el controlador USB (teclado USB o unidad de"
  nocheck "HSPUSB lleva el USB"
  nocheck "Teclado USB preparado."
  check "Hispa-DOS versión"
  fin
fi

# ---- con 8042 pero sin ratón PS/2 ----
g_sinaux() {
  sleep 7
  send "A:\\HISPADOS\\HSPRATON" 2
  send "BIN\\MOUSETST" 2
  gestos
  sleep 1
}
if quiere sinaux; then
  caso "con 8042, sin ratón PS/2" "" "-DSIN_PS2=1"
  qemu g_sinaux 50 -device qemu-xhci,id=hc -device usb-kbd,bus=hc.0 -device usb-mouse,bus=hc.0
  check "La BIOS usa el USB y no hay ratón PS/2: HSPUSB lleva el USB (ratón USB)."
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  nocheck "Sin controlador de teclado (8042)"
  check_gestos
  fin
fi

# ---- traspaso de los puertos de Intel ----
if quiere intel; then
  caso "Intel: puertos del EHCI al xHCI" "/F /INFO" "-DINTEL_FAKE"
  ESPERA=24 qemu g_usbauto 80 -machine pc,i8042=off -device usb-ehci,id=he -device qemu-xhci,id=hc \
      -device usb-kbd,bus=hc.0 -device usb-mouse,bus=he.0
  check "Intel: puertos pasados del EHCI al xHCI: USB 2 XUSB2PR=0000000F (de 0000000F),"
  check "  USB 3 USB3_PSSEN=0000000F (de 0000000F)"
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "USB: teclado por xHCI 1B36:000D, ratón por EHCI 8086:24CD."
  check "A:\\>rem usb sin 8042"
  check_gestos
  fin
fi

# ---- receptores de Bluetooth (HID proxy) ----
if quiere proxy; then
  caso "receptor de Bluetooth" "/INFO" "-DPROXY_TEST"
  ESPERA=24 qemu g_usbauto 80 -machine pc,i8042=off -device qemu-xhci,id=hc -device usb-hub,bus=hc.0,port=1 \
      -device usb-kbd,bus=hc.0,port=1.1 -device usb-mouse,bus=hc.0,port=1.2
  check "Bluetooth (HID proxy): para después"
  check "Teclado USB preparado."
  check "Ratón USB preparado."
  check "A:\\>rem usb sin 8042"
  check_gestos
  fin
fi
exit $fail
