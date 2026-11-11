#!/bin/bash
# Descarga programas libres de FreeDOS para probar la compatibilidad de HispaDOS.
# No se incluyen en el repositorio: se guardan en tests/thirdparty/dl/ (ignorado).
# Downloads free FreeDOS programs used only as a compatibility test suite.
set -eu
BASE=https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/repositories/1.3/base
DIR=$(cd "$(dirname "$0")" && pwd)/dl
PKGS="find more sort attrib tree choice fc mem deltree xcopy label edit edlin debug nansi ctmouse himemx"
mkdir -p "$DIR"
for p in $PKGS; do
  [ -f "$DIR/$p/.ok" ] && continue
  mkdir -p "$DIR/$p"
  curl -sfL --max-time 120 -o "$DIR/$p/$p.zip" "$BASE/$p.zip"
  (cd "$DIR/$p" && unzip -qo "$p.zip" && touch .ok)
  echo "  $p"
done
