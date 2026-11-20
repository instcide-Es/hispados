#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# Descarga las pruebas «de un solo paso» de los dos procesadores de la consola
# de 16 bits, para Hispa-Snes: SingleStepTests/65816 y SingleStepTests/spc700
# (Thomas Harte y otros, licencia MIT). Cada archivo trae miles de casos de
# una instrucción (el estado de antes, el de después y los ciclos), sacados
# de procesadores de verdad. Son muy grandes (unos 6 MB por instrucción): se
# baja solo el principio de cada uno (los primeros casos), con «Range», y
# tests/hispasnes/sst.py los pasa a un formato binario. Van a
# tests/thirdparty/dl/snes/ (ignorado). Si no hay red, esas pruebas se saltan.
# Downloads the beginning of each SingleStepTests file (MIT) for Hispa-Snes.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)/dl/snes
TROZO=${TROZO:-60000}
mkdir -p "$DIR/65816" "$DIR/spc700"
baja() {                                # baja URL ARCHIVO
  [ -s "$2" ] && return 0
  curl -sfL --max-time 60 -r 0-$TROZO -o "$2.tmp" "$1" && mv "$2.tmp" "$2"
  rm -f "$2.tmp"
}
B=https://raw.githubusercontent.com/SingleStepTests
for i in $(seq 0 255); do
  h=$(printf '%02x' $i)
  baja "$B/65816/main/v1/$h.e.json" "$DIR/65816/$h.e.json" || exit 0
  baja "$B/65816/main/v1/$h.n.json" "$DIR/65816/$h.n.json" || exit 0
  baja "$B/spc700/main/v1/$h.json" "$DIR/spc700/$h.json" || exit 0
done
[ -s "$DIR/LICENCIA-65816" ] || curl -sfL --max-time 60 -o "$DIR/LICENCIA-65816" "$B/65816/main/LICENCE"
[ -s "$DIR/LICENCIA-spc700" ] || curl -sfL --max-time 60 -o "$DIR/LICENCIA-spc700" "$B/spc700/main/LICENSE"
exit 0
