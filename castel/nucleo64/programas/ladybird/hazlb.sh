#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/ladybird/hazlb.sh - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
# sección 23): las bibliotecas de Ladybird que piden Rust (terceros/ladybird:
# LibUnicode, LibRegex, LibURL, LibTextCodec, LibCompress, LibIPC y LibGfx),
# compiladas con el compilador cruzado y musl como las compila Ladybird
# (las mismas opciones que hazak.sh), con sus «crates» de Rust (la parte de
# Rust de cada una: libunicode_rust, liburl_rust...), y sus pruebas
# (Tests/LibUnicode, LibURL, LibIPC y LibGfx: TestLibUnicode, TestLibURL,
# TestLibIPC y TestLibGfx, cada una un programa, como las de Ladybird), en
# SALIDA con MLB (mlb.c), que las corre en Hispa-64 y dice el resumen.
#
# Lo que hace el CMake de Ladybird (Meta/CMake/rust_crate.cmake,
# code_generators.cmake), aquí a mano:
#  - un «workspace» de cargo con solo las crates que hacen falta (copiadas de
#    terceros/ladybird a build/lb-obj/ws, con el Cargo.lock de Ladybird) y las
#    de crates.io de terceros/rust-crates (los .crate tal cual, comprobados con
#    el SHA-256 del Cargo.lock), sin red;
#  - cada crate, «cargo rustc --lib» con sus «features» (como import_rust_crate)
#    para x86_64-unknown-linux-musl con el cruzado de enlazador: una
#    biblioteca estática por crate (cada una con su copia de la biblioteca de
#    Rust: se enlazan con --allow-multiple-definition, como Ladybird en Linux
#    sin bibliotecas compartidas); su RustFFI.h (de cbindgen), a
#    build/lb-obj/gen/LibX/; y su inicializador (RustCrateInit.cpp.in);
#  - los .ipc de LibGfx, con generate_ipc_definitions.py (Python).
# Rust solo hay en el PC (rustup, la 1.98.0); sin cargo, se usan los
# programas ya hechos de build/lb-obj/sal (copiados del PC, como los de
# Skia); sin ellos, nada.
# Uso: hazlb.sh SALIDA   (HILOS=N: N a la vez). La última línea dice lo hecho.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
T=$R/terceros
L=$T/ladybird
S=$1
[ -n "$S" ] || { echo "uso: hazlb.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
X=$CR/bin/x86_64-hispa64-musl
CXX=$X-g++
CC=$X-gcc
O=$B/lb-obj
mkdir -p "$S"
CARGO=$(command -v cargo || true)
[ -z "$CARGO" ] && [ -x "$HOME/.cargo/bin/cargo" ] && CARGO=$HOME/.cargo/bin/cargo
# MLB (mlb.c), con los archivos de las pruebas dentro (lb.tar: los
# test-inputs de LibGfx, en el orden de sus nombres y sin fechas ni dueños,
# para que salga siempre igual)
haz_mlb() {
  mkdir -p "$O"
  tar --format=ustar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -C "$L" -cf "$O/lb.tar" Tests/LibGfx/test-inputs
  printf '\t.section .rodata\n\t.globl lb_tar\n\t.globl lb_tar_fin\nlb_tar:\n\t.incbin "%s"\nlb_tar_fin:\n\t.byte 0\n\t.section .note.GNU-stack,"",@progbits\n' "$O/lb.tar" > "$O/lbtar.s"
  "$CC" -O2 -Wall -fno-stack-protector -static -o "$S/MLB" "$AQUI/mlb.c" "$O/lbtar.s"
  "$X-strip" --strip-unneeded "$S/MLB"
}
if [ -z "$CARGO" ] || [ ! -x "$CXX" ]; then
  # sin Rust: lo ya hecho
  if [ -f "$O/sal/TestLB" ] && [ -x "$CC" ]; then
    cp "$O/sal/TestLB" "$S/"
    haz_mlb
    echo "hechos: TestLB MLB"
  else
    echo "hechos: (sin Rust: ni cargo ni build/lb-obj/sal)"
  fi
  exit 0
fi
for l in simdutf zmij mimalloc psl woff2 brotlidec libjpeg zlib bearssl64 avif dav1d libwebp; do
  ls "$B/$l-obj/"*.a > /dev/null 2>&1 || { echo "hazlb.sh: falta $B/$l-obj (libs/hazlibs.sh, libs/hazimg.sh)" >&2; exit 2; }
done
[ -f "$B/ladybird-obj/.compilador" ] || { echo "hazlb.sh: falta build/ladybird-obj (hazak.sh)" >&2; exit 2; }
export PATH="$(dirname "$CARGO"):$PATH"
export RUSTUP_TOOLCHAIN=${RUSTUP_TOOLCHAIN:-1.98.0}
HILOS=${HILOS:-1}
ID="$("$CXX" --version | head -1) / $(rustc --version)"
if [ "$(cat "$O/.compilador" 2> /dev/null)" != "$ID" ] || [ "$L/LEEME.txt" -nt "$O/.compilador" ] || [ "$AQUI/hazlb.sh" -nt "$O/.compilador" ]; then rm -rf "$O"; fi
mkdir -p "$O/gen" "$O/sal"

# ---- 1. las crates de crates.io, de terceros/rust-crates, sin red ----
V=$O/vendor
if [ ! -f "$V/.hecho" ] || [ "$T/rust-crates/LEEME.txt" -nt "$V/.hecho" ]; then
  rm -rf "$V" && mkdir -p "$V"
  python3 -I "$AQUI/crates.py" "$L/Cargo.lock" "$T/rust-crates" "$V"
  touch "$V/.hecho"
fi
# ---- 2. el workspace: las crates de Ladybird que hacen falta ----
W=$O/ws
MIEMBROS="AK/Rust Libraries/RustAllocator Libraries/LibUnicode/Rust Libraries/LibRegex/Rust Libraries/LibRegex/Compiler/Rust
 Libraries/LibURL/Rust Libraries/LibTextCodec/Rust Libraries/LibCompress/Rust Libraries/LibCrypto/Rust Libraries/LibIPC/Rust
 Libraries/LibIPC/Rust/macros Libraries/LibGfx/Rust Libraries/LibImageDecoders/Rust"
rm -rf "$W" && mkdir -p "$W/Libraries" "$W/.cargo"
for m in $MIEMBROS; do mkdir -p "$W/$(dirname "$m")"; [ -e "$W/$m" ] || cp -a "$L/$m" "$W/$m"; done
cp "$L/Libraries/RustPanic.rs" "$L/Libraries/RustDemangle.rs" "$W/Libraries/"
cp "$L/Cargo.lock" "$L/rust-toolchain.toml" "$W/"
python3 -I - "$L/Cargo.toml" "$W/Cargo.toml" $MIEMBROS << 'FIN'
import re, sys
s = open(sys.argv[1]).read()
miembros = "".join('    "%s",\n' % m for m in sys.argv[3:])
s = re.sub(r'members = \[.*?\n\]', 'members = [\n' + miembros + ']', s, count=1, flags=re.S)
open(sys.argv[2], "w").write(s)
FIN
cat > "$W/.cargo/config.toml" << FIN
[source.crates-io]
replace-with = "terceros"
[source.terceros]
directory = "$V"
[net]
offline = true
FIN
# ---- 3. cada crate, como import_rust_crate ----
TU=x86_64_unknown_linux_musl
export CC_$TU="$CC" CXX_$TU="$CXX" AR_$TU="$X-ar"
export CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_LINKER="$CC"
export CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_RUSTFLAGS="-C link-self-contained=no -C target-feature=+crt-static"
export LADYBIRD_LINKS_AK=1 LLVM_LIBC_INCLUDE_DIR="$T/llvm-libc"
export CARGO_TERM_PROGRESS_WHEN=never CARGO_PROFILE_RELEASE_PANIC=abort
CT=$O/cargo
RD=$CT/x86_64-unknown-linux-musl/release
crate() {   # RUTA NOMBRE CARPETA_DE_LA_BIBLIOTECA FEATURES
  local f=""
  [ -n "$4" ] && f="--features=$4"
  # shellcheck disable=SC2086
  nice cargo rustc -q --lib --package "$2" $f --target=x86_64-unknown-linux-musl --manifest-path "$W/$1/Cargo.toml" \
    --target-dir "$CT" --release -j "$HILOS" -- -Cdefault-linker-libraries=yes
  cp "$RD/lib$2.a" "$O/lib$2.a"
  # su RustFFI.h, del build.rs (cbindgen)
  if [ -n "$3" ]; then
    h=$(ls -t "$RD"/build/"$2"-*/out/RustFFI.h 2> /dev/null | head -1)
    [ -n "$h" ] || { echo "hazlb.sh: $2 no ha dejado RustFFI.h" >&2; exit 2; }
    mkdir -p "$O/gen/$3"; cp "$h" "$O/gen/$3/RustFFI.h"
  fi
  sed "s/@RUST_CRATE_NAME@/$2/g" "$L/Meta/CMake/RustCrateInit.cpp.in" > "$O/gen/${2}_init.cpp"
}
echo "hazlb.sh: las crates de Rust" >&2
crate Libraries/LibUnicode/Rust libunicode_rust LibUnicode allocator,idna
crate Libraries/LibRegex/Rust libregex_rust LibRegex allocator
crate Libraries/LibURL/Rust liburl_rust LibURL allocator
crate Libraries/LibTextCodec/Rust libtextcodec_rust LibTextCodec allocator
crate Libraries/LibIPC/Rust libipc_rust LibIPC allocator,cpp_transport
crate Libraries/LibGfx/Rust libgfx_rust LibGfx allocator
CARGO_PROFILE_RELEASE_PANIC=unwind crate Libraries/LibImageDecoders/Rust libimagedecoders_rust LibImageDecoders allocator   # (PANIC_UNWIND)
echo "hazlb.sh: hechas las crates" >&2

# ---- 4. lo de C++, con las opciones de Ladybird (las de hazak.sh) ----
G=$O/gen
AKO=$B/ladybird-obj
# lo que hace su CMake: los Export.h de las que exportan con X_API
for l in Unicode:UNICODE URL:URL IPC:IPC Gfx:GFX Regex:REGEX TextCodec:TEXTCODEC Compress:COMPRESS Crypto:CRYPTO ImageDecoders:IMAGEDECODERS MediaCodecs:MEDIACODECS GC:GC; do
  mkdir -p "$G/Lib${l%%:*}"; printf '#pragma once\n#define %s_API\n#define %s_NO_EXPORT\n' "${l#*:}" "${l#*:}" > "$G/Lib${l%%:*}/Export.h"
done
# los .ipc de LibGfx (compile_ipc: generate_ipc_definitions.py)
for i in Point Rect Size; do
  python3 -I "$L/Meta/Generators/generate_ipc_definitions.py" --input "$L/Libraries/LibGfx/$i.ipc" --output "$G/LibGfx/${i}Codecs.h" \
    --implementation "$G/LibGfx/${i}Codecs.cpp" --header-name "${i}Codecs.h" --library LibGfx --export-macro=GFX_API
done
# (TIFFMetadata.h y TIFFTagHandler.cpp de LibImageDecoders: TIFFGenerator.py)
mkdir -p "$G/LibGfx/ImageFormats"
python3 -I "$L/Libraries/LibGfx/TIFFGenerator.py" -o "$G/LibGfx/ImageFormats"
IC=$T/icu/source
# (<harfbuzz/hb.h>, como en vcpkg)
mkdir -p "$G/inc" && ln -sfn "$T/harfbuzz/src" "$G/inc/harfbuzz"
DEP="-I$G/inc -I$T/fast_float/include -I$T/simdutf -I$T/zmij -I$T/mimalloc/include -I$IC/common -I$IC/i18n -DU_STATIC_IMPLEMENTATION -DU_USING_ICU_NAMESPACE=0 \
 -I$T/libpsl/include -I$T/harfbuzz/src -I$T/libwebp/src -I$T/libavif/include -I$AQUI/hispa64 -I$T/wuffs/release/c -I$T/freetype/include -I$T/woff2/include -I$T/libjpeg-turbo/src -I$T/libjpeg-turbo/config -I$T/zlib"
CF="-std=c++23 -O2 -fno-exceptions -fsigned-char -ffp-contract=off -fno-semantic-interposition -fstack-protector-strong -fstrict-flex-arrays=2 \
 -ftrivial-auto-var-init=zero -fvisibility-inlines-hidden -w -I$L -I$L/Libraries -I$AKO/gen -I$G $DEP"
OB=$O/obj
mkdir -p "$OB"
LIS=$O/.lista
: > "$LIS"
a_lista() {   # FUENTE OBJETO [OPCIONES]
  if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then echo "$CXX $CF $3 -c $1 -o $2" >> "$LIS"; fi
}
biblio() {   # PREFIJO CARPETA OPCIONES ARCHIVOS...
  local p=$1 d=$2 op=$3 f; shift 3
  for f in "$@"; do a_lista "$d/$f" "$OB/${p}_$(echo "${f%.cpp}" | tr / _).o" "$op"; done
}
LU=$L/Libraries
biblio unicode "$LU/LibUnicode" "-I$G/LibUnicode" Bidi.cpp Calendar.cpp Calendars/AdjustedEraCalendar.cpp Calendars/ChineseDangiCalendar.cpp CaseMapping.cpp \
  CharacterTypes.cpp Collator.cpp CurrencyCode.cpp DateTimeFormat.cpp DateTimeFormatExports.cpp DisplayNames.cpp DurationFormat.cpp FullwidthMapping.cpp \
  ICU.cpp IDNA.cpp IntlExports.cpp ListFormat.cpp Locale.cpp Normalize.cpp NumberFormat.cpp NumberFormatExports.cpp PluralRules.cpp \
  RelativeTimeFormat.cpp RelativeTimeFormatExports.cpp Segmenter.cpp String.cpp TimeZone.cpp UnicodeKeywords.cpp Utf16String.cpp
biblio core "$LU/LibCore" "" TimeZone.cpp
biblio regex "$LU/LibRegex" "-I$G/LibRegex" ECMAScriptRegex.cpp RustRegex.cpp
biblio url "$LU/LibURL" "-I$G/LibURL -DENABLE_RUST" Host.cpp Origin.cpp Parser.cpp PublicSuffixData.cpp RustIntegration.cpp Site.cpp URL.cpp
biblio textcodec "$LU/LibTextCodec" "-I$G/LibTextCodec" Decoder.cpp Encoder.cpp
biblio compress "$LU/LibCompress" "" GenericZlib.cpp Zlib.cpp
biblio ipc "$LU/LibIPC" "-I$G/LibIPC" Connection.cpp Decoder.cpp Encoder.cpp File.cpp Message.cpp RawAttachment.cpp RawMessage.cpp ReceivedMessageBytes.cpp \
  StartupTransport.cpp ThreadSafeMessagePoster.cpp TransportHandle.cpp Transport.cpp Attachment.cpp
biblio gfx "$LU/LibGfx" "-I$G/LibGfx" AffineTransform.cpp Bitmap.cpp BitmapSequence.cpp CMYKBitmap.cpp Color.cpp ColorConversion.cpp ColorSpace.cpp Cursor.cpp \
  Font/FaceDescription.cpp Font/Font.cpp Font/FontCatalog.cpp Font/FontDatabase.cpp Font/FontSupport.cpp Font/FontTable.cpp Font/FontVariationSettings.cpp \
  Font/PathFontProvider.cpp Font/SharedFontProvider.cpp Font/SystemFallbackFonts.cpp Font/SystemFontMatcher.cpp Font/Typeface.cpp Font/WOFF/Loader.cpp \
  Font/WOFF2/Loader.cpp FontCascadeList.cpp GlyphOutlines.cpp GpuDeviceIdentity.cpp LinuxDmaBufHandle.cpp DecodedImageFrame.cpp RustProcessState.cpp \
  ImageFormats/JPEGWriter.cpp ImageFormats/PNGWriter.cpp Palette.cpp PathData.cpp Point.cpp Rect.cpp SharedImage.cpp SharedImageBuffer.cpp \
  ShareableBitmap.cpp ShapeFeature.cpp Size.cpp SystemTheme.cpp TextLayout.cpp YUVData.cpp Font/SystemFontMatcherNone.cpp
biblio imgdec "$LU/LibGfx/ImageFormats" "-I$G/LibImageDecoders" AVIFLoader.cpp BMPLoader.cpp ExifReader.cpp GIFLoader.cpp ICOLoader.cpp ImageDecoder.cpp \
  JPEGLoader.cpp JPEGXLLoader.cpp PNGLoader.cpp WebPLoader.cpp
biblio imgdecg "$G/LibGfx/ImageFormats" "" TIFFTagHandler.cpp
# LibCrypto: la de Hispa-64 (hispa64/LibCrypto: la misma interfaz, sobre
# BearSSL y monocypher, sin OpenSSL), con las cabeceras de Ladybird
HC=$AQUI/hispa64/LibCrypto
biblio crypto "$HC" "-I$T/bearssl/inc -I$T/monocypher -I$HC" Hash.cpp HMAC.cpp RSA.cpp EC.cpp EdwardsCurve.cpp
for f in "$HC/sha3.c" "$T/monocypher/monocypher.c" "$T/monocypher/monocypher-ed25519.c"; do
  o=$OB/cryptoc_$(basename "$f" .c).o
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then echo "$CC -O2 -fno-stack-protector -I$T/monocypher -c $f -o $o" >> "$LIS"; fi
done
biblio gfxipc "$G/LibGfx" "-I$G/LibGfx" PointCodecs.cpp RectCodecs.cpp SizeCodecs.cpp
for c in libunicode_rust libregex_rust liburl_rust libtextcodec_rust libipc_rust libgfx_rust libimagedecoders_rust; do a_lista "$G/${c}_init.cpp" "$OB/init_$c.o"; done
# las pruebas
TL=$L/Tests
biblio tunicode "$TL/LibUnicode" "-I$G/LibUnicode" TestBidi.cpp TestCollator.cpp TestDisplayNames.cpp TestFullwidthMapping.cpp TestIDNA.cpp TestLocale.cpp \
  TestSegmenter.cpp TestTimeZone.cpp TestUnicodeCharacterTypes.cpp TestUnicodeNormalization.cpp
# (y las dos de AK que piden LibUnicode, fuera de TestAK: TestString y TestUtf16String)
biblio tak "$TL/AK" "-I$TL/AK" TestString.cpp TestUtf16String.cpp
biblio tak "$AQUI/hispa64" "" gcc-utf16view.cpp
biblio turl "$TL/LibURL" "-I$G/LibURL" TestPublicSuffix.cpp TestURL.cpp TestURLPattern.cpp
biblio tipc "$TL/LibIPC" "-I$G/LibIPC" TestConnection.cpp TestDecoder.cpp TestFile.cpp TestTransportSocket.cpp
# (LADYBIRD_SOURCE_DIR: donde las pruebas buscan sus archivos; en Hispa-64 y
# en Linux, /tmp/lb, donde MLB deja los que lleva dentro)
biblio tcrypto "$TL/LibCrypto" "" TestEd25519.cpp TestHash.cpp TestHMAC.cpp TestRSA.cpp
biblio tgfx "$TL/LibGfx" "-I$G/LibGfx '-DLADYBIRD_SOURCE_DIR=\"/tmp/lb\"'" TestColor.cpp TestFont.cpp TestFontCatalog.cpp TestImageWriter.cpp TestRect.cpp \
  TestSharedFontProvider.cpp TestWOFF.cpp TestWOFF2.cpp TestYUVData.cpp TestBitmapSequence.cpp TestShareableBitmap.cpp TestImageDecoder.cpp
if [ -s "$LIS" ]; then
  echo "hazlb.sh: $(wc -l < "$LIS") archivos de Ladybird que compilar, con $HILOS a la vez" >&2
  # shellcheck disable=SC2016
  xargs -d '\n' -P "$HILOS" -I{} sh -c 'nice {} || { echo "hazlb.sh: falla: {}" | cut -c1-400 >&2; exit 1; }' < "$LIS" || { echo "hazlb.sh: no se compila todo" >&2; exit 2; }
fi
# ---- 5. los programas de prueba ----
BASE="$AKO/test_TestMain.o $AKO/test_TestSuite.o $AKO/test_AssertionHandler.o $AKO/test_TemporaryDirectory.o"
ARCH="$OB/unicode_*.o $OB/core_*.o $OB/regex_*.o $OB/url_*.o $OB/textcodec_*.o $OB/compress_*.o $OB/ipc_*.o $OB/gfx_*.o $OB/gfxipc_*.o $OB/imgdec_*.o $OB/imgdecg_*.o $OB/crypto_*.o $OB/cryptoc_*.o \
 $AKO/core_*.o $AKO/fs_*.o $AKO/th_*.o $AKO/ak_*.o"
rm -f "$O/liblb.a"; ar rcs "$O/liblb.a" $ARCH
LIBS="$B/bearssl64-obj/libbearssl.a $B/avif-obj/libavif.a $B/dav1d-obj/libdav1d.a $B/libwebp-obj/libwebp.a $B/icu-obj/libicu.a $B/psl-obj/libpsl.a $B/harfbuzz-obj/libharfbuzz.a $B/freetype-obj/libfreetype.a $B/woff2-obj/libwoff2.a $B/brotlidec-obj/libbrotlidec.a \
 $B/libjpeg-obj/libjpeg.a $B/zlib-obj/libz.a $B/simdutf-obj/libsimdutf.a $B/zmij-obj/libzmij.a $B/mimalloc-obj/libmimalloc.a"
# Un solo programa, TestLB, con las pruebas de las cuatro (como TestAK: cada
# archivo de pruebas, un grupo; LibTest lanza un proceso por grupo), con los
# datos de ICU dentro, como el ICU::data que enlaza Ladybird (los de todos los
# idiomas: 27 MB, hechos como los pequeños de terceros/icu/LEEME.txt pero con
# terceros/icu/filtro-ladybird.json, en build/icu-datos/icudt78l-ladybird.dat;
# MICU y Skia siguen con los pequeños), en vez de stubdata
IV=$(sed -n 's/^#define U_ICU_VERSION "\(.*\)"$/\1/p' "$IC/common/unicode/uvernum.h")
IDAT=$B/icu-datos/icudt${IV%%.*}l-ladybird.dat
[ -f "$IDAT" ] || { echo "hazlb.sh: faltan los datos de ICU de Ladybird, $IDAT (terceros/icu/LEEME.txt)" >&2; exit 2; }
printf '\t.section .rodata\n\t.balign 16\n\t.globl icudt%s_dat\n\t.type icudt%s_dat, @object\nicudt%s_dat:\n\t.incbin "%s"\n\t.size icudt%s_dat, .-icudt%s_dat\n\t.section .note.GNU-stack,"",@progbits\n' \
  "${IV%%.*}" "${IV%%.*}" "${IV%%.*}" "$IDAT" "${IV%%.*}" "${IV%%.*}" > "$O/icudatos.s"
"$CC" -c "$O/icudatos.s" -o "$O/icudatos.o"
rm -f "$O/libicusin.a"; ar rcs "$O/libicusin.a" $(ls "$B"/icu-obj/*.o | grep -v '/stubdata_')
LIBS=${LIBS/$B\/icu-obj\/libicu.a/$O/libicusin.a}
CRATES="libimagedecoders_rust libgfx_rust libipc_rust libtextcodec_rust liburl_rust libregex_rust libunicode_rust"
R_A=""; R_I=""
for c in $CRATES; do R_A="$R_A $O/lib$c.a"; R_I="$R_I $OB/init_$c.o"; done
# shellcheck disable=SC2086
nice "$CXX" -static -pthread -Wl,--allow-multiple-definition -o "$O/sal/TestLB" $OB/tunicode_*.o $OB/tak_*.o $OB/turl_*.o $OB/tipc_*.o $OB/tcrypto_*.o $OB/tgfx_*.o $BASE $R_I \
  "$O/icudatos.o" -Wl,--start-group "$O/liblb.a" $R_A $LIBS -Wl,--end-group
"$X-strip" --strip-unneeded "$O/sal/TestLB"
cp "$O/sal/TestLB" "$S/"
haz_mlb
echo "$ID" > "$O/.compilador"
echo "hechos: TestLB MLB"
