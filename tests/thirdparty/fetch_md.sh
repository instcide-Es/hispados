#!/bin/bash
# Descarga las pruebas de una instrucción del 68000 para Hispa-Drive: las de
# SingleStepTests/680x0 (unas 8.000 por cada instrucción, más de 800.000, con
# el estado de antes y el de después; JSON comprimido, unos 200 MB). No van
# en el proyecto: se guardan en tests/thirdparty/dl/md/ (ignorado). Si no
# hay red, esas pruebas se saltan.
# Downloads the SingleStepTests 68000 single-instruction tests.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)/dl/md
mkdir -p "$DIR"
BASE=https://github.com/SingleStepTests/680x0/raw/main/68000/v1
LISTA=$(curl -sfL --max-time 60 -A "Mozilla/5.0" https://api.github.com/repos/SingleStepTests/680x0/contents/68000/v1 \
  | python3 -c "import json, sys; [print(x['name']) for x in json.load(sys.stdin) if x['name'].endswith('.json.gz')]" 2> /dev/null)
[ -n "$LISTA" ] || exit 0
for f in $LISTA; do
  [ -s "$DIR/$f" ] && continue
  curl -sfL --max-time 120 -A "Mozilla/5.0" -o "$DIR/$f.tmp" "$BASE/$f" && mv "$DIR/$f.tmp" "$DIR/$f" && echo "  $f"
  rm -f "$DIR/$f.tmp"
done
exit 0
