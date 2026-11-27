#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/ladybird/hazak.sh - Hispa-64, etapa 10 e (docs/64/HISPA-64.md,
# sección 22): las bibliotecas de base de Ladybird (terceros/ladybird: AK,
# LibCore, LibFileSystem, LibThreading y LibTest), compiladas con el
# compilador cruzado como las compila Ladybird (C++23, sus opciones de GCC),
# y sus pruebas de AK (Tests/AK: TestAK, las de Ladybird de un solo programa,
# como su «ladybird_test_suite»), en SALIDA con MAK (mak.c), que las corre en
# Hispa-64 y dice el resumen. Con fast_float, simdutf, zmij y mimalloc de
# libs/hazlibs.sh y las cabeceras de llvm-libc (terceros/llvm-libc: las
# funciones de matemáticas de AK).
# Sin LibUnicode (pide ICU 78 y Rust: más adelante), sin TimeZone de LibCore
# ni las pruebas TestString y TestUtf16String, que la piden.
# Lo que Ladybird hace con CMake (Debug.h, Backtrace.h, BrowserVersion.h, los
# Export.h), aquí, en build/ladybird-obj/gen.
# Uso: hazak.sh SALIDA   (HILOS=N: N a la vez; con uno, en el servidor, unos
# 20 minutos la primera vez). La última línea dice lo que ha hecho.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
T=$R/terceros
L=$T/ladybird
S=$1
[ -n "$S" ] || { echo "uso: hazak.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
CXX=$CR/bin/x86_64-hispa64-musl-g++
CC=$CR/bin/x86_64-hispa64-musl-gcc
[ -x "$CXX" ] || { echo "hechos: (sin el compilador cruzado: hazgcc.sh)"; exit 0; }
for l in simdutf zmij mimalloc; do
  [ -f "$B/$l-obj/lib$l.a" ] || { echo "hazak.sh: falta $B/$l-obj/lib$l.a (libs/hazlibs.sh)" >&2; exit 2; }
done
ID=$("$CXX" --version | head -1)
HILOS=${HILOS:-1}
O=$B/ladybird-obj
G=$O/gen
if [ "$(cat "$O/.compilador" 2> /dev/null)" != "$ID" ] || [ "$L/LEEME.txt" -nt "$O/.compilador" ] || [ "$AQUI/hazak.sh" -nt "$O/.compilador" ]; then rm -rf "$O"; fi
mkdir -p "$G/AK" "$G/LibCore"
# lo que hace el CMake de Ladybird: los DEBUG, a 0; sin backtrace; los X_API, vacíos (estático)
sed 's/#\( *\)cmakedefine01 \([A-Za-z0-9_]*\)/#\1define \2 0/' "$L/AK/Debug.h.in" > "$G/AK/Debug.h"
sed 's/#cmakedefine Backtrace_FOUND//' "$L/AK/Backtrace.h.in" > "$G/AK/Backtrace.h"
for l in Core:CORE FileSystem:FILESYSTEM Test:TEST Threading:THREADING; do
  mkdir -p "$G/Lib${l%%:*}"; printf '#pragma once\n#define %s_API\n#define %s_NO_EXPORT\n' "${l#*:}" "${l#*:}" > "$G/Lib${l%%:*}/Export.h"
done
sed 's/@LADYBIRD_BROWSER_VERSION@/1.0/; s/@CMAKE_BUILD_TYPE@/Release/; s/@browser_product_name@/Ladybird/; s/@browser_release_stage@/Hispa-64/; s/@browser_is_release_build@/false/; s/@browser_is_final_release@/false/' \
  "$L/Libraries/LibCore/BrowserVersion.h.in" > "$G/LibCore/BrowserVersion.h"
# las opciones de Ladybird para GCC (Meta/CMake/compile_options.cmake), sin -Werror
CF="-std=c++23 -O2 -fno-exceptions -fsigned-char -ffp-contract=off -fno-semantic-interposition -fstack-protector-strong -fstrict-flex-arrays=2 \
 -ftrivial-auto-var-init=zero -fvisibility-inlines-hidden -w -I$L -I$L/Libraries -I$G -I$AQUI/compat -I$T/fast_float/include -I$T/simdutf -I$T/zmij -I$T/mimalloc/include"
AKF="Assertions Base64 ByteString ByteStringImpl CircularBuffer CountingStream Demangle Error FFIHelpers FlyString Format GenericLexer Hex JsonArray
 JsonObject JsonParser JsonValue MathFunctions MemoryStream NumberFormat OptionParser Random StackInfo Stream String StringBase StringBuilder
 StringConversions StringUtils StringView ThreadID Time Utf16FlyString Utf16String Utf16StringBuilder Utf16StringData Utf16View Utf8View kmalloc
 LexicalPath ConditionVariablePOSIX MutexPOSIX RWLockPOSIX"
COREF="AddressInfoVector ArgsParser ConfigFile CrashReportData DirIterator Directory DirectoryEntry ElapsedTimer Environment EventLoop
 EventLoopImplementation EventReceiver File GeolocationAuthorization GeolocationProvider ImmutableBytes MimeData Notifier ReportTime Resource
 ResourceImplementation ResourceImplementationFile SharedVersion SocketAddress StandardPaths StatsLog SystemServerTakeover ThreadEventQueue Timer
 Version GeolocationProviderUnimplemented AnonymousBuffer EventLoopImplementationUnix FilePosix LocalServer MappedFile Process Socket System
 TCPServer UDPServer FileWatcherInotify Platform/MemoryPressureLinux Platform/ProcessStatisticsLinux TimeZoneWatcherUnix"
LIS=$O/.lista
: > "$LIS"
cambio=0
a_lista() {   # FUENTE OBJETO [OPCIONES]
  if [ ! -f "$2" ] || [ "$1" -nt "$2" ]; then echo "$CXX $CF $3 -c $1 -o $2" >> "$LIS"; cambio=1; fi
}
for f in $AKF; do a_lista "$L/AK/$f.cpp" "$O/ak_$f.o" "$( [ "$f" = MathFunctions ] && echo "-isystem $T/llvm-libc")"; done
for f in $COREF; do a_lista "$L/Libraries/LibCore/$f.cpp" "$O/core_$(echo "$f" | tr / _).o"; done
a_lista "$L/Libraries/LibFileSystem/FileSystem.cpp" "$O/fs_FileSystem.o"
for f in "$L"/Libraries/LibThreading/*.cpp; do a_lista "$f" "$O/th_$(basename "$f" .cpp).o"; done
for f in TemporaryDirectory TestSuite TestMain AssertionHandler; do a_lista "$L/Libraries/LibTest/$f.cpp" "$O/test_$f.o"; done
TS=$(sed -n '/set(AK_TEST_SOURCES/,/^)/p' "$L/Tests/AK/CMakeLists.txt" | grep -o 'Test[A-Za-z0-9]*\.cpp' | grep -v '^TestString\.cpp$\|^TestUtf16String\.cpp$')
for f in $TS TestLexicalPath.cpp; do a_lista "$L/Tests/AK/$f" "$O/t_${f%.cpp}.o" "-I$L/Tests/AK"; done
if [ -s "$LIS" ]; then
  echo "hazak.sh: $(wc -l < "$LIS") archivos de Ladybird que compilar, con $HILOS a la vez" >&2
  # shellcheck disable=SC2016
  xargs -d '\n' -P "$HILOS" -I{} sh -c 'nice {} || { echo "hazak.sh: falla: {}" >&2; exit 255; }' < "$LIS"
fi
echo "$ID" > "$O/.compilador"
mkdir -p "$S"
if [ $cambio = 1 ] || [ ! -f "$S/TestAK" ]; then
  nice "$CXX" -static -pthread -o "$S/TestAK" "$O"/t_*.o "$O"/test_*.o "$O"/core_*.o "$O"/fs_*.o "$O"/th_*.o "$O"/ak_*.o \
    "$B/simdutf-obj/libsimdutf.a" "$B/zmij-obj/libzmij.a" "$B/mimalloc-obj/libmimalloc.a"
  "$CR/bin/x86_64-hispa64-musl-strip" --strip-unneeded "$S/TestAK"
fi
"$CC" -O2 -Wall -fno-stack-protector -static -o "$S/MAK" "$AQUI/mak.c"
"$CR/bin/x86_64-hispa64-musl-strip" --strip-unneeded "$S/MAK"
echo "hechos: TestAK MAK"
