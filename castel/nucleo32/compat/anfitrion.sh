#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/compat/anfitrion.sh [SALIDA] [asan|rapido]
# (COMPAT) El Asistente de compatibilidad en Linux: lo mismo que COMPAT.BIN,
# con oficina/comun/so_anfitrion.c y compat/plat_anfitrion.c (para las
# pruebas: con ASan y UBSan por defecto). Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
OFI=$(readlink -f "$AQUI/../oficina")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=${1:-$RAIZ/build/compat-anfitrion/compat}
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
	local c=$1 W=$2 f o
	case "$c" in comun/*) f=$OFI/$c.c ;; compat/*) f=$AQUI/${c#compat/}.c ;; esac
	o=$O/$(echo $c | tr / _).o
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find $OFI/comun $AQUI -name '*.h' -newer "$o" 2>/dev/null | head -1)" ]; then
		# shellcheck disable=SC2086
		gcc -std=gnu11 $BF $W $INC -c "$f" -o "$o"
	fi
	OBJS="$OBJS $o"
}
while read -r c w; do
	case "$c" in ""|\#*) continue;; esac
	W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function -Wno-missing-field-initializers"
	[ "$w" = "-w" ] && W="-w"
	compila "$c" "$W"
done < "$AQUI/fuentes.lst"
compila comun/so_anfitrion "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
compila compat/plat_anfitrion "-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare"
# shellcheck disable=SC2086
gcc -std=gnu11 $BF -w -I"$OFI/comun" -c "$O/letras_dejavu.c" -o "$O/letras_dejavu.o"
# shellcheck disable=SC2086
gcc $BF -o "$SAL" $OBJS "$O/letras_dejavu.o" -lm
echo "anfitrion.sh: $SAL"
