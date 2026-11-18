#!/bin/bash
# Hispa-DOS - imagen de prueba de HispaBIOS para un equipo de verdad que se
# queda parado al arrancar por UEFI (docs/HISPABIOS.md). A partir de
# build/hispados-usb.img (make usb): el mismo BOOTX64.EFI ensamblado con
# -DREGISTRO_PASOS, que guarda \HISPABIOS.LOG en la memoria USB en cada paso
# aunque todo vaya bien (la imagen normal solo lo guarda si algo falla). Deja
# en la carpeta de destino el .zip con la imagen, el LEEME.TXT y el .EFI suelto.
#   tools/mkprueba_hispabios.sh [carpeta de destino]   (por omisión, build/prueba-hispabios)
set -eu
cd "$(dirname "$0")/.."
B=build
D=${1:-$B/prueba-hispabios}
[ -f $B/hispados-usb.img ] || { echo "falta $B/hispados-usb.img (make usb)"; exit 1; }
mkdir -p "$D" "$D/sueltos" $B/prueba-hispabios-tmp
D=$(realpath "$D")
T=$B/prueba-hispabios-tmp
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
nasm -f bin -I hispabios/ -I $B/ -DREGISTRO_PASOS -o $T/BOOTX64.EFI hispabios/efi64.asm
mcopy -o -i "$P" $T/BOOTX64.EFI ::EFI/BOOT/BOOTX64.EFI
txt usb/prueba-hispabios/LEEME.TXT $T/LEEME-HB.TXT
mcopy -o -i "$P" $T/LEEME-HB.TXT ::
[ ! -e "$D/hispados-usb-hispabios.zip" ] || mv -f "$D/hispados-usb-hispabios.zip" "$D/hispados-usb-hispabios.zip.ant"
(cd $T && zip -q -9 "$D/hispados-usb-hispabios.zip" hispados-usb.img)
txt usb/prueba-hispabios/LEEME.TXT "$D/LEEME.TXT"
cp $T/BOOTX64.EFI "$D/sueltos/"
ls -l "$D" "$D/sueltos"
echo "imagen de prueba: $IMG (y $D/hispados-usb-hispabios.zip)"
