#!/bin/bash
# Prueba de la red (docs/RED.md): HSPRED con las tres tarjetas de QEMU
# (NE2000 PCI, RTL8139 y e1000) en la red de usuario de QEMU, y CONFRED,
# PING y DESCARGAR:
#  1. HSPRED encuentra la tarjeta y da la MAC de QEMU (52:54:00:12:34:56).
#  2. CONFRED: DHCP da 10.0.2.15, puerta 10.0.2.2 y DNS 10.0.2.3.
#  3. PING 10.0.2.2 contesta (la red de usuario contesta al eco de su puerta).
#  4. DESCARGAR de un servidor HTTP propio (python3 -m http.server en
#     127.0.0.1, que el invitado ve en 10.0.2.2:puerto): un archivo de
#     150.000 octetos llega entero (se compara con mtools al acabar), un 404
#     se dice y no deja archivo, https:// se rechaza con su mensaje, y una
#     redirección (301 de /sub a /sub/) se sigue.
#  5. DNS con un servidor DNS falso propio (tests/red_dns.py, en el puerto 53
#     de 127.0.0.1 = 10.0.2.2:53 para el invitado; DNS=10.0.2.2 en RED.INI):
#     PING y DESCARGAR por nombre, y un nombre que no existe.
#  6. RED.INI con DHCP=NO: la dirección a mano (10.0.2.20).
#  7. HSPRED /U lo descarga.
#  8. Con HSPEMM (modo V86, memoria superior): LH HSPRED con la NE2000 y con
#     la RTL8139 (búferes de DMA en la memoria convencional, dirección física
#     por VCPI), y la e1000 sin registros por puertos lo dice y no se carga.
# Todo va en un espacio de nombres de red propio (unshare -rn): no hace falta
# Internet ni se toca la red del equipo. Como mucho 3 QEMU a la vez.
# Network test (QEMU user networking, own HTTP and DNS servers).
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HSPRED CONFRED PING DESCARGAR; do
  [ -f $B/$f.COM ] || { echo "falta $B/$f.COM (make)"; exit 1; }
done
if [ -z "${RED_NS:-}" ] && unshare -rn true 2>/dev/null; then
  RED_NS=1 exec unshare -rn "$0" "$@"
fi
if [ -n "${RED_NS:-}" ]; then
  ip link set lo up
fi
T=$(mktemp -d)
PORT=8080
mkdir -p $T/www/sub
python3 -c "import random; random.seed(7); open('$T/www/PRUEBA.BIN','wb').write(bytes(random.getrandbits(8) for _ in range(150000)))"
printf '<html><body>Hispa-DOS: redireccion seguida</body></html>\n' > $T/www/sub/index.html
python3 -m http.server --bind 127.0.0.1 --directory $T/www $PORT > $T/http.log 2>&1 &
HTTP_PID=$!
DNS=1
python3 tests/red_dns.py 127.0.0.1 53 > $T/dns.log 2>&1 &
DNS_PID=$!
sleep 1
if ! kill -0 $DNS_PID 2>/dev/null; then
  DNS=0
  echo "  (sin servidor DNS propio: no se puede usar el puerto 53; se saltan las pruebas de DNS)"
fi
# (GUARDAR=carpeta: deja allí los registros de QEMU)
trap 'kill $HTTP_PID $DNS_PID 2>/dev/null; [ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
# configuraciones de RED.INI para la prueba
printf 'DHCP=SI\r\nDNS=10.0.2.2\r\n' > $T/RED2.INI
printf '; a mano\r\nDHCP=NO\r\nIP=10.0.2.20\r\nMASCARA=255.255.255.0\r\nPUERTA=10.0.2.2\r\nDNS=10.0.2.2\r\n' > $T/RED3.INI
mkimg() {   # mkimg destino
  cp "$IMG" $1
  mcopy -o -i $1 $T/RED2.INI ::RED2.INI
  mcopy -o -i $1 $T/RED3.INI ::RED3.INI
}
# corre NOMBRE TARJETA TIEMPO «s|orden»...
corre() {
  local n=$1 nic=$2 tmo=$3; shift 3
  { sleep 6
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
        -m 32 -drive file=$T/$n.img,format=raw,if=floppy -boot a -no-reboot \
        -netdev user,id=n0 -device $nic,netdev=n0${NICOPT:-} > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" $T/$1.txt; then ko "(no debía salir) $2"; else ok "sin «$2»"; fi; }
U="http://10.0.2.2:$PORT"
CMDS=('3|HSPRED' '2|HSPRED /INFO' '4|CONFRED' '7|PING 10.0.2.2 /N:3'
      "12|DESCARGAR $U/PRUEBA.BIN"
      "4|DESCARGAR $U/no-existe.txt" '3|DESCARGAR https://ejemplo.org/x.zip'
      '2|DEL A:\HISPADOS\RED.INI' '2|COPY A:\RED2.INI A:\HISPADOS\RED.INI'
      '4|CONFRED /RENOVAR' '5|PING prueba.hispados /N:2'
      "6|DESCARGAR http://servidor.casa:$PORT/sub OTRO.HTM"
      '5|PING no.existe'
      '2|DEL A:\HISPADOS\RED.INI' '2|COPY A:\RED3.INI A:\HISPADOS\RED.INI'
      '3|CONFRED /RENOVAR' '5|PING 10.0.2.2 /N:2'
      '2|HSPRED /INFO' '2|HSPRED /U' '2|ECHO fin-de-la-prueba')
PIDS=()
# (SOLO=v86: solo la segunda parte, la de HSPEMM)
RONDA1="ne2k_pci rtl8139 e1000"
[ "${SOLO:-}" = v86 ] && RONDA1=""
for nic in $RONDA1; do
  mkimg $T/$nic.img
  corre $nic $nic 150 "${CMDS[@]}" &
  PIDS+=($!)
done
[ -n "$RONDA1" ] && wait "${PIDS[@]}"
for nic in $RONDA1; do
  case $nic in
    ne2k_pci) nombre="NE2000 PCI";;
    rtl8139) nombre="Realtek RTL8139";;
    e1000) nombre="Intel PRO/1000 (e1000)";;
  esac
  echo "== Hispa-DOS red: $nic =="
  check $nic "Tarjeta $nombre, MAC 52:54:00:12:34:56, IRQ"
  check $nic "Dirección MAC:   52:54:00:12:34:56"
  check $nic "Dirección IP  . . . . : 10.0.2.15"
  check $nic "Máscara de subred . . : 255.255.255.0"
  check $nic "Puerta de enlace  . . : 10.0.2.2"
  check $nic "Servidor DNS  . . . . : 10.0.2.3"
  check $nic "Obtenida  . . . . . . : por DHCP (servidor 10.0.2.2"
  n=$(grep -c "Respuesta desde 10.0.2.2: octetos=32" $T/$nic.txt)
  if [ "$n" -ge 5 ]; then ok "PING 10.0.2.2: $n respuestas (3 + 2 a mano)"; else ko "PING 10.0.2.2: $n respuestas"; fi
  check $nic "Paquetes: enviados = 3, recibidos = 3, perdidos = 0"
  check $nic "Guardado en PRUEBA.BIN (150.000 octetos)."
  check $nic "[█████████████████████████] 100%"
  check $nic "El servidor contesta: HTTP/1.0 404 File not found"
  check $nic "Las direcciones https:// (cifradas) todavía no funcionan"
  rm -f $T/$nic.bin $T/$nic.htm
  mcopy -i $T/$nic.img ::PRUEBA.BIN $T/$nic.bin 2>/dev/null
  if cmp -s $T/$nic.bin $T/www/PRUEBA.BIN; then ok "PRUEBA.BIN igual que en el servidor (mtools)"; else ko "PRUEBA.BIN distinto o no está"; fi
  if mdir -i $T/$nic.img ::NO-EXIS~1.TXT >/dev/null 2>&1 || mdir -i $T/$nic.img ::NO-EXIST.TXT >/dev/null 2>&1; then ko "el 404 dejó un archivo"; else ok "el 404 no deja archivo"; fi
  if [ $DNS = 1 ]; then
    check $nic "Servidor DNS  . . . . : 10.0.2.2"
    check $nic "Haciendo ping a prueba.hispados [10.0.2.2] con 32 octetos de datos:"
    check $nic "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
    check $nic "Servidor servidor.casa: 10.0.2.2:$PORT"
    check $nic "El servidor manda a otra dirección: http://servidor.casa:$PORT/sub/"
    check $nic "Guardado en OTRO.HTM ("
    mcopy -i $T/$nic.img ::OTRO.HTM $T/$nic.htm 2>/dev/null
    if grep -q "redireccion seguida" $T/$nic.htm 2>/dev/null; then ok "OTRO.HTM con la página de /sub/"; else ko "OTRO.HTM"; fi
    check $nic "No existe ese nombre"
  fi
  check $nic "Dirección IP  . . . . : 10.0.2.20"
  check $nic "Obtenida  . . . . . . : a mano (RED.INI)"
  check $nic "Dirección IP:    10.0.2.20 (a mano, RED.INI)"
  check $nic "HSPRED descargado."
  check $nic "fin-de-la-prueba"
  if fsck.fat -n $T/$nic.img >/dev/null 2>&1; then ok "fsck.fat del disquete"; else ko "fsck.fat del disquete"; fi
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>HSPRED$/,$p' $T/$nic.txt | head -150; fi
done

PIDS=()
# ---- con HSPEMM: LH y búferes de DMA en la memoria convencional ----
# (sin la ROM de arranque por red de la tarjeta, que ocupa la memoria
# superior: como en un portátil con el arranque por red apagado)
NICOPT=",romfile="
printf 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH,UMB\r\nFILES=30\r\n' > $T/CONFIG.SYS
CMDS2=('3|LH HSPRED' '3|MEM /C' '2|HSPRED /INFO' '4|CONFRED' '4|PING 10.0.2.2 /N:2'
       "12|DESCARGAR $U/PRUEBA.BIN" '2|HSPRED /U' '2|ECHO fin-de-la-prueba')
for nic in ne2k_pci rtl8139 e1000; do
  mkimg $T/v$nic.img
  mcopy -o -i $T/v$nic.img $T/CONFIG.SYS ::CONFIG.SYS
  corre v$nic $nic 90 "${CMDS2[@]}" &
  PIDS+=($!)
done
wait "${PIDS[@]}"
for nic in ne2k_pci rtl8139; do
  echo "== Hispa-DOS red con HSPEMM (LH): $nic =="
  check v$nic "HSPEMM 0.1"
  check v$nic "Dirección MAC:   52:54:00:12:34:56"
  if sed -n '/Programas en la memoria superior/,/^  Total/p' $T/v$nic.txt | grep -q HSPRED; then ok "HSPRED en la memoria superior (MEM /C)"; else ko "HSPRED en la memoria superior (MEM /C)"; fi
  check v$nic "Dirección IP  . . . . : 10.0.2.15"
  check v$nic "Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
  check v$nic "Guardado en PRUEBA.BIN (150.000 octetos)."
  rm -f $T/v$nic.bin
  mcopy -i $T/v$nic.img ::PRUEBA.BIN $T/v$nic.bin 2>/dev/null
  if cmp -s $T/v$nic.bin $T/www/PRUEBA.BIN; then ok "PRUEBA.BIN igual que en el servidor"; else ko "PRUEBA.BIN distinto o no está"; fi
  check v$nic "HSPRED descargado."
  if [ $nic = rtl8139 ]; then check v$nic "residentes, y búferes en la convencional: 9744 octetos"; fi
  if [ $fail -ne 0 ] || [ -n "${VERBOSE:-}" ]; then sed -n '/>LH HSPRED/,$p' $T/v$nic.txt | head -120; fi
done
echo "== Hispa-DOS red con HSPEMM: e1000 (solo registros en memoria) =="
check ve1000 "en modo V86 (con"
check ve1000 "No hay ningún controlador de red cargado"
exit $fail
