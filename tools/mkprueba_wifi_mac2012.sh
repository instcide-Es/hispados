#!/bin/bash
# Hispa-486 - imagen de prueba del Wi-Fi para el MacBook Air 2012 (A1466,
# arrancado por el CSM de Apple; docs/WIFI.md). A partir de
# build/hispa486-usb.img (make usb486): una opción más en el menú de
# arranque, la primera y por omisión, que carga lo mismo que «Normal»
# (HSPUSB lleva el teclado del Mac) y lanza PRUWIFI.BAT en vez del
# AUTOEXEC.BAT de siempre. Deja en la carpeta de destino el .zip con la
# imagen y el LEEME.TXT.
#   tools/mkprueba_wifi_mac2012.sh [carpeta de destino [nombre]]
#   (nombre: el del .zip y la imagen, sin extensión; por omisión
#   hispa486-usb-wifi-mac2012)
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-wifi-mac2012}
N=${2:-hispa486-usb-wifi-mac2012}
[ -f $B/hispa486-usb.img ] || { echo "falta $B/hispa486-usb.img (make usb486)"; exit 1; }
T=$B/prueba-wifi-mac2012-tmp
rm -rf $T; mkdir -p "$D" $T
IMG=$T/$N.img
cp $B/hispa486-usb.img $IMG
P="$IMG@@$((2048 * 512))"
export MTOOLS_SKIP_CHECK=1
FECHA=$(date +%d/%m/%Y); HORA=$(date +%H:%M)
VERSION=$(git describe --always --dirty 2>/dev/null || echo "sin git")
NOMBRE=$N
export FECHA HORA VERSION NOMBRE
txt() { python3 -c "
import os, sys
t = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
for k in ('FECHA', 'HORA', 'VERSION', 'NOMBRE'): t = t.replace('@' + k + '@', os.environ[k])
open(sys.argv[2], 'wb').write(t.encode('cp437'))" "$1" "$2"; }
mtype -i "$P" ::CONFIG.SYS > $T/CONFIG.0
mtype -i "$P" ::AUTOEXEC.BAT > $T/AUTOEXEC.0
python3 - $T/CONFIG.0 $T/CONFIG.SYS $T/AUTOEXEC.0 $T/AUTOEXEC.BAT <<'PY'
import re, sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r\n', '\n')
a = "[menu]\n"
assert a in t
t = t.replace(a, a + "MENUITEM=WIFIMAC,PRUEBA DEL WI-FI (Mac 2012): lo apunta todo en WIFI.LOG\n", 1)
t, n = re.subn(r"MENUDEFAULT=.*\n", "MENUDEFAULT=WIFIMAC,20\n", t, 1)
assert n == 1
t += ("\n[WIFIMAC]\nREM ---- Prueba del Wi-Fi en el MacBook Air 2012: lo de «Normal» ----\nINCLUDE=NORMAL\n")
open(sys.argv[2], 'wb').write(t.replace('\n', '\r\n').encode('cp437'))
t = open(sys.argv[3], 'rb').read().decode('cp437').replace('\r\n', '\n')
a = 'IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS\n'
assert a in t
t = t.replace(a, 'IF "%CONFIG%"=="WIFIMAC" GOTO WIFIMAC\n' + a, 1)
a = ":COMUN\n"
assert a in t
t = t.replace(a, ":WIFIMAC\nTECLADO SP\nCALL C:\\PRUWIFI.BAT\nGOTO FIN\n\n" + a, 1)
open(sys.argv[4], 'wb').write(t.replace('\n', '\r\n').encode('cp437'))
PY
for f in PRUWIFI.BAT PASO2.BAT; do txt usb/prueba-wifi-mac2012/$f $T/$f; done
txt usb/prueba-wifi-mac2012/LEEME.TXT $T/LEEME-WI.TXT
mcopy -o -i "$P" $T/CONFIG.SYS $T/AUTOEXEC.BAT $T/PRUWIFI.BAT $T/PASO2.BAT $T/LEEME-WI.TXT ::
mcopy -o -i "$P" $T/PASO2.BAT ::HISPADOS/PASO2.BAT
# [LETRAS] los guiones y las instrucciones de esta imagen dicen «C:\»: las
# letras, como siempre (la memoria es la C: aunque haya disco duro)
tools/letras_clasico.sh "$IMG"
Z="$(realpath "$D")/$N.zip"
rm -f "$Z"
(cd $T && zip -q -9 "$Z" $N.img)
txt usb/prueba-wifi-mac2012/LEEME.TXT "$D/LEEME.TXT"
ls -l "$D"
echo "imagen de prueba: $IMG (y $Z)"
