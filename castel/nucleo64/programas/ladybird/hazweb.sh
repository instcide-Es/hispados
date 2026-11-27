#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/ladybird/hazweb.sh - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
# sección 24): Ladybird entero para Hispa-64: LibJS, LibWeb, LibWebView y los
# servicios (Hub, que con --headless es el navegador sin ventana; WebContent,
# RequestServer, ImageDecoder, Compositor y SystemProbe), con el CMake de
# Ladybird (terceros/ladybird, sin cambiar) y nuestra capa (hispa64/):
#  - hispa64/cmake/cadena.cmake: el compilador cruzado y musl, estático; Rust
#    para x86_64-unknown-linux-musl con nuestro enlazador;
#  - hispa64/cmake/check_for_dependencies.cmake: las dependencias, nuestras
#    bibliotecas de build/*-obj y sus cabeceras de terceros/ (lo que no hay:
#    OpenSSL, ANGLE, Vulkan, FFmpeg, SDL, fontconfig, D-Bus, PulseAudio, Qt:
#    vacío, o solo sus cabeceras);
#  - hispa64/cmake/raiz/CMakeLists.txt: la raíz (la de Ladybird pide Qt), con
#    nuestra LibCrypto (hispa64/LibCrypto), nuestro RequestServer sin OpenSSL
#    (hispa64/RequestServer), FFmpegFunctions.cpp sin FFmpeg
#    (hispa64/LibMediaCodecs) y el buscador de letras con las DejaVu
#    (hispa64/LibGfx);
#  - hispa64/pila.c (la pila del hilo principal con musl), sinopenssl.c,
#    sindbus.c y singl.c (lo de OpenSSL, D-Bus y EGL/GLES que se nombra), en
#    libh64capa.a, que va en cada programa.
# Y dos programas de prueba para Hispa-64:
#  - MJS (mjs.c): las pruebas de LibJS. Se compila con el cruzado de C
#    (estático) junto con js.tar (terceros/ladybird/Tests/LibJS/Runtime, en
#    un tar con .incbin: haz_mjs); corre test-js-runtime, que tiene que estar
#    en su misma carpeta.
#  - MWEB (mweb.c): el navegador sin ventana sobre una página de prueba
#    (haz_mweb: web.tar con los recursos, las letras, las autoridades y la
#    página).
# Rust y cmake solo hay en el PC (rustup, la 1.98.0; cmake y ninja en
# HERR); sin ellos, se usan los programas ya hechos de build/web-obj/sal
# (copiados del PC, como los de Skia o TestLB), y se hacen MJS y MWEB; sin
# ellos, nada. Las crates de Rust, de terceros/rust-crates, sin red.
# Uso: hazweb.sh SALIDA   (HILOS=N: N a la vez). La última línea dice lo hecho.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
T=$R/terceros
L=${HISPA64_LADYBIRD:-$T/ladybird}
S=$1
[ -n "$S" ] || { echo "uso: hazweb.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
X=$CR/bin/x86_64-hispa64-musl
CC=$X-gcc
O=$B/web-obj
mkdir -p "$S"
CARGO=$(command -v cargo || true)
[ -z "$CARGO" ] && [ -x "$HOME/.cargo/bin/cargo" ] && CARGO=$HOME/.cargo/bin/cargo
HERR=${HERR:-$HOME/h64e/herr}
CMAKE=$(command -v cmake || true)
[ -z "$CMAKE" ] && [ -x "$HERR/cmake-3.31.6-linux-x86_64/bin/cmake" ] && CMAKE=$HERR/cmake-3.31.6-linux-x86_64/bin/cmake
NINJA=$(command -v ninja || true)
[ -z "$NINJA" ] && [ -x "$HERR/ninja" ] && NINJA=$HERR/ninja
PROGS="Hub WebContent RequestServer ImageDecoder Compositor SystemProbe test-js-runtime js Navegador"

# MJS (mjs.c), con las pruebas de LibJS dentro (js.tar, como lb.tar de MLB)
haz_mjs() {
  mkdir -p "$O"
  tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$L/Tests/LibJS" -cf "$O/js.tar" Runtime
  printf '\t.section .rodata\n\t.globl js_tar\n\t.globl js_tar_fin\njs_tar:\n\t.incbin "%s"\njs_tar_fin:\n\t.byte 0\n\t.section .note.GNU-stack,"",@progbits\n' "$O/js.tar" > "$O/jstar.s"
  "$CC" -O2 -Wall -fno-stack-protector -static -o "$S/MJS" "$AQUI/mjs.c" "$O/jstar.s"
  "$X-strip" --strip-unneeded "$S/MJS"
}
# MWEB (mweb.c), con lo que deja en /tmp/web dentro (web.tar): share/Lagom (los
# recursos de Ladybird, Base/res), share/fonts (las DejaVu de terceros/dejavu:
# sin fontconfig, Ladybird las toma de XDG_DATA_DIRS/fonts),
# share/autoridades.pem (las raíces para https) y paginas/ (la de prueba)
haz_mweb() {
  rm -rf "$O/webtar" && mkdir -p "$O/webtar/share/fonts"
  cp -a "$L/Base/res" "$O/webtar/share/Lagom"
  cp "$T/dejavu/"*.TTF "$O/webtar/share/fonts/"
  # ([etapa 10 h] la DejaVu Sans entera, no la recortada de la oficina: las
  # páginas de hoy traen flechas, griego, cirílico, signos...)
  cp "$T/dejavu/DejaVuSans.ttf" "$O/webtar/share/fonts/SANS.TTF"
  # ([etapa 10 h] las autoridades de Mozilla, no solo las de Let's Encrypt)
  cp "$T/autoridades-mozilla/autoridades.pem" "$O/webtar/share/"
  cp -a "$AQUI/hispa64/paginas" "$O/webtar/paginas"
  tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$O/webtar" -cf "$O/web.tar" share paginas
  printf '\t.section .rodata\n\t.globl web_tar\n\t.globl web_tar_fin\nweb_tar:\n\t.incbin "%s"\nweb_tar_fin:\n\t.byte 0\n\t.section .note.GNU-stack,"",@progbits\n' "$O/web.tar" > "$O/webtar.s"
  "$CC" -O2 -Wall -fno-stack-protector -static -o "$S/MWEB" "$AQUI/mweb.c" "$O/webtar.s"
  "$X-strip" --strip-unneeded "$S/MWEB"
}
if [ -z "$CARGO" ] || [ -z "$CMAKE" ] || [ -z "$NINJA" ] || [ ! -x "$CC" ]; then
  # sin Rust o sin cmake: lo ya hecho
  if [ -f "$O/sal/Hub" ] && [ -x "$CC" ]; then
    for p in $PROGS; do [ -f "$O/sal/$p" ] && cp "$O/sal/$p" "$S/"; done
    [ -f "$S/Navegador" ] && mv "$S/Navegador" "$S/NAVEGADOR"
    haz_mjs
    haz_mweb
    echo "hechos: $(cd "$S" && ls | tr '\n' ' ')"
  else
    echo "hechos: (sin Rust o sin cmake: ni build/web-obj/sal)"
  fi
  exit 0
fi
export PATH="$(dirname "$CARGO"):$(dirname "$CMAKE"):$(dirname "$NINJA"):$PATH"
export RUSTUP_TOOLCHAIN=${RUSTUP_TOOLCHAIN:-1.98.0}
HILOS=${HILOS:-1}
for l in simdutf zmij mimalloc psl woff2 brotlidec libjpeg zlib bearssl64 avif dav1d libwebp harfbuzz freetype icu skia curl sqlite simdjson zstd; do
  ls "$B/$l-obj/"*.a > /dev/null 2>&1 || { echo "hazweb.sh: falta $B/$l-obj (libs/hazlibs.sh, hazimg.sh, hazcxx.sh, hazskia.sh, hazcurl.sh)" >&2; exit 2; }
done
mkdir -p "$O/inc" "$O/llvm" "$O/sal"
# ---- 1. lo que ponen nuestras dependencias (check_for_dependencies.cmake) ----
ln -sfn "$T/harfbuzz/src" "$O/inc/harfbuzz"
ln -sfn "$T/llvm-libc" "$O/llvm/llvm-libc"
# ICU sin stubdata y con los datos de todos los idiomas (como hazlb.sh)
IC=$T/icu/source
IV=$(sed -n 's/^#define U_ICU_VERSION "\(.*\)"$/\1/p' "$IC/common/unicode/uvernum.h")
IDAT=$B/icu-datos/icudt${IV%%.*}l-ladybird.dat
[ -f "$IDAT" ] || { echo "hazweb.sh: faltan los datos de ICU de Ladybird, $IDAT (terceros/icu/LEEME.txt)" >&2; exit 2; }
if [ ! -f "$O/libicudatos.a" ] || [ "$IDAT" -nt "$O/libicudatos.a" ]; then
  printf '\t.section .rodata\n\t.balign 16\n\t.globl icudt%s_dat\n\t.type icudt%s_dat, @object\nicudt%s_dat:\n\t.incbin "%s"\n\t.size icudt%s_dat, .-icudt%s_dat\n\t.section .note.GNU-stack,"",@progbits\n' \
    "${IV%%.*}" "${IV%%.*}" "${IV%%.*}" "$IDAT" "${IV%%.*}" "${IV%%.*}" > "$O/icudatos.s"
  "$CC" -c "$O/icudatos.s" -o "$O/icudatos.o"
  rm -f "$O/libicudatos.a"; "$X-ar" rcs "$O/libicudatos.a" "$O/icudatos.o"
fi
if [ ! -f "$O/libicusin.a" ] || [ "$B/icu-obj/libicu.a" -nt "$O/libicusin.a" ]; then
  rm -f "$O/libicusin.a"; "$X-ar" rcs "$O/libicusin.a" $(ls "$B"/icu-obj/*.o | grep -v '/stubdata_')
fi
# lo nuestro de cada programa
"$CC" -O2 -c "$AQUI/hispa64/pila.c" -o "$O/pila.o"
"$CC" -O2 -c "$AQUI/hispa64/sinopenssl.c" -o "$O/sinopenssl.o"
"$CC" -O2 -I"$AQUI/hispa64/incluir" -c "$AQUI/hispa64/sindbus.c" -o "$O/sindbus.o"
"$CC" -O2 -DEGL_NO_X11 -I"$T/angle-cabeceras" -c "$AQUI/hispa64/singl.c" -o "$O/singl.o"
rm -f "$O/libh64capa.a"; "$X-ar" rcs "$O/libh64capa.a" "$O/pila.o" "$O/sinopenssl.o" "$O/sindbus.o" "$O/singl.o"
# ---- 2. las crates de crates.io, de terceros/rust-crates, sin red (como
# hazlb.sh: crates.py las comprueba con el Cargo.lock y las abre en V; cargo
# las toma de ahí por la configuración de CARGO_HOME) ----
V=$O/vendor
if [ ! -f "$V/.hecho" ] || [ "$T/rust-crates/LEEME.txt" -nt "$V/.hecho" ]; then
  rm -rf "$V" && mkdir -p "$V"
  python3 -I "$AQUI/crates.py" "$L/Cargo.lock" "$T/rust-crates" "$V"
  touch "$V/.hecho"
fi
export CARGO_HOME=$O/cargo-home
mkdir -p "$CARGO_HOME"
cat > "$CARGO_HOME/config.toml" << FIN
[source.crates-io]
replace-with = "terceros"
[source.terceros]
directory = "$V"
[net]
offline = true
FIN
# ---- 3. el CMake de Ladybird, con nuestra raíz y nuestra cadena ----
C=$O/cmake
mkdir -p "$C"
"$CMAKE" -G Ninja -S "$AQUI/hispa64/cmake/raiz" -B "$C" -DCMAKE_TOOLCHAIN_FILE="$AQUI/hispa64/cmake/cadena.cmake" \
  -DHISPA64_R="$R" -DHISPA64_LADYBIRD="$L" -DCMAKE_MODULE_PATH="$AQUI/hispa64/cmake" -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_JS_JIT=OFF -DLADYBIRD_ENABLE_CCACHE=OFF -DENABLE_NETWORK_DOWNLOADS=OFF -DLADYBIRD_CACHE_DIR="$AQUI/hispa64/cache" \
  -DLADYBIRD_ENABLE_UPDATE_CHECK=OFF -DBUILD_TESTING=OFF > "$O/cmake.txt" 2>&1 || { tail -20 "$O/cmake.txt" >&2; exit 2; }
echo "hazweb.sh: Ladybird, con $HILOS a la vez" >&2
# shellcheck disable=SC2046
nice "$NINJA" -C "$C" -j "$HILOS" $(for p in $PROGS; do echo "bin/$p"; done) > "$O/ninja.txt" 2>&1 || { grep -a -A8 '^FAILED' "$O/ninja.txt" | head -40 >&2; exit 2; }
for p in $PROGS; do
  "$X-strip" --strip-unneeded -o "$O/sal/$p" "$C/bin/$p"
  cp "$O/sal/$p" "$S/"
done
# ([etapa 10 h] la interfaz, NAVEGADOR: con mayúsculas, como los demás programas de Hispa-64)
mv "$S/Navegador" "$S/NAVEGADOR"
haz_mjs
haz_mweb
echo "hechos: $(cd "$S" && ls | tr '\n' ' ')"
