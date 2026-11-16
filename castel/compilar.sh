#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compilar.sh ENTRADA.cas [SALIDA.COM]
# Compila un fuente Castel a ensamblador (ccastel.py) y lo ensambla a un .COM
# de DOS (NASM). Sin SALIDA, deja NOMBRE.COM (en mayusculas) al lado del fuente.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
ENT=$1
[ -z "$ENT" ] && { echo "uso: compilar.sh ENTRADA.cas [SALIDA.COM]"; exit 2; }
BASE=$(basename "${ENT%.*}")
ASM=$(dirname "$ENT")/$BASE.asm
COM=${2:-$(dirname "$ENT")/$(echo "$BASE" | tr 'a-z' 'A-Z').COM}
python3 -I "$AQUI/ccastel.py" "$ENT" -o "$ASM"
nasm -f bin "$ASM" -o "$COM"
echo "compilar: $ENT -> $COM"
