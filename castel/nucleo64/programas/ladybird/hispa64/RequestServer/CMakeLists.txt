# Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
# hispa64/RequestServer/CMakeLists.txt - Hispa-64, etapa 10 g: el de
# Services/RequestServer de Ladybird, con CURL.cpp, AIA.cpp y
# CertificateExceptions.cpp de aquí (sin OpenSSL) y sin la caja de arena de
# Linux (seccomp y Landlock, que Hispa-64 no tiene: SandboxUnimplemented.cpp).
set(RS ${LADYBIRD_SOURCE_DIR}/Services/RequestServer)
set(SOURCES
    AIA.cpp
    CertificateExceptions.cpp
    CURL.cpp
    ${RS}/AlternativeServices.cpp
    ${RS}/ConnectionFromClient.cpp
    ${RS}/ControlConnectionFromClient.cpp
    ${RS}/DNSStreamCurl.cpp
    ${RS}/Request.cpp
    ${RS}/RequestPipe.cpp
    ${RS}/Resolver.cpp
    ${RS}/ResourceSubstitutionMap.cpp
    ${RS}/WebSocketImplCurl.cpp
    ${RS}/SandboxUnimplemented.cpp
)
set(GENERATED_SOURCES
    RequestClientEndpoint.h
    RequestServerControlClientEndpoint.h
    RequestServerControlEndpoint.h
    RequestServerEndpoint.h
)
# (los .ipc los compila LibRequests: generate_RequestServerEndpoint.h...)
add_library(requestserverservice STATIC ${SOURCES} ${GENERATED_SOURCES})
ladybird_generated_sources(requestserverservice)
add_executable(RequestServer ${RS}/main.cpp)
target_include_directories(requestserverservice PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../..)
target_include_directories(requestserverservice PRIVATE ${LADYBIRD_SOURCE_DIR}/Services/)
target_link_libraries(RequestServer PRIVATE requestserverservice LibFileSystem)
target_link_libraries(requestserverservice PUBLIC LibCore LibDNS LibHTTP LibIPC LibMain LibPlatformFeatures LibRequests LibSandbox LibWebSocket LibURL LibTextCodec CURL::libcurl)
