#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazlos.sh - Hispa-64, etapa 4 (docs/64/HISPA-64.md,
# sección 10): hace los programas de prueba de la unidad en memoria (los
# módulos del cargador) en la carpeta SALIDA, con los nombres que buscan las
# pruebas del núcleo (prproc64.cas):
#   HOLA, MEMORIA, CUENTA, MALO   en C, ELF64 estáticos sin biblioteca (gcc
#                                 -static -nostdlib -ffreestanding: el de
#                                 Linux x86-64 vale tal cual)
#   HOLACAS                       en Castel, «modelo linux64» (ccastel.py y NASM)
#   SALUDO.TXT                    un texto, para leerlo con open y read
# Uso: hazlos.sh SALIDA   (dice en la última línea los que ha hecho)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
C=$(dirname "$(dirname "$AQUI")")
S=$1
[ -n "$S" ] || { echo "uso: hazlos.sh SALIDA" >&2; exit 2; }
mkdir -p "$S"
HECHOS=""
if command -v gcc > /dev/null; then
  for p in hola memoria cuenta malo archivos; do
    gcc -static -nostdlib -ffreestanding -fno-stack-protector -fno-pie -no-pie -O2 -fno-builtin \
        -fcf-protection=none -fno-asynchronous-unwind-tables -o "$S/$(echo $p | tr a-z A-Z)" "$AQUI/$p.c"
    HECHOS="$HECHOS $(echo $p | tr a-z A-Z)"
  done
fi
python3 -I "$C/ccastel.py" "$AQUI/holacas.cas" -o "$S/holacas.asm" -I "$C/biblioteca64/linux" -I "$C/biblioteca" > /dev/null
nasm -f bin "$S/holacas.asm" -o "$S/HOLACAS"
rm -f "$S/holacas.asm"
cp "$AQUI/saludo.txt" "$S/SALUDO.TXT"
echo "hechos:$HECHOS HOLACAS SALUDO.TXT"
