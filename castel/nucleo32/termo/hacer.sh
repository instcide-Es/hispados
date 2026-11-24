#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/termo/hacer.sh SALIDA
# (TERMO) Hace TERMO.BIN, el Termómetro (termo.c): un programa del anillo 3
# en C con el marco de los juegos (../juegos/juego.c, dibujo.c y sintes.c) y
# la entrada y la biblioteca de C de Comprimidos (../comprim/entrada.asm,
# pentium.ld y base.c), enlazado para 0x40000000. Necesita gcc (con -m32),
# nasm, ld, objcopy y python3. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "$(dirname "${1:?uso: hacer.sh SALIDA}")")/$(basename "$1")
cd "$RAIZ"
O=build/termo-pentium
mkdir -p $O
C=castel/nucleo32/comprim
J=castel/nucleo32/juegos
M=castel/nucleo32/termo
python3 -I $J/datos386.py $O/datos386.h $O/letra.h
# (como mucho -march=i586: nada de CMOV ni de FCOMI, que el Pentium no tiene)
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -I$C/inc -I$C -I$J -I$M -I$O"
for f in $J/juego $J/dibujo $J/sintes $M/termo $M/veredicto $M/envio $M/rendimiento $M/disco $M/nvidia; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -Wextra -Wno-unused-parameter -c $f.c -o $O/$(basename $f).o
done
# shellcheck disable=SC2086
gcc $BF -Wall -c $C/base.c -o $O/base.o
nasm -f elf32 -DPRINCIPAL=principal $C/entrada.asm -o $O/entrada.o
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $C/pentium.ld -o $O/TERMO.elf \
  $O/entrada.o $O/juego.o $O/dibujo.o $O/sintes.o $O/base.o $O/termo.o $O/veredicto.o $O/envio.o $O/rendimiento.o $O/disco.o $O/nvidia.o
objcopy -O binary $O/TERMO.elf "$SAL"
if objdump -d $O/TERMO.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: TERMO.BIN lleva instrucciones de después del Pentium"; exit 1; fi
