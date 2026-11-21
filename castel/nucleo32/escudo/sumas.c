/* sumas.c - ESCUDO.BIN: las sumas MD5 (RFC 1321), SHA-1 (FIPS 180-4) y
 * SHA-256 (FIPS 180-4), las tres que usan las firmas de ClamAV (.hdb, .hsb,
 * .mdb, .msb). Escritas aquí, pequeñas, de las normas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "escudo.h"

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static u32 le32(const u8 *p) { return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

/* ---------------------------------------------------------------- MD5 */
static const u32 md5_k[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
static const u8 md5_r[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

static void md5_bloque(u32 *h, const u8 *p)
{
	u32 w[16], a = h[0], b = h[1], c = h[2], d = h[3], f, t;
	int i, g;

	for (i = 0; i < 16; i++)
		w[i] = le32(p + 4 * i);
	for (i = 0; i < 64; i++) {
		if (i < 16) {
			f = (b & c) | (~b & d);
			g = i;
		} else if (i < 32) {
			f = (d & b) | (~d & c);
			g = (5 * i + 1) & 15;
		} else if (i < 48) {
			f = b ^ c ^ d;
			g = (3 * i + 5) & 15;
		} else {
			f = c ^ (b | ~d);
			g = (7 * i) & 15;
		}
		t = d;
		d = c;
		c = b;
		b = b + ROL(a + f + md5_k[i] + w[g], md5_r[i]);
		a = t;
	}
	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
}

void md5_inicia(md5_ctx *c)
{
	c->h[0] = 0x67452301;
	c->h[1] = 0xefcdab89;
	c->h[2] = 0x98badcfe;
	c->h[3] = 0x10325476;
	c->n = 0;
}

void md5_suma(md5_ctx *c, const void *d, u32 n)
{
	const u8 *p = d;
	u32 k = (u32)(c->n & 63);

	c->n += n;
	if (k) {
		while (n && k < 64)
			c->b[k++] = *p++, n--;
		if (k < 64)
			return;
		md5_bloque(c->h, c->b);
	}
	for (; n >= 64; n -= 64, p += 64)
		md5_bloque(c->h, p);
	memcpy(c->b, p, n);
}

void md5_acaba(md5_ctx *c, u8 r[16])
{
	u64 bits = c->n * 8;
	u8 z[72];
	u32 k = (u32)(c->n & 63), m = k < 56 ? 56 - k : 120 - k, i;

	memset(z, 0, sizeof z);
	z[0] = 0x80;
	for (i = 0; i < 8; i++)
		z[m + i] = (u8)(bits >> (8 * i));
	md5_suma(c, z, m + 8);
	for (i = 0; i < 16; i++)
		r[i] = (u8)(c->h[i / 4] >> (8 * (i & 3)));
}

/* -------------------------------------------------------------- SHA-1 */
static void sha1_bloque(u32 *h, const u8 *p)
{
	u32 w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f, k, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = be32(p + 4 * i);
	for (; i < 80; i++)
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	for (i = 0; i < 80; i++) {
		if (i < 20)
			f = (b & c) | (~b & d), k = 0x5A827999;
		else if (i < 40)
			f = b ^ c ^ d, k = 0x6ED9EBA1;
		else if (i < 60)
			f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
		else
			f = b ^ c ^ d, k = 0xCA62C1D6;
		t = ROL(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = ROL(b, 30);
		b = a;
		a = t;
	}
	h[0] += a;
	h[1] += b;
	h[2] += c;
	h[3] += d;
	h[4] += e;
}

void sha1_inicia(sha1_ctx *c)
{
	c->h[0] = 0x67452301;
	c->h[1] = 0xEFCDAB89;
	c->h[2] = 0x98BADCFE;
	c->h[3] = 0x10325476;
	c->h[4] = 0xC3D2E1F0;
	c->n = 0;
}

void sha1_suma(sha1_ctx *c, const void *d, u32 n)
{
	const u8 *p = d;
	u32 k = (u32)(c->n & 63);

	c->n += n;
	if (k) {
		while (n && k < 64)
			c->b[k++] = *p++, n--;
		if (k < 64)
			return;
		sha1_bloque(c->h, c->b);
	}
	for (; n >= 64; n -= 64, p += 64)
		sha1_bloque(c->h, p);
	memcpy(c->b, p, n);
}

static void acaba_be(u64 n, void (*suma)(void *, const void *, u32), void *c)
{
	u64 bits = n * 8;
	u8 z[72];
	u32 k = (u32)(n & 63), m = k < 56 ? 56 - k : 120 - k, i;

	memset(z, 0, sizeof z);
	z[0] = 0x80;
	for (i = 0; i < 8; i++)
		z[m + i] = (u8)(bits >> (56 - 8 * i));
	suma(c, z, m + 8);
}

void sha1_acaba(sha1_ctx *c, u8 r[20])
{
	u32 i;

	acaba_be(c->n, (void (*)(void *, const void *, u32))sha1_suma, c);
	for (i = 0; i < 20; i++)
		r[i] = (u8)(c->h[i / 4] >> (24 - 8 * (i & 3)));
}

/* ------------------------------------------------------------ SHA-256 */
static const u32 sha256_k[64] = {
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
	u32 w[64], a, b, c, d, e, f, g, hh, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = be32(p + 4 * i);
	for (; i < 64; i++) {
		u32 s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
		u32 s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}
	a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
	for (i = 0; i < 64; i++) {
		t1 = hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha256_k[i] + w[i];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e, h[5] += f, h[6] += g, h[7] += hh;
}

void sha256_inicia(sha256_ctx *c)
{
	static const u32 h0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
				   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	memcpy(c->h, h0, sizeof h0);
	c->n = 0;
}

void sha256_suma(sha256_ctx *c, const void *d, u32 n)
{
	const u8 *p = d;
	u32 k = (u32)(c->n & 63);

	c->n += n;
	if (k) {
		while (n && k < 64)
			c->b[k++] = *p++, n--;
		if (k < 64)
			return;
		sha256_bloque(c->h, c->b);
	}
	for (; n >= 64; n -= 64, p += 64)
		sha256_bloque(c->h, p);
	memcpy(c->b, p, n);
}

void sha256_acaba(sha256_ctx *c, u8 r[32])
{
	u32 i;

	acaba_be(c->n, (void (*)(void *, const void *, u32))sha256_suma, c);
	for (i = 0; i < 32; i++)
		r[i] = (u8)(c->h[i / 4] >> (24 - 8 * (i & 3)));
}
