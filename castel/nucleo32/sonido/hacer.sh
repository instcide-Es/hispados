#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/sonido/hacer.sh CARPETA
# (SONIDO) Hace, en CARPETA, el servicio de sonido de Hispa-Pentium
# (SONIDO.BIN: servicio.c, mezcla.c, hda.c, ac97.c, sb16.c y ventana.c, con
# lo común de hispac/ y la libc de TLS.BIN) y los sonidos del sistema
# (INICIO.WAV, APAGAR.WAV, AVISO.WAV y ERROR.WAV, de hazwav.py), enlazado
# para 0x40000000 como los demás programas en C del anillo 3. Los objetos,
# en build/sonido-pentium/ (solo lo que ha cambiado). Necesita gcc (con
# -m32), nasm, ld, objcopy y python3. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
mkdir -p "$SAL"
cd "$RAIZ"
O=build/sonido-pentium
mkdir -p $O
N=castel/nucleo32
HC=$N/hispac
S=$N/sonido
CF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -Wall -Wno-unused
    -I$HC -I$S -Iterceros/monocypher"
CABECERAS="$HC/hispa.h $HC/cripto.h $S/sonido.h"
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
objeto $HC/cripto.c $O/cripto.o
OBJ=
for f in servicio mezcla hda ac97 sb16 ventana; do
  objeto $S/$f.c $O/$f.o -fexec-charset=CP437
  OBJ="$OBJ $O/$f.o"
done
nasm -f elf32 $HC/entrada.asm -o $O/entrada.o
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $HC/pentium.ld -o $O/sonido.elf \
  $O/entrada.o $OBJ $O/hispa.o $O/cripto.o $O/libc.o $O/monocypher.o "$LIBGCC"
objcopy -O binary $O/sonido.elf "$SAL/SONIDO.BIN"
python3 -I $S/hazwav.py "$SAL"
