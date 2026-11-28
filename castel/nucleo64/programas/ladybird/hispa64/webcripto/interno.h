/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/interno.h - Hispa-64, etapa 10 i (docs/64/HISPA-64.md,
 * sección 26): lo de dentro de la WebCrypto de Hispa-64, las funciones de
 * OpenSSL 3 que pide la parte de Rust de LibCrypto de Ladybird
 * (terceros/ladybird/Libraries/LibCrypto/Rust/src/sys.rs), hechas sobre
 * BearSSL 0.6 (terceros/bearssl) y monocypher 4 (terceros/monocypher), sin
 * OpenSSL. Ver LEEME.txt.
 * Los tipos de OpenSSL (EVP_PKEY, BIGNUM...) son nuestros: el Rust solo los
 * ve como punteros opacos (opaque_types! de sys.rs), así que su forma por
 * dentro es la que queramos. Lo único que tiene que ser como en OpenSSL es
 * OSSL_PARAM (el Rust lo construye y lo pasa por valor).
 */
#ifndef H64_WEBCRIPTO_INTERNO_H
#define H64_WEBCRIPTO_INTERNO_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "bearssl.h"

#define H64_VISIBLE __attribute__((visibility("default")))

/* ---- OSSL_PARAM, como en OpenSSL 3 (core.h) ---- */
typedef struct {
	const char *key;
	unsigned int data_type;
	void *data;
	size_t data_size;
	size_t return_size;
} OSSL_PARAM;
#define OSSL_PARAM_INTEGER 1
#define OSSL_PARAM_UNSIGNED_INTEGER 2
#define OSSL_PARAM_UTF8_STRING 4
#define OSSL_PARAM_OCTET_STRING 5
#define OSSL_PARAM_UNMODIFIED ((size_t)-1)

const OSSL_PARAM *h64_param(const OSSL_PARAM *p, const char *clave);
int h64_param_uint(const OSSL_PARAM *p, const char *clave, uint64_t *v);
int h64_param_int(const OSSL_PARAM *p, const char *clave, int64_t *v);
/* el texto de un parámetro (UTF8_STRING), o NULL */
const char *h64_param_texto(const OSSL_PARAM *p, const char *clave, size_t *n);
/* los octetos de un parámetro (OCTET_STRING), o NULL si no está */
const uint8_t *h64_param_octetos(const OSSL_PARAM *p, const char *clave, size_t *n);

/* ---- el azar (getrandom) ---- */
int h64_azar(void *b, size_t n);
/* un br_prng_class que saca de getrandom, para BearSSL */
typedef struct { const br_prng_class *vtable; } h64_prng;
void h64_prng_ini(h64_prng *g);
void h64_borra(void *p, size_t n);
int h64_iguales_ct(const void *a, const void *b, size_t n);

/* ---- los números grandes (BIGNUM): palabras de 32 bits, de menos a más ---- */
typedef struct bignum_st {
	uint32_t *w;
	size_t n, cap;
	int secreto;
} BIGNUM;
BIGNUM *h64bn_nuevo(void);
void h64bn_libera(BIGNUM *a);
int h64bn_reserva(BIGNUM *a, size_t cap);
int h64bn_de_octetos(BIGNUM *a, const uint8_t *b, size_t n);
int h64bn_a_octetos(const BIGNUM *a, uint8_t *b, size_t n);   /* con ceros delante; 0 si no cabe */
size_t h64bn_octetos(const BIGNUM *a);
size_t h64bn_bits(const BIGNUM *a);
int h64bn_cmp(const BIGNUM *a, const BIGNUM *b);
int h64bn_copia(BIGNUM *r, const BIGNUM *a);
int h64bn_pon(BIGNUM *r, uint32_t v);
int h64bn_es_cero(const BIGNUM *a);
int h64bn_es_uno(const BIGNUM *a);
int h64bn_es_impar(const BIGNUM *a);
int h64bn_suma(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);
int h64bn_resta(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);  /* a >= b */
int h64bn_resta1(BIGNUM *r, const BIGNUM *a);                  /* a - 1, a >= 1 */
int h64bn_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);
int h64bn_divmod(BIGNUM *q, BIGNUM *r, const BIGNUM *a, const BIGNUM *m);
int h64bn_mod(BIGNUM *r, const BIGNUM *a, const BIGNUM *m);
int h64bn_modmul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m);
int h64bn_modexp(BIGNUM *r, const BIGNUM *b, const BIGNUM *e, const BIGNUM *m);
int h64bn_mcd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);
int h64bn_modinv(BIGNUM *r, const BIGNUM *a, const BIGNUM *m);  /* 0 si no hay */
int h64bn_shr(BIGNUM *r, const BIGNUM *a, size_t bits);
int h64bn_shl(BIGNUM *r, const BIGNUM *a, size_t bits);
int h64bn_bit(const BIGNUM *a, size_t i);
int h64bn_azar_menor(BIGNUM *r, const BIGNUM *m);  /* en [1, m-1] */
/* ¿es primo? (división por los primos pequeños y Miller-Rabin con rondas al azar) */
int h64bn_primo(const BIGNUM *a, int rondas);
/* 1 si tiene un factor primo menor que 752 (lo que mira OpenSSL de un módulo RSA) */
int h64bn_factor_pequeno(const BIGNUM *a);

/* ---- los resúmenes: SHA-1, SHA-2, SHA-3, SHAKE y el Keccak de KMAC ---- */
enum {
	R_NADA = 0, R_SHA1, R_SHA256, R_SHA384, R_SHA512, R_SHA224,
	R_SHA3_256, R_SHA3_384, R_SHA3_512, R_SHAKE128, R_SHAKE256, R_KMACK128, R_KMACK256
};
typedef struct {
	uint64_t a[25];
	size_t tasa, pos;
	uint8_t dominio;
	int exprime;
} h64_keccak;
typedef struct {
	int tipo;
	union {
		br_sha1_context s1;
		br_sha224_context s224;
		br_sha256_context s256;
		br_sha384_context s384;
		br_sha512_context s512;
		h64_keccak k;
	} u;
} h64_resumen;
int h64_resumen_tipo(const char *nombre);
size_t h64_resumen_tam(int tipo);      /* 0 en los XOF */
size_t h64_resumen_bloque(int tipo);
int h64_resumen_es_xof(int tipo);
void h64_resumen_ini(h64_resumen *c, int tipo);
void h64_resumen_mas(h64_resumen *c, const void *d, size_t n);
void h64_resumen_fin(h64_resumen *c, uint8_t *sal);       /* h64_resumen_tam octetos */
void h64_resumen_saca(h64_resumen *c, uint8_t *sal, size_t n);  /* los XOF */
void h64_resumen_de(int tipo, const void *d, size_t n, uint8_t *sal);
void h64_keccak_ini(h64_keccak *k, size_t tasa, uint8_t dominio);
void h64_keccak_mas(h64_keccak *k, const void *d, size_t n);
void h64_keccak_saca(h64_keccak *k, uint8_t *sal, size_t n);
/* HMAC de cualquiera de los resúmenes (no XOF) */
typedef struct { h64_resumen in, ex; int tipo; } h64_hmac;
void h64_hmac_ini(h64_hmac *h, int tipo, const uint8_t *k, size_t kn);
void h64_hmac_mas(h64_hmac *h, const void *d, size_t n);
void h64_hmac_fin(h64_hmac *h, uint8_t *sal);
/* el DigestInfo de PKCS #1 v1.5 que va delante del resumen (NULL si no hay) */
const uint8_t *h64_resumen_digestinfo(int tipo, size_t *n);
/* MGF1 (RFC 8017, B.2.1): sal ^= MGF1(semilla) */
void h64_mgf1_xor(int tipo, uint8_t *sal, size_t n, const uint8_t *semilla, size_t sn);

/* ---- DER, lo justo ---- */
typedef struct { const uint8_t *p; size_t n; } h64_trozo;
/* lee un elemento: etiqueta, contenido y lo que ocupa entero; 0 si no se puede */
int h64_der_lee(const uint8_t *p, size_t n, uint8_t *etiqueta, h64_trozo *contenido, size_t *ocupa);
/* escribe la cabecera (etiqueta y longitud) en b (si no es NULL); devuelve lo que ocupa */
size_t h64_der_cabecera(uint8_t *b, uint8_t etiqueta, size_t n);
/* un búfer que crece */
typedef struct { uint8_t *b; size_t n, cap; int mal; } h64_buf;
void h64_buf_mete(h64_buf *s, const void *d, size_t n);
void h64_buf_cab(h64_buf *s, uint8_t etiqueta, size_t n);
void h64_buf_entero(h64_buf *s, const uint8_t *v, size_t n);  /* INTEGER sin signo (big-endian) */
void h64_buf_entero_bn(h64_buf *s, const BIGNUM *a);
void h64_buf_libera(h64_buf *s);

/* ---- las claves ---- */
enum { K_RSA = 1, K_EC, K_ED25519, K_X25519 };
typedef struct evp_pkey_st {
	int refs;
	int tipo;
	int privada;
	/* RSA: n, e y, si es privada, d (y el CRT, si lo hay) */
	BIGNUM *n, *e, *d, *p, *q, *dp, *dq, *qi;
	int crt;          /* 1 si tiene p, q, dp, dq, qi (dados o hechos al generarla) */
	int crt_dentro;   /* 1 si no se dieron pero se han sacado de n, e y d (solo para operar) */
	/* EC */
	int curva;        /* BR_EC_secp256r1, 384r1 o 521r1 */
	size_t tam;       /* 32, 48 o 66 */
	uint8_t dd[66];   /* el escalar privado, con ceros delante */
	uint8_t pub[133]; /* el punto sin comprimir (04 x y) */
	size_t publen;
	int comprimido;   /* cómo vino el punto (para escribirlo igual) */
	/* Ed25519 y X25519 */
	uint8_t cruda[32], crudap[32], edsk[64];
} EVP_PKEY;
EVP_PKEY *h64_clave_nueva(int tipo);
void EVP_PKEY_free(EVP_PKEY *k);
EVP_PKEY *EVP_PKEY_new_raw_private_key_ex(void *lib, const char *nombre, const char *prop, const uint8_t *b, size_t n);
const char *h64_nombre_tipo(int tipo);
/* datos de las curvas */
size_t h64_curva_tam(int curva);
int h64_curva_por_nid(int nid);
int h64_curva_por_nombre(const char *nombre);
int h64_curva_por_oid(const uint8_t *oid, size_t n);
const uint8_t *h64_curva_oid(int curva, size_t *n);
size_t h64_curva_bits(int curva);
/* pone el punto de la clave desde su codificación SEC 1 (con o sin comprimir), comprobado */
int h64_ec_pon_punto(EVP_PKEY *k, int curva, const uint8_t *p, size_t n);
int h64_ec_pon_privada(EVP_PKEY *k, int curva, const uint8_t *d, size_t n);
int h64_rsa_completa(EVP_PKEY *k);
/* la clave pública de un SPKI y la privada de un PKCS #8 (der.c) */
size_t h64_pubkey_der(const EVP_PKEY *k, uint8_t *sal);
#endif
