#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/tls/hacer.sh SALIDA
# Hace TLS.BIN, el servicio TLS de Hispa-Pentium (docs/PENTIUM.md, «HTTPS: el
# servicio TLS»): tls.c y libc.c de aquí con BearSSL 0.6 (terceros/bearssl,
# licencia MIT; los mismos fuentes que HSPTLS.DLL de Hispa-486, compilados
# para Pentium) y las autoridades raíz de HSPTLS (w32/tls/anclas.h),
# enlazado para 0x40000000 como programa del anillo 3 (como ACPI.BIN,
# acpi/pentium/hacer.sh). Los objetos, en build/tls-pentium/ (solo lo que
# ha cambiado). Necesita gcc (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh SALIDA}")
cd "$RAIZ"
O=build/tls-pentium
mkdir -p $O/bearssl
# el día del último cambio de tls.c (para que el binario no cambie con el día en que se hace):
# la hora del PC que diga una fecha de antes no se cree (tls.c, «LA HORA»)
DIA=$(git log -1 --format=%cd --date=format:%Y%m%d -- castel/nucleo32/tls/tls.c 2> /dev/null || true)
[ -n "$DIA" ] || DIA=$(date +%Y%m%d)
BF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections
    -Iw32/inc -Iterceros/bearssl/inc -Iterceros/bearssl/src
    -DBR_USE_URANDOM=0 -DBR_USE_UNIX_TIME=0 -DBR_USE_WIN32_TIME=0 -DBR_USE_WIN32_RAND=0 -DBR_USE_GETENTROPY=0
    -DBR_SSE2=0 -DBR_AES_X86NI=0 -DBR_RDRAND=0 -DBR_LE_UNALIGNED=1 -DBR_64=0 -DBR_INT128=0 -DBR_UMUL128=0 -DBR_POWER8=0"
for f in terceros/bearssl/src/*.c; do
  o=$O/bearssl/$(basename $f .c).o
  if [ ! -f $o ] || [ $f -nt $o ]; then
    # shellcheck disable=SC2086
    gcc $BF -c $f -o $o
  fi
done
# shellcheck disable=SC2086
gcc $BF -fexec-charset=CP437 -Wall -Wno-unused -DCOMPILADO=$DIA -Iw32/tls -Icastel/nucleo32/tls -c castel/nucleo32/tls/tls.c -o $O/tls.o
# shellcheck disable=SC2086
gcc $BF -Wall -Icastel/nucleo32/tls -c castel/nucleo32/tls/libc.c -o $O/libc.o
nasm -f elf32 castel/nucleo32/tls/entrada.asm -o $O/entrada.o
ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T castel/nucleo32/tls/pentium.ld -o $O/tls.elf \
  $O/entrada.o $O/tls.o $O/libc.o $O/bearssl/*.o
objcopy -O binary $O/tls.elf "$SAL"
