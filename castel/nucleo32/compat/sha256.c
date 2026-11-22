/* sha256.c - (COMPAT) SHA-256 (sha256.h), de FIPS 180-4.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "sha256.h"

static const u32 K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
#define RD(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void bloque(sha256 *s, const u8 *p)
{
	u32 w[64], a, b, c, d, e, f, g, h, k;

	for (k = 0; k < 16; k++)
		w[k] = (u32)p[4 * k] << 24 | (u32)p[4 * k + 1] << 16 | (u32)p[4 * k + 2] << 8 | p[4 * k + 3];
	for (; k < 64; k++)
		w[k] = (RD(w[k - 2], 17) ^ RD(w[k - 2], 19) ^ (w[k - 2] >> 10)) + w[k - 7] +
		       (RD(w[k - 15], 7) ^ RD(w[k - 15], 18) ^ (w[k - 15] >> 3)) + w[k - 16];
	a = s->h[0], b = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
	for (k = 0; k < 64; k++) {
		u32 t1 = h + (RD(e, 6) ^ RD(e, 11) ^ RD(e, 25)) + ((e & f) ^ (~e & g)) + K[k] + w[k];
		u32 t2 = (RD(a, 2) ^ RD(a, 13) ^ RD(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));

		h = g, g = f, f = e, e = d + t1, d = c, c = b, b = a, a = t1 + t2;
	}
	s->h[0] += a, s->h[1] += b, s->h[2] += c, s->h[3] += d, s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

void sha_empieza(sha256 *s)
{
	static const u32 h0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

	memcpy(s->h, h0, sizeof h0);
	s->n = 0;
	s->total = 0;
}

void sha_anade(sha256 *s, const void *d_, u32 n)
{
	const u8 *d = d_;

	s->total += n;
	while (n) {
		u32 t = 64 - s->n;

		if (t > n)
			t = n;
		memcpy(s->b + s->n, d, t);
		s->n += t;
		d += t;
		n -= t;
		if (s->n == 64) {
			bloque(s, s->b);
			s->n = 0;
		}
	}
}

void sha_acaba(sha256 *s, u8 r[32])
{
	u64 bits = s->total * 8;
	u8 l[8];
	int k;

	for (k = 0; k < 8; k++)
		l[k] = (u8)(bits >> (56 - 8 * k));
	sha_anade(s, "\x80", 1);
	while (s->n != 56)
		sha_anade(s, "", 1);
	sha_anade(s, l, 8);
	for (k = 0; k < 8; k++) {
		r[4 * k] = (u8)(s->h[k] >> 24);
		r[4 * k + 1] = (u8)(s->h[k] >> 16);
		r[4 * k + 2] = (u8)(s->h[k] >> 8);
		r[4 * k + 3] = (u8)s->h[k];
	}
}

void sha_hex(const u8 r[32], char d[65])
{
	static const char x[] = "0123456789abcdef";
	int k;

	for (k = 0; k < 32; k++) {
		d[2 * k] = x[r[k] >> 4];
		d[2 * k + 1] = x[r[k] & 15];
	}
	d[64] = 0;
}
