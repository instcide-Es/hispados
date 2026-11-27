# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/navegador/CMakeLists.txt - Hispa-64, etapa 10 h (docs/64/HISPA-64.md,
# sección 25): NAVEGADOR, la interfaz de Ladybird para el escritorio de
# Hispa-64 (navegador.cpp), con el Hub de Ladybird detrás. Lleva la capa de
# las ventanas y la letra de la oficina (castel/nucleo32/oficina/comun, en C:
# ventana.c) y los mensajes del Hub (hubipc, de Services/Hub).
set(OFI "${HISPA64_R}/castel/nucleo32/oficina/comun")
set(VEN "${HISPA64_R}/castel/nucleo64/programas/ventanas")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# los talones de los mensajes del Hub que no se atienden (talones.py)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/talones.h"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/talones.py"
            "${LADYBIRD_SOURCE_DIR}/Services/Hub/HubClient.ipc" "${CMAKE_CURRENT_SOURCE_DIR}/conexion.h"
            "${CMAKE_CURRENT_BINARY_DIR}/talones.h"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/talones.py" "${CMAKE_CURRENT_SOURCE_DIR}/conexion.h"
            "${LADYBIRD_SOURCE_DIR}/Services/Hub/HubClient.ipc"
    VERBATIM)
# las DejaVu de la oficina, dentro (haz_letras.py)
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/letras_dejavu.c"
    COMMAND "${Python3_EXECUTABLE}" -I "${OFI}/haz_letras.py" "${HISPA64_R}/terceros/dejavu" "${CMAKE_CURRENT_BINARY_DIR}/letras_dejavu.c"
    DEPENDS "${OFI}/haz_letras.py"
    VERBATIM)

add_library(navoficina STATIC
    ventana.c
    "${OFI}/texto.c" "${OFI}/ttf.c" "${OFI}/letras.c" "${OFI}/lienzo.c"
    "${CMAKE_CURRENT_BINARY_DIR}/letras_dejavu.c")
target_compile_definitions(navoficina PRIVATE OFI_HISPA64)
target_compile_options(navoficina PRIVATE -std=gnu11 -w)
target_include_directories(navoficina PRIVATE "${OFI}" "${VEN}" "${HISPA64_R}/terceros/miniz" "${HISPA64_R}/terceros/stb")

add_executable(Navegador navegador.cpp "${CMAKE_CURRENT_BINARY_DIR}/talones.h")
add_dependencies(Navegador hubipc)
target_include_directories(Navegador PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}"
    "${LADYBIRD_SOURCE_DIR}/Services" "${CMAKE_BINARY_DIR}/Services")
target_link_libraries(Navegador PRIVATE hubipc LibCore LibGfx LibIPC LibMain LibURL LibWebView LibWebCommon LibHTTP navoficina m)
