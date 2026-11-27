/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/sha3.h - SHA-3 (FIPS 202) de
 * la LibCrypto de Hispa-64 (sha3.c). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
	uint64_t a[25];          /* el estado de Keccak */
	size_t tasa, pos, tam;   /* la tasa (136, 104 o 72 octetos), por dónde va, y el tamaño del resumen */
} hispa_sha3;
void hispa_sha3_init(hispa_sha3 *c, size_t tam_resumen);          /* 32, 48 o 64 */
void hispa_sha3_update(hispa_sha3 *c, const void *datos, size_t n);
void hispa_sha3_final(hispa_sha3 *c, uint8_t *resumen);
#ifdef __cplusplus
}
#endif
