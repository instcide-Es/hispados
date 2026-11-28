/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/webcripto/cifra.c - Hispa-64, etapa 10 i: el cifrado simétrico de
 * la WebCrypto (EVP_CIPHER de OpenSSL): AES-CBC con PKCS #7, AES-CTR con el
 * contador de 128 bits, AES-GCM (BearSSL), AES-KW (RFC 3394), AES-OCB (RFC
 * 7253) y ChaCha20-Poly1305 (RFC 8439, con monocypher). El AES, el de BearSSL:
 * con AES-NI si el procesador lo tiene, si no el de tiempo constante (ct64).
 * Como OpenSSL: EVP_CipherUpdate saca lo que puede (y, al descifrar en CBC,
 * se guarda el último bloque para quitar el relleno), EVP_CipherFinal_ex lo
 * que queda (16 octetos como mucho) y comprueba la etiqueta de los AEAD.
 */
#include <stdlib.h>
#include "interno.h"
#include "monocypher.h"

enum { C_CBC = 1, C_CTR, C_GCM, C_OCB, C_KW, C_CHACHA };
typedef struct evp_cipher_st { int modo; int clave; int iv; } EVP_CIPHER;
static const EVP_CIPHER CIFRAS[] = {
	{ C_CBC, 16, 16 }, { C_CBC, 24, 16 }, { C_CBC, 32, 16 },
	{ C_CTR, 16, 16 }, { C_CTR, 24, 16 }, { C_CTR, 32, 16 },
	{ C_GCM, 16, 12 }, { C_GCM, 24, 12 }, { C_GCM, 32, 12 },
	{ C_OCB, 16, 12 }, { C_OCB, 24, 12 }, { C_OCB, 32, 12 },
	{ C_KW, 16, 8 }, { C_KW, 24, 8 }, { C_KW, 32, 8 },
	{ C_CHACHA, 32, 12 },
};
H64_VISIBLE const EVP_CIPHER *EVP_aes_128_cbc(void) { return &CIFRAS[0]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_192_cbc(void) { return &CIFRAS[1]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_256_cbc(void) { return &CIFRAS[2]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_128_ctr(void) { return &CIFRAS[3]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_192_ctr(void) { return &CIFRAS[4]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_256_ctr(void) { return &CIFRAS[5]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_128_gcm(void) { return &CIFRAS[6]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_192_gcm(void) { return &CIFRAS[7]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_256_gcm(void) { return &CIFRAS[8]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_128_ocb(void) { return &CIFRAS[9]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_192_ocb(void) { return &CIFRAS[10]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_256_ocb(void) { return &CIFRAS[11]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_128_wrap(void) { return &CIFRAS[12]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_192_wrap(void) { return &CIFRAS[13]; }
H64_VISIBLE const EVP_CIPHER *EVP_aes_256_wrap(void) { return &CIFRAS[14]; }
H64_VISIBLE const EVP_CIPHER *EVP_chacha20_poly1305(void) { return &CIFRAS[15]; }
H64_VISIBLE int EVP_CIPHER_get_key_length(const EVP_CIPHER *c) { return c ? c->clave : 0; }
H64_VISIBLE int EVP_CIPHER_get_iv_length(const EVP_CIPHER *c) { return c ? c->iv : 0; }

typedef struct evp_cipher_ctx_st {
	const EVP_CIPHER *c;
	int cifra;          /* 1 cifrar, 0 descifrar */
	int hay_clave, hay_iv, en_marcha, datos, acabado, mal;
	size_t ivlen, etiq_n;
	uint8_t clave[32], iv[128], etiq[16];
	/* AES */
	const br_block_cbcenc_class *venc;
	const br_block_cbcdec_class *vdec;
	const br_block_ctr_class *vctr;
	br_aes_gen_cbcenc_keys kenc;
	br_aes_gen_cbcdec_keys kdec;
	br_aes_gen_ctr_keys kctr;
	/* CBC: lo que queda de un bloque */
	uint8_t resto[16];
	size_t resto_n;
	/* CTR: el contador y lo que queda del flujo del bloque */
	uint8_t ctr[16], flujo[16];
	size_t flujo_n;
	/* GCM */
	br_gcm_context gcm;
	/* OCB */
	uint8_t l_ast[16], l_dol[16], l[40][16], ofs[16], suma[16], ofs_a[16], suma_a[16], resto_a[16];
	size_t resto_a_n;
	uint64_t bloques, bloques_a;
	/* ChaCha20-Poly1305 */
	crypto_poly1305_ctx poly;
	uint32_t contador;
	uint64_t aad_n, texto_n;
	uint8_t cflujo[64];
	size_t cflujo_n;
} EVP_CIPHER_CTX;

H64_VISIBLE EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void) { return calloc(1, sizeof(EVP_CIPHER_CTX)); }
H64_VISIBLE void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *x)
{
	if (!x) return;
	h64_borra(x, sizeof *x);
	free(x);
}
H64_VISIBLE int EVP_CIPHER_CTX_get_key_length(const EVP_CIPHER_CTX *x) { return x && x->c ? x->c->clave : 0; }
H64_VISIBLE int EVP_CIPHER_CTX_get_iv_length(const EVP_CIPHER_CTX *x) { return x && x->c ? (int)x->ivlen : 0; }
H64_VISIBLE void EVP_CIPHER_CTX_set_flags(EVP_CIPHER_CTX *x, int f) { (void)x; (void)f; }

/* un bloque de AES, cifrado o descifrado a pelo (CBC de un bloque con el IV a cero) */
static void aes_cifra(EVP_CIPHER_CTX *x, uint8_t b[16])
{
	uint8_t iv[16] = { 0 };
	x->venc->run(&x->kenc.vtable, iv, b, 16);
}
static void aes_descifra(EVP_CIPHER_CTX *x, uint8_t b[16])
{
	uint8_t iv[16] = { 0 };
	x->vdec->run(&x->kdec.vtable, iv, b, 16);
}
static void xor16(uint8_t *a, const uint8_t *b) { for (int i = 0; i < 16; i++) a[i] ^= b[i]; }
static void dobla(uint8_t s[16])
{
	uint8_t c = s[0] >> 7;
	for (int i = 0; i < 15; i++) s[i] = (uint8_t)(s[i] << 1 | s[i + 1] >> 7);
	s[15] = (uint8_t)(s[15] << 1) ^ (c ? 0x87 : 0);
}
static unsigned ceros_finales(uint64_t i)
{
	unsigned n = 0;
	while (!(i & 1)) { i >>= 1; n++; }
	return n;
}

static void claves_aes(EVP_CIPHER_CTX *x)
{
	x->venc = br_aes_x86ni_cbcenc_get_vtable();
	x->vdec = br_aes_x86ni_cbcdec_get_vtable();
	x->vctr = br_aes_x86ni_ctr_get_vtable();
	if (!x->venc) x->venc = &br_aes_ct64_cbcenc_vtable;
	if (!x->vdec) x->vdec = &br_aes_ct64_cbcdec_vtable;
	if (!x->vctr) x->vctr = &br_aes_ct64_ctr_vtable;
	x->venc->init(&x->kenc.vtable, x->clave, (size_t)x->c->clave);
	x->vdec->init(&x->kdec.vtable, x->clave, (size_t)x->c->clave);
	x->vctr->init(&x->kctr.vtable, x->clave, (size_t)x->c->clave);
}
static void chacha_bloque(EVP_CIPHER_CTX *x)
{
	/* el flujo de un bloque (64 octetos) con el contador de ahora */
	crypto_chacha20_ietf(x->cflujo, NULL, 64, x->clave, x->iv, x->contador);
	x->contador++;
	x->cflujo_n = 64;
}
/* empieza, cuando ya están la clave y el IV */
static int empieza(EVP_CIPHER_CTX *x)
{
	x->en_marcha = 1;
	x->datos = x->acabado = x->mal = 0;
	x->resto_n = x->flujo_n = x->resto_a_n = 0;
	x->bloques = x->bloques_a = 0;
	switch (x->c->modo) {
	case C_CBC: case C_KW:
		claves_aes(x);
		return 1;
	case C_CTR:
		claves_aes(x);
		memcpy(x->ctr, x->iv, 16);
		return 1;
	case C_GCM:
		if (!x->ivlen) return 0;
		claves_aes(x);
		br_gcm_init(&x->gcm, &x->kctr.vtable, br_ghash_pclmul_get() ? br_ghash_pclmul_get() : br_ghash_ctmul64);
		br_gcm_reset(&x->gcm, x->iv, x->ivlen);
		return 1;
	case C_OCB: {
		if (x->ivlen < 1 || x->ivlen > 15) return 0;
		claves_aes(x);
		memset(x->l_ast, 0, 16);
		aes_cifra(x, x->l_ast);
		memcpy(x->l_dol, x->l_ast, 16);
		dobla(x->l_dol);
		memcpy(x->l[0], x->l_dol, 16);
		dobla(x->l[0]);
		for (int i = 1; i < 40; i++) { memcpy(x->l[i], x->l[i - 1], 16); dobla(x->l[i]); }
		/* el desplazamiento inicial (RFC 7253, 4.2) */
		uint8_t nonce[16] = { 0 }, ktop[16], estira[24];
		nonce[0] = (uint8_t)(((x->etiq_n * 8) % 128) << 1);
		memcpy(nonce + 16 - x->ivlen, x->iv, x->ivlen);
		nonce[15 - x->ivlen] |= 1;
		unsigned bottom = nonce[15] & 63;
		nonce[15] &= 0xC0;
		memcpy(ktop, nonce, 16);
		aes_cifra(x, ktop);
		memcpy(estira, ktop, 16);
		for (int i = 0; i < 8; i++) estira[16 + i] = ktop[i] ^ ktop[i + 1];
		unsigned oct = bottom / 8, bit = bottom % 8;
		for (int i = 0; i < 16; i++)
			x->ofs[i] = (uint8_t)(bit ? (estira[i + oct] << bit) | (estira[i + oct + 1] >> (8 - bit)) : estira[i + oct]);
		memset(x->suma, 0, 16);
		memset(x->ofs_a, 0, 16);
		memset(x->suma_a, 0, 16);
		return 1;
	}
	case C_CHACHA: {
		if (x->ivlen != 12) return 0;
		uint8_t pk[64];
		crypto_chacha20_ietf(pk, NULL, 64, x->clave, x->iv, 0);
		crypto_poly1305_init(&x->poly, pk);
		h64_borra(pk, sizeof pk);
		x->contador = 1;
		x->cflujo_n = 0;
		x->aad_n = x->texto_n = 0;
		return 1;
	}
	}
	return 0;
}
H64_VISIBLE int EVP_CipherInit_ex(EVP_CIPHER_CTX *x, const EVP_CIPHER *c, void *motor, const uint8_t *clave, const uint8_t *iv, int cifra)
{
	(void)motor;
	if (!x) return 0;
	if (c) {
		h64_borra(x, sizeof *x);
		x->c = c;
		x->ivlen = (size_t)c->iv;
		x->etiq_n = 16;
	}
	if (!x->c) return 0;
	if (cifra != -1) x->cifra = cifra ? 1 : 0;
	if (clave) { memcpy(x->clave, clave, (size_t)x->c->clave); x->hay_clave = 1; }
	if (iv) { memcpy(x->iv, iv, x->ivlen); x->hay_iv = 1; }
	if (x->c->modo == C_KW && !x->hay_iv) {
		/* el IV de RFC 3394, si no se da otro */
		memset(x->iv, 0xA6, 8);
		x->hay_iv = 1;
	}
	if (x->hay_clave && x->hay_iv && (clave || iv)) return empieza(x);
	return 1;
}
#define EVP_CTRL_AEAD_SET_IVLEN 0x9
#define EVP_CTRL_AEAD_GET_TAG 0x10
#define EVP_CTRL_AEAD_SET_TAG 0x11
H64_VISIBLE int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *x, int que, int arg, void *ptr)
{
	if (!x || !x->c) return 0;
	int aead = x->c->modo == C_GCM || x->c->modo == C_OCB || x->c->modo == C_CHACHA;
	if (!aead) return 0;
	switch (que) {
	case EVP_CTRL_AEAD_SET_IVLEN:
		if (arg <= 0 || x->en_marcha) return 0;
		if (x->c->modo == C_GCM && arg > 128) return 0;   /* (hasta 128 octetos, lo que cabe aquí) */
		if (x->c->modo == C_OCB && arg > 15) return 0;
		if (x->c->modo == C_CHACHA && arg > 12) return 0;
		x->ivlen = (size_t)arg;
		return 1;
	case EVP_CTRL_AEAD_SET_TAG:
		if (arg <= 0 || arg > 16) return 0;
		if (x->c->modo == C_CHACHA && arg != 16) return 0;
		if (ptr) {
			if (x->cifra) return 0;
			memcpy(x->etiq, ptr, (size_t)arg);
		} else if (x->c->modo != C_OCB || x->en_marcha) return 0;
		x->etiq_n = (size_t)arg;
		return 1;
	case EVP_CTRL_AEAD_GET_TAG:
		if (!x->cifra || !x->acabado || arg <= 0 || arg > 16 || !ptr) return 0;
		if (x->c->modo == C_OCB && (size_t)arg != x->etiq_n) return 0;
		memcpy(ptr, x->etiq, (size_t)arg);
		return 1;
	}
	return 0;
}

/* ---- CTR: el contador entero de 128 bits ---- */
static void ctr_mas(EVP_CIPHER_CTX *x, uint8_t *d, size_t n)
{
	while (n) {
		if (x->flujo_n) {
			size_t k = n < x->flujo_n ? n : x->flujo_n;
			for (size_t i = 0; i < k; i++) d[i] ^= x->flujo[16 - x->flujo_n + i];
			x->flujo_n -= k;
			d += k;
			n -= k;
			continue;
		}
		uint32_t cc = (uint32_t)x->ctr[12] << 24 | (uint32_t)x->ctr[13] << 16 | (uint32_t)x->ctr[14] << 8 | x->ctr[15];
		uint64_t hasta_vuelta = ((uint64_t)1 << 32) - cc;
		uint64_t bl = n / 16;
		if (bl) {
			if (bl > hasta_vuelta) bl = hasta_vuelta;
			if (bl > ((uint64_t)1 << 20)) bl = (uint64_t)1 << 20;
			x->vctr->run(&x->kctr.vtable, x->ctr, cc, d, (size_t)bl * 16);
			d += bl * 16;
			n -= bl * 16;
		} else {
			memset(x->flujo, 0, 16);
			x->vctr->run(&x->kctr.vtable, x->ctr, cc, x->flujo, 16);
			x->flujo_n = 16;
			bl = 1;
		}
		/* el contador, bl más (con el acarreo a los 96 bits de arriba) */
		uint64_t nuevo = (uint64_t)cc + bl;
		x->ctr[12] = (uint8_t)(nuevo >> 24); x->ctr[13] = (uint8_t)(nuevo >> 16);
		x->ctr[14] = (uint8_t)(nuevo >> 8); x->ctr[15] = (uint8_t)nuevo;
		if (nuevo >> 32)
			for (int i = 11; i >= 0 && ++x->ctr[i] == 0; i--) {}
	}
}

/* ---- OCB ---- */
static void ocb_aad(EVP_CIPHER_CTX *x, const uint8_t *a, size_t n)
{
	while (n) {
		size_t k = 16 - x->resto_a_n;
		if (k > n) k = n;
		memcpy(x->resto_a + x->resto_a_n, a, k);
		x->resto_a_n += k;
		a += k;
		n -= k;
		if (x->resto_a_n == 16 && n) {
			/* un bloque entero, y hay más detrás: no es el último */
			uint8_t b[16];
			x->bloques_a++;
			xor16(x->ofs_a, x->l[ceros_finales(x->bloques_a)]);
			memcpy(b, x->resto_a, 16);
			xor16(b, x->ofs_a);
			aes_cifra(x, b);
			xor16(x->suma_a, b);
			x->resto_a_n = 0;
		}
	}
}
static void ocb_aad_fin(EVP_CIPHER_CTX *x)
{
	uint8_t b[16];
	if (x->resto_a_n == 16) {
		x->bloques_a++;
		xor16(x->ofs_a, x->l[ceros_finales(x->bloques_a)]);
		memcpy(b, x->resto_a, 16);
		xor16(b, x->ofs_a);
		aes_cifra(x, b);
		xor16(x->suma_a, b);
	} else if (x->resto_a_n) {
		xor16(x->ofs_a, x->l_ast);
		memset(b, 0, 16);
		memcpy(b, x->resto_a, x->resto_a_n);
		b[x->resto_a_n] = 0x80;
		xor16(b, x->ofs_a);
		aes_cifra(x, b);
		xor16(x->suma_a, b);
	}
	x->resto_a_n = 0;
}
static void ocb_bloque(EVP_CIPHER_CTX *x, const uint8_t *en, uint8_t *sal)
{
	uint8_t b[16];
	x->bloques++;
	xor16(x->ofs, x->l[ceros_finales(x->bloques)]);
	memcpy(b, en, 16);
	if (x->cifra) xor16(x->suma, b);
	xor16(b, x->ofs);
	if (x->cifra) aes_cifra(x, b); else aes_descifra(x, b);
	xor16(b, x->ofs);
	if (!x->cifra) xor16(x->suma, b);
	memcpy(sal, b, 16);
}

/* ---- KW (RFC 3394, 2.2.1 y 2.2.2) ---- */
static int kw(EVP_CIPHER_CTX *x, const uint8_t *en, size_t n, uint8_t *sal, int *escrito)
{
	if (x->cifra) {
		if (n < 16 || n % 8) return 0;
		size_t m = n / 8;
		uint8_t a[8], b[16];
		memcpy(a, x->iv, 8);
		memmove(sal + 8, en, n);
		for (uint64_t j = 0; j < 6; j++)
			for (size_t i = 1; i <= m; i++) {
				memcpy(b, a, 8);
				memcpy(b + 8, sal + 8 * i, 8);
				aes_cifra(x, b);
				uint64_t t = m * j + i;
				memcpy(a, b, 8);
				for (int k = 0; k < 8; k++) a[7 - k] ^= (uint8_t)(t >> (8 * k));
				memcpy(sal + 8 * i, b + 8, 8);
			}
		memcpy(sal, a, 8);
		*escrito = (int)(n + 8);
		return 1;
	}
	if (n < 24 || n % 8) return 0;
	size_t m = n / 8 - 1;
	uint8_t a[8], b[16];
	memcpy(a, en, 8);
	memmove(sal, en + 8, n - 8);
	for (uint64_t j = 6; j-- > 0;)
		for (size_t i = m; i >= 1; i--) {
			uint64_t t = m * j + i;
			memcpy(b, a, 8);
			for (int k = 0; k < 8; k++) b[7 - k] ^= (uint8_t)(t >> (8 * k));
			memcpy(b + 8, sal + 8 * (i - 1), 8);
			aes_descifra(x, b);
			memcpy(a, b, 8);
			memcpy(sal + 8 * (i - 1), b + 8, 8);
		}
	if (!h64_iguales_ct(a, x->iv, 8)) { h64_borra(sal, n - 8); return 0; }
	*escrito = (int)(n - 8);
	return 1;
}

/* ---- ChaCha20-Poly1305 ---- */
static void chacha_xor(EVP_CIPHER_CTX *x, const uint8_t *en, uint8_t *sal, size_t n)
{
	while (n) {
		if (!x->cflujo_n) {
			if (n >= 64) {
				/* los bloques enteros, de una vez */
				size_t k = n / 64 * 64;
				x->contador = crypto_chacha20_ietf(sal, en, k, x->clave, x->iv, x->contador);
				en += k; sal += k; n -= k;
				continue;
			}
			chacha_bloque(x);
		}
		size_t k = n < x->cflujo_n ? n : x->cflujo_n;
		for (size_t i = 0; i < k; i++) sal[i] = en[i] ^ x->cflujo[64 - x->cflujo_n + i];
		x->cflujo_n -= k;
		en += k; sal += k; n -= k;
	}
}
static void poly_relleno(EVP_CIPHER_CTX *x, uint64_t n)
{
	static const uint8_t ceros[16];
	if (n % 16) crypto_poly1305_update(&x->poly, ceros, 16 - n % 16);
}

H64_VISIBLE int EVP_CipherUpdate(EVP_CIPHER_CTX *x, uint8_t *sal, int *saln, const uint8_t *en, int enn)
{
	if (!x || !x->en_marcha || x->acabado || enn < 0) return 0;
	size_t n = (size_t)enn;
	*saln = 0;
	switch (x->c->modo) {
	case C_CBC: {
		size_t hecho = 0;
		while (n) {
			size_t k = 16 - x->resto_n;
			if (k > n) k = n;
			memcpy(x->resto + x->resto_n, en, k);
			x->resto_n += k;
			en += k;
			n -= k;
			/* al descifrar, el último bloque entero se queda hasta el final (el relleno) */
			if (x->resto_n == 16 && (x->cifra || n)) {
				memcpy(sal + hecho, x->resto, 16);
				if (x->cifra) x->venc->run(&x->kenc.vtable, x->iv, sal + hecho, 16);
				else x->vdec->run(&x->kdec.vtable, x->iv, sal + hecho, 16);
				hecho += 16;
				x->resto_n = 0;
			}
		}
		*saln = (int)hecho;
		return 1;
	}
	case C_CTR:
		memmove(sal, en, n);
		ctr_mas(x, sal, n);
		*saln = enn;
		return 1;
	case C_GCM:
		if (!sal) {
			if (x->datos) return 0;
			br_gcm_aad_inject(&x->gcm, en, n);
			*saln = enn;
			return 1;
		}
		if (!x->datos) { br_gcm_flip(&x->gcm); x->datos = 1; }
		memmove(sal, en, n);
		br_gcm_run(&x->gcm, x->cifra, sal, n);
		*saln = enn;
		return 1;
	case C_OCB: {
		if (!sal) {
			if (x->datos) return 0;
			ocb_aad(x, en, n);
			*saln = enn;
			return 1;
		}
		x->datos = 1;
		size_t hecho = 0;
		while (n) {
			size_t k = 16 - x->resto_n;
			if (k > n) k = n;
			memcpy(x->resto + x->resto_n, en, k);
			x->resto_n += k;
			en += k;
			n -= k;
			if (x->resto_n == 16 && n) {
				ocb_bloque(x, x->resto, sal + hecho);
				hecho += 16;
				x->resto_n = 0;
			}
		}
		*saln = (int)hecho;
		return 1;
	}
	case C_KW:
		if (!sal) return 0;
		if (!kw(x, en, n, sal, saln)) return -1;
		x->acabado = 1;
		return 1;
	case C_CHACHA:
		if (!sal) {
			if (x->datos) return 0;
			crypto_poly1305_update(&x->poly, en, n);
			x->aad_n += n;
			*saln = enn;
			return 1;
		}
		if (!x->datos) { poly_relleno(x, x->aad_n); x->datos = 1; }
		if (!x->cifra) crypto_poly1305_update(&x->poly, en, n);
		chacha_xor(x, en, sal, n);
		if (x->cifra) crypto_poly1305_update(&x->poly, sal, n);
		x->texto_n += n;
		*saln = enn;
		return 1;
	}
	return 0;
}
H64_VISIBLE int EVP_CipherFinal_ex(EVP_CIPHER_CTX *x, uint8_t *sal, int *saln)
{
	if (!x || !x->en_marcha) return 0;
	*saln = 0;
	if (x->acabado) return x->c->modo == C_KW;
	x->acabado = 1;
	switch (x->c->modo) {
	case C_CBC:
		if (x->cifra) {
			uint8_t r = (uint8_t)(16 - x->resto_n);
			memset(x->resto + x->resto_n, r, r);
			memcpy(sal, x->resto, 16);
			x->venc->run(&x->kenc.vtable, x->iv, sal, 16);
			*saln = 16;
			return 1;
		} else {
			if (x->resto_n != 16) return 0;
			uint8_t b[16];
			memcpy(b, x->resto, 16);
			x->vdec->run(&x->kdec.vtable, x->iv, b, 16);
			/* el relleno de PKCS #7 (sin ramas que dependan de los octetos del relleno) */
			unsigned r = b[15], malo = (r == 0) | (r > 16), rr = malo ? 16 : r;
			for (unsigned i = 0; i < 16; i++) malo |= (unsigned)(i >= 16 - rr) & (unsigned)(b[i] != r);
			if (malo) { h64_borra(b, 16); return 0; }
			memcpy(sal, b, 16 - r);
			*saln = (int)(16 - r);
			h64_borra(b, 16);
			return 1;
		}
	case C_CTR:
		return 1;
	case C_GCM:
		if (!x->datos) br_gcm_flip(&x->gcm);
		if (x->cifra) { br_gcm_get_tag(&x->gcm, x->etiq); return 1; }
		return br_gcm_check_tag_trunc(&x->gcm, x->etiq, x->etiq_n) ? 1 : 0;
	case C_OCB: {
		uint8_t b[16], etiq[16];
		if (x->resto_n == 16) { ocb_bloque(x, x->resto, sal); *saln = 16; }
		else if (x->resto_n) {
			xor16(x->ofs, x->l_ast);
			memcpy(b, x->ofs, 16);
			aes_cifra(x, b);
			for (size_t i = 0; i < x->resto_n; i++) sal[i] = x->resto[i] ^ b[i];
			memset(b, 0, 16);
			memcpy(b, x->cifra ? x->resto : sal, x->resto_n);
			b[x->resto_n] = 0x80;
			xor16(x->suma, b);
			*saln = (int)x->resto_n;
		}
		ocb_aad_fin(x);
		memcpy(etiq, x->suma, 16);
		xor16(etiq, x->ofs);
		xor16(etiq, x->l_dol);
		aes_cifra(x, etiq);
		xor16(etiq, x->suma_a);
		if (x->cifra) { memcpy(x->etiq, etiq, 16); return 1; }
		if (!h64_iguales_ct(etiq, x->etiq, x->etiq_n)) { h64_borra(sal, (size_t)*saln); *saln = 0; return 0; }
		return 1;
	}
	case C_CHACHA: {
		uint8_t lon[16], etiq[16];
		if (!x->datos) poly_relleno(x, x->aad_n);
		poly_relleno(x, x->texto_n);
		for (int i = 0; i < 8; i++) { lon[i] = (uint8_t)(x->aad_n >> (8 * i)); lon[8 + i] = (uint8_t)(x->texto_n >> (8 * i)); }
		crypto_poly1305_update(&x->poly, lon, 16);
		crypto_poly1305_final(&x->poly, etiq);
		if (x->cifra) { memcpy(x->etiq, etiq, 16); return 1; }
		return h64_iguales_ct(etiq, x->etiq, 16);
	}
	}
	return 0;
}
