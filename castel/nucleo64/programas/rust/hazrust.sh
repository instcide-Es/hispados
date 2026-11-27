#!/bin/bash
# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/rust/hazrust.sh - Hispa-64, etapa 10 f (docs/64/HISPA-64.md,
# sección 23): los programas de Rust (MRUST, la prueba: mrust/), para el
# objetivo x86_64-unknown-linux-musl de Rust, pero enlazados con el
# compilador cruzado de Hispa-64 (build/cruzado) y NUESTRA musl
# (-C link-self-contained=no: sin los crt1.o ni la libc.a que trae Rust),
# estáticos (+crt-static).
# Con cargo (el Rust de rustup, la 1.98.0 que fija Ladybird en su
# rust-toolchain.toml, con «rustup target add x86_64-unknown-linux-musl»; en
# el PC de Albinyana, no en el servidor), se compila y se guarda en
# build/rust-obj; sin cargo, se usa lo que haya en build/rust-obj (copiado
# del PC, como los objetos de Skia); sin eso, nada.
# Uso: hazrust.sh SALIDA   (HILOS=N: cargo -jN). La última línea dice lo hecho.
set -e
AQUI=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$AQUI/../../../.." && pwd)
B=$R/build
O=$B/rust-obj
S=$1
[ -n "$S" ] || { echo "uso: hazrust.sh SALIDA" >&2; exit 2; }
CR=${HISPA64_CRUZADO:-$B/cruzado}
CC=$CR/bin/x86_64-hispa64-musl-gcc
CARGO=$(command -v cargo || true)
[ -z "$CARGO" ] && [ -x "$HOME/.cargo/bin/cargo" ] && CARGO=$HOME/.cargo/bin/cargo
mkdir -p "$S"
if [ -n "$CARGO" ] && [ -x "$CC" ]; then
  export PATH="$(dirname "$CARGO"):$PATH"
  export RUSTUP_TOOLCHAIN=${RUSTUP_TOOLCHAIN:-1.98.0}
  RV=$(rustc --version | cut -d' ' -f2)
  export MRUST_RUSTC=$RV
  export CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_LINKER=$CC
  export CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_RUSTFLAGS="-C link-self-contained=no -C target-feature=+crt-static -C relocation-model=static"
  # el desenrollador de la pila: el de GCC (libgcc_eh.a, con la excepción de
  # las bibliotecas de GCC), el mismo que usa la libstdc++; el crate «unwind»
  # de Rust pide -lunwind: se le da con ese nombre (en un programa con C++ y
  # Rust, como Ladybird, uno solo para los dos)
  mkdir -p "$O/enlaces"
  cp "$("$CC" -print-file-name=libgcc_eh.a)" "$O/enlaces/libunwind.a"
  export CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_RUSTFLAGS="$CARGO_TARGET_X86_64_UNKNOWN_LINUX_MUSL_RUSTFLAGS -L native=$O/enlaces"
  nice "$CARGO" build -q --release --locked --offline -j "${HILOS:-1}" --target x86_64-unknown-linux-musl \
    --manifest-path "$AQUI/mrust/Cargo.toml" --target-dir "$O/destino"
  cp "$O/destino/x86_64-unknown-linux-musl/release/mrust" "$O/MRUST"
  "$CR/bin/x86_64-hispa64-musl-strip" --strip-unneeded "$O/MRUST"
  echo "$RV" > "$O/.rustc"
fi
if [ -f "$O/MRUST" ]; then
  cp "$O/MRUST" "$S/MRUST"
  echo "hechos: MRUST"
else
  echo "hechos: (sin Rust: ni cargo ni build/rust-obj)"
fi
