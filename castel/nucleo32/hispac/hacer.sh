#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo32/hispac/hacer.sh CARPETA
# Hace, en CARPETA, los programas en C del anillo 3 de Hispa-Pentium que
# hablan SSH, WireGuard y Tailscale (docs/PENTIUM.md, «SSH» y «Tailscale»):
#   SSH.BIN        el cliente SSH (ssh/sshc.c)
#   SSHD.BIN       el servidor SSH (ssh/sshd.c)
#   WG.BIN         un túnel WireGuard, como wg-quick (wg/wgmain.c)
#   TAILSCAL.BIN   el cliente de Tailscale (tailscale/ts.c, con wg/wg.c; la orden TAILSCALE)
# Con lo común de hispac/ (hispa.c, cripto.c), la libc de TLS.BIN
# (tls/libc.c) y Monocypher 4.0.2 (terceros/monocypher, BSD-2 o CC0),
# enlazados para 0x40000000 como programas del anillo 3 (como TLS.BIN). Los
# objetos, en build/hispac-pentium/ (solo lo que ha cambiado). Necesita gcc
# (con -m32), nasm, ld y objcopy. Sin «make -j».
set -e
AQUI=$(dirname "$(readlink -f "$0")")
RAIZ=$(readlink -f "$AQUI/../../..")
SAL=$(readlink -f "${1:?uso: hacer.sh CARPETA}")
mkdir -p "$SAL"
cd "$RAIZ"
O=build/hispac-pentium
mkdir -p $O
N=castel/nucleo32
HC=$N/hispac
CF="-m32 -std=gnu11 -march=i586 -O2 -ffreestanding -fno-pic -fno-stack-protector -fno-asynchronous-unwind-tables
    -fno-builtin -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -Wall -Wno-unused
    -I$HC -Iterceros/monocypher -I$N/ssh -I$N/wg -I$N/tailscale"
CABECERAS="$HC/hispa.h $HC/cripto.h $N/ssh/ssh.h $N/wg/wg.h"
[ -f $N/tailscale/ts.h ] && CABECERAS="$CABECERAS $N/tailscale/ts.h"
# objeto(fuente, objeto, opciones de más): solo si ha cambiado (o alguna cabecera)
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
objeto terceros/monocypher/monocypher-ed25519.c $O/monocypher-ed25519.o -Wno-all
objeto $N/tls/libc.c $O/libc.o -I$N/tls
objeto $HC/hispa.c $O/hispa.o -fexec-charset=CP437
objeto $HC/cripto.c $O/cripto.o
for f in ssh/ssh ssh/sshd ssh/sshc wg/wg wg/wgmain; do
  objeto $N/$f.c $O/$(basename $f).o -fexec-charset=CP437
done
nasm -f elf32 $HC/entrada.asm -o $O/entrada.o
COMUN="$O/entrada.o $O/hispa.o $O/cripto.o $O/libc.o $O/monocypher.o $O/monocypher-ed25519.o"
LIBGCC=$(gcc -m32 -print-libgcc-file-name)
enlaza() {
  local nombre=$1
  shift
  # shellcheck disable=SC2086
  ld -m elf_i386 --no-warn-rwx-segments -z noexecstack --gc-sections -T $HC/pentium.ld -o $O/$nombre.elf $COMUN "$@" "$LIBGCC"
  objcopy -O binary $O/$nombre.elf "$SAL/$nombre.BIN"
}
enlaza SSHD $O/ssh.o $O/sshd.o
enlaza SSH $O/ssh.o $O/sshc.o
enlaza WG $O/wg.o $O/wgmain.o
if [ -f $N/tailscale/ts.c ]; then
  for f in ts json derp control; do
    [ -f $N/tailscale/$f.c ] && objeto $N/tailscale/$f.c $O/ts_$f.o -fexec-charset=CP437
  done
  enlaza TAILSCAL $O/wg.o $O/ts_*.o
fi
