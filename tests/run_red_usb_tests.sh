#!/bin/bash
# Prueba de la red por USB (docs/RED.md, docs/USB.md): un adaptador
# USB-Ethernet con la clase estándar CDC-ECM, que lleva HSPUSB.SYS
# (lib/usb_red.inc) y que HSPRED usa como una tarjeta más.
# QEMU tiene -device usb-net: ofrece RNDIS en su configuración 1 y CDC-ECM en
# la 2 (justo el caso de «hay que elegir la configuración»), a velocidad
# plena (USB 1.1). Casos:
#   xhci, uhci, ohci        el adaptador en un puerto raíz
#   xhcihub, uhcihub,       detrás de un concentrador (-device usb-hub)
#   ohcihub
#   ehci                    ich9-usb-ehci1 con sus tres UHCI compañeras: el
#                           EHCI cede el adaptador (USB 1.1) a una de ellas
#   todo                    xHCI con concentrador: teclado, ratón, memoria y
#                           el adaptador a la vez (se copia de la memoria
#                           USB con la red en marcha)
#   v86                     con HSPEMM (modo V86): HSPUSB con DEVICEHIGH y
#                           LH HSPRED
#   v86uhci                 lo mismo con una UHCI
#   sinecm                  HSPUSB ensamblado con -DRED_SIN_ECM (como un
#                           adaptador que solo trae RNDIS o su modo propio):
#                           lo dice con su VID:PID, y HSPRED no lo encuentra
#   consola                 CONSOLA (servidor TCP, tests/consola_cliente.py)
#                           por el adaptador, con xHCI
#   pciusb                  una tarjeta PCI (RTL8139) y el adaptador: HSPRED
#                           usa la PCI; HSPRED /USB, el adaptador
#   sinusb3                 HSPUSB /2 con el xHCI de Intel simulado
#                           (-DINTEL_FAKE): no enciende el USB 3
#   memoria                 la memoria convencional libre (MEM /C) con el
#                           adaptador y sin él: sin él, HSPUSB no se queda
#                           con nada de la red
#   tecladoehci             [USB-TARDE] lo del MacBook Air (17/11/2026): el
#                           teclado en un EHCI y, en el xHCI, memoria, dos
#                           concentradores encadenados, ratón y el adaptador,
#                           todo puesto al arrancar. La lista periódica del
#                           EHCI no cabía en la zona y el teclado salía
#                           «preparado» y mudo. Se teclea por el teclado USB
#                           (sendkey del monitor) antes y después de poner
#                           la red en marcha
#   dosteclados             [USB-DOS] dos teclados (xHCI y EHCI) y dos
#                           ratones, con el adaptador: van los cuatro (se
#                           quita un teclado y se teclea por el otro, en los
#                           dos sentidos; se mueve un ratón y luego el otro)
#   muchos                  más de 8 aparatos en uso a la vez (10: la reserva
#                           era de 8 y en el MacBook se llenaba)
#   sinred                  sin /RED en la línea de HSPUSB: la red por USB está
#                           apagada, el adaptador no se toca (se dice cómo
#                           encenderla) y HSPUSB ocupa lo mismo que sin él
#   sinsitio                HSPUSB con -DRED_SIN_SITIO (como si al acabar la
#                           enumeración no quedara memoria para el
#                           adaptador): se dice, la red no se usa y el
#                           teclado sigue yendo
#   sinlista                HSPUSB con -DFL_FIN_FALLA (como si no cupiera la
#                           lista periódica del EHCI): se dice y no se
#                           cuelga
# En cada uno de los de red: HSPUSB /INFO («red (CDC-ECM)», eligiendo la
# configuración 2), HSPRED lo carga con la MAC del adaptador, HSPRED /INFO,
# CONFRED (DHCP: 10.0.2.15), PING 10.0.2.2, DESCARGAR de un servidor HTTP
# propio (150.000 octetos, se compara con mtools; un 404), PING por nombre
# (DNS propio, tests/red_dns.py), HSPRED /U y volver a cargarlo.
# Todo va en un espacio de nombres de red propio (unshare -rn), como
# tests/run_red_tests.sh. Dos QEMU a la vez como mucho (JOBS=1: de uno en
# uno). Variables: CASOS="xhci uhcihub" (solo ésos), VERBOSE=1,
# GUARDAR=carpeta (deja allí la pantalla de cada caso), VOLCAR=carpeta (las
# tramas de cada caso, en pcap).
# Lo que QEMU NO puede probar: el adaptador a alta velocidad en un EHCI o a
# supervelocidad (usb-net es de USB 1.1), un concentrador de USB 2.0 o 3.0
# (el de QEMU es de USB 1.1) y un Realtek RTL8153 de verdad.
# USB network test (CDC-ECM adapter through HSPUSB, as an HSPRED card).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=$(dirname "$IMG")
for f in HSPRED CONFRED PING DESCARGAR; do
  [ -f $B/$f.COM ] || { echo "falta $B/$f.COM (make)"; exit 1; }
done
if [ -z "${RED_NS:-}" ] && unshare -rn true 2>/dev/null; then
  RED_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${RED_NS:-}" ]; then
  ip link set lo up
fi
W=$(mktemp -d)
PORT=8080
mkdir -p $W/www
python3 -c "import random; random.seed(11); open('$W/www/PRUEBA.BIN','wb').write(bytes(random.getrandbits(8) for _ in range(150000)))"
python3 -m http.server --bind 127.0.0.1 --directory $W/www $PORT > $W/http.log 2>&1 &
HTTP_PID=$!
DNS=1
python3 tests/red_dns.py 127.0.0.1 53 > $W/dns.log 2>&1 &
DNS_PID=$!
sleep 1
if ! kill -0 $DNS_PID 2>/dev/null; then
  DNS=0
  echo "  (sin servidor DNS propio: no se puede usar el puerto 53; se salta el PING por nombre)"
fi
trap 'kill $HTTP_PID $DNS_PID $(jobs -p) 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $W/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $W' EXIT
printf 'DHCP=SI\r\nDNS=10.0.2.2\r\n' > $W/RED2.INI
U="http://10.0.2.2:$PORT"
MAC="40:54:00:12:34:56"     # la que da usb-net en su cadena iMACAddress

# ---- herramientas de cada caso (cada caso corre en su subproceso) ----
T=""
QP=""
cfail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; cfail=1; }
check() { if grep -qF -- "$1" $T/log.txt; then ok "$1"; else ko "$1"; fi; }
checkn() { if grep -qF -- "$1" $T/log.txt; then ko "(no debía salir) $1"; else ok "sin «$1»"; fi; }
veces() { grep -aoF -- "$1" $T/log 2>/dev/null | wc -l; }
# Se espera a lo que se busca (no un tiempo fijo), con un tope generoso: el
# de cada espera por TOPE (3 de serie), para que con la máquina cargada (la
# batería entera a la vez) no se quede corto. Si llega antes, no se espera.
# Una máquina que no contesta no puede tener la prueba parada horas (pasó: 58
# esperas agotadas, casi 3 horas): tras la primera espera agotada de un caso,
# las siguientes aguardan 30 s como mucho, y tras la segunda, 2 s. Y el tope
# se mide con el reloj (SECONDS), no contando vueltas, que con la máquina
# cargada duran bastante más de una décima.
TOPE=${TOPE:-3}
SIN=0       # esperas agotadas en este caso
espera() {  # texto [veces] [décimas de segundo]
  local n=${2:-1} max=$(( ${3:-600} * TOPE )) hasta
  if [ $SIN -ge 2 ]; then max=20; elif [ $SIN -ge 1 ] && [ $max -gt 300 ]; then max=300; fi
  hasta=$(( SECONDS + (max + 9) / 10 ))
  while [ "$(veces "$1")" -lt "$n" ]; do
    if [ $SECONDS -ge $hasta ]; then SIN=$((SIN + 1)); echo "  (no llega: $1)"; return; fi
    sleep 0.1
  done
}
orden() {   # teclear una orden y esperar a que vuelva el indicador
  local n=$(veces 'A:\>')
  printf '%s\r' "$1" >&3
  espera 'A:\>' $((n + 1)) ${2:-600}
}
caso() {  # nombre, título, líneas de CONFIG.SYS (con \r\n), [variante -D...]
  NOMBRE=$1
  T=$W/$1
  mkdir -p $T
  SIN=0
  cp "$IMG" $T/f.img
  mdel -i $T/f.img ::HISPADOS/HISPA.WAV 2>/dev/null   # sitio para lo que se descarga
  cfail=0
  echo "== Hispa-DOS red por USB: $2 =="
  printf '%b' "$3" > $T/CONFIG.SYS
  mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
  mcopy -o -i $T/f.img $W/RED2.INI ::RED2.INI
  if [ -n "${4:-}" ]; then
    python3 tools/cp437.py drivers/hspusb.asm $T/hspusb.asm
    if nasm -f bin -i $B/gen/lib/ -i lib/ $4 -o $T/HSPUSB.SYS $T/hspusb.asm; then
      mcopy -o -i $T/f.img $T/HSPUSB.SYS ::HISPADOS/HSPUSB.SYS
    else
      ko "no se ensambla HSPUSB con $4"
    fi
  fi
}
arranca() {  # opciones de QEMU (la consola, por el puerto serie)
  local a=() x
  for x in "$@"; do a+=("${x//@T@/$T}"); done
  mkfifo $T/in
  qemu-system-i386 -display none -serial stdio -monitor none -parallel none -m 32 \
      -drive file=$T/f.img,format=raw,if=floppy "${a[@]}" -boot a -no-reboot \
      ${VOLCAR:+-object filter-dump,id=f0,netdev=n0,file=$VOLCAR/$NOMBRE.pcap} < $T/in > $T/log 2>&1 &
  QP=$!
  exec 3> $T/in
  espera 'A:\>' 1 900
}
fin() {
  exec 3>&-
  kill $QP 2>/dev/null
  wait $QP 2>/dev/null
  tr -d '\r' < $T/log | iconv -f CP437 -t UTF-8 -c > $T/log.txt
  cp $T/log.txt $W/$NOMBRE.txt
  "$@"
  if [ $cfail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then cat $T/log.txt; fi
  return $cfail
}
NET="-netdev user,id=n0"
USBNET() { echo "-device usb-net,netdev=n0,id=red,bus=$1"; }
STICK() { echo "-drive if=none,id=u,format=raw,file=@T@/usb.img -device usb-storage,bus=$1,drive=u,id=mem"; }
SOLOUSB='DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED /INFO\r\n'

# las órdenes de red de todos los casos
ordenes_red() {
  orden "HSPRED"
  orden "HSPRED /INFO"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:3"
  orden "DESCARGAR $U/PRUEBA.BIN" 900
  orden "DESCARGAR $U/no-existe.txt"
  if [ $DNS = 1 ]; then
    orden 'DEL A:\HISPADOS\RED.INI'
    orden 'COPY A:\RED2.INI A:\HISPADOS\RED.INI'
    orden "CONFRED /RENOVAR"
    orden "PING prueba.hispados /N:2"
  fi
  orden "HSPRED /INFO"
  orden "HSPRED /U"
  orden "PING 10.0.2.2 /N:1"
  orden "HSPRED"
  orden "PING 10.0.2.2 /N:2"
  orden "ECHO fin-de-la-prueba"
}
# ...y lo que tiene que haber salido. $1 = línea de /INFO de HSPUSB, $2 =
# texto de la controladora en HSPRED /INFO
c_red() {
  check "HSPUSB 0.2 - USB nativo de Hispa-DOS (UHCI, OHCI, EHCI y xHCI)"
  check "$1"
  check "Adaptador de red USB preparado (CDC-ECM), MAC $MAC: cargue HSPRED."
  check "Tarjeta Adaptador USB (CDC-ECM), MAC $MAC, por HSPUSB, packet driver en INT 60h"
  check "USB:             0525:A4A2, velocidad plena (12 Mbit/s), por $2 (lo lleva HSPUSB)"
  check "Dirección MAC:   $MAC"
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Puerta de enlace  . . : 10.0.2.2"
  check "Obtenida  . . . . . . : por DHCP (servidor 10.0.2.2"
  check "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  check "Guardado en PRUEBA.BIN (150.000 octetos)."
  rm -f $T/prueba.bin
  mcopy -i $T/f.img ::PRUEBA.BIN $T/prueba.bin 2>/dev/null
  if cmp -s $T/prueba.bin $W/www/PRUEBA.BIN; then ok "PRUEBA.BIN igual que en el servidor (mtools)"; else ko "PRUEBA.BIN distinto o no está"; fi
  check "El servidor contesta: HTTP/1.0 404 File not found"
  if [ $DNS = 1 ]; then
    check "Haciendo ping a prueba.hispados [10.0.2.2] con 32 octetos de datos:"
  fi
  check "Dirección IP:    10.0.2.15 (DHCP)"
  if grep -qE "Paquetes:        recibidos [0-9]+, enviados [0-9]+, descartados [0-9]+, errores 0$" $T/log.txt; then ok "HSPRED /INFO: sin errores de envío ni de recepción"; else ko "HSPRED /INFO: hay errores (o no sale la línea de paquetes)"; fi
  check "HSPRED descargado."
  check "No hay ningún controlador de red cargado"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  check "fin-de-la-prueba"
  n1=$(grep -c "núcleo compatible con MS-DOS" $T/log.txt)
  if [ "$n1" -eq 1 ]; then ok "no se ha reiniciado"; else ko "se ha reiniciado o no ha arrancado ($n1 arranques)"; fi
  if fsck.fat -n $T/f.img >/dev/null 2>&1; then ok "fsck.fat del disquete"; else ko "fsck.fat del disquete"; fi
}
directo() {  # nombre, título, bus, línea de /INFO, clase, opciones de QEMU...
  local n=$1 t=$2 bus=$3 linea=$4 hc=$5; shift 5
  caso $n "$t" "$SOLOUSB"
  arranca "$@" $NET $(USBNET $bus)
  ordenes_red
  fin c_red "$linea" "$hc"
}
EHCICOMP="-device ich9-usb-ehci1,id=hc,addr=1d.7,multifunction=on
    -device ich9-usb-uhci1,masterbus=hc.0,firstport=0,addr=1d.0,multifunction=on
    -device ich9-usb-uhci2,masterbus=hc.0,firstport=2,addr=1d.1,multifunction=on
    -device ich9-usb-uhci3,masterbus=hc.0,firstport=4,addr=1d.2,multifunction=on"
ECM="FS 0525:A4A2 02/06/00 0A/00/00 -> red (CDC-ECM)"

t_xhci() { directo xhci "xHCI, puerto raíz" hc.0 "  5 $ECM" xHCI -device qemu-xhci,id=hc; }
t_uhci() { directo uhci "UHCI, puerto raíz" hc.0 "  1 $ECM" UHCI -device piix3-usb-uhci,id=hc; }
t_ohci() { directo ohci "OHCI, puerto raíz" hc.0 "  1 $ECM" OHCI -device pci-ohci,id=hc; }
t_ehci() { directo ehci "EHCI con UHCI compañeras (el adaptador, cedido a una UHCI)" hc.0 "  1 $ECM" UHCI $EHCICOMP; }
conhub() {  # nombre, título, clase, opciones de QEMU...
  local n=$1 t=$2 hc=$3; shift 3
  caso $n "$t" "$SOLOUSB"
  arranca "$@" -device usb-hub,bus=hc.0,port=1,id=hub1 $NET -device usb-net,netdev=n0,id=red,bus=hc.0,port=1.2
  ordenes_red
  fin c_hub "$hc"
}
c_hub() {
  check "FS 0409:55AA 09/00/00 -> concentrador de 8 puertos"
  if grep -qF -- ".2 $ECM" $T/log.txt; then ok "detrás del concentrador: .2 $ECM"; else ko "detrás del concentrador: .2 $ECM"; fi
  c_red "$ECM" "$1"
}
t_xhcihub() { conhub xhcihub "xHCI, detrás de un concentrador" xHCI -device qemu-xhci,id=hc; }
t_uhcihub() { conhub uhcihub "UHCI, detrás de un concentrador" UHCI -device piix3-usb-uhci,id=hc; }
t_ohcihub() { conhub ohcihub "OHCI, detrás de un concentrador" OHCI -device pci-ohci,id=hc; }

# xHCI con concentrador: teclado, ratón, memoria y el adaptador a la vez
t_todo() {
  caso todo "xHCI: teclado, ratón, memoria y adaptador de red a la vez" "$SOLOUSB"
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBDATA ::
  printf 'Hola desde la memoria USB\r\n' > $T/HOLA.TXT
  mcopy -i $T/usb.img $T/HOLA.TXT ::HOLA.TXT
  arranca -device qemu-xhci,id=hc -device usb-hub,bus=hc.0,port=1,id=hub1 \
      -device usb-kbd,bus=hc.0,port=1.1 -device usb-mouse,bus=hc.0,port=1.3 \
      $(STICK hc.0),port=1.4 $NET -device usb-net,netdev=n0,id=red,bus=hc.0,port=1.2
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:3"
  orden 'MOSTRAR D:\HOLA.TXT'
  orden "DESCARGAR $U/PRUEBA.BIN D:\\PRUEBA.BIN" 900
  orden 'COPIAR D:\PRUEBA.BIN A:\COPIA.BIN' 900
  orden "PING 10.0.2.2 /N:2"
  orden "HSPRED /INFO"
  orden "ECHO fin-de-la-prueba"
  fin c_todo
}
c_todo() {
  check "03/01/01 -> teclado"
  check "03/01/02 -> ratón"
  check "08/06/50 -> disco"
  if grep -qF -- ".2 $ECM" $T/log.txt; then ok "detrás del concentrador: .2 $ECM"; else ko "detrás del concentrador: .2 $ECM"; fi
  check "Disco USB: 32 MB en la unidad D:"
  check "Teclado USB preparado."
  check "Adaptador de red USB preparado (CDC-ECM), MAC $MAC: cargue HSPRED."
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  check "Hola desde la memoria USB"
  check "Guardado en D:\\PRUEBA.BIN (150.000 octetos)."
  rm -f $T/a.bin $T/b.bin
  mcopy -i $T/usb.img ::PRUEBA.BIN $T/a.bin 2>/dev/null
  if cmp -s $T/a.bin $W/www/PRUEBA.BIN; then ok "PRUEBA.BIN descargado a la memoria USB, igual que en el servidor"; else ko "PRUEBA.BIN de la memoria USB distinto o no está"; fi
  mcopy -i $T/f.img ::COPIA.BIN $T/b.bin 2>/dev/null
  if cmp -s $T/b.bin $W/www/PRUEBA.BIN; then ok "copiado de la memoria USB al disquete, igual"; else ko "la copia al disquete es distinta o no está"; fi
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  if grep -qE "Paquetes:        recibidos [0-9]+, enviados [0-9]+, descartados [0-9]+, errores 0$" $T/log.txt; then ok "HSPRED /INFO: sin errores"; else ko "HSPRED /INFO: hay errores"; fi
  check "fin-de-la-prueba"
  if fsck.fat -n $T/usb.img >/dev/null 2>&1; then ok "fsck.fat de la memoria USB"; else ko "fsck.fat de la memoria USB"; fi
}

# con HSPEMM (modo V86): HSPUSB arriba (DEVICEHIGH) y LH HSPRED
V86CFG='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS NOEMS\r\nDOS=HIGH,UMB\r\nFILES=30\r\nDEVICEHIGH=A:\\HISPADOS\\HSPUSB.SYS /F /RED /INFO\r\n'
v86() {  # nombre, título, clase, opciones de QEMU
  local n=$1 t=$2 hc=$3; shift 3
  caso $n "$t" "$V86CFG"
  arranca "$@" $NET $(USBNET hc.0)
  orden "LH HSPRED"
  orden "MEM /C"
  orden "HSPRED /INFO"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:3"
  orden "DESCARGAR $U/PRUEBA.BIN" 900
  orden "HSPRED /INFO"
  orden "HSPRED /U"
  orden "ECHO fin-de-la-prueba"
  fin c_v86 "$hc"
}
c_v86() {
  check "HSPEMM 0.1"
  check "$ECM"
  check "Adaptador de red USB preparado (CDC-ECM), MAC $MAC: cargue HSPRED."
  # MEM /C: lo que ocupa el sistema en la memoria superior es HSPUSB
  n=$(sed -n '/memoria superior (UMB):/,$p' $T/log.txt | awk '$1 == "SISTEMA" { gsub(/\./, "", $2); print $2; exit }')
  if [ -n "$n" ] && [ "$n" -ge 20000 ]; then ok "HSPUSB cargado en la memoria superior ($n octetos, con el bloque de la red)"; else ko "HSPUSB no está en la memoria superior ($n)"; fi
  if sed -n '/Programas en la memoria superior/,/^  Total/p' $T/log.txt | grep -q HSPRED; then ok "HSPRED en la memoria superior (MEM /C)"; else ko "HSPRED en la memoria superior (MEM /C)"; fi
  check "USB:             0525:A4A2, velocidad plena (12 Mbit/s), por $1 (lo lleva HSPUSB)"
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  check "Guardado en PRUEBA.BIN (150.000 octetos)."
  rm -f $T/prueba.bin
  mcopy -i $T/f.img ::PRUEBA.BIN $T/prueba.bin 2>/dev/null
  if cmp -s $T/prueba.bin $W/www/PRUEBA.BIN; then ok "PRUEBA.BIN igual que en el servidor (mtools)"; else ko "PRUEBA.BIN distinto o no está"; fi
  if grep -qE "Paquetes:        recibidos [0-9]+, enviados [0-9]+, descartados [0-9]+, errores 0$" $T/log.txt; then ok "HSPRED /INFO: sin errores"; else ko "HSPRED /INFO: hay errores"; fi
  check "HSPRED descargado."
  check "fin-de-la-prueba"
}
t_v86() { v86 v86 "xHCI con HSPEMM (modo V86), DEVICEHIGH y LH" xHCI -device qemu-xhci,id=hc; }
t_v86uhci() { v86 v86uhci "UHCI con HSPEMM (modo V86), DEVICEHIGH y LH" UHCI -device piix3-usb-uhci,id=hc; }

# un adaptador de red sin CDC-ECM: se dice cuál es, y HSPRED no lo encuentra
t_sinecm() {
  caso sinecm "adaptador sin CDC-ECM (HSPUSB con -DRED_SIN_ECM)" "$SOLOUSB" "-DRED_SIN_ECM"
  arranca -device qemu-xhci,id=hc -device usb-kbd,bus=hc.0 $NET $(USBNET hc.0)
  orden "HSPRED"
  orden "ECHO fin-de-la-prueba"
  fin c_sinecm
}
c_sinecm() {
  check "Adaptador de red USB 0525:A4A2: aún no se admite (no ofrece la clase estándar CDC-ECM)."
  check "FS 0525:A4A2 02/02/FF 0A/00/00 -> adaptador de red que aún no se admite (sin CDC-ECM)"
  checkn "red (CDC-ECM)"
  check "Teclado USB preparado."
  check "No se ha encontrado ninguna tarjeta de red que HSPRED sepa usar."
  check "(Un adaptador USB-Ethernet lo lleva HSPUSB.SYS, en CONFIG.SYS: tiene que"
  check "estar cargado con /RED y con el adaptador conectado al arrancar.)"
  check "fin-de-la-prueba"
}

# CONSOLA (la consola remota: un servidor TCP) por el adaptador USB, con el
# cliente de tests/run_consola_tests.sh
t_consola() {
  caso consola "CONSOLA (servidor TCP) por el adaptador USB, xHCI" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED\r\n'
  CONSOLA_QEMU="-device qemu-xhci,id=hc" python3 tests/consola_cliente.py $T/f.img "usb-net,bus=hc.0" 12399 > $T/consola.out 2>&1
  local rc=$?
  cat $T/consola.out
  cp $T/consola.out $W/consola.txt
  if [ $rc -eq 0 ] && ! grep -q "FAIL" $T/consola.out && grep -q "  OK   " $T/consola.out; then return 0; fi
  return 1
}

# con una tarjeta PCI además: HSPRED elige la PCI; HSPRED /USB, el adaptador
t_pciusb() {
  caso pciusb "tarjeta PCI y adaptador USB a la vez (HSPRED /USB)" "$SOLOUSB"
  arranca -device qemu-xhci,id=hc $NET $(USBNET hc.0) -netdev user,id=n1 -device rtl8139,netdev=n1,mac=52:54:00:AA:BB:CC
  orden "HSPRED"
  orden "HSPRED /U"
  orden "HSPRED /USB"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:2"
  orden "HSPRED /?"
  orden "ECHO fin-de-la-prueba"
  fin c_pciusb
}
c_pciusb() {
  check "$ECM"
  check "Tarjeta Realtek RTL8139, MAC 52:54:00:AA:BB:CC, IRQ"
  check "HSPRED descargado."
  check "Tarjeta Adaptador USB (CDC-ECM), MAC $MAC, por HSPUSB, packet driver en INT 60h"
  check "Dirección MAC . . . . : $MAC"
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  check "  /USB     el adaptador USB aunque haya una tarjeta PCI que valga"
  check "fin-de-la-prueba"
}

# /2: en un xHCI de Intel de las series 7 a 9 (simulado con -DINTEL_FAKE: QEMU
# no tiene esos registros) no se enciende el USB 3, y la red sigue yendo
t_sinusb3() {
  caso sinusb3 "/2 (sin USB 3) en un xHCI de Intel simulado (-DINTEL_FAKE)" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED /INFO /2\r\n' "-DINTEL_FAKE"
  arranca -device usb-ehci,id=he -device qemu-xhci,id=hc $NET $(USBNET hc.0)
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:2"
  orden "ECHO fin-de-la-prueba"
  fin c_sinusb3
}
c_sinusb3() {
  check "Intel: puertos pasados del EHCI al xHCI: USB 2 XUSB2PR=0000000F (de 0000000F),"
  check "  USB 3 USB3_PSSEN=00000000 (de 0000000F)"
  check "$ECM"
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  check "fin-de-la-prueba"
}

# lo que ocupa HSPUSB (MEM /C) con el adaptador y sin él: con adaptador se
# queda su bloque (código, búferes y anillos); sin él, nada de eso
t_memoria() {
  local con sin mal=0
  caso memoria "memoria de HSPUSB con el adaptador (xHCI, teclado)" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED\r\n'
  arranca -device qemu-xhci,id=hc -device usb-kbd,bus=hc.0 $NET $(USBNET hc.0)
  orden "MEM /C"
  orden "ECHO fin-de-la-prueba"
  fin check "Adaptador de red USB preparado (CDC-ECM)" || mal=1
  con=$(grep -m1 -E "^ *Libre " $T/log.txt | awk '{print $2}' | tr -d .)
  caso memoria2 "memoria de HSPUSB sin el adaptador (xHCI, teclado)" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED\r\n'
  arranca -device qemu-xhci,id=hc -device usb-kbd,bus=hc.0
  orden "MEM /C"
  orden "ECHO fin-de-la-prueba"
  fin checkn "Adaptador de red USB" || mal=1
  sin=$(grep -m1 -E "^ *Libre " $T/log.txt | awk '{print $2}' | tr -d .)
  echo "  memoria convencional libre con el adaptador: ${con:-?} octetos; sin él: ${sin:-?} octetos"
  if [ -n "$con" ] && [ -n "$sin" ] && [ "$sin" -gt "$con" ]; then
    ok "sin adaptador, HSPUSB no se queda con el bloque de la red ($((sin - con)) octetos menos)"
  else
    ko "tamaños de HSPUSB con y sin adaptador"; mal=1
  fi
  return $mal
}

# ---- [USB-TARDE] [USB-DOS] el teclado no se pierde por el adaptador ----
# El monitor de QEMU (además de la serie): sendkey va al teclado USB.
MON="-chardev socket,id=mon,path=@T@/mon,server=on,wait=off -mon chardev=mon,mode=readline"
mon() { printf '%s\n' "$1" | socat - UNIX-CONNECT:$T/mon 2>/dev/null | tr -d '\r'; }
teclea() {  # una orden, por el teclado USB; espera a que vuelva el indicador
  local n=$(veces 'A:\>') k
  python3 -c "
import sys
m = {' ': 'spc', '.': 'dot', '\\\\': 'backslash', '-': 'minus'}
for ch in sys.argv[1]: print(m.get(ch, ch))
print('ret')" "$1" | while read -r k; do mon "sendkey $k" > /dev/null; sleep 0.15; done
  espera 'A:\>' $((n + 1)) ${2:-100}
}
llega() { if grep -qx -- "$1" $T/log.txt; then ok "tecleado por el teclado USB: $2"; else ko "no llega lo tecleado por el teclado USB: $2"; fi; }
# lo del Mac: el xHCI lleno de cosas y el teclado en el EHCI
MACXHCI="-device usb-ehci,id=he -device qemu-xhci,id=hc
    -device usb-hub,bus=hc.0,port=1,id=hub1 -device usb-hub,bus=hc.0,port=1.1,id=hub2
    -device usb-net,netdev=n0,id=red,bus=hc.0,port=1.1.1"
usbimg() {
  dd if=/dev/zero of=$T/usb.img bs=1M count=32 status=none
  mformat -i $T/usb.img -T 65536 -h 64 -s 32 -v USBDATA ::
}
t_tecladoehci() {
  caso tecladoehci "teclado en un EHCI con el xHCI lleno (memoria, 2 concentradores, ratón y adaptador)" "$SOLOUSB"
  usbimg
  arranca $MON $MACXHCI $NET -device usb-mouse,bus=hc.0,port=1.2 $(STICK hc.0),port=2 \
      -device usb-kbd,bus=he.0,id=k1
  teclea "echo antesdelared"
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:3"
  teclea "echo conlared"
  orden "DESCARGAR $U/PRUEBA.BIN" 900
  teclea "echo trasdescargar"
  orden "ECHO fin-de-la-prueba"
  fin c_tecladoehci
}
c_tecladoehci() {
  check "      5.1.1 $ECM"
  check "  1 HS 0627:0001 03/01/01 -> teclado"
  check "USB: teclado por EHCI 8086:24CD, ratón por xHCI 1B36:000D."
  check "Disco USB: 32 MB en la unidad D:"
  check "Adaptador de red USB preparado (CDC-ECM), MAC $MAC: cargue HSPRED."
  llega antesdelared "antes de cargar HSPRED"
  check "Dirección IP  . . . . : 10.0.2.15"
  check "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  llega conlared "con la red en marcha"
  check "Guardado en PRUEBA.BIN (150.000 octetos)."
  llega trasdescargar "después de descargar"
  checkn "no queda memoria"
  check "fin-de-la-prueba"
}
# dos teclados y dos ratones; $1 = el teclado que se quita a mitad
dosteclados() {
  local quitar=$1 i a b
  arranca $MON $MACXHCI $NET -device usb-mouse,bus=hc.0,port=1.2 \
      -device usb-kbd,bus=hc.0,port=1.3,id=k1 -device usb-kbd,bus=he.0,id=k2 \
      -device usb-tablet,bus=he.0,id=t1
  teclea "echo unteclado$quitar"
  mon "device_del $quitar" > /dev/null
  sleep 2
  teclea "echo elotro$quitar"
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:2"
  teclea "echo conlared$quitar"
  orden "ECHO fin-de-la-prueba"
}
c_dosteclados() {
  check "    5.3 FS 0627:0001 03/01/01 -> teclado"
  check "  1 HS 0627:0001 03/01/01 -> teclado"
  check "Teclado USB preparado."
  check "Otro teclado USB preparado (van los dos)."
  llega "unteclado$1" "con los dos teclados puestos"
  llega "elotro$1" "por el otro teclado, quitado $1"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  llega "conlared$1" "por el otro teclado, con la red en marcha"
  check "fin-de-la-prueba"
}
t_dosteclados() {
  local mal=0
  caso dosteclados "dos teclados (xHCI y EHCI) con el adaptador; se quita el del xHCI" "$SOLOUSB"
  dosteclados k1
  fin c_dosteclados k1 || mal=1
  caso dosteclados2 "dos teclados (xHCI y EHCI) con el adaptador; se quita el del EHCI" "$SOLOUSB"
  dosteclados k2
  fin c_dosteclados k2 || mal=1
  return $mal
}
# dos ratones (uno detrás del concentrador del xHCI y otro en una UHCI): se
# mueve uno, luego el otro (mouse_set del monitor elige cuál) y MOUSETST ve
# los dos movimientos
t_dosratones() {
  local m1 m2
  caso dosratones "dos ratones (xHCI y UHCI) con el adaptador: van los dos" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /RED /INFO\r\n'
  arranca $MON -device qemu-xhci,id=hc -device piix3-usb-uhci,id=hu \
      -device usb-hub,bus=hc.0,port=1,id=hub1 $NET -device usb-net,netdev=n0,id=red,bus=hc.0,port=1.1 \
      -device usb-mouse,bus=hc.0,port=1.2,id=m1 -device usb-kbd,bus=hc.0,port=1.3 \
      -device usb-mouse,bus=hu.0,id=m2
  orden 'A:\HISPADOS\HSPRATON'
  printf 'BIN\\MOUSETST\r' >&3
  sleep 3
  mon "info mice" > $T/mice.txt
  m1=$(grep -o "Mouse #[0-9]*: QEMU HID" $T/mice.txt | sed -n 1p | tr -dc 0-9)
  m2=$(grep -o "Mouse #[0-9]*: QEMU HID" $T/mice.txt | sed -n 2p | tr -dc 0-9)
  echo "  ratones de QEMU: #$m1 y #$m2"
  mon "mouse_set $m1" > /dev/null; sleep 1; mon "mouse_move 40 0" > /dev/null
  sleep 1; mon "mouse_set $m2" > /dev/null; sleep 1; mon "mouse_move 0 24" > /dev/null
  sleep 1; mon "mouse_set $m1" > /dev/null; sleep 1; mon "mouse_move 0 24" > /dev/null
  sleep 1; mon "mouse_set $m2" > /dev/null; sleep 1; mon "mouse_move 40 0" > /dev/null
  sleep 1; mon "mouse_button 1" > /dev/null; sleep 1; mon "mouse_button 0" > /dev/null
  sleep 1; mon "sendkey ret" > /dev/null
  espera 'A:\>' 3 100
  orden "ECHO fin-de-la-prueba"
  fin c_dosratones "$m1" "$m2"
}
c_dosratones() {
  if [ -n "$1" ] && [ -n "$2" ] && [ "$1" != "$2" ]; then ok "QEMU ve los dos ratones activos (los sondea HSPUSB)"; else ko "QEMU no ve dos ratones activos"; fi
  check "Ratón USB preparado."
  check "Otro ratón USB preparado (van los dos)."
  check "Adaptador de red USB preparado (CDC-ECM)"
  check "X=320 Y=100 B=0"
  # cada ratón mueve 40 en X y 24 en Y (12 en la pantalla): con los dos, el doble que con uno
  local x y
  x=$(grep -oE "^X=[0-9]+ Y=[0-9]+ B=" $T/log.txt | sed 's/^X=\([0-9]*\).*/\1/' | sort -n | tail -1)
  y=$(grep -oE "^X=[0-9]+ Y=[0-9]+ B=" $T/log.txt | sed 's/.* Y=\([0-9]*\).*/\1/' | sort -n | tail -1)
  x1=$(grep -oE "^X=[0-9]+ Y=100 B=0" $T/log.txt | sed 's/^X=\([0-9]*\).*/\1/' | sort -n | tail -1)
  echo "  X llega a ${x:-?} (con el primer ratón solo, a ${x1:-?}); Y llega a ${y:-?}"
  if [ -n "$x" ] && [ -n "$x1" ] && [ "$x1" -gt 320 ] && [ "$x" -gt "$x1" ]; then ok "los dos ratones mueven en X"; else ko "los dos ratones mueven en X"; fi
  if [ -n "$y" ] && [ "$y" -gt 112 ]; then ok "los dos ratones mueven en Y"; else ko "los dos ratones mueven en Y"; fi
  check "Pulsaciones del botón izquierdo: 1"
  check "fin-de-la-prueba"
}
# Más de 8 aparatos en uso (lo que llenaba la reserva en el MacBook Air: con
# el adaptador y sus dos concentradores ya no cabía el lector de tarjetas):
# 3 en el xHCI (dos concentradores y el adaptador) y, en un EHCI con UHCI
# compañeras, cinco concentradores, el teclado y un ratón
t_muchos() {
  caso muchos "más de 8 aparatos en uso (3 en el xHCI y 7 en las UHCI compañeras de un EHCI)" "$SOLOUSB"
  arranca $MON -device qemu-xhci,id=hc \
      -device ich9-usb-ehci1,id=he,addr=1d.7,multifunction=on \
      -device ich9-usb-uhci1,masterbus=he.0,firstport=0,addr=1d.0,multifunction=on \
      -device ich9-usb-uhci2,masterbus=he.0,firstport=2,addr=1d.1,multifunction=on \
      -device ich9-usb-uhci3,masterbus=he.0,firstport=4,addr=1d.2,multifunction=on \
      -device usb-hub,bus=hc.0,port=1,id=hub1 -device usb-hub,bus=hc.0,port=1.1,id=hub2 \
      $NET -device usb-net,netdev=n0,id=red,bus=hc.0,port=1.1.1 \
      -device usb-hub,bus=he.0,port=1,id=c1 -device usb-hub,bus=he.0,port=1.1,id=c2 \
      -device usb-hub,bus=he.0,port=1.2,id=c3 -device usb-kbd,bus=he.0,port=1.1.1,id=k1 \
      -device usb-hub,bus=he.0,port=1.3,id=c4 -device usb-hub,bus=he.0,port=1.4,id=c5 \
      -device usb-mouse,bus=he.0,port=1.2.1
  teclea "echo diezaparatos"
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:2"
  teclea "echo conlared"
  orden "ECHO fin-de-la-prueba"
  fin c_muchos
}
c_muchos() {
  check "      5.1.1 $ECM"
  check "      1.1.1 FS 0627:0001 03/01/01 -> teclado"
  check "      1.2.1 FS 0627:0001 03/01/02 -> ratón"
  check "Aparatos en uso: 10."
  checkn "demasiados dispositivos"
  check "Ratón USB preparado."
  check "Adaptador de red USB preparado (CDC-ECM), MAC $MAC: cargue HSPRED."
  llega diezaparatos "con 10 aparatos en uso"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  llega conlared "con la red en marcha"
  check "fin-de-la-prueba"
}

# Sin /RED la red por USB está apagada: HSPUSB no toca el adaptador (ni lee
# sus demás configuraciones ni se queda con nada suyo), dice en una línea
# cómo encenderla, y ocupa lo mismo que sin adaptador
t_sinred() {
  local con sin mal=0
  caso sinred "sin /RED: el adaptador no se toca (y se dice cómo usarlo)" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /INFO\r\n'
  arranca $MON $MACXHCI $NET -device usb-kbd,bus=he.0,id=k1
  teclea "echo sinlared"
  orden "HSPRED"
  orden "MEM /C"
  orden "ECHO fin-de-la-prueba"
  fin c_sinred || mal=1
  con=$(grep -m1 -E "^ *Libre " $T/log.txt | awk '{print $2}' | tr -d .)
  caso sinred2 "sin /RED y sin adaptador: la misma memoria" 'DEVICE=A:\\HISPADOS\\HSPUSB.SYS /F /INFO\r\n'
  arranca -device usb-ehci,id=he -device qemu-xhci,id=hc \
      -device usb-hub,bus=hc.0,port=1,id=hub1 -device usb-hub,bus=hc.0,port=1.1,id=hub2 \
      -device usb-kbd,bus=he.0,id=k1
  orden "MEM /C"
  orden "ECHO fin-de-la-prueba"
  fin check "Teclado USB preparado." || mal=1
  sin=$(grep -m1 -E "^ *Libre " $T/log.txt | awk '{print $2}' | tr -d .)
  echo "  memoria convencional libre sin /RED: con el adaptador puesto, ${con:-?} octetos; sin él, ${sin:-?}"
  if [ -n "$con" ] && [ "$con" = "$sin" ]; then ok "sin /RED, el adaptador no le cuesta memoria a HSPUSB"; else ko "sin /RED, el adaptador cambia la memoria de HSPUSB"; mal=1; fi
  return $mal
}
c_sinred() {
  check "      5.1.1 FS 0525:A4A2 02/02/FF 0A/00/00 -> sin uso"
  check "Adaptador de red USB 0525:A4A2: sin usar. Para usarlo, añada /RED a la línea de"
  checkn "red (CDC-ECM)"
  checkn "Adaptador de red USB preparado"
  check "Teclado USB preparado."
  llega sinlared "sin /RED"
  check "No se ha encontrado ninguna tarjeta de red que HSPRED sepa usar."
  check "fin-de-la-prueba"
}

# sin sitio para el adaptador: no se usa, y el teclado no lo nota
t_sinsitio() {
  caso sinsitio "sin memoria para el adaptador (HSPUSB con -DRED_SIN_SITIO): la red cede" "$SOLOUSB" "-DRED_SIN_SITIO"
  arranca $MON $MACXHCI $NET -device usb-kbd,bus=he.0,id=k1
  teclea "echo sigueelteclado"
  orden "HSPRED"
  orden "ECHO fin-de-la-prueba"
  fin c_sinsitio
}
c_sinsitio() {
  check "      5.1.1 $ECM"
  check "Adaptador de red USB: no queda memoria para él (hay muchos aparatos USB"
  checkn "Adaptador de red USB preparado"
  check "Teclado USB preparado."
  llega sigueelteclado "sin la red"
  check "No se ha encontrado ninguna tarjeta de red que HSPRED sepa usar."
  check "fin-de-la-prueba"
}
# sin sitio para la lista periódica del EHCI: se dice (y lo del xHCI sigue)
t_sinlista() {
  caso sinlista "sin memoria para la lista periódica del EHCI (HSPUSB con -DFL_FIN_FALLA)" "$SOLOUSB" "-DFL_FIN_FALLA"
  arranca -device usb-ehci,id=he -device qemu-xhci,id=hc -device usb-kbd,bus=he.0 $NET $(USBNET hc.0)
  orden "HSPRED"
  orden "CONFRED"
  orden "PING 10.0.2.2 /N:2"
  orden "ECHO fin-de-la-prueba"
  fin c_sinlista
}
c_sinlista() {
  check "USB: no queda memoria para la lista periódica de un EHCI: su teclado, ratón,"
  check "Adaptador de red USB preparado (CDC-ECM)"
  check "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  check "fin-de-la-prueba"
}

# ---- todos, de dos en dos ----
TODOS="xhci xhcihub uhci uhcihub ohci ohcihub ehci todo v86 v86uhci sinecm sinusb3 pciusb consola memoria tecladoehci dosteclados dosratones muchos sinred sinsitio sinlista"
CASOS=${CASOS:-$TODOS}
JOBS=${JOBS:-2}
fail=0
set -- $CASOS
while [ $# -gt 0 ]; do
  PIDS=()
  TANDA=()
  for c in ${1:-} $([ "$JOBS" -ge 2 ] && echo ${2:-}); do
    ( t_$c ) > $W/$c.out 2>&1 &
    PIDS+=($!)
    TANDA+=($c)
  done
  i=0
  for p in "${PIDS[@]}"; do
    wait $p || fail=1
    cat $W/${TANDA[$i]}.out
    i=$((i + 1))
  done
  shift; [ "$JOBS" -ge 2 ] && [ $# -gt 0 ] && shift
done
exit $fail
