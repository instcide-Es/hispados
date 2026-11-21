/* cripto.c - SHA-256, HMAC, PBKDF2, BLAKE2s, HKDF de Noise y la secretbox
 * de NaCl (cripto.h). Escrito a partir de FIPS 180-4, RFC 2104, RFC 8018,
 * RFC 7693, el documento de Noise (rev. 34) y la descripción de Salsa20 de
 * D. J. Bernstein («Extending the Salsa20 nonce»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "cripto.h"
#include "monocypher.h"

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u32 le32(const u8 *p) { return (u32)p[3] << 24 | (u32)p[2] << 16 | (u32)p[1] << 8 | p[0]; }
static void pon_le32(u8 *p, u32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* ---- SHA-256 ---- */
static const u32 K256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static void sha256_bloque(u32 *h, const u8 *p)
{
	u32 w[64], a, b, c, d, e, f, g, k, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = be32(p + 4 * i);
	for (; i < 64; i++) {
		u32 s0 = ROTR(w[i - 15], 7) ^ ROTR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		u32 s1 = ROTR(w[i - 2], 17) ^ ROTR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
	for (i = 0; i < 64; i++) {
		t1 = k + (ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
		t2 = (ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		k = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
	}
	h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += k;
}

void sha256_inicia(sha256_ctx *c)
{
	static const u32 h0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	memcpy(c->h, h0, sizeof h0);
	c->n = 0;
	c->k = 0;
}

void sha256_mas(sha256_ctx *c, const void *d, u32 n)
{
	const u8 *p = d;

	c->n += n;
	while (n) {
		if (c->k == 0 && n >= 64) {
			sha256_bloque(c->h, p);
			p += 64, n -= 64;
			continue;
		}
		c->b[c->k++] = *p++;
		n--;
		if (c->k == 64) {
			sha256_bloque(c->h, c->b);
			c->k = 0;
		}
	}
}

void sha256_fin(sha256_ctx *c, u8 r[32])
{
	u64 bits = c->n * 8;
	int i;

	c->b[c->k++] = 0x80;
	if (c->k > 56) {
		while (c->k < 64)
			c->b[c->k++] = 0;
		sha256_bloque(c->h, c->b);
		c->k = 0;
	}
	while (c->k < 56)
		c->b[c->k++] = 0;
	for (i = 0; i < 8; i++)
		c->b[56 + i] = bits >> (56 - 8 * i);
	sha256_bloque(c->h, c->b);
	for (i = 0; i < 8; i++) {
		r[4 * i] = c->h[i] >> 24;
		r[4 * i + 1] = c->h[i] >> 16;
		r[4 * i + 2] = c->h[i] >> 8;
		r[4 * i + 3] = c->h[i];
	}
}

void sha256(const void *d, u32 n, u8 r[32])
{
	sha256_ctx c;

	sha256_inicia(&c);
	sha256_mas(&c, d, n);
	sha256_fin(&c, r);
}

void hmac_sha256(const u8 *clave, u32 nc, const void *d, u32 n, u8 r[32])
{
	u8 k[64], t[32];
	sha256_ctx c;
	int i;

	memset(k, 0, 64);
	if (nc > 64)
		sha256(clave, nc, k);
	else
		memcpy(k, clave, nc);
	for (i = 0; i < 64; i++)
		k[i] ^= 0x36;
	sha256_inicia(&c);
	sha256_mas(&c, k, 64);
	sha256_mas(&c, d, n);
	sha256_fin(&c, t);
	for (i = 0; i < 64; i++)
		k[i] ^= 0x36 ^ 0x5c;
	sha256_inicia(&c);
	sha256_mas(&c, k, 64);
	sha256_mas(&c, t, 32);
	sha256_fin(&c, r);
	crypto_wipe(k, sizeof k);
}

/* PBKDF2-HMAC-SHA-256, un solo bloque de 32 */
void pbkdf2_sha256(const u8 *clave, u32 nc, const u8 *sal, u32 ns, u32 vueltas, u8 r[32])
{
	u8 u[64 + 4], t[32];
	u32 i, j;

	if (ns > 60)
		ns = 60;
	memcpy(u, sal, ns);
	u[ns] = 0, u[ns + 1] = 0, u[ns + 2] = 0, u[ns + 3] = 1;
	hmac_sha256(clave, nc, u, ns + 4, t);
	memcpy(r, t, 32);
	for (i = 1; i < vueltas; i++) {
		hmac_sha256(clave, nc, t, 32, t);
		for (j = 0; j < 32; j++)
			r[j] ^= t[j];
	}
	crypto_wipe(t, sizeof t);
}

/* ---- BLAKE2s ---- */
static const u32 IV2s[8] = { 0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A, 0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19 };
static const u8 SIGMA[10][16] = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },
	{ 14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3 },
	{ 11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4 },
	{ 7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8 },
	{ 9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13 },
	{ 2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9 },
	{ 12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11 },
	{ 13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10 },
	{ 6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5 },
	{ 10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0 },
};

static void b2s_comprime(blake2s_ctx *c, int ultimo)
{
	u32 v[16], m[16];
	int i, r;

	for (i = 0; i < 16; i++)
		m[i] = le32(c->b + 4 * i);
	for (i = 0; i < 8; i++)
		v[i] = c->h[i], v[i + 8] = IV2s[i];
	v[12] ^= c->t[0];
	v[13] ^= c->t[1];
	if (ultimo)
		v[14] = ~v[14];
#define G(a, b, cc, d, x, y) \
	v[a] = v[a] + v[b] + x; v[d] = ROTR(v[d] ^ v[a], 16); \
	v[cc] = v[cc] + v[d]; v[b] = ROTR(v[b] ^ v[cc], 12); \
	v[a] = v[a] + v[b] + y; v[d] = ROTR(v[d] ^ v[a], 8); \
	v[cc] = v[cc] + v[d]; v[b] = ROTR(v[b] ^ v[cc], 7);
	for (r = 0; r < 10; r++) {
		const u8 *s = SIGMA[r];
		G(0, 4, 8, 12, m[s[0]], m[s[1]]);
		G(1, 5, 9, 13, m[s[2]], m[s[3]]);
		G(2, 6, 10, 14, m[s[4]], m[s[5]]);
		G(3, 7, 11, 15, m[s[6]], m[s[7]]);
		G(0, 5, 10, 15, m[s[8]], m[s[9]]);
		G(1, 6, 11, 12, m[s[10]], m[s[11]]);
		G(2, 7, 8, 13, m[s[12]], m[s[13]]);
		G(3, 4, 9, 14, m[s[14]], m[s[15]]);
	}
#undef G
	for (i = 0; i < 8; i++)
		c->h[i] ^= v[i] ^ v[i + 8];
}

void blake2s_inicia(blake2s_ctx *c, u32 largo, const u8 *clave, u32 nc)
{
	int i;

	for (i = 0; i < 8; i++)
		c->h[i] = IV2s[i];
	c->h[0] ^= 0x01010000 ^ (nc << 8) ^ largo;
	c->t[0] = c->t[1] = 0;
	c->k = 0;
	c->largo = largo;
	memset(c->b, 0, 64);
	if (nc) {
		memcpy(c->b, clave, nc);
		c->k = 64;
	}
}

void blake2s_mas(blake2s_ctx *c, const void *d, u32 n)
{
	const u8 *p = d;

	while (n--) {
		if (c->k == 64) {
			c->t[0] += 64;
			if (c->t[0] < 64)
				c->t[1]++;
			b2s_comprime(c, 0);
			c->k = 0;
		}
		c->b[c->k++] = *p++;
	}
}

void blake2s_fin(blake2s_ctx *c, u8 *r)
{
	u8 t[32];
	u32 i;

	c->t[0] += c->k;
	if (c->t[0] < c->k)
		c->t[1]++;
	while (c->k < 64)
		c->b[c->k++] = 0;
	b2s_comprime(c, 1);
	for (i = 0; i < 8; i++)
		pon_le32(t + 4 * i, c->h[i]);
	memcpy(r, t, c->largo);
}

void blake2s(u8 *r, u32 largo, const u8 *clave, u32 nc, const void *d, u32 n)
{
	blake2s_ctx c;

	blake2s_inicia(&c, largo, clave, nc);
	blake2s_mas(&c, d, n);
	blake2s_fin(&c, r);
}

void hmac_blake2s(const u8 *clave, u32 nc, const void *d, u32 n, u8 r[32])
{
	u8 k[64], t[32];
	blake2s_ctx c;
	int i;

	memset(k, 0, 64);
	if (nc > 64)
		blake2s(k, 32, 0, 0, clave, nc);
	else
		memcpy(k, clave, nc);
	for (i = 0; i < 64; i++)
		k[i] ^= 0x36;
	blake2s_inicia(&c, 32, 0, 0);
	blake2s_mas(&c, k, 64);
	blake2s_mas(&c, d, n);
	blake2s_fin(&c, t);
	for (i = 0; i < 64; i++)
		k[i] ^= 0x36 ^ 0x5c;
	blake2s_inicia(&c, 32, 0, 0);
	blake2s_mas(&c, k, 64);
	blake2s_mas(&c, t, 32);
	blake2s_fin(&c, r);
	crypto_wipe(k, sizeof k);
}

void hkdf_blake2s(const u8 ck[32], const u8 *ent, u32 n, u8 *s1, u8 *s2, u8 *s3)
{
	u8 prk[32], t[33], o1[32], o2[32];

	hmac_blake2s(ck, 32, ent, n, prk);
	t[0] = 1;
	hmac_blake2s(prk, 32, t, 1, o1);
	memcpy(t, o1, 32);
	t[32] = 2;
	hmac_blake2s(prk, 32, t, 33, o2);
	if (s3) {
		memcpy(t, o2, 32);
		t[32] = 3;
		hmac_blake2s(prk, 32, t, 33, s3);
	}
	if (s1)
		memcpy(s1, o1, 32);
	if (s2)
		memcpy(s2, o2, 32);
	crypto_wipe(prk, 32);
	crypto_wipe(t, 33);
	crypto_wipe(o1, 32);
	crypto_wipe(o2, 32);
}

/* ---- Salsa20, HSalsa20 y la secretbox de NaCl ---- */
static void salsa_vueltas(u32 *x)
{
	int i;

	for (i = 0; i < 10; i++) {
		x[4] ^= ROTL(x[0] + x[12], 7);  x[8] ^= ROTL(x[4] + x[0], 9);
		x[12] ^= ROTL(x[8] + x[4], 13); x[0] ^= ROTL(x[12] + x[8], 18);
		x[9] ^= ROTL(x[5] + x[1], 7);   x[13] ^= ROTL(x[9] + x[5], 9);
		x[1] ^= ROTL(x[13] + x[9], 13); x[5] ^= ROTL(x[1] + x[13], 18);
		x[14] ^= ROTL(x[10] + x[6], 7); x[2] ^= ROTL(x[14] + x[10], 9);
		x[6] ^= ROTL(x[2] + x[14], 13); x[10] ^= ROTL(x[6] + x[2], 18);
		x[3] ^= ROTL(x[15] + x[11], 7); x[7] ^= ROTL(x[3] + x[15], 9);
		x[11] ^= ROTL(x[7] + x[3], 13); x[15] ^= ROTL(x[11] + x[7], 18);
		x[1] ^= ROTL(x[0] + x[3], 7);   x[2] ^= ROTL(x[1] + x[0], 9);
		x[3] ^= ROTL(x[2] + x[1], 13);  x[0] ^= ROTL(x[3] + x[2], 18);
		x[6] ^= ROTL(x[5] + x[4], 7);   x[7] ^= ROTL(x[6] + x[5], 9);
		x[4] ^= ROTL(x[7] + x[6], 13);  x[5] ^= ROTL(x[4] + x[7], 18);
		x[11] ^= ROTL(x[10] + x[9], 7); x[8] ^= ROTL(x[11] + x[10], 9);
		x[9] ^= ROTL(x[8] + x[11], 13); x[10] ^= ROTL(x[9] + x[8], 18);
		x[12] ^= ROTL(x[15] + x[14], 7); x[13] ^= ROTL(x[12] + x[15], 9);
		x[14] ^= ROTL(x[13] + x[12], 13); x[15] ^= ROTL(x[14] + x[13], 18);
	}
}

static void salsa_estado(u32 *x, const u8 k[32], const u8 n16[16])
{
	x[0] = 0x61707865, x[5] = 0x3320646e, x[10] = 0x79622d32, x[15] = 0x6b206574;
	x[1] = le32(k), x[2] = le32(k + 4), x[3] = le32(k + 8), x[4] = le32(k + 12);
	x[11] = le32(k + 16), x[12] = le32(k + 20), x[13] = le32(k + 24), x[14] = le32(k + 28);
	x[6] = le32(n16), x[7] = le32(n16 + 4), x[8] = le32(n16 + 8), x[9] = le32(n16 + 12);
}

void hsalsa20(u8 r[32], const u8 k[32], const u8 n16[16])
{
	u32 x[16];

	salsa_estado(x, k, n16);
	salsa_vueltas(x);
	pon_le32(r, x[0]); pon_le32(r + 4, x[5]); pon_le32(r + 8, x[10]); pon_le32(r + 12, x[15]);
	pon_le32(r + 16, x[6]); pon_le32(r + 20, x[7]); pon_le32(r + 24, x[8]); pon_le32(r + 28, x[9]);
	crypto_wipe(x, sizeof x);
}

/* Salsa20 de la clave k y el nonce de 8: lo que sale, sumado (xor) a d */
static void salsa20_xor(u8 *d, u32 n, const u8 k[32], const u8 nonce8[8], u32 bloque0)
{
	u8 n16[16], b[64];
	u32 x[16], y[16], blq = bloque0, i;

	memcpy(n16, nonce8, 8);
	while (n) {
		pon_le32(n16 + 8, blq), pon_le32(n16 + 12, 0);
		salsa_estado(x, k, n16);
		memcpy(y, x, sizeof x);
		salsa_vueltas(y);
		for (i = 0; i < 16; i++)
			pon_le32(b + 4 * i, x[i] + y[i]);
		for (i = 0; i < 64 && n; i++, n--)
			*d++ ^= b[i];
		blq++;
	}
	crypto_wipe(b, sizeof b);
	crypto_wipe(x, sizeof x);
	crypto_wipe(y, sizeof y);
}

static void xsalsa_sub(u8 sub[32], const u8 nonce[24], const u8 k[32])
{
	hsalsa20(sub, k, nonce);
}

/* secretbox: la clave de Poly1305 son los 32 primeros octetos del flujo; lo
 * cifrado, el resto (desde el octeto 32 del primer bloque) */
void secretbox_cierra(u8 *sale, const u8 *ent, u32 n, const u8 nonce[24], const u8 k[32])
{
	u8 sub[32], pk[64];

	xsalsa_sub(sub, nonce, k);
	memset(pk, 0, 64);
	salsa20_xor(pk, 64, sub, nonce + 16, 0);
	memmove(sale + 16, ent, n);
	/* el flujo desde el octeto 32: el primer bloque a medias */
	{
		u32 i, m = n < 32 ? n : 32;
		for (i = 0; i < m; i++)
			sale[16 + i] ^= pk[32 + i];
		if (n > 32) {
			/* lo demás, desde el bloque 1 */
			salsa20_xor(sale + 16 + 32, n - 32, sub, nonce + 16, 1);
		}
	}
	crypto_poly1305(sale, sale + 16, n, pk);
	crypto_wipe(sub, 32);
	crypto_wipe(pk, 64);
}

int secretbox_abre(u8 *sale, const u8 *ent, u32 n, const u8 nonce[24], const u8 k[32])
{
	u8 sub[32], pk[64], marca[16];
	u32 i, m;

	if (n < 16)
		return -1;
	n -= 16;
	xsalsa_sub(sub, nonce, k);
	memset(pk, 0, 64);
	salsa20_xor(pk, 64, sub, nonce + 16, 0);
	crypto_poly1305(marca, ent + 16, n, pk);
	if (crypto_verify16(marca, ent)) {
		crypto_wipe(sub, 32);
		crypto_wipe(pk, 64);
		return -1;
	}
	memmove(sale, ent + 16, n);
	m = n < 32 ? n : 32;
	for (i = 0; i < m; i++)
		sale[i] ^= pk[32 + i];
	if (n > 32)
		salsa20_xor(sale + 32, n - 32, sub, nonce + 16, 1);
	crypto_wipe(sub, 32);
	crypto_wipe(pk, 64);
	return 0;
}

void box_clave(u8 r[32], const u8 suya[32], const u8 mia[32])
{
	u8 s[32], cero[16];

	crypto_x25519(s, mia, suya);
	memset(cero, 0, 16);
	hsalsa20(r, s, cero);
	crypto_wipe(s, 32);
}
