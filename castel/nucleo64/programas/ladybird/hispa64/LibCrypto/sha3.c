/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/hispa64/LibCrypto/sha3.c - Hispa-64, etapa 10 f:
 * SHA-3 (FIPS 202: SHA3-256, SHA3-384 y SHA3-512) para la LibCrypto de
 * Hispa-64 (BearSSL no lo tiene). La permutación Keccak-f[1600], de 24
 * vueltas, con las 25 palabras de 64 bits en el orden de la norma (x + 5y)
 * y en little-endian; la esponja con el relleno de SHA-3 (0x06 ... 0x80). */
#include <stdint.h>
#include <string.h>
#include "sha3.h"

static const uint64_t RC[24] = {
	0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL, 0x8000000080008000ULL,
	0x000000000000808BULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
	0x000000000000008AULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
	0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
	0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800AULL, 0x800000008000000AULL,
	0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};
/* rho (cuánto se gira cada palabra) y pi (adónde va), en el orden del recorrido de pi */
static const unsigned ROT[24] = { 1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44 };
static const unsigned PI[24] = { 10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1 };

static uint64_t gira(uint64_t x, unsigned n) { return (x << n) | (x >> (64 - n)); }

static void keccak_f(uint64_t a[25])
{
	uint64_t c[5], t;
	for (int v = 0; v < 24; v++) {
		/* theta */
		for (int x = 0; x < 5; x++) c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
		for (int x = 0; x < 5; x++) {
			t = c[(x + 4) % 5] ^ gira(c[(x + 1) % 5], 1);
			for (int y = 0; y < 25; y += 5) a[y + x] ^= t;
		}
		/* rho y pi */
		t = a[1];
		for (int i = 0; i < 24; i++) {
			uint64_t s = a[PI[i]];
			a[PI[i]] = gira(t, ROT[i]);
			t = s;
		}
		/* chi */
		for (int y = 0; y < 25; y += 5) {
			for (int x = 0; x < 5; x++) c[x] = a[y + x];
			for (int x = 0; x < 5; x++) a[y + x] ^= ~c[(x + 1) % 5] & c[(x + 2) % 5];
		}
		/* iota */
		a[0] ^= RC[v];
	}
}

static void xor_octeto(uint64_t a[25], size_t i, uint8_t b) { a[i / 8] ^= (uint64_t)b << (8 * (i % 8)); }

void hispa_sha3_init(hispa_sha3 *c, size_t tam_resumen)
{
	memset(c, 0, sizeof *c);
	c->tasa = 200 - 2 * tam_resumen;
	c->tam = tam_resumen;
}

void hispa_sha3_update(hispa_sha3 *c, const void *datos, size_t n)
{
	const uint8_t *p = datos;
	while (n--) {
		xor_octeto(c->a, c->pos++, *p++);
		if (c->pos == c->tasa) { keccak_f(c->a); c->pos = 0; }
	}
}

void hispa_sha3_final(hispa_sha3 *c, uint8_t *resumen)
{
	xor_octeto(c->a, c->pos, 0x06);
	xor_octeto(c->a, c->tasa - 1, 0x80);
	keccak_f(c->a);
	for (size_t i = 0; i < c->tam; i++) resumen[i] = (uint8_t)(c->a[i / 8] >> (8 * (i % 8)));
}
