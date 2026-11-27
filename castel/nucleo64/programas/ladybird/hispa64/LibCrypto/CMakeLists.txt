# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/LibCrypto/CMakeLists.txt - Hispa-64, etapa 10 g (docs/64/HISPA-64.md,
# sección 24): la LibCrypto de Hispa-64 (LEEME.txt: la de Ladybird, con sus
# mismas cabeceras, sobre BearSSL y monocypher) en el CMake de Ladybird, en
# vez de Libraries/LibCrypto (la de OpenSSL).
set(T ${HISPA64_R}/terceros)
set(SOURCES
    Hash.cpp
    HMAC.cpp
    RSA.cpp
    EC.cpp
    EdwardsCurve.cpp
    sha3.c
    ${T}/monocypher/monocypher.c
    ${T}/monocypher/monocypher-ed25519.c
)
ladybird_lib(LibCrypto crypto)
target_include_directories(LibCrypto PRIVATE ${T}/monocypher ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LibCrypto PUBLIC BearSSL::BearSSL)
target_link_libraries(LibCrypto PRIVATE LibCore)
