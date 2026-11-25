#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# acpi/hispa64/hacer.sh SALIDA
# Hace ACPICA64.BIN, ACPI en el núcleo de Hispa-64 (docs/64/HISPA-64.md,
# etapa 11 b): ACPICA (acpi/acpica/, la misma de Hispa-DOS y Hispa-Pentium)
# y el núcleo de C de acpi/nucleo/ (base.c, osl.c, ec.c, bateria.c; no
# nucleo.c, que es la tarea de Hispa-DOS) compilados para 64 bits, en el
# anillo 0 (sin la zona roja ni SSE: las interrupciones van en la misma pila
# y el núcleo no guarda los registros SSE), con lo de aquí (entrada.asm,
# maquina.c, servicio.c), enlazado para 0xFFFFFFFFC0000000. Los objetos, en
# build/acpi64/ (solo se rehace lo que ha cambiado). Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../..")
SAL=${1:?uso: hacer.sh SALIDA}
cd "$RAIZ"
O=build/acpi64
mkdir -p $O
CF="-m64 -std=gnu11 -ffreestanding -fno-pic -fno-pie -mcmodel=kernel -mno-red-zone -mgeneral-regs-only -mno-sse -mno-mmx
    -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -D__HISPA__ -Iacpi/acpica/include -Iacpi/nucleo
    -Iacpi/hispa64 -Os -ffunction-sections -fdata-sections -fexec-charset=CP437"
DEP=$(ls acpi/nucleo/*.h acpi/hispa64/*.h acpi/acpica/include/*.h acpi/acpica/include/platform/*.h)
nuevo() {   # OBJETO FUENTE: ¿hay que rehacerlo?
  [ ! -f "$1" ] || [ "$2" -nt "$1" ] || [ -n "$(find $DEP -newer "$1" 2> /dev/null | head -1)" ]
}
OBJ=""
for f in $(ls acpi/acpica/*/*.c | grep -v '/rsdump\.c$'); do
  o=$O/$(basename "$f" .c).o
  # shellcheck disable=SC2086
  nuevo "$o" "$f" && nice gcc $CF -w -c "$f" -o "$o"
  OBJ="$OBJ $o"
done
for f in acpi/nucleo/base.c acpi/nucleo/osl.c acpi/nucleo/ec.c acpi/nucleo/bateria.c acpi/hispa64/maquina.c acpi/hispa64/servicio.c; do
  o=$O/n_$(basename "$f" .c).o
  # shellcheck disable=SC2086
  nuevo "$o" "$f" && nice gcc $CF -Wall -Wno-unused -c "$f" -o "$o"
  OBJ="$OBJ $o"
done
nasm -f elf64 acpi/hispa64/entrada.asm -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_x86_64 --no-warn-rwx-segments -z noexecstack -z max-page-size=4096 --gc-sections -T acpi/hispa64/hispa64.ld \
  -o $O/acpica64.elf $O/entrada.o $OBJ
objcopy -O binary $O/acpica64.elf "$SAL"
