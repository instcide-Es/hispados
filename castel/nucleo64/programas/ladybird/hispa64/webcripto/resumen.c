/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/resumen.c - Hispa-64, etapa 10 i: los resúmenes y los MAC
 * de la WebCrypto: SHA-1 y SHA-2 (BearSSL), SHA-3, SHAKE y el Keccak de KMAC
 * (FIPS 202 y SP 800-185: Keccak-f[1600], nuestro), HMAC (RFC 2104) de todos
 * ellos y KMAC128/256; y lo de OpenSSL que los usa: EVP_Q_digest,
 * EVP_MD_fetch y EVP_DigestInit_ex/Update/FinalXOF, EVP_MAC_*.
 */
#include <stdlib.h>
#include <strings.h>
#include "interno.h"

/* ---- Keccak-f[1600] (FIPS 202, 3.2) ---- */
static const uint64_t RC[24] = {
	0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL, 0x8000000080008000ULL,
	0x000000000000808bULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
	0x000000000000008aULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
	0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
	0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800aULL, 0x800000008000000aULL,
	0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};
static const unsigned ROT[25] = { 0, 1, 62, 28, 27, 36, 44, 6, 55, 20, 3, 10, 43, 25, 39, 41, 45, 15, 21, 8, 18, 2, 61, 56, 14 };
static uint64_t gira(uint64_t x, unsigned n) { return n ? (x << n) | (x >> (64 - n)) : x; }
static void keccak_f(uint64_t a[25])
{
	for (int r = 0; r < 24; r++) {
		uint64_t c[5], d[5], b[25];
		for (int x = 0; x < 5; x++) c[x] = a[x] ^ a[x + 5] ^ a[x + 10] ^ a[x + 15] ^ a[x + 20];
		for (int x = 0; x < 5; x++) d[x] = c[(x + 4) % 5] ^ gira(c[(x + 1) % 5], 1);
		for (int i = 0; i < 25; i++) a[i] ^= d[i % 5];
		/* rho y pi: B[y, 2x+3y] = rot(A[x, y]) */
		for (int x = 0; x < 5; x++)
			for (int y = 0; y < 5; y++)
				b[y + 5 * ((2 * x + 3 * y) % 5)] = gira(a[x + 5 * y], ROT[x + 5 * y]);
		/* chi */
		for (int y = 0; y < 5; y++)
			for (int x = 0; x < 5; x++)
				a[x + 5 * y] = b[x + 5 * y] ^ (~b[(x + 1) % 5 + 5 * y] & b[(x + 2) % 5 + 5 * y]);
		a[0] ^= RC[r];
	}
}
void h64_keccak_ini(h64_keccak *k, size_t tasa, uint8_t dominio)
{
	memset(k, 0, sizeof *k);
	k->tasa = tasa;
	k->dominio = dominio;
}
static void xor_oct(uint64_t a[25], size_t i, uint8_t b) { a[i / 8] ^= (uint64_t)b << (8 * (i % 8)); }
void h64_keccak_mas(h64_keccak *k, const void *d, size_t n)
{
	const uint8_t *p = d;
	if (k->exprime) return;
	while (n--) {
		xor_oct(k->a, k->pos++, *p++);
		if (k->pos == k->tasa) { keccak_f(k->a); k->pos = 0; }
	}
}
void h64_keccak_saca(h64_keccak *k, uint8_t *sal, size_t n)
{
	if (!k->exprime) {
		xor_oct(k->a, k->pos, k->dominio);
		xor_oct(k->a, k->tasa - 1, 0x80);
		keccak_f(k->a);
		k->pos = 0;
		k->exprime = 1;
	}
	while (n--) {
		if (k->pos == k->tasa) { keccak_f(k->a); k->pos = 0; }
		*sal++ = (uint8_t)(k->a[k->pos / 8] >> (8 * (k->pos % 8)));
		k->pos++;
	}
}

/* ---- los resúmenes por su nombre (los de OpenSSL 3 y sus alias) ---- */
static const struct { const char *nombre; int tipo; } NOMBRES[] = {
	{ "SHA1", R_SHA1 }, { "SHA-1", R_SHA1 }, { "SHA2-224", R_SHA224 }, { "SHA224", R_SHA224 }, { "SHA-224", R_SHA224 },
	{ "SHA2-256", R_SHA256 }, { "SHA256", R_SHA256 }, { "SHA-256", R_SHA256 },
	{ "SHA2-384", R_SHA384 }, { "SHA384", R_SHA384 }, { "SHA-384", R_SHA384 },
	{ "SHA2-512", R_SHA512 }, { "SHA512", R_SHA512 }, { "SHA-512", R_SHA512 },
	{ "SHA3-256", R_SHA3_256 }, { "SHA3-384", R_SHA3_384 }, { "SHA3-512", R_SHA3_512 },
	{ "SHAKE128", R_SHAKE128 }, { "SHAKE-128", R_SHAKE128 }, { "SHAKE256", R_SHAKE256 }, { "SHAKE-256", R_SHAKE256 },
	{ "KECCAK-KMAC-128", R_KMACK128 }, { "KECCAK-KMAC128", R_KMACK128 },
	{ "KECCAK-KMAC-256", R_KMACK256 }, { "KECCAK-KMAC256", R_KMACK256 },
};
int h64_resumen_tipo(const char *nombre)
{
	if (!nombre) return R_NADA;
	for (size_t i = 0; i < sizeof NOMBRES / sizeof *NOMBRES; i++)
		if (!strcasecmp(nombre, NOMBRES[i].nombre)) return NOMBRES[i].tipo;
	return R_NADA;
}
size_t h64_resumen_tam(int t)
{
	switch (t) {
	case R_SHA1: return 20;
	case R_SHA224: return 28;
	case R_SHA256: case R_SHA3_256: return 32;
	case R_SHA384: case R_SHA3_384: return 48;
	case R_SHA512: case R_SHA3_512: return 64;
	default: return 0;
	}
}
size_t h64_resumen_bloque(int t)
{
	switch (t) {
	case R_SHA1: case R_SHA224: case R_SHA256: return 64;
	case R_SHA384: case R_SHA512: return 128;
	case R_SHA3_256: return 136;
	case R_SHA3_384: return 104;
	case R_SHA3_512: return 72;
	case R_SHAKE128: case R_KMACK128: return 168;
	case R_SHAKE256: case R_KMACK256: return 136;
	default: return 0;
	}
}
int h64_resumen_es_xof(int t) { return t == R_SHAKE128 || t == R_SHAKE256 || t == R_KMACK128 || t == R_KMACK256; }
void h64_resumen_ini(h64_resumen *c, int t)
{
	c->tipo = t;
	switch (t) {
	case R_SHA1: br_sha1_init(&c->u.s1); break;
	case R_SHA224: br_sha224_init(&c->u.s224); break;
	case R_SHA256: br_sha256_init(&c->u.s256); break;
	case R_SHA384: br_sha384_init(&c->u.s384); break;
	case R_SHA512: br_sha512_init(&c->u.s512); break;
	case R_SHA3_256: case R_SHA3_384: case R_SHA3_512: h64_keccak_ini(&c->u.k, h64_resumen_bloque(t), 0x06); break;
	case R_SHAKE128: case R_SHAKE256: h64_keccak_ini(&c->u.k, h64_resumen_bloque(t), 0x1F); break;
	case R_KMACK128: case R_KMACK256: h64_keccak_ini(&c->u.k, h64_resumen_bloque(t), 0x04); break;
	default: break;
	}
}
void h64_resumen_mas(h64_resumen *c, const void *d, size_t n)
{
	switch (c->tipo) {
	case R_SHA1: br_sha1_update(&c->u.s1, d, n); break;
	case R_SHA224: br_sha224_update(&c->u.s224, d, n); break;
	case R_SHA256: br_sha256_update(&c->u.s256, d, n); break;
	case R_SHA384: br_sha384_update(&c->u.s384, d, n); break;
	case R_SHA512: br_sha512_update(&c->u.s512, d, n); break;
	case R_NADA: break;
	default: h64_keccak_mas(&c->u.k, d, n); break;
	}
}
void h64_resumen_fin(h64_resumen *c, uint8_t *sal)
{
	switch (c->tipo) {
	case R_SHA1: br_sha1_out(&c->u.s1, sal); break;
	case R_SHA224: br_sha224_out(&c->u.s224, sal); break;
	case R_SHA256: br_sha256_out(&c->u.s256, sal); break;
	case R_SHA384: br_sha384_out(&c->u.s384, sal); break;
	case R_SHA512: br_sha512_out(&c->u.s512, sal); break;
	case R_SHA3_256: case R_SHA3_384: case R_SHA3_512: h64_keccak_saca(&c->u.k, sal, h64_resumen_tam(c->tipo)); break;
	default: break;
	}
}
void h64_resumen_saca(h64_resumen *c, uint8_t *sal, size_t n) { h64_keccak_saca(&c->u.k, sal, n); }
void h64_resumen_de(int t, const void *d, size_t n, uint8_t *sal)
{
	h64_resumen c;
	h64_resumen_ini(&c, t);
	h64_resumen_mas(&c, d, n);
	h64_resumen_fin(&c, sal);
	h64_borra(&c, sizeof c);
}

/* ---- HMAC (RFC 2104) ---- */
void h64_hmac_ini(h64_hmac *h, int t, const uint8_t *k, size_t kn)
{
	uint8_t kb[168], pad[168];
	size_t b = h64_resumen_bloque(t);
	memset(kb, 0, sizeof kb);
	h->tipo = t;
	if (kn > b) { h64_resumen_de(t, k, kn, kb); }
	else if (kn) memcpy(kb, k, kn);
	for (size_t i = 0; i < b; i++) pad[i] = kb[i] ^ 0x36;
	h64_resumen_ini(&h->in, t);
	h64_resumen_mas(&h->in, pad, b);
	for (size_t i = 0; i < b; i++) pad[i] = kb[i] ^ 0x5c;
	h64_resumen_ini(&h->ex, t);
	h64_resumen_mas(&h->ex, pad, b);
	h64_borra(kb, sizeof kb);
	h64_borra(pad, sizeof pad);
}
void h64_hmac_mas(h64_hmac *h, const void *d, size_t n) { h64_resumen_mas(&h->in, d, n); }
void h64_hmac_fin(h64_hmac *h, uint8_t *sal)
{
	uint8_t r[64];
	h64_resumen_fin(&h->in, r);
	h64_resumen_mas(&h->ex, r, h64_resumen_tam(h->tipo));
	h64_resumen_fin(&h->ex, sal);
	h64_borra(r, sizeof r);
	h64_borra(h, sizeof *h);
}

/* ---- el DigestInfo de PKCS #1 v1.5 (RFC 8017, 9.2, nota 1; los de SHA-3, de NIST CSOR) ---- */
static const uint8_t DI_SHA1[] = { 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14 };
static const uint8_t DI_SHA224[] = { 0x30, 0x2d, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x04, 0x05, 0x00, 0x04, 0x1c };
static const uint8_t DI_SHA256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
static const uint8_t DI_SHA384[] = { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02, 0x05, 0x00, 0x04, 0x30 };
static const uint8_t DI_SHA512[] = { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03, 0x05, 0x00, 0x04, 0x40 };
static const uint8_t DI_SHA3_256[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x08, 0x05, 0x00, 0x04, 0x20 };
static const uint8_t DI_SHA3_384[] = { 0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x09, 0x05, 0x00, 0x04, 0x30 };
static const uint8_t DI_SHA3_512[] = { 0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x0a, 0x05, 0x00, 0x04, 0x40 };
const uint8_t *h64_resumen_digestinfo(int t, size_t *n)
{
#define DEVUELVE(x) do { *n = sizeof x; return x; } while (0)
	switch (t) {
	case R_SHA1: DEVUELVE(DI_SHA1);
	case R_SHA224: DEVUELVE(DI_SHA224);
	case R_SHA256: DEVUELVE(DI_SHA256);
	case R_SHA384: DEVUELVE(DI_SHA384);
	case R_SHA512: DEVUELVE(DI_SHA512);
	case R_SHA3_256: DEVUELVE(DI_SHA3_256);
	case R_SHA3_384: DEVUELVE(DI_SHA3_384);
	case R_SHA3_512: DEVUELVE(DI_SHA3_512);
	default: *n = 0; return NULL;
	}
#undef DEVUELVE
}
void h64_mgf1_xor(int t, uint8_t *sal, size_t n, const uint8_t *semilla, size_t sn)
{
	size_t hl = h64_resumen_tam(t);
	uint8_t r[64];
	for (uint32_t c = 0; n; c++) {
		uint8_t cb[4] = { (uint8_t)(c >> 24), (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
		h64_resumen h;
		h64_resumen_ini(&h, t);
		h64_resumen_mas(&h, semilla, sn);
		h64_resumen_mas(&h, cb, 4);
		h64_resumen_fin(&h, r);
		size_t k = n < hl ? n : hl;
		for (size_t i = 0; i < k; i++) sal[i] ^= r[i];
		sal += k;
		n -= k;
	}
	h64_borra(r, sizeof r);
}

/* ---- EVP_MD y los resúmenes de OpenSSL ---- */
typedef struct evp_md_st { int tipo; } EVP_MD;
static EVP_MD MDS[R_KMACK256 + 1];
H64_VISIBLE EVP_MD *EVP_MD_fetch(void *lib, const char *nombre, const char *prop)
{
	(void)lib; (void)prop;
	int t = h64_resumen_tipo(nombre);
	if (!t) return NULL;
	MDS[t].tipo = t;
	return &MDS[t];
}
H64_VISIBLE void EVP_MD_free(EVP_MD *m) { (void)m; }
H64_VISIBLE int EVP_Q_digest(void *lib, const char *nombre, const char *prop, const void *d, size_t n, uint8_t *sal, size_t *saln)
{
	(void)lib; (void)prop;
	int t = h64_resumen_tipo(nombre);
	if (!t || h64_resumen_es_xof(t)) return 0;
	h64_resumen_de(t, d, n, sal);
	if (saln) *saln = h64_resumen_tam(t);
	return 1;
}

/* EVP_MD_CTX: un resumen, o firmar o comprobar con una clave (claves.c) */
#include "mdctx.h"
H64_VISIBLE EVP_MD_CTX *EVP_MD_CTX_new(void) { return calloc(1, sizeof(EVP_MD_CTX)); }
H64_VISIBLE void EVP_MD_CTX_free(EVP_MD_CTX *c)
{
	if (!c) return;
	h64_mdctx_suelta(c);
	h64_borra(c, sizeof *c);
	free(c);
}
H64_VISIBLE int EVP_DigestInit_ex(EVP_MD_CTX *c, const EVP_MD *m, void *motor)
{
	(void)motor;
	if (!c || !m) return 0;
	h64_mdctx_suelta(c);
	c->modo = MD_RESUMEN;
	h64_resumen_ini(&c->r, m->tipo);
	return 1;
}
H64_VISIBLE int EVP_DigestUpdate(EVP_MD_CTX *c, const void *d, size_t n)
{
	if (!c || c->modo != MD_RESUMEN) return 0;
	h64_resumen_mas(&c->r, d, n);
	return 1;
}
H64_VISIBLE int EVP_DigestFinalXOF(EVP_MD_CTX *c, uint8_t *sal, size_t n)
{
	if (!c || c->modo != MD_RESUMEN || !h64_resumen_es_xof(c->r.tipo)) return 0;
	h64_resumen_saca(&c->r, sal, n);
	return 1;
}

/* ---- EVP_MAC: HMAC y KMAC (SP 800-185, 4) ---- */
enum { MAC_HMAC = 1, MAC_KMAC128, MAC_KMAC256 };
typedef struct evp_mac_st { int tipo; } EVP_MAC;
static EVP_MAC MACS[] = { { 0 }, { MAC_HMAC }, { MAC_KMAC128 }, { MAC_KMAC256 } };
typedef struct evp_mac_ctx_st {
	int tipo;
	int listo;
	int resumen;      /* HMAC */
	size_t tam;       /* KMAC: lo que sale */
	h64_hmac h;
	h64_keccak k;
} EVP_MAC_CTX;
H64_VISIBLE EVP_MAC *EVP_MAC_fetch(void *lib, const char *nombre, const char *prop)
{
	(void)lib; (void)prop;
	if (!nombre) return NULL;
	if (!strcasecmp(nombre, "HMAC")) return &MACS[MAC_HMAC];
	if (!strcasecmp(nombre, "KMAC-128") || !strcasecmp(nombre, "KMAC128")) return &MACS[MAC_KMAC128];
	if (!strcasecmp(nombre, "KMAC-256") || !strcasecmp(nombre, "KMAC256")) return &MACS[MAC_KMAC256];
	return NULL;
}
H64_VISIBLE void EVP_MAC_free(EVP_MAC *m) { (void)m; }
H64_VISIBLE EVP_MAC_CTX *EVP_MAC_CTX_new(EVP_MAC *m)
{
	if (!m) return NULL;
	EVP_MAC_CTX *c = calloc(1, sizeof *c);
	if (!c) return NULL;
	c->tipo = m->tipo;
	c->tam = m->tipo == MAC_KMAC128 ? 32 : 64;
	return c;
}
H64_VISIBLE void EVP_MAC_CTX_free(EVP_MAC_CTX *c)
{
	if (!c) return;
	h64_borra(c, sizeof *c);
	free(c);
}
/* left_encode y encode_string de SP 800-185, 2.3 */
static size_t codifica_izq(uint8_t *b, uint64_t v)
{
	uint8_t t[8];
	size_t n = 0;
	do { t[n++] = (uint8_t)v; v >>= 8; } while (v);
	b[0] = (uint8_t)n;
	for (size_t i = 0; i < n; i++) b[1 + i] = t[n - 1 - i];
	return n + 1;
}
static size_t codifica_der(uint8_t *b, uint64_t v)
{
	uint8_t t[8];
	size_t n = 0;
	do { t[n++] = (uint8_t)v; v >>= 8; } while (v);
	for (size_t i = 0; i < n; i++) b[i] = t[n - 1 - i];
	b[n] = (uint8_t)n;
	return n + 1;
}
static void keccak_cadena(h64_keccak *k, const uint8_t *s, size_t n, size_t *cuenta)
{
	uint8_t b[9];
	size_t l = codifica_izq(b, (uint64_t)n * 8);
	h64_keccak_mas(k, b, l);
	h64_keccak_mas(k, s, n);
	*cuenta += l + n;
}
static void keccak_rellena(h64_keccak *k, size_t cuenta, size_t tasa)
{
	static const uint8_t ceros[168];
	size_t r = cuenta % tasa;
	if (r) h64_keccak_mas(k, ceros, tasa - r);
}
H64_VISIBLE int EVP_MAC_init(EVP_MAC_CTX *c, const uint8_t *k, size_t kn, const OSSL_PARAM *p)
{
	if (!c) return 0;
	c->listo = 0;
	if (c->tipo == MAC_HMAC) {
		size_t n;
		const char *d = h64_param_texto(p, "digest", &n);
		if (d) c->resumen = h64_resumen_tipo(d);
		if (!c->resumen || h64_resumen_es_xof(c->resumen)) return 0;
		h64_hmac_ini(&c->h, c->resumen, k, kn);
		c->listo = 1;
		return 1;
	}
	/* KMAC: como OpenSSL, la clave de 4 a 512 octetos y lo de personalizar, hasta 512 */
	uint64_t tam;
	if (h64_param_uint(p, "size", &tam)) {
		if (tam == 0 || tam > 0xFFFFFF / 8) return 0;
		c->tam = (size_t)tam;
	}
	size_t cn = 0;
	const uint8_t *cus = h64_param_octetos(p, "custom", &cn);
	if (cn > 512 || kn < 4 || kn > 512) return 0;
	size_t tasa = c->tipo == MAC_KMAC128 ? 168 : 136;
	h64_keccak_ini(&c->k, tasa, 0x04);
	/* cSHAKE(X, L, "KMAC", S): bytepad(encode_string("KMAC") || encode_string(S), tasa) */
	uint8_t b[9];
	size_t cuenta = codifica_izq(b, tasa);
	h64_keccak_mas(&c->k, b, cuenta);
	keccak_cadena(&c->k, (const uint8_t *)"KMAC", 4, &cuenta);
	keccak_cadena(&c->k, cus ? cus : (const uint8_t *)"", cn, &cuenta);
	keccak_rellena(&c->k, cuenta, tasa);
	/* X = bytepad(encode_string(K), tasa) || el mensaje || right_encode(L) */
	cuenta = codifica_izq(b, tasa);
	h64_keccak_mas(&c->k, b, cuenta);
	keccak_cadena(&c->k, k, kn, &cuenta);
	keccak_rellena(&c->k, cuenta, tasa);
	c->listo = 1;
	return 1;
}
H64_VISIBLE int EVP_MAC_update(EVP_MAC_CTX *c, const uint8_t *d, size_t n)
{
	if (!c || !c->listo) return 0;
	if (c->tipo == MAC_HMAC) h64_hmac_mas(&c->h, d, n);
	else h64_keccak_mas(&c->k, d, n);
	return 1;
}
H64_VISIBLE int EVP_MAC_final(EVP_MAC_CTX *c, uint8_t *sal, size_t *saln, size_t tam)
{
	if (!c || !c->listo) return 0;
	size_t n = c->tipo == MAC_HMAC ? h64_resumen_tam(c->resumen) : c->tam;
	if (saln) *saln = n;
	if (!sal) return 1;
	if (tam < n) return 0;
	if (c->tipo == MAC_HMAC) h64_hmac_fin(&c->h, sal);
	else {
		uint8_t b[9];
		size_t l = codifica_der(b, (uint64_t)n * 8);
		h64_keccak_mas(&c->k, b, l);
		h64_keccak_saca(&c->k, sal, n);
	}
	c->listo = 0;
	return 1;
}
