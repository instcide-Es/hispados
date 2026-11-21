#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/comprim/hacer.sh CARPETA
# Hace Comprimidos de Hispa-Pentium (docs/PENTIUM.md, «Comprimidos») y deja
# en CARPETA COMPRIM.BIN (la ventana, las órdenes y el motor: zip.c, con
# miniz de terceros/miniz, MIT), ZIP.BIN y UNZIP.BIN (las órdenes: lanza.c),
# programas del anillo 3 enlazados para 0x40000000 como MINIAT.BIN; y, al
# lado de CARPETA, el acceso del menú Inicio (COMPRIM.MNU y su icono,
# COMPRIM.ICP). Necesita gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
cd "$RAIZ"
O=build/comprim-pentium
mkdir -p $O "$SAL"
C=castel/nucleo32/comprim
# (como mucho -march=i586: nada de CMOV, que el Pentium de verdad no tiene)
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections
    -I$C/inc -I$C -Iterceros/miniz
    -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_ZLIB_APIS -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES"
for f in comprim zip ordenes cifra plat ui base; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -Wno-unused-function -c $C/$f.c -o $O/$f.o
done
# shellcheck disable=SC2086
gcc $BF -w -c terceros/miniz/miniz.c -o $O/miniz.o
nasm -f elf32 -DPRINCIPAL=principal $C/entrada.asm -o $O/entrada.o
nasm -f elf32 -DPRINCIPAL=lanza $C/entrada.asm -o $O/entrada_l.o
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $C/pentium.ld -o $O/comprim.elf \
  $O/entrada.o $O/comprim.o $O/zip.o $O/ordenes.o $O/cifra.o $O/plat.o $O/ui.o $O/base.o $O/miniz.o
objcopy -O binary $O/comprim.elf "$SAL/COMPRIM.BIN"
for o in ZIP UNZIP; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -DORDEN="\"/$o\"" -c $C/lanza.c -o $O/lanza_$o.o
  ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $C/pentium.ld -o $O/lanza_$o.elf $O/entrada_l.o $O/lanza_$o.o
  objcopy -O binary $O/lanza_$o.elf "$SAL/$o.BIN"
done
# (nada de CMOV ni de FCOMI: el Pentium de verdad no los tiene)
if objdump -d $O/comprim.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: COMPRIM.BIN lleva instrucciones de después del Pentium"; exit 1; fi
# el acceso del menú Inicio (accesos.cas)
python3 -I tools/hpicono.py $C/comprim32.png "$SAL/../COMPRIM.ICP"
printf 'nombre=Comprimidos\r\nprograma=%s\r\nargumentos=\r\nicono=%s\r\nescritorio=no\r\n' \
  "${COMPRIM_PROGRAMA:-\\HISPA\\COMPRIM.BIN}" "${COMPRIM_ICONO:-\\HISPA\\MENU\\COMPRIM.ICP}" > "$SAL/../COMPRIM.MNU"
