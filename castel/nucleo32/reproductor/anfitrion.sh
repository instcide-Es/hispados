#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/reproductor/anfitrion.sh SALIDA [asan]
# El Reproductor en el anfitrión (Linux): el programa de las pruebas
# (pruebas/anfitrion.c) con so_anfitrion.c y las bibliotecas de terceros/.
# Con «asan», con AddressSanitizer y UBSan (para el fuzz). Con «i586», de
# 32 bits y con el x87 (lo mismo que en Hispa-Pentium: para medir y para que
# las cuentas salgan igual). Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: anfitrion.sh SALIDA [asan|i586]}")
T=$RAIZ/terceros
X=$T/libxmp-lite/src
O=$(mktemp -d)
trap 'rm -rf $O' EXIT
CF="-O2 -g -std=gnu11 -I$AQUI -I$O -I$T/minimp3 -I$T/stb_vorbis -I$T/dr_flac -I$T/stb -I$T/libxmp-lite/include/libxmp-lite"
case "${2:-}" in
  asan) CF="$CF -fsanitize=address,undefined -fno-omit-frame-pointer -O1" ;;
  i586) CF="$CF -m32 -march=i586 -mfpmath=387" ;;
esac
XMP="virtual format period player read_event misc dataio lfo scan control filter effects mixer mix_all load_helpers load filetype hio smix memio"
XMPL="common itsex sample xm_load mod_load s3m_load it_load"
for f in $XMP; do gcc $CF -w -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -c $X/$f.c -o $O/x_$f.o; done
for f in $XMPL; do gcc $CF -w -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC -c $X/loaders/$f.c -o $O/xl_$f.o; done
gcc -O2 -I$T/stb -o $O/haz_letras $AQUI/haz_letras.c -lm
$O/haz_letras $T/dejavu/SANS.TTF $T/dejavu/SANSN.TTF > $O/letras.c
gcc $CF -w -c $O/letras.c -o $O/letras.o
for f in util decod dmp3 dvorbis dflac dwav dmod etiquetas lista dsp motor dibujo imagen vista principal so_anfitrion; do
  [ -f $AQUI/$f.c ] || continue
  gcc $CF -Wall -Wno-unused-function -c $AQUI/$f.c -o $O/$f.o
done
gcc $CF -Wall -c $AQUI/pruebas/anfitrion.c -o $O/prueba.o
# shellcheck disable=SC2086
gcc $CF -o "$SAL" $O/*.o -lm
echo "anfitrion.sh: $SAL"
