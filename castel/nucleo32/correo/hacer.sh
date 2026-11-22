#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/correo/hacer.sh SALIDA
# Hace CORREO.BIN, el cliente de correo de Hispa-Pentium (docs/PENTIUM.md,
# «Correo»): el C portable de esta carpeta (FUENTES) con so_pentium.c y
# libc_pentium.c, y stb_image (terceros/stb, dominio público o MIT),
# enlazado para 0x40000000 como programa del anillo 3 (como PDF.BIN).
# -march=i586: nada de CMOV. Necesita gcc (con -m32), nasm, ld y objcopy.
# Sin «make -j».
set -e -o pipefail
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/correo-pentium
mkdir -p $O
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DCORREO_PENTIUM
    -I$AQUI/inc -I$AQUI -Iterceros/stb"
W="-Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-format-truncation"
OBJS=""
for c in $(cat "$AQUI/FUENTES") so_pentium libc_pentium; do
	case $c in imagenes) WW="-w";; *) WW="$W";; esac
	# shellcheck disable=SC2086
	gcc $BF $WW -c "$AQUI/$c.c" -o $O/$c.o
	OBJS="$OBJS $O/$c.o"
done
nasm -f elf32 "$AQUI/entrada.asm" -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T "$AQUI/pentium.ld" -o $O/correo.elf $O/entrada.o $OBJS
objcopy -O binary $O/correo.elf "$SAL"
# que no haya CMOV ni FCOMI (-march=i586: el Pentium de verdad no los tiene)
if objdump -d $O/correo.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: ¡hay instrucciones de después del Pentium!" >&2; exit 1; fi
echo "hacer.sh: $SAL ($(stat -c %s "$SAL") octetos)"
