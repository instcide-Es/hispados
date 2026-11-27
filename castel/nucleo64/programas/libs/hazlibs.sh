#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/libs/hazlibs.sh - Hispa-64, etapa 10 e (docs/64/HISPA-64.md):
# las bibliotecas de terceros que pide Ladybird (además de Skia, zlib, libpng,
# libjpeg-turbo y libwebp, que van con Skia), compiladas con el compilador
# cruzado (x86_64-hispa64-musl-gcc y -g++, de hazgcc.sh: build/cruzado), un
# archivo cada vez, cada una en build/<biblioteca>-obj/lib<biblioteca>.a:
#   simdutf 9.5.0      (AK: UTF-8, UTF-16 y UTF-32, base64)
#   fast_float 8.3.3   (AK: de texto a número; solo cabeceras: no se compila)
#   zmij 1.2           (AK: de número a texto, la forma más corta)
#   mimalloc 2.2.7     (AK y LibCore: el reparto de la memoria; MI_SECURE)
#   sqlite 3.54.1      (LibDatabase: las cookies, la caché, el almacén local)
#   woff2 1.0.2        (LibGfx: las letras web WOFF2), con brotli (el de
#                      terceros/brotli, solo el descompresor) en brotlidec
#   wuffs 0.4.0-alpha.10 (LibGfx: GIF; wuffs-v0.4.c: la 0.3, solo Apache-2.0)
#   dav1d 1.5.4        (AV1 en C, sin ensamblador) y libavif 1.4.2 (AVIF)
#   libpsl 0.23.3      (LibURL: la lista de sufijos públicos, dentro)
#   simdjson 5.1.1     (lo pide la configuración de Ladybird; JSON)
#   zstd 1.5.7         (para curl: «Content-Encoding: zstd»)
#   libtiff 4.7.2      (la pide vcpkg.json; sin zlib, JPEG ni zstd)
# y MLIBS (mlibs.cpp), la prueba de todas, con sus datos (letra.woff2, de
# tests/woff, y colors-animated-8bpc.avif, de las pruebas de libavif).
# Cada biblioteca se rehace solo si cambia su LEEME.txt, el compilador
# (.compilador) o lo que se le pasa (.opciones: las opciones y los archivos).
# Uso: [HILOS=N] hazlibs.sh SALIDA   (dice en la última línea los que ha hecho;
# sin el compilador cruzado, ninguno)
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
TE=$R/terceros
S=$1
[ -n "$S" ] || { echo "uso: hazlibs.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
CC=$CR/bin/x86_64-hispa64-musl-gcc
CXX=$CR/bin/x86_64-hispa64-musl-g++
AR=$CR/bin/x86_64-hispa64-musl-ar
TIRA=$CR/bin/x86_64-hispa64-musl-strip
[ -x "$CC" ] && [ -x "$CXX" ] || { echo "hechos: (sin el compilador cruzado: hazgcc.sh)"; exit 0; }
ID=$("$CXX" --version | head -1)
HILOS=${HILOS:-1}
mkdir -p "$S"
COMUN="-O2 -fno-stack-protector -ffunction-sections -fdata-sections"

# haz NOMBRE LEEME OPCIONES_C OPCIONES_C++ ARCHIVO[@DEFINICIONES]...
# (los .c con CC, los .cc y .cpp con CXX; «.c+» es un .c que se compila como C++)
haz() {
  local n=$1 leeme=$2 oc=$3 ox=$4; shift 4
  local o=$B/$n-obj lib=$B/$n-obj/lib$n.a firma
  firma=$(printf '%s\n' "$oc" "$ox" "$@" | sha256sum | cut -c1-16)
  if [ -f "$lib" ] && [ "$(cat "$o/.compilador" 2> /dev/null)" = "$ID" ] && [ "$(cat "$o/.opciones" 2> /dev/null)" = "$firma" ] \
     && [ ! "$leeme" -nt "$lib" ]; then return 0; fi
  rm -rf "$o"; mkdir -p "$o"
  local a f d obj lista=$o/.lista
  : > "$lista"
  for a in "$@"; do
    f=${a%%@*}; d=; [ "$a" != "$f" ] && d=${a#*@}
    obj=$(basename "$(dirname "$f")")_$(basename "$f"); obj=$o/${obj%.*}$(echo "$d" | tr -c 'A-Za-z0-9\n' '_').o
    case $f in
      *.c+) printf '%s\t%s\t%s\t%s\n' "$CXX" "$COMUN $ox $(echo "$d" | tr ',' ' ') -x c++" "${f%+}" "$obj" ;;
      *.c|*.S) printf '%s\t%s\t%s\t%s\n' "$CC" "$COMUN $oc $(echo "$d" | tr ',' ' ')" "$f" "$obj" ;;
      *) printf '%s\t%s\t%s\t%s\n' "$CXX" "$COMUN $ox $(echo "$d" | tr ',' ' ')" "$f" "$obj" ;;
    esac >> "$lista"
  done
  # shellcheck disable=SC2016
  tr '\n' '\0' < "$lista" | xargs -0 -P "$HILOS" -n 1 bash -c 'IFS=$'"'"'\t'"'"' read -r c f s ob <<< "$1"; nice $c $f -c "$s" -o "$ob" || { echo "hazlibs.sh: falla $s" >&2; exit 255; }' _
  "$AR" rcs "$lib" "$o"/*.o
  echo "$ID" > "$o/.compilador"
  echo "$firma" > "$o/.opciones"
}

# simdutf (de una pieza: simdutf.cpp)
haz simdutf "$TE/simdutf/LEEME.txt" "" "-std=c++17 -I$TE/simdutf" "$TE/simdutf/simdutf.cpp"
# zmij (zmij.cc; Ladybird le pone el parche de los tamaños de los arreglos: ya puesto)
haz zmij "$TE/zmij/LEEME.txt" "" "-std=c++17 -I$TE/zmij" "$TE/zmij/zmij.cc"
# mimalloc (static.c, como C++: MI_USE_CXX de Ladybird; MI_SECURE=4 y sin relleno)
haz mimalloc "$TE/mimalloc/LEEME.txt" "" "-std=c++17 -I$TE/mimalloc/include -DMI_SECURE=4 -DMI_PADDING=0 -DNDEBUG -Wno-unused-function" "$TE/mimalloc/src/static.c+"
# sqlite (la amalgamación)
haz sqlite "$TE/sqlite/LEEME.txt" "-DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION=1 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DQS=0 -DSQLITE_ENABLE_FTS5=1 -DSQLITE_ENABLE_MATH_FUNCTIONS=1 -DHAVE_USLEEP=1 -DHAVE_FDATASYNC=1" "" "$TE/sqlite/sqlite3.c"
# brotli (solo el descompresor, para woff2)
BR=$TE/brotli/c
haz brotlidec "$TE/brotli/LEEME.txt" "-I$BR/include" "" "$BR"/common/*.c "$BR"/dec/*.c
# woff2 (solo el descompresor: lo que usa Ladybird)
haz woff2 "$TE/woff2/LEEME.txt" "" "-std=c++11 -I$TE/woff2/include -I$BR/include -DNDEBUG" \
  "$TE/woff2/src/table_tags.cc" "$TE/woff2/src/variable_length.cc" "$TE/woff2/src/woff2_common.cc" "$TE/woff2/src/woff2_dec.cc" "$TE/woff2/src/woff2_out.cc"
# wuffs (wuffs-v0.4.c, con su implementación)
haz wuffs "$TE/wuffs/LEEME.txt" "-DWUFFS_IMPLEMENTATION" "" "$TE/wuffs/release/c/wuffs-v0.4.c"
# dav1d (C99 sin ensamblador; lo de «_tmpl» dos veces: 8 y 16 bits)
DV=$TE/dav1d
DVT=
for f in "$DV"/src/*_tmpl.c; do DVT="$DVT $f@-DBITDEPTH=8 $f@-DBITDEPTH=16"; done
# shellcheck disable=SC2086
haz dav1d "$DV/LEEME.txt" "-std=c99 -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -I$DV/hispa64 -I$DV -I$DV/include -I$DV/include/dav1d -Wno-maybe-uninitialized" "" \
  $(for f in cdf cpu ctx data decode dequant_tables getbits intra_edge itx_1d lf_mask lib log mem msac obu pal picture qm ref refmvs scan tables thread_task warpmv wedge; do echo "$DV/src/$f.c"; done) $DVT
# libavif (con dav1d; con su trozo de libyuv, el de escalar; sin libsharpyuv)
AV=$TE/libavif
haz avif "$AV/LEEME.txt" "-std=c11 -D_GNU_SOURCE -DAVIF_CODEC_DAV1D=1 -I$AV/include -I$DV/include -I$AV/third_party/libyuv/include" "" "$AV"/src/*.c \
  "$AV"/third_party/libyuv/source/*.c
# libpsl (con la lista dentro, sin IDNA en marcha: los nombres ya en ASCII)
PS=$TE/libpsl
haz psl "$PS/LEEME.txt" "-DHAVE_CONFIG_H -DBUILDING_PSL -I$PS/hispa64 -I$PS/include -I$PS/src" "" "$PS/src/psl.c" "$PS/src/lookup_string_in_fixed_set.c"
# simdjson (de una pieza)
haz simdjson "$TE/simdjson/LEEME.txt" "" "-std=c++17 -I$TE/simdjson -DNDEBUG" "$TE/simdjson/simdjson.cpp"
# zstd (sin hilos; con el ensamblador de huf_decompress)
ZS=$TE/zstd/lib
haz zstd "$TE/zstd/LEEME.txt" "-DXXH_NAMESPACE=ZSTD_ -DZSTD_LEGACY_SUPPORT=0 -I$ZS -I$ZS/common" "" "$ZS"/common/*.c "$ZS"/compress/*.c "$ZS"/decompress/*.c "$ZS"/decompress/huf_decompress_amd64.S
# libtiff (sin zlib, JPEG, zstd, LZMA, WebP ni JBIG: LZW, PackBits, CCITT...)
TF=$TE/tiff
haz tiff "$TF/LEEME.txt" "-I$TF/hispa64 -I$TF/libtiff -Wno-format" "" \
  $(ls "$TF"/libtiff/*.c | grep -v -e tif_win32.c)

# MLIBS: la prueba de todas
I="-I$TE/simdutf -I$TE/fast_float/include -I$TE/zmij -I$TE/mimalloc/include -I$TE/sqlite -I$TE/woff2/include -I$TE/wuffs/release/c -I$DV/include -I$AV/include -I$PS/include -I$TE/simdjson -I$ZS -I$TF/hispa64 -I$TF/libtiff"
L=""
for n in avif dav1d woff2 brotlidec simdutf zmij sqlite wuffs psl simdjson zstd tiff mimalloc; do L="$L $B/$n-obj/lib$n.a"; done
# shellcheck disable=SC2086
nice "$CXX" -std=c++17 -O2 -Wall -fno-stack-protector -static -pthread $I -o "$S/MLIBS" "$AQUI/mlibs.cpp" $L -Wl,--gc-sections
"$TIRA" --strip-unneeded "$S/MLIBS"
cp "$R/tests/woff/letra.woff2" "$S/letra.woff2"
cp "$AV/datos/colors-animated-8bpc.avif" "$S/colores.avif"
echo "hechos: MLIBS letra.woff2 colores.avif"
