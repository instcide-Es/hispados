#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# blocprueba/prueba_exp.sh: prueba BlocExp, las expresiones regulares del
# Bloc de notas (blocexp.cas). Compila pexp.cas, lo corre con corre32 en
# Linux (hazexp.py: la tabla a mano, el tope, lo que tarda, el fuzz contra
# el «re» de Python —FUZZ_EXP vueltas, 3000 si no se dice— y los patrones
# rotos) y mira con igual32.sh que CASTEL32 saque lo mismo que ccastel.py.
# Escribe «bien: ...» o «mal: ...» y acaba con 1 si algo va mal.
set -u
B=$(cd "$(dirname "$0")" && pwd)
C=$(cd "$B/../.." && pwd)                   # castel/
T=${BLOC_TMP:-/tmp/blocprueba}/exp
mkdir -p $T
if ! python3 -I $C/ccastel.py $B/pexp.cas -o $T/pexp.asm -I $C/nucleo32 > $T/compila.txt 2>&1 || ! nasm -f bin $T/pexp.asm -o $T/PEXP.BIN 2>> $T/compila.txt; then
  echo "mal: pexp.cas (con blocexp.cas) no compila:"; head -5 $T/compila.txt; exit 1
fi
mal=0
python3 -I $B/hazexp.py $T/PEXP.BIN $C/corre32/corre32.sh $T || mal=1
BLOC_TMP=$T $B/igual32.sh $B/pexp.cas || mal=1
exit $mal
