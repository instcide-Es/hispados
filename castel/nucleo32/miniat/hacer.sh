#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/miniat/hacer.sh SALIDA
# Hace MINIAT.BIN, las miniaturas del explorador de archivos de Hispa-Pentium
# (docs/PENTIUM.md, «El explorador de archivos»): miniat.c con stb_image
# (terceros/stb, dominio público o MIT), enlazado para 0x40000000 como
# programa del anillo 3 (como TLS.BIN, nucleo32/tls/hacer.sh). Necesita gcc
# (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/miniat-pentium
mkdir -p $O
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections
    -Icastel/nucleo32/miniat/inc -Iw32/inc -Iterceros/stb"
# shellcheck disable=SC2086
gcc $BF -w -c castel/nucleo32/miniat/miniat.c -o $O/miniat.o
nasm -f elf32 castel/nucleo32/miniat/entrada.asm -o $O/entrada.o
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T castel/nucleo32/miniat/pentium.ld -o $O/miniat.elf \
  $O/entrada.o $O/miniat.o
objcopy -O binary $O/miniat.elf "$SAL"
