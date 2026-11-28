/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/sinopenssl.c - Hispa-64, etapas 10 g y 10 i (docs/64/HISPA-64.md,
 * secciones 24 y 26): lo de OpenSSL 3 que nombran Ladybird y libcrypto_rust
 * (la parte de Rust de LibCrypto, Libraries/LibCrypto/Rust/src/sys.rs: la
 * usan la WebCrypto de LibWeb y LibDNS), sin OpenSSL (la red cifrada va por
 * curl con BearSSL; la LibCrypto de C++ es la nuestra, hispa64/LibCrypto).
 * [Etapa 10 i] Ya no falla nada de la WebCrypto: las funciones de sys.rs
 * están de verdad en hispa64/webcripto/ (sobre BearSSL y monocypher; ver su
 * LEEME.txt). Aquí queda lo de en medio: los OSSL_PARAM (que el Rust
 * construye y pasa por valor, con la misma forma que en OpenSSL) y su
 * constructor (OSSL_PARAM_BLD), el azar (RAND_bytes, con getrandom), borrar
 * sin que el compilador lo quite (OPENSSL_cleanse), la cola de errores (que
 * aquí no hay) y lo poco que nombran RequestServer y WebContent.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include "webcripto/interno.h"

static OSSL_PARAM param(const char *k, unsigned int t, void *d, size_t n)
{
	OSSL_PARAM p = { k, t, d, n, OSSL_PARAM_UNMODIFIED };
	return p;
}
H64_VISIBLE OSSL_PARAM OSSL_PARAM_construct_octet_string(const char *k, void *d, size_t n) { return param(k, OSSL_PARAM_OCTET_STRING, d, n); }
H64_VISIBLE OSSL_PARAM OSSL_PARAM_construct_utf8_string(const char *k, char *d, size_t n) { return param(k, OSSL_PARAM_UTF8_STRING, d, n ? n : (d ? strlen(d) : 0)); }
H64_VISIBLE OSSL_PARAM OSSL_PARAM_construct_uint(const char *k, unsigned int *d) { return param(k, OSSL_PARAM_UNSIGNED_INTEGER, d, sizeof *d); }
H64_VISIBLE OSSL_PARAM OSSL_PARAM_construct_int(const char *k, int *d) { return param(k, OSSL_PARAM_INTEGER, d, sizeof *d); }
H64_VISIBLE OSSL_PARAM OSSL_PARAM_construct_end(void) { OSSL_PARAM p = { 0, 0, 0, 0, 0 }; return p; }

/* ---- leer los parámetros ---- */
const OSSL_PARAM *h64_param(const OSSL_PARAM *p, const char *clave)
{
	for (; p && p->key; p++)
		if (!strcmp(p->key, clave)) return p;
	return NULL;
}
static int lee_entero(const OSSL_PARAM *q, uint64_t *v, int con_signo)
{
	if (!q || !q->data || (q->data_type != OSSL_PARAM_UNSIGNED_INTEGER && q->data_type != OSSL_PARAM_INTEGER)) return 0;
	if (q->data_size == 4) {
		if (q->data_type == OSSL_PARAM_INTEGER) {
			int32_t x;
			memcpy(&x, q->data, 4);
			if (x < 0 && !con_signo) return 0;
			*v = (uint64_t)(int64_t)x;
		} else {
			uint32_t x;
			memcpy(&x, q->data, 4);
			*v = x;
		}
		return 1;
	}
	if (q->data_size == 8) { memcpy(v, q->data, 8); return 1; }
	return 0;
}
int h64_param_uint(const OSSL_PARAM *p, const char *clave, uint64_t *v) { return lee_entero(h64_param(p, clave), v, 0); }
int h64_param_int(const OSSL_PARAM *p, const char *clave, int64_t *v)
{
	uint64_t u;
	if (!lee_entero(h64_param(p, clave), &u, 1)) return 0;
	*v = (int64_t)u;
	return 1;
}
const char *h64_param_texto(const OSSL_PARAM *p, const char *clave, size_t *n)
{
	const OSSL_PARAM *q = h64_param(p, clave);
	if (!q || q->data_type != OSSL_PARAM_UTF8_STRING || !q->data) return NULL;
	if (n) *n = q->data_size;
	return q->data;
}
const uint8_t *h64_param_octetos(const OSSL_PARAM *p, const char *clave, size_t *n)
{
	const OSSL_PARAM *q = h64_param(p, clave);
	if (!q || q->data_type != OSSL_PARAM_OCTET_STRING) return NULL;
	*n = q->data_size;
	return q->data ? q->data : (const uint8_t *)"";
}

/* ---- OSSL_PARAM_BLD: los parámetros con números grandes, en un solo bloque ---- */
typedef struct {
	char *clave;
	unsigned int tipo;
	uint8_t *datos;
	size_t n;
} entrada;
typedef struct ossl_param_bld_st {
	entrada *e;
	size_t n, cap;
} OSSL_PARAM_BLD;
H64_VISIBLE OSSL_PARAM_BLD *OSSL_PARAM_BLD_new(void) { return calloc(1, sizeof(OSSL_PARAM_BLD)); }
H64_VISIBLE void OSSL_PARAM_BLD_free(OSSL_PARAM_BLD *b)
{
	if (!b) return;
	for (size_t i = 0; i < b->n; i++) {
		free(b->e[i].clave);
		if (b->e[i].datos) { h64_borra(b->e[i].datos, b->e[i].n); free(b->e[i].datos); }
	}
	free(b->e);
	free(b);
}
static int mete(OSSL_PARAM_BLD *b, const char *clave, unsigned int tipo, const void *d, size_t n)
{
	if (!b || !clave) return 0;
	if (b->n == b->cap) {
		size_t cap = b->cap * 2 + 8;
		entrada *e = realloc(b->e, cap * sizeof *e);
		if (!e) return 0;
		b->e = e;
		b->cap = cap;
	}
	entrada *x = &b->e[b->n];
	x->clave = strdup(clave);
	x->datos = malloc(n + 1);
	if (!x->clave || !x->datos) { free(x->clave); free(x->datos); return 0; }
	if (n) memcpy(x->datos, d, n);
	x->datos[n] = 0;
	x->tipo = tipo;
	x->n = n;
	b->n++;
	return 1;
}
/* un número: el entero sin signo en el orden de la máquina (de menos a más), como OpenSSL */
H64_VISIBLE int OSSL_PARAM_BLD_push_BN(OSSL_PARAM_BLD *b, const char *clave, const BIGNUM *v)
{
	if (!v) return 0;
	size_t n = h64bn_octetos(v);
	if (!n) n = 1;
	uint8_t *t = malloc(n), *u = malloc(n);
	int ok = t && u && h64bn_a_octetos(v, t, n);
	if (ok) {
		for (size_t i = 0; i < n; i++) u[i] = t[n - 1 - i];
		ok = mete(b, clave, OSSL_PARAM_UNSIGNED_INTEGER, u, n);
	}
	if (t) { h64_borra(t, n); free(t); }
	if (u) { h64_borra(u, n); free(u); }
	return ok;
}
H64_VISIBLE int OSSL_PARAM_BLD_push_utf8_string(OSSL_PARAM_BLD *b, const char *clave, const char *v, size_t n)
{
	if (!v) return 0;
	return mete(b, clave, OSSL_PARAM_UTF8_STRING, v, n ? n : strlen(v));
}
H64_VISIBLE int OSSL_PARAM_BLD_push_octet_string(OSSL_PARAM_BLD *b, const char *clave, const void *v, size_t n)
{
	return mete(b, clave, OSSL_PARAM_OCTET_STRING, v, n);
}
H64_VISIBLE int OSSL_PARAM_BLD_push_uint(OSSL_PARAM_BLD *b, const char *clave, unsigned int v)
{
	return mete(b, clave, OSSL_PARAM_UNSIGNED_INTEGER, &v, sizeof v);
}
/* el bloque: la tabla (con el final) y, detrás, los datos; se suelta con OSSL_PARAM_free */
typedef struct { size_t tam; } cabecera_bloque;
H64_VISIBLE OSSL_PARAM *OSSL_PARAM_BLD_to_param(OSSL_PARAM_BLD *b)
{
	if (!b) return NULL;
	size_t tabla = (b->n + 1) * sizeof(OSSL_PARAM), datos = 0;
	for (size_t i = 0; i < b->n; i++) datos += strlen(b->e[i].clave) + 1 + b->e[i].n + 1 + 8;
	size_t tam = 16 + tabla + datos;
	uint8_t *blq = calloc(1, tam);
	if (!blq) return NULL;
	((cabecera_bloque *)blq)->tam = tam;
	OSSL_PARAM *p = (OSSL_PARAM *)(blq + 16);
	uint8_t *d = (uint8_t *)(p + b->n + 1);
	for (size_t i = 0; i < b->n; i++) {
		entrada *x = &b->e[i];
		d = (uint8_t *)(((uintptr_t)d + 7) & ~(uintptr_t)7);
		memcpy(d, x->datos, x->n + 1);
		p[i].data = d;
		p[i].data_size = x->n;
		p[i].data_type = x->tipo;
		p[i].return_size = OSSL_PARAM_UNMODIFIED;
		d += x->n + 1;
		size_t kl = strlen(x->clave) + 1;
		memcpy(d, x->clave, kl);
		p[i].key = (const char *)d;
		d += kl;
	}
	return p;
}
H64_VISIBLE void OSSL_PARAM_free(OSSL_PARAM *p)
{
	if (!p) return;
	uint8_t *blq = (uint8_t *)p - 16;
	h64_borra(blq, ((cabecera_bloque *)blq)->tam);
	free(blq);
}

/* ---- el azar ---- */
int h64_azar(void *b, size_t n)
{
	uint8_t *p = b;
	while (n > 0) {
		ssize_t r = getrandom(p, n, 0);
		if (r <= 0) return 0;
		p += r;
		n -= (size_t)r;
	}
	return 1;
}
H64_VISIBLE int RAND_bytes(unsigned char *b, int n)
{
	return n >= 0 && h64_azar(b, (size_t)n);
}
static void prng_init(const br_prng_class **ctx, const void *params, const void *semilla, size_t n) { (void)ctx; (void)params; (void)semilla; (void)n; }
static void prng_genera(const br_prng_class **ctx, void *sal, size_t n)
{
	(void)ctx;
	if (!h64_azar(sal, n)) abort();   /* sin azar no se sigue: mejor parar que dar una clave floja */
}
static void prng_mezcla(const br_prng_class **ctx, const void *semilla, size_t n) { (void)ctx; (void)semilla; (void)n; }
static const br_prng_class PRNG = { sizeof(h64_prng), prng_init, prng_genera, prng_mezcla };
void h64_prng_ini(h64_prng *g) { g->vtable = &PRNG; }

void h64_borra(void *p, size_t n)
{
	volatile unsigned char *q = p;
	while (n--) *q++ = 0;
}
int h64_iguales_ct(const void *a, const void *b, size_t n)
{
	const uint8_t *x = a, *y = b;
	uint8_t d = 0;
	for (size_t i = 0; i < n; i++) d |= x[i] ^ y[i];
	return d == 0;
}
H64_VISIBLE void OPENSSL_cleanse(void *p, size_t n) { h64_borra(p, n); }
H64_VISIBLE int OSSL_set_max_threads(void *ctx, uint64_t max_threads) { (void)ctx; (void)max_threads; return 1; }
/* la cola de errores: no hay */
H64_VISIBLE void ERR_clear_error(void) {}
H64_VISIBLE void ERR_print_errors_cb(int (*cb)(const char *, size_t, void *), void *u) { (void)cb; (void)u; }
/* lo que nombran RequestServer y WebContent y no se usa (X509_free: el
 * Request.cpp de Ladybird suelta con ella los certificados de AIA, que aquí
 * no hay: le llega nulo) */
H64_VISIBLE void X509_free(void *x) { (void)x; }
#define NO_HAY(f) long f(void); long f(void) { return 0; }
NO_HAY(is_end)
NO_HAY(OPENSSL_version_major)
NO_HAY(OPENSSL_version_minor)
NO_HAY(OPENSSL_version_patch)
