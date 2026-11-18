#!/bin/bash
# Hispa-DOS - imagen de prueba del sonido del MacBook Air 2017 arrancado con
# HispaBIOS (la DMA cerrada por el firmware: lib/vtd.inc, docs/AUDICION.md).
# Es build/hispados-usb.img (make usb) tal cual, con su LEEME dentro; deja en
# la carpeta de destino el .zip con la imagen, el LEEME.TXT y, sueltos,
# HSPSND.COM y AUDICION.COM (para copiarlos a mano a una memoria ya hecha).
#   tools/mkprueba_sonido_mac.sh [carpeta de destino]   (por omisión, build/prueba-sonido-mac2017)
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-sonido-mac2017}
[ -f $B/hispados-usb.img ] || { echo "falta $B/hispados-usb.img (make usb)"; exit 1; }
mkdir -p "$D" "$D/sueltos" $B/prueba-sonido-mac-tmp
D=$(realpath "$D")
T=$B/prueba-sonido-mac-tmp
IMG=$T/hispados-usb.img
cp --sparse=always $B/hispados-usb.img $IMG
P="$IMG@@$((2048 * 512))"
export MTOOLS_SKIP_CHECK=1
FECHA=$(date +%d/%m/%Y); HORA=$(date +%H:%M)
VERSION=$(git describe --always --dirty 2>/dev/null || echo "sin git")
export FECHA HORA VERSION
txt() { python3 -c "
import os, sys
t = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
for k in ('FECHA', 'HORA', 'VERSION'): t = t.replace('@' + k + '@', os.environ[k])
open(sys.argv[2], 'wb').write(t.encode('cp437'))" "$1" "$2"; }
# los dos programas de la imagen tienen que ser los de este árbol
for f in HSPSND.COM AUDICION.COM; do
  mcopy -n -i "$P" ::HISPADOS/$f $T/$f.img
  cmp -s $T/$f.img $B/$f || { echo "$f de la imagen no es el de $B (make usb)"; exit 1; }
  cp $B/$f "$D/sueltos/"
done
txt usb/prueba-sonido-mac2017/LEEME.TXT $T/LEEME-SM.TXT
mcopy -o -i "$P" $T/LEEME-SM.TXT ::
[ ! -e "$D/hispados-usb-sonido-mac2017.zip" ] || mv -f "$D/hispados-usb-sonido-mac2017.zip" "$D/hispados-usb-sonido-mac2017.zip.ant"
(cd $T && zip -q -9 "$D/hispados-usb-sonido-mac2017.zip" hispados-usb.img)
txt usb/prueba-sonido-mac2017/LEEME.TXT "$D/LEEME.TXT"
ls -l "$D" "$D/sueltos"
echo "imagen de prueba: $IMG (y $D/hispados-usb-sonido-mac2017.zip)"
