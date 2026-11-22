#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/pdf/anfitrion.sh [SALIDA] [asan|rapido]
# Hispa-PDF en Linux: lo mismo que PDF.BIN, con so_anfitrion.c (para las
# pruebas: con ASan y UBSan por defecto). Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=${1:-$RAIZ/build/pdf-anfitrion/pdf}
MODO=${2:-asan}
O=$(dirname "$SAL")/obj-$MODO
mkdir -p "$O"
python3 -I "$AQUI/haz_fuentes.py" "$RAIZ/terceros/dejavu" "$O/fuentes.c"
if [ "$MODO" = asan ]; then
	BF="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
else
	BF="-O2 -g"
fi
INC="-I$AQUI -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
OBJS=""
for c in objetos documento filtros cifra escribe paginas util orden imagen fuente pinta texto so_anfitrion miniz_hpdf; do
	W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function"
	case $c in miniz_hpdf|imagen|fuente) W="-w";; esac
	# shellcheck disable=SC2086
	gcc -std=gnu11 $BF $W $INC -c "$AQUI/$c.c" -o "$O/$c.o"
	OBJS="$OBJS $O/$c.o"
done
gcc -std=gnu11 $BF -w -I"$AQUI" -c "$O/fuentes.c" -o "$O/fuentes.o"
# shellcheck disable=SC2086
gcc $BF -o "$SAL" $OBJS "$O/fuentes.o" -lm
echo "anfitrion.sh: $SAL"
