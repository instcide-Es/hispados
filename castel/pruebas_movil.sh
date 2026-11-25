# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# pruebas_movil.sh - la parte 55 (SOLO=movil) de pruebas.sh, que lee este
# archivo («.»): (MÓVIL) el móvil por USB (el «Anclaje de USB» de Android),
# con los ayudantes de pruebas.sh (ok, ko, mira, saca, sano) y sus carpetas
# ($AQUI, $P, $G).
#
# QEMU tiene -device usb-net, que da RNDIS en su primera configuración (la que
# pone el núcleo) y CDC-ECM en la segunda: hace de móvil Android con el
# anclaje (con -netdev user: DHCP, la puerta 10.0.2.2 y el DNS 10.0.2.3). Es
# de velocidad plena (USB 1.1): en el xHCI va por su anillo (recibir sin
# esperar); detrás de un EHCI con compañeras, por la UHCI (su fila de TD).
#  1. xHCI, el móvil enchufado al arrancar: el núcleo lo reconoce (RNDIS,
#     02/02/FF), RED.BIN lo pone en marcha (INITIALIZE, la MAC, el filtro),
#     «la red va por el móvil por USB», DHCP, IPCONFIG («el móvil por USB»),
#     PING y DESCARGAR (400 K, el SHA-256 igual que el del servidor).
#  2. EHCI con sus UHCI compañeras: lo mismo por la UHCI (con un teclado USB
#     en la misma UHCI, que sigue tecleando bien).
#  3. En caliente (xHCI): sin tarjeta; se enchufa (device_add): el núcleo
#     pone en marcha RED.BIN solo (no hay programa que lo pida), DHCP y
#     PING; se quita (device_del): «se ha ido», sin tarjeta; vuelve con
#     otra MAC (el móvil que cambia de cara: otra generación): otra vez.
#  4. Cable y móvil: el cable con enlace gana; sin enlace (set_link), el
#     móvil; con enlace otra vez, el cable.
#  5. NCM (que QEMU no tiene) y lo que el RNDIS de QEMU no hace (varias
#     tramas en una transferencia): UMOVILP.BIN con los datos grabados de
#     tests/movil_ncm.py (los NTB16 como los arma f_ncm de Linux, y otros).
#  6. La cola a granel que no espera en el EHCI (que el usb-net, de
#     velocidad plena, no alcanza): una memoria USB de alta velocidad con
#     «usbasinc» (usbalm.cas recibe por ella): copiar a la memoria y de
#     ella, igual octeto a octeto, y fsck.fat.
#  7. El escritorio, sin tarjeta PCI y con el móvil: RED.BIN en marcha al
#     arrancar y, al pulsar el icono de la red, «La red: por el móvil por USB».

parte_movil() {
echo "== (MÓVIL) El móvil por USB (el anclaje de USB de Android: RNDIS; NCM con datos grabados) =="
local K=$P/movil SPID U S hc dev que nom
mkdir -p $K/srv $G
export MTOOLS_SKIP_CHECK=1
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/semilla.cas -o $K/semilla.asm > /dev/null 2>&1 && nasm -f bin $K/semilla.asm -o $K/SEMILLA.BIN || { ko "semilla.cas no compila"; return; }
$AQUI/nucleo32/disco.sh $K/d.img 16 red > $K/disco.txt 2>&1 || { ko "disco.sh no hace el disco: $(tail -n 2 $K/disco.txt)"; return; }
python3 -I $AQUI/ccastel.py $AQUI/nucleo32/umovilp.cas -o $K/umovilp.asm > /dev/null 2>&1 && nasm -f bin $K/umovilp.asm -o $K/UMOVILP.BIN || { ko "umovilp.cas no compila"; return; }
python3 -I $AQUI/nucleo32/servidor_prueba.py $K/srv > $K/srv.txt 2>&1 &
SPID=$!
for _ in $(seq 50); do [ -s $K/srv/puerto ] && break; sleep 0.1; done
U=http://10.0.2.2:$(cat $K/srv/puerto 2> /dev/null)
local NET="-netdev user,id=n0"
local MOVIL="usb-net,netdev=n0,mac=40:54:00:12:34:56"
# 1 y 2. al arrancar, en el xHCI y detrás de un EHCI (por la UHCI compañera)
for hc in xhci ehci; do
  case $hc in
    xhci) dev="-device qemu-xhci,id=u"; que="xHCI" ;;
    ehci) dev="-device ich9-usb-ehci1,id=u -device ich9-usb-uhci1,masterbus=u.0,firstport=0 -device ich9-usb-uhci2,masterbus=u.0,firstport=2"; que="EHCI (por la UHCI compañera)" ;;
  esac
  cp $K/d.img $K/$hc.img
  ESCRIBE=1 DISCO=$K/$hc.img ARRANQUE=ordenes MODULOS= MAQUINA=pc,i8042=off QEMU_MAS="$dev $NET -device $MOVIL,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
    ":pausa 1" ":monitor device_add usb-kbd,bus=u.0,id=k1" ":pausa 2" ipconfig ":pausa 2" "ping -n 3 10.0.2.2" ":pausa 3" "descargar $U/grande.bin" ":hasta 60 1 Hecho: GRANDE.BIN" "ipconfig" ":pausa 1" > $K/$hc.sal 2>&1
  tr -d '\r' < $K/$hc.sal > $K/$hc.txt
  S=$K/$hc.txt
  mira "$que: el núcleo reconoce el móvil (RNDIS, 02/02/FF) y RED.BIN lo pone en marcha: la MAC, «la red va por el móvil por USB» y DHCP" \
    "^usb: el móvil por USB (Android, RNDIS): aparato [0-9]*, 0525:A4A2; interfaces 0 y 1 (clase 0202FF), extremos 82 y 02$" \
    "^red: móvil por USB: RNDIS 1.0, medio 0, hasta [0-9]* octetos$" "^red: móvil por USB: en marcha; MAC 40:54:00:12$" "^red: móvil por USB: MAC ...:34:56$" \
    "^red: la red va por el móvil por USB$" "^red: DHCP: IP 10.0.2.15, máscara 255.255.255.0, puerta 10.0.2.2, DNS 10.0.2.3$"
  mira "$que: IPCONFIG dice «el móvil por USB (Android, RNDIS)», PING va (3 de 3) y DESCARGAR baja 400 K" \
    "^  Tarjeta . . . . . . . : el móvil por USB (Android, RNDIS) (USB 0525:A4A2)$" "^  Por . . . . . . . . . : el cable USB del móvil (Anclaje de USB)$" \
    "^  MAC . . . . . . . . . : 40:54:00:12:34:56$" "^  Enlace  . . . . . . . : sí, a 12 Mbit/s$" "^  Dirección IP  . . . . : 10.0.2.15$" \
    "^    Paquetes: enviados = 3, recibidos = 3, perdidos = 0 (0% perdidos)$" "^Hecho: GRANDE.BIN, 409600 octetos"
  if grep -q "tiempo=[0-9]\{4,\} ms" $S; then ko "$que: un PING con un tiempo imposible: $(grep -m1 "tiempo=" $S)"; fi
  if saca $K/$hc.img GRANDE.BIN $K/g.bin && [ "$(sha256sum < $K/g.bin)" = "$(sha256sum < $K/srv/grande.bin)" ]; then ok "$que: lo descargado por el móvil, con el mismo SHA-256 que lo del servidor"
  else ko "$que: lo descargado no es igual"; fi
  # (en la UHCI, con la red en marcha, el teclado USB de la misma controladora sigue: antes, recibiendo con granel() esperando, se quedaba una tecla pulsada)
  if grep -q "AAAAAAAA" $S; then ko "$que: el teclado USB se queda con una tecla pulsada mientras va la red"; else ok "$que: el teclado USB sigue bien con la red por el móvil"; fi
done
# 3. en caliente: sin tarjeta; el núcleo pone RED.BIN en marcha al llegar el móvil
cp $K/d.img $K/cal.img
ESCRIBE=1 DISCO=$K/cal.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u $NET" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 2" ":monitor device_add $MOVIL,bus=u.0,id=m1" ":hasta 30 1 red: DHCP: IP" "ping -n 2 10.0.2.2" ":pausa 2" ":monitor device_del m1" ":pausa 4" "ipconfig" ":pausa 1" \
  ":monitor device_add usb-net,netdev=n0,bus=u.0,mac=40:54:00:12:34:57,id=m2" ":hasta 30 2 red: DHCP: IP" "ping -n 2 10.0.2.2" ":pausa 2" "ipconfig" ":pausa 1" > $K/cal.sal 2>&1
tr -d '\r' < $K/cal.sal > $K/cal.txt
S=$K/cal.txt
mira "en caliente: el móvil que se enchufa (o activa el anclaje) en marcha pone en marcha el servicio de red, sin que nadie lo pida, y la red va" \
  "usb: se enchufa el aparato 0, 0525:A4A2, velocidad plena, en el puerto [0-9]* (xHCI); interfaz 02/02/FF; interfaz 0A/00/00$" "^usb: aviso: Móvil conectado por USB: la red va por él$" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)$" "^red: la red va por el móvil por USB$" "^red: DHCP: IP 10.0.2.15"
mira "en caliente: al quitarlo, la red lo sabe («se ha ido», sin tarjeta); y vuelve con otra cara (otra MAC): otra vez en marcha, otra IP y PING" \
  "^usb: el móvil por USB (aparato 0) ya no está$" "^usb: aviso: El móvil por USB se ha desconectado$" "^red: móvil por USB: se ha ido" \
  "^red: la red ya no va por el móvil por USB (no hay cable)$" "^  No hay ninguna tarjeta de red que Hispa-Pentium sepa llevar$" "^red: móvil por USB: MAC ...:34:57$" \
  "^  MAC . . . . . . . . . : 40:54:00:12:34:57$"
[ "$(grep -c "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0" $S)" = 2 ] && ok "en caliente: PING va las dos veces (2 de 2)" || ko "en caliente: PING no va las dos veces"
# 4. el cable gana; sin enlace, el móvil; con enlace, otra vez el cable
cp $K/d.img $K/pri.img
ESCRIBE=1 DISCO=$K/pri.img ARRANQUE=ordenes MODULOS= QEMU_MAS="-device qemu-xhci,id=u -nic user,model=e1000,id=c0 -netdev user,id=n0 -device $MOVIL,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":pausa 1" ipconfig ":pausa 2" ":monitor set_link c0 off" ":pausa 3" ipconfig ":pausa 1" "ping -n 2 10.0.2.2" ":pausa 2" ":monitor set_link c0 on" ":pausa 6" ipconfig ":pausa 1" > $K/pri.sal 2>&1
tr -d '\r' < $K/pri.sal > $K/pri.txt
S=$K/pri.txt
mira "cable y móvil: con enlace, el cable; sin enlace (set_link off), el móvil (y PING va); con enlace otra vez, el cable" \
  "^red: móvil por USB: en marcha; MAC 40:54:00:12$" "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000)" "red: la red va por el móvil por USB$" \
  "^  Tarjeta . . . . . . . : el móvil por USB (Android, RNDIS) (USB 0525:A4A2)$" "^    Paquetes: enviados = 2, recibidos = 2, perdidos = 0" "^red: la red va por el cable$"
[ "$(grep -c "^  Tarjeta . . . . . . . : Intel PRO/1000 (e1000)" $S)" = 2 ] && ok "cable y móvil: IPCONFIG dice el cable antes y después" || ko "cable y móvil: IPCONFIG no dice el cable las dos veces"
# 5. NCM y RNDIS con datos grabados
cp $K/d.img $K/ncm.img
python3 -I $AQUI/../tests/movil_ncm.py hacer $K/MOVIL.IN
mcopy -o -i $K/ncm.img $K/UMOVILP.BIN ::UMOVILP.BIN && mcopy -o -i $K/ncm.img $K/MOVIL.IN ::MOVIL.IN
ESCRIBE=1 DISCO=$K/ncm.img ARRANQUE=ordenes MODULOS= $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN ":pausa 1" umovilp ":pausa 2" > $K/ncm.sal 2>&1
if saca $K/ncm.img MOVIL.OUT $K/MOVIL.OUT; then
  python3 -I $AQUI/../tests/movil_ncm.py mirar $K/MOVIL.OUT > $K/ncm.txt 2>&1
  while IFS= read -r l; do
    case "$l" in "  OK   "*) ok "(datos grabados) ${l#  OK   }" ;; "  FAIL "*) ko "(datos grabados) ${l#  FAIL }" ;; esac
  done < $K/ncm.txt
else ko "UMOVILP no ha dejado MOVIL.OUT: $(tr -d '\r' < $K/ncm.sal | grep -i umovilp | tail -2)"; fi
# 6. la cola a granel que no espera del EHCI (y el anillo del xHCI), con una memoria de alta velocidad
python3 -c "import random; random.seed(55); open('$K/AZAR.BIN','wb').write(bytes(random.getrandbits(8) for _ in range(300000)))"
for hc in ehci xhci; do
  case $hc in ehci) dev="usb-ehci,id=u"; que="EHCI: la cola asíncrona propia"; nom=EHCI ;; xhci) dev="qemu-xhci,id=u"; que="xHCI: el anillo del extremo, a otro búfer"; nom=xHCI ;; esac
  rm -f $K/mem.img; dd if=/dev/zero of=$K/mem.img bs=1M count=16 status=none; mkfs.fat -F 16 -n MEMORIA $K/mem.img > /dev/null
  mcopy -i $K/mem.img $K/AZAR.BIN ::AZAR.BIN
  cp $K/d.img $K/asi.img
  ESCRIBE=1 DISCO=$K/asi.img ARRANQUE="ordenes usbasinc" MODULOS= QEMU_MAS="-device $dev -drive if=none,id=mm,file=$K/mem.img,format=raw -device usb-storage,bus=u.0,drive=mm" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
    ":pausa 1" "copy x:\\azar.bin c:\\azar.bin" ":pausa 3" "copy c:\\azar.bin x:\\copia.bin" ":pausa 3" "dir x:" ":pausa 1" > $K/asi$hc.sal 2>&1
  tr -d '\r' < $K/asi$hc.sal > $K/asi$hc.txt
  S=$K/asi$hc.txt
  if saca $K/asi.img AZAR.BIN $K/a1.bin && cmp -s $K/a1.bin $K/AZAR.BIN && saca $K/mem.img COPIA.BIN $K/a2.bin && cmp -s $K/a2.bin $K/AZAR.BIN && sano $K/mem.img && grep -q "velocidad [a-zú]*, en el puerto [0-9]* ($nom); almacenamiento" $S; then
    ok "recibir a granel sin esperar ($que; con «usbasinc», una memoria USB de alta velocidad): 300 K leídos y escritos, iguales, y la memoria sana"
  else ko "recibir a granel sin esperar ($que) no va:"; grep -i "usb\|copia\|disco" $S | head -8 | sed 's/^/        /'; fi
done
# 7. el escritorio sin tarjeta PCI, con el móvil
CAPTURAS=$G DISCO=$K/d.img ARRANQUE=escritorio MODULOS= QEMU_MAS="-device qemu-xhci,id=u $NET -device $MOVIL,bus=u.0" $AQUI/nucleo32/probar.sh $K/SEMILLA.BIN \
  ":hasta 40 1 red: DHCP: IP" ":pausa 2" ":clic 720 583" ":pausa 1" > $K/esc.sal 2>&1
S=$K/esc.sal
mira "el escritorio, sin tarjeta PCI y con el móvil: RED.BIN al arrancar y, al pulsar el icono de la red, «La red: por el móvil por USB (Android)»" \
  "^red: Hispa-Pentium 0.8: el servicio de red (RED.BIN)" "^red: la red va por el móvil por USB" "escritorio: aviso: La red: por el móvil por USB (Android), con la IP 10.0.2.15\."
kill $SPID 2> /dev/null; wait $SPID 2> /dev/null
}
