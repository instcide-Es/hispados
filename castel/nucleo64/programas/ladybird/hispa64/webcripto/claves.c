/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/claves.c - Hispa-64, etapa 10 i: las claves de la
 * WebCrypto (EVP_PKEY de OpenSSL) y lo que se hace con ellas:
 *  - RSA (BearSSL: generar, con i62 o i31; las operaciones privadas con el
 *    CRT, de tiempo constante; las públicas): PKCS #1 v1.5 y PSS para
 *    firmar, OAEP para cifrar (RFC 8017, nuestros, con SHA-1, SHA-2 y SHA-3);
 *    las comprobaciones de OpenSSL al importar (SP 800-56B);
 *  - EC con P-256, P-384 y P-521 (BearSSL, ec_all_m31): ECDSA (con el número
 *    de RFC 6979) y ECDH; los puntos comprimidos (la raíz, con bn.c);
 *  - Ed25519 y X25519 (monocypher).
 * Y EC_GROUP, EC_POINT y ECDSA_SIG, que el Rust usa para sacar la clave
 * pública de un escalar y para pasar las firmas de ECDSA a DER y de vuelta.
 * Ed448, X448, ML-DSA y ML-KEM no están: fallan como OpenSSL cuando no puede
 * (nulo o 0), y la WebCrypto dice que no se puede.
 */
#include <stdlib.h>
#include <strings.h>
#include "interno.h"
#include "mdctx.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"

/* ---- las curvas ---- */
static const struct {
	int curva, nid;
	size_t tam, bits;
	const char *p, *b;
	uint8_t oid[8];
	size_t oidn;
} CURVAS[] = {
	{ BR_EC_secp256r1, 415, 32, 256,
	  "ffffffff00000001000000000000000000000000ffffffffffffffffffffffff",
	  "5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b",
	  { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 }, 8 },
	{ BR_EC_secp384r1, 715, 48, 384,
	  "fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffeffffffff0000000000000000ffffffff",
	  "b3312fa7e23ee7e4988e056be3f82d19181d9c6efe8141120314088f5013875ac656398d8a2ed19d2a85c8edd3ec2aef",
	  { 0x2b, 0x81, 0x04, 0x00, 0x22 }, 5 },
	{ BR_EC_secp521r1, 716, 66, 521,
	  "01ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
	  "0051953eb9618e1c9a1f929a21a0b68540eea2da725b99b315f3b8b489918ef109e156193951ec7e937b1652c0bd3bb1bf073573df883d2c34f1ef451fd46b503f00",
	  { 0x2b, 0x81, 0x04, 0x00, 0x23 }, 5 },
};
static int indice_curva(int curva)
{
	for (int i = 0; i < 3; i++) if (CURVAS[i].curva == curva) return i;
	return -1;
}
size_t h64_curva_tam(int curva) { int i = indice_curva(curva); return i < 0 ? 0 : CURVAS[i].tam; }
size_t h64_curva_bits(int curva) { int i = indice_curva(curva); return i < 0 ? 0 : CURVAS[i].bits; }
int h64_curva_por_nid(int nid) { for (int i = 0; i < 3; i++) if (CURVAS[i].nid == nid) return CURVAS[i].curva; return 0; }
int h64_curva_por_nombre(const char *s)
{
	if (!s) return 0;
	if (!strcasecmp(s, "P-256") || !strcasecmp(s, "prime256v1") || !strcasecmp(s, "secp256r1")) return BR_EC_secp256r1;
	if (!strcasecmp(s, "P-384") || !strcasecmp(s, "secp384r1")) return BR_EC_secp384r1;
	if (!strcasecmp(s, "P-521") || !strcasecmp(s, "secp521r1")) return BR_EC_secp521r1;
	return 0;
}
int h64_curva_por_oid(const uint8_t *oid, size_t n)
{
	for (int i = 0; i < 3; i++) if (CURVAS[i].oidn == n && !memcmp(CURVAS[i].oid, oid, n)) return CURVAS[i].curva;
	return 0;
}
const uint8_t *h64_curva_oid(int curva, size_t *n)
{
	int i = indice_curva(curva);
	if (i < 0) return NULL;
	*n = CURVAS[i].oidn;
	return CURVAS[i].oid;
}
static int de_hex(BIGNUM *r, const char *h)
{
	uint8_t b[70];
	size_t n = strlen(h) / 2;
	for (size_t i = 0; i < n; i++) {
		unsigned v;
		char t[3] = { h[2 * i], h[2 * i + 1], 0 };
		v = (unsigned)strtoul(t, NULL, 16);
		b[i] = (uint8_t)v;
	}
	return h64bn_de_octetos(r, b, n);
}
static const br_ec_impl *ec(void) { return &br_ec_all_m31; }
/* el orden de la curva, como número */
static int orden(int curva, BIGNUM *r)
{
	size_t n;
	const unsigned char *o = ec()->order(curva, &n);
	return o && h64bn_de_octetos(r, o, n);
}
/* y² = x³ - 3x + b (mod p): el lado derecho */
static int lado_derecho(int i, const BIGNUM *x, BIGNUM *r, BIGNUM *p)
{
	BIGNUM *b = h64bn_nuevo(), *t = h64bn_nuevo(), *tres = h64bn_nuevo();
	int ok = b && t && tres && de_hex(p, CURVAS[i].p) && de_hex(b, CURVAS[i].b) && h64bn_pon(tres, 3)
		&& h64bn_modmul(t, x, x, p) && h64bn_modmul(t, t, x, p) && h64bn_suma(t, t, b);
	/* + 3·(p - x) */
	BIGNUM *px = h64bn_nuevo();
	ok = ok && px && h64bn_resta(px, p, x) && h64bn_mul(px, px, tres) && h64bn_suma(t, t, px) && h64bn_mod(r, t, p);
	h64bn_libera(b); h64bn_libera(t); h64bn_libera(tres); h64bn_libera(px);
	return ok;
}
int h64_ec_pon_punto(EVP_PKEY *k, int curva, const uint8_t *pt, size_t n)
{
	int i = indice_curva(curva);
	if (i < 0 || !n) return 0;
	size_t tam = CURVAS[i].tam;
	BIGNUM *x = h64bn_nuevo(), *y = h64bn_nuevo(), *p = h64bn_nuevo(), *r = h64bn_nuevo(), *y2 = h64bn_nuevo();
	int ok = x && y && p && r && y2, comp = 0;
	if (ok && pt[0] == 0x04 && n == 1 + 2 * tam) {
		ok = h64bn_de_octetos(x, pt + 1, tam) && h64bn_de_octetos(y, pt + 1 + tam, tam) && lado_derecho(i, x, r, p)
			&& h64bn_cmp(x, p) < 0 && h64bn_cmp(y, p) < 0 && h64bn_modmul(y2, y, y, p) && !h64bn_cmp(y2, r);
	} else if (ok && (pt[0] == 0x02 || pt[0] == 0x03) && n == 1 + tam) {
		/* la raíz: r^((p+1)/4), que vale en las tres (p ≡ 3 mod 4) */
		BIGNUM *e = h64bn_nuevo(), *uno = h64bn_nuevo();
		ok = e && uno && h64bn_de_octetos(x, pt + 1, tam) && lado_derecho(i, x, r, p) && h64bn_cmp(x, p) < 0
			&& h64bn_pon(uno, 1) && h64bn_suma(e, p, uno) && h64bn_shr(e, e, 2) && h64bn_modexp(y, r, e, p)
			&& h64bn_modmul(y2, y, y, p) && !h64bn_cmp(y2, r);
		if (ok && (unsigned)h64bn_es_impar(y) != (unsigned)(pt[0] & 1)) {
			if (h64bn_es_cero(y)) ok = 0;
			else ok = h64bn_resta(y, p, y);
		}
		h64bn_libera(e); h64bn_libera(uno);
		comp = 1;
	} else ok = 0;
	if (ok) {
		k->curva = curva;
		k->tam = tam;
		k->pub[0] = 0x04;
		ok = h64bn_a_octetos(x, k->pub + 1, tam) && h64bn_a_octetos(y, k->pub + 1 + tam, tam);
		k->publen = 1 + 2 * tam;
		k->comprimido = comp;
	}
	h64bn_libera(x); h64bn_libera(y); h64bn_libera(p); h64bn_libera(r); h64bn_libera(y2);
	return ok;
}
/* el escalar privado (de cualquier ancho; tiene que estar en [1, n-1]) y su punto */
int h64_ec_pon_privada(EVP_PKEY *k, int curva, const uint8_t *d, size_t n)
{
	int i = indice_curva(curva);
	if (i < 0) return 0;
	size_t tam = CURVAS[i].tam;
	BIGNUM *dd = h64bn_nuevo(), *o = h64bn_nuevo();
	int ok = dd && o && h64bn_de_octetos(dd, d, n) && orden(curva, o) && !h64bn_es_cero(dd) && h64bn_cmp(dd, o) < 0
		&& h64bn_a_octetos(dd, k->dd, tam);
	h64bn_libera(dd); h64bn_libera(o);
	if (!ok) return 0;
	k->curva = curva;
	k->tam = tam;
	k->privada = 1;
	if (!k->publen) {
		k->publen = ec()->mulgen(k->pub, k->dd, tam, curva);
		k->comprimido = 0;
		if (k->publen != 1 + 2 * tam) return 0;
	}
	return 1;
}

/* ---- las claves ---- */
EVP_PKEY *h64_clave_nueva(int tipo)
{
	EVP_PKEY *k = calloc(1, sizeof *k);
	if (k) { k->refs = 1; k->tipo = tipo; }
	return k;
}
const char *h64_nombre_tipo(int t)
{
	switch (t) {
	case K_RSA: return "RSA";
	case K_EC: return "EC";
	case K_ED25519: return "ED25519";
	case K_X25519: return "X25519";
	}
	return "";
}
static int tipo_por_nombre(const char *s)
{
	if (!s) return 0;
	if (!strcasecmp(s, "RSA") || !strcasecmp(s, "rsaEncryption")) return K_RSA;
	if (!strcasecmp(s, "EC")) return K_EC;
	if (!strcasecmp(s, "ED25519")) return K_ED25519;
	if (!strcasecmp(s, "X25519")) return K_X25519;
	return 0;
}
H64_VISIBLE int EVP_PKEY_up_ref(EVP_PKEY *k)
{
	if (!k) return 0;
	__atomic_add_fetch(&k->refs, 1, __ATOMIC_SEQ_CST);
	return 1;
}
H64_VISIBLE void EVP_PKEY_free(EVP_PKEY *k)
{
	if (!k || __atomic_sub_fetch(&k->refs, 1, __ATOMIC_SEQ_CST) > 0) return;
	BIGNUM *bs[] = { k->n, k->e, k->d, k->p, k->q, k->dp, k->dq, k->qi };
	for (size_t i = 0; i < 8; i++) h64bn_libera(bs[i]);
	h64_borra(k, sizeof *k);
	free(k);
}

typedef struct evp_pkey_ctx_st {
	EVP_PKEY *k, *par;
	int tipo;
	int op;
	/* generar */
	int curva;
	uint64_t bits;
	BIGNUM *e;
	/* RSA */
	int relleno, resumen, mgf1;
	long sal;
	uint8_t *etiq;
	size_t etiq_n;
} EVP_PKEY_CTX;
enum { OP_NADA, OP_FIRMA, OP_COMPRUEBA, OP_CIFRA, OP_DESCIFRA, OP_DERIVA, OP_GENERA, OP_DATOS };

H64_VISIBLE EVP_PKEY_CTX *EVP_PKEY_CTX_new_from_name(void *lib, const char *nombre, const char *prop)
{
	(void)lib; (void)prop;
	int t = tipo_por_nombre(nombre);
	if (!t) return NULL;
	EVP_PKEY_CTX *c = calloc(1, sizeof *c);
	if (c) { c->tipo = t; c->bits = 2048; }
	return c;
}
H64_VISIBLE EVP_PKEY_CTX *EVP_PKEY_CTX_new_from_pkey(void *lib, EVP_PKEY *k, const char *prop)
{
	(void)lib; (void)prop;
	if (!k) return NULL;
	EVP_PKEY_CTX *c = calloc(1, sizeof *c);
	if (!c) return NULL;
	EVP_PKEY_up_ref(k);
	c->k = k;
	c->tipo = k->tipo;
	return c;
}
H64_VISIBLE void EVP_PKEY_CTX_free(EVP_PKEY_CTX *c)
{
	if (!c) return;
	EVP_PKEY_free(c->k);
	EVP_PKEY_free(c->par);
	h64bn_libera(c->e);
	if (c->etiq) { h64_borra(c->etiq, c->etiq_n); free(c->etiq); }
	free(c);
}

/* un número de un OSSL_PARAM: el entero sin signo en el orden de la máquina
 * (como OpenSSL; lo deja así OSSL_PARAM_BLD_push_BN, sinopenssl.c) */
static BIGNUM *param_bn(const OSSL_PARAM *p, const char *clave)
{
	const OSSL_PARAM *q = h64_param(p, clave);
	if (!q || q->data_type != OSSL_PARAM_UNSIGNED_INTEGER || !q->data) return NULL;
	uint8_t *b = malloc(q->data_size ? q->data_size : 1);
	if (!b) return NULL;
	for (size_t i = 0; i < q->data_size; i++) b[i] = ((const uint8_t *)q->data)[q->data_size - 1 - i];
	BIGNUM *r = h64bn_nuevo();
	if (r && !h64bn_de_octetos(r, b, q->data_size)) { h64bn_libera(r); r = NULL; }
	h64_borra(b, q->data_size);
	free(b);
	return r;
}

H64_VISIBLE int EVP_PKEY_CTX_set_params(EVP_PKEY_CTX *c, const OSSL_PARAM *p)
{
	if (!c) return 0;
	size_t n;
	const char *g = h64_param_texto(p, "group", &n);
	if (g) { c->curva = h64_curva_por_nombre(g); if (!c->curva) return 0; }
	uint64_t bits;
	if (h64_param_uint(p, "bits", &bits)) c->bits = bits;
	BIGNUM *e = param_bn(p, "e");
	if (e) { h64bn_libera(c->e); c->e = e; }
	return 1;
}
H64_VISIBLE int EVP_PKEY_fromdata_init(EVP_PKEY_CTX *c) { if (!c) return 0; c->op = OP_DATOS; return 1; }
H64_VISIBLE int EVP_PKEY_keygen_init(EVP_PKEY_CTX *c) { if (!c) return 0; c->op = OP_GENERA; return 1; }

/* el CRT de una clave RSA que solo trae n, e y d (RFC 8017 no lo dice; es el
 * de siempre: k = d·e - 1 = 2^t·r, y una raíz de 1 que no sea ±1 da p) */
int h64_rsa_completa(EVP_PKEY *k)
{
	BIGNUM *kk = h64bn_nuevo(), *r = h64bn_nuevo(), *g = h64bn_nuevo(), *y = h64bn_nuevo(), *x = h64bn_nuevo();
	BIGNUM *n1 = h64bn_nuevo(), *p = h64bn_nuevo(), *q = h64bn_nuevo(), *rr = h64bn_nuevo(), *t1 = h64bn_nuevo();
	int ok = kk && r && g && y && x && n1 && p && q && rr && t1 && h64bn_mul(kk, k->d, k->e) && h64bn_resta1(kk, kk)
		&& h64bn_resta1(n1, k->n) && !h64bn_es_cero(kk);
	int hallado = 0;
	size_t t = 0;
	while (ok && !h64bn_bit(kk, t)) t++;
	ok = ok && t > 0 && h64bn_shr(r, kk, t);
	for (int intento = 0; ok && !hallado && intento < 100; intento++) {
		ok = h64bn_azar_menor(g, k->n) && h64bn_modexp(y, g, r, k->n);
		if (!ok || h64bn_es_uno(y) || !h64bn_cmp(y, n1)) continue;
		for (size_t i = 1; ok && i <= t; i++) {
			ok = h64bn_modmul(x, y, y, k->n);
			if (!ok) break;
			if (h64bn_es_uno(x)) {
				ok = h64bn_resta1(t1, y) && h64bn_mcd(p, t1, k->n) && h64bn_divmod(q, rr, k->n, p);
				if (ok && h64bn_es_cero(rr) && !h64bn_es_uno(p) && !h64bn_es_uno(q)) hallado = 1;
				break;
			}
			if (!h64bn_cmp(x, n1)) break;
			ok = h64bn_copia(y, x);
		}
	}
	if (ok && hallado) {
		if (h64bn_cmp(p, q) < 0) { BIGNUM *z = p; p = q; q = z; }
		k->p = p; k->q = q; p = q = NULL;
		k->dp = h64bn_nuevo(); k->dq = h64bn_nuevo(); k->qi = h64bn_nuevo();
		ok = k->dp && k->dq && k->qi && h64bn_resta1(t1, k->p) && h64bn_mod(k->dp, k->d, t1) && h64bn_resta1(t1, k->q)
			&& h64bn_mod(k->dq, k->d, t1) && h64bn_modinv(k->qi, k->q, k->p);
		k->crt_dentro = ok;
	}
	h64bn_libera(kk); h64bn_libera(r); h64bn_libera(g); h64bn_libera(y); h64bn_libera(x);
	h64bn_libera(n1); h64bn_libera(p); h64bn_libera(q); h64bn_libera(rr); h64bn_libera(t1);
	return ok && hallado;
}

H64_VISIBLE int EVP_PKEY_fromdata(EVP_PKEY_CTX *c, EVP_PKEY **sal, int seleccion, OSSL_PARAM *p)
{
	(void)seleccion;
	if (!c || !sal || c->op != OP_DATOS) return 0;
	EVP_PKEY *k = h64_clave_nueva(c->tipo);
	if (!k) return 0;
	int ok = 0;
	if (c->tipo == K_RSA) {
		k->n = param_bn(p, "n");
		k->e = param_bn(p, "e");
		k->d = param_bn(p, "d");
		ok = k->n && k->e && !h64bn_es_cero(k->n) && !h64_param(p, "rsa-factor3");
		if (ok && k->d) {
			k->privada = 1;
			k->p = param_bn(p, "rsa-factor1");
			k->q = param_bn(p, "rsa-factor2");
			k->dp = param_bn(p, "rsa-exponent1");
			k->dq = param_bn(p, "rsa-exponent2");
			k->qi = param_bn(p, "rsa-coefficient1");
			int cuantos = !!k->p + !!k->q + !!k->dp + !!k->dq + !!k->qi;
			if (cuantos == 5) k->crt = 1;
			else if (cuantos == 0) h64_rsa_completa(k);   /* si no sale, se opera con d a pelo */
			else ok = 0;
		}
	} else if (c->tipo == K_EC) {
		size_t n, pn = 0;
		int curva = h64_curva_por_nombre(h64_param_texto(p, "group", &n));
		const uint8_t *pt = h64_param_octetos(p, "pub", &pn);
		BIGNUM *d = param_bn(p, "priv");
		ok = curva != 0;
		if (ok && pt) ok = h64_ec_pon_punto(k, curva, pt, pn);
		if (ok && d) {
			uint8_t b[66];
			size_t dn = h64bn_octetos(d);
			ok = dn <= 66 && h64bn_a_octetos(d, b, dn) && h64_ec_pon_privada(k, curva, b, dn);
			h64_borra(b, sizeof b);
		}
		if (ok && !pt && !d) ok = 0;
		h64bn_libera(d);
	}
	if (!ok) { EVP_PKEY_free(k); return 0; }
	*sal = k;
	return 1;
}

/* el azar, para BearSSL */
static int genera_rsa(EVP_PKEY_CTX *c, EVP_PKEY *k)
{
	uint32_t e = 65537;
	if (c->e) {
		if (h64bn_bits(c->e) > 32 || !h64bn_es_impar(c->e) || h64bn_bits(c->e) < 2) return 0;
		e = c->e->n ? c->e->w[0] : 0;
	}
	if (c->bits < 512 || c->bits > 4096) return 0;
	unsigned bits = (unsigned)c->bits;
	uint8_t *kpriv = malloc(BR_RSA_KBUF_PRIV_SIZE(4096)), *kpub = malloc(BR_RSA_KBUF_PUB_SIZE(4096));
	br_rsa_private_key sk;
	br_rsa_public_key pk;
	h64_prng g;
	h64_prng_ini(&g);
	int ok = kpriv && kpub && br_rsa_keygen_get_default()(&g.vtable, &sk, kpriv, &pk, kpub, bits, e);
	if (ok) {
		BIGNUM **bs[] = { &k->n, &k->e, &k->p, &k->q, &k->dp, &k->dq, &k->qi, &k->d };
		for (size_t i = 0; i < 8; i++) { *bs[i] = h64bn_nuevo(); if (!*bs[i]) ok = 0; }
		ok = ok && h64bn_de_octetos(k->n, pk.n, pk.nlen) && h64bn_de_octetos(k->e, pk.e, pk.elen)
			&& h64bn_de_octetos(k->p, sk.p, sk.plen) && h64bn_de_octetos(k->q, sk.q, sk.qlen)
			&& h64bn_de_octetos(k->dp, sk.dp, sk.dplen) && h64bn_de_octetos(k->dq, sk.dq, sk.dqlen)
			&& h64bn_de_octetos(k->qi, sk.iq, sk.iqlen);
		/* d = e^-1 mod mcm(p-1, q-1), como OpenSSL */
		BIGNUM *p1 = h64bn_nuevo(), *q1 = h64bn_nuevo(), *m = h64bn_nuevo(), *g2 = h64bn_nuevo(), *r = h64bn_nuevo();
		ok = ok && p1 && q1 && m && g2 && r && h64bn_resta1(p1, k->p) && h64bn_resta1(q1, k->q) && h64bn_mul(m, p1, q1)
			&& h64bn_mcd(g2, p1, q1) && h64bn_divmod(m, r, m, g2) && h64bn_modinv(k->d, k->e, m);
		h64bn_libera(p1); h64bn_libera(q1); h64bn_libera(m); h64bn_libera(g2); h64bn_libera(r);
		k->crt = 1;
		k->privada = 1;
	}
	if (kpriv) { h64_borra(kpriv, BR_RSA_KBUF_PRIV_SIZE(4096)); free(kpriv); }
	free(kpub);
	return ok;
}
H64_VISIBLE int EVP_PKEY_generate(EVP_PKEY_CTX *c, EVP_PKEY **sal)
{
	if (!c || !sal || c->op != OP_GENERA) return 0;
	EVP_PKEY *k = h64_clave_nueva(c->tipo);
	if (!k) return 0;
	int ok = 0;
	h64_prng g;
	h64_prng_ini(&g);
	switch (c->tipo) {
	case K_RSA: ok = genera_rsa(c, k); break;
	case K_EC: {
		br_ec_private_key sk;
		uint8_t kb[BR_EC_KBUF_PRIV_MAX_SIZE];
		ok = c->curva && br_ec_keygen(&g.vtable, ec(), &sk, kb, c->curva) == h64_curva_tam(c->curva)
			&& h64_ec_pon_privada(k, c->curva, kb, h64_curva_tam(c->curva));
		h64_borra(kb, sizeof kb);
		break;
	}
	case K_ED25519: {
		uint8_t semilla[32];
		ok = h64_azar(k->cruda, 32);
		memcpy(semilla, k->cruda, 32);
		crypto_ed25519_key_pair(k->edsk, k->crudap, semilla);
		k->privada = 1;
		break;
	}
	case K_X25519:
		ok = h64_azar(k->cruda, 32);
		crypto_x25519_public_key(k->crudap, k->cruda);
		k->privada = 1;
		break;
	}
	if (!ok) { EVP_PKEY_free(k); return 0; }
	*sal = k;
	return 1;
}

H64_VISIBLE int EVP_PKEY_get_bn_param(const EVP_PKEY *k, const char *nombre, BIGNUM **sal)
{
	if (!k || !nombre || !sal) return 0;
	const BIGNUM *v = NULL;
	BIGNUM tmp = { 0 };
	uint32_t pal[20];
	if (k->tipo == K_RSA) {
		int crt = k->crt;
		if (!strcmp(nombre, "n")) v = k->n;
		else if (!strcmp(nombre, "e")) v = k->e;
		else if (!strcmp(nombre, "d")) v = k->d;
		else if (crt && !strcmp(nombre, "rsa-factor1")) v = k->p;
		else if (crt && !strcmp(nombre, "rsa-factor2")) v = k->q;
		else if (crt && !strcmp(nombre, "rsa-exponent1")) v = k->dp;
		else if (crt && !strcmp(nombre, "rsa-exponent2")) v = k->dq;
		else if (crt && !strcmp(nombre, "rsa-coefficient1")) v = k->qi;
	} else if (k->tipo == K_EC && k->publen) {
		const uint8_t *b = NULL;
		if (!strcmp(nombre, "qx")) b = k->pub + 1;
		else if (!strcmp(nombre, "qy")) b = k->pub + 1 + k->tam;
		else if (!strcmp(nombre, "priv") && k->privada) b = k->dd;
		if (b) {
			tmp.w = pal; tmp.cap = 20;
			memset(pal, 0, sizeof pal);
			for (size_t i = 0; i < k->tam; i++) pal[i / 4] |= (uint32_t)b[k->tam - 1 - i] << (8 * (i % 4));
			tmp.n = (k->tam + 3) / 4;
			while (tmp.n && !pal[tmp.n - 1]) tmp.n--;
			v = &tmp;
		}
	}
	if (!v) return 0;
	BIGNUM *r = *sal ? *sal : h64bn_nuevo();
	if (!r || !h64bn_copia(r, v)) { if (!*sal) h64bn_libera(r); return 0; }
	h64_borra(pal, sizeof pal);
	*sal = r;
	return 1;
}
H64_VISIBLE int EVP_PKEY_get_octet_string_param(const EVP_PKEY *k, const char *nombre, uint8_t *b, size_t tam, size_t *n)
{
	if (!k || !nombre || k->tipo != K_EC || !k->publen || (strcmp(nombre, "pub") && strcmp(nombre, "encoded-pub-key"))) return 0;
	if (n) *n = k->publen;
	if (!b) return 1;
	if (tam < k->publen) return 0;
	memcpy(b, k->pub, k->publen);
	return 1;
}
H64_VISIBLE int EVP_PKEY_get_raw_public_key(const EVP_PKEY *k, uint8_t *b, size_t *n)
{
	if (!k || !n || (k->tipo != K_ED25519 && k->tipo != K_X25519)) return 0;
	if (!b) { *n = 32; return 1; }
	if (*n < 32) return 0;
	memcpy(b, k->crudap, 32);
	*n = 32;
	return 1;
}
H64_VISIBLE int EVP_PKEY_get_raw_private_key(const EVP_PKEY *k, uint8_t *b, size_t *n)
{
	if (!k || !n || !k->privada || (k->tipo != K_ED25519 && k->tipo != K_X25519)) return 0;
	if (!b) { *n = 32; return 1; }
	if (*n < 32) return 0;
	memcpy(b, k->cruda, 32);
	*n = 32;
	return 1;
}
static EVP_PKEY *cruda(const char *nombre, const uint8_t *b, size_t n, int privada)
{
	int t = tipo_por_nombre(nombre);
	if ((t != K_ED25519 && t != K_X25519) || n != 32) return NULL;
	EVP_PKEY *k = h64_clave_nueva(t);
	if (!k) return NULL;
	if (privada) {
		uint8_t semilla[32];
		memcpy(k->cruda, b, 32);
		memcpy(semilla, b, 32);
		if (t == K_ED25519) crypto_ed25519_key_pair(k->edsk, k->crudap, semilla);
		else crypto_x25519_public_key(k->crudap, k->cruda);
		h64_borra(semilla, 32);
		k->privada = 1;
	} else memcpy(k->crudap, b, 32);
	return k;
}
H64_VISIBLE EVP_PKEY *EVP_PKEY_new_raw_public_key_ex(void *lib, const char *nombre, const char *prop, const uint8_t *b, size_t n)
{
	(void)lib; (void)prop;
	return cruda(nombre, b, n, 0);
}
H64_VISIBLE EVP_PKEY *EVP_PKEY_new_raw_private_key_ex(void *lib, const char *nombre, const char *prop, const uint8_t *b, size_t n)
{
	(void)lib; (void)prop;
	return cruda(nombre, b, n, 1);
}
static size_t tam_ecdsa_der(size_t tam) { size_t c = 2 * (tam + 3); return c + (c < 128 ? 2 : 3); }
H64_VISIBLE int EVP_PKEY_get_size(const EVP_PKEY *k)
{
	if (!k) return 0;
	switch (k->tipo) {
	case K_RSA: return (int)h64bn_octetos(k->n);
	case K_EC: return (int)tam_ecdsa_der(k->tam);
	case K_ED25519: return 64;
	case K_X25519: return 32;
	}
	return 0;
}
H64_VISIBLE int EVP_PKEY_get_bits(const EVP_PKEY *k)
{
	if (!k) return 0;
	switch (k->tipo) {
	case K_RSA: return (int)h64bn_bits(k->n);
	case K_EC: return (int)h64_curva_bits(k->curva);
	case K_ED25519: return 256;
	case K_X25519: return 253;
	}
	return 0;
}

/* ---- las comprobaciones (como OpenSSL 3: rsa_sp800_56b_check.c) ---- */
static int rsa_publica_bien(const EVP_PKEY *k)
{
	if (!k->n || !k->e || h64bn_bits(k->n) > 16384 || !h64bn_es_impar(k->n)) return 0;
	if (!h64bn_es_impar(k->e) || h64bn_bits(k->e) < 2) return 0;   /* e impar y mayor que 1 */
	if (h64bn_factor_pequeno(k->n)) return 0;
	/* compuesto: n no es primo */
	return !h64bn_primo(k->n, 3);
}
static int rsa_privada_bien(const EVP_PKEY *k)
{
	return k->d && h64bn_bits(k->d) > 1 && h64bn_cmp(k->d, k->n) < 0;
}
static int primo_de_rsa(const BIGNUM *p, const BIGNUM *e, size_t nbits)
{
	BIGNUM *pp = h64bn_nuevo(), *p1 = h64bn_nuevo(), *g = h64bn_nuevo();
	int ok = pp && p1 && g && h64bn_bits(p) == nbits / 2 && h64bn_mul(pp, p, p) && h64bn_bits(pp) >= nbits
		&& h64bn_primo(p, 5) && h64bn_resta1(p1, p) && h64bn_mcd(g, p1, e) && h64bn_es_uno(g);
	h64bn_libera(pp); h64bn_libera(p1); h64bn_libera(g);
	return ok;
}
static int rsa_pareja_bien(const EVP_PKEY *k)
{
	if (!k->crt || !k->d) return 0;
	size_t nb = h64bn_bits(k->n);
	BIGNUM *t = h64bn_nuevo(), *p1 = h64bn_nuevo(), *q1 = h64bn_nuevo(), *m = h64bn_nuevo(), *g = h64bn_nuevo(), *r = h64bn_nuevo();
	int ok = t && p1 && q1 && m && g && r && h64bn_es_impar(k->e) && h64bn_bits(k->e) >= 2
		&& h64bn_mul(t, k->p, k->q) && !h64bn_cmp(t, k->n)
		&& primo_de_rsa(k->p, k->e, nb) && primo_de_rsa(k->q, k->e, nb);
	/* |p - q| > 2^(nbits/2 - 100) */
	if (ok) {
		ok = h64bn_cmp(k->p, k->q) >= 0 ? h64bn_resta(t, k->p, k->q) : h64bn_resta(t, k->q, k->p);
		ok = ok && nb / 2 >= 100 ? h64bn_bits(t) > nb / 2 - 100 : ok;
	}
	/* 2^(nbits/2) < d < mcm(p-1, q-1) y e·d = 1 (mod mcm) */
	ok = ok && h64bn_bits(k->d) > nb / 2 && h64bn_resta1(p1, k->p) && h64bn_resta1(q1, k->q) && h64bn_mul(m, p1, q1)
		&& h64bn_mcd(g, p1, q1) && h64bn_divmod(m, r, m, g) && h64bn_cmp(k->d, m) < 0
		&& h64bn_modmul(t, k->d, k->e, m) && h64bn_es_uno(t);
	/* el CRT: 1 < dP < p-1, 1 < dQ < q-1, 1 < qInv < p; dP·e = 1 (mod p-1); dQ·e = 1 (mod q-1); qInv·q = 1 (mod p) */
	ok = ok && h64bn_bits(k->dp) > 1 && h64bn_cmp(k->dp, p1) < 0 && h64bn_bits(k->dq) > 1 && h64bn_cmp(k->dq, q1) < 0
		&& h64bn_bits(k->qi) > 1 && h64bn_cmp(k->qi, k->p) < 0
		&& h64bn_modmul(t, k->dp, k->e, p1) && h64bn_es_uno(t) && h64bn_modmul(t, k->dq, k->e, q1) && h64bn_es_uno(t)
		&& h64bn_modmul(t, k->qi, k->q, k->p) && h64bn_es_uno(t);
	h64bn_libera(t); h64bn_libera(p1); h64bn_libera(q1); h64bn_libera(m); h64bn_libera(g); h64bn_libera(r);
	return ok;
}
H64_VISIBLE int EVP_PKEY_public_check(EVP_PKEY_CTX *c)
{
	if (!c || !c->k) return 0;
	EVP_PKEY *k = c->k;
	switch (k->tipo) {
	case K_RSA: return rsa_publica_bien(k);
	case K_EC: return k->publen == 1 + 2 * k->tam;
	case K_ED25519: case K_X25519: return 1;
	}
	return 0;
}
H64_VISIBLE int EVP_PKEY_private_check(EVP_PKEY_CTX *c)
{
	if (!c || !c->k || !c->k->privada) return 0;
	if (c->k->tipo == K_RSA) return rsa_privada_bien(c->k);
	return 1;
}
H64_VISIBLE int EVP_PKEY_pairwise_check(EVP_PKEY_CTX *c)
{
	if (!c || !c->k || !c->k->privada) return 0;
	EVP_PKEY *k = c->k;
	if (k->tipo == K_RSA) return rsa_pareja_bien(k);
	if (k->tipo == K_EC) {
		uint8_t pt[133];
		size_t n = ec()->mulgen(pt, k->dd, k->tam, k->curva);
		return n == k->publen && !memcmp(pt, k->pub, n);
	}
	return 1;
}

/* ---- RSA: las operaciones a pelo ---- */
static int rsa_publica(const EVP_PKEY *k, const uint8_t *en, uint8_t *sal)
{
	size_t kn = h64bn_octetos(k->n);
	BIGNUM *x = h64bn_nuevo(), *y = h64bn_nuevo();
	int ok = x && y && h64bn_de_octetos(x, en, kn) && h64bn_cmp(x, k->n) < 0;
	if (ok && h64bn_bits(k->n) <= 4096 && h64bn_octetos(k->e) <= 512) {
		uint8_t nb[512], eb[512];
		size_t en_ = h64bn_octetos(k->e);
		h64bn_a_octetos(k->n, nb, kn);
		h64bn_a_octetos(k->e, eb, en_);
		br_rsa_public_key pk = { nb, kn, eb, en_ };
		memmove(sal, en, kn);
		ok = br_rsa_public_get_default()(sal, kn, &pk) == 1;
	} else if (ok) {
		ok = h64bn_modexp(y, x, k->e, k->n) && h64bn_a_octetos(y, sal, kn);
	}
	h64bn_libera(x); h64bn_libera(y);
	return ok;
}
static int rsa_privada(const EVP_PKEY *k, const uint8_t *en, uint8_t *sal)
{
	size_t kn = h64bn_octetos(k->n);
	BIGNUM *x = h64bn_nuevo(), *y = h64bn_nuevo();
	int ok = x && y && h64bn_de_octetos(x, en, kn) && h64bn_cmp(x, k->n) < 0;
	if (ok && (k->crt || k->crt_dentro) && h64bn_bits(k->n) <= 4096) {
		/* BearSSL, con el CRT y de tiempo constante */
		uint8_t b[5][512];
		const BIGNUM *v[5] = { k->p, k->q, k->dp, k->dq, k->qi };
		size_t l[5];
		for (int i = 0; i < 5; i++) { l[i] = h64bn_octetos(v[i]); h64bn_a_octetos(v[i], b[i], l[i]); }
		br_rsa_private_key sk = { (uint32_t)h64bn_bits(k->n), b[0], l[0], b[1], l[1], b[2], l[2], b[3], l[3], b[4], l[4] };
		memmove(sal, en, kn);
		ok = br_rsa_private_get_default()(sal, &sk) == 1;
		h64_borra(b, sizeof b);
		if (ok) {
			/* y, comprobado (que un fallo no deje ver la clave) */
			uint8_t *vuelta = malloc(kn);
			ok = vuelta && rsa_publica(k, sal, vuelta) && h64_iguales_ct(vuelta, en, kn);
			free(vuelta);
		}
	} else if (ok) {
		ok = h64bn_modexp(y, x, k->d, k->n) && h64bn_a_octetos(y, sal, kn);
	}
	h64bn_libera(x); h64bn_libera(y);
	if (!ok) h64_borra(sal, kn);
	return ok;
}

/* EMSA-PKCS1-v1_5 (RFC 8017, 9.2) */
static int emsa_pkcs1(int t, const uint8_t *h, uint8_t *em, size_t k)
{
	size_t din, hl = h64_resumen_tam(t);
	const uint8_t *di = h64_resumen_digestinfo(t, &din);
	if (!di || k < din + hl + 11) return 0;
	em[0] = 0; em[1] = 1;
	memset(em + 2, 0xFF, k - din - hl - 3);
	em[k - din - hl - 1] = 0;
	memcpy(em + k - din - hl, di, din);
	memcpy(em + k - hl, h, hl);
	return 1;
}
/* EMSA-PSS (RFC 8017, 9.1) */
static int emsa_pss(int t, int mgf, const uint8_t *mh, size_t sl, uint8_t *em, size_t embits)
{
	size_t hl = h64_resumen_tam(t), emlen = (embits + 7) / 8;
	if (emlen < hl + sl + 2 || sl > 1024) return 0;
	uint8_t sal[1024], hh[64];
	if (!h64_azar(sal, sl)) return 0;
	static const uint8_t ceros[8];
	h64_resumen r;
	h64_resumen_ini(&r, t);
	h64_resumen_mas(&r, ceros, 8);
	h64_resumen_mas(&r, mh, hl);
	h64_resumen_mas(&r, sal, sl);
	h64_resumen_fin(&r, hh);
	size_t dbl = emlen - hl - 1;
	memset(em, 0, dbl);
	em[dbl - sl - 1] = 1;
	memcpy(em + dbl - sl, sal, sl);
	h64_mgf1_xor(mgf, em, dbl, hh, hl);
	em[0] &= (uint8_t)(0xFF >> (8 * emlen - embits));
	memcpy(em + dbl, hh, hl);
	em[emlen - 1] = 0xbc;
	return 1;
}
static int emsa_pss_comprueba(int t, int mgf, const uint8_t *mh, long sl, uint8_t *em, size_t embits)
{
	size_t hl = h64_resumen_tam(t), emlen = (embits + 7) / 8;
	if (sl < 0 || emlen < hl + (size_t)sl + 2 || em[emlen - 1] != 0xbc) return 0;
	size_t dbl = emlen - hl - 1;
	if (em[0] & (uint8_t)~(0xFF >> (8 * emlen - embits))) return 0;
	uint8_t *h = em + dbl;
	h64_mgf1_xor(mgf, em, dbl, h, hl);
	em[0] &= (uint8_t)(0xFF >> (8 * emlen - embits));
	for (size_t i = 0; i < dbl - (size_t)sl - 1; i++) if (em[i]) return 0;
	if (em[dbl - sl - 1] != 1) return 0;
	uint8_t hh[64];
	static const uint8_t ceros[8];
	h64_resumen r;
	h64_resumen_ini(&r, t);
	h64_resumen_mas(&r, ceros, 8);
	h64_resumen_mas(&r, mh, hl);
	h64_resumen_mas(&r, em + dbl - sl, (size_t)sl);
	h64_resumen_fin(&r, hh);
	return h64_iguales_ct(hh, h, hl);
}

/* ---- firmar y comprobar con EVP_DigestSign y EVP_DigestVerify ---- */
void h64_mdctx_suelta(EVP_MD_CTX *c)
{
	if (c->k) { EVP_PKEY_free(c->k); c->k = NULL; }
	c->modo = MD_NADA;
}
static int firma_init(EVP_MD_CTX *c, const char *resumen, EVP_PKEY *k, const OSSL_PARAM *p, int modo)
{
	if (!c || !k) return 0;
	h64_mdctx_suelta(c);
	int t = resumen ? h64_resumen_tipo(resumen) : R_NADA;
	if (k->tipo == K_RSA) {
		if (!t || h64_resumen_es_xof(t)) return 0;
		size_t n;
		const char *pm = h64_param_texto(p, "pad-mode", &n);
		c->relleno = pm && !strcasecmp(pm, "pss") ? 2 : 1;
		const char *mg = h64_param_texto(p, "mgf1-digest", &n);
		c->mgf1 = mg ? h64_resumen_tipo(mg) : t;
		int64_t sl;
		c->sal = h64_param_int(p, "saltlen", &sl) ? (long)sl : -1;
		if (c->relleno == 2 && c->sal == -1) c->sal = (long)h64_resumen_tam(t);
		if (c->relleno == 2 && (c->sal < 0 || !c->mgf1 || h64_resumen_es_xof(c->mgf1))) return 0;
		if (modo == MD_FIRMA && !k->privada) return 0;
	} else if (k->tipo == K_ED25519) {
		if (t) return 0;
		if (modo == MD_FIRMA && !k->privada) return 0;
	} else return 0;
	c->resumen = t;
	EVP_PKEY_up_ref(k);
	c->k = k;
	c->modo = modo;
	return 1;
}
H64_VISIBLE int EVP_DigestSignInit_ex(EVP_MD_CTX *c, EVP_PKEY_CTX **pc, const char *resumen, void *lib, const char *prop, EVP_PKEY *k, const OSSL_PARAM *p)
{
	(void)lib; (void)prop;
	if (pc) *pc = NULL;
	return firma_init(c, resumen, k, p, MD_FIRMA);
}
H64_VISIBLE int EVP_DigestVerifyInit_ex(EVP_MD_CTX *c, EVP_PKEY_CTX **pc, const char *resumen, void *lib, const char *prop, EVP_PKEY *k, const OSSL_PARAM *p)
{
	(void)lib; (void)prop;
	if (pc) *pc = NULL;
	return firma_init(c, resumen, k, p, MD_COMPRUEBA);
}
H64_VISIBLE int EVP_DigestSign(EVP_MD_CTX *c, uint8_t *firma, size_t *n, const uint8_t *d, size_t dn)
{
	if (!c || c->modo != MD_FIRMA || !n) return 0;
	EVP_PKEY *k = c->k;
	if (k->tipo == K_ED25519) {
		if (!firma) { *n = 64; return 1; }
		if (*n < 64) return 0;
		crypto_ed25519_sign(firma, k->edsk, d, dn);
		*n = 64;
		return 1;
	}
	size_t kn = h64bn_octetos(k->n);
	if (!firma) { *n = kn; return 1; }
	if (*n < kn) return 0;
	uint8_t h[64];
	h64_resumen_de(c->resumen, d, dn, h);
	uint8_t *em = calloc(1, kn);
	if (!em) return 0;
	int ok;
	if (c->relleno == 1) ok = emsa_pkcs1(c->resumen, h, em, kn);
	else {
		size_t embits = h64bn_bits(k->n) - 1, emlen = (embits + 7) / 8;
		ok = emsa_pss(c->resumen, c->mgf1, h, (size_t)c->sal, em + (kn - emlen), embits);
	}
	ok = ok && rsa_privada(k, em, firma);
	h64_borra(em, kn);
	free(em);
	if (ok) *n = kn;
	return ok;
}
H64_VISIBLE int EVP_DigestVerify(EVP_MD_CTX *c, const uint8_t *firma, size_t fn, const uint8_t *d, size_t dn)
{
	if (!c || c->modo != MD_COMPRUEBA) return -1;
	EVP_PKEY *k = c->k;
	if (k->tipo == K_ED25519) {
		if (fn != 64) return 0;
		return crypto_ed25519_check(firma, k->crudap, d, dn) == 0 ? 1 : 0;
	}
	size_t kn = h64bn_octetos(k->n);
	if (fn != kn) return 0;
	uint8_t h[64];
	h64_resumen_de(c->resumen, d, dn, h);
	uint8_t *em = malloc(kn), *esp = malloc(kn);
	int ok = em && esp && rsa_publica(k, firma, em);
	if (ok) {
		if (c->relleno == 1) ok = emsa_pkcs1(c->resumen, h, esp, kn) && h64_iguales_ct(em, esp, kn);
		else {
			size_t embits = h64bn_bits(k->n) - 1, emlen = (embits + 7) / 8;
			ok = (kn == emlen || em[0] == 0) && emsa_pss_comprueba(c->resumen, c->mgf1, h, c->sal, em + (kn - emlen), embits);
		}
	}
	free(em);
	free(esp);
	return ok ? 1 : 0;
}

/* ---- EVP_PKEY_sign y EVP_PKEY_verify: ECDSA sobre el resumen (la firma, en DER) ---- */
H64_VISIBLE int EVP_PKEY_sign_init(EVP_PKEY_CTX *c)
{
	if (!c || !c->k || c->k->tipo != K_EC || !c->k->privada) return 0;
	c->op = OP_FIRMA;
	return 1;
}
H64_VISIBLE int EVP_PKEY_verify_init(EVP_PKEY_CTX *c)
{
	if (!c || !c->k || c->k->tipo != K_EC) return 0;
	c->op = OP_COMPRUEBA;
	return 1;
}
static const br_hash_class *hash_por_tam(size_t n)
{
	switch (n) {
	case 20: return &br_sha1_vtable;
	case 28: return &br_sha224_vtable;
	case 32: return &br_sha256_vtable;
	case 48: return &br_sha384_vtable;
	case 64: return &br_sha512_vtable;
	}
	return NULL;
}
size_t h64_ecdsa_a_der(const uint8_t *rs, size_t tam, uint8_t *sal);
int h64_ecdsa_de_der(const uint8_t *der, size_t n, uint8_t *rs, size_t tam);
H64_VISIBLE int EVP_PKEY_sign(EVP_PKEY_CTX *c, uint8_t *firma, size_t *n, const uint8_t *h, size_t hn)
{
	if (!c || c->op != OP_FIRMA || !n) return 0;
	EVP_PKEY *k = c->k;
	size_t max = tam_ecdsa_der(k->tam);
	if (!firma) { *n = max; return 1; }
	const br_hash_class *hf = hash_por_tam(hn);
	if (!hf || *n < max) return 0;
	br_ec_private_key sk = { k->curva, k->dd, k->tam };
	uint8_t rs[132];
	size_t l = br_ecdsa_i31_sign_raw(ec(), hf, h, &sk, rs);
	if (l != 2 * k->tam) return 0;
	*n = h64_ecdsa_a_der(rs, k->tam, firma);
	return *n > 0;
}
H64_VISIBLE int EVP_PKEY_verify(EVP_PKEY_CTX *c, const uint8_t *firma, size_t fn, const uint8_t *h, size_t hn)
{
	if (!c || c->op != OP_COMPRUEBA) return -1;
	EVP_PKEY *k = c->k;
	uint8_t rs[132];
	if (!h64_ecdsa_de_der(firma, fn, rs, k->tam)) return 0;
	br_ec_public_key pk = { k->curva, k->pub, k->publen };
	return br_ecdsa_i31_vrfy_raw(ec(), h, hn, &pk, rs, 2 * k->tam) ? 1 : 0;
}
/* lo de ML-DSA (firmar un mensaje con un algoritmo): no hay */
typedef struct evp_signature_st { int nada; } EVP_SIGNATURE;
H64_VISIBLE EVP_SIGNATURE *EVP_SIGNATURE_fetch(void *lib, const char *nombre, const char *prop) { (void)lib; (void)nombre; (void)prop; return NULL; }
H64_VISIBLE void EVP_SIGNATURE_free(EVP_SIGNATURE *s) { (void)s; }
H64_VISIBLE int EVP_PKEY_sign_message_init(EVP_PKEY_CTX *c, EVP_SIGNATURE *s, const OSSL_PARAM *p) { (void)c; (void)s; (void)p; return 0; }
H64_VISIBLE int EVP_PKEY_verify_message_init(EVP_PKEY_CTX *c, EVP_SIGNATURE *s, const OSSL_PARAM *p) { (void)c; (void)s; (void)p; return 0; }
/* ML-KEM: no hay */
H64_VISIBLE int EVP_PKEY_encapsulate_init(EVP_PKEY_CTX *c, const OSSL_PARAM *p) { (void)c; (void)p; return 0; }
H64_VISIBLE int EVP_PKEY_encapsulate(EVP_PKEY_CTX *c, uint8_t *a, size_t *an, uint8_t *b, size_t *bn) { (void)c; (void)a; (void)an; (void)b; (void)bn; return 0; }
H64_VISIBLE int EVP_PKEY_decapsulate_init(EVP_PKEY_CTX *c, const OSSL_PARAM *p) { (void)c; (void)p; return 0; }
H64_VISIBLE int EVP_PKEY_decapsulate(EVP_PKEY_CTX *c, uint8_t *a, size_t *an, const uint8_t *b, size_t bn) { (void)c; (void)a; (void)an; (void)b; (void)bn; return 0; }

/* ---- RSA-OAEP (RFC 8017, 7.1) ---- */
static int oaep_params(EVP_PKEY_CTX *c, const OSSL_PARAM *p)
{
	if (!c || !c->k || c->k->tipo != K_RSA) return 0;
	size_t n;
	const char *pm = h64_param_texto(p, "pad-mode", &n);
	if (!pm || strcasecmp(pm, "oaep")) return 0;
	const char *d = h64_param_texto(p, "digest", &n);
	c->resumen = d ? h64_resumen_tipo(d) : R_SHA1;
	const char *m = h64_param_texto(p, "mgf1-digest", &n);
	c->mgf1 = m ? h64_resumen_tipo(m) : c->resumen;
	if (!c->resumen || !c->mgf1 || h64_resumen_es_xof(c->resumen) || h64_resumen_es_xof(c->mgf1)) return 0;
	if (c->etiq) { h64_borra(c->etiq, c->etiq_n); free(c->etiq); c->etiq = NULL; c->etiq_n = 0; }
	const uint8_t *e = h64_param_octetos(p, "oaep-label", &n);
	if (e && n) {
		c->etiq = malloc(n);
		if (!c->etiq) return 0;
		memcpy(c->etiq, e, n);
		c->etiq_n = n;
	}
	return 1;
}
H64_VISIBLE int EVP_PKEY_encrypt_init_ex(EVP_PKEY_CTX *c, const OSSL_PARAM *p)
{
	if (!oaep_params(c, p)) return 0;
	c->op = OP_CIFRA;
	return 1;
}
H64_VISIBLE int EVP_PKEY_decrypt_init_ex(EVP_PKEY_CTX *c, const OSSL_PARAM *p)
{
	if (!oaep_params(c, p) || !c->k->privada) return 0;
	c->op = OP_DESCIFRA;
	return 1;
}
H64_VISIBLE int EVP_PKEY_encrypt(EVP_PKEY_CTX *c, uint8_t *sal, size_t *saln, const uint8_t *m, size_t mn)
{
	if (!c || c->op != OP_CIFRA || !saln) return 0;
	EVP_PKEY *k = c->k;
	size_t kn = h64bn_octetos(k->n), hl = h64_resumen_tam(c->resumen);
	if (!sal) { *saln = kn; return 1; }
	if (*saln < kn || kn < 2 * hl + 2 || mn > kn - 2 * hl - 2) return 0;
	uint8_t *em = calloc(1, kn);
	if (!em) return 0;
	uint8_t *semilla = em + 1, *db = em + 1 + hl;
	size_t dbl = kn - hl - 1;
	h64_resumen_de(c->resumen, c->etiq ? c->etiq : (const uint8_t *)"", c->etiq_n, db);
	db[dbl - mn - 1] = 1;
	memcpy(db + dbl - mn, m, mn);
	int ok = h64_azar(semilla, hl);
	if (ok) {
		h64_mgf1_xor(c->mgf1, db, dbl, semilla, hl);
		h64_mgf1_xor(c->mgf1, semilla, hl, db, dbl);
		ok = rsa_publica(k, em, sal);
	}
	h64_borra(em, kn);
	free(em);
	if (ok) *saln = kn;
	return ok;
}
H64_VISIBLE int EVP_PKEY_decrypt(EVP_PKEY_CTX *c, uint8_t *sal, size_t *saln, const uint8_t *en, size_t enn)
{
	if (!c || c->op != OP_DESCIFRA || !saln) return 0;
	EVP_PKEY *k = c->k;
	size_t kn = h64bn_octetos(k->n), hl = h64_resumen_tam(c->resumen);
	if (!sal) { *saln = kn; return 1; }
	if (enn != kn || kn < 2 * hl + 2) return 0;
	uint8_t *em = malloc(kn);
	if (!em) return 0;
	int ok = rsa_privada(k, en, em);
	size_t lon = 0;
	if (ok) {
		uint8_t *semilla = em + 1, *db = em + 1 + hl, lh[64];
		size_t dbl = kn - hl - 1;
		h64_mgf1_xor(c->mgf1, semilla, hl, db, dbl);
		h64_mgf1_xor(c->mgf1, db, dbl, semilla, hl);
		h64_resumen_de(c->resumen, c->etiq ? c->etiq : (const uint8_t *)"", c->etiq_n, lh);
		/* sin ramas que dependan de lo descifrado hasta el final */
		unsigned malo = em[0] | (unsigned)!h64_iguales_ct(lh, db, hl);
		unsigned visto = 0, pos = 0;
		for (size_t i = hl; i < dbl; i++) {
			unsigned es1 = db[i] == 1, es0 = db[i] == 0;
			unsigned primero = es1 & !visto;
			pos |= primero ? (unsigned)i : 0;
			malo |= !visto & !es0 & !es1;
			visto |= es1;
		}
		malo |= !visto;
		if (malo) ok = 0;
		else {
			lon = dbl - pos - 1;
			if (*saln < lon) ok = 0;
			else memcpy(sal, db + pos + 1, lon);
		}
	}
	h64_borra(em, kn);
	free(em);
	if (ok) *saln = lon;
	return ok;
}

/* ---- derivar: ECDH y X25519 ---- */
H64_VISIBLE int EVP_PKEY_derive_init(EVP_PKEY_CTX *c)
{
	if (!c || !c->k || !c->k->privada || (c->k->tipo != K_EC && c->k->tipo != K_X25519)) return 0;
	c->op = OP_DERIVA;
	return 1;
}
H64_VISIBLE int EVP_PKEY_derive_set_peer_ex(EVP_PKEY_CTX *c, EVP_PKEY *par, int valida)
{
	(void)valida;
	if (!c || c->op != OP_DERIVA || !par || par->tipo != c->k->tipo) return 0;
	if (par->tipo == K_EC && (par->curva != c->k->curva || par->publen != 1 + 2 * par->tam)) return 0;
	EVP_PKEY_up_ref(par);
	EVP_PKEY_free(c->par);
	c->par = par;
	return 1;
}
H64_VISIBLE int EVP_PKEY_derive(EVP_PKEY_CTX *c, uint8_t *sal, size_t *n)
{
	if (!c || c->op != OP_DERIVA || !c->par || !n) return 0;
	EVP_PKEY *k = c->k;
	size_t tam = k->tipo == K_EC ? k->tam : 32;
	if (!sal) { *n = tam; return 1; }
	if (*n < tam) return 0;
	if (k->tipo == K_EC) {
		uint8_t g[133];
		memcpy(g, c->par->pub, c->par->publen);
		if (!ec()->mul(g, c->par->publen, k->dd, k->tam, k->curva)) return 0;
		memcpy(sal, g + 1, tam);
		h64_borra(g, sizeof g);
	} else {
		crypto_x25519(sal, k->cruda, c->par->crudap);
		static const uint8_t ceros[32];
		if (h64_iguales_ct(sal, ceros, 32)) { h64_borra(sal, 32); return 0; }   /* un punto de orden pequeño */
	}
	*n = tam;
	return 1;
}

/* ---- EC_GROUP, EC_POINT y ECDSA_SIG ---- */
typedef struct ec_group_st { int curva; BIGNUM *orden; } EC_GROUP;
typedef struct ec_point_st { uint8_t p[133]; size_t n; } EC_POINT;
typedef struct ecdsa_sig_st { BIGNUM *r, *s; } ECDSA_SIG;
H64_VISIBLE EC_GROUP *EC_GROUP_new_by_curve_name(int nid)
{
	int c = h64_curva_por_nid(nid);
	if (!c) return NULL;
	EC_GROUP *g = calloc(1, sizeof *g);
	if (!g) return NULL;
	g->curva = c;
	g->orden = h64bn_nuevo();
	if (!g->orden || !orden(c, g->orden)) { h64bn_libera(g->orden); free(g); return NULL; }
	return g;
}
H64_VISIBLE void EC_GROUP_free(EC_GROUP *g)
{
	if (!g) return;
	h64bn_libera(g->orden);
	free(g);
}
H64_VISIBLE const BIGNUM *EC_GROUP_get0_order(const EC_GROUP *g) { return g ? g->orden : NULL; }
H64_VISIBLE EC_POINT *EC_POINT_new(const EC_GROUP *g) { return g ? calloc(1, sizeof(EC_POINT)) : NULL; }
H64_VISIBLE void EC_POINT_free(EC_POINT *p) { free(p); }
H64_VISIBLE int EC_POINT_mul(const EC_GROUP *g, EC_POINT *r, const BIGNUM *n, const EC_POINT *q, const BIGNUM *m, void *ctx)
{
	(void)ctx;
	if (!g || !r || !n || q || m) return 0;
	size_t tam = h64_curva_tam(g->curva);
	uint8_t x[66];
	if (h64bn_es_cero(n) || h64bn_cmp(n, g->orden) >= 0 || !h64bn_a_octetos(n, x, tam)) return 0;
	r->n = ec()->mulgen(r->p, x, tam, g->curva);
	h64_borra(x, sizeof x);
	return r->n == 1 + 2 * tam;
}
H64_VISIBLE size_t EC_POINT_point2oct(const EC_GROUP *g, const EC_POINT *p, int forma, uint8_t *b, size_t n, void *ctx)
{
	(void)ctx;
	if (!g || !p || forma != 4 || !p->n) return 0;
	if (!b) return p->n;
	if (n < p->n) return 0;
	memcpy(b, p->p, p->n);
	return p->n;
}
H64_VISIBLE ECDSA_SIG *ECDSA_SIG_new(void) { return calloc(1, sizeof(ECDSA_SIG)); }
H64_VISIBLE void ECDSA_SIG_free(ECDSA_SIG *s)
{
	if (!s) return;
	h64bn_libera(s->r);
	h64bn_libera(s->s);
	free(s);
}
H64_VISIBLE int ECDSA_SIG_set0(ECDSA_SIG *s, BIGNUM *r, BIGNUM *ss)
{
	if (!s || !r || !ss) return 0;
	h64bn_libera(s->r);
	h64bn_libera(s->s);
	s->r = r;
	s->s = ss;
	return 1;
}
H64_VISIBLE void ECDSA_SIG_get0(const ECDSA_SIG *s, const BIGNUM **r, const BIGNUM **ss)
{
	if (r) *r = s ? s->r : NULL;
	if (ss) *ss = s ? s->s : NULL;
}
