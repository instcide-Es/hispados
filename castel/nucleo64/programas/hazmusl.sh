#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/hazmusl.sh - Hispa-64, etapa 7 (docs/64/HISPA-64.md,
# sección 13): musl (terceros/musl) compilado con el gcc del sistema en el
# árbol de construcción (build/musl: nada se instala en el sistema; con
# make -j1, y solo la primera vez), Lua (terceros/lua, en build/lua-obj) y
# los programas de prueba de musl (programas/musl/), en la carpeta SALIDA:
#   MHOLA, MHOLAP, MHOLAD       estático, estático de posición libre y
#                               dinámico (con libsaludo.so)
#   MHILOS(D), MSENALES(D), MPROCESOS(D), MMAPA(D)   estático y dinámico
#   MBUCLE                      da vueltas sin llamar al sistema (Ctrl+C)
#   ld-musl-x86_64.so.1         el cargador dinámico (la libc.so de musl):
#                               en Hispa-64, /lib/ld-musl-x86_64.so.1
#   libsaludo.so                una biblioteca compartida de prueba
#   LUA, PRUEBA.LUA             Lua 5.5 y su prueba
#   PING IFCONFIG DESCARGA NTP ECO MRED   los de red (hazred.sh, etapa 9)
#   MCXX                        la prueba de C++ (cxx/hazcxx.sh, etapa 10)
# Uso: hazmusl.sh SALIDA   (dice en la última línea los que ha hecho; si no
# hay gcc, no hace nada)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../.." && pwd)          # la raíz del repositorio
S=$1
[ -n "$S" ] || { echo "uso: hazmusl.sh SALIDA" >&2; exit 2; }
command -v gcc > /dev/null || { echo "hechos: (sin gcc)"; exit 0; }
mkdir -p "$S"
B=$R/build
M=$B/musl/bin/musl-gcc
if [ ! -x "$M" ] || [ ! -f "$B/musl/lib/libc.a" ]; then
  echo "musl: se compila en $B/musl (make -j1: unos minutos)"
  mkdir -p "$B/musl-obj"
  (cd "$B/musl-obj" && "$R/terceros/musl/configure" --prefix="$B/musl" --syslibdir="$B/musl/lib" CC=gcc --enable-wrapper=gcc > configure.log 2>&1 \
     && make -j1 > make.log 2>&1 && make -j1 install > install.log 2>&1)
fi
if [ ! -x "$B/lua-obj/lua" ] || [ "$R/terceros/lua/src/lua.c" -nt "$B/lua-obj/lua" ]; then
  rm -rf "$B/lua-obj"
  mkdir -p "$B/lua-obj"
  cp "$R/terceros/lua/src/"* "$B/lua-obj/"
  make -C "$B/lua-obj" -j1 linux CC="$M -std=gnu99" MYLDFLAGS="-static" > "$B/lua-obj/make.log" 2>&1
fi
P=$AQUI/musl
CF="-O2 -Wall -Wno-unused-result -fno-stack-protector"
DIN="-Wl,-dynamic-linker,/lib/ld-musl-x86_64.so.1"
T=$(mktemp -d "$S/.musl.XXXXXX")
"$M" $CF -shared -fPIC -o "$T/libsaludo.so" "$P/libsaludo.c"
"$M" $CF -static -o "$S/MHOLA" "$P/mhola.c"
# (estático de posición libre: el musl-gcc de musl no sabe de -static-pie; a mano, con rcrt1.o)
"$M" $CF -fPIE -nostartfiles -Wl,-static -Wl,-pie -Wl,--no-dynamic-linker -Wl,-z,text -o "$S/MHOLAP" \
    "$B/musl/lib/rcrt1.o" "$B/musl/lib/crti.o" "$(gcc -print-file-name=crtbeginS.o)" "$P/mhola.c" \
    "$(gcc -print-file-name=crtendS.o)" "$B/musl/lib/crtn.o"
"$M" $CF -DCON_LIB -o "$S/MHOLAD" "$P/mhola.c" -L"$T" -lsaludo $DIN
for p in mhilos msenales mprocesos mmapa; do
  N=$(echo $p | tr a-z A-Z)
  "$M" $CF -static -o "$S/$N" "$P/$p.c"
  "$M" $CF -o "$S/${N}D" "$P/$p.c" $DIN
done
"$M" $CF -static -o "$S/MBUCLE" "$P/mbucle.c"
"$M" $CF -static -o "$S/MDISP" "$P/mdisp.c"          # [etapa 8] los dispositivos
cp "$T/libsaludo.so" "$S/libsaludo.so"
cp "$B/musl/lib/libc.so" "$S/ld-musl-x86_64.so.1"
cp "$B/lua-obj/lua" "$S/LUA"
cp "$P/prueba.lua" "$S/PRUEBA.LUA"
rm -rf "$T"
for f in MHOLA MHOLAP MHOLAD MHILOS MHILOSD MSENALES MSENALESD MPROCESOS MPROCESOSD MMAPA MMAPAD MBUCLE MDISP LUA libsaludo.so ld-musl-x86_64.so.1; do
  strip --strip-unneeded "$S/$f"
done
RED=$("$AQUI/hazred.sh" "$S"); RED=${RED##*hechos: }   # [etapa 9] los programas de red (programas/red/)
CXX=$("$AQUI/cxx/hazcxx.sh" "$S"); CXX=${CXX##*hechos: }   # [etapa 10] los de C++ (programas/cxx/)
echo "hechos: MHOLA MHOLAP MHOLAD MHILOS MHILOSD MSENALES MSENALESD MPROCESOS MPROCESOSD MMAPA MMAPAD MBUCLE MDISP ld-musl-x86_64.so.1 libsaludo.so LUA PRUEBA.LUA $RED $CXX"
