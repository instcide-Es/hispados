#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hazgcc.sh - Hispa-64, etapa 10 c (docs/64/HISPA-64.md,
# sección 20): el compilador cruzado de Hispa-64, x86_64-hispa64-linux-musl
# (con los nombres cortos x86_64-hispa64-musl-gcc y -g++): binutils 2.44,
# GCC 14.3.0 (C y C++, con la libstdc++ compilada para musl, estática y
# compartida) y musl 1.2.6 (terceros/musl, con sus parches) en su raíz
# (sysroot), al estilo de musl-cross-make. Los programas del anfitrión van
# estáticos (corren en cualquier Linux x86-64: se hace en el PC de Albinyana
# y se usa en el servidor) y la carpeta se puede mover (todo relativo).
#
# Uso: hazgcc.sh [PREFIJO]      (por omisión, build/cruzado)
#   HILOS=12 hazgcc.sh ...      make -jHILOS (por omisión, 1)
#   FUENTES=carpeta             donde están (o se bajan) los tar de GNU
#   TRABAJO=carpeta             donde se compila (por omisión, build/gcc-obj)
# Los tar se comprueban con su SHA-256 (abajo; las firmas de GNU, comprobadas
# con gnu-keyring.gpg el 26/11/2026). Sin red, que estén ya en FUENTES.
# Al acabar deja PREFIJO/LEEME.txt con las versiones y PREFIJO.tar.xz.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
T=x86_64-hispa64-linux-musl
C=x86_64-hispa64-musl
P=${1:-$R/build/cruzado}
F=${FUENTES:-$R/build/gcc-fuentes}
W=${TRABAJO:-$R/build/gcc-obj}
J=${HILOS:-1}
MUSL=$R/terceros/musl
declare -A SUMA=(
  [binutils-2.44.tar.xz]=ce2017e059d63e67ddb9240e9d4ec49c2893605035cd60e92ad53177f4377237
  [gcc-14.3.0.tar.xz]=e0dc77297625631ac8e50fa92fffefe899a4eb702592da5c32ef04e2293aca3a
  [gmp-6.3.0.tar.xz]=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898
  [mpfr-4.2.2.tar.xz]=b67ba0383ef7e8a8563734e2e889ef5ec3c3b898a01d00fa0a6869ad81c6ce01
  [mpc-1.3.1.tar.gz]=ab642492f5cf882b74aa0cb730cd410a81edcdbec895183ce930e706c1c759b8
  [m4-1.4.19.tar.xz]=63aede5c6d33b6d9b13511cd0be2cac046f2e70fd0a07aa9573a04a82783af96
)
declare -A DONDE=(
  [binutils-2.44.tar.xz]=binutils [gcc-14.3.0.tar.xz]=gcc/gcc-14.3.0 [gmp-6.3.0.tar.xz]=gmp
  [mpfr-4.2.2.tar.xz]=mpfr [mpc-1.3.1.tar.gz]=mpc [m4-1.4.19.tar.xz]=m4
)
mkdir -p "$F" "$W"
for a in "${!SUMA[@]}"; do
  [ -f "$F/$a" ] || curl -sSfL -o "$F/$a" "https://ftp.gnu.org/gnu/${DONDE[$a]}/$a"
  echo "${SUMA[$a]}  $F/$a" | sha256sum -c --quiet - || { echo "hazgcc.sh: $a no es el que debe" >&2; exit 2; }
done
# GCC 14 compilado con un GCC de anfitrión nuevo (el 15 del PC: C23 por omisión)
HCC="gcc -std=gnu17"
HCXX="g++ -std=gnu++17"
ES="-static --static"
cd "$W"
desempaqueta() { [ -d "$2" ] || { tar xf "$F/$1"; }; }
desempaqueta binutils-2.44.tar.xz binutils-2.44
desempaqueta gcc-14.3.0.tar.xz gcc-14.3.0
desempaqueta gmp-6.3.0.tar.xz gmp-6.3.0
desempaqueta mpfr-4.2.2.tar.xz mpfr-4.2.2
desempaqueta mpc-1.3.1.tar.gz mpc-1.3.1
desempaqueta m4-1.4.19.tar.xz m4-1.4.19
for d in gmp-6.3.0 mpfr-4.2.2 mpc-1.3.1; do [ -e gcc-14.3.0/${d%-*} ] || ln -sfn ../$d gcc-14.3.0/${d%-*}; done
# m4, solo para compilar gmp (no va en el compilador)
if [ ! -x "$W/m4/bin/m4" ]; then
  mkdir -p o-m4 && (cd o-m4 && ../m4-1.4.19/configure --prefix="$W/m4" CC="$HCC" > c.log 2>&1 && make -j"$J" > m.log 2>&1 && make install > i.log 2>&1)
fi
export PATH="$W/m4/bin:$PATH"
S=$W/raiz                     # la raíz mientras se compila (musl, las cabeceras)
D=$W/destino                  # donde se instala (luego, a PREFIJO)
COMUN="--target=$T --prefix= --libdir=/lib --disable-multilib --with-sysroot=/$T --disable-werror --enable-deterministic-archives"
# 1. binutils
if [ ! -f o-binutils/.hecho ]; then
  rm -rf o-binutils && mkdir o-binutils
  (cd o-binutils && ../binutils-2.44/configure $COMUN --with-build-sysroot="$S" --disable-nls --disable-gprofng --disable-plugins \
       --disable-shared --enable-static CC="$HCC $ES" CXX="$HCXX $ES" > c.log 2>&1 \
   && make -j"$J" MAKEINFO=true > m.log 2>&1 && make MAKEINFO=true install DESTDIR="$D" > i.log 2>&1 && touch .hecho)
fi
export PATH="$D/bin:$PATH"
# 2. las cabeceras de musl en la raíz
if [ ! -f "$S/include/stdio.h" ]; then
  rm -rf o-muslh && mkdir -p o-muslh "$S/lib"
  (cd o-muslh && "$MUSL/configure" --prefix= --target=$T CC=gcc > c.log 2>&1 && make install-headers DESTDIR="$S" > i.log 2>&1)
fi
# 3. gcc, la primera parte (el compilador y la libgcc estática)
GCF="$COMUN --with-build-sysroot=$S --enable-languages=c,c++ --enable-tls --enable-threads=posix --enable-shared \
  --enable-initfini-array --enable-libstdcxx-time=rt --disable-libsanitizer --disable-libmudflap --disable-gnu-indirect-function \
  --disable-libmpx --disable-libquadmath --disable-libgomp --disable-libitm --disable-libvtv --disable-libssp --disable-nls \
  --with-native-system-header-dir=/include --disable-lto --disable-plugin --disable-bootstrap --with-pkgversion=Hispa-64 --enable-default-pie --enable-cxx-flags=-fno-stack-protector"
if [ ! -f o-gcc/.gcc ]; then
  rm -rf o-gcc && mkdir o-gcc
  (cd o-gcc && ../gcc-14.3.0/configure $GCF CC="$HCC $ES" CXX="$HCXX $ES" \
       AR_FOR_TARGET="$D/bin/$T-ar" AS_FOR_TARGET="$D/bin/$T-as" LD_FOR_TARGET="$D/bin/$T-ld" NM_FOR_TARGET="$D/bin/$T-nm" \
       OBJDUMP_FOR_TARGET="$D/bin/$T-objdump" RANLIB_FOR_TARGET="$D/bin/$T-ranlib" STRIP_FOR_TARGET="$D/bin/$T-strip" \
       READELF_FOR_TARGET="$D/bin/$T-readelf" > c.log 2>&1 \
   && make -j"$J" MAKEINFO=true all-gcc > m1.log 2>&1 && touch .gcc)
fi
# (la libgcc sin la compartida: aún no hay libc; enable_shared=no a todos los make de dentro, como musl-cross-make)
if [ ! -f o-gcc/.compilador ]; then
  rm -rf "o-gcc/$T/libgcc"
  (cd o-gcc && make -j"$J" MAKEINFO=true MAKE="make enable_shared=no" all-target-libgcc > m2.log 2>&1 && touch .compilador)
fi
# 4. musl, con ese compilador, en la raíz
if [ ! -f o-musl/.hecho ]; then
  rm -rf o-musl && mkdir o-musl
  XGCC="$W/o-gcc/gcc/xgcc -B $W/o-gcc/gcc"
  (cd o-musl && "$MUSL/configure" --prefix= --syslibdir=/lib --target=$T CC="$XGCC" CROSS_COMPILE="$T-" \
       LIBCC="$W/o-gcc/$T/libgcc/libgcc.a" > c.log 2>&1 \
   && make -j"$J" AR="$D/bin/$T-ar" RANLIB="$D/bin/$T-ranlib" > m.log 2>&1 \
   && make install DESTDIR="$S" AR="$D/bin/$T-ar" RANLIB="$D/bin/$T-ranlib" > i.log 2>&1 && touch .hecho)
fi
# 5. el resto de gcc: la libgcc compartida, la libstdc++ (estática y compartida)
if [ ! -f o-gcc/.hecho ]; then
  rm -rf "o-gcc/$T/libgcc"
  (cd o-gcc && make -j"$J" MAKEINFO=true > m3.log 2>&1 && make MAKEINFO=true install-strip DESTDIR="$D" > i.log 2>&1 && touch .hecho)
fi
# 6. musl en la raíz del destino; los nombres cortos; a PREFIJO
make -C o-musl install DESTDIR="$D/$T" AR="$D/bin/$T-ar" RANLIB="$D/bin/$T-ranlib" > o-musl/i2.log 2>&1
ln -sf libc.so "$D/$T/lib/ld-musl-x86_64.so.1"
(cd "$D/bin" && for h in "$T"-*; do ln -sf "$h" "$C${h#$T}"; done)
rm -rf "$D/share/info" "$D/share/man"
cat > "$D/LEEME.txt" << FIN
El compilador cruzado de Hispa-64 ($T; también $C-gcc y $C-g++),
hecho por castel/nucleo64/programas/cxx/hazgcc.sh el $(date +%d/%m/%Y) en $(uname -n):
binutils 2.44, GCC 14.3.0 (C y C++), musl $(cat "$MUSL/VERSION") (terceros/musl) en $T/.
Los programas del anfitrión son estáticos: corren en cualquier Linux x86-64.
Licencias: GPL-3.0 o posterior con la excepción de las bibliotecas de GCC
(libgcc, libstdc++: se pueden enlazar con programas GPL-2.0), y MIT (musl).
FIN
rm -rf "$P" && mkdir -p "$(dirname "$P")" && cp -a "$D" "$P"
tar -C "$(dirname "$P")" -cJf "$P.tar.xz" "$(basename "$P")"
echo "hazgcc.sh: hecho en $P ($("$P/bin/$C-g++" --version | head -1))"
