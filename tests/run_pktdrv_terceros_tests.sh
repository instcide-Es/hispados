#!/bin/bash
# Prueba de los «packet drivers» de otros con la pila de Hispa-DOS (docs/RED.md,
# docs/DRIVERS-MICROSOFT.md, hito 2): en vez de HSPRED, el driver de DOS de la
# tarjeta, como en un PC de los 90. Con los drivers libres de Crynwr (GPL), que
# NO van en el repositorio: se dan con CRYNWR=carpeta (la que tiene NE2000.COM y
# PCNTPK.COM, por ejemplo la del paquete «crynwr» de FreeDOS 1.3,
# DRIVERS/CRYNWR); sin ellos, la prueba se salta y lo dice.
#   1. NE2000.COM 0x60 3 0x300 con la NE2000 ISA de QEMU (ne2k_isa).
#   2. PCNTPK.COM INT=0x60 con la AMD PCnet de QEMU (pcnet, PCI).
#   3. y 4. Lo mismo con HSPXMS + HSPEMM (modo V86, DOS=HIGH,UMB) y el driver
#      en la memoria superior (LH).
# En cada una: el driver dice su MAC; CONFRED (DHCP de QEMU), PING a la
# puerta, y DESCARGAR de un servidor HTTP propio (150.000 octetos, comparados
# con mtools); fsck.fat del disquete.
# Si no se da CRYNWR=, se usa la que baja tests/thirdparty/fetch_drivers.sh.
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
C=${CRYNWR:-}
[ -z "$C" ] && [ -f tests/thirdparty/dl/crynwr/.ok ] && C=tests/thirdparty/dl/crynwr/DRIVERS/CRYNWR
[ -z "${RED_NS:-}" ] && echo "== Hispa-DOS: packet drivers de otros (Crynwr) =="
if [ -z "$C" ] || [ ! -f "$C/NE2000.COM" ] || [ ! -f "$C/PCNTPK.COM" ]; then
  echo "  (sin CRYNWR=carpeta con NE2000.COM y PCNTPK.COM: se salta)"; exit 0
fi
command -v qemu-system-i386 > /dev/null || { echo "  (sin qemu-system-i386: se salta)"; exit 0; }
if [ -z "${RED_NS:-}" ] && unshare -rn true 2>/dev/null; then
  RED_NS=1 exec unshare -rn "$0" "$@"
fi
[ -n "${RED_NS:-}" ] && ip link set lo up
T=$(mktemp -d)
PORT=8080
mkdir -p $T/www
python3 -c "import random; random.seed(7); open('$T/www/PRUEBA.BIN','wb').write(bytes(random.getrandbits(8) for _ in range(150000)))"
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
trap 'kill $HTTP_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
sleep 1
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\nFILES=30\r\n' > $T/CONFIG.SYS
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$1: $2"; else ko "$1: $2"; fi; }
# corre NOMBRE «opciones de QEMU» «orden del driver» [v86]
corre() {
  local n=$1 dev=$2 drv=$3 v86=${4:-} lh=""
  cp "$IMG" $T/$n.img
  mdel -i $T/$n.img ::HISPADOS/HISPA.WAV 2>/dev/null   # sitio para lo que se descarga
  mcopy -o -i $T/$n.img "$C/NE2000.COM" "$C/PCNTPK.COM" ::
  if [ -n "$v86" ]; then mcopy -o -i $T/$n.img $T/CONFIG.SYS ::CONFIG.SYS; lh="LH "; fi
  { sleep 7
    for c in "4|$lh$drv" '3|MEM /C' '5|CONFRED' '7|PING 10.0.2.2 /N:3' \
             "14|DESCARGAR http://10.0.2.2:$PORT/PRUEBA.BIN" '2|ECHO fin-de-la-prueba'; do
      printf '%s\r' "${c#*|}"; sleep "${c%%|*}"
    done
  } | timeout 150 qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -drive file=$T/$n.img,format=raw,if=floppy -boot a -no-reboot \
        -netdev user,id=n0 -device $dev,netdev=n0 > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}
mira() {   # NOMBRE «lo que dice el driver» «lo que dice CONFRED de la tarjeta»
  local n=$1
  echo "-- $n --"
  check $n "$2"
  check $n "My Ethernet address is 52:54:00:12:34:56"
  check $n "Tarjeta . . . . . . . : $3 (INT 60h)"
  check $n "Dirección IP  . . . . : 10.0.2.15"
  check $n "Obtenida  . . . . . . : por DHCP (servidor 10.0.2.2"
  check $n "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  check $n "Guardado en PRUEBA.BIN (150.000 octetos)."
  rm -f $T/$n.bin
  mcopy -i $T/$n.img ::PRUEBA.BIN $T/$n.bin 2>/dev/null
  if cmp -s $T/$n.bin $T/www/PRUEBA.BIN; then ok "$n: PRUEBA.BIN igual que en el servidor (mtools)"; else ko "$n: PRUEBA.BIN distinto o no está"; fi
  check $n "fin-de-la-prueba"
  if fsck.fat -n $T/$n.img > /dev/null 2>&1; then ok "$n: fsck.fat del disquete"; else ko "$n: fsck.fat del disquete"; fi
}
corre ne2000 "ne2k_isa,iobase=0x300,irq=3" "NE2000 0x60 3 0x300" & P1=$!
corre pcntpk "pcnet" "PCNTPK INT=0x60" & P2=$!
wait $P1 $P2     # (solo esas dos: el servidor HTTP también es un hijo y no acaba)
corre v_ne2000 "ne2k_isa,iobase=0x300,irq=3" "NE2000 0x60 3 0x300" v86 & P1=$!
corre v_pcntpk "pcnet" "PCNTPK INT=0x60" v86 & P2=$!
wait $P1 $P2
mira ne2000 "Packet driver for NE2000" "NE2000"
mira pcntpk "Packet driver for an PCNTPK" "NE2100"
for n in v_ne2000 v_pcntpk; do check $n "HSPEMM 0.1"; done
mira v_ne2000 "Packet driver for NE2000" "NE2000"
mira v_pcntpk "Packet driver for an PCNTPK" "NE2100"
if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then for n in ne2000 pcntpk v_ne2000 v_pcntpk; do echo "---- $n"; grep -v '░' $T/$n.txt | tail -40; done; fi
[ $fail -eq 0 ] && echo "RESULTADO: BIEN" || echo "RESULTADO: FALLO"
exit $fail
