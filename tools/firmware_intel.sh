#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# tools/firmware_intel.sh CARPETA [archivo.ucode...]
#
# Baja de linux-firmware (kernel.org) el firmware de las Wi-Fi de Intel que
# lleva el Wi-Fi de Hispa-Pentium (WIFI.BIN, wifi/nucleo/radio_iwl.c) y su
# licencia, y lo deja en CARPETA/FIRMWARE/, listo para copiarlo a la raíz de
# la memoria USB (X:\FIRMWARE\) o a C:\HISPA\FIRMWARE\ en un sistema
# instalado.
#
# El firmware de Intel NO es software libre: Intel deja repartirlo en binario,
# sin cambiarlo y con su licencia (LICENCE.iwlwifi_firmware), pero no va en el
# repositorio de Hispa-DOS ni en sus imágenes: se baja aparte, con esto (o lo
# baja quien lo use). docs/PENTIUM.md, «El Wi-Fi de Intel».
#
# Sin nombres, los de las tarjetas que se llevan (la familia 7000: 7265D y
# 3165 con el 7265D-29; la 7265 sin «D» con el 7265-17; la 3168 con el
# 3168-29). Comprueba cada archivo con build/iwl_analiza si está hecho.
set -eu
DEST=${1:?uso: firmware_intel.sh CARPETA [archivo.ucode...]}
shift
LISTA=${*:-iwlwifi-7265D-29.ucode iwlwifi-7265-17.ucode iwlwifi-3168-29.ucode}
BASE=https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain
RAIZ=$(dirname "$(readlink -f "$0")")/..
mkdir -p "$DEST/FIRMWARE"
curl -s -f -o "$DEST/FIRMWARE/LICENCE.iwlwifi_firmware" "$BASE/LICENCE.iwlwifi_firmware"
for f in $LISTA; do
  if curl -s -f -o "$DEST/FIRMWARE/$f" "$BASE/intel/iwlwifi/$f"; then
    echo "$f: $(stat -c %s "$DEST/FIRMWARE/$f") octetos, SHA-256 $(sha256sum "$DEST/FIRMWARE/$f" | cut -c1-16)..."
    if [ -x "$RAIZ/build/iwl_analiza" ]; then "$RAIZ/build/iwl_analiza" "$DEST/FIRMWARE/$f" | sed -n '2,3p;$p' | sed 's/^/    /'; fi
  else
    echo "$f: no está en linux-firmware (¿otro nombre?)" >&2
  fi
done
echo "Hecho: copie la carpeta $DEST/FIRMWARE a la raíz de la memoria USB (o a C:\\HISPA\\FIRMWARE)."
