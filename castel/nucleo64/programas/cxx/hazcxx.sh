#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hazcxx.sh - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
# sección 16): los programas de C++ y de la letra, con cxx.sh ([etapa 10 c]
# el compilador cruzado, con la libstdc++ compilada para musl), en SALIDA:
#   MCXX        la prueba de C++17 (mcxx.cpp): excepciones, RTTI,
#               contenedores, flujos, regex, cientos de hilos, thread_local...
#   MTEXTO      HarfBuzz sobre FreeType (mtexto.c), con DEJAVU.TTF
#   DEJAVU.TTF  DejaVu Sans entera (terceros/dejavu/DejaVuSans.ttf)
#   MICU        [etapa 10 b] la prueba de ICU 77.1 (micu.cpp), con
#   icudt77l.dat sus datos (terceros/icu/data: es, ca, gl, eu, en)
# FreeType (terceros/freetype, en C: con musl-gcc) y HarfBuzz
# (terceros/harfbuzz, en C++: con cxx.sh) se compilan en build/ la primera
# vez (libfreetype.a y libharfbuzz.a), de uno en uno; e ICU (terceros/icu,
# en C++: libicu.a, common, i18n y stubdata), un archivo cada vez, solo lo
# que ha cambiado.
#   MCXXD       [etapa 10 c] MCXX dinámico, con libstdc++.so.6 y libgcc_s.so.1
#   MIPC        [etapa 10 d] dos procesos que se hablan como los de Ladybird
#               (C++23: AF_UNIX, SCM_RIGHTS, memfd, posix_spawn, poll)
# Uso: hazcxx.sh SALIDA   (dice en la última línea los que ha hecho; [etapa
# 10 c] sin el compilador cruzado, build/cruzado de hazgcc.sh, ninguno)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
S=$1
[ -n "$S" ] || { echo "uso: hazcxx.sh SALIDA" >&2; exit 2; }
# [etapa 10 c] con el compilador cruzado (cxx.sh; sin él, nada)
ID=$("$AQUI/cxx.sh" --version 2> /dev/null | head -1) || { echo "hechos: (sin el compilador cruzado: hazgcc.sh)"; exit 0; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
TIRA=strip
[ -x "$CR/bin/x86_64-hispa64-musl-strip" ] && TIRA=$CR/bin/x86_64-hispa64-musl-strip
# (lo de C++ ya compilado con otro compilador, otra vez)
for o in "$B/harfbuzz-obj" "$B/icu-obj"; do
  if [ -d "$o" ] && [ "$(cat "$o/.compilador" 2> /dev/null)" != "$ID" ]; then rm -rf "$o"; fi
done
"$AQUI/cxx.sh" -std=c++17 -O2 -Wall -o "$S/MCXX" "$AQUI/mcxx.cpp"
$TIRA --strip-unneeded "$S/MCXX"
"$AQUI/cxx.sh" -std=c++23 -O2 -Wall -pthread -o "$S/MIPC" "$AQUI/mipc.cpp"
$TIRA --strip-unneeded "$S/MIPC"
# [etapa 10 c] C++ dinámico: MCXXD, la misma prueba con libstdc++.so.6 y
# libgcc_s.so.1 (en Hispa-64, en /lib: la unidad en memoria), con ld-musl
if [ -f "$CR/x86_64-hispa64-linux-musl/lib64/libstdc++.so.6" ]; then
  "$AQUI/cxx.sh" -dinamico -std=c++17 -O2 -Wall -o "$S/MCXXD" "$AQUI/mcxx.cpp"
  $TIRA --strip-unneeded "$S/MCXXD"
  cp -L "$CR/x86_64-hispa64-linux-musl/lib64/libstdc++.so.6" "$CR/x86_64-hispa64-linux-musl/lib64/libgcc_s.so.1" "$S/"
  DIN=" MCXXD libstdc++.so.6 libgcc_s.so.1"
fi
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
  echo "$ID" > "$B/harfbuzz-obj/.compilador"
fi
"$AQUI/cxx.sh" -x c -O2 -Wall -I"$FT/include" -I"$HB/src" -c "$AQUI/mtexto.c" -o "$B/harfbuzz-obj/mtexto.o" 2> /dev/null
"$AQUI/cxx.sh" -o "$S/MTEXTO" "$B/harfbuzz-obj/mtexto.o" "$LH" "$LF"
$TIRA --strip-unneeded "$S/MTEXTO"
cp "$R/terceros/dejavu/DejaVuSans.ttf" "$S/DEJAVU.TTF"
# [etapa 10 b] ICU: common, i18n y stubdata (los datos van aparte, en
# icudt77l.dat); un archivo cada vez, solo lo que ha cambiado
IC=$R/terceros/icu/source
OI=$B/icu-obj
mkdir -p "$OI"
echo "$ID" > "$OI/.compilador"
IF="-std=c++17 -O2 -fno-stack-protector -DU_STATIC_IMPLEMENTATION -DU_ATTRIBUTE_DEPRECATED= -DU_CHARSET_IS_UTF8=1 -DU_USING_ICU_NAMESPACE=0 -DU_HAVE_STRTOD_L=0 -I$IC/common"
cambio=0
for d in common i18n stubdata; do
  case $d in common) DF="-DU_COMMON_IMPLEMENTATION" ;; i18n) DF="-DU_I18N_IMPLEMENTATION -I$IC/i18n" ;; *) DF="-DU_COMMON_IMPLEMENTATION -I$IC/stubdata" ;; esac
  for f in "$IC/$d"/*.cpp; do
    o=$OI/${d}_$(basename "$f" .cpp).o
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
      # shellcheck disable=SC2086
      nice "$AQUI/cxx.sh" $IF $DF -c "$f" -o "$o"
      cambio=1
    fi
  done
done
if [ $cambio = 1 ] || [ ! -f "$OI/libicu.a" ]; then rm -f "$OI/libicu.a"; ar rcs "$OI/libicu.a" "$OI"/*.o; fi
nice "$AQUI/cxx.sh" -std=c++17 -O2 -Wall -Wno-unused-function -DU_STATIC_IMPLEMENTATION -DU_USING_ICU_NAMESPACE=0 -I"$IC/common" -I"$IC/i18n" -o "$S/MICU" "$AQUI/micu.cpp" "$OI/libicu.a"
$TIRA --strip-unneeded "$S/MICU"
cp "$R/terceros/icu/data/icudt77l.dat" "$S/icudt77l.dat"
echo "hechos: MCXX MIPC${DIN:-} MTEXTO DEJAVU.TTF MICU icudt77l.dat"
