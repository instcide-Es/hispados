#!/bin/bash
# Pruebas del Wi-Fi de Hispa-486 (docs/WIFI.md). Sin la tarjeta:
#  1. build/prueba_wifi (anfitrión): vectores de las normas (SHA-1, HMAC,
#     PBKDF2, PRF, AES, envoltorio de claves, CCMP) y la gestión 802.11 con
#     WPA2 contra el punto de acceso de mentira.
#  2. build/chip_sim (anfitrión): el controlador de la Broadcom (brcmsmac,
#     bcma y nuestra capa) contra el modelo de la tarjeta.
#  3. QEMU, HSPWIFI /SIMULA: el residente, el paso a modo protegido, el
#     packet driver y la orden WIFI con la radio simulada: buscar, conectar
#     (clave equivocada, clave preguntada sin enseñarla, por número, un
#     punto de acceso que no contesta),
#     CONFRED (DHCP), PING, RED.INI (sin la contraseña), /AUTO, OLVIDAR, /U.
#  4. QEMU, núcleo de pruebas (HSPWSIM.BIN: el núcleo de verdad con el modelo
#     de la tarjeta dentro) y el microcódigo de verdad: puesta en marcha,
#     WPA2, DHCP, PING y el registro en el disco.
#  5. QEMU con HSPEMM (modo V86): lo dice y no se carga (y WIFI tampoco lo
#     intenta: «solo en la configuración Normal»).
#  6. QEMU con HSPRED y una NE2000: HSPWIFI no se carga al lado... salvo con
#     /JUNTO, y entonces la red sigue yendo por el cable.
#  7. QEMU, WIFI sin HSPWIFI cargado: lo carga él (HAYWIFI=SI y el núcleo de
#     pruebas) y sigue con la orden; el residente queda donde quedaría
#     cargándolo a mano (MEM /F igual: WIFI no deja su hueco debajo).
# Dos QEMU a la vez como mucho. Wi-Fi tests without the hardware.
set -u
cd "$(dirname "$0")/.."
IMG=${1:-build/hispados.img}
B=build
for f in HSPWIFI.COM WIFI.COM HSPWIFI.BIN HSPWSIM.BIN BCM43XX.FW BCM43XXH.FW CONFRED.COM PING.COM prueba_wifi chip_sim; do
  [ -f $B/$f ] || { echo "falta $B/$f (make)"; exit 1; }
done
T=$(mktemp -d)
trap '[ -n "${GUARDAR:-}" ] && cp $T/*.txt "$GUARDAR"/ 2>/dev/null; rm -rf $T' EXIT
fail=0
ok() { echo "  OK   $1"; }
ko() { echo "  FAIL $1"; fail=1; }
check() { if grep -qF -- "$2" $T/$1.txt; then ok "$2"; else ko "$2"; fi; }
checkn() { if grep -qF -- "$2" $T/$1.txt; then ko "(no debía salir) $2"; else ok "sin «$2»"; fi; }

echo "== Wi-Fi: normas y gestión 802.11 (anfitrión) =="
if $B/prueba_wifi > $T/host.txt 2>&1 && grep -q ", 0 fallos" $T/host.txt; then ok "$(tail -1 $T/host.txt)"; else ko "prueba_wifi"; tail -20 $T/host.txt; fi
echo "== Wi-Fi: controlador de la BCM43224 contra el modelo de la tarjeta (anfitrión) =="
if $B/chip_sim > $T/chip.txt 2>&1 && grep -q ", 0 fallos" $T/chip.txt; then ok "$(tail -1 $T/chip.txt)"; else ko "chip_sim"; tail -20 $T/chip.txt | iconv -f CP437 -t UTF-8 -c; fi

mkimg() {   # mkimg nombre núcleo config
  cp "$IMG" $T/$1.img
  # sitio para el núcleo (400 KB) y el microcódigo
  for f in HISPA.WAV VENTANAS.COM AUDICION.COM HSPSND.COM EQUIPO.COM CONFIG.HVP HSPUSB.SYS HSPDPMI.COM; do
    mdel -i $T/$1.img ::HISPADOS/$f 2>/dev/null
  done
  mdeltree -i $T/$1.img ::HISPADOS/PROGS ::HISPADOS/JUEGOS 2>/dev/null
  printf "$3" > $T/CONFIG.SYS
  mcopy -o -i $T/$1.img $T/CONFIG.SYS ::CONFIG.SYS
  cp $B/$2 $T/HSPWIFI.BIN
  mcopy -o -i $T/$1.img $B/HSPWIFI.COM $B/WIFI.COM $T/HSPWIFI.BIN $B/BCM43XX.FW $B/BCM43XXH.FW ::HISPADOS/
}
corre() {   # corre nombre plazo 'segundos|orden'...
  local n=$1 tmo=$2; shift 2
  { sleep 7
    for c in "$@"; do printf '%s\r' "${c#*|}"; sleep "${c%%|*}"; done
  } | timeout $tmo qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor none -parallel none \
        -m 32 -drive file=$T/$n.img,format=raw,if=floppy -boot a -no-reboot > $T/$n.raw 2>&1
  tr -d '\r' < $T/$n.raw | iconv -f CP437 -t UTF-8 -c > $T/$n.txt
}
XMS='DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDOS=HIGH\r\n'

mkimg sim HSPWIFI.BIN "$XMS"
corre sim 150 '3|WIFI' '6|HSPWIFI /SIMULA' '2|HSPWIFI' '5|WIFI REDES' '3|WIFI' \
  '9|WIFI CONECTAR HispaPrueba otra clave mala' '3|WIFI' \
  '9|WIFI CONECTAR NoExiste clave' '5|WIFI CONECTAR 1' '6|WIFI CONECTAR 3' '2|WIFI CONECTAR 9' '2|WIFI DESCONECTAR' \
  '3|WIFI CONECTAR 2' '8|clave de prueba' '2|WIFI' '6|CONFRED' '7|PING 192.168.77.1 /N:3' \
  '3|TYPE A:\HISPADOS\RED.INI' '2|WIFI DESCONECTAR' '2|PING 192.168.77.1 /N:1' '6|WIFI /AUTO' \
  '5|PING 192.168.77.1 /N:2' '3|WIFI REGISTRO A:\W.LOG' '2|WIFI OLVIDAR' '3|TYPE A:\HISPADOS\RED.INI' \
  '2|WIFI /AUTO' '3|HSPWIFI /U' '2|WIFI' '2|WIFI /AUTO' '2|ECHO fin-de-la-prueba' &
P1=$!
mkimg chip HSPWSIM.BIN "$XMS"
corre chip 95 '14|HSPWIFI /REG:A:\WIFI.LOG' '10|WIFI CONECTAR HispaPrueba clave de prueba' '6|CONFRED' \
  '7|PING 192.168.77.1 /N:3' '3|WIFI' '3|WIFI REGISTRO' '3|HSPWIFI /U' '2|ECHO fin-de-la-prueba' &
P2=$!
wait $P1 $P2

echo "== Wi-Fi en QEMU: radio simulada (residente, packet driver y orden WIFI) =="
check sim "Este equipo no tiene una tarjeta Wi-Fi de las que conoce Hispa-DOS."
checkn sim "Cárguelo antes"
checkn sim "Cargando el controlador Wi-Fi"
check sim "Tarjeta Wi-Fi simulado (pruebas), MAC 02:48:53:57:46:01, controlador de paquetes en INT 60h"
check sim "HSPWIFI ya está cargado."
check sim "  2  HispaPrueba                        6  ████ -47 dBm  WPA2"
check sim "  3  Vecina abierta                    11  ████ -47 dBm  abierta"
check sim "Wi-Fi: sin conectar. Para ver las redes: WIFI REDES"
check sim "No se ha podido conectar: la contraseña no es la de esta red."
check sim "Esa red no aparece. Mire las que hay con WIFI REDES."
check sim "Esa red no se puede usar: su seguridad es WPA antigua (no se admite)."
check sim "Conectando con «Vecina abierta»..."
check sim "No se ha podido conectar: el punto de acceso no contesta (señal débil)."
check sim "Ese número no está en la lista de WIFI REDES."
check sim "Contraseña de la red: ***************"
checkn sim "Contraseña de la red: clave"
check sim "Conectado a «HispaPrueba» (señal -47 dBm)."
check sim "Wi-Fi: conectado a «HispaPrueba» (canal 6, señal -47 dBm ████, WPA2)"
check sim "  Tarjeta . . . . . . . : Wi-Fi simulado (pruebas) (INT 60h)"
check sim "  Dirección IP  . . . . : 192.168.77.10"
check sim "  Obtenida  . . . . . . : por DHCP (servidor 192.168.77.1"
n=$(grep -c "Respuesta desde 192.168.77.1: octetos=32" $T/sim.txt)
if [ "$n" -eq 5 ]; then ok "PING: 5 respuestas (3 y, tras /AUTO, 2); ninguna estando desconectado"; else ko "PING: $n respuestas (tenían que ser 5)"; fi
check sim "WIFI=HispaPrueba"
if grep -Eq "^WIFICLAVE=[0-9a-f]{64}$" $T/sim.txt; then ok "RED.INI guarda la clave preparada (64 cifras)"; else ko "WIFICLAVE= en RED.INI"; fi
checkn sim "WIFICLAVE=clave"
check sim "DHCP=SI"
check sim "Registro guardado en A:\\W.LOG"
check sim "Red quitada de RED.INI."
n=$(grep -c "^WIFI=HispaPrueba" $T/sim.txt)
if [ "$n" -eq 1 ]; then ok "tras OLVIDAR, RED.INI ya no lleva la red"; else ko "WIFI= sale $n veces"; fi
check sim "HSPWIFI descargado."
rm -f $T/w.log; mcopy -i $T/sim.img ::W.LOG $T/w.log 2>/dev/null
iconv -f CP437 -t UTF-8 -c $T/w.log > $T/wlog.txt 2>/dev/null
check wlog "WPA: paso 3 de 4 correcto; se manda el 4. Claves puestas"
check wlog "802.11: CONECTADO a «HispaPrueba»"
check wlog "ESTADO: CONECTADO"
check sim "fin-de-la-prueba"

echo "== Wi-Fi en QEMU: el núcleo con la tarjeta simulada y el microcódigo de verdad =="
check chip "Tarjeta Wi-Fi Broadcom BCM43224, MAC 00:26:82:12:34:56, controlador de paquetes en INT 60h"
check chip "Conectado a «HispaPrueba»"
check chip "  Dirección IP  . . . . : 192.168.77.10"
n=$(grep -c "Respuesta desde 192.168.77.1: octetos=32" $T/chip.txt)
if [ "$n" -eq 3 ]; then ok "PING: 3 respuestas"; else ko "PING: $n respuestas"; fi
check chip "HSPWIFI descargado."
rm -f $T/w.log; mcopy -i $T/chip.img ::WIFI.LOG $T/w.log 2>/dev/null
iconv -f CP437 -t UTF-8 -c $T/w.log > $T/clog.txt 2>/dev/null
check clog "Chip BCM43224 (0xA8D8), revisión 1"
check clog "SPROM: revisión 8"
check clog "Microcódigo brcm/bcm43xx-0.fw: 96224 octetos"
check clog "Radio encendida en el canal 1"
check clog "802.11: CONECTADO a «HispaPrueba»"
check clog "PASO 3 de 3 hecho: radio en marcha"
# (la base del núcleo, alineada a 64 KB: si no, los anillos de DMA quedan mal)
if grep -Eq "Memoria: bloque de 2048 KB en [0-9A-F]{4}0000;" $T/clog.txt; then ok "la base del núcleo está alineada a 64 KB"; else ko "base del núcleo sin alinear"; fi
check clog "ESTADO: el montón está entero"
checkn clog "MEMORIA:"
checkn clog "EXCEPCI"
check chip "fin-de-la-prueba"

echo "== Wi-Fi en QEMU: con HSPEMM (modo V86) no se carga =="
mkimg v86 HSPWIFI.BIN 'DEVICE=A:\\HISPADOS\\HSPXMS.SYS\r\nDEVICE=A:\\HISPADOS\\HSPEMM.SYS\r\nDOS=HIGH\r\n'
corre v86 30 '4|HSPWIFI /SIMULA' '2|WIFI REDES' '2|WIFI /AUTO' '2|ECHO fin-de-la-prueba'
check v86 "HSPWIFI no funciona con HSPEMM"
check v86 "El Wi-Fi solo funciona en la configuración «Normal» (sin HSPEMM)."
check v86 "Reinicie y elija «Normal»."
n=$(grep -c "Reinicie y elija" $T/v86.txt)
if [ "$n" -eq 1 ]; then ok "WIFI /AUTO con HSPEMM: calla"; else ko "«Reinicie y elija» sale $n veces (WIFI /AUTO tenía que callar)"; fi
checkn v86 "Cargando el controlador Wi-Fi"
check v86 "fin-de-la-prueba"

echo "== Wi-Fi en QEMU: /JUNTO, al lado de HSPRED (la red sigue por el cable) =="
mkimg junto HSPWIFI.BIN "$XMS"
{ sleep 7
  for c in '4|HSPRED' '5|CONFRED' '3|HSPWIFI /SIMULA' '6|HSPWIFI /SIMULA /JUNTO' \
           '9|WIFI CONECTAR HispaPrueba clave de prueba' '5|PING 10.0.2.2 /N:2' '3|CONFRED' \
           '3|HSPWIFI /U' '3|PING 10.0.2.2 /N:1' '2|HSPRED /U' '2|ECHO fin-de-la-prueba'; do
    printf '%s\r' "${c#*|}"; sleep "${c%%|*}"
  done
} | timeout 70 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor none -parallel none \
      -m 32 -drive file=$T/junto.img,format=raw,if=floppy -boot a -no-reboot \
      -netdev user,id=n0 -device ne2k_pci,netdev=n0 > $T/junto.raw 2>&1
tr -d '\r' < $T/junto.raw | iconv -f CP437 -t UTF-8 -c > $T/junto.txt
check junto "La red va por el cable (HSPRED). Para usar el Wi-Fi:"
check junto "Tarjeta Wi-Fi simulado (pruebas), MAC 02:48:53:57:46:01, controlador de paquetes en INT 61h"
check junto "Conectado a «HispaPrueba» (señal -47 dBm)."
n=$(grep -c "Respuesta desde 10.0.2.2: octetos=32" $T/junto.txt)
if [ "$n" -eq 3 ]; then ok "PING por el cable con el Wi-Fi al lado, y tras descargarlo: 3 respuestas"; else ko "PING 10.0.2.2: $n respuestas"; fi
n=$(grep -c "Dirección IP  . . . . : 10.0.2.15" $T/junto.txt)
if [ "$n" -eq 2 ]; then ok "la configuración IP de HSPRED no se toca al conectar el Wi-Fi"; else ko "CONFRED: $n veces 10.0.2.15"; fi
check junto "HSPWIFI descargado."
check junto "HSPRED descargado."
check junto "fin-de-la-prueba"

echo "== Wi-Fi en QEMU: HSPWIFI /AUTO (el AUTOEXEC.BAT de Hispa-486: cable primero) =="
# sin red guardada se carga (desde la 0.9.28: así Hispa Ventanas puede
# buscar las redes); sin tarjeta (sin /SIMULA, QEMU no tiene Broadcom)
# calla; con HSPRED con enlace calla y se queda el cable; con el cable
# quitado (set_link off), descarga HSPRED y se carga el Wi-Fi
mkimg auto HSPWIFI.BIN "$XMS"
rm -f $T/amon
{ sleep 7
  for c in '6|HSPWIFI /AUTO /SIMULA' '1|ECHO =A1=' '2|WIFI' '3|HSPWIFI /U' \
           '6|HSPWIFI /SIMULA' '9|WIFI CONECTAR HispaPrueba clave de prueba' '3|HSPWIFI /U' \
           '6|HSPWIFI /AUTO' '1|ECHO =A2=' '2|WIFI' \
           '4|HSPRED /AUTO' '4|HSPWIFI /AUTO /SIMULA' '1|ECHO =A3=' '2|WIFI' '3|PING 10.0.2.2 /N:1' \
           'L|' '9|HSPWIFI /AUTO /SIMULA' '9|WIFI /AUTO' '6|CONFRED' '3|HSPRED /U' '3|HSPWIFI /U' '2|ECHO fin-de-la-prueba'; do
    if [ "${c%%|*}" = L ]; then printf 'set_link n0 off\n' | socat - UNIX-CONNECT:$T/amon > /dev/null 2>&1; sleep 1; continue; fi
    printf '%s\r' "${c#*|}"; sleep "${c%%|*}"
  done
} | timeout 120 qemu-system-i386 ${QEMU_PANTALLA--vga std -global VGA.xres=800 -global VGA.yres=600} -display none -serial stdio -monitor unix:$T/amon,server,nowait -parallel none \
      -m 32 -drive file=$T/auto.img,format=raw,if=floppy -boot a -no-reboot \
      -netdev user,id=n0 -device e1000,netdev=n0 > $T/auto.raw 2>&1
tr -d '\r' < $T/auto.raw | iconv -f CP437 -t UTF-8 -c > $T/auto.txt
sed -n '/HSPWIFI \/AUTO \/SIMULA/,/=A1=/p' $T/auto.txt | head -3 > $T/auto1.txt
if grep -q "HSPWIFI 0.1" $T/auto1.txt; then ok "/AUTO sin red guardada (y sin cable): se carga, para elegir la red"; else ko "/AUTO sin red guardada: tenía que cargarse (0.9.28)"; fi
sed -n '/^.*>HSPWIFI \/AUTO$/,/=A2=/p' $T/auto.txt > $T/auto2.txt
if grep -q "HSPWIFI 0.1" $T/auto2.txt; then ko "/AUTO sin tarjeta: tenía que callar"; else ok "/AUTO sin tarjeta Wi-Fi: calla"; fi
sed -n '/HSPRED \/AUTO/,/=A3=/p' $T/auto.txt > $T/auto3.txt
if grep -q "HSPWIFI 0.1" $T/auto3.txt; then ko "/AUTO con cable: tenía que callar"; else ok "/AUTO con HSPRED y enlace: calla y se queda el cable"; fi
check auto "La red por cable no tiene enlace (sin cable): HSPRED descargado."
check auto "Tarjeta Wi-Fi simulado (pruebas), MAC 02:48:53:57:46:01, controlador de paquetes en INT 60h"
check auto "  Dirección IP  . . . . : 192.168.77.10"
check auto "HSPWIFI descargado."
check auto "fin-de-la-prueba"

echo "== Cable o Wi-Fi: HSPRED /AUTO y HSPWIFI /AUTO con Preferir= (RED.INI) =="
# HAYWIFI=SI hace como si hubiera tarjeta Wi-Fi en el PCI (QEMU no tiene
# ninguna de las admitidas); la radio, la simulada (/SIMULA). La e1000
# empieza sin enlace (set_link off antes de escribir nada).
mkimg pref HSPWIFI.BIN "$XMS"
mtype -i $T/pref.img ::HISPADOS/RED.INI > $T/RED.INI
printf 'HAYWIFI=SI\r\n' >> $T/RED.INI
mcopy -o -i $T/pref.img $T/RED.INI ::HISPADOS/RED.INI
rm -f $T/pmon
{ sleep 5
  printf 'set_link n0 off\n' | socat - UNIX-CONNECT:$T/pmon > /dev/null 2>&1; sleep 2
  for c in '6|HSPRED /AUTO' '1|ECHO =P1=' '2|HSPRED /U' '1|ECHO =P1B=' '6|HSPWIFI /AUTO /SIMULA' '1|ECHO =P2=' '3|HSPWIFI /U' \
           'E|' '4|HSPRED /AUTO' '1|ECHO =P3=' '3|HSPRED /U' \
           '1|ECHO Preferir=WIFI>>A:\HISPADOS\RED.INI' '4|HSPRED /AUTO' '1|ECHO =P4=' \
           'D|' '1|ECHO Preferir=CABLE>>A:\HISPADOS\RED.INI' '4|HSPRED /AUTO' '1|ECHO =P5=' '6|HSPWIFI /AUTO /SIMULA' '1|ECHO =P6=' \
           'E|' '6|HSPWIFI /SIMULA' '1|ECHO =P7=' '6|WIFI REDES' '1|ECHO =P8=' '3|HSPRED /U' '2|ECHO fin-de-la-prueba'; do
    case "${c%%|*}" in
      E) printf 'set_link n0 on\n' | socat - UNIX-CONNECT:$T/pmon > /dev/null 2>&1; sleep 1; continue;;
      D) printf 'set_link n0 off\n' | socat - UNIX-CONNECT:$T/pmon > /dev/null 2>&1; sleep 1; continue;;
    esac
    printf '%s\r' "${c#*|}"; sleep "${c%%|*}"
  done
} | timeout 150 qemu-system-i386 -display none -serial stdio -monitor unix:$T/pmon,server,nowait -parallel none \
      -m 32 -drive file=$T/pref.img,format=raw,if=floppy -boot a -no-reboot \
      -netdev user,id=n0 -device e1000,netdev=n0 > $T/pref.raw 2>&1
tr -d '\r' < $T/pref.raw | iconv -f CP437 -t UTF-8 -c > $T/pref.txt
tramo() { sed -n "/$1/,/$2/{p;/$2/q}" $T/pref.txt > $T/$3.txt; }
tramo 'HSPRED \/AUTO' '=P1=' p1
check p1 "HSPRED: no hay cable enchufado; la red irá por el Wi-Fi."
checkn p1 "Tarjeta Intel"
tramo '=P1=' '=P1B=' p1b
check p1b "HSPRED no está cargado"
tramo '=P1B=' '=P2=' p2
check p2 "Tarjeta Wi-Fi simulado (pruebas)"
tramo '=P2=' '=P3=' p3
check p3 "Tarjeta Intel"
checkn p3 "no hay cable enchufado"
tramo '=P3=' '=P4=' p4
check p4 "HSPRED: RED.INI dice Preferir=WIFI; la red irá por el Wi-Fi, no por el cable."
tramo '=P4=' '=P5=' p5
check p5 "Tarjeta Intel"
tramo '=P5=' '=P6=' p6
checkn p6 "HSPWIFI 0.1"
tramo '=P6=' '=P7=' p7
check p7 "Para usar el Wi-Fi:"
tramo '=P7=' '=P8=' p8
check p8 "Cargando el controlador Wi-Fi (HSPWIFI)..."
check p8 "La red va por el cable (HSPRED). Para usar el Wi-Fi:"
check p8 "Preferir=WIFI en \\HISPADOS\\RED.INI) y reinicie."
check pref "fin-de-la-prueba"

echo "== WIFI carga HSPWIFI solo (HAYWIFI=SI, núcleo de pruebas) =="
# primero a mano (HSPWIFI y MEM /F), se descarga; luego WIFI REDES sin
# cargarlo: lo carga y sigue; los bloques libres tienen que salir iguales
mkimg carga HSPWSIM.BIN "$XMS"
mtype -i $T/carga.img ::HISPADOS/RED.INI > $T/RED.INI 2>/dev/null
printf 'HAYWIFI=SI\r\n' >> $T/RED.INI
mcopy -o -i $T/carga.img $T/RED.INI ::HISPADOS/RED.INI
corre carga 120 '14|HSPWIFI' '2|MEM /F' '1|ECHO =M1=' '3|HSPWIFI /U' '1|ECHO =M2=' \
  '16|WIFI REDES' '2|MEM /F' '1|ECHO =M3=' '10|WIFI CONECTAR HispaPrueba clave de prueba' '3|WIFI' \
  '3|HSPWIFI /U' '14|WIFI /AUTO' '1|ECHO =M4=' '2|ECHO fin-de-la-prueba'
tc() { sed -n "/$1/,/$2/{p;/$2/q}" $T/carga.txt > $T/$3.txt; }
tc '>WIFI REDES' '=M3=' c1
check c1 "Cargando el controlador Wi-Fi (HSPWIFI)..."
check c1 "HSPWIFI 0.1: controlador Wi-Fi de Hispa-486"
check c1 "  2  HispaPrueba"
check carga "Conectado a «HispaPrueba»"
check carga "Wi-Fi: conectado a «HispaPrueba»"
tc '=M1=' '=M2=' c2
check c2 "HSPWIFI descargado."
sed -n '/>HSPWIFI$/,/=M1=/p' $T/carga.txt | sed -n '/Bloques libres/,/Mayor bloque/p' > $T/mf1.txt
sed -n '/>WIFI REDES/,/=M3=/p' $T/carga.txt | sed -n '/Bloques libres/,/Mayor bloque/p' > $T/mf2.txt
if [ -s $T/mf1.txt ] && cmp -s $T/mf1.txt $T/mf2.txt; then ok "la memoria libre, igual que cargándolo a mano (sin hueco de WIFI)"; else ko "MEM /F distinto tras cargarlo WIFI"; diff $T/mf1.txt $T/mf2.txt; fi
tc '>WIFI \/AUTO' '=M4=' c3
check c3 "HSPWIFI 0.1: controlador Wi-Fi de Hispa-486"
check c3 "Conectado a «HispaPrueba»"
checkn c3 "Cargando el controlador Wi-Fi"
check carga "fin-de-la-prueba"

if [ $fail -eq 0 ]; then echo "Wi-Fi: todas las pruebas pasan"; else echo "Wi-Fi: HAY FALLOS"; fi
exit $fail
