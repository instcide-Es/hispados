/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/incluir/openssl/crypto.h - Hispa-64, etapa 10 g: lo único de
 * <openssl/crypto.h> que pide SystemProbe de Ladybird (OpenSSLDescription.cpp:
 * la versión de OpenSSL con la que va enlazado). Hispa-64 no lleva OpenSSL:
 * en hispa64/sinopenssl.c, la versión es la 0.0.0.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
unsigned int OPENSSL_version_major(void);
unsigned int OPENSSL_version_minor(void);
unsigned int OPENSSL_version_patch(void);
#ifdef __cplusplus
}
#endif
