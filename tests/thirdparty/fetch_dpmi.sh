#!/bin/bash
# Descarga programas DPMI libres (hechos con DJGPP, del repositorio de FreeDOS)
# para probar HSPDPMI con clientes de terceros. No se incluyen en el
# repositorio: se guardan en tests/thirdparty/dl/ (ignorado).
# Downloads free DJGPP-built (DPMI) programs used only as a test for HSPDPMI.
set -eu
BASE=https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3
DIR=$(cd "$(dirname "$0")" && pwd)/dl
PKGS="unix/gnused unix/gnubc devel/nasm devel/lua archiver/gzip unix/md5sum unix/less util/cwsdpmi"
mkdir -p "$DIR"
for q in $PKGS; do
  p=${q#*/}
  [ -f "$DIR/dpmi-$p/.ok" ] && continue
  mkdir -p "$DIR/dpmi-$p"
  curl -sfL --max-time 180 -o "$DIR/dpmi-$p/$p.zip" "$BASE/$q.zip" || { echo "  (no se pudo bajar $p)"; continue; }
  (cd "$DIR/dpmi-$p" && unzip -qo "$p.zip" && touch .ok)
  echo "  $p"
done
