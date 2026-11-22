#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/juegos/prueba/anfitrion.sh CARPETA
# (JUEGOS) Compila en el anfitrión (Linux, 64 bits, con ASan y UBSan) las
# pruebas de lo de los juegos que no necesita la pantalla y las pasa:
# prueba_pinball (la física de Hispa-Pinball) y prueba_sonido (la música y
# los efectos de sintes.c). Deja los programas en CARPETA. Sale con 1 si
# algo no compila o falla. Lo llama castel/pruebas_juegos.sh (la parte 39).
set -e
AQUI=$(dirname "$(readlink -f "$0")")
J=$(readlink -f "$AQUI/..")
RAIZ=$(readlink -f "$J/../../..")
mkdir -p "${1:?uso: anfitrion.sh CARPETA}"
SAL=$(readlink -f "$1")
python3 -I $J/datos386.py $SAL/datos386.h $SAL/letra.h
F="-O1 -g -fno-pie -no-pie -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -I$AQUI/anfitrion -I$J -I$RAIZ/castel/nucleo32/comprim -I$SAL"
# shellcheck disable=SC2086
gcc $F -Wall -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast $AQUI/prueba_pinball.c $J/pinfis.c $J/pinmesa.c $J/dibujo.c -o $SAL/prueba_pinball -lm
# shellcheck disable=SC2086
gcc $F -Wall -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast $AQUI/prueba_sonido.c $AQUI/sistema.c $J/sintes.c -o $SAL/prueba_sonido
export ASAN_OPTIONS=detect_leaks=0
$SAL/prueba_pinball
$SAL/prueba_sonido $SAL/sonido.raw
