#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# oficina/pintura/hacer.sh SALIDA
# Hace PINTURA.BIN, Pintura 2 de Hispa-Pentium (docs/PENTIUM.md, «Pintura 2»): el
# C portable de oficina/pintura y oficina/comun con comun/so_pentium.c, miniz
# (terceros/miniz, MIT), stb_image, stb_image_write y stb_truetype
# (terceros/stb, dominio público o MIT) y las DejaVu (terceros/dejavu),
# enlazado para 0x40000000 como programa del anillo 3 (como PDF.BIN y
# NAVEGA.BIN). -march=i586: nada de CMOV (va en un Pentium de 1993). Necesita
# gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
OFI=$(readlink -f "$AQUI/..")
RAIZ=$(readlink -f "$AQUI/../../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
O=$RAIZ/build/pintura-pentium
mkdir -p $O
python3 -I "$OFI/comun/haz_letras.py" "$RAIZ/terceros/dejavu" $O/letras_dejavu.c
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DOFI_PENTIUM
    -I$OFI/comun/inc -I$OFI/comun -I$AQUI -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
OBJS=""
while read -r c w; do
	case "$c" in ""|\#*) continue;; esac
	o=$O/$(echo $c | tr / _).o
	# shellcheck disable=SC2086
	gcc $BF -w -c "$OFI/$c.c" -o $o
	OBJS="$OBJS $o"
done < "$AQUI/fuentes.lst"
# shellcheck disable=SC2086
gcc $BF -w -c $OFI/comun/so_pentium.c -o $O/so_pentium.o
# shellcheck disable=SC2086
gcc $BF -w -c $O/letras_dejavu.c -o $O/letras_dejavu.o
nasm -f elf32 "$OFI/comun/entrada.asm" -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T "$OFI/comun/pentium.ld" -o $O/pintura.elf \
  $O/entrada.o $OBJS $O/so_pentium.o $O/letras_dejavu.o
objcopy -O binary $O/pintura.elf "$SAL"
if objdump -d $O/pintura.elf | grep -qE '\scmov'; then echo "hacer.sh: ¡hay CMOV!" >&2; exit 1; fi
T=$(stat -c %s "$SAL")
MAX=$(( $(sed -n 's/^ *constante MAX_PROGRAMA = \(0x[0-9A-Fa-f]*\).*/\1/p' $RAIZ/castel/nucleo32/usuario.cas) ))
[ "$T" -le "$MAX" ] || { echo "hacer.sh: PINTURA.BIN mide $T y el núcleo no carga más de $MAX" >&2; exit 1; }
echo "hacer.sh: $SAL ($T octetos)"
