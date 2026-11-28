/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/der.c - Hispa-64, etapa 10 i: el DER de la WebCrypto, lo
 * justo: SubjectPublicKeyInfo (RFC 5280, 4.1), PrivateKeyInfo (PKCS #8,
 * RFC 5208 y 5958), RSAPublicKey y RSAPrivateKey (RFC 8017, A.1),
 * ECPrivateKey (RFC 5915), CurvePrivateKey (RFC 8410) y Ecdsa-Sig-Value
 * (RFC 3279), con las funciones d2i_ e i2d_ de OpenSSL que pide el Rust.
 * Como OpenSSL, se lee también lo que no es DER del todo (longitudes largas
 * de más, ceros de más delante de un entero): el Rust lo vuelve a escribir y
 * compara, y así dice «no es DER» donde lo diría con OpenSSL.
 */
#include <stdlib.h>
#include "interno.h"

/* ---- leer y escribir ---- */
int h64_der_lee(const uint8_t *p, size_t n, uint8_t *etiqueta, h64_trozo *c, size_t *ocupa)
{
	if (n < 2) return 0;
	uint8_t e = p[0];
	if ((e & 0x1F) == 0x1F) return 0;   /* (etiquetas largas: aquí no hacen falta) */
	size_t lon, cab;
	if (p[1] < 0x80) { lon = p[1]; cab = 2; }
	else {
		size_t k = p[1] & 0x7F;
		if (k == 0 || k > 4 || n < 2 + k) return 0;   /* (sin la longitud indefinida de BER) */
		lon = 0;
		for (size_t i = 0; i < k; i++) lon = lon << 8 | p[2 + i];
		cab = 2 + k;
	}
	if (lon > n - cab) return 0;
	*etiqueta = e;
	c->p = p + cab;
	c->n = lon;
	*ocupa = cab + lon;
	return 1;
}
size_t h64_der_cabecera(uint8_t *b, uint8_t e, size_t n)
{
	uint8_t t[6];
	size_t l = 0;
	t[l++] = e;
	if (n < 0x80) t[l++] = (uint8_t)n;
	else {
		size_t k = 0;
		for (size_t v = n; v; v >>= 8) k++;
		t[l++] = (uint8_t)(0x80 | k);
		for (size_t i = k; i-- > 0;) t[l++] = (uint8_t)(n >> (8 * i));
	}
	if (b) memcpy(b, t, l);
	return l;
}
void h64_buf_mete(h64_buf *s, const void *d, size_t n)
{
	if (s->mal) return;
	if (s->n + n > s->cap) {
		size_t cap = (s->n + n) * 2 + 64;
		uint8_t *b = malloc(cap);
		if (!b) { s->mal = 1; return; }
		if (s->b) { memcpy(b, s->b, s->n); h64_borra(s->b, s->cap); free(s->b); }
		s->b = b;
		s->cap = cap;
	}
	if (n) memcpy(s->b + s->n, d, n);
	s->n += n;
}
void h64_buf_cab(h64_buf *s, uint8_t e, size_t n)
{
	uint8_t t[8];
	h64_buf_mete(s, t, h64_der_cabecera(t, e, n));
}
void h64_buf_entero(h64_buf *s, const uint8_t *v, size_t n)
{
	while (n > 1 && !v[0]) { v++; n--; }
	if (!n) { static const uint8_t z[] = { 2, 1, 0 }; h64_buf_mete(s, z, 3); return; }
	int cero = v[0] & 0x80;
	h64_buf_cab(s, 0x02, n + (cero ? 1 : 0));
	if (cero) h64_buf_mete(s, "", 1);
	h64_buf_mete(s, v, n);
}
void h64_buf_entero_bn(h64_buf *s, const BIGNUM *a)
{
	size_t n = h64bn_octetos(a);
	uint8_t *b = malloc(n ? n : 1);
	if (!b) { s->mal = 1; return; }
	h64bn_a_octetos(a, b, n);
	h64_buf_entero(s, b, n);
	h64_borra(b, n);
	free(b);
}
void h64_buf_libera(h64_buf *s)
{
	if (s->b) { h64_borra(s->b, s->cap); free(s->b); }
	memset(s, 0, sizeof *s);
}
/* un elemento envuelto: etiqueta + lo de dentro (otro búfer) */
static void envuelve(h64_buf *s, uint8_t e, h64_buf *dentro)
{
	if (dentro->mal) s->mal = 1;
	h64_buf_cab(s, e, dentro->n);
	h64_buf_mete(s, dentro->b, dentro->n);
	h64_buf_libera(dentro);
}
/* lo de i2d_: sin destino, la longitud; con *pp nulo, lo reserva; si no, escribe y avanza */
static int i2d_sal(h64_buf *s, uint8_t **pp)
{
	if (s->mal || s->n > 0x7FFFFFFF) { h64_buf_libera(s); return -1; }
	int n = (int)s->n;
	if (pp) {
		if (!*pp) {
			*pp = malloc(s->n ? s->n : 1);
			if (!*pp) { h64_buf_libera(s); return -1; }
			memcpy(*pp, s->b, s->n);
		} else {
			memcpy(*pp, s->b, s->n);
			*pp += s->n;
		}
	}
	h64_buf_libera(s);
	return n;
}
/* un INTEGER sin signo (lo que no es DER se deja: el Rust compara) */
static int lee_entero(const uint8_t **p, size_t *n, BIGNUM **r)
{
	uint8_t e;
	h64_trozo c;
	size_t o;
	if (!h64_der_lee(*p, *n, &e, &c, &o) || e != 0x02 || !c.n || (c.p[0] & 0x80)) return 0;
	*r = h64bn_nuevo();
	if (!*r || !h64bn_de_octetos(*r, c.p, c.n)) { h64bn_libera(*r); *r = NULL; return 0; }
	*p += o;
	*n -= o;
	return 1;
}

/* ---- Ecdsa-Sig-Value ---- */
typedef struct ecdsa_sig_st { BIGNUM *r, *s; } ECDSA_SIG;
size_t h64_ecdsa_a_der(const uint8_t *rs, size_t tam, uint8_t *sal)
{
	h64_buf s = { 0 }, d = { 0 };
	h64_buf_entero(&d, rs, tam);
	h64_buf_entero(&d, rs + tam, tam);
	envuelve(&s, 0x30, &d);
	if (s.mal) { h64_buf_libera(&s); return 0; }
	size_t n = s.n;
	memcpy(sal, s.b, n);
	h64_buf_libera(&s);
	return n;
}
int h64_ecdsa_de_der(const uint8_t *der, size_t n, uint8_t *rs, size_t tam)
{
	uint8_t e;
	h64_trozo c;
	size_t o;
	if (!h64_der_lee(der, n, &e, &c, &o) || e != 0x30 || o != n) return 0;
	const uint8_t *p = c.p;
	size_t q = c.n;
	BIGNUM *r = NULL, *s = NULL;
	int ok = lee_entero(&p, &q, &r) && lee_entero(&p, &q, &s) && q == 0
		&& h64bn_a_octetos(r, rs, tam) && h64bn_a_octetos(s, rs + tam, tam);
	h64bn_libera(r);
	h64bn_libera(s);
	return ok;
}
H64_VISIBLE ECDSA_SIG *d2i_ECDSA_SIG(ECDSA_SIG **a, const uint8_t **pp, long n)
{
	uint8_t e;
	h64_trozo c;
	size_t o;
	if (!pp || !*pp || n < 0 || !h64_der_lee(*pp, (size_t)n, &e, &c, &o) || e != 0x30) return NULL;
	const uint8_t *p = c.p;
	size_t q = c.n;
	ECDSA_SIG *s = calloc(1, sizeof *s);
	if (!s) return NULL;
	if (!lee_entero(&p, &q, &s->r) || !lee_entero(&p, &q, &s->s) || q) {
		h64bn_libera(s->r); h64bn_libera(s->s); free(s);
		return NULL;
	}
	*pp += o;
	if (a) {
		if (*a) { h64bn_libera((*a)->r); h64bn_libera((*a)->s); free(*a); }
		*a = s;
	}
	return s;
}
H64_VISIBLE int i2d_ECDSA_SIG(const ECDSA_SIG *sig, uint8_t **pp)
{
	if (!sig || !sig->r || !sig->s) return -1;
	h64_buf s = { 0 }, d = { 0 };
	h64_buf_entero_bn(&d, sig->r);
	h64_buf_entero_bn(&d, sig->s);
	envuelve(&s, 0x30, &d);
	return i2d_sal(&s, pp);
}

/* ---- AlgorithmIdentifier y los OID ---- */
typedef struct asn1_object_st { uint8_t d[64]; size_t n; } ASN1_OBJECT;
typedef struct X509_algor_st {
	ASN1_OBJECT oid;
	int ptipo;            /* V_ASN1_UNDEF (-1), V_ASN1_NULL (5), V_ASN1_OBJECT (6) u otra etiqueta */
	ASN1_OBJECT poid;
	uint8_t *praw;        /* los parámetros, tal cual (el elemento entero) */
	size_t prawn;
} X509_ALGOR;
#define V_ASN1_UNDEF -1
#define V_ASN1_NULL 5
#define V_ASN1_OBJECT 6
static const uint8_t OID_RSA[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01 };
static const uint8_t OID_EC[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
static const uint8_t OID_X25519[] = { 0x2b, 0x65, 0x6e };
static const uint8_t OID_ED25519[] = { 0x2b, 0x65, 0x70 };
static int oid_es(const ASN1_OBJECT *o, const uint8_t *v, size_t n) { return o->n == n && !memcmp(o->d, v, n); }
H64_VISIBLE size_t OBJ_length(const ASN1_OBJECT *o) { return o ? o->n : 0; }
H64_VISIBLE const uint8_t *OBJ_get0_data(const ASN1_OBJECT *o) { return o ? o->d : NULL; }
H64_VISIBLE void X509_ALGOR_get0(const ASN1_OBJECT **o, int *ptipo, const void **pval, const X509_ALGOR *a)
{
	if (o) *o = &a->oid;
	if (ptipo) *ptipo = a->ptipo;
	if (pval) *pval = a->ptipo == V_ASN1_OBJECT ? (const void *)&a->poid : NULL;
}
static int lee_algor(const uint8_t *p, size_t n, X509_ALGOR *a, size_t *ocupa)
{
	uint8_t e;
	h64_trozo c, o;
	size_t oc, k;
	if (!h64_der_lee(p, n, &e, &c, ocupa) || e != 0x30) return 0;
	if (!h64_der_lee(c.p, c.n, &e, &o, &oc) || e != 0x06 || !o.n || o.n > sizeof a->oid.d) return 0;
	memcpy(a->oid.d, o.p, o.n);
	a->oid.n = o.n;
	a->ptipo = V_ASN1_UNDEF;
	if (oc < c.n) {
		const uint8_t *q = c.p + oc;
		size_t qn = c.n - oc;
		h64_trozo pc;
		if (!h64_der_lee(q, qn, &e, &pc, &k) || k != qn) return 0;
		a->ptipo = e & 0x1F;
		if (e == 0x05 && pc.n) return 0;
		if (e == 0x06) {
			if (!pc.n || pc.n > sizeof a->poid.d) return 0;
			memcpy(a->poid.d, pc.p, pc.n);
			a->poid.n = pc.n;
		}
		a->praw = malloc(k);
		if (!a->praw) return 0;
		/* (escrito otra vez en DER: la cabecera, la justa) */
		a->prawn = h64_der_cabecera(a->praw, e, pc.n);
		uint8_t *t = realloc(a->praw, a->prawn + pc.n);
		if (!t) return 0;
		a->praw = t;
		memcpy(a->praw + a->prawn, pc.p, pc.n);
		a->prawn += pc.n;
	}
	return 1;
}
static void mete_algor(h64_buf *s, const X509_ALGOR *a)
{
	h64_buf d = { 0 };
	h64_buf_cab(&d, 0x06, a->oid.n);
	h64_buf_mete(&d, a->oid.d, a->oid.n);
	if (a->ptipo != V_ASN1_UNDEF) h64_buf_mete(&d, a->praw, a->prawn);
	envuelve(s, 0x30, &d);
}
static void mete_algor_de(h64_buf *s, const uint8_t *oid, size_t on, int nulo, const uint8_t *curva, size_t cn)
{
	h64_buf d = { 0 };
	h64_buf_cab(&d, 0x06, on);
	h64_buf_mete(&d, oid, on);
	if (nulo) h64_buf_mete(&d, "\x05\x00", 2);
	if (curva) { h64_buf_cab(&d, 0x06, cn); h64_buf_mete(&d, curva, cn); }
	envuelve(s, 0x30, &d);
}

/* ---- RSAPublicKey y la clave de un SPKI ---- */
static void mete_rsa_publica(h64_buf *s, const EVP_PKEY *k)
{
	h64_buf d = { 0 };
	h64_buf_entero_bn(&d, k->n);
	h64_buf_entero_bn(&d, k->e);
	envuelve(s, 0x30, &d);
}
static void mete_spki(h64_buf *s, const EVP_PKEY *k)
{
	h64_buf d = { 0 }, bits = { 0 };
	h64_buf_mete(&bits, "", 1);
	if (k->tipo == K_RSA) {
		mete_algor_de(&d, OID_RSA, sizeof OID_RSA, 1, NULL, 0);
		mete_rsa_publica(&bits, k);
	} else if (k->tipo == K_EC) {
		size_t cn;
		const uint8_t *c = h64_curva_oid(k->curva, &cn);
		mete_algor_de(&d, OID_EC, sizeof OID_EC, 0, c, cn);
		if (k->comprimido) {
			uint8_t pt[67];
			pt[0] = (uint8_t)(0x02 | (k->pub[k->publen - 1] & 1));
			memcpy(pt + 1, k->pub + 1, k->tam);
			h64_buf_mete(&bits, pt, 1 + k->tam);
		} else h64_buf_mete(&bits, k->pub, k->publen);
	} else if (k->tipo == K_ED25519 || k->tipo == K_X25519) {
		mete_algor_de(&d, k->tipo == K_ED25519 ? OID_ED25519 : OID_X25519, 3, 0, NULL, 0);
		h64_buf_mete(&bits, k->crudap, 32);
	} else s->mal = 1;
	envuelve(&d, 0x03, &bits);
	envuelve(s, 0x30, &d);
}
H64_VISIBLE int i2d_PUBKEY(const EVP_PKEY *k, uint8_t **pp)
{
	if (!k) return -1;
	h64_buf s = { 0 };
	mete_spki(&s, k);
	return i2d_sal(&s, pp);
}

/* ---- X509_PUBKEY ---- */
typedef struct X509_pubkey_st {
	X509_ALGOR alg;
	uint8_t *clave;
	size_t claven;
	uint8_t sin_usar;
} X509_PUBKEY;
H64_VISIBLE void X509_PUBKEY_free(X509_PUBKEY *x)
{
	if (!x) return;
	free(x->alg.praw);
	free(x->clave);
	free(x);
}
H64_VISIBLE X509_PUBKEY *d2i_X509_PUBKEY(X509_PUBKEY **a, const uint8_t **pp, long n)
{
	uint8_t e;
	h64_trozo c, b;
	size_t o, ao, bo;
	if (!pp || !*pp || n < 0 || !h64_der_lee(*pp, (size_t)n, &e, &c, &o) || e != 0x30) return NULL;
	X509_PUBKEY *x = calloc(1, sizeof *x);
	if (!x) return NULL;
	if (!lee_algor(c.p, c.n, &x->alg, &ao) || !h64_der_lee(c.p + ao, c.n - ao, &e, &b, &bo) || e != 0x03
	    || ao + bo != c.n || !b.n || b.p[0] > 7) {
		X509_PUBKEY_free(x);
		return NULL;
	}
	x->sin_usar = b.p[0];
	x->claven = b.n - 1;
	x->clave = malloc(x->claven ? x->claven : 1);
	if (!x->clave) { X509_PUBKEY_free(x); return NULL; }
	memcpy(x->clave, b.p + 1, x->claven);
	*pp += o;
	if (a) { X509_PUBKEY_free(*a); *a = x; }
	return x;
}
H64_VISIBLE int i2d_X509_PUBKEY(const X509_PUBKEY *x, uint8_t **pp)
{
	if (!x) return -1;
	h64_buf s = { 0 }, d = { 0 }, bits = { 0 };
	mete_algor(&d, &x->alg);
	h64_buf_mete(&bits, &x->sin_usar, 1);
	h64_buf_mete(&bits, x->clave, x->claven);
	envuelve(&d, 0x03, &bits);
	envuelve(&s, 0x30, &d);
	return i2d_sal(&s, pp);
}
H64_VISIBLE int X509_PUBKEY_get0_param(ASN1_OBJECT **o, const uint8_t **k, int *kn, X509_ALGOR **alg, const X509_PUBKEY *x)
{
	if (!x) return 0;
	if (o) *o = (ASN1_OBJECT *)&x->alg.oid;
	if (k) *k = x->clave;
	if (kn) *kn = (int)x->claven;
	if (alg) *alg = (X509_ALGOR *)&x->alg;
	return 1;
}
/* la clave de un SPKI (una referencia nueva) */
H64_VISIBLE EVP_PKEY *X509_PUBKEY_get(const X509_PUBKEY *x)
{
	if (!x || x->sin_usar) return NULL;
	EVP_PKEY *k = NULL;
	if (oid_es(&x->alg.oid, OID_RSA, sizeof OID_RSA)) {
		if (x->alg.ptipo != V_ASN1_UNDEF && x->alg.ptipo != V_ASN1_NULL) return NULL;
		uint8_t e;
		h64_trozo c;
		size_t o;
		if (!h64_der_lee(x->clave, x->claven, &e, &c, &o) || e != 0x30 || o != x->claven) return NULL;
		k = h64_clave_nueva(K_RSA);
		const uint8_t *p = c.p;
		size_t q = c.n;
		if (!k || !lee_entero(&p, &q, &k->n) || !lee_entero(&p, &q, &k->e) || q || h64bn_es_cero(k->n)) {
			EVP_PKEY_free(k);
			return NULL;
		}
	} else if (oid_es(&x->alg.oid, OID_EC, sizeof OID_EC)) {
		if (x->alg.ptipo != V_ASN1_OBJECT) return NULL;
		int curva = h64_curva_por_oid(x->alg.poid.d, x->alg.poid.n);
		k = h64_clave_nueva(K_EC);
		if (!curva || !k || !h64_ec_pon_punto(k, curva, x->clave, x->claven)) { EVP_PKEY_free(k); return NULL; }
	} else if (oid_es(&x->alg.oid, OID_ED25519, 3) || oid_es(&x->alg.oid, OID_X25519, 3)) {
		if (x->claven != 32) return NULL;
		k = h64_clave_nueva(oid_es(&x->alg.oid, OID_ED25519, 3) ? K_ED25519 : K_X25519);
		if (!k) return NULL;
		memcpy(k->crudap, x->clave, 32);
	}
	return k;
}

/* ---- PrivateKeyInfo (PKCS #8) ---- */
typedef struct pkcs8_priv_key_info_st {
	long version;
	X509_ALGOR alg;
	uint8_t *clave, *atributos, *publica;
	size_t claven, atributosn, publican;
} PKCS8_PRIV_KEY_INFO;
H64_VISIBLE void PKCS8_PRIV_KEY_INFO_free(PKCS8_PRIV_KEY_INFO *p8)
{
	if (!p8) return;
	free(p8->alg.praw);
	if (p8->clave) { h64_borra(p8->clave, p8->claven); free(p8->clave); }
	free(p8->atributos);
	free(p8->publica);
	free(p8);
}
static uint8_t *copia_elemento(uint8_t e, const h64_trozo *c, size_t *n)
{
	uint8_t *b = malloc(c->n + 8);
	if (!b) return NULL;
	*n = h64_der_cabecera(b, e, c->n);
	memcpy(b + *n, c->p, c->n);
	*n += c->n;
	return b;
}
H64_VISIBLE PKCS8_PRIV_KEY_INFO *d2i_PKCS8_PRIV_KEY_INFO(PKCS8_PRIV_KEY_INFO **a, const uint8_t **pp, long n)
{
	uint8_t e;
	h64_trozo c, t;
	size_t o, k;
	if (!pp || !*pp || n < 0 || !h64_der_lee(*pp, (size_t)n, &e, &c, &o) || e != 0x30) return NULL;
	PKCS8_PRIV_KEY_INFO *p8 = calloc(1, sizeof *p8);
	if (!p8) return NULL;
	const uint8_t *p = c.p;
	size_t q = c.n;
	int ok = h64_der_lee(p, q, &e, &t, &k) && e == 0x02 && t.n >= 1 && t.n <= 4 && !(t.p[0] & 0x80);
	if (ok) {
		for (size_t i = 0; i < t.n; i++) p8->version = p8->version << 8 | t.p[i];
		ok = p8->version <= 1;
		p += k; q -= k;
	}
	ok = ok && lee_algor(p, q, &p8->alg, &k);
	if (ok) { p += k; q -= k; }
	ok = ok && h64_der_lee(p, q, &e, &t, &k) && e == 0x04;
	if (ok) {
		p8->claven = t.n;
		p8->clave = malloc(t.n ? t.n : 1);
		ok = p8->clave != NULL;
		if (ok) memcpy(p8->clave, t.p, t.n);
		p += k; q -= k;
	}
	if (ok && q && h64_der_lee(p, q, &e, &t, &k) && e == 0xA0) {
		p8->atributos = copia_elemento(e, &t, &p8->atributosn);
		ok = p8->atributos != NULL;
		p += k; q -= k;
	}
	if (ok && q && h64_der_lee(p, q, &e, &t, &k) && (e == 0x81 || e == 0xA1)) {
		p8->publica = copia_elemento(e, &t, &p8->publican);
		ok = p8->publica != NULL;
		p += k; q -= k;
	}
	if (!ok || q) { PKCS8_PRIV_KEY_INFO_free(p8); return NULL; }
	*pp += o;
	if (a) { PKCS8_PRIV_KEY_INFO_free(*a); *a = p8; }
	return p8;
}
H64_VISIBLE int i2d_PKCS8_PRIV_KEY_INFO(const PKCS8_PRIV_KEY_INFO *p8, uint8_t **pp)
{
	if (!p8) return -1;
	h64_buf s = { 0 }, d = { 0 };
	uint8_t v = (uint8_t)p8->version;
	h64_buf_entero(&d, &v, 1);
	mete_algor(&d, &p8->alg);
	h64_buf_cab(&d, 0x04, p8->claven);
	h64_buf_mete(&d, p8->clave, p8->claven);
	if (p8->atributos) h64_buf_mete(&d, p8->atributos, p8->atributosn);
	if (p8->publica) h64_buf_mete(&d, p8->publica, p8->publican);
	envuelve(&s, 0x30, &d);
	return i2d_sal(&s, pp);
}
H64_VISIBLE int PKCS8_pkey_get0(const ASN1_OBJECT **o, const uint8_t **k, int *kn, const X509_ALGOR **alg, const PKCS8_PRIV_KEY_INFO *p8)
{
	if (!p8) return 0;
	if (o) *o = &p8->alg.oid;
	if (k) *k = p8->clave;
	if (kn) *kn = (int)p8->claven;
	if (alg) *alg = &p8->alg;
	return 1;
}
/* RSAPrivateKey (RFC 8017, A.1.2), dos primos */
static EVP_PKEY *rsa_privada_de_der(const uint8_t *b, size_t n)
{
	uint8_t e;
	h64_trozo c;
	size_t o;
	if (!h64_der_lee(b, n, &e, &c, &o) || e != 0x30 || o != n) return NULL;
	const uint8_t *p = c.p;
	size_t q = c.n;
	BIGNUM *v = NULL;
	EVP_PKEY *k = h64_clave_nueva(K_RSA);
	if (!k || !lee_entero(&p, &q, &v) || !h64bn_es_cero(v)) { h64bn_libera(v); EVP_PKEY_free(k); return NULL; }
	h64bn_libera(v);
	BIGNUM **bs[] = { &k->n, &k->e, &k->d, &k->p, &k->q, &k->dp, &k->dq, &k->qi };
	for (size_t i = 0; i < 8; i++)
		if (!lee_entero(&p, &q, bs[i])) { EVP_PKEY_free(k); return NULL; }
	if (q || h64bn_es_cero(k->n)) { EVP_PKEY_free(k); return NULL; }   /* (otherPrimeInfos: no) */
	k->privada = 1;
	k->crt = 1;
	return k;
}
/* ECPrivateKey (RFC 5915, 3) */
static EVP_PKEY *ec_privada_de_der(const uint8_t *b, size_t n, int curva)
{
	uint8_t e;
	h64_trozo c, t;
	size_t o, k;
	if (!h64_der_lee(b, n, &e, &c, &o) || e != 0x30 || o != n) return NULL;
	const uint8_t *p = c.p;
	size_t q = c.n;
	if (!h64_der_lee(p, q, &e, &t, &k) || e != 0x02 || t.n != 1 || t.p[0] != 1) return NULL;
	p += k; q -= k;
	h64_trozo d;
	if (!h64_der_lee(p, q, &e, &d, &k) || e != 0x04) return NULL;
	p += k; q -= k;
	if (q && h64_der_lee(p, q, &e, &t, &k) && e == 0xA0) {
		h64_trozo oid;
		size_t ok2;
		if (!h64_der_lee(t.p, t.n, &e, &oid, &ok2) || e != 0x06 || h64_curva_por_oid(oid.p, oid.n) != curva) return NULL;
		p += k; q -= k;
	}
	if (q && h64_der_lee(p, q, &e, &t, &k) && e == 0xA1) { p += k; q -= k; }
	if (q) return NULL;
	EVP_PKEY *kk = h64_clave_nueva(K_EC);
	if (!kk || !h64_ec_pon_privada(kk, curva, d.p, d.n)) { EVP_PKEY_free(kk); return NULL; }
	return kk;
}
H64_VISIBLE EVP_PKEY *EVP_PKCS82PKEY(const PKCS8_PRIV_KEY_INFO *p8)
{
	if (!p8) return NULL;
	const X509_ALGOR *a = &p8->alg;
	if (oid_es(&a->oid, OID_RSA, sizeof OID_RSA)) return rsa_privada_de_der(p8->clave, p8->claven);
	if (oid_es(&a->oid, OID_EC, sizeof OID_EC)) {
		if (a->ptipo != V_ASN1_OBJECT) return NULL;
		int curva = h64_curva_por_oid(a->poid.d, a->poid.n);
		return curva ? ec_privada_de_der(p8->clave, p8->claven, curva) : NULL;
	}
	if (oid_es(&a->oid, OID_ED25519, 3) || oid_es(&a->oid, OID_X25519, 3)) {
		uint8_t e;
		h64_trozo c;
		size_t o;
		if (!h64_der_lee(p8->clave, p8->claven, &e, &c, &o) || e != 0x04 || o != p8->claven || c.n != 32) return NULL;
		return EVP_PKEY_new_raw_private_key_ex(NULL, oid_es(&a->oid, OID_ED25519, 3) ? "ED25519" : "X25519", NULL, c.p, 32);
	}
	return NULL;
}
/* la estructura propia de la clave privada: RSAPrivateKey o ECPrivateKey */
H64_VISIBLE int i2d_PrivateKey(const EVP_PKEY *k, uint8_t **pp)
{
	if (!k || !k->privada) return -1;
	h64_buf s = { 0 }, d = { 0 };
	if (k->tipo == K_RSA) {
		if (!k->crt) return -1;
		h64_buf_entero(&d, (const uint8_t *)"", 1);
		const BIGNUM *bs[] = { k->n, k->e, k->d, k->p, k->q, k->dp, k->dq, k->qi };
		for (size_t i = 0; i < 8; i++) h64_buf_entero_bn(&d, bs[i]);
	} else if (k->tipo == K_EC) {
		size_t cn;
		const uint8_t *c = h64_curva_oid(k->curva, &cn);
		h64_buf o = { 0 }, b = { 0 }, bb = { 0 };
		h64_buf_entero(&d, (const uint8_t *)"\x01", 1);
		h64_buf_cab(&d, 0x04, k->tam);
		h64_buf_mete(&d, k->dd, k->tam);
		h64_buf_cab(&o, 0x06, cn);
		h64_buf_mete(&o, c, cn);
		envuelve(&d, 0xA0, &o);
		h64_buf_mete(&bb, "", 1);
		h64_buf_mete(&bb, k->pub, k->publen);
		envuelve(&b, 0x03, &bb);
		envuelve(&d, 0xA1, &b);
	} else return -1;
	envuelve(&s, 0x30, &d);
	return i2d_sal(&s, pp);
}
