#!/bin/bash
# Hispa-486 - imagen de prueba de la batería por ACPI para el equipo de verdad
# (docs/ACPI.md). A partir de build/hispa486-usb.img (make usb486): un menú de
# arranque con la prueba de la batería (por omisión), PRUBAT.BAT, PASOSBAT.BAT
# y COMPLETO.BAT, y el registro en C:\BATERIA.LOG. Deja en la carpeta de
# destino el .zip con la imagen, el LEEME.TXT y los archivos sueltos.
#   tools/mkprueba_bateria.sh [carpeta de destino]   (por omisión, build/prueba-bateria)
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-bateria}
[ -f $B/hispa486-usb.img ] || { echo "falta $B/hispa486-usb.img (make usb486)"; exit 1; }
mkdir -p "$D" "$D/sueltos" $B/prueba-bateria-tmp
D=$(realpath "$D")
T=$B/prueba-bateria-tmp
IMG=$T/hispa486-usb.img
cp --sparse=always $B/hispa486-usb.img $IMG
P="$IMG@@$((2048 * 512))"
FECHA=$(date +%d/%m/%Y); HORA=$(date +%H:%M)
VERSION=$(git describe --always --dirty 2>/dev/null || echo "sin git")
export FECHA HORA VERSION
txt() { python3 -c "
import os, sys
t = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
for k in ('FECHA', 'HORA', 'VERSION'): t = t.replace('@' + k + '@', os.environ[k])
open(sys.argv[2], 'wb').write(t.encode('cp437'))" "$1" "$2"; }
# CONFIG.SYS: una opción nueva en el menú, la primera y por omisión
python3 - usb/CONFIG.SYS $T/CONFIG.SYS <<'PY'
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
a = "[menu]\n"
assert a in t
t = t.replace(a, a + "MENUITEM=BATERIA,PRUEBA DE LA BATERIA (lo guarda todo en BATERIA.LOG)\n", 1)
t = re.sub(r"MENUDEFAULT=.*\n", "MENUDEFAULT=BATERIA,20\n", t, 1)
t += "\n[bateria]\nREM ---- Prueba de la batería: lo justo (memoria extendida y nada más) ----\nDOS=HIGH\n"
open(sys.argv[2], 'wb').write(t.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp437'))
PY
# AUTOEXEC.BAT: el de la imagen de Hispa-486 (ya lleva HSPACPI /AUTO en el
# arranque normal), con la opción nueva
mtype -i "$P" ::AUTOEXEC.BAT > $T/AUTOEXEC.ORI
python3 - $T/AUTOEXEC.ORI $T/AUTOEXEC.BAT <<'PY'
import sys
t = open(sys.argv[1], 'rb').read().decode('cp437').replace('\r\n', '\n')
a = 'IF "%CONFIG%"=="JUEGOS" GOTO JUEGOS\n'
assert a in t
t = t.replace(a, 'IF "%CONFIG%"=="BATERIA" GOTO BATERIA\n' + a, 1)
a = ":COMUN\n"
assert a in t
t = t.replace(a, ":BATERIA\nTECLADO SP\nCALL C:\\PRUBAT.BAT\nGOTO FIN\n\n" + a, 1)
open(sys.argv[2], 'wb').write(t.replace('\n', '\r\n').encode('cp437'))
PY
for f in PRUBAT.BAT PASOSBAT.BAT COMPLETO.BAT; do txt usb/prueba-bateria/$f $T/$f; done
txt usb/prueba-bateria/LEEME.TXT $T/LEEME-BA.TXT
mcopy -o -i "$P" $T/CONFIG.SYS $T/AUTOEXEC.BAT $T/PRUBAT.BAT $T/PASOSBAT.BAT $T/COMPLETO.BAT $T/LEEME-BA.TXT ::
# (COMPLETO se escribe sin ruta: que esté también en el PATH)
mcopy -o -i "$P" $T/COMPLETO.BAT $T/PASOSBAT.BAT ::HISPADOS/
[ ! -e "$D/hispados-usb-bateria.zip" ] || mv -f "$D/hispados-usb-bateria.zip" "$D/hispados-usb-bateria.zip.ant"
(cd $T && zip -q -9 "$D/hispados-usb-bateria.zip" hispa486-usb.img)
txt usb/prueba-bateria/LEEME.TXT "$D/LEEME.TXT"
cp $B/HSPACPI.COM $B/HSPACPI.BIN $T/PRUBAT.BAT $T/PASOSBAT.BAT $T/COMPLETO.BAT "$D/sueltos/"
ls -l "$D" "$D/sueltos"
echo "imagen de prueba: $IMG (y $D/hispados-usb-bateria.zip)"
