#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/escudo/hacer.sh CARPETA
# nucleo32/escudo/hacer.sh --anfitrion [SALIDA]
# Hace Escudo, el antivirus de Hispa-Pentium (docs/PENTIUM.md, «Escudo»), y
# deja en CARPETA lo que va en \HISPA: ESCUDO.BIN (el motor, las órdenes y la
# ventana: un programa del anillo 3, en C), ESCUDO\LEEME.TXT y, en
# CARPETA/MENU, el acceso del menú Inicio (ESCUDO.MNU y su icono, ESCUDO.ICP).
# Con --anfitrion, el mismo motor para Linux (escudo-anfitrion: las pruebas y
# hacer FIRMAS.ESC en el VPS, tools/escudo_firmas.py).
# Lleva de BearSSL (terceros/bearssl, licencia MIT) solo la cuenta de RSA de
# la firma de los .cvd. Los objetos, en build/escudo-pentium/ (y
# build/escudo-anfitrion/). Necesita gcc (con -m32), nasm, ld, objcopy y
# python3. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
BEAR="rsa_i31_pub i31_decode i31_encode i31_bitlen i31_ninv31 i31_modpow2 i31_modpow i31_montmul i31_tmont i31_fmont i31_muladd i31_sub i31_add i31_iszero i31_decmod i31_rshift i32_div32 ccopy"
MOTOR="motor inflar cvd sumas"
cd "$RAIZ"
if [ "${1:-}" = --anfitrion ]; then
  SAL=$(readlink -m "${2:-build/escudo-anfitrion/escudo-anfitrion}")
  O=build/escudo-anfitrion
  mkdir -p $O
  for f in $BEAR; do
    [ -f $O/$f.o ] && [ $O/$f.o -nt terceros/bearssl/src/$f.c ] || gcc -O2 -c -Iterceros/bearssl/inc -Iterceros/bearssl/src terceros/bearssl/src/$f.c -o $O/$f.o
  done
  objs=
  for f in $BEAR; do objs="$objs $O/$f.o"; done
  # shellcheck disable=SC2086
  gcc -O2 -g -Wall -Wno-unused-parameter -DESCUDO_ANFITRION -I$AQUI -Iterceros/bearssl/inc $(for f in $MOTOR anfitrion; do echo $AQUI/$f.c; done) $objs -lz -o "$SAL" \
    || { echo "hacer.sh: no se ha podido hacer escudo-anfitrion (hace falta gcc y la zlib: zlib1g-dev)"; exit 1; }
  echo "hacer.sh: $SAL"
  exit 0
fi
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
O=build/escudo-pentium
mkdir -p $O
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections"
for f in $BEAR; do
  o=$O/$f.o
  if [ ! -f $o ] || [ terceros/bearssl/src/$f.c -nt $o ]; then
    # shellcheck disable=SC2086
    gcc $BF -Iw32/inc -Iterceros/bearssl/inc -Iterceros/bearssl/src -DBR_LE_UNALIGNED=1 -DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -c terceros/bearssl/src/$f.c -o $o
  fi
done
# la letra del escritorio (letra.cas: 256 letras de 8x16), para la ventana
python3 -I - castel/nucleo32/letra.cas $O/letra.h <<'PY'
import re, sys
t = open(sys.argv[1], encoding='utf-8').read()
t = t[t.index('HP_letra8x16:'):]
v = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', t)][:4096]
assert len(v) == 4096, len(v)
with open(sys.argv[2], 'w') as f:
    f.write('/* hecho por nucleo32/escudo/hacer.sh de castel/nucleo32/letra.cas: no se edita */\n')
    f.write('static const unsigned char letra8x16[4096] = {\n')
    for i in range(0, 4096, 16):
        f.write('  ' + ','.join('0x%02x' % x for x in v[i:i + 16]) + ',\n')
    f.write('};\n')
PY
for f in $MOTOR plataforma escudo ventana; do
  # shellcheck disable=SC2086
  gcc $BF -Wall -Wno-unused-parameter -I$AQUI -I$O -Iw32/inc -Iterceros/bearssl/inc -c $AQUI/$f.c -o $O/$f.o
done
nasm -f elf32 $AQUI/entrada.asm -o $O/entrada.o
# shellcheck disable=SC2046
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $AQUI/pentium.ld -o $O/escudo.elf \
  $O/entrada.o $(for f in $MOTOR plataforma escudo ventana; do echo $O/$f.o; done) $(for f in $BEAR; do echo $O/$f.o; done)
objcopy -O binary $O/escudo.elf $O/ESCUDO.BIN
mkdir -p "$SAL/ESCUDO" "$SAL/MENU"
cp $O/ESCUDO.BIN "$SAL/"
iconv -f UTF-8 -t CP437 $AQUI/LEEME.txt | sed 's/$/\r/' > "$SAL/ESCUDO/LEEME.TXT"
python3 -I tools/hpicono.py $AQUI/escudo32.png "$SAL/MENU/ESCUDO.ICP"
printf 'nombre=Escudo (antivirus)\r\nprograma=%s\r\nargumentos=/VENTANA\r\nicono=%s\r\nescritorio=no\r\n' \
  "${ESCUDO_PROGRAMA:-\\HISPA\\ESCUDO.BIN}" "${ESCUDO_ICONO:-\\HISPA\\MENU\\ESCUDO.ICP}" > "$SAL/MENU/ESCUDO.MNU"
