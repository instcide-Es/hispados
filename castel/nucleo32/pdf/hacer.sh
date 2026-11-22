#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/pdf/hacer.sh SALIDA
# Hace PDF.BIN, Hispa-PDF (docs/PENTIUM.md, «Hispa-PDF»): el C portable de
# esta carpeta con so_pentium.c, miniz (terceros/miniz, MIT), stb_image,
# stb_image_write y stb_truetype (terceros/stb, dominio público o MIT) y
# las DejaVu (terceros/dejavu), enlazado para 0x40000000 como programa del
# anillo 3 (como TLS.BIN y MINIAT.BIN). -march=i586: nada de CMOV. Necesita
# gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/pdf-pentium
mkdir -p $O
python3 -I "$AQUI/haz_fuentes.py" terceros/dejavu $O/fuentes.c
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DHPDF_PENTIUM
    -I$AQUI/inc -Iw32/inc -I$AQUI -Iterceros/miniz -Iterceros/stb"
OBJS=""
for c in objetos documento filtros cifra escribe paginas util orden imagen fuente pinta texto so_pentium miniz_hpdf; do
	# shellcheck disable=SC2086
	gcc $BF -w -c "$AQUI/$c.c" -o $O/$c.o
	OBJS="$OBJS $O/$c.o"
done
# shellcheck disable=SC2086
gcc $BF -w -c $O/fuentes.c -o $O/fuentes.o
nasm -f elf32 "$AQUI/entrada.asm" -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T "$AQUI/pentium.ld" -o $O/pdf.elf \
  $O/entrada.o $OBJS $O/fuentes.o
objcopy -O binary $O/pdf.elf "$SAL"
# que no haya CMOV (-march=i586)
if objdump -d $O/pdf.elf | grep -qE '\scmov'; then echo "hacer.sh: ¡hay CMOV!" >&2; exit 1; fi
echo "hacer.sh: $SAL ($(stat -c %s "$SAL") octetos)"
