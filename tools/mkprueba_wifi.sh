#!/bin/bash
# Hispa-486 - imagen de prueba del Wi-Fi para el equipo de verdad (docs/WIFI.md).
# A partir de build/hispados-usb.img (make usb): un menú de arranque con la
# prueba del Wi-Fi (por omisión), PRUWIFI.BAT y PASO2.BAT, y el registro en
# C:\WIFI.LOG. Deja en la carpeta de destino el .zip con la imagen, el
# LEEME.TXT y los archivos sueltos.
#   tools/mkprueba_wifi.sh [carpeta de destino]   (por omisión, build/prueba-wifi)
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-wifi}
[ -f $B/hispados-usb.img ] || { echo "falta $B/hispados-usb.img (make usb)"; exit 1; }
mkdir -p "$D" $B/prueba-wifi-tmp
T=$B/prueba-wifi-tmp
IMG=$T/hispados-usb.img
cp $B/hispados-usb.img $IMG
P="$IMG@@$((2048 * 512))"
# la fecha, la hora y la versión de la imagen, en el LEEME (@FECHA@ ...)
FECHA=$(date +%d/%m/%Y); HORA=$(date +%H:%M)
VERSION=$(git describe --always --dirty 2>/dev/null || echo "sin git")
export FECHA HORA VERSION
txt() { python3 -c "
import os, sys
t = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
for k in ('FECHA', 'HORA', 'VERSION'): t = t.replace('@' + k + '@', os.environ[k])
open(sys.argv[2], 'wb').write(t.encode('cp437'))" "$1" "$2"; }
# CONFIG.SYS: dos opciones nuevas en el menú, la del Wi-Fi por omisión
python3 - usb/CONFIG.SYS $T/CONFIG.SYS <<'PY'
import sys
t = open(sys.argv[1], encoding='utf-8').read()
a = "[menu]\n"
assert a in t
t = t.replace(a, a + "MENUITEM=WIFI,PRUEBA DEL WI-FI (lo guarda todo en WIFI.LOG)\n"
              "MENUITEM=WIFIUSB,Prueba del Wi-Fi con consola remota por el adaptador USB de red\n", 1)
import re
t = re.sub(r"MENUDEFAULT=.*\n", "MENUDEFAULT=WIFI,20\n", t, 1)
t += ("\n[wifi]\nREM ---- Prueba del Wi-Fi: lo justo (memoria extendida y nada más) ----\nDOS=HIGH\n"
      "\n[wifiusb]\nREM ---- Prueba del Wi-Fi con la red por el adaptador USB (consola remota) ----\n"
      "DOS=HIGH\nDEVICE=C:\\HISPADOS\\HSPUSB.SYS /F /RED /INFO\n")
open(sys.argv[2], 'wb').write(t.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437'))
PY
python3 - usb/AUTOEXEC.BAT $T/AUTOEXEC.BAT <<'PY'
import sys
t = open(sys.argv[1], encoding='utf-8').read()
a = 'IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS\n'
assert a in t
t = t.replace(a, 'IF "%CONFIG%"=="WIFI" GOTO WIFI\nIF "%CONFIG%"=="WIFIUSB" GOTO WIFIUSB\n' + a, 1)
a = ":COMUN\n"
assert a in t
t = t.replace(a, ":WIFI\nTECLADO SP\nCALL C:\\PRUWIFI.BAT\nGOTO FIN\n\n:WIFIUSB\nTECLADO SP\nCALL C:\\WIFIUSB.BAT\nGOTO FIN\n\n" + a, 1)
open(sys.argv[2], 'wb').write(t.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437'))
PY
for f in PRUWIFI.BAT PASO2.BAT WIFIUSB.BAT; do txt usb/prueba-wifi/$f $T/$f; done
txt usb/prueba-wifi/LEEME.TXT $T/LEEME-WI.TXT
mcopy -o -i "$P" $T/CONFIG.SYS $T/AUTOEXEC.BAT $T/PRUWIFI.BAT $T/PASO2.BAT $T/WIFIUSB.BAT $T/LEEME-WI.TXT ::
# (PASO2 se escribe sin ruta: que esté también en el PATH)
mcopy -o -i "$P" $T/PASO2.BAT ::HISPADOS/PASO2.BAT
rm -f "$D/hispados-usb-wifi.zip"
(cd $T && zip -q -9 "$OLDPWD/$D/hispados-usb-wifi.zip" hispados-usb.img) 2>/dev/null || \
  (cd $T && zip -q -9 "$(cd "$OLDPWD" && realpath "$D")/hispados-usb-wifi.zip" hispados-usb.img)
txt usb/prueba-wifi/LEEME.TXT "$D/LEEME.TXT"
# los archivos sueltos, por si se prefiere copiarlos a una memoria que ya arranca
mkdir -p "$D/sueltos"
cp $B/HSPWIFI.COM $B/HSPWIFI.BIN $B/WIFI.COM $B/BCM43XX.FW $B/BCM43XXH.FW $B/BCM43XX.TXT "$D/sueltos/"
cp $T/PRUWIFI.BAT $T/PASO2.BAT "$D/sueltos/"
ls -l "$D" "$D/sueltos"
echo "imagen de prueba: $IMG (y $D/hispados-usb-wifi.zip)"
