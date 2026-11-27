# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/cmake/raiz/CMakeLists.txt - Hispa-64, etapa 10 g
# (docs/64/HISPA-64.md, sección 24): la raíz del CMake de Ladybird para
# Hispa-64, en vez de la suya (terceros/ladybird/CMakeLists.txt), que pide la
# interfaz de Qt. Hace lo mismo que la suya (las mismas inclusiones de
# Meta/CMake, en el mismo orden), pero con solo lo que hace falta para el
# navegador sin ventana: AK, las bibliotecas, los servicios (Hub, que es el
# navegador sin ventana con --headless, WebContent, RequestServer,
# ImageDecoder y Compositor) y las herramientas de JavaScript; sin UI/ (Qt),
# sin WebDriver, sin las pruebas. Y con lo de Hispa-64 en vez de lo que no hay:
# LibCrypto, la nuestra (hispa64/LibCrypto, sobre BearSSL y monocypher), y los
# archivos de RequestServer que hablan con OpenSSL, los nuestros
# (hispa64/RequestServer). Lo de Ladybird no se cambia.
cmake_minimum_required(VERSION 3.25)

set(LB "${HISPA64_LADYBIRD}")
set(CAPA "${CMAKE_CURRENT_LIST_DIR}/../..")
include("${LB}/Meta/CMake/environment.cmake")
include("${LB}/Meta/CMake/use_linker.cmake")

project(ladybird
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "Ladybird Web Browser (Hispa-64)"
    HOMEPAGE_URL "https://ladybird.org"
)
if (POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# las fuentes de Ladybird, no esta carpeta
set(ladybird_SOURCE_DIR "${LB}")
set(LADYBIRD_SOURCE_DIR "${LB}")
list(APPEND CMAKE_MODULE_PATH "${LB}/Meta/CMake")

include(cmake_options)
include(compile_options)
include(check_for_dependencies)
include(ladybird_helper_processes)
include(revision)

add_library(JSClangPlugin INTERFACE)
add_library(GenericClangPlugin INTERFACE)

include_directories(BEFORE "${CAPA}/incluir")
include_directories(${LB})
include_directories(${LB}/Libraries)
include_directories(${LB}/Services)
include_directories(${CMAKE_BINARY_DIR})
include_directories(${CMAKE_BINARY_DIR}/Libraries)
include_directories(${CMAKE_BINARY_DIR}/Services)

include(lagom_install)
include(rust_crate)
include(licenses)
include(targets)
add_lagom_library_install_rules(GenericClangPlugin)
add_lagom_library_install_rules(JSClangPlugin)

add_subdirectory(${LB}/AK AK)
# (lo de GCC con el constructor consteval de Utf16View: hispa64/gcc-utf16view.cpp,
# en AK, que va en todos los programas)
target_sources(AK PRIVATE ${CAPA}/gcc-utf16view.cpp)

# Libraries/CMakeLists.txt de Ladybird, con nuestra LibCrypto
macro(biblio nombre)
    add_subdirectory(${LB}/Libraries/${nombre} Libraries/${nombre})
endmacro()
foreach (b LibCompress LibCore)
    biblio(${b})
endforeach()
add_subdirectory(${CAPA}/LibCrypto Libraries/LibCrypto)
foreach (b LibDNS LibDatabase LibDiff LibFileSystem LibGC LibHTTP LibIPC)
    biblio(${b})
endforeach()
set(LIBJS_FOLDED_INTO_LIBWEB ON)
foreach (b LibJS LibMain LibRegex LibRequests LibSandbox LibSyntax LibTest LibTextCodec LibThreading LibURL LibUnicode LibWasm
           LibWebRTC LibWebRTCClient LibWebSocket LibWakeLock LibXML
           LibDevTools LibGfx LibImageDecoders LibImageDecoderClient LibAudio LibMediaCodecs LibMedia LibMediaClient
           LibPlatformFeatures LibWebCommon LibCompositing LibWeb LibWebView LibGamepad)
    biblio(${b})
endforeach()

# (SystemFontMatcherNone.cpp de LibGfx no da ninguna letra para sans-serif,
# serif ni monospace: el nuestro, con las DejaVu)
set_source_files_properties(${LB}/Libraries/LibGfx/Font/SystemFontMatcherNone.cpp DIRECTORY ${LB}/Libraries/LibGfx
    TARGET_DIRECTORY LibGfx PROPERTIES HEADER_FILE_ONLY ON)
target_sources(LibGfx PRIVATE ${CAPA}/LibGfx/SystemFontMatcherHispa64.cpp)
# (FFmpegFunctions.cpp de LibMediaCodecs carga FFmpeg con dlopen: el nuestro,
# sin FFmpeg, hispa64/LibMediaCodecs/FFmpegFunctions.cpp)
set_source_files_properties(${LB}/Libraries/LibMediaCodecs/FFmpeg/FFmpegFunctions.cpp DIRECTORY ${LB}/Libraries/LibMediaCodecs
    TARGET_DIRECTORY LibMediaCodecs PROPERTIES HEADER_FILE_ONLY ON)
target_sources(LibMediaCodecs PRIVATE ${CAPA}/LibMediaCodecs/FFmpegFunctions.cpp)
# (Seccomp.cpp de LibSandbox pide _IOC_TYPEMASK, que la <sys/ioctl.h> de musl
# no da: el de las cabeceras de Linux)
set_source_files_properties(${LB}/Libraries/LibSandbox/Seccomp.cpp DIRECTORY ${LB}/Libraries/LibSandbox
    TARGET_DIRECTORY LibSandbox PROPERTIES COMPILE_OPTIONS "-include;linux/ioctl.h")

add_subdirectory(${LB}/Utilities Utilities)
# el corredor de las pruebas de LibJS (Tests/LibJS/CMakeLists.txt de Ladybird,
# sin las pruebas de CTest): lo usa MJS
ladybird_utility(test-js-runtime SOURCES ${LB}/Tests/LibJS/test-js-runtime.cpp LIBS LibJSStandalone LibMain)

# Services/CMakeLists.txt de Ladybird: los que lanza el navegador sin ventana
foreach (s AudioServer Compositor Hub ImageDecoder MediaServer SystemProbe VideoDecoderServer WebContent WebRTCClient)
    add_subdirectory(${LB}/Services/${s} Services/${s})
endforeach()
add_subdirectory(${CAPA}/RequestServer Services/RequestServer)
