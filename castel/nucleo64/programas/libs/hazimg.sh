#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/libs/hazimg.sh - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
# sección 22): las bibliotecas de las imágenes que piden Skia y Ladybird,
# compiladas con el compilador cruzado (build/cruzado, de hazgcc.sh), un
# archivo cada vez, en build/<biblioteca>-obj (solo lo que ha cambiado; todo
# otra vez si cambia su LEEME.txt o el compilador):
#   zlib 1.4             terceros/zlib           build/zlib-obj/libz.a
#   libpng 1.6.59        terceros/libpng         build/libpng-obj/libpng.a
#   libjpeg-turbo 3.2.1  terceros/libjpeg-turbo  build/libjpeg-obj/libjpeg.a (sin SIMD)
#   libwebp 1.6.0        terceros/libwebp        build/libwebp-obj/libwebp.a (con demux y mux)
# y, en SALIDA, MIMG (mimg.c: su prueba) y PNGTEST.PNG (el de libpng).
# Uso: hazimg.sh SALIDA   (HILOS=N: N a la vez; si no, de uno en uno). La
# última línea dice los que ha hecho; sin el compilador cruzado, ninguno.
# Con «hazimg.sh -solo», las bibliotecas y nada más (para hazskia.sh).
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
TER=$R/terceros
S=$1
[ -n "$S" ] || { echo "uso: hazimg.sh SALIDA | -solo" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
CC=$CR/bin/x86_64-hispa64-musl-gcc
[ -x "$CC" ] || { echo "hechos: (sin el compilador cruzado: hazgcc.sh)"; exit 0; }
ID=$("$CC" --version | head -1)
HILOS=${HILOS:-1}
CF="-O2 -fno-stack-protector -fvisibility=hidden -w"

# biblioteca NOMBRE LIB.a DIR "OPCIONES" archivo [archivo...]: compila los
# que han cambiado (con HILOS a la vez) y hace LIB.a; «archivo:OPC» añade
# OPC a ese archivo (-msse4.1, -mavx2)
biblioteca() {
  local nom=$1 lib=$2 dir=$3 opc=$4 o=$B/$1-obj f a x cambio=0 lista
  shift 4
  if [ "$(cat "$o/.compilador" 2> /dev/null)" != "$ID" ] || [ "$dir/LEEME.txt" -nt "$o/$lib" ]; then rm -rf "$o"; fi
  mkdir -p "$o"
  lista=$o/.lista
  : > "$lista"
  for a in "$@"; do
    f=${a%%:*}; x=""
    [ "$f" != "$a" ] && x=${a#*:}
    local ob=$o/$(echo "$f" | tr / _ | sed 's/\.c$/.o/')
    if [ ! -f "$ob" ] || [ "$dir/$f" -nt "$ob" ]; then
      echo "$CC $CF $opc $x -c $dir/$f -o $ob" >> "$lista"
      cambio=1
    fi
  done
  if [ -s "$lista" ]; then
    # shellcheck disable=SC2016
    xargs -d '\n' -P "$HILOS" -I{} sh -c 'nice {} || { echo "hazimg.sh: falla: {}" >&2; exit 255; }' < "$lista"
  fi
  if [ $cambio = 1 ] || [ ! -f "$o/$lib" ]; then rm -f "$o/$lib"; "$CR/bin/x86_64-hispa64-musl-ar" rcs "$o/$lib" "$o"/*.o; fi
  echo "$ID" > "$o/.compilador"
}

# ---- zlib ----
Z=$TER/zlib
biblioteca zlib libz.a "$Z" "-DHAVE_UNISTD_H -DHAVE_STDARG_H" \
  adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c

# ---- libpng (sin lo de SSE: PNG_INTEL_SSE sin poner) ----
P=$TER/libpng
biblioteca libpng libpng.a "$P" "-I$Z" \
  png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c

# ---- libjpeg-turbo: lo de 8, 12 y 16 bits (src/wrapper), sin SIMD ----
J=$TER/libjpeg-turbo
JW="jcapistd-8 jcapistd-12 jcapistd-16 jccoefct-8 jccoefct-12 jccolor-8 jccolor-12 jccolor-16 jcdctmgr-8 jcdctmgr-12 jcdiffct-8 jcdiffct-12 jcdiffct-16
    jclossls-8 jclossls-12 jclossls-16 jcmainct-8 jcmainct-12 jcmainct-16 jcprepct-8 jcprepct-12 jcprepct-16 jcsample-8 jcsample-12 jcsample-16
    jdapistd-8 jdapistd-12 jdapistd-16 jdcoefct-8 jdcoefct-12 jdcolor-8 jdcolor-12 jdcolor-16 jddctmgr-8 jddctmgr-12 jddiffct-8 jddiffct-12 jddiffct-16
    jdlossls-8 jdlossls-12 jdlossls-16 jdmainct-8 jdmainct-12 jdmainct-16 jdmerge-8 jdmerge-12 jdpostct-8 jdpostct-12 jdpostct-16
    jdsample-8 jdsample-12 jdsample-16 jfdctfst-8 jfdctfst-12 jfdctint-8 jfdctint-12 jidctflt-8 jidctflt-12 jidctfst-8 jidctfst-12
    jidctint-8 jidctint-12 jidctred-8 jidctred-12 jquant1-8 jquant1-12 jquant2-8 jquant2-12 jutils-8 jutils-12 jutils-16"
JS="jcapimin jchuff jcicc jcinit jclhuff jcmarker jcmaster jcomapi jcparam jcphuff jctrans jdapimin jdatadst jdatasrc jdhuff jdicc jdinput
    jdlhuff jdmarker jdmaster jdphuff jdtrans jerror jfdctflt jmemmgr jmemnobs jpeg_nbits jaricom jcarith jdarith"
# shellcheck disable=SC2046
biblioteca libjpeg libjpeg.a "$J" "-I$J/config -I$J/src" \
  $(for f in $JS; do echo "src/$f.c"; done) $(for f in $JW; do echo "src/wrapper/$f.c"; done)

# ---- libwebp: lo de leer y escribir, sharpyuv, demux y mux (SSE2 siempre;
# SSE4.1 y AVX2, los que elige al arrancar según el procesador) ----
W=$TER/libwebp
WD="dec/alpha_dec dec/buffer_dec dec/frame_dec dec/idec_dec dec/io_dec dec/quant_dec dec/tree_dec dec/vp8_dec dec/vp8l_dec dec/webp_dec
    dsp/alpha_processing dsp/cpu dsp/dec dsp/dec_clip_tables dsp/filters dsp/lossless dsp/rescaler dsp/upsampling dsp/yuv
    dsp/alpha_processing_sse2 dsp/dec_sse2 dsp/filters_sse2 dsp/lossless_sse2 dsp/rescaler_sse2 dsp/upsampling_sse2 dsp/yuv_sse2
    dsp/cost dsp/enc dsp/lossless_enc dsp/ssim dsp/cost_sse2 dsp/enc_sse2 dsp/lossless_enc_sse2 dsp/ssim_sse2
    utils/bit_reader_utils utils/color_cache_utils utils/filters_utils utils/huffman_utils utils/palette utils/quant_levels_dec_utils
    utils/rescaler_utils utils/random_utils utils/thread_utils utils/utils utils/bit_writer_utils utils/huffman_encode_utils utils/quant_levels_utils
    enc/alpha_enc enc/analysis_enc enc/backward_references_cost_enc enc/backward_references_enc enc/config_enc enc/cost_enc enc/filter_enc
    enc/frame_enc enc/histogram_enc enc/iterator_enc enc/near_lossless_enc enc/picture_enc enc/picture_csp_enc enc/picture_psnr_enc
    enc/picture_rescale_enc enc/picture_tools_enc enc/predictor_enc enc/quant_enc enc/syntax_enc enc/token_enc enc/tree_enc enc/vp8l_enc enc/webp_enc
    demux/anim_decode demux/demux mux/anim_encode mux/muxedit mux/muxinternal mux/muxread"
W41="alpha_processing_sse41 dec_sse41 lossless_sse41 upsampling_sse41 yuv_sse41 enc_sse41 lossless_enc_sse41"
# shellcheck disable=SC2046
biblioteca libwebp libwebp.a "$W" "-DHAVE_CONFIG_H -I$W -pthread" \
  $(for f in $WD; do echo "src/$f.c"; done) $(for f in $W41; do echo "src/dsp/$f.c:-msse4.1"; done) \
  src/dsp/lossless_avx2.c:-mavx2 src/dsp/lossless_enc_avx2.c:-mavx2 \
  sharpyuv/sharpyuv_sse2.c sharpyuv/sharpyuv_cpu.c sharpyuv/sharpyuv_csp.c sharpyuv/sharpyuv_dsp.c sharpyuv/sharpyuv_gamma.c sharpyuv/sharpyuv.c

[ "$S" = -solo ] && { echo "hechos: libz.a libpng.a libjpeg.a libwebp.a"; exit 0; }
mkdir -p "$S"
"$CC" -O2 -Wall -fno-stack-protector -static -pthread -I"$Z" -I"$P" -I"$J/src" -I"$J/config" -I"$W/src" -o "$S/MIMG" "$AQUI/mimg.c" \
  "$B/libwebp-obj/libwebp.a" "$B/libjpeg-obj/libjpeg.a" "$B/libpng-obj/libpng.a" "$B/zlib-obj/libz.a" -lm
"$CR/bin/x86_64-hispa64-musl-strip" --strip-unneeded "$S/MIMG"
cp "$P/pngtest.png" "$S/PNGTEST.PNG"
echo "hechos: MIMG PNGTEST.PNG"
