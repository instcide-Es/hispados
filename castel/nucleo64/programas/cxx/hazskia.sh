#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hazskia.sh - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
# sección 22): Skia m148 (terceros/skia: lo que se compila para dibujar con
# el procesador, sin GPU, con FreeType, HarfBuzz e ICU nuestros) con el
# compilador cruzado, un archivo cada vez, en build/skia-obj/libskia.a (con
# skshaper, skunicode y skcms dentro); solo lo que ha cambiado, y todo otra
# vez si cambia terceros/skia/LEEME.txt o el compilador. Los archivos y sus
# opciones son los de terceros/skia/compila.txt (los que saca el gn de Skia
# con las opciones de terceros/skia/args.gn: cada «@X opciones» y debajo sus
# archivos). Y, en SALIDA, MSKIA (mskia.cpp: su prueba).
# Pide antes libs/hazimg.sh (zlib, libpng, libjpeg-turbo y libwebp) y lo de
# hazcxx.sh (libfreetype.a, libharfbuzz.a y libicu.a), y en SALIDA,
# DEJAVU.TTF, icudt77l.dat y PNGTEST.PNG.
# Uso: hazskia.sh SALIDA   (HILOS=N: N a la vez; si no, de uno en uno: con
# uno, en el servidor, cerca de una hora la primera vez; con 10 en el PC de
# Albinyana, unos 8 minutos). La última línea dice lo que ha hecho.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
T=$R/terceros
K=$T/skia
S=$1
[ -n "$S" ] || { echo "uso: hazskia.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
CXX=$CR/bin/x86_64-hispa64-musl-g++
[ -x "$CXX" ] || { echo "hechos: (sin el compilador cruzado: hazgcc.sh)"; exit 0; }
ID=$("$CXX" --version | head -1)
HILOS=${HILOS:-1}
for l in "$B/zlib-obj/libz.a" "$B/libpng-obj/libpng.a" "$B/libjpeg-obj/libjpeg.a" "$B/libwebp-obj/libwebp.a" \
         "$B/freetype-obj/libfreetype.a" "$B/harfbuzz-obj/libharfbuzz.a" "$B/icu-obj/libicu.a"; do
  [ -f "$l" ] || { echo "hazskia.sh: falta $l (libs/hazimg.sh y hazcxx.sh)" >&2; exit 2; }
done
# las cabeceras de las bibliotecas (lo que en compila.txt es $INC)
INC="-I$T/zlib -I$T/libpng -I$T/libjpeg-turbo/src -I$T/libjpeg-turbo/config -I$T/libwebp/src"
O=$B/skia-obj
if [ "$(cat "$O/.compilador" 2> /dev/null)" != "$ID" ] || [ "$K/LEEME.txt" -nt "$O/libskia.a" ]; then rm -rf "$O"; fi
mkdir -p "$O"
L=$O/.lista
: > "$L"
op=""
cambio=0
while IFS= read -r l; do
  case "$l" in
    @X\ *) op=${l#@X }
           op=${op//\$INC/$INC}; op=${op//\$K/$K}; op=${op//\$T/$T}
           # (lo del anfitrión, nunca: gn pone -I/usr/include/freetype2 y harfbuzz)
           op=$(echo "$op" | sed 's| -I/usr/[^ ]*||g') ;;
    "") ;;
    *) ob=$O/$(echo "$l" | tr / _ | sed 's/\.\(cpp\|cc\|c\)$/.o/')
       if [ ! -f "$ob" ] || [ "$K/$l" -nt "$ob" ]; then
         echo "$CXX $op -c $K/$l -o $ob" >> "$L"
         cambio=1
       fi ;;
  esac
done < "$K/compila.txt"
if [ -s "$L" ]; then
  echo "hazskia.sh: $(wc -l < "$L") archivos de Skia que compilar, con $HILOS a la vez" >&2
  # shellcheck disable=SC2016
  xargs -d '\n' -P "$HILOS" -I{} sh -c 'nice {} || { echo "hazskia.sh: falla: {}" >&2; exit 255; }' < "$L"
fi
if [ $cambio = 1 ] || [ ! -f "$O/libskia.a" ]; then rm -f "$O/libskia.a"; "$CR/bin/x86_64-hispa64-musl-ar" rcs "$O/libskia.a" "$O"/*.o; fi
echo "$ID" > "$O/.compilador"
mkdir -p "$S"
# MSKIA, con las mismas definiciones públicas que Skia
PUB="-DNDEBUG -DSK_R32_SHIFT=16 -DSK_GANESH -DSK_DISABLE_TRACING -DSK_GAMMA_APPLY_TO_A8 -DSK_TYPEFACE_FACTORY_FREETYPE \
     -DSK_SHAPER_PRIMITIVE_AVAILABLE -DSK_SHAPER_HARFBUZZ_AVAILABLE -DSK_SHAPER_UNICODE_AVAILABLE -DSK_UNICODE_AVAILABLE -DSK_UNICODE_ICU_IMPLEMENTATION"
# shellcheck disable=SC2086
nice "$CXX" -std=c++20 -O2 -Wall -Wno-attributes -fno-stack-protector -static -pthread $PUB -I"$K" -I"$T/zlib" \
  -I"$T/icu/source/common" -DU_STATIC_IMPLEMENTATION -DU_USING_ICU_NAMESPACE=0 -o "$S/MSKIA" "$AQUI/mskia.cpp" \
  "$O/libskia.a" "$B/harfbuzz-obj/libharfbuzz.a" "$B/freetype-obj/libfreetype.a" "$B/icu-obj/libicu.a" \
  "$B/libwebp-obj/libwebp.a" "$B/libjpeg-obj/libjpeg.a" "$B/libpng-obj/libpng.a" "$B/zlib-obj/libz.a" -lm
"$CR/bin/x86_64-hispa64-musl-strip" --strip-unneeded "$S/MSKIA"
echo "hechos: MSKIA"
