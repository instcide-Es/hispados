/* cifra.c - Hispa-PDF: el cifrado de los PDF (el «manejador estándar»):
 * RC4 de 40 a 128 bits (R2 y R3), AES-128 (R4, AESV2) y AES-256 (R5 y R6,
 * AESV3), para leer; y RC4-128, AES-128 y AES-256 para escribir. Con su MD5,
 * su RC4, su AES y sus SHA-2, hechos aquí (sin nada de fuera).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

/* ---- MD5 (RFC 1321) ---- */
typedef struct { u32 h[4]; u64 n; u8 b[64]; u32 k; } md5_t;
static const u32 md5_k[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
static const u8 md5_r[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
			     4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

static u32 rotl(u32 x, int n) { return x << n | x >> (32 - n); }

static void md5_bloque(md5_t *m, const u8 *p)
{
	u32 w[16], a = m->h[0], b = m->h[1], c = m->h[2], d = m->h[3], f, g, t;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (u32)p[4 * i] | (u32)p[4 * i + 1] << 8 | (u32)p[4 * i + 2] << 16 | (u32)p[4 * i + 3] << 24;
	for (i = 0; i < 64; i++) {
		if (i < 16) { f = (b & c) | (~b & d); g = (u32)i; }
		else if (i < 32) { f = (d & b) | (~d & c); g = (5u * i + 1) & 15; }
		else if (i < 48) { f = b ^ c ^ d; g = (3u * i + 5) & 15; }
		else { f = c ^ (b | ~d); g = (7u * i) & 15; }
		t = d;
		d = c;
		c = b;
		b = b + rotl(a + f + md5_k[i] + w[g], md5_r[i]);
		a = t;
	}
	m->h[0] += a; m->h[1] += b; m->h[2] += c; m->h[3] += d;
}

static void md5_ini(md5_t *m)
{
	m->h[0] = 0x67452301; m->h[1] = 0xefcdab89; m->h[2] = 0x98badcfe; m->h[3] = 0x10325476;
	m->n = 0;
	m->k = 0;
}

static void md5_mas(md5_t *m, const u8 *d, u32 n)
{
	m->n += n;
	while (n) {
		u32 t = 64 - m->k < n ? 64 - m->k : n;

		memcpy(m->b + m->k, d, t);
		m->k += t;
		d += t;
		n -= t;
		if (m->k == 64) {
			md5_bloque(m, m->b);
			m->k = 0;
		}
	}
}

static void md5_fin(md5_t *m, u8 s[16])
{
	u64 bits = m->n * 8;
	u8 uno = 0x80, cero = 0, l[8];
	int i;

	md5_mas(m, &uno, 1);
	while (m->k != 56)
		md5_mas(m, &cero, 1);
	for (i = 0; i < 8; i++)
		l[i] = (u8)(bits >> (8 * i));
	md5_mas(m, l, 8);
	for (i = 0; i < 16; i++)
		s[i] = (u8)(m->h[i / 4] >> (8 * (i % 4)));
}

void md5(const u8 *d, u32 n, u8 s[16])
{
	md5_t m;

	md5_ini(&m);
	md5_mas(&m, d, n);
	md5_fin(&m, s);
}

/* ---- RC4 ---- */
void rc4_pon(rc4_t *r, const u8 *k, int n)
{
	int i, j = 0;

	for (i = 0; i < 256; i++)
		r->s[i] = (u8)i;
	for (i = 0; i < 256; i++) {
		u8 t = r->s[i];

		j = (j + t + k[i % n]) & 255;
		r->s[i] = r->s[j];
		r->s[j] = t;
	}
	r->i = r->j = 0;
}

void rc4(rc4_t *r, u8 *d, u32 n)
{
	u8 i = r->i, j = r->j;

	while (n--) {
		u8 t;

		i++;
		t = r->s[i];
		j = (u8)(j + t);
		r->s[i] = r->s[j];
		r->s[j] = t;
		*d++ ^= r->s[(u8)(r->s[i] + t)];
	}
	r->i = i;
	r->j = j;
}

/* ---- AES (FIPS 197) ---- */
static u8 sbox[256], isbox[256];
static int aes_listo;

static u8 xtime(u8 x) { return (u8)(x << 1 ^ (x & 0x80 ? 0x1b : 0)); }

static u8 mul(u8 a, u8 b)
{
	u8 r = 0;

	while (b) {
		if (b & 1)
			r ^= a;
		a = xtime(a);
		b >>= 1;
	}
	return r;
}

static u8 rotl8(u8 x, int n) { return (u8)(x << n | x >> (8 - n)); }

static void aes_tablas(void)
{
	int x, y;

	for (x = 0; x < 256; x++) {
		u8 inv = 0, b;

		if (x)
			for (y = 1; y < 256; y++)
				if (mul((u8)x, (u8)y) == 1) {
					inv = (u8)y;
					break;
				}
		b = inv;
		sbox[x] = (u8)(b ^ rotl8(b, 1) ^ rotl8(b, 2) ^ rotl8(b, 3) ^ rotl8(b, 4) ^ 0x63);
	}
	for (x = 0; x < 256; x++)
		isbox[sbox[x]] = (u8)x;
	aes_listo = 1;
}

void aes_pon(aes_t *a, const u8 *k, int bits)
{
	int nk = bits / 32, i, total;
	u32 rcon = 1;

	if (!aes_listo)
		aes_tablas();
	a->rondas = nk + 6;
	total = 4 * (a->rondas + 1);
	for (i = 0; i < nk; i++)
		a->k[i] = (u32)k[4 * i] << 24 | (u32)k[4 * i + 1] << 16 | (u32)k[4 * i + 2] << 8 | k[4 * i + 3];
	for (i = nk; i < total; i++) {
		u32 t = a->k[i - 1];

		if (i % nk == 0) {
			t = t << 8 | t >> 24;
			t = (u32)sbox[t >> 24] << 24 | (u32)sbox[(t >> 16) & 255] << 16 | (u32)sbox[(t >> 8) & 255] << 8 | sbox[t & 255];
			t ^= rcon << 24;
			rcon = xtime((u8)rcon);
		} else if (nk > 6 && i % nk == 4) {
			t = (u32)sbox[t >> 24] << 24 | (u32)sbox[(t >> 16) & 255] << 16 | (u32)sbox[(t >> 8) & 255] << 8 | sbox[t & 255];
		}
		a->k[i] = a->k[i - nk] ^ t;
	}
}

static void suma_clave(u8 s[16], const u32 *k)
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] ^= (u8)(k[i / 4] >> (24 - 8 * (i % 4)));
}

void aes_cifra(const aes_t *a, const u8 e[16], u8 o[16])
{
	u8 s[16], t[16];
	int r, i, c;

	memcpy(s, e, 16);
	suma_clave(s, a->k);
	for (r = 1; r <= a->rondas; r++) {
		for (i = 0; i < 16; i++)
			s[i] = sbox[s[i]];
		/* desplazar las filas (el estado va por columnas) */
		for (c = 0; c < 4; c++)
			for (i = 0; i < 4; i++)
				t[4 * c + i] = s[4 * ((c + i) % 4) + i];
		if (r != a->rondas) {
			for (c = 0; c < 4; c++) {
				u8 *p = t + 4 * c, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];

				s[4 * c] = (u8)(xtime(a0) ^ xtime(a1) ^ a1 ^ a2 ^ a3);
				s[4 * c + 1] = (u8)(a0 ^ xtime(a1) ^ xtime(a2) ^ a2 ^ a3);
				s[4 * c + 2] = (u8)(a0 ^ a1 ^ xtime(a2) ^ xtime(a3) ^ a3);
				s[4 * c + 3] = (u8)(xtime(a0) ^ a0 ^ a1 ^ a2 ^ xtime(a3));
			}
		} else
			memcpy(s, t, 16);
		suma_clave(s, a->k + 4 * r);
	}
	memcpy(o, s, 16);
}

void aes_descifra(const aes_t *a, const u8 e[16], u8 o[16])
{
	u8 s[16], t[16];
	int r, i, c;

	memcpy(s, e, 16);
	suma_clave(s, a->k + 4 * a->rondas);
	for (r = a->rondas - 1; r >= 0; r--) {
		for (c = 0; c < 4; c++)
			for (i = 0; i < 4; i++)
				t[4 * ((c + i) % 4) + i] = s[4 * c + i];
		for (i = 0; i < 16; i++)
			t[i] = isbox[t[i]];
		suma_clave(t, a->k + 4 * r);
		if (r > 0) {
			for (c = 0; c < 4; c++) {
				u8 *p = t + 4 * c, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];

				s[4 * c] = (u8)(mul(a0, 14) ^ mul(a1, 11) ^ mul(a2, 13) ^ mul(a3, 9));
				s[4 * c + 1] = (u8)(mul(a0, 9) ^ mul(a1, 14) ^ mul(a2, 11) ^ mul(a3, 13));
				s[4 * c + 2] = (u8)(mul(a0, 13) ^ mul(a1, 9) ^ mul(a2, 14) ^ mul(a3, 11));
				s[4 * c + 3] = (u8)(mul(a0, 11) ^ mul(a1, 13) ^ mul(a2, 9) ^ mul(a3, 14));
			}
		} else
			memcpy(s, t, 16);
	}
	memcpy(o, s, 16);
}

/* ---- SHA-256 y SHA-512/384 (FIPS 180-4) ---- */
static const u32 k256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static u32 ror(u32 x, int n) { return x >> n | x << (32 - n); }

static void sha256_bloque(u32 h[8], const u8 *p)
{
	u32 w[64], a, b, c, d, e, f, g, hh, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (u32)p[4 * i] << 24 | (u32)p[4 * i + 1] << 16 | (u32)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (i = 16; i < 64; i++)
		w[i] = (ror(w[i - 2], 17) ^ ror(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] +
		       (ror(w[i - 15], 7) ^ ror(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for (i = 0; i < 64; i++) {
		t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + k256[i] + w[i];
		t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void sha256(const u8 *d, u32 n, u8 s[32])
{
	u32 h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
	u8 b[128];
	u32 i, resto;
	u64 bits = (u64)n * 8;

	for (i = 0; i + 64 <= n; i += 64)
		sha256_bloque(h, d + i);
	resto = n - i;
	memset(b, 0, sizeof b);
	memcpy(b, d + i, resto);
	b[resto] = 0x80;
	{
		u32 tot = resto + 9 <= 64 ? 64 : 128, k;

		for (k = 0; k < 8; k++)
			b[tot - 1 - k] = (u8)(bits >> (8 * k));
		sha256_bloque(h, b);
		if (tot == 128)
			sha256_bloque(h, b + 64);
	}
	for (i = 0; i < 32; i++)
		s[i] = (u8)(h[i / 4] >> (24 - 8 * (i % 4)));
}

static const u64 k512[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL,
	0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
	0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL, 0x983e5152ee66dfabULL,
	0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL,
	0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
	0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL, 0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL,
	0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL,
	0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL, 0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
	0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};

static u64 ror64(u64 x, int n) { return x >> n | x << (64 - n); }

static void sha512_bloque(u64 h[8], const u8 *p)
{
	u64 w[80], a, b, c, d, e, f, g, hh, t1, t2;
	int i, k;

	for (i = 0; i < 16; i++) {
		w[i] = 0;
		for (k = 0; k < 8; k++)
			w[i] = w[i] << 8 | p[8 * i + k];
	}
	for (i = 16; i < 80; i++)
		w[i] = (ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6)) + w[i - 7] +
		       (ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7)) + w[i - 16];
	a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
	for (i = 0; i < 80; i++) {
		t1 = hh + (ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41)) + ((e & f) ^ (~e & g)) + k512[i] + w[i];
		t2 = (ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
		hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
	}
	h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

static void sha512_con(const u8 *d, u32 n, u8 *s, const u64 ini[8], int largo)
{
	u64 h[8];
	u8 b[256];
	u32 i, resto;
	u64 bits = (u64)n * 8;
	int k;

	memcpy(h, ini, sizeof h);
	for (i = 0; i + 128 <= n; i += 128)
		sha512_bloque(h, d + i);
	resto = n - i;
	memset(b, 0, sizeof b);
	memcpy(b, d + i, resto);
	b[resto] = 0x80;
	{
		u32 tot = resto + 17 <= 128 ? 128 : 256;

		for (k = 0; k < 8; k++)
			b[tot - 1 - k] = (u8)(bits >> (8 * k));
		sha512_bloque(h, b);
		if (tot == 256)
			sha512_bloque(h, b + 128);
	}
	for (k = 0; k < largo; k++)
		s[k] = (u8)(h[k / 8] >> (56 - 8 * (k % 8)));
}

void sha512(const u8 *d, u32 n, u8 s[64])
{
	static const u64 ini[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
				   0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};
	sha512_con(d, n, s, ini, 64);
}

void sha384(const u8 *d, u32 n, u8 s[48])
{
	static const u64 ini[8] = {0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
				   0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL};
	sha512_con(d, n, s, ini, 48);
}

/* ---- AES-CBC ---- */
/* descifra n octetos (múltiplo de 16) con el IV dado; quita el relleno si «relleno» */
static u32 cbc_descifra(const aes_t *a, const u8 iv0[16], const u8 *e, u32 n, u8 *s, int relleno)
{
	u8 iv[16], b[16];
	u32 i;
	int k;

	memcpy(iv, iv0, 16);
	for (i = 0; i + 16 <= n; i += 16) {
		aes_descifra(a, e + i, b);
		for (k = 0; k < 16; k++)
			s[i + k] = b[k] ^ iv[k];
		memcpy(iv, e + i, 16);
	}
	if (relleno && i >= 16) {
		u8 p = s[i - 1];

		if (p >= 1 && p <= 16)
			i -= p;
	}
	return i;
}

static u32 cbc_cifra(const aes_t *a, const u8 iv0[16], const u8 *e, u32 n, u8 *s, int relleno)
{
	u8 iv[16], b[16];
	u32 i, total = relleno ? (n / 16 + 1) * 16 : n;
	int k;

	memcpy(iv, iv0, 16);
	for (i = 0; i < total; i += 16) {
		for (k = 0; k < 16; k++) {
			u8 x = i + k < n ? e[i + k] : (u8)(total - n);

			b[k] = x ^ iv[k];
		}
		aes_cifra(a, b, s + i);
		memcpy(iv, s + i, 16);
	}
	return total;
}

/* ---- el manejador estándar ---- */
static const u8 relleno_pdf[32] = {0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
				   0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};

static void rellena(const char *clave, u8 r[32])
{
	u32 n = clave ? (u32)strlen(clave) : 0;

	if (n > 32)
		n = 32;
	if (n)
		memcpy(r, clave, n);
	memcpy(r + n, relleno_pdf, 32 - n);
}

/* algoritmo 2: la clave del archivo a partir de la contraseña del usuario */
static void clave_r4(pcifrado *c, const u8 rell[32], u8 *clave)
{
	md5_t m;
	u8 h[16], p[4];
	int i;

	md5_ini(&m);
	md5_mas(&m, rell, 32);
	md5_mas(&m, c->o, 32);
	p[0] = (u8)c->p; p[1] = (u8)(c->p >> 8); p[2] = (u8)(c->p >> 16); p[3] = (u8)(c->p >> 24);
	md5_mas(&m, p, 4);
	md5_mas(&m, c->id0, (u32)c->n_id0);
	if (c->r >= 4 && !c->cifra_metadatos) {
		static const u8 ff[4] = {255, 255, 255, 255};
		md5_mas(&m, ff, 4);
	}
	md5_fin(&m, h);
	if (c->r >= 3)
		for (i = 0; i < 50; i++)
			md5(h, (u32)c->largo, h);
	memcpy(clave, h, (size_t)c->largo);
}

/* algoritmos 4 y 5: la U que da esa clave */
static void u_de(pcifrado *c, const u8 *clave, u8 u[32])
{
	rc4_t r;
	int i, k;

	if (c->r == 2) {
		memcpy(u, relleno_pdf, 32);
		rc4_pon(&r, clave, c->largo);
		rc4(&r, u, 32);
		return;
	}
	{
		md5_t m;

		md5_ini(&m);
		md5_mas(&m, relleno_pdf, 32);
		md5_mas(&m, c->id0, (u32)c->n_id0);
		md5_fin(&m, u);
	}
	for (i = 0; i < 20; i++) {
		u8 k2[16];

		for (k = 0; k < c->largo; k++)
			k2[k] = clave[k] ^ (u8)i;
		rc4_pon(&r, k2, c->largo);
		rc4(&r, u, 16);
	}
	memset(u + 16, 0, 16);
}

static int prueba_usuario(pcifrado *c, const u8 rell[32])
{
	u8 clave[16], u[32];

	clave_r4(c, rell, clave);
	u_de(c, clave, u);
	if (memcmp(u, c->u, c->r == 2 ? 32 : 16) != 0)
		return 0;
	memcpy(c->clave, clave, (size_t)c->largo);
	return 1;
}

/* algoritmo 7: con la del dueño se saca la del usuario */
static int prueba_dueno(pcifrado *c, const char *clave)
{
	u8 rell[32], h[16], u[32];
	rc4_t r;
	int i, k;

	rellena(clave, rell);
	md5(rell, 32, h);
	if (c->r >= 3)
		for (i = 0; i < 50; i++)
			md5(h, 16, h);
	memcpy(u, c->o, 32);
	if (c->r == 2) {
		rc4_pon(&r, h, c->largo);
		rc4(&r, u, 32);
	} else {
		for (i = 19; i >= 0; i--) {
			u8 k2[16];

			for (k = 0; k < c->largo; k++)
				k2[k] = h[k] ^ (u8)i;
			rc4_pon(&r, k2, c->largo);
			rc4(&r, u, 32);
		}
	}
	return prueba_usuario(c, u);
}

/* el «hash 2.B» de R6 (y el SHA-256 sencillo de R5) */
static void hash_r6(int r, const u8 *clave, u32 nc, const u8 *sal, const u8 *udatos, u32 nu, u8 sale[32])
{
	u8 k[64], *k1, *e;
	u32 n, ronda, largo_k = 32, i;
	u8 tmp[127 + 8 + 48];

	memcpy(tmp, clave, nc);
	memcpy(tmp + nc, sal, 8);
	if (nu)
		memcpy(tmp + nc + 8, udatos, nu);
	sha256(tmp, nc + 8 + nu, k);
	if (r == 5) {
		memcpy(sale, k, 32);
		return;
	}
	k1 = pide(64 * (127 + 64 + 48));
	e = pide(64 * (127 + 64 + 48));
	for (ronda = 0;; ronda++) {
		aes_t a;
		u32 suma = 0;

		n = 0;
		for (i = 0; i < 64; i++) {
			memcpy(k1 + n, clave, nc);
			n += nc;
			memcpy(k1 + n, k, largo_k);
			n += largo_k;
			if (nu)
				memcpy(k1 + n, udatos, nu);
			n += nu;
		}
		aes_pon(&a, k, 128);
		cbc_cifra(&a, k + 16, k1, n, e, 0);
		for (i = 0; i < 16; i++)
			suma += e[i];
		switch (suma % 3) {
		case 0: sha256(e, n, k); largo_k = 32; break;
		case 1: sha384(e, n, k); largo_k = 48; break;
		default: sha512(e, n, k); largo_k = 64; break;
		}
		if (ronda >= 63 && e[n - 1] <= ronda - 32)
			break;
		if (ronda > 2000)
			break;
	}
	so_suelta(k1);
	so_suelta(e);
	memcpy(sale, k, 32);
}

/* la contraseña, para R5 y R6: en UTF-8 y como mucho 127 octetos (lo de la
 * página 437 del sistema, pasado a UTF-8) */
static u32 clave_utf8(const char *clave, u8 *s)
{
	u32 n = 0;
	const u8 *p = (const u8 *)(clave ? clave : "");

	while (*p && n < 127)
		s[n++] = *p++;
	return n;
}

static int prueba_r6(pcifrado *c, const char *clave)
{
	u8 pw[128], h[32], iv[16];
	u32 n = clave_utf8(clave, pw);
	aes_t a;

	/* ¿la del dueño? */
	hash_r6(c->r, pw, n, c->o + 32, c->u, 48, h);
	if (memcmp(h, c->o, 32) == 0) {
		hash_r6(c->r, pw, n, c->o + 40, c->u, 48, h);
		aes_pon(&a, h, 256);
		memset(iv, 0, 16);
		cbc_descifra(&a, iv, c->oe, 32, c->clave, 0);
		return 1;
	}
	hash_r6(c->r, pw, n, c->u + 32, NULL, 0, h);
	if (memcmp(h, c->u, 32) == 0) {
		hash_r6(c->r, pw, n, c->u + 40, NULL, 0, h);
		aes_pon(&a, h, 256);
		memset(iv, 0, 16);
		cbc_descifra(&a, iv, c->ue, 32, c->clave, 0);
		return 1;
	}
	return 0;
}

static int copia_cadena(pdoc *d, pobj *dic, const char *k, u8 *dst, u32 cabe, u32 minimo)
{
	pobj *o = dic_r(d, dic, k);
	u32 n;

	if (!o || o->t != O_CADENA || o->n < minimo)
		return 0;
	n = o->n < cabe ? o->n : cabe;
	memcpy(dst, o->u.s, n);
	return 1;
}

static int metodo_de(pdoc *d, pobj *enc, const char *k)
{
	pobj *nombre = dic_r(d, enc, k), *cf, *f, *m;

	if (!nombre || o_es_nombre(nombre, "Identity"))
		return 0;
	cf = dic_r(d, enc, "CF");
	f = nombre->t == O_NOMBRE ? dic_r(d, cf, (const char *)nombre->u.s) : NULL;
	m = f ? dic_r(d, f, "CFM") : NULL;
	if (o_es_nombre(m, "AESV2"))
		return 2;
	if (o_es_nombre(m, "AESV3"))
		return 3;
	if (o_es_nombre(m, "V2"))
		return 1;
	if (o_es_nombre(m, "None"))
		return 0;
	return -1;
}

int cifrado_prepara(pdoc *d, pobj *enc, const char *clave)
{
	pcifrado *c = &d->c;
	pobj *filtro = dic_r(d, enc, "Filter"), *id = dic_r(d, d->trailer, "ID"), *em;

	if (!enc || enc->t != O_DICC || !o_es_nombre(filtro, "Standard"))
		return -1;
	c->v = (int)o_num(dic_r(d, enc, "V"), 0);
	c->r = (int)o_num(dic_r(d, enc, "R"), 0);
	c->p = (i32)(u32)(i64)o_num(dic_r(d, enc, "P"), -4);
	em = dic_r(d, enc, "EncryptMetadata");
	c->cifra_metadatos = !(em && em->t == O_BOOL && !em->u.b);
	if (id && id->t == O_ARREGLO && id->n > 0) {
		pobj *i0 = resuelve(d, id->u.v[0]);

		if (i0->t == O_CADENA) {
			c->n_id0 = (int)(i0->n < sizeof c->id0 ? i0->n : sizeof c->id0);
			memcpy(c->id0, i0->u.s, (size_t)c->n_id0);
		}
	}
	if (c->r >= 5) {
		if (!copia_cadena(d, enc, "O", c->o, 48, 48) || !copia_cadena(d, enc, "U", c->u, 48, 48) ||
		    !copia_cadena(d, enc, "OE", c->oe, 32, 32) || !copia_cadena(d, enc, "UE", c->ue, 32, 32))
			return -1;
		c->largo = 32;
		c->metodo_flujo = metodo_de(d, enc, "StmF");
		c->metodo_cadena = metodo_de(d, enc, "StrF");
		if (c->metodo_flujo < 0 || c->metodo_cadena < 0)
			return -1;
		if (clave && *clave && prueba_r6(c, clave))
			return 1;
		return prueba_r6(c, "");
	}
	if (!copia_cadena(d, enc, "O", c->o, 32, 32) || !copia_cadena(d, enc, "U", c->u, 32, 16))
		return -1;
	if (c->v == 1 || c->r == 2) {
		c->largo = 5;
	} else {
		int bits = (int)o_num(dic_r(d, enc, "Length"), 40);

		if (bits < 40 || bits > 128 || bits % 8)
			bits = 128;
		c->largo = bits / 8;
	}
	if (c->v == 4) {
		c->metodo_flujo = metodo_de(d, enc, "StmF");
		c->metodo_cadena = metodo_de(d, enc, "StrF");
		if (c->metodo_flujo < 0 || c->metodo_cadena < 0)
			return -1;
		if (c->metodo_flujo == 2 || c->metodo_cadena == 2)
			c->largo = 16;
	} else if (c->v == 1 || c->v == 2) {
		c->metodo_flujo = c->metodo_cadena = 1;
	} else
		return -1;
	if (c->r < 2 || c->r > 4)
		return -1;
	{
		u8 rell[32];

		rellena("", rell);
		if (prueba_usuario(c, rell))
			return 1;
		if (clave && *clave) {
			rellena(clave, rell);
			if (prueba_usuario(c, rell))
				return 1;
			if (prueba_dueno(c, clave))
				return 1;
		}
		if (prueba_dueno(c, ""))
			return 1;
	}
	return 0;
}

/* la clave de un objeto (R2 a R4) */
static int clave_obj(const u8 *clave, int largo, u32 num, u32 gen, int aes, u8 k[16])
{
	u8 b[16 + 9];
	int n = largo;

	memcpy(b, clave, (size_t)largo);
	b[n++] = (u8)num; b[n++] = (u8)(num >> 8); b[n++] = (u8)(num >> 16);
	b[n++] = (u8)gen; b[n++] = (u8)(gen >> 8);
	if (aes) {
		memcpy(b + n, "sAlT", 4);
		n += 4;
	}
	md5(b, (u32)n, k);
	return largo + 5 > 16 ? 16 : largo + 5;
}

u8 *descifra(pdoc *d, u32 num, u32 gen, const u8 *e, u32 n, u32 *sal, int cadena)
{
	pcifrado *c = &d->c;
	int metodo = cadena ? c->metodo_cadena : c->metodo_flujo;
	u8 *r;

	if (metodo == 0) {
		r = pide(n + 1);
		memcpy(r, e, n);
		*sal = n;
		return r;
	}
	if (metodo == 1) {
		u8 k[16];
		int kl = clave_obj(c->clave, c->largo, num, gen, 0, k);
		rc4_t r4;

		r = pide(n + 1);
		memcpy(r, e, n);
		rc4_pon(&r4, k, kl);
		rc4(&r4, r, n);
		*sal = n;
		return r;
	}
	/* AES: los 16 primeros, el IV */
	{
		aes_t a;
		u8 k[16];

		if (n < 32 || (n - 16) % 16) {
			/* (mal cifrado: lo que se pueda) */
			if (n < 32) {
				r = pide(1);
				*sal = 0;
				return r;
			}
			n = 16 + (n - 16) / 16 * 16;
		}
		if (metodo == 3)
			aes_pon(&a, c->clave, 256);
		else {
			clave_obj(c->clave, c->largo, num, gen, 1, k);
			aes_pon(&a, k, 128);
		}
		r = pide(n);
		*sal = cbc_descifra(&a, e, e + 16, n - 16, r, 1);
		return r;
	}
}

/* las cadenas de un objeto recién leído (sin recursión sin tope: lo
 * anidado ya viene con su tope del analizador) */
void descifra_cadenas(pdoc *d, pobj *o, u32 num, u32 gen, int hondo)
{
	u32 i;

	if (!o || hondo > MAX_HONDO)
		return;
	if (o->t == O_CADENA) {
		u32 n;
		u8 *r = descifra(d, num, gen, o->u.s, o->n, &n, 1);
		u8 *s = ar_pide(d, n + 1);

		memcpy(s, r, n);
		so_suelta(r);
		o->u.s = s;
		o->n = n;
	} else if (o->t == O_ARREGLO) {
		for (i = 0; i < o->n; i++)
			descifra_cadenas(d, o->u.v[i], num, gen, hondo + 1);
	} else if (o->t == O_DICC) {
		for (i = 0; i < o->n; i++)
			descifra_cadenas(d, o->u.v[2 * i + 1], num, gen, hondo + 1);
	}
}

/* ---- para escribir uno cifrado ---- */
int cifrado_nuevo(pdoc *d, pcifra_sal *s, int metodo, const char *usuario, const char *dueno, const u8 id0[16], i32 permisos)
{
	pobj *enc = o_dicc(d);
	pcifrado c;
	u8 rell[32];

	memset(&c, 0, sizeof c);
	memset(s, 0, sizeof *s);
	s->metodo = metodo;
	if (!dueno || !*dueno)
		dueno = usuario;
	c.p = permisos;
	c.cifra_metadatos = 1;
	memcpy(c.id0, id0, 16);
	c.n_id0 = 16;
	o_pon(d, enc, "Filter", o_nombre(d, "Standard"));
	o_pon(d, enc, "P", o_ent(d, permisos));
	if (metodo == 3) {
		u8 sales[32], h[32], iv[16], perms[16], u[48], o[48], ue[32], oe[32], pw[128];
		u32 n;
		aes_t a;
		pobj *cf = o_dicc(d), *std = o_dicc(d);

		so_azar(s->clave, 32);
		so_azar(sales, 32);
		memset(iv, 0, 16);
		/* U y UE */
		n = clave_utf8(usuario, pw);
		hash_r6(6, pw, n, sales, NULL, 0, h);
		memcpy(u, h, 32);
		memcpy(u + 32, sales, 16);
		hash_r6(6, pw, n, sales + 8, NULL, 0, h);
		aes_pon(&a, h, 256);
		cbc_cifra(&a, iv, s->clave, 32, ue, 0);
		/* O y OE */
		n = clave_utf8(dueno, pw);
		hash_r6(6, pw, n, sales + 16, u, 48, h);
		memcpy(o, h, 32);
		memcpy(o + 32, sales + 16, 16);
		hash_r6(6, pw, n, sales + 24, u, 48, h);
		aes_pon(&a, h, 256);
		cbc_cifra(&a, iv, s->clave, 32, oe, 0);
		/* Perms */
		perms[0] = (u8)permisos; perms[1] = (u8)(permisos >> 8); perms[2] = (u8)(permisos >> 16); perms[3] = (u8)(permisos >> 24);
		perms[4] = perms[5] = perms[6] = perms[7] = 0xFF;
		perms[8] = 'T'; perms[9] = 'a'; perms[10] = 'd'; perms[11] = 'b';
		so_azar(perms + 12, 4);
		aes_pon(&a, s->clave, 256);
		aes_cifra(&a, perms, perms);
		o_pon(d, enc, "V", o_ent(d, 5));
		o_pon(d, enc, "R", o_ent(d, 6));
		o_pon(d, enc, "Length", o_ent(d, 256));
		o_pon(d, enc, "O", o_cadena(d, o, 48));
		o_pon(d, enc, "U", o_cadena(d, u, 48));
		o_pon(d, enc, "OE", o_cadena(d, oe, 32));
		o_pon(d, enc, "UE", o_cadena(d, ue, 32));
		o_pon(d, enc, "Perms", o_cadena(d, perms, 16));
		o_pon(d, std, "CFM", o_nombre(d, "AESV3"));
		o_pon(d, std, "AuthEvent", o_nombre(d, "DocOpen"));
		o_pon(d, std, "Length", o_ent(d, 32));
		o_pon(d, cf, "StdCF", std);
		o_pon(d, enc, "CF", cf);
		o_pon(d, enc, "StmF", o_nombre(d, "StdCF"));
		o_pon(d, enc, "StrF", o_nombre(d, "StdCF"));
		s->largo = 32;
		s->dic = enc;
		{
			pobj *x;
			for (x = enc; 0;)
				(void)x;
		}
		return 1;
	}
	/* R3 (RC4-128) o R4 (AES-128): O (algoritmo 3) y U (algoritmo 5) */
	c.r = metodo == 2 ? 4 : 3;
	c.largo = 16;
	{
		u8 h[16];
		rc4_t r;
		int i, k;

		rellena(dueno, rell);
		md5(rell, 32, h);
		for (i = 0; i < 50; i++)
			md5(h, 16, h);
		rellena(usuario, c.o);
		for (i = 0; i < 20; i++) {
			u8 k2[16];

			for (k = 0; k < 16; k++)
				k2[k] = h[k] ^ (u8)i;
			rc4_pon(&r, k2, 16);
			rc4(&r, c.o, 32);
		}
	}
	rellena(usuario, rell);
	clave_r4(&c, rell, s->clave);
	u_de(&c, s->clave, c.u);
	s->largo = 16;
	o_pon(d, enc, "V", o_ent(d, metodo == 2 ? 4 : 2));
	o_pon(d, enc, "R", o_ent(d, c.r));
	o_pon(d, enc, "Length", o_ent(d, 128));
	o_pon(d, enc, "O", o_cadena(d, c.o, 32));
	o_pon(d, enc, "U", o_cadena(d, c.u, 32));
	if (metodo == 2) {
		pobj *cf = o_dicc(d), *std = o_dicc(d);

		o_pon(d, std, "CFM", o_nombre(d, "AESV2"));
		o_pon(d, std, "AuthEvent", o_nombre(d, "DocOpen"));
		o_pon(d, std, "Length", o_ent(d, 16));
		o_pon(d, cf, "StdCF", std);
		o_pon(d, enc, "CF", cf);
		o_pon(d, enc, "StmF", o_nombre(d, "StdCF"));
		o_pon(d, enc, "StrF", o_nombre(d, "StdCF"));
	}
	s->dic = enc;
	return 1;
}

u8 *cifra(pcifra_sal *c, u32 num, u32 gen, const u8 *e, u32 n, u32 *sal)
{
	u8 *r;

	if (c->metodo == 1) {
		u8 k[16];
		int kl = clave_obj(c->clave, c->largo, num, gen, 0, k);
		rc4_t r4;

		r = pide(n + 1);
		memcpy(r, e, n);
		rc4_pon(&r4, k, kl);
		rc4(&r4, r, n);
		*sal = n;
		return r;
	}
	{
		aes_t a;
		u8 k[16];

		if (c->metodo == 3)
			aes_pon(&a, c->clave, 256);
		else {
			clave_obj(c->clave, c->largo, num, gen, 1, k);
			aes_pon(&a, k, 128);
		}
		r = pide(n + 32);
		so_azar(r, 16);
		*sal = 16 + cbc_cifra(&a, r, e, n, r + 16, 1);
		return r;
	}
}
