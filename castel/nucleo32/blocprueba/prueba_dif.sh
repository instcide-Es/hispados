#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/prueba_dif.sh: prueba BlocDif (comparar por líneas, para el Bloc
# de notas) en Linux, con corre32: PDIF.BIN contra Python (prueba_dif.py).
# FUZZ_DIF=vueltas (1500 si no se dice); temporales en $BLOC_TMP/dif.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
C=$AQUI/../..
D=${BLOC_TMP:-/tmp/blocprueba}/dif
rm -rf $D; mkdir -p $D
python3 -I $C/ccastel.py $AQUI/pdif.cas -o $D/pdif.asm -I $C/nucleo32 > /dev/null && nasm -f bin $D/pdif.asm -o $D/PDIF.BIN || { echo "MAL: pdif.cas no compila"; exit 1; }
python3 -I $AQUI/prueba_dif.py $D/PDIF.BIN $D
