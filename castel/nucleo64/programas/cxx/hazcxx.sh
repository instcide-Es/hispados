#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hazcxx.sh - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
# sección 16): los programas de C++ de prueba, con cxx.sh (la libstdc++ del
# sistema con musl), en la carpeta SALIDA:
#   MCXX     la prueba de C++17 (mcxx.cpp): excepciones, RTTI, contenedores,
#            flujos, regex, cientos de hilos, thread_local...
# Uso: hazcxx.sh SALIDA   (dice en la última línea los que ha hecho; sin g++
# o sin las cabeceras de la libstdc++, ninguno)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
S=$1
[ -n "$S" ] || { echo "uso: hazcxx.sh SALIDA" >&2; exit 2; }
command -v g++ > /dev/null || { echo "hechos: (sin g++)"; exit 0; }
"$AQUI/cxx.sh" -std=c++17 -O2 -Wall -o "$S/MCXX" "$AQUI/mcxx.cpp"
strip --strip-unneeded "$S/MCXX"
echo "hechos: MCXX"
