#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compilar.sh ENTRADA.cas [SALIDA.COM]
# Compila un fuente Castel a ensamblador (ccastel.py) y lo ensambla a un .COM
# de DOS (NASM). Sin SALIDA, deja NOMBRE.COM (en mayúsculas) al lado del fuente.
# El ensamblador intermedio (NOMBRE.asm) queda al lado del .COM.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
ENT=$1
[ -z "$ENT" ] && { echo "uso: compilar.sh ENTRADA.cas [SALIDA.COM]"; exit 2; }
BASE=$(basename "${ENT%.*}")
COM=${2:-$(dirname "$ENT")/$(echo "$BASE" | tr 'a-z' 'A-Z').COM}
ASM=$(dirname "$COM")/$BASE.asm
python3 -I "$AQUI/ccastel.py" "$ENT" -o "$ASM"
nasm -f bin "$ASM" -o "$COM"
echo "compilar: $ENT -> $COM"
