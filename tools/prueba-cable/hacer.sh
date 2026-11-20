#!/bin/bash
# Memoria USB de prueba de la red por cable del HP Mini 5103 (Marvell Yukon
# 88E8059): la imagen USB de siempre (build/hispados-usb.img, de «make usb»)
# con CABLE.BAT en la raíz y un AUTOEXEC.BAT que lo llama al acabar. Deja en
# la carpeta de destino hispados-usb-cable.zip (con la imagen), LEEME.TXT y
# HSPRED.COM, PING.COM y CABLE.BAT sueltos (para copiarlos a mano encima de
# una memoria que ya esté hecha).
#   tools/prueba-cable/hacer.sh [carpeta de destino]
# Builds the HP Mini wired-network test USB image.
set -e
cd "$(dirname "$0")/../.."
DEST=${1:-/root/FTP/HispaDOS/prueba-cable-hpmini}
B=build
[ -f $B/hispados-usb.img ] || { echo "falta $B/hispados-usb.img (make usb)"; exit 1; }
export MTOOLS_SKIP_CHECK=1 MTOOLS_NO_VFAT=1
T=$(mktemp -d)
trap 'rm -rf $T' EXIT
cp $B/hispados-usb.img $T/hispados-usb-cable.img
IMG="$T/hispados-usb-cable.img@@$((2048 * 512))"
python3 tools/cp437.py tools/prueba-cable/CABLE.BAT $T/CABLE.BAT
python3 tools/cp437.py tools/prueba-cable/LEEME.TXT $T/LEEME.TXT
sed -i 's/\r$//; s/$/\r/' $T/CABLE.BAT $T/LEEME.TXT
# el AUTOEXEC.BAT de la imagen, con la llamada a la prueba al final
mcopy -n -i "$IMG" ::AUTOEXEC.BAT $T/AUTOEXEC.BAT
printf 'IF EXIST C:\\CABLE.BAT CALL C:\\CABLE.BAT\r\n' >> $T/AUTOEXEC.BAT
mcopy -o -i "$IMG" $T/AUTOEXEC.BAT ::AUTOEXEC.BAT
mcopy -o -i "$IMG" $T/CABLE.BAT ::CABLE.BAT
mcopy -o -i "$IMG" $T/LEEME.TXT ::CABLE.LEE
# (por si la imagen es de antes de este HSPRED: los de ahora)
mcopy -o -i "$IMG" $B/HSPRED.COM ::HISPADOS/HSPRED.COM
mcopy -o -i "$IMG" $B/PING.COM ::HISPADOS/PING.COM
mkdir -p "$DEST"
rm -f "$DEST/hispados-usb-cable.zip"
# [LETRAS] los guiones y las instrucciones de esta imagen dicen «C:\»: las
# letras, como siempre (la memoria es la C: aunque haya disco duro)
tools/letras_clasico.sh "$T/hispados-usb-cable.img"
(cd $T && zip -q -9 "$DEST/hispados-usb-cable.zip" hispados-usb-cable.img)
# el LEEME del FTP se lee en Windows: UTF-8 con marca y finales de línea de DOS
{ printf '\xef\xbb\xbf'; sed 's/\r$//; s/$/\r/' tools/prueba-cable/LEEME.TXT; } > "$DEST/LEEME.TXT"
cp $T/CABLE.BAT $B/HSPRED.COM $B/PING.COM "$DEST/"
ls -l "$DEST"
