#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hazcxx.sh - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
# sección 16): los programas de C++ y de la letra, con cxx.sh (la libstdc++
# del sistema con musl), en la carpeta SALIDA:
#   MCXX        la prueba de C++17 (mcxx.cpp): excepciones, RTTI,
#               contenedores, flujos, regex, cientos de hilos, thread_local...
#   MTEXTO      HarfBuzz sobre FreeType (mtexto.c), con DEJAVU.TTF
#   DEJAVU.TTF  DejaVu Sans entera (terceros/dejavu/DejaVuSans.ttf)
# FreeType (terceros/freetype, en C: con musl-gcc) y HarfBuzz
# (terceros/harfbuzz, en C++: con cxx.sh) se compilan en build/ la primera
# vez (libfreetype.a y libharfbuzz.a), de uno en uno.
# Uso: hazcxx.sh SALIDA   (dice en la última línea los que ha hecho; sin g++
# o sin las cabeceras de la libstdc++, ninguno)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
S=$1
[ -n "$S" ] || { echo "uso: hazcxx.sh SALIDA" >&2; exit 2; }
command -v g++ > /dev/null || { echo "hechos: (sin g++)"; exit 0; }
"$AQUI/cxx.sh" -std=c++17 -O2 -Wall -o "$S/MCXX" "$AQUI/mcxx.cpp"
strip --strip-unneeded "$S/MCXX"
# FreeType (los módulos de siempre, uno por archivo)
FT=$R/terceros/freetype
LF=$B/freetype-obj/libfreetype.a
if [ ! -f "$LF" ] || [ "$FT/LEEME.txt" -nt "$LF" ]; then
  rm -rf "$B/freetype-obj"; mkdir -p "$B/freetype-obj"
  for f in base/ftsystem.c base/ftinit.c base/ftdebug.c base/ftbase.c base/ftbbox.c base/ftbdf.c base/ftbitmap.c base/ftcid.c \
           base/ftfstype.c base/ftgasp.c base/ftglyph.c base/ftgxval.c base/ftmm.c base/ftotval.c base/ftpatent.c base/ftpfr.c \
           base/ftstroke.c base/ftsynth.c base/fttype1.c base/ftwinfnt.c autofit/autofit.c bdf/bdf.c cff/cff.c cid/type1cid.c \
           gzip/ftgzip.c lzw/ftlzw.c pcf/pcf.c pfr/pfr.c psaux/psaux.c pshinter/pshinter.c psnames/psnames.c raster/raster.c \
           sdf/sdf.c sfnt/sfnt.c smooth/smooth.c svg/svg.c truetype/truetype.c type1/type1.c type42/type42.c winfonts/winfnt.c; do
    "$B/musl/bin/musl-gcc" -O2 -fno-stack-protector -DFT2_BUILD_LIBRARY -I"$FT/include" -c "$FT/src/$f" -o "$B/freetype-obj/$(echo "$f" | tr / _ | sed 's/\.c$/.o/')"
  done
  ar rcs "$LF" "$B/freetype-obj/"*.o
fi
# HarfBuzz (de una vez: harfbuzz.cc), con FreeType y los hilos de pthread
HB=$R/terceros/harfbuzz
LH=$B/harfbuzz-obj/libharfbuzz.a
if [ ! -f "$LH" ] || [ "$HB/LEEME.txt" -nt "$LH" ]; then
  rm -rf "$B/harfbuzz-obj"; mkdir -p "$B/harfbuzz-obj"
  "$AQUI/cxx.sh" -std=c++17 -O2 -fno-exceptions -fno-rtti -fno-threadsafe-statics -DHAVE_FREETYPE -DHAVE_PTHREAD -I"$FT/include" \
    -c "$HB/src/harfbuzz.cc" -o "$B/harfbuzz-obj/harfbuzz.o"
  ar rcs "$LH" "$B/harfbuzz-obj/harfbuzz.o"
fi
"$AQUI/cxx.sh" -x c -O2 -Wall -I"$FT/include" -I"$HB/src" -c "$AQUI/mtexto.c" -o "$B/harfbuzz-obj/mtexto.o" 2> /dev/null
"$AQUI/cxx.sh" -o "$S/MTEXTO" "$B/harfbuzz-obj/mtexto.o" "$LH" "$LF"
strip --strip-unneeded "$S/MTEXTO"
cp "$R/terceros/dejavu/DejaVuSans.ttf" "$S/DEJAVU.TTF"
echo "hechos: MCXX MTEXTO DEJAVU.TTF"
