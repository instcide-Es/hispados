#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/correo/anfitrion.sh [SALIDA] [asan|rapido]
# Correo en Linux: lo mismo que CORREO.BIN, con so_anfitrion.c (POSIX y
# OpenSSL) y sin ventana, para las pruebas (con ASan y UBSan por defecto) y
# como modelo para Hispa-64. Sin «make -j».
set -e -o pipefail
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=${1:-$RAIZ/build/correo-anfitrion/correo}
MODO=${2:-asan}
O=$(dirname "$SAL")/obj-$MODO
mkdir -p "$O"
if [ "$MODO" = asan ]; then
	BF="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
else
	BF="-O2 -g"
fi
W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-format-truncation"
OBJS=""
for c in $(cat "$AQUI/FUENTES") so_anfitrion; do
	# shellcheck disable=SC2086
	gcc -std=gnu11 $BF $W -I"$AQUI" -I"$RAIZ/terceros/stb" -c "$AQUI/$c.c" -o "$O/$c.o"
	OBJS="$OBJS $O/$c.o"
done
# shellcheck disable=SC2086
gcc $BF -o "$SAL" $OBJS -lssl -lcrypto
echo "anfitrion.sh: $SAL"
