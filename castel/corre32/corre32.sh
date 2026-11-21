#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# corre32.sh [-C carpeta] PROGRAMA.BIN [argumentos...]
# Corre en Linux un programa de usuario de Hispa-Pentium (corre32.c dice
# cómo). La primera vez (o si han cambiado los fuentes) hace corre32 y su
# arranque en CORRE32_DIR (~/.cache/corre32 si no se dice). Necesita gcc, nasm
# y ld (con elf_i386).
set -u
AQUI=$(dirname "$(readlink -f "$0")")
D=${CORRE32_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/corre32}
mkdir -p "$D"
if [ ! -x "$D/corre32" ] || [ "$AQUI/corre32.c" -nt "$D/corre32" ] || [ "$AQUI/arranque.asm" -nt "$D/arranque32" ]; then
  gcc -O2 -Wall -Wno-format-truncation -o "$D/corre32" "$AQUI/corre32.c" || exit 120
  nasm -f elf32 "$AQUI/arranque.asm" -o "$D/arranque.o" && ld -m elf_i386 -Ttext=0x08048000 "$D/arranque.o" -o "$D/arranque32" || exit 120
fi
exec "$D/corre32" "$@"
