#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/usb.sh IMAGEN
# Hace la imagen de una memoria USB (64 MB, FAT16, arranque BIOS con GRUB) con
# Hispa-Pentium, para probarlo en un PC de verdad: se graba tal cual en la
# memoria (dd, Rufus en modo «imagen DD», balenaEtcher...) y se arranca de ella.
# GRUB carga el núcleo por multiboot y, como módulos, los programas (el
# intérprete de órdenes, el Bloc de notas, la Consola...) y un LEEME.TXT: el
# núcleo no sabe leer todavía una memoria USB, así que esos módulos hacen de
# unidad C: en memoria, de solo lectura (usuario.cas). Las entradas del menú:
#   - el escritorio a 800x600 (el de siempre);
#   - el escritorio «a la medida de la pantalla»: GRUB pone el modo que el
#     panel dice que prefiere (lo lee de su EDID): «set gfxpayload=auto»;
#   - el escritorio a 1024x600, a 1366x768 y a 1024x768 (copias del núcleo que
#     le piden a GRUB ese modo en su cabecera: nucleo32/modo_grafico.py; si la
#     BIOS no lo da por VBE, GRUB pone el preferido o el que más se parezca);
#   - los modos de vídeo que da la BIOS de ese equipo (la orden «videoinfo»
#     de GRUB), para saber cuáles se pueden pedir;
#   - el intérprete de órdenes y la consola del núcleo, en modo texto.
# Todas llevan «sindisco»: el núcleo ni mira los discos del PC. (Sin eso
# tampoco escribiría en ellos: hace falta «escritura» en la línea del núcleo.)
# Necesita ser root (losetup, mount), python3, nasm, parted, dosfstools y
# grub-pc-bin (grub-install --target=i386-pc).
set -eu
export LC_ALL=C.UTF-8
AQUI=$(dirname "$(readlink -f "$0")")
OUT=${1:?uso: usb.sh IMAGEN}
W=$(mktemp -d /dev/shm/pgraf-usb.XXXX)
L=
trap '[ -n "$L" ] && { umount $W/m 2>/dev/null; losetup -d $L 2>/dev/null; }; rm -rf $W' EXIT
mkdir $W/f $W/m
python3 -I $AQUI/../ccastel.py $AQUI/semilla.cas -o $W/semilla.asm > /dev/null
nasm -f bin $W/semilla.asm -o $W/f/SEMILLA.BIN
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT8.BIN 800 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT10.BIN 1024 768 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT6.BIN 1024 600 32 > /dev/null
python3 -I $AQUI/modo_grafico.py $W/f/SEMILLA.BIN $W/f/ESCRIT13.BIN 1366 768 32 > /dev/null
MODULOS=
for p in uordenes:ORDENES ubloc:BLOC uconsola:CONSOLA uequipo:EQUIPO uhola:HOLA ueco:ECO uprueba:PRUEBA ubucle:BUCLE; do
  f=${p%%:*}; N=${p##*:}
  python3 -I $AQUI/../ccastel.py $AQUI/$f.cas -o $W/$f.asm > /dev/null
  nasm -f bin $W/$f.asm -o $W/f/$N.BIN
  MODULOS="$MODULOS  module /$N.BIN $N.BIN
"
done
printf 'Hispa-Pentium: un n\xa3cleo de 32 bits escrito en Castel.\r\nEste archivo ha venido con el cargador: est\xa0 en la memoria, no en un disco.\r\n' > $W/f/LEEME.TXT
MODULOS="$MODULOS  module /LEEME.TXT LEEME.TXT
"
IMG=$W/usb.img
truncate -s 64M $IMG
parted -s $IMG mklabel msdos mkpart primary fat16 1MiB 100% set 1 boot on
L=$(losetup -fP --show $IMG)
mkfs.fat -F 16 -n HISPAPENT ${L}p1 > /dev/null
mount ${L}p1 $W/m
cp $W/f/* $W/m/
mkdir -p $W/m/boot/grub
{ cat <<'G'
set timeout=8
set default=0
insmod all_video
G
  # el rótulo, el núcleo, lo que se le dice y, si hace falta, el modo que
  # se le pide a GRUB (después de «multiboot», que pone el de la cabecera)
  entrada() {
    printf 'menuentry "%s" {\n  multiboot /%s %s\n%s' "$1" "$2" "$3" "$MODULOS"
    [ -n "${4:-}" ] && printf '  set gfxpayload=%s\n' "$4"
    printf '  boot\n}\n'
  }
  entrada "Hispa-Pentium 0.5: el escritorio (800 x 600)" ESCRIT8.BIN "escritorio sindisco"
  entrada "Hispa-Pentium 0.5: el escritorio a la medida de la pantalla" ESCRIT8.BIN "escritorio sindisco" "auto"
  entrada "Hispa-Pentium 0.5: el escritorio (1024 x 600)" ESCRIT6.BIN "escritorio sindisco"
  entrada "Hispa-Pentium 0.5: el escritorio (1366 x 768)" ESCRIT13.BIN "escritorio sindisco"
  entrada "Hispa-Pentium 0.5: el escritorio (1024 x 768)" ESCRIT10.BIN "escritorio sindisco"
  cat <<'G'
menuentry "Ver los modos de video que da la BIOS de este equipo" {
  insmod videoinfo
  set pager=1
  videoinfo
  echo
  echo "Los modos de 32, 24 o 16 bits (Direct color) valen. Pulse Intro para volver."
  read nada
}
G
  entrada "Hispa-Pentium 0.5: el interprete de ordenes (modo texto)" SEMILLA.BIN "ordenes sindisco"
  entrada "Hispa-Pentium 0.5: la consola del nucleo (modo texto)" SEMILLA.BIN "sindisco"
} > $W/m/boot/grub/grub.cfg
grub-install --target=i386-pc --boot-directory=$W/m/boot --modules="part_msdos fat multiboot all_video videoinfo read echo" $L > $W/grub.txt 2>&1 || { cat $W/grub.txt; exit 1; }
sync
umount $W/m
losetup -d $L
L=
cp $IMG "$OUT"
echo "usb.sh: $OUT, $(($(stat -c %s "$OUT") / 1048576)) MB"
