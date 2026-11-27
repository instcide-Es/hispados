# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/cmake/check_for_dependencies.cmake - Hispa-64, etapa 10 g
# (docs/64/HISPA-64.md, sección 24): lo que hace el de Ladybird
# (Meta/CMake/check_for_dependencies.cmake, con vcpkg o los paquetes del
# sistema), pero con nuestras bibliotecas: las de build/*-obj (compiladas con
# el cruzado por libs/hazlibs.sh, hazimg.sh, hazcxx.sh, hazskia.sh,
# hazcurl.sh) y sus cabeceras de terceros/. Va delante en CMAKE_MODULE_PATH,
# así que «include(check_for_dependencies)» de Ladybird toma este; no se
# cambia nada de Ladybird.
# Lo que Hispa-64 no tiene (ni hace falta para el navegador sin ventana):
# OpenSSL (se usa BearSSL, por nuestra LibCrypto y curl), ANGLE y Vulkan
# (Skia dibuja por el procesador), FFmpeg y SDL (sin vídeo, sin mandos),
# fontconfig (las letras, por su ruta: PathFontProvider), D-Bus, PulseAudio,
# libedit y Qt: objetos vacíos con el mismo nombre, para que el CMake de
# Ladybird se configure; lo que los usa de verdad no se compila (hazweb.sh
# dice qué objetivos compilar) o va por la capa hispa64/.
include_guard()

set(R "${HISPA64_R}")
set(B "${R}/build")
set(T "${R}/terceros")
set(HISPA64_CAPA "${CMAKE_CURRENT_LIST_DIR}/..")

find_package(PkgConfig)
# pkg_check_modules del resto del CMake de Ladybird (LibWebView: dbus-1;
# LibGfx: libdrm...): sin pkg-config, lo que ya está aquí abajo como objeto
# importado (PkgConfig::NOMBRE), o un objeto vacío
# (lo que no está: sin «encontrado», para que Ladybird no lo use si puede:
# PulseAudio; dbus-1 lo pide sí o sí: hispa64/incluir/dbus)
macro(pkg_check_modules prefijo)
    if (TARGET PkgConfig::${prefijo})
        set(${prefijo}_FOUND ON)
    else()
        add_library(PkgConfig::${prefijo} INTERFACE IMPORTED GLOBAL)
        set(${prefijo}_FOUND OFF)
    endif()
endmacro()

# una biblioteca nuestra como objeto importado: NOMBRE ARCHIVO.a CABECERAS...
function(h64_biblio nombre archivo)
    if (NOT TARGET ${nombre})
        if (archivo)
            add_library(${nombre} STATIC IMPORTED GLOBAL)
            set_target_properties(${nombre} PROPERTIES IMPORTED_LOCATION "${archivo}")
        else()
            add_library(${nombre} INTERFACE IMPORTED GLOBAL)
        endif()
    endif()
    if (ARGN)
        set_property(TARGET ${nombre} APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ARGN})
    endif()
endfunction()
# lo que no hay: un objeto vacío
function(h64_vacio nombre)
    if (NOT TARGET ${nombre})
        add_library(${nombre} INTERFACE IMPORTED GLOBAL)
    endif()
endfunction()

# ---- AK ----
h64_biblio(mimalloc "${B}/mimalloc-obj/libmimalloc.a" "${T}/mimalloc/include")
h64_biblio(FastFloat::fast_float "" "${T}/fast_float/include")
h64_biblio(simdutf::simdutf "${B}/simdutf-obj/libsimdutf.a" "${T}/simdutf")
h64_biblio(zmij::zmij "${B}/zmij-obj/libzmij.a" "${T}/zmij")
# (Ladybird busca «llvm-libc/shared/math.h»: web-obj/llvm/llvm-libc, un enlace
# a terceros/llvm-libc, que hace hazweb.sh)
set(LLVM_LIBC_INCLUDE_DIR "${B}/web-obj/llvm" CACHE PATH "" FORCE)
# (sin cpptrace ni backtrace: AK no da la pila al pararse, como hazak.sh)
set(cpptrace_FOUND OFF)
set(Backtrace_FOUND OFF)

# ---- LibGfx ----
h64_biblio(freetype "${B}/freetype-obj/libfreetype.a" "${T}/freetype/include")
h64_biblio(harfbuzz "${B}/harfbuzz-obj/libharfbuzz.a" "${T}/harfbuzz/src" "${B}/web-obj/inc")
set_property(TARGET harfbuzz APPEND PROPERTY INTERFACE_LINK_LIBRARIES freetype)
h64_biblio(harfbuzz::harfbuzz "" )
set_property(TARGET harfbuzz::harfbuzz APPEND PROPERTY INTERFACE_LINK_LIBRARIES harfbuzz)
h64_biblio(JPEG::JPEG "${B}/libjpeg-obj/libjpeg.a" "${T}/libjpeg-turbo/src" "${T}/libjpeg-turbo/config")
h64_biblio(dav1d "${B}/dav1d-obj/libdav1d.a")
h64_biblio(avif "${B}/avif-obj/libavif.a" "${T}/libavif/include")
set_property(TARGET avif APPEND PROPERTY INTERFACE_LINK_LIBRARIES dav1d)
set(LIBAVIF_FOUND ON)
h64_biblio(WebP::webp "${B}/libwebp-obj/libwebp.a" "${T}/libwebp/src")
foreach (w webpdecoder webpdemux libwebpmux sharpyuv)
    h64_biblio(WebP::${w} "")
    set_property(TARGET WebP::${w} APPEND PROPERTY INTERFACE_LINK_LIBRARIES WebP::webp)
endforeach()
set(WebP_FOUND ON)
# wuffs: la capa hispa64/wuffs (la 0.4, con el nombre de la 0.3)
set(WUFFS_INCLUDE_DIR "${HISPA64_CAPA};${T}/wuffs/release/c" CACHE STRING "" FORCE)
h64_biblio(brotlidec "${B}/brotlidec-obj/libbrotlidec.a")
h64_biblio(PkgConfig::WOFF2 "${B}/woff2-obj/libwoff2.a" "${T}/woff2/include")
set_property(TARGET PkgConfig::WOFF2 APPEND PROPERTY INTERFACE_LINK_LIBRARIES brotlidec)
set(PDFJS_FOUND OFF)
# FFmpeg: solo sus cabeceras (terceros/ffmpeg-cabeceras); LibMediaCodecs lo
# carga al correr con dlopen(), que en un programa estático no lo encuentra
foreach (f AVCODEC AVUTIL LIBSWRESAMPLE)
    h64_biblio(PkgConfig::${f} "" "${T}/ffmpeg-cabeceras")
endforeach()
function(ladybird_use_bundled_ffmpeg_headers target)
    target_include_directories(${target} SYSTEM PRIVATE "${T}/ffmpeg-cabeceras")
endfunction()
# sin fontconfig: las letras por su ruta
h64_vacio(Fontconfig::Fontconfig)
set(HAS_FONTCONFIG OFF CACHE BOOL "" FORCE)
set(HAS_VULKAN OFF CACHE BOOL "" FORCE)
set(USE_VULKAN_DMABUF_IMAGES OFF CACHE BOOL "" FORCE)
set(HAS_DIRECTX OFF CACHE BOOL "" FORCE)

# ---- la red, ICU, sqlite... ----
h64_biblio(zstd "${B}/zstd-obj/libzstd.a" "${T}/zstd/lib")
h64_biblio(ZLIB::ZLIB "${B}/zlib-obj/libz.a" "${T}/zlib")
h64_biblio(BearSSL::BearSSL "${B}/bearssl64-obj/libbearssl.a" "${T}/bearssl/inc")
h64_biblio(PkgConfig::LIBPSL "${B}/psl-obj/libpsl.a" "${T}/libpsl/include")
h64_biblio(CURL::libcurl "${B}/curl-obj/libcurl.a" "${T}/curl/include")
set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_LINK_LIBRARIES BearSSL::BearSSL ZLIB::ZLIB)
set_property(TARGET CURL::libcurl APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS CURL_STATICLIB)
# ICU: la 78.3 entera en una (build/icu-obj/libicu.a, sin los datos de
# stubdata: los de todos los idiomas, como en hazlb.sh, en icudatos.o)
h64_biblio(ICU::uc "${B}/web-obj/libicusin.a" "${T}/icu/source/common")
set_property(TARGET ICU::uc APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS U_STATIC_IMPLEMENTATION U_USING_ICU_NAMESPACE=0)
h64_biblio(ICU::i18n "" "${T}/icu/source/i18n")
set_property(TARGET ICU::i18n APPEND PROPERTY INTERFACE_LINK_LIBRARIES ICU::uc)
h64_biblio(ICU::data "${B}/web-obj/libicudatos.a")
set(ICU_FOUND ON)
h64_biblio(SQLite3::SQLite3 "${B}/sqlite-obj/libsqlite.a" "${T}/sqlite")
h64_biblio(SQLite::SQLite3 "")
set_property(TARGET SQLite::SQLite3 APPEND PROPERTY INTERFACE_LINK_LIBRARIES SQLite3::SQLite3)
h64_biblio(simdjson::simdjson "${B}/simdjson-obj/libsimdjson.a" "${T}/simdjson")
find_package(Threads REQUIRED)
# lo que no hay
h64_vacio(OpenSSL::Crypto)
h64_vacio(OpenSSL::SSL)
h64_vacio(SDL3::SDL3)
# (libedit: lo poco que pide «js», en hispa64/editline/readline.h)
h64_biblio(PkgConfig::LIBEDIT "" "${HISPA64_CAPA}")
h64_vacio(PkgConfig::DBUS)
# ANGLE: solo las cabeceras de EGL y GLES (terceros/angle-cabeceras, sin
# X11); las funciones, en hispa64/singl.c (no hay EGL: Skia por el procesador)
h64_biblio(PkgConfig::angle "" "${T}/angle-cabeceras")
set_property(TARGET PkgConfig::angle APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS EGL_NO_X11)
set(ANGLE_TARGETS PkgConfig::angle)

find_package(Git REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Skia: build/skia-obj/libskia.a (hazskia.sh: por el procesador, sin GPU)
h64_biblio(PkgConfig::skia "${B}/skia-obj/libskia.a" "${T}/skia" "${T}/skia/include")
set_property(TARGET PkgConfig::skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES freetype JPEG::JPEG WebP::webp ZLIB::ZLIB)
set(SKIA_TARGET PkgConfig::skia)
add_library(skia ALIAS PkgConfig::skia)
