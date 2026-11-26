#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# acpi/pentium/hacer.sh SALIDA
# Hace ACPI.BIN, el servicio de ACPI de Hispa-Pentium (docs/PENTIUM.md,
# «Apagar de verdad y la batería»): el mismo núcleo de C que HSPACPI.BIN de
# Hispa-DOS (acpi/nucleo/ y ACPICA, los mismos objetos de build/acpi/, que
# hace el Makefile) con su entrada, su «máquina» y su bucle de aquí
# (entrada.asm, maquina.c, servicio.c; nucleo.c otra vez, con
# -DHISPA_PENTIUM), enlazado para 0x40000000 como programa del anillo 3.
# Necesita make, gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../..")
SAL=${1:?uso: hacer.sh SALIDA}
cd "$RAIZ"
make -s build/HSPACPI.BIN > /dev/null
O=build/acpi-pentium
mkdir -p $O
CF="-m32 -std=gnu11 -march=i586 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -D__HISPA__ -DHISPA_PENTIUM -Iacpi/acpica/include -Iacpi/nucleo -Iacpi/pentium -Os -ffunction-sections
    -fdata-sections -fexec-charset=CP437 -Wall -Wno-unused"
DEP="acpi/nucleo/*.h acpi/pentium/*.h acpi/acpica/include/*.h"
for f in acpi/pentium/servicio.c acpi/pentium/maquina.c acpi/pentium/i2chid.c acpi/pentium/sdhci.c acpi/pentium/sdio.c acpi/pentium/hpradio.c acpi/nucleo/nucleo.c; do
  o=$O/$(basename $f .c).o
  # (solo lo que ha cambiado)
  if [ ! -f $o ] || [ -n "$(find $f $DEP -newer $o 2>/dev/null | head -1)" ]; then
    # shellcheck disable=SC2086
    gcc $CF -c $f -o $o
  fi
done
nasm -f elf32 acpi/pentium/entrada.asm -o $O/entrada.o
OBJ=$(ls build/acpi/*.o | grep -v -e '/maquina\.o$' -e '/nucleo\.o$' -e '/entrada\.o$')
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T acpi/pentium/pentium.ld -o $O/acpi.elf \
  $O/entrada.o $O/servicio.o $O/maquina.o $O/i2chid.o $O/sdhci.o $O/sdio.o $O/hpradio.o $O/nucleo.o $OBJ
objcopy -O binary $O/acpi.elf "$SAL"
