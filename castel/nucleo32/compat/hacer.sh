#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/compat/hacer.sh SALIDA
# (COMPAT) Hace COMPAT.BIN, el Asistente de compatibilidad de Hispa-Pentium
# (docs/PENTIUM.md, «El Asistente de compatibilidad»): el C portable de
# compat/ con la biblioteca común de la oficina (oficina/comun, con
# so_pentium.c) y compat/plat_pentium.c, miniz (terceros/miniz, MIT) y las
# DejaVu, enlazado para 0x40000000 como programa del anillo 3.
# -march=i586: nada de CMOV. Necesita gcc (con -m32), nasm, ld y objcopy.
# Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
OFI=$(readlink -f "$AQUI/../oficina")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -m "${1:?uso: hacer.sh SALIDA}")
mkdir -p "$(dirname "$SAL")"
O=$RAIZ/build/compat-pentium
mkdir -p $O
python3 -I "$OFI/comun/haz_letras.py" "$RAIZ/terceros/dejavu" $O/letras_dejavu.c
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -DOFI_PENTIUM
    -I$OFI/comun/inc -I$OFI/comun -I$AQUI -I$RAIZ/terceros/miniz -I$RAIZ/terceros/stb"
OBJS=""
while read -r c w; do
	case "$c" in ""|\#*) continue;; esac
	case "$c" in comun/*) f=$OFI/$c.c ;; compat/*) f=$AQUI/${c#compat/}.c ;; esac
	o=$O/$(echo $c | tr / _).o
	if [ "$w" = "-w" ]; then W=-w; else W="-Wall -Wno-unused-parameter -Wno-sign-compare -Wno-unused-function"; fi
	# shellcheck disable=SC2086
	gcc $BF $W -c "$f" -o $o
	OBJS="$OBJS $o"
done < "$AQUI/fuentes.lst"
# shellcheck disable=SC2086
gcc $BF -w -c $OFI/comun/so_pentium.c -o $O/so_pentium.o
# shellcheck disable=SC2086
gcc $BF -Wall -Wno-unused-parameter -c $AQUI/plat_pentium.c -o $O/plat_pentium.o
# shellcheck disable=SC2086
gcc $BF -w -c $O/letras_dejavu.c -o $O/letras_dejavu.o
nasm -f elf32 "$OFI/comun/entrada.asm" -o $O/entrada.o
# shellcheck disable=SC2086
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T "$OFI/comun/pentium.ld" -o $O/compat.elf \
  $O/entrada.o $OBJS $O/so_pentium.o $O/plat_pentium.o $O/letras_dejavu.o
objcopy -O binary $O/compat.elf "$SAL"
if objdump -d $O/compat.elf | grep -qE '\scmov'; then echo "hacer.sh: ¡hay CMOV!" >&2; exit 1; fi
T=$(stat -c %s "$SAL")
MAX=$(( $(sed -n 's/^ *constante MAX_PROGRAMA = \(0x[0-9A-Fa-f]*\).*/\1/p' $RAIZ/castel/nucleo32/usuario.cas) ))
[ "$T" -le "$MAX" ] || { echo "hacer.sh: COMPAT.BIN mide $T y el núcleo no carga más de $MAX" >&2; exit 1; }
echo "hacer.sh: $SAL ($T octetos)"
