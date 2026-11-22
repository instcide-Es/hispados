#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/prueba_pag.sh: prueba BlocPag (los juegos de caracteres del Bloc
# de notas) en Linux, con corre32: PPAG.BIN contra Python (prueba_pag.py).
# FUZZ_PAG=vueltas (3000 si no se dice); temporales en $BLOC_TMP/pag.
set -u
AQUI=$(cd "$(dirname "$0")" && pwd)
C=$AQUI/../..
D=${BLOC_TMP:-/tmp/blocprueba}/pag
rm -rf $D; mkdir -p $D
python3 -I $C/ccastel.py $AQUI/ppag.cas -o $D/ppag.asm -I $C/nucleo32 > /dev/null && nasm -f bin $D/ppag.asm -o $D/PPAG.BIN || { echo "MAL: ppag.cas no compila"; exit 1; }
python3 -I $AQUI/prueba_pag.py $D/PPAG.BIN $D
