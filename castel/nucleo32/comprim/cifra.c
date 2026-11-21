/* cifra.c - lo de cifrar que hace falta para leer los ZIP con contraseña
 * (Comprimidos, COMPRIM.BIN): ZipCrypto (el «tradicional» de PKWARE,
 * APPNOTE 6.1), y el AES de WinZip (AE-1 y AE-2): AES-128/192/256 (solo
 * cifrar: el modo CTR no necesita descifrar), SHA-1 (FIPS 180-4), HMAC
 * (RFC 2104) y PBKDF2 (RFC 8018). Escrito aquí, sin bibliotecas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <string.h>
#include "cifra.h"

/* ------------------------------------------------------------ ZipCrypto */
static uint32_t tabla_crc[256];
static int con_tabla;

static void haz_tabla(void)
{
	uint32_t i, j, c;

	for (i = 0; i < 256; i++) {
		c = i;
		for (j = 0; j < 8; j++)
			c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		tabla_crc[i] = c;
	}
	con_tabla = 1;
}

static inline uint32_t crc_uno(uint32_t c, uint8_t b)
{
	return tabla_crc[(c ^ b) & 0xFF] ^ (c >> 8);
}

static void zc_pon(zc_llaves *k, uint8_t c)
{
	k->k0 = crc_uno(k->k0, c);
	k->k1 = (k->k1 + (k->k0 & 0xFF)) * 134775813u + 1;
	k->k2 = crc_uno(k->k2, (uint8_t)(k->k1 >> 24));
}

void zc_empieza(zc_llaves *k, const char *clave)
{
	if (!con_tabla)
		haz_tabla();
	k->k0 = 0x12345678u;
	k->k1 = 0x23456789u;
	k->k2 = 0x34567890u;
	while (*clave)
		zc_pon(k, (uint8_t)*clave++);
}

void zc_descifra(zc_llaves *k, uint8_t *p, uint32_t n)
{
	uint32_t i;
	uint16_t t;

	for (i = 0; i < n; i++) {
		t = (uint16_t)(k->k2 | 2);
		p[i] ^= (uint8_t)(((uint32_t)t * (uint32_t)(t ^ 1)) >> 8);
		zc_pon(k, p[i]);
	}
}

/* ------------------------------------------------------------------ AES */
static const uint8_t sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static uint8_t por2(uint8_t x)
{
	return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

void aes_llave(aes_ctx *c, const uint8_t *llave, int n)
{
	int nk = n / 4, total, i;
	uint8_t t[4], r = 1, x;

	c->vueltas = nk + 6;
	total = 4 * (c->vueltas + 1);
	memcpy(c->w, llave, (size_t)n);
	for (i = nk; i < total; i++) {
		memcpy(t, c->w + 4 * (i - 1), 4);
		if (i % nk == 0) {
			x = t[0];
			t[0] = (uint8_t)(sbox[t[1]] ^ r);
			t[1] = sbox[t[2]];
			t[2] = sbox[t[3]];
			t[3] = sbox[x];
			r = por2(r);
		} else if (nk > 6 && i % nk == 4) {
			t[0] = sbox[t[0]];
			t[1] = sbox[t[1]];
			t[2] = sbox[t[2]];
			t[3] = sbox[t[3]];
		}
		c->w[4 * i] = c->w[4 * (i - nk)] ^ t[0];
		c->w[4 * i + 1] = c->w[4 * (i - nk) + 1] ^ t[1];
		c->w[4 * i + 2] = c->w[4 * (i - nk) + 2] ^ t[2];
		c->w[4 * i + 3] = c->w[4 * (i - nk) + 3] ^ t[3];
	}
}

void aes_cifra(const aes_ctx *c, uint8_t *b)
{
	int v, i;
	uint8_t s[16], a0, a1, a2, a3;

	for (i = 0; i < 16; i++)
		b[i] ^= c->w[i];
	for (v = 1; v <= c->vueltas; v++) {
		/* SubBytes y ShiftRows */
		for (i = 0; i < 16; i++)
			s[i] = sbox[b[(i + 4 * (i % 4)) % 16]];
		if (v != c->vueltas)
			for (i = 0; i < 4; i++) {	/* MixColumns */
				a0 = s[4 * i];
				a1 = s[4 * i + 1];
				a2 = s[4 * i + 2];
				a3 = s[4 * i + 3];
				b[4 * i] = (uint8_t)(por2(a0) ^ por2(a1) ^ a1 ^ a2 ^ a3);
				b[4 * i + 1] = (uint8_t)(a0 ^ por2(a1) ^ por2(a2) ^ a2 ^ a3);
				b[4 * i + 2] = (uint8_t)(a0 ^ a1 ^ por2(a2) ^ por2(a3) ^ a3);
				b[4 * i + 3] = (uint8_t)(por2(a0) ^ a0 ^ a1 ^ a2 ^ por2(a3));
			}
		else
			memcpy(b, s, 16);
		for (i = 0; i < 16; i++)
			b[i] ^= c->w[16 * v + i];
	}
}

/* ---------------------------------------------------------------- SHA-1 */
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_bloque(sha1_ctx *c, const uint8_t *p)
{
	uint32_t w[80], a, b, d, e, f, k, t, cc;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (i = 16; i < 80; i++)
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	a = c->h[0];
	b = c->h[1];
	cc = c->h[2];
	d = c->h[3];
	e = c->h[4];
	for (i = 0; i < 80; i++) {
		if (i < 20) {
			f = (b & cc) | (~b & d);
			k = 0x5A827999u;
		} else if (i < 40) {
			f = b ^ cc ^ d;
			k = 0x6ED9EBA1u;
		} else if (i < 60) {
			f = (b & cc) | (b & d) | (cc & d);
			k = 0x8F1BBCDCu;
		} else {
			f = b ^ cc ^ d;
			k = 0xCA62C1D6u;
		}
		t = ROL(a, 5) + f + e + k + w[i];
		e = d;
		d = cc;
		cc = ROL(b, 30);
		b = a;
		a = t;
	}
	c->h[0] += a;
	c->h[1] += b;
	c->h[2] += cc;
	c->h[3] += d;
	c->h[4] += e;
}

void sha1_empieza(sha1_ctx *c)
{
	c->h[0] = 0x67452301u;
	c->h[1] = 0xEFCDAB89u;
	c->h[2] = 0x98BADCFEu;
	c->h[3] = 0x10325476u;
	c->h[4] = 0xC3D2E1F0u;
	c->n = 0;
	c->lleno = 0;
}

void sha1_pon(sha1_ctx *c, const void *datos, size_t n)
{
	const uint8_t *p = datos;

	c->n += n;
	while (n) {
		size_t k = 64 - c->lleno;

		if (k > n)
			k = n;
		memcpy(c->buf + c->lleno, p, k);
		c->lleno += (uint32_t)k;
		p += k;
		n -= k;
		if (c->lleno == 64) {
			sha1_bloque(c, c->buf);
			c->lleno = 0;
		}
	}
}

void sha1_acaba(sha1_ctx *c, uint8_t sale[20])
{
	uint64_t bits = c->n * 8;
	uint8_t uno = 0x80, cero = 0, l[8];
	int i;

	sha1_pon(c, &uno, 1);
	while (c->lleno != 56)
		sha1_pon(c, &cero, 1);
	for (i = 0; i < 8; i++)
		l[i] = (uint8_t)(bits >> (56 - 8 * i));
	sha1_pon(c, l, 8);
	for (i = 0; i < 20; i++)
		sale[i] = (uint8_t)(c->h[i / 4] >> (24 - 8 * (i % 4)));
}

/* ----------------------------------------------------------------- HMAC */
void hmac_empieza(hmac_ctx *h, const uint8_t *llave, size_t n)
{
	uint8_t k[64], ip[64];
	int i;

	memset(k, 0, 64);
	if (n > 64) {
		sha1_ctx c;

		sha1_empieza(&c);
		sha1_pon(&c, llave, n);
		sha1_acaba(&c, k);
	} else
		memcpy(k, llave, n);
	for (i = 0; i < 64; i++) {
		ip[i] = k[i] ^ 0x36;
		h->fuera[i] = k[i] ^ 0x5c;
	}
	sha1_empieza(&h->dentro);
	sha1_pon(&h->dentro, ip, 64);
}

void hmac_pon(hmac_ctx *h, const void *p, size_t n)
{
	sha1_pon(&h->dentro, p, n);
}

void hmac_acaba(hmac_ctx *h, uint8_t sale[20])
{
	uint8_t d[20];
	sha1_ctx c;

	sha1_acaba(&h->dentro, d);
	sha1_empieza(&c);
	sha1_pon(&c, h->fuera, 64);
	sha1_pon(&c, d, 20);
	sha1_acaba(&c, sale);
}

void pbkdf2_sha1(const char *clave, const uint8_t *sal, size_t sal_n, uint32_t vueltas, uint8_t *sale, size_t n)
{
	uint32_t bloque = 1, i;
	size_t k, lc = strlen(clave);
	uint8_t u[20], t[20], num[4];
	hmac_ctx h;

	while (n) {
		num[0] = (uint8_t)(bloque >> 24);
		num[1] = (uint8_t)(bloque >> 16);
		num[2] = (uint8_t)(bloque >> 8);
		num[3] = (uint8_t)bloque;
		hmac_empieza(&h, (const uint8_t *)clave, lc);
		hmac_pon(&h, sal, sal_n);
		hmac_pon(&h, num, 4);
		hmac_acaba(&h, u);
		memcpy(t, u, 20);
		for (i = 1; i < vueltas; i++) {
			hmac_empieza(&h, (const uint8_t *)clave, lc);
			hmac_pon(&h, u, 20);
			hmac_acaba(&h, u);
			for (k = 0; k < 20; k++)
				t[k] ^= u[k];
		}
		k = n < 20 ? n : 20;
		memcpy(sale, t, k);
		sale += k;
		n -= k;
		bloque++;
	}
}

/* ---------------------------------------------------- el AES de WinZip */
/* el contador va en los 16 octetos, el bajo primero, desde 1 */
int wz_empieza(wz_ctx *w, int fuerza, const char *clave, const uint8_t *sal, const uint8_t verifica[2])
{
	int lk = fuerza == 1 ? 16 : fuerza == 2 ? 24 : 32;
	uint8_t d[66];

	pbkdf2_sha1(clave, sal, (size_t)(lk / 2), 1000, d, (size_t)(2 * lk + 2));
	if (d[2 * lk] != verifica[0] || d[2 * lk + 1] != verifica[1])
		return -1;
	aes_llave(&w->aes, d, lk);
	hmac_empieza(&w->mac, d + lk, (size_t)lk);
	memset(w->cont, 0, 16);
	w->usado = 16;
	return 0;
}

void wz_descifra(wz_ctx *w, uint8_t *p, uint32_t n)
{
	uint32_t i;
	int j;

	hmac_pon(&w->mac, p, n);
	for (i = 0; i < n; i++) {
		if (w->usado == 16) {
			for (j = 0; j < 16; j++)
				if (++w->cont[j])
					break;
			memcpy(w->flujo, w->cont, 16);
			aes_cifra(&w->aes, w->flujo);
			w->usado = 0;
		}
		p[i] ^= w->flujo[w->usado++];
	}
}

int wz_comprueba(wz_ctx *w, const uint8_t firma[10])
{
	uint8_t m[20];

	hmac_acaba(&w->mac, m);
	return memcmp(m, firma, 10) == 0 ? 0 : -1;
}
