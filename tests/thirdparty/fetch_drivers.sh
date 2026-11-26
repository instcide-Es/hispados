#!/bin/bash
# Descarga controladores de DOS libres (GPL) para probar que Hispa-DOS carga
# los de otros (docs/DRIVERS-MICROSOFT.md). No van en el repositorio: se
# guardan en tests/thirdparty/dl/ (ignorado).
#   - SRDISK 2.09 (FreeDOS 1.3, GPL-2.0): un disco en memoria XMS que cambia
#     de tamaño, con sus .SYS comprimidos con UPX;
#   - los «packet drivers» de Crynwr (pktd11, GPL; crynwr.com ya no contesta:
#     la copia del Internet Archive).
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
set -eu
DIR=$(cd "$(dirname "$0")" && pwd)/dl
mkdir -p "$DIR"
if [ ! -f "$DIR/srdisk/.ok" ]; then
  mkdir -p "$DIR/srdisk"
  curl -sfL --max-time 120 -o "$DIR/srdisk/srdisk.zip" https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/drivers/srdisk.zip
  (cd "$DIR/srdisk" && unzip -qo srdisk.zip && touch .ok)
  echo "  srdisk"
fi
if [ ! -f "$DIR/pktd11/.ok" ]; then
  mkdir -p "$DIR/pktd11"
  curl -sfL --max-time 120 -o "$DIR/pktd11/pktd11.zip" https://web.archive.org/web/2015/http://www.crynwr.com/drivers/pktd11.zip
  (cd "$DIR/pktd11" && unzip -qo pktd11.zip && touch .ok)
  echo "  pktd11"
fi
