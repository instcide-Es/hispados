# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_redusb.sh - la parte 59 (SOLO=redusb) de pruebas.sh, que lee este
# archivo («.»): (RTL8153) los adaptadores de red por USB de cable, CDC-ECM
# (usb.cas elige la configuración; usbmovil.cas y rmovil.cas, lo demás), con
# los ayudantes de pruebas.sh (ok, ko, mira, saca) y sus carpetas ($AQUI, $P, $G).
#
# QEMU no tiene el RTL8153; su usb-net da RNDIS en la primera configuración
# y CDC-ECM en la segunda (y es de velocidad plena). Así que:
#  1. el RTL8153 de mentira (tests/tools/usbrtl8153.py, por usb-redir; de
#     alta velocidad): su primera configuración es la de Realtek (FF/FF/00)
#     y la segunda, ECM; el núcleo elige la 2, RED.BIN lo pone en marcha (la
#     MAC de iMACAddress, la variante 1 de los datos, el filtro, los avisos:
#     el enlace a 1000 Mbit/s), DHCP, IPCONFIG («Realtek RTL8153 por USB
#     (CDC-ECM)»), PING y DESCARGAR (400 K, el SHA-256 del servidor): las
#     tramas pasan por el guion a la red de QEMU (-netdev socket + hubport);
#     y el teclado USB sigue bien. En el xHCI y en el EHCI (con sus UHCI
#     compañeras, para el teclado, que es de velocidad plena).
#  2. sin cable (el aviso NETWORK_CONNECTION a 0): «enlace: no», y con cable
#     otra vez, DHCP; con una tarjeta PCI con enlace, gana la PCI; y en
#     caliente: se enchufa en marcha (el servicio de red se pone solo) y se
#     quita (device_del): «ya no va por el adaptador».
#  3. «sinredusb»: la configuración de siempre (la 1), sin red, y lo dice.
#  4. el usb-net de QEMU con «usbecm» (la segunda configuración, ECM, en vez
#     de RNDIS): DHCP, PING y DESCARGAR por ECM, de velocidad plena.
#  5. sin «usbecm», el usb-net sigue siendo el móvil RNDIS (la parte 55 no cambia).
#  6. el escritorio con el adaptador y sin tarjeta PCI: el icono de la red
#     dice «La red: por el adaptador de red USB, con la IP 10.0.2.15.»

parte_redusb() {
echo "== (RTL8153) La red por USB de cable: CDC-ECM (el Realtek RTL8153 de mentira por usb-redir; el usb-net de QEMU) =="
local K=$P/redusb SPID U S hc dev que PQ PM SOCK RPID R
rm -rf $K; mkdir -p $K/srv $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $K/semilla.asm > /dev/null 2>&1 && nasm -f bin $K/semilla.asm -o $K/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $K/d.img 16 red > $K/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $K/disco.txt)"; return; }
printf 'Registro=SI\r\n' > $K/RED.INI
mcopy -o -i $K/d.img $K/RED.INI ::RED.INI
python3 -I $AQUI/nucleo32/servidor_prueba.py $K/srv > $K/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $K/srv/puerto ] && break; sleep 0.1; done
U=http://10.0.2.2:$(cat $K/srv/puerto 2> /dev/null)
R=$AQUI/../tests/tools/usbrtl8153.py
# el RTL8153 de mentira: $1, el nombre (para sus archivos); el resto, sus órdenes
rtl() {
  local n=$1; shift
  PQ=$((40000 + RANDOM % 9000)); PM=$((PQ + 1))
  SOCK=$K/$n.sock; rm -f $SOCK
  RED_RTL="-netdev user,id=n0 -netdev socket,id=s0,udp=127.0.0.1:$PM,localaddr=127.0.0.1:$PQ -netdev hubport,id=h0,hubid=1,netdev=n0 -netdev hubport,id=h1,hubid=1,netdev=s0 -chardev socket,id=rd,path=$SOCK,server=on,wait=off"
  USBRTL_VE=$K/$n.viva USBRTL_LOG=$K/$n.rtl python3 -I $R $SOCK $PQ $PM "$@" 2> $K/$n.err &
  RPID=$!
}
fin_rtl() { local i; for i in $(seq 50); do kill -0 $RPID 2> /dev/null || break; sleep 0.1; done; kill $RPID 2> /dev/null; wait $RPID 2> /dev/null; }
sin_fallos() {    # lo que ha visto el de mentira
  if grep -q "modelo: FALLO" $K/$1.rtl 2> /dev/null; then ko "$2: el RTL8153 de mentira ha visto cosas mal hechas: $(grep -m3 'modelo: FALLO' $K/$1.rtl | tr '\n' ' ')"
  elif [ ! -s $K/$1.rtl ]; then ko "$2: el RTL8153 de mentira no ha dejado su registro: $(tail -2 $K/$1.err)"
  else ok "$2: el RTL8153 de mentira no ha visto nada mal hecho ($(tail -1 $K/$1.rtl))"; fi
}
# 1. al arrancar, en el xHCI y en el EHCI (de alta velocidad: el RTL8153 va por él; el teclado, por la UHCI)
for hc in xhci ehci; do
  case $hc in
    xhci) dev="-device qemu-xhci,id=u"; que="xHCI" ;;
    ehci) dev="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2"; que="EHCI (el teclado, por su UHCI compañera)" ;;
  esac
  cp $K/d.img $K/$hc.img
  rtl $hc
  ESCRIBE=1 DISCO=$K/$hc.img ARRANQUE=ordenes MODULOS= MAQUINA=pc,i8042=off QEMU_MAS="$dev $RED_RTL -device usb-redir,chardev=rd,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
    ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 3" ipconfig ":pausa 2" "ping -n 3 10.0.2.2" ":pausa 3" "descargar $U/grande.bin" ":hasta 90 1 Hecho: GRANDE.BIN" "ipconfig" ":pausa 3" > $K/$hc.sal 2>&1
  fin_rtl
  tr -d '\r' < $K/$hc.sal > $K/$hc.txt
  S=$K/$hc.txt
  mira "$que: el núcleo elige la configuración 2 del RTL8153 (CDC-ECM; la 1 es la de Realtek) y lo aparta para la red" \
    "0BDA:8153, velocidad alta, .*; configuración 2 de 2; interfaz 02/06/00; interfaz 0A/00/00$" \
    "usb: el adaptador de red USB (CDC-ECM, configuración 2): aparato [0-9]*, 0BDA:8153; interfaces 0 y 1 (variante 1), avisos por 83, extremos 81 y 02$"
  mira "$que: RED.BIN lo pone en marcha: la MAC (iMACAddress), el enlace por sus avisos (1000 Mbit/s), «la red va por el adaptador de red USB» y DHCP" \
    "red: red USB: 0BDA:8153, CDC-ECM (adaptador de red por USB de cable), USB a 480 Mbit/s$" "red: red USB: en marcha; MAC 00:E0:4C:68$" "red: red USB: MAC ...:01:53$" \
    "red: red USB: el adaptador dice que el enlace va a 1000 Mbit/s$" "red: la red va por el adaptador de red USB$" "red: DHCP: IP 10.0.2.15, máscara 255.255.255.0, puerta 10.0.2.2, DNS 10.0.2.3$"
  mira "$que: IPCONFIG dice «Realtek RTL8153 por USB (CDC-ECM)», PING va (3 de 3) y DESCARGAR baja 400 K" \
    "^  Tarjeta . . . . . . . : Realtek RTL8153 por USB (CDC-ECM) (USB 0BDA:8153)$" "^  Por . . . . . . . . . : el adaptador de red USB (y su cable de red)$" \
    "^  MAC . . . . . . . . . : 00:E0:4C:68:01:53$" "^  Enlace  . . . . . . . : sí, a 1000 Mbit/s$" "^  Dirección IP  . . . . : 10.0.2.15$" \
    "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$" "^Hecho: GRANDE.BIN, 409600 octetos"
  if saca $K/$hc.img GRANDE.BIN $K/g.bin && [ "$(sha256sum < $K/g.bin)" = "$(sha256sum < $K/srv/grande.bin)" ]; then ok "$que: lo descargado por el RTL8153, con el mismo SHA-256 que lo del servidor"
  else ko "$que: lo descargado no es igual"; fi
  if grep -q "AAAAAAAA" $S; then ko "$que: el teclado USB se queda con una tecla pulsada mientras va la red"; else ok "$que: el teclado USB sigue bien con la red por el adaptador"; fi
  if saca $K/$hc.img RED.LOG $K/$hc.log && grep -q "la red va por el adaptador de red USB" $K/$hc.log; then ok "$que: RED.LOG (Registro=SI) lo apunta"; else ko "$que: RED.LOG no dice lo del adaptador"; fi
  sin_fallos $hc "$que"
done
# 2. sin cable y con cable; la tarjeta PCI con enlace gana
cp $K/d.img $K/enl.img
rtl enl espera "Corta" enlace 0 espera "Lo pone" enlace 1
SERIE_VIVA=$K/enl.viva ESCRIBE=1 DISCO=$K/enl.img ARRANQUE="ordenes conred" MODULOS= QEMU_MAS="-device qemu-xhci,id=u $RED_RTL -device usb-redir,chardev=rd,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":hasta 40 1 red: DHCP: IP" ":pausa 1" "eco Corta" ":pausa 3" ipconfig ":pausa 2" "eco Lo pone" ":hasta 30 2 red: DHCP: IP" "ping -n 2 10.0.2.2" ":pausa 3" > $K/enl.sal 2>&1
fin_rtl
tr -d '\r' < $K/enl.sal > $K/enl.txt
S=$K/enl.txt
mira "sin cable (el aviso NETWORK_CONNECTION a 0): «enlace: no» e IPCONFIG lo dice; con cable otra vez, DHCP y PING" \
  "red: red USB: el adaptador dice que no hay enlace (¿el cable?)$" "red: enlace: no (el adaptador de red USB: ¿el cable?)$" \
  "^  Enlace  . . . . . . . : no (¿está enchufado el cable?)$" "red: red USB: el adaptador dice que hay enlace (cable)$" \
  "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
[ "$(grep -c '^red: DHCP: IP 10.0.2.15' $S)" -ge 2 ] && ok "sin cable y con cable: DHCP las dos veces" || ko "sin cable y con cable: DHCP no va otra vez"
sin_fallos enl "sin cable y con cable"
cp $K/d.img $K/pci.img
rtl pci
ESCRIBE=1 DISCO=$K/pci.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u -netdev user,id=c0 -device e1000,netdev=c0,id=e0 $RED_RTL -device usb-redir,chardev=rd,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ipconfig ":pausa 2" ":monitor set_link e0 off" ":pausa 4" ipconfig ":pausa 1" "ping -n 2 10.0.2.2" ":pausa 3" > $K/pci.sal 2>&1
fin_rtl
tr -d '\r' < $K/pci.sal > $K/pci.txt
S=$K/pci.txt
mira "con una tarjeta PCI con enlace, gana la PCI; sin su enlace, el adaptador USB (y PING va)" \
  "red: red USB: en marcha; MAC 00:E0:4C:68$" "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000)" "red: la red va por el adaptador de red USB$" \
  "^  Tarjeta . . . . . . . : Realtek RTL8153 por USB (CDC-ECM) (USB 0BDA:8153)$" "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0"
cp $K/d.img $K/cal.img
rtl cal
ESCRIBE=1 DISCO=$K/cal.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u $RED_RTL" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ":monitor device_add usb-redir,chardev=rd,bus=u.0,id=r1" ":hasta 40 1 red: DHCP: IP" "ping -n 2 10.0.2.2" ":pausa 3" ":monitor device_del r1" ":pausa 4" "ipconfig" ":pausa 2" > $K/cal.sal 2>&1
fin_rtl
tr -d '\r' < $K/cal.sal > $K/cal.txt
S=$K/cal.txt
mira "en caliente: se enchufa en marcha (el servicio de red se pone solo, DHCP, PING) y se quita («ya no va por el adaptador»)" \
  "usb: se enchufa el aparato 0, 0BDA:8153, velocidad alta, .*; configuración 2 de 2; interfaz 02/06/00" "usb: aviso: Adaptador de red USB conectado$" \
  "red: la red va por el adaptador de red USB$" "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0" \
  "usb: el adaptador de red USB (aparato 0) ya no está$" "usb: aviso: El adaptador de red USB se ha desconectado$" "red: red USB: el adaptador de red USB se ha ido (desenchufado)$" \
  "red: la red ya no va por el adaptador de red USB$" "^  No hay ninguna tarjeta de red que Hispa-Pentium sepa llevar$"
# 3. con «sinredusb»: la configuración de siempre, sin red
cp $K/d.img $K/sin.img
rtl sin
ESCRIBE=1 DISCO=$K/sin.img ARRANQUE="ordenes sinredusb" MODULOS= QEMU_MAS="-device qemu-xhci,id=u $RED_RTL -device usb-redir,chardev=rd,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ipconfig ":pausa 2" > $K/sin.sal 2>&1
fin_rtl
tr -d '\r' < $K/sin.sal > $K/sin.txt
S=$K/sin.txt
mira "«sinredusb»: la primera configuración (la de Realtek), sin red, y lo dice" \
  "0BDA:8153, velocidad alta, .*; interfaz FF/FF/00$" "usb: el adaptador de red USB Realtek 8153 (aparato [0-9]*): la red USB está apagada («sinredusb»): no se usa$"
grep -q "set_configuration 1" $K/sin.rtl && ok "«sinredusb»: al RTL8153 se le pone la configuración 1" || ko "«sinredusb»: no se le pone la 1"
# 4 y 5. el usb-net de QEMU: con «usbecm», por ECM; sin, el móvil RNDIS
cp $K/d.img $K/net.img
ESCRIBE=1 DISCO=$K/net.img ARRANQUE="ordenes usbecm" MODULOS= QEMU_MAS="-device qemu-xhci,id=u -netdev user,id=n0 -device usb-net,netdev=n0,mac=40:54:00:12:34:58,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ipconfig ":pausa 2" "ping -n 3 10.0.2.2" ":pausa 3" "descargar $U/grande.bin" ":hasta 90 1 Hecho: GRANDE.BIN" > $K/net.sal 2>&1
tr -d '\r' < $K/net.sal > $K/net.txt
S=$K/net.txt
mira "el usb-net de QEMU con «usbecm»: su segunda configuración (ECM), DHCP, IPCONFIG, PING (3 de 3) y DESCARGAR, a velocidad plena" \
  "0525:A4A2, velocidad plena, .*; configuración 2 de 2; interfaz 02/06/00; interfaz 0A/00/00$" "usb: el adaptador de red USB (CDC-ECM, configuración 1): aparato [0-9]*, 0525:A4A2" "red: red USB: en marcha; MAC 40:54:00:12$" \
  "red: la red va por el adaptador de red USB$" "^  Tarjeta . . . . . . . : el adaptador de red USB (CDC-ECM) (USB 0525:A4A2)$" \
  "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$" "^Hecho: GRANDE.BIN, 409600 octetos"
if saca $K/net.img GRANDE.BIN $K/g2.bin && [ "$(sha256sum < $K/g2.bin)" = "$(sha256sum < $K/srv/grande.bin)" ]; then ok "usb-net por ECM: lo descargado, igual que lo del servidor"; else ko "usb-net por ECM: lo descargado no es igual"; fi
cp $K/d.img $K/rnd.img
ESCRIBE=1 DISCO=$K/rnd.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u -netdev user,id=n0 -device usb-net,netdev=n0,mac=40:54:00:12:34:59,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ipconfig ":pausa 2" > $K/rnd.sal 2>&1
tr -d '\r' < $K/rnd.sal > $K/rnd.txt
S=$K/rnd.txt
mira "sin «usbecm», el usb-net sigue siendo el móvil (RNDIS, la primera configuración)" \
  "usb: el móvil por USB (Android, RNDIS): aparato [0-9]*, 0525:A4A2; interfaces 0 y 1 (clase 0202FF), extremos 82 y 02$" \
  "^  Tarjeta . . . . . . . : el móvil por USB (Android, RNDIS) (USB 0525:A4A2)$"
if grep -q "configuración 2 de 2" $S; then ko "sin «usbecm», el usb-net no tenía que cambiar de configuración"; else ok "sin «usbecm», el usb-net se queda en su primera configuración"; fi
# 6. el escritorio sin tarjeta PCI, con el adaptador
rtl esc
CAPTURAS=$G DISCO=$K/d.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-device qemu-xhci,id=u $RED_RTL -device usb-redir,chardev=rd,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":hasta 40 1 red: DHCP: IP" ":pausa 2" ":clic 720 583" ":pausa 1" > $K/esc.sal 2>&1
fin_rtl
S=$K/esc.sal
mira "el escritorio, sin tarjeta PCI y con el adaptador: RED.BIN al arrancar y, al pulsar el icono de la red, «La red: por el adaptador de red USB»" \
  "red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)" "red: la red va por el adaptador de red USB" "escritorio: aviso: La red: por el adaptador de red USB, con la IP 10.0.2.15\."
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
}
