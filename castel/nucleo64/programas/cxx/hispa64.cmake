# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# nucleo64/programas/cxx/hispa64.cmake - Hispa-64, etapa 10 d (docs/64/HISPA-64.md,
# sección 21): el archivo de «toolchain» de CMake para compilar para
# Hispa-64 con el compilador cruzado (x86_64-hispa64-linux-musl, de
# hazgcc.sh: build/cruzado), lo que pide Ladybird (y Skia y las demás
# bibliotecas) para compilarse fuera de Hispa-64:
#   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=.../hispa64.cmake ...
# HISPA64_CRUZADO (en el entorno o con -D) dice dónde está el compilador; si
# no, build/cruzado del repositorio.
set(CMAKE_SYSTEM_NAME Linux)            # (para CMake, Hispa-64 es un Linux: los números y las formas de Linux x86-64)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(HISPA64 ON)

if(NOT HISPA64_CRUZADO)
  if(DEFINED ENV{HISPA64_CRUZADO})
    set(HISPA64_CRUZADO $ENV{HISPA64_CRUZADO})
  else()
    get_filename_component(HISPA64_CRUZADO "${CMAKE_CURRENT_LIST_DIR}/../../../../build/cruzado" ABSOLUTE)
  endif()
endif()
set(_t "${HISPA64_CRUZADO}/bin/x86_64-hispa64-linux-musl")
set(CMAKE_C_COMPILER "${_t}-gcc")
set(CMAKE_CXX_COMPILER "${_t}-g++")
set(CMAKE_AR "${_t}-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_t}-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP "${_t}-strip" CACHE FILEPATH "")

# lo del compilador, solo; las bibliotecas y las cabeceras del anfitrión, nunca
set(CMAKE_SYSROOT "${HISPA64_CRUZADO}/x86_64-hispa64-linux-musl")
set(CMAKE_FIND_ROOT_PATH "${HISPA64_CRUZADO}/x86_64-hispa64-linux-musl")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# los programas, estáticos salvo que se pida otra cosa (HISPA64_DINAMICO):
# en Hispa-64, el cargador dinámico es /lib/ld-musl-x86_64.so.1
if(NOT HISPA64_DINAMICO)
  set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
else()
  set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-dynamic-linker,/lib/ld-musl-x86_64.so.1")
endif()
