#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/reproductor/hacer.sh SALIDA
# Hace REPRODUC.BIN, el Reproductor de Hispa-Pentium (docs/PENTIUM.md, «El
# Reproductor»): el C portable de esta carpeta con so_pentium.c, minimp3
# (CC0), stb_vorbis y dr_flac (dominio público), libxmp-lite (MIT) y
# stb_image (terceros/stb), y las letras DejaVu ya hechas puntos
# (haz_letras.c, en el anfitrión), enlazado para 0x40000000 como programa del
# anillo 3. -march=i586 y nada de CMOV ni FCOMI (el Pentium de verdad y el
# Atom). Los objetos, en build/reproductor-pentium/ (solo lo que ha
# cambiado). Necesita gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/reproductor-pentium
mkdir -p $O
T=terceros
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -fno-strict-aliasing -Werror=implicit-function-declaration -Werror=int-conversion -Werror=incompatible-pointer-types
    -nostdinc -isystem $(gcc -m32 -print-file-name=include) -I$AQUI/inc -I$AQUI -I$T/minimp3 -I$T/stb_vorbis -I$T/dr_flac -I$T/stb
    -I$T/libxmp-lite/include/libxmp-lite -DREPRO_PENTIUM"
CABECERAS="$AQUI/so.h $AQUI/decod.h $AQUI/util.h $AQUI/lista.h $AQUI/motor.h $AQUI/dsp.h $AQUI/etiquetas.h $AQUI/vista.h"
objeto() {
  local f=$1 o=$2 h hacer=
  shift 2
  [ -f $o ] && [ ! $f -nt $o ] || hacer=1
  for h in $CABECERAS; do [ $h -nt $o ] && hacer=1; done
  if [ -n "$hacer" ]; then
    # shellcheck disable=SC2086
    gcc $BF "$@" -c $f -o $o
  fi
}
OBJ=
X=$T/libxmp-lite/src
for f in virtual format period player read_event misc dataio lfo scan control filter effects mixer mix_all load_helpers load filetype hio smix memio; do
  objeto $X/$f.c $O/x_$f.o -w -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC
  OBJ="$OBJ $O/x_$f.o"
done
for f in common itsex sample xm_load mod_load s3m_load it_load; do
  objeto $X/loaders/$f.c $O/xl_$f.o -w -DLIBXMP_CORE_PLAYER -DLIBXMP_STATIC
  OBJ="$OBJ $O/xl_$f.o"
done
# las letras: DejaVu Sans (y la negrita) hechas puntos en el anfitrión
if [ ! -f $O/letras.c ] || [ $AQUI/haz_letras.c -nt $O/letras.c ]; then
  gcc -O2 -I$T/stb -o $O/haz_letras $AQUI/haz_letras.c -lm
  $O/haz_letras $T/dejavu/SANS.TTF $T/dejavu/SANSN.TTF > $O/letras.c
fi
objeto $O/letras.c $O/letras.o -w
OBJ="$OBJ $O/letras.o"
for f in dmp3 dvorbis dflac dmod; do
  objeto $AQUI/$f.c $O/$f.o -w
  OBJ="$OBJ $O/$f.o"
done
for f in util decod etiquetas lista dsp motor dibujo vista imagen principal so_pentium; do
  [ -f $AQUI/$f.c ] || continue
  objeto $AQUI/$f.c $O/$f.o -Wall -Wno-unused-function
  OBJ="$OBJ $O/$f.o"
done
nasm -f elf32 $AQUI/entrada.asm -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $AQUI/pentium.ld -o $O/reproduc.elf $O/entrada.o $OBJ
objcopy -O binary $O/reproduc.elf "$SAL"
# nada de después del Pentium (el Atom N455 lo tiene, pero el Pentium de verdad no)
# (sin la cabecera HPU3 de delante, que son datos: objdump los lee como instrucciones)
if objdump -d --start-address=0x40000018 $O/reproduc.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: ¡REPRODUC.BIN lleva CMOV o FCOMI!" >&2; exit 1; fi
echo "hacer.sh: $SAL ($(stat -c %s "$SAL") octetos)"
