#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/juegos/hacer.sh CARPETA
# (JUEGOS) Hace los juegos de Hispa-Pentium (docs/PENTIUM.md, «Los juegos»):
# deja en CARPETA BLOQUES.BIN, ROMPEMUR.BIN, PINBALL.BIN, CULEBRA.BIN,
# MINAS.BIN, SOLITARI.BIN, TRAGABOL.BIN y ALMACEN.BIN, programas del anillo 3
# (en C, con la entrada y la biblioteca de C de Comprimidos: ../comprim/
# entrada.asm, pentium.ld y base.c) enlazados para 0x40000000; y, al lado de
# CARPETA, el acceso del menú Inicio de cada uno (X.MNU, «tipo=Juegos», y su
# icono, X.ICP). Con JUEGOS_CARPETA=\OTRA, los .MNU dicen que los programas
# están allí (si no, en \HISPA\JUEGOS). Necesita gcc (con -m32), nasm, ld,
# objcopy y python3. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
mkdir -p "${1:?uso: hacer.sh CARPETA}"
SAL=$(readlink -f "$1")
cd "$RAIZ"
O=build/juegos-pentium
mkdir -p $O
C=castel/nucleo32/comprim
J=castel/nucleo32/juegos
python3 -I $J/datos386.py $O/datos386.h $O/letra.h
# (como mucho -march=i586: nada de CMOV ni de FCOMI, que el Pentium no tiene)
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -I$C/inc -I$C -I$J -I$O"
for f in juego dibujo sintes; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -Wextra -Wno-unused-parameter -c $J/$f.c -o $O/$f.o
done
# shellcheck disable=SC2086
gcc $BF -Wall -c $C/base.c -o $O/base.o
nasm -f elf32 -DPRINCIPAL=principal $C/entrada.asm -o $O/entrada.o
COMUN="$O/entrada.o $O/juego.o $O/dibujo.o $O/sintes.o $O/base.o"
# el juego: su nombre en el disco, los fuentes, su nombre en el menú y su icono
JUEGOS=(
  "BLOQUES:bloques:Bloques:bloques"
  "ROMPEMUR:rompemur:Rompemuros:rompemur"
  "PINBALL:pinball pinfis pinmesa:Hispa-Pinball:pinball"
  "CULEBRA:culebra:Culebra:culebra"
  "MINAS:minas:Campo de minas:minas"
  "SOLITARI:solitar:Solitario:solitario"
  "TRAGABOL:tragabol:Tragabolas:tragabolas"
  "ALMACEN:almacen:Almac\x82n:almacen"
)
DONDE=${JUEGOS_CARPETA:-\\HISPA\\JUEGOS}
for j in "${JUEGOS[@]}"; do
  IFS=: read -r B FUENTES NOMBRE ICO <<< "$j"
  [ -f $J/${FUENTES%% *}.c ] || continue      # (los que aún no están)
  OBJS=
  for f in $FUENTES; do
    # shellcheck disable=SC2086
    gcc $BF -Wall -Wextra -Wno-unused-parameter -c $J/$f.c -o $O/$f.o
    OBJS="$OBJS $O/$f.o"
  done
  # shellcheck disable=SC2086
  ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $C/pentium.ld -o $O/$B.elf $COMUN $OBJS
  objcopy -O binary $O/$B.elf "$SAL/$B.BIN"
  if objdump -d $O/$B.elf | grep -qE '\scmov|\sfcomi|\sfucomi'; then echo "hacer.sh: $B.BIN lleva instrucciones de después del Pentium"; exit 1; fi
  python3 -I $J/hazicono.py $ICO "$SAL/../$B.ICP"
  printf "nombre=$NOMBRE\r\nprograma=%s\\\\$B.BIN\r\nargumentos=\r\nicono=%s\\\\$B.ICP\r\nescritorio=no\r\ntipo=Juegos\r\n" \
    "$DONDE" "${JUEGOS_ICONOS:-\\HISPA\\MENU}" > "$SAL/../$B.MNU"
done
