#!/bin/bash
# Arranca la imagen con un CONFIG.SYS dado y teclea órdenes por el puerto
# serie; deja la salida (UTF-8) en la salida estándar.
# uso: emm_run.sh "líneas de CONFIG.SYS (\n)" tiempo "orden1" "orden2" ...
#   variables: EXTRA (ficheros a copiar a ::BIN), QEMU_EXTRA, MEM (MB), PAUSA
# Boots the image with a given CONFIG.SYS and types commands on the serial port.
set -u
cd "$(dirname "$0")/../.."
IMG=${IMG:-build/hispados.img}
CFG=$1; TMO=$2; shift 2
T=$(mktemp -d)
cp "$IMG" $T/f.img
printf "$CFG" | sed 's/$/\r/' > $T/CONFIG.SYS
mcopy -o -i $T/f.img $T/CONFIG.SYS ::CONFIG.SYS
for f in ${EXTRA:-}; do mcopy -o -i $T/f.img $f ::BIN/; done
{
  sleep ${ARRANQUE:-6}
  for c in "$@"; do printf '%s\r' "$c"; sleep ${PAUSA:-2}; done
  sleep 1
} | timeout $TMO qemu-system-i386 -display none -serial stdio -monitor none -parallel none \
      -m ${MEM:-32} -drive file=$T/f.img,format=raw,if=floppy,index=0 -boot a -no-reboot \
      ${QEMU_EXTRA:-} > $T/raw 2>&1
tr -d '\r' < $T/raw | iconv -f CP437 -t UTF-8
rm -rf $T
