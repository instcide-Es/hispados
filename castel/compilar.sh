#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# compilar.sh ENTRADA.cas [SALIDA.COM] [-I CARPETA]...
# Compila un fuente Castel a ensamblador (ccastel.py) y lo ensambla a un .COM
# de DOS (NASM). Sin SALIDA, deja NOMBRE.COM (en mayúsculas) al lado del fuente.
# El ensamblador intermedio (NOMBRE.asm) queda al lado del .COM.
# Los módulos que el programa importa se buscan en su carpeta, en las que se
# den con -I y en biblioteca/ (la biblioteca de Castel).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
ENT=
COM=
RUTAS=()
while [ $# -gt 0 ]; do
  case "$1" in
    -I) RUTAS+=(-I "$2"); shift 2 ;;
    -I*) RUTAS+=("$1"); shift ;;
    *) if [ -z "$ENT" ]; then ENT=$1; elif [ -z "$COM" ]; then COM=$1; fi; shift ;;
  esac
done
[ -z "$ENT" ] && { echo "uso: compilar.sh ENTRADA.cas [SALIDA.COM] [-I CARPETA]..."; exit 2; }
BASE=$(basename "${ENT%.*}")
COM=${COM:-$(dirname "$ENT")/$(echo "$BASE" | tr 'a-z' 'A-Z').COM}
ASM=$(dirname "$COM")/$BASE.asm
python3 -I "$AQUI/ccastel.py" "$ENT" -o "$ASM" "${RUTAS[@]}"
nasm -f bin "$ASM" -o "$COM"
echo "compilar: $ENT -> $COM"
