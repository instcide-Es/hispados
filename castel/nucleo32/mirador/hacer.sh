#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/mirador/hacer.sh CARPETA
# Hace el Mirador de Hispa-Pentium (docs/PENTIUM.md, «El Mirador»): deja en
# CARPETA MIRADOR.BIN (mirador.c, con stb_image de terceros/stb, dominio
# público o MIT, y la ventana y la biblioteca de C de Comprimidos:
# ../comprim/ui.c y base.c), un programa del anillo 3 enlazado para
# 0x40000000 como MINIAT.BIN; y, al lado de CARPETA, su acceso del menú
# Inicio (MIRADOR.MNU y su icono, MIRADOR.ICP). Necesita gcc (con -m32),
# nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
mkdir -p "${1:?uso: hacer.sh CARPETA}"
SAL=$(readlink -f "$1")
cd "$RAIZ"
O=build/mirador-pentium
mkdir -p $O
C=castel/nucleo32/comprim
M=castel/nucleo32/mirador
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -I$C/inc -I$C -Iterceros/stb"
# shellcheck disable=SC2086
gcc $BF -w -c $M/mirador.c -o $O/mirador.o
for f in ui base; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -c $C/$f.c -o $O/$f.o
done
nasm -f elf32 -DPRINCIPAL=principal $C/entrada.asm -o $O/entrada.o
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $C/pentium.ld -o $O/mirador.elf $O/entrada.o $O/mirador.o $O/ui.o $O/base.o
objcopy -O binary $O/mirador.elf "$SAL/MIRADOR.BIN"
if objdump -d $O/mirador.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: MIRADOR.BIN lleva instrucciones de después del Pentium"; exit 1; fi
python3 -I tools/hpicono.py $M/mirador32.png "$SAL/../MIRADOR.ICP"
printf 'nombre=Mirador\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=no\r\n' \
  "${MIRADOR_PROGRAMA:-\\HISPA\\MIRADOR.BIN}" "${MIRADOR_ICONO:-\\HISPA\\MENU\\MIRADOR.ICP}" > "$SAL/../MIRADOR.MNU"
