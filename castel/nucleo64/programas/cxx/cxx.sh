#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/cxx.sh - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
# sección 16) y [etapa 10 c] (sección 20): el compilador de C++ para
# Hispa-64. Desde la 10 c es el compilador cruzado de verdad,
# x86_64-hispa64-musl-g++ (GCC 14.3.0 con la libstdc++ compilada para musl:
# build/cruzado, que hace hazgcc.sh), y no el g++ del anfitrión con las
# cabeceras de musl y un puente (puente.c, lo de glibc que pedía su
# libstdc++): las pruebas ya no dependen del GCC del sistema.
# Lo que se le pase va a g++ tal cual: cxx.sh -O2 -o PROG prog.cpp;
# cxx.sh -c a.cpp. Al enlazar, estático, salvo con -dinamico (el primer
# argumento): con libstdc++.so.6 y libgcc_s.so.1, y /lib/ld-musl-x86_64.so.1.
# Sin build/cruzado: con HISPA64_PUENTE=1, lo de antes (el g++ del sistema y
# puente.c); si no, dice cómo hacerlo.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
X=${HISPA64_CRUZADO:-$B/cruzado}/bin/x86_64-hispa64-musl-g++
if [ -x "$X" ] && [ "${HISPA64_PUENTE:-0}" != 1 ]; then
  enlaza=1
  for a in "$@"; do case "$a" in -c|-S|-E|-M|-MM) enlaza=0 ;; esac; done
  if [ "${1:-}" = -dinamico ]; then
    shift
    exec "$X" -fno-stack-protector "$@"
  fi
  if [ $enlaza = 0 ]; then exec "$X" -fno-stack-protector "$@"; fi
  exec "$X" -fno-stack-protector -static "$@"
fi
if [ "${HISPA64_PUENTE:-0}" != 1 ]; then
  echo "cxx.sh: falta el compilador cruzado ($X): castel/nucleo64/programas/cxx/hazgcc.sh lo hace (HILOS=12: unos 15 minutos; o su .tar.xz, hecho en otro equipo, en build/)" >&2
  exit 2
fi
# ---- lo de antes de la 10 c: el g++ del sistema con las cabeceras de musl ----
M=$B/musl
G=$(dirname "$(g++ -print-libgcc-file-name)")
V=$(g++ -dumpversion | cut -d. -f1)
CI="/usr/include/c++/$V"
CA="/usr/include/x86_64-linux-gnu/c++/$V"
[ -f "$M/lib/libc.a" ] || { echo "cxx.sh: falta musl en $M (hazmusl.sh lo hace)" >&2; exit 2; }
[ -d "$CI" ] && [ -d "$CA" ] || { echo "cxx.sh: no están las cabeceras de la libstdc++ ($CI)" >&2; exit 2; }
[ "${1:-}" = -dinamico ] && { echo "cxx.sh: -dinamico pide el compilador cruzado" >&2; exit 2; }
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
ENV="-Wl,--wrap=newlocale -Wl,--wrap=__newlocale -Wl,--wrap=duplocale -Wl,--wrap=__duplocale -Wl,--wrap=strerror_r"
exec g++ $INC -static -nostdlib $ENV "$M/lib/crt1.o" "$M/lib/crti.o" "$G/crtbeginT.o" "$@" \
  -Wl,--start-group "$G/libstdc++.a" "$G/libgcc.a" "$G/libgcc_eh.a" "$P" "$M/lib/libc.a" -Wl,--end-group "$G/crtend.o" "$M/lib/crtn.o"
