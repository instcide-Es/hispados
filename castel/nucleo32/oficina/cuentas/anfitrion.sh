#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/cuentas/anfitrion.sh [SALIDA] [asan|rapido]
# Cuentas 2 en Linux: lo mismo que CUENTAS.BIN, con comun/so_anfitrion.c (para
# las pruebas: con ASan y UBSan por defecto; y el modelo para Hispa-64: el
# mismo C, otro so_*.c). Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
OFI=$(readlink -f "$AQUI/..")
RAIZ=$(readlink -f "$AQUI/../../../..")
SAL=${1:-$RAIZ/build/cuentas-anfitrion/cuentas}
MODO=${2:-asan}
O=$(dirname "$SAL")/obj-$MODO
mkdir -p "$O"
python3 -I "$OFI/comun/haz_letras.py" "$RAIZ/terceros/dejavu" "$O/letras_dejavu.c"
if [ "$MODO" = asan ]; then
	BF="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
else
	BF="-O2 -g"
fi
INC="-I$OFI/comun -I$AQUI -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
OBJS=""
compila() {
	local c=$1 W=$2 o=$O/$(echo $1 | tr / _).o
	if [ ! -f "$o" ] || [ "$OFI/$c.c" -nt "$o" ] || [ -n "$(find $OFI/comun $AQUI -name '*.h' -newer "$o" 2>/dev/null | head -1)" ]; then
		# shellcheck disable=SC2086
		gcc -std=gnu11 $BF $W $INC -c "$OFI/$c.c" -o "$o"
	fi
	OBJS="$OBJS $o"
}
while read -r c w; do
	case "$c" in ""|\#*) continue;; esac
	W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-missing-field-initializers"
	[ "$w" = "-w" ] && W="-w"
	compila "$c" "$W"
done < "${CUENTAS_FUENTES:-$AQUI/fuentes.lst}"
compila comun/so_anfitrion "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
# shellcheck disable=SC2086
gcc -std=gnu11 $BF -w -I"$OFI/comun" -c "$O/letras_dejavu.c" -o "$O/letras_dejavu.o"
# shellcheck disable=SC2086
gcc $BF -o "$SAL" $OBJS "$O/letras_dejavu.o" -lm
echo "anfitrion.sh: $SAL"
