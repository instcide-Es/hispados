#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/ssh/hacer.sh CARPETA
# Hace SSH.BIN (el cliente: sshc.c) y SSHD.BIN (el servidor: sshd.c) de
# Hispa-Pentium (docs/PENTIUM.md, «SSH»), en CARPETA: el transporte de
# ssh.c, lo común de hispac/ (hispa.c, cripto.c), la libc de TLS.BIN
# (tls/libc.c) y Monocypher 4.0.2 (terceros/monocypher, BSD-2 o CC0),
# enlazados para 0x40000000 como programas del anillo 3 (como TLS.BIN). Los
# objetos, en build/ssh-pentium/ (solo lo que ha cambiado). Necesita gcc
# (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
mkdir -p "$SAL"
cd "$RAIZ"
O=build/ssh-pentium
mkdir -p $O
HC=castel/nucleo32/hispac
CF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -Wall -Wno-unused
    -I$HC -Iterceros/monocypher -Icastel/nucleo32/ssh"
# objeto(fuente, objeto, opciones de más): solo si ha cambiado (o alguna cabecera)
objeto() {
  local f=$1 o=$2
  shift 2
  if [ ! -f $o ] || [ $f -nt $o ] || [ $HC/hispa.h -nt $o ] || [ $HC/cripto.h -nt $o ] || [ castel/nucleo32/ssh/ssh.h -nt $o ]; then
    # shellcheck disable=SC2086
    gcc $CF "$@" -c $f -o $o
  fi
}
objeto terceros/monocypher/monocypher.c $O/monocypher.o -Wno-all
objeto terceros/monocypher/monocypher-ed25519.c $O/monocypher-ed25519.o -Wno-all
objeto castel/nucleo32/tls/libc.c $O/libc.o -Icastel/nucleo32/tls
objeto $HC/hispa.c $O/hispa.o -fexec-charset=CP437
objeto $HC/cripto.c $O/cripto.o
objeto castel/nucleo32/ssh/ssh.c $O/ssh.o -fexec-charset=CP437
objeto castel/nucleo32/ssh/sshd.c $O/sshd.o -fexec-charset=CP437
objeto castel/nucleo32/ssh/sshc.c $O/sshc.o -fexec-charset=CP437
nasm -f elf32 $HC/entrada.asm -o $O/entrada.o
COMUN="$O/entrada.o $O/hispa.o $O/cripto.o $O/ssh.o $O/libc.o $O/monocypher.o $O/monocypher-ed25519.o"
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
for p in sshd:SSHD sshc:SSH; do
  f=${p%%:*}; N=${p##*:}
  # shellcheck disable=SC2086
  ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $HC/pentium.ld -o $O/$f.elf $COMUN $O/$f.o "$LIBGCC"
  objcopy -O binary $O/$f.elf "$SAL/$N.BIN"
done
