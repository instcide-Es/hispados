/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/kdf.c - Hispa-64, etapa 10 i: las derivaciones de claves
 * de la WebCrypto (EVP_KDF de OpenSSL): HKDF (RFC 5869) y PBKDF2 (RFC 8018,
 * 5.2) sobre nuestro HMAC (resumen.c), y Argon2d, Argon2i y Argon2id (RFC
 * 9106, versión 0x13) con monocypher; con los mismos límites que OpenSSL.
 */
#include <stdlib.h>
#include <strings.h>
#include "interno.h"
#include "monocypher.h"

enum { KDF_HKDF = 1, KDF_PBKDF2, KDF_ARGON2D, KDF_ARGON2I, KDF_ARGON2ID };
typedef struct evp_kdf_st { int tipo; } EVP_KDF;
typedef struct evp_kdf_ctx_st { int tipo; } EVP_KDF_CTX;
static EVP_KDF KDFS[] = { { 0 }, { KDF_HKDF }, { KDF_PBKDF2 }, { KDF_ARGON2D }, { KDF_ARGON2I }, { KDF_ARGON2ID } };

H64_VISIBLE EVP_KDF *EVP_KDF_fetch(void *lib, const char *nombre, const char *prop)
{
	(void)lib; (void)prop;
	if (!nombre) return NULL;
	if (!strcasecmp(nombre, "HKDF")) return &KDFS[KDF_HKDF];
	if (!strcasecmp(nombre, "PBKDF2")) return &KDFS[KDF_PBKDF2];
	if (!strcasecmp(nombre, "ARGON2D")) return &KDFS[KDF_ARGON2D];
	if (!strcasecmp(nombre, "ARGON2I")) return &KDFS[KDF_ARGON2I];
	if (!strcasecmp(nombre, "ARGON2ID")) return &KDFS[KDF_ARGON2ID];
	return NULL;
}
H64_VISIBLE void EVP_KDF_free(EVP_KDF *k) { (void)k; }
H64_VISIBLE EVP_KDF_CTX *EVP_KDF_CTX_new(EVP_KDF *k)
{
	if (!k) return NULL;
	EVP_KDF_CTX *c = calloc(1, sizeof *c);
	if (c) c->tipo = k->tipo;
	return c;
}
H64_VISIBLE void EVP_KDF_CTX_free(EVP_KDF_CTX *c) { free(c); }

static int resumen_de(const OSSL_PARAM *p)
{
	size_t n;
	const char *d = h64_param_texto(p, "digest", &n);
	int t = d ? h64_resumen_tipo(d) : R_NADA;
	return h64_resumen_es_xof(t) ? R_NADA : t;
}
/* RFC 5869: extraer y expandir */
int h64_hkdf(int t, const uint8_t *ikm, size_t ikmn, const uint8_t *sal, size_t saln,
	const uint8_t *info, size_t infon, uint8_t *okm, size_t n)
{
	size_t hl = h64_resumen_tam(t);
	if (!hl || n > 255 * hl) return 0;
	uint8_t prk[64], ti[64];
	h64_hmac h;
	h64_hmac_ini(&h, t, sal, saln);
	h64_hmac_mas(&h, ikm, ikmn);
	h64_hmac_fin(&h, prk);
	size_t hecho = 0;
	for (uint8_t i = 1; hecho < n; i++) {
		h64_hmac_ini(&h, t, prk, hl);
		if (i > 1) h64_hmac_mas(&h, ti, hl);
		h64_hmac_mas(&h, info, infon);
		h64_hmac_mas(&h, &i, 1);
		h64_hmac_fin(&h, ti);
		size_t k = n - hecho < hl ? n - hecho : hl;
		memcpy(okm + hecho, ti, k);
		hecho += k;
	}
	h64_borra(prk, sizeof prk);
	h64_borra(ti, sizeof ti);
	return 1;
}
/* RFC 8018, 5.2 */
int h64_pbkdf2(int t, const uint8_t *pw, size_t pwn, const uint8_t *sal, size_t saln, uint64_t iter, uint8_t *dk, size_t n)
{
	size_t hl = h64_resumen_tam(t);
	if (!hl || iter < 1) return 0;
	uint8_t u[64], f[64];
	h64_hmac base, h;
	h64_hmac_ini(&base, t, pw, pwn);
	for (uint32_t i = 1; n; i++) {
		uint8_t ib[4] = { (uint8_t)(i >> 24), (uint8_t)(i >> 16), (uint8_t)(i >> 8), (uint8_t)i };
		h = base;
		h64_hmac_mas(&h, sal, saln);
		h64_hmac_mas(&h, ib, 4);
		h64_hmac_fin(&h, u);
		memcpy(f, u, hl);
		for (uint64_t j = 1; j < iter; j++) {
			h = base;
			h64_hmac_mas(&h, u, hl);
			h64_hmac_fin(&h, u);
			for (size_t k = 0; k < hl; k++) f[k] ^= u[k];
		}
		size_t k = n < hl ? n : hl;
		memcpy(dk, f, k);
		dk += k;
		n -= k;
	}
	h64_borra(&base, sizeof base);
	h64_borra(u, sizeof u);
	h64_borra(f, sizeof f);
	return 1;
}

H64_VISIBLE int EVP_KDF_derive(EVP_KDF_CTX *c, uint8_t *sal, size_t n, const OSSL_PARAM *p)
{
	if (!c) return 0;
	size_t kn = 0, sn = 0, in = 0;
	if (c->tipo == KDF_HKDF) {
		int t = resumen_de(p);
		const uint8_t *k = h64_param_octetos(p, "key", &kn);
		const uint8_t *s = h64_param_octetos(p, "salt", &sn);
		const uint8_t *i = h64_param_octetos(p, "info", &in);
		if (!t || !k || n == 0) return 0;
		return h64_hkdf(t, k, kn, s, s ? sn : 0, i, i ? in : 0, sal, n);
	}
	if (c->tipo == KDF_PBKDF2) {
		int t = resumen_de(p);
		uint64_t iter = 2048;
		const uint8_t *pw = h64_param_octetos(p, "pass", &kn);
		const uint8_t *s = h64_param_octetos(p, "salt", &sn);
		h64_param_uint(p, "iter", &iter);
		if (!t || !s) return 0;
		if (n == 0) return 1;
		return h64_pbkdf2(t, pw ? pw : (const uint8_t *)"", pw ? kn : 0, s, sn, iter, sal, n);
	}
	/* Argon2 (como OpenSSL: la sal, de 8 octetos o más; lo que sale, de 4 o
	 * más; la memoria, 8 bloques por carril como poco; de 1 a 2^24-1 carriles) */
	uint64_t pasos = 3, carriles = 1, memoria = 1024, tam = 0;
	h64_param_uint(p, "iter", &pasos);
	h64_param_uint(p, "lanes", &carriles);
	h64_param_uint(p, "memcost", &memoria);
	if (h64_param_uint(p, "size", &tam) && tam != n) return 0;
	const uint8_t *pw = h64_param_octetos(p, "pass", &kn);
	const uint8_t *s = h64_param_octetos(p, "salt", &sn);
	size_t secn = 0, adn = 0;
	const uint8_t *sec = h64_param_octetos(p, "secret", &secn);
	const uint8_t *ad = h64_param_octetos(p, "ad", &adn);
	if (!s || sn < 8 || n < 4 || n > 0xFFFFFFFFu || pasos < 1 || pasos > 0xFFFFFFFFu || carriles < 1 || carriles > 0xFFFFFF
	    || memoria < 8 * carriles || memoria > 0xFFFFFFFFu || kn > 0xFFFFFFFFu || secn > 0xFFFFFFFFu || adn > 0xFFFFFFFFu)
		return 0;
	if (memoria > 4194304) return 0;  /* más de 4 GB: no */
	void *trabajo = malloc((size_t)memoria * 1024);
	if (!trabajo) return 0;
	crypto_argon2_config cf = {
		.algorithm = c->tipo == KDF_ARGON2D ? CRYPTO_ARGON2_D : c->tipo == KDF_ARGON2I ? CRYPTO_ARGON2_I : CRYPTO_ARGON2_ID,
		.nb_blocks = (uint32_t)memoria, .nb_passes = (uint32_t)pasos, .nb_lanes = (uint32_t)carriles,
	};
	crypto_argon2_inputs en = { .pass = pw ? pw : (const uint8_t *)"", .salt = s, .pass_size = (uint32_t)(pw ? kn : 0), .salt_size = (uint32_t)sn };
	crypto_argon2_extras ex = { .key = sec, .ad = ad, .key_size = (uint32_t)(sec ? secn : 0), .ad_size = (uint32_t)(ad ? adn : 0) };
	crypto_argon2(sal, (uint32_t)n, trabajo, cf, en, ex);
	h64_borra(trabajo, (size_t)memoria * 1024);
	free(trabajo);
	return 1;
}
