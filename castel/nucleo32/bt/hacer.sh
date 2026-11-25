#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/bt/hacer.sh SALIDA
# (BLUETOOTH) Hace BT.BIN, el servicio Bluetooth de Hispa-Pentium (y la orden
# BT y su ventana: servicio.c, ventana.c; la pila: hci.c, l2cap.c, intel.c y
# claves.c), con lo común de hispac/, la libc de TLS.BIN y Monocypher (las
# claves de BT.INI), enlazado para 0x40000000 como los demás programas en C
# del anillo 3. Los objetos, en build/bt-pentium/ (solo lo que ha cambiado).
# Necesita gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
mkdir -p "$(dirname "$SAL")"
cd "$RAIZ"
O=build/bt-pentium
mkdir -p $O
N=castel/nucleo32
HC=$N/hispac
B=$N/bt
CF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -Wall -Wno-unused
    -DHL_PENTIUM_BT -I$HC -I$B -Iterceros/monocypher"
CABECERAS="$HC/hispa.h $B/bt.h"
objeto() {
  local f=$1 o=$2 h
  shift 2
  local hacer=
  [ -f $o ] && [ ! $f -nt $o ] || hacer=1
  for h in $CABECERAS; do [ $h -nt $o ] && hacer=1; done
  if [ -n "$hacer" ]; then
    # shellcheck disable=SC2086
    gcc $CF "$@" -c $f -o $o
  fi
}
objeto terceros/monocypher/monocypher.c $O/monocypher.o -Wno-all
objeto $N/tls/libc.c $O/libc.o -I$N/tls
objeto $HC/hispa.c $O/hispa.o -fexec-charset=CP437
OBJ=
for f in servicio ventana hci l2cap intel claves; do
  objeto $B/$f.c $O/$f.o -fexec-charset=CP437
  OBJ="$OBJ $O/$f.o"
done
nasm -f elf32 $HC/entrada.asm -o $O/entrada.o
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $HC/pentium.ld -o $O/bt.elf \
  $O/entrada.o $OBJ $O/hispa.o $O/libc.o $O/monocypher.o "$LIBGCC"
objcopy -O binary $O/bt.elf "$SAL"
