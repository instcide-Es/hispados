#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/cxx.sh - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
# sección 16): el «compilador cruzado» de C++ para Hispa-64, como musl-gcc
# para C: el g++ del sistema con las cabeceras de musl (build/musl) y las de
# la libstdc++ del sistema delante, puente.h antes de todo, y al enlazar
# (estático) la libstdc++.a y la libgcc del sistema con la libc.a de musl y
# libpuente.a (puente.c: lo de glibc que pide la libstdc++). Lo que se le
# pase va a g++ tal cual: cxx.sh -O2 -o PROG prog.cpp; cxx.sh -c a.cpp.
# Hace build/cxx-obj/libpuente.a la primera vez.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
M=$B/musl
G=$(dirname "$(g++ -print-libgcc-file-name)")
V=$(g++ -dumpversion | cut -d. -f1)
CI="/usr/include/c++/$V"
CA="/usr/include/x86_64-linux-gnu/c++/$V"
[ -f "$M/lib/libc.a" ] || { echo "cxx.sh: falta musl en $M (hazmusl.sh lo hace)" >&2; exit 2; }
[ -d "$CI" ] && [ -d "$CA" ] || { echo "cxx.sh: no están las cabeceras de la libstdc++ ($CI)" >&2; exit 2; }
INC="-nostdinc -nostdinc++ -isystem $CI -isystem $CA -isystem $M/include -isystem $G/include -include $AQUI/puente.h -fno-stack-protector -D_GNU_SOURCE"
P=$B/cxx-obj/libpuente.a
if [ ! -f "$P" ] || [ "$AQUI/puente.c" -nt "$P" ]; then
  mkdir -p "$B/cxx-obj"
  gcc -O2 -Wall -nostdinc -isystem "$M/include" -isystem "$G/include" -fno-stack-protector -c "$AQUI/puente.c" -o "$B/cxx-obj/puente.o"
  ar rcs "$P" "$B/cxx-obj/puente.o"
fi
enlaza=1
for a in "$@"; do case "$a" in -c|-S|-E|-M|-MM) enlaza=0 ;; esac; done
if [ $enlaza = 0 ]; then exec g++ $INC "$@"; fi
# (newlocale y duplocale, envueltas: el locale_t de glibc que espera la libstdc++, en puente.c)
# (y strerror_r: la de GNU)
ENV="-Wl,--wrap=newlocale -Wl,--wrap=__newlocale -Wl,--wrap=duplocale -Wl,--wrap=__duplocale -Wl,--wrap=strerror_r"
exec g++ $INC -static -nostdlib $ENV "$M/lib/crt1.o" "$M/lib/crti.o" "$G/crtbeginT.o" "$@" \
  -Wl,--start-group "$G/libstdc++.a" "$G/libgcc.a" "$G/libgcc_eh.a" "$P" "$M/lib/libc.a" -Wl,--end-group "$G/crtend.o" "$M/lib/crtn.o"
