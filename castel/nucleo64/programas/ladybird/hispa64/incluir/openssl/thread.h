/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/incluir/openssl/thread.h - Hispa-64, etapa 10 g: lo único de
 * <openssl/thread.h> que pide WebContent (main.cpp: cuántos hilos puede usar
 * OpenSSL). Hispa-64 no lleva OpenSSL; la función, en hispa64/sinopenssl.c,
 * no hace nada.
 */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct ossl_lib_ctx_st OSSL_LIB_CTX;
int OSSL_set_max_threads(OSSL_LIB_CTX* ctx, uint64_t max_threads);
#ifdef __cplusplus
}
#endif
