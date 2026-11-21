#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/comprim/anfitrion.sh SALIDA [ASAN]
# El motor de Comprimidos (zip.c, ordenes.c, cifra.c y miniz) en Linux, para
# las pruebas (prueba/prueba.sh): con ASAN=1, con AddressSanitizer y UBSan.
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=${1:?uso: anfitrion.sh SALIDA [1]}
MZ="-DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES"
if [ -n "${2:-}" ]; then F="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"; else F="-O2"; fi
# shellcheck disable=SC2086
gcc $F -Wall -Wno-unused-parameter $MZ -I$RAIZ/terceros/miniz -I$AQUI $AQUI/anfitrion.c $AQUI/zip.c $AQUI/ordenes.c $AQUI/cifra.c \
  $RAIZ/terceros/miniz/miniz.c -o "$SAL"
