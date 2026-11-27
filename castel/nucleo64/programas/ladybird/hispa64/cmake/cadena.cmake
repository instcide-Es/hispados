# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/cmake/cadena.cmake - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
# sección 24): la «cadena» de CMake para compilar Ladybird con su propio CMake
# para Hispa-64: el compilador cruzado (build/cruzado: GCC 14.3 con nuestra
# musl), todo estático, y Rust para x86_64-unknown-linux-musl con el mismo
# enlazador y sin la musl de Rust (como hazlb.sh).
# Lo usa hazweb.sh: cmake -DCMAKE_TOOLCHAIN_FILE=cadena.cmake -DHISPA64_R=...
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
if (NOT HISPA64_R)
    set(HISPA64_R "$ENV{HISPA64_R}")
endif()
set(HISPA64_R "${HISPA64_R}" CACHE PATH "La raíz del repositorio (build/ y terceros/)")
# (las pruebas del compilador de CMake leen esta cadena otra vez sin las -D:
# por el entorno)
set(ENV{HISPA64_R} "${HISPA64_R}")
set(HISPA64_CRUZADO "${HISPA64_R}/build/cruzado" CACHE PATH "El compilador cruzado")
set(CMAKE_C_COMPILER "${HISPA64_CRUZADO}/bin/x86_64-hispa64-musl-gcc")
set(CMAKE_CXX_COMPILER "${HISPA64_CRUZADO}/bin/x86_64-hispa64-musl-g++")
set(CMAKE_AR "${HISPA64_CRUZADO}/bin/x86_64-hispa64-musl-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${HISPA64_CRUZADO}/bin/x86_64-hispa64-musl-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP "${HISPA64_CRUZADO}/bin/x86_64-hispa64-musl-strip" CACHE FILEPATH "")
set(CMAKE_FIND_ROOT_PATH "${HISPA64_CRUZADO}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# (todo estático: ni bibliotecas compartidas ni programas dinámicos)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -pthread -Wl,--allow-multiple-definition")
# -w: sin avisos (Ladybird pide -Werror; con GCC 14 y musl salen avisos que no
# son fallos)
set(CMAKE_C_FLAGS_INIT "-w")
set(CMAKE_CXX_FLAGS_INIT "-w")
# Rust: la musl de Hispa-64 y nuestro enlazador (rust_crate.cmake pone
# CARGO_TARGET_..._LINKER al compilador de C)
set(RUST_TARGET_TRIPLE "x86_64-unknown-linux-musl" CACHE INTERNAL "Rust target triple")
# (--cap-lints=warn: sin el JIT, rustc ve código sin usar y, con -Dwarnings,
# pararía)
set(RUSTC_TARGET_CPU_FLAGS "-Clink-self-contained=no;-Ctarget-feature=+crt-static;--cap-lints=warn" CACHE STRING "" FORCE)
# lo nuestro que va en cada programa (hazweb.sh lo compila en
# build/web-obj/libh64capa.a): la pila del hilo principal (hispa64/pila.c)
set(CMAKE_C_STANDARD_LIBRARIES "-Wl,--wrap=pthread_getattr_np ${HISPA64_R}/build/web-obj/libh64capa.a" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "-Wl,--wrap=pthread_getattr_np ${HISPA64_R}/build/web-obj/libh64capa.a" CACHE STRING "" FORCE)
# las bibliotecas estáticas se piden unas a otras en los dos sentidos (LibGfx
# llama a libgfx_rust y libgfx_rust a LibGfx): todas entre --start-group y
# --end-group, como cuando se enlazan con bibliotecas compartidas
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> -Wl,--start-group <LINK_LIBRARIES> -Wl,--end-group")
