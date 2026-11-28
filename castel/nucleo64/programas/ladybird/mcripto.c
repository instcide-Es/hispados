/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/ladybird/mcripto.c - Hispa-64, etapa 10 i (docs/64/HISPA-64.md,
 * sección 26): MCRIPTO prueba la WebCrypto de Hispa-64 (hispa64/webcripto y
 * sinopenssl.c: las funciones de OpenSSL 3 que pide la parte de Rust de
 * LibCrypto de Ladybird, sobre BearSSL y monocypher) por la misma API que
 * usa el Rust (sys.rs), con los vectores de las normas: FIPS 180-4 y 202,
 * SP 800-185 (cSHAKE y KMAC), RFC 4231 y 2202 (HMAC), SP 800-38A (AES-CBC y
 * CTR), el GCM de McGrew y Viega, RFC 3394 (KW), RFC 7253 (OCB), RFC 8439
 * (ChaCha20-Poly1305), RFC 6070 y 7914 (PBKDF2), RFC 5869 (HKDF), RFC 9106
 * (Argon2), RFC 8032 (Ed25519), RFC 7748 (X25519), RFC 6979 (ECDSA, la misma
 * firma: BearSSL también saca el número de RFC 6979), NIST CAVP (ECDH); y,
 * de otra implementación (mcripto-vectores.h, hecho con
 * hispa64/webcripto/herramientas/vectores.py en el anfitrión): una clave RSA
 * en PKCS #8 y sus firmas PKCS #1 v1.5 y PSS y sus cifrados OAEP, una firma
 * de P-521, un PKCS #8 de P-384 y un punto comprimido de P-256.
 * Se compila con el cruzado de Hispa-64 (hazweb.sh) o en el anfitrión (para
 * probar deprisa), con libh64capa (o los objetos de webcripto, BearSSL y
 * monocypher). Al final, «MCRIPTO: todo bien (N pruebas)» y el código 0.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- lo de OpenSSL (como en sys.rs: los tipos, opacos) ---- */
typedef struct { const char *key; unsigned int data_type; void *data; size_t data_size; size_t return_size; } OSSL_PARAM;
typedef struct x_ X;
OSSL_PARAM OSSL_PARAM_construct_octet_string(const char *, void *, size_t);
OSSL_PARAM OSSL_PARAM_construct_utf8_string(const char *, char *, size_t);
OSSL_PARAM OSSL_PARAM_construct_uint(const char *, unsigned int *);
OSSL_PARAM OSSL_PARAM_construct_int(const char *, int *);
OSSL_PARAM OSSL_PARAM_construct_end(void);
X *OSSL_PARAM_BLD_new(void);
void OSSL_PARAM_BLD_free(X *);
int OSSL_PARAM_BLD_push_BN(X *, const char *, const X *);
int OSSL_PARAM_BLD_push_utf8_string(X *, const char *, const char *, size_t);
int OSSL_PARAM_BLD_push_octet_string(X *, const char *, const void *, size_t);
int OSSL_PARAM_BLD_push_uint(X *, const char *, unsigned int);
OSSL_PARAM *OSSL_PARAM_BLD_to_param(X *);
void OSSL_PARAM_free(OSSL_PARAM *);
X *BN_new(void);
void BN_clear_free(X *);
X *BN_bin2bn(const uint8_t *, int, X *);
int BN_bn2binpad(const X *, uint8_t *, int);
int BN_num_bits(const X *);
int BN_cmp(const X *, const X *);
int BN_add(X *, const X *, const X *);
const X *BN_value_one(void);
int RAND_bytes(uint8_t *, int);
int EVP_Q_digest(void *, const char *, const char *, const void *, size_t, uint8_t *, size_t *);
X *EVP_MD_fetch(void *, const char *, const char *);
void EVP_MD_free(X *);
X *EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(X *);
int EVP_DigestInit_ex(X *, const X *, void *);
int EVP_DigestUpdate(X *, const void *, size_t);
int EVP_DigestFinalXOF(X *, uint8_t *, size_t);
int EVP_DigestSignInit_ex(X *, X **, const char *, void *, const char *, X *, const OSSL_PARAM *);
int EVP_DigestVerifyInit_ex(X *, X **, const char *, void *, const char *, X *, const OSSL_PARAM *);
int EVP_DigestSign(X *, uint8_t *, size_t *, const uint8_t *, size_t);
int EVP_DigestVerify(X *, const uint8_t *, size_t, const uint8_t *, size_t);
X *EVP_MAC_fetch(void *, const char *, const char *);
void EVP_MAC_free(X *);
X *EVP_MAC_CTX_new(X *);
void EVP_MAC_CTX_free(X *);
int EVP_MAC_init(X *, const uint8_t *, size_t, const OSSL_PARAM *);
int EVP_MAC_update(X *, const uint8_t *, size_t);
int EVP_MAC_final(X *, uint8_t *, size_t *, size_t);
X *EVP_KDF_fetch(void *, const char *, const char *);
void EVP_KDF_free(X *);
X *EVP_KDF_CTX_new(X *);
void EVP_KDF_CTX_free(X *);
int EVP_KDF_derive(X *, uint8_t *, size_t, const OSSL_PARAM *);
X *EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(X *);
int EVP_CipherInit_ex(X *, const X *, void *, const uint8_t *, const uint8_t *, int);
int EVP_CipherUpdate(X *, uint8_t *, int *, const uint8_t *, int);
int EVP_CipherFinal_ex(X *, uint8_t *, int *);
int EVP_CIPHER_CTX_ctrl(X *, int, int, void *);
const X *EVP_aes_128_cbc(void), *EVP_aes_128_ctr(void), *EVP_aes_128_gcm(void), *EVP_aes_128_ocb(void), *EVP_aes_128_wrap(void), *EVP_chacha20_poly1305(void);
void EVP_PKEY_free(X *);
int EVP_PKEY_up_ref(X *);
X *EVP_PKEY_CTX_new_from_name(void *, const char *, const char *);
X *EVP_PKEY_CTX_new_from_pkey(void *, X *, const char *);
void EVP_PKEY_CTX_free(X *);
int EVP_PKEY_CTX_set_params(X *, const OSSL_PARAM *);
int EVP_PKEY_fromdata_init(X *);
int EVP_PKEY_fromdata(X *, X **, int, OSSL_PARAM *);
int EVP_PKEY_keygen_init(X *);
int EVP_PKEY_generate(X *, X **);
int EVP_PKEY_get_bn_param(const X *, const char *, X **);
int EVP_PKEY_get_raw_public_key(const X *, uint8_t *, size_t *);
int EVP_PKEY_get_raw_private_key(const X *, uint8_t *, size_t *);
X *EVP_PKEY_new_raw_public_key_ex(void *, const char *, const char *, const uint8_t *, size_t);
X *EVP_PKEY_new_raw_private_key_ex(void *, const char *, const char *, const uint8_t *, size_t);
int EVP_PKEY_get_size(const X *);
int EVP_PKEY_get_bits(const X *);
int EVP_PKEY_public_check(X *), EVP_PKEY_private_check(X *), EVP_PKEY_pairwise_check(X *);
int EVP_PKEY_sign_init(X *), EVP_PKEY_verify_init(X *);
int EVP_PKEY_sign(X *, uint8_t *, size_t *, const uint8_t *, size_t);
int EVP_PKEY_verify(X *, const uint8_t *, size_t, const uint8_t *, size_t);
int EVP_PKEY_encrypt_init_ex(X *, const OSSL_PARAM *), EVP_PKEY_decrypt_init_ex(X *, const OSSL_PARAM *);
int EVP_PKEY_encrypt(X *, uint8_t *, size_t *, const uint8_t *, size_t);
int EVP_PKEY_decrypt(X *, uint8_t *, size_t *, const uint8_t *, size_t);
int EVP_PKEY_derive_init(X *);
int EVP_PKEY_derive_set_peer_ex(X *, X *, int);
int EVP_PKEY_derive(X *, uint8_t *, size_t *);
X *EVP_SIGNATURE_fetch(void *, const char *, const char *);
int EVP_PKEY_encapsulate_init(X *, const OSSL_PARAM *);
int i2d_PUBKEY(const X *, uint8_t **);
int i2d_PrivateKey(const X *, uint8_t **);
X *EVP_PKCS82PKEY(const X *);
X *d2i_PKCS8_PRIV_KEY_INFO(X **, const uint8_t **, long);
int i2d_PKCS8_PRIV_KEY_INFO(const X *, uint8_t **);
void PKCS8_PRIV_KEY_INFO_free(X *);
int PKCS8_pkey_get0(const X **, const uint8_t **, int *, const X **, const X *);
X *d2i_X509_PUBKEY(X **, const uint8_t **, long);
int i2d_X509_PUBKEY(const X *, uint8_t **);
void X509_PUBKEY_free(X *);
int X509_PUBKEY_get0_param(X **, const uint8_t **, int *, X **, const X *);
X *X509_PUBKEY_get(const X *);
void X509_ALGOR_get0(const X **, int *, const void **, const X *);
size_t OBJ_length(const X *);
const uint8_t *OBJ_get0_data(const X *);
X *EC_GROUP_new_by_curve_name(int);
void EC_GROUP_free(X *);
const X *EC_GROUP_get0_order(const X *);
X *EC_POINT_new(const X *);
void EC_POINT_free(X *);
int EC_POINT_mul(const X *, X *, const X *, const X *, const X *, void *);
size_t EC_POINT_point2oct(const X *, const X *, int, uint8_t *, size_t, void *);
X *ECDSA_SIG_new(void);
void ECDSA_SIG_free(X *);
int ECDSA_SIG_set0(X *, X *, X *);
void ECDSA_SIG_get0(const X *, const X **, const X **);
X *d2i_ECDSA_SIG(X **, const uint8_t **, long);
int i2d_ECDSA_SIG(const X *, uint8_t **);
#define EVP_PKEY_PUBLIC_KEY 0x86
#define EVP_PKEY_KEYPAIR 0x87

#include "mcripto-vectores.h"

static int bien, mal;
static void prueba(int ok, const char *que)
{
	if (ok) bien++;
	else { mal++; printf("  MAL: %s\n", que); }
}
static size_t hex(const char *h, uint8_t *b)
{
	size_t n = strlen(h) / 2;
	for (size_t i = 0; i < n; i++) { unsigned v; sscanf(h + 2 * i, "%2x", &v); b[i] = (uint8_t)v; }
	return n;
}
static int igual_hex(const uint8_t *b, size_t n, const char *h)
{
	uint8_t e[1024];
	return hex(h, e) == n && !memcmp(e, b, n);
}
#define UTF8(k, v) OSSL_PARAM_construct_utf8_string(k, (char *)(v), 0)
#define OCT(k, v, n) OSSL_PARAM_construct_octet_string(k, (void *)(v), n)

/* ---- resúmenes ---- */
static int resumen(const char *nombre, const char *m, const char *esperado)
{
	uint8_t s[64];
	size_t n = 0;
	return EVP_Q_digest(NULL, nombre, NULL, m, strlen(m), s, &n) == 1 && igual_hex(s, n, esperado);
}
static int xof(const char *nombre, const uint8_t **partes, const size_t *tams, size_t np, size_t n, const char *esperado)
{
	uint8_t s[128];
	X *md = EVP_MD_fetch(NULL, nombre, NULL), *c = EVP_MD_CTX_new();
	int ok = md && c && EVP_DigestInit_ex(c, md, NULL) == 1;
	for (size_t i = 0; ok && i < np; i++) ok = EVP_DigestUpdate(c, partes[i], tams[i]) == 1;
	ok = ok && EVP_DigestFinalXOF(c, s, n) == 1 && igual_hex(s, n, esperado);
	EVP_MD_CTX_free(c);
	EVP_MD_free(md);
	return ok;
}
/* el cSHAKE de lib.rs: KECCAK[c](bytepad(encode_string(N) || encode_string(S), tasa) || X || 00, L) */
static int cshake(int tasa, const char *nombre, const char *n_, const char *s_, const uint8_t *x, size_t xn, size_t l, const char *esperado)
{
	uint8_t pre[512];
	size_t k = 0;
	pre[k++] = 1; pre[k++] = (uint8_t)tasa;
	size_t nn = strlen(n_), sn = strlen(s_);
	pre[k++] = 1; pre[k++] = (uint8_t)(nn * 8);  /* (cadenas de menos de 32 octetos) */
	memcpy(pre + k, n_, nn); k += nn;
	if (sn * 8 < 256) { pre[k++] = 1; pre[k++] = (uint8_t)(sn * 8); }
	else { pre[k++] = 2; pre[k++] = (uint8_t)((sn * 8) >> 8); pre[k++] = (uint8_t)(sn * 8); }
	memcpy(pre + k, s_, sn); k += sn;
	while (k % tasa) pre[k++] = 0;
	const uint8_t *partes[] = { pre, x };
	size_t tams[] = { k, xn };
	return xof(nombre, partes, tams, 2, l, esperado);
}
static int mac(const char *nombre, const char *resumen_, const uint8_t *k, size_t kn, const uint8_t *m, size_t mn,
	unsigned tam, const char *cus, const char *esperado)
{
	uint8_t s[128];
	size_t n = 0;
	OSSL_PARAM p[4];
	int i = 0;
	if (resumen_) p[i++] = UTF8("digest", resumen_);
	if (tam) p[i++] = OSSL_PARAM_construct_uint("size", &tam);
	if (cus) p[i++] = OCT("custom", cus, strlen(cus));
	p[i] = OSSL_PARAM_construct_end();
	X *m_ = EVP_MAC_fetch(NULL, nombre, NULL), *c = m_ ? EVP_MAC_CTX_new(m_) : NULL;
	int ok = c && EVP_MAC_init(c, k, kn, p) == 1 && EVP_MAC_update(c, m, mn) == 1 && EVP_MAC_final(c, s, &n, sizeof s) == 1
		&& igual_hex(s, n, esperado);
	EVP_MAC_CTX_free(c);
	EVP_MAC_free(m_);
	return ok;
}
static void resumenes(void)
{
	static const char *aa = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	prueba(resumen("SHA1", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"), "SHA-1 de «abc» (FIPS 180-4)");
	prueba(resumen("SHA2-256", "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "SHA-256 de «abc»");
	prueba(resumen("SHA2-256", aa, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"), "SHA-256 de 448 bits");
	prueba(resumen("SHA2-384", "abc", "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"), "SHA-384 de «abc»");
	prueba(resumen("SHA2-512", "abc", "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"), "SHA-512 de «abc»");
	prueba(resumen("SHA3-256", "abc", "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"), "SHA3-256 de «abc» (FIPS 202)");
	prueba(resumen("SHA3-384", "abc", "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25"), "SHA3-384 de «abc»");
	prueba(resumen("SHA3-512", "abc", "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0"), "SHA3-512 de «abc»");
	const uint8_t *nada[] = { (const uint8_t *)"" };
	size_t cero[] = { 0 };
	prueba(xof("SHAKE128", nada, cero, 1, 32, "7f9c2ba4e88f827d616045507605853ed73b8093f6efbc88eb1a6eacfa66ef26"), "SHAKE128 de nada");
	prueba(xof("SHAKE256", nada, cero, 1, 64, "46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762fd75dc4ddd8c0f200cb05019d67b592f6fc821c49479ab48640292eacb3b7c4be"), "SHAKE256 de nada");
	/* SP 800-185, los ejemplos de cSHAKE */
	static const uint8_t x4[] = { 0, 1, 2, 3 };
	prueba(cshake(168, "KECCAK-KMAC-128", "", "Email Signature", x4, 4, 32, "c1c36925b6409a04f1b504fcbca9d82b4017277cb5ed2b2065fc1d3814d5aaf5"), "cSHAKE128, ejemplo 1 de SP 800-185");
	uint8_t x200[200];
	for (int i = 0; i < 200; i++) x200[i] = (uint8_t)i;
	prueba(cshake(136, "KECCAK-KMAC-256", "", "Email Signature", x200, 200, 64,
		"07dc27b11e51fbac75bc7b3c1d983e8b4b85fb1defaf218912ac86430273091727f42b17ed1df63e8ec118f04b23633c1dfb1574c8fb55cb45da8e25afb092bb"), "cSHAKE256, ejemplo 4 de SP 800-185");
	/* KMAC (SP 800-185, ejemplos 1, 2 y 4) */
	uint8_t kk[32];
	for (int i = 0; i < 32; i++) kk[i] = (uint8_t)(0x40 + i);
	prueba(mac("KMAC-128", NULL, kk, 32, x4, 4, 32, NULL, "e5780b0d3ea6f7d3a429c5706aa43a00fadbd7d49628839e3187243f456ee14e"), "KMAC128, ejemplo 1");
	prueba(mac("KMAC-128", NULL, kk, 32, x4, 4, 32, "My Tagged Application", "3b1fba963cd8b0b59e8c1a6d71888b7143651af8ba0a7070c0979e2811324aa5"), "KMAC128, ejemplo 2");
	prueba(mac("KMAC-256", NULL, kk, 32, x4, 4, 64, "My Tagged Application",
		"20c570c31346f703c9ac36c61c03cb64c3970d0cfc787e9b79599d273a68d2f7f69d4cc3de9d104a351689f27cf6f5951f0103f33f4f24871024d9c27773a8dd"), "KMAC256, ejemplo 4");
	prueba(!mac("KMAC-128", NULL, kk, 3, x4, 4, 32, NULL, "00"), "KMAC con una clave de 3 octetos: no (como OpenSSL)");
	/* HMAC (RFC 4231 y 2202) */
	uint8_t k0b[20];
	memset(k0b, 0x0b, 20);
	prueba(mac("HMAC", "SHA2-256", k0b, 20, (const uint8_t *)"Hi There", 8, 0, NULL, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"), "HMAC-SHA-256, RFC 4231 caso 1");
	prueba(mac("HMAC", "SHA2-512", k0b, 20, (const uint8_t *)"Hi There", 8, 0, NULL,
		"87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854"), "HMAC-SHA-512, RFC 4231 caso 1");
	const char *wy = "what do ya want for nothing?";
	prueba(mac("HMAC", "SHA2-384", (const uint8_t *)"Jefe", 4, (const uint8_t *)wy, strlen(wy), 0, NULL,
		"af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649"), "HMAC-SHA-384, RFC 4231 caso 2");
	prueba(mac("HMAC", "SHA1", k0b, 20, (const uint8_t *)"Hi There", 8, 0, NULL, "b617318655057264e28bc0b6fb378c8ef146be00"), "HMAC-SHA-1, RFC 2202 caso 1");
	prueba(mac("HMAC", "SHA3-256", (const uint8_t *)"Jefe", 4, (const uint8_t *)wy, strlen(wy), 0, NULL,
		"c7d4072e788877ae3596bbb0da73b887c9171f93095b294ae857fbe2645e1ba5"), "HMAC-SHA3-256 (comprobado con otra implementación)");
	uint8_t larga[131];
	memset(larga, 0xaa, sizeof larga);
	const char *m6 = "Test Using Larger Than Block-Size Key - Hash Key First";
	prueba(mac("HMAC", "SHA2-256", larga, 131, (const uint8_t *)m6, strlen(m6), 0, NULL, "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"), "HMAC-SHA-256, RFC 4231 caso 6 (la clave más larga que el bloque)");
}

/* ---- cifrado ---- */
static int cifra(const X *cf, int enc, const uint8_t *k, const uint8_t *iv, int ivn, const uint8_t *aad, int aadn,
	const uint8_t *en, int n, uint8_t *sal, int *saln, uint8_t *etiq, int etiqn)
{
	X *c = EVP_CIPHER_CTX_new();
	int l = 0, l2 = 0, t = 0;
	int ok = c != NULL;
	if (etiqn || ivn) {
		ok = ok && EVP_CipherInit_ex(c, cf, NULL, NULL, NULL, enc) == 1;
		if (ivn) ok = ok && EVP_CIPHER_CTX_ctrl(c, 0x9, ivn, NULL) == 1;
		if (cf == EVP_aes_128_ocb() && etiqn) ok = ok && EVP_CIPHER_CTX_ctrl(c, 0x11, etiqn, NULL) == 1;
		ok = ok && EVP_CipherInit_ex(c, NULL, NULL, k, iv, enc) == 1;
		if (!enc && etiqn) ok = ok && EVP_CIPHER_CTX_ctrl(c, 0x11, etiqn, etiq) == 1;
		if (aadn) ok = ok && EVP_CipherUpdate(c, NULL, &t, aad, aadn) == 1;
	} else ok = ok && EVP_CipherInit_ex(c, cf, NULL, k, iv, enc) == 1;
	/* en dos trozos, para ver que se lleva bien lo que queda de un bloque */
	int a = n / 3;
	ok = ok && EVP_CipherUpdate(c, sal, &l, en, a) == 1 && EVP_CipherUpdate(c, sal + l, &l2, en + a, n - a) == 1;
	l += l2;
	ok = ok && EVP_CipherFinal_ex(c, sal + l, &l2) == 1;
	l += l2;
	if (ok && enc && etiqn) ok = EVP_CIPHER_CTX_ctrl(c, 0x10, etiqn, etiq) == 1;
	*saln = l;
	EVP_CIPHER_CTX_free(c);
	return ok;
}
static int kw(int enc, const uint8_t *k, const uint8_t *en, int n, uint8_t *sal, int *saln)
{
	X *c = EVP_CIPHER_CTX_new();
	int l = 0, l2 = 0;
	int ok = c && EVP_CipherInit_ex(c, EVP_aes_128_wrap(), NULL, k, NULL, enc) == 1 && EVP_CipherUpdate(c, sal, &l, en, n) == 1
		&& EVP_CipherFinal_ex(c, sal + l, &l2) == 1;
	*saln = l + l2;
	EVP_CIPHER_CTX_free(c);
	return ok;
}
static void cifrados(void)
{
	uint8_t k[32], iv[16], p[256], s[300], d[300], e[16], a[64];
	int n, m;
	hex("2b7e151628aed2a6abf7158809cf4f3c", k);
	size_t pn = hex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", p);
	hex("000102030405060708090a0b0c0d0e0f", iv);
	prueba(cifra(EVP_aes_128_cbc(), 1, k, iv, 0, NULL, 0, p, (int)pn, s, &n, NULL, 0) && n == 80
		&& igual_hex(s, 64, "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7"),
		"AES-128-CBC (SP 800-38A, F.2.1; con el bloque de relleno)");
	prueba(cifra(EVP_aes_128_cbc(), 0, k, iv, 0, NULL, 0, s, n, d, &m, NULL, 0) && m == (int)pn && !memcmp(d, p, pn), "AES-128-CBC, descifrar y quitar el relleno");
	s[n - 1] ^= 1;
	prueba(!cifra(EVP_aes_128_cbc(), 0, k, iv, 0, NULL, 0, s, n, d, &m, NULL, 0), "AES-128-CBC con el relleno mal: falla");
	prueba(cifra(EVP_aes_128_cbc(), 1, k, iv, 0, NULL, 0, p, 13, s, &n, NULL, 0) && n == 16 && cifra(EVP_aes_128_cbc(), 0, k, iv, 0, NULL, 0, s, n, d, &m, NULL, 0)
		&& m == 13 && !memcmp(d, p, 13), "AES-128-CBC de 13 octetos, ida y vuelta");
	hex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", iv);
	prueba(cifra(EVP_aes_128_ctr(), 1, k, iv, 0, NULL, 0, p, (int)pn, s, &n, NULL, 0) && n == 64
		&& igual_hex(s, 64, "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee"),
		"AES-128-CTR (SP 800-38A, F.5.1)");
	memset(iv, 0xff, 16);
	prueba(cifra(EVP_aes_128_ctr(), 1, k, iv, 0, NULL, 0, p, 40, s, &n, NULL, 0) && n == 40
		&& igual_hex(s, 40, "e13338e36cb71962e00d020b4cedbd86d3dae15b04bb352fa0f59febfcb4da3e67da610697ed5aae"), "AES-128-CTR: el contador de 128 bits da la vuelta");
	hex("00000000000000000000000ffffffffe", iv);
	prueba(cifra(EVP_aes_128_ctr(), 1, k, iv, 0, NULL, 0, p, 64, s, &n, NULL, 0)
		&& igual_hex(s, 64, "787f5cc8b6a3bbefff337b46b302c2744076dcf1c406d567d7cdd4364f9be746424503c6ae7256d2b94aa04d9695bc89944ea98f0ffb5a172d0e7528530af90b"),
		"AES-128-CTR: el acarreo de los 32 bits de abajo a los de arriba");
	/* GCM (McGrew y Viega, caso 4) */
	uint8_t gk[16], giv[16], gp[64], ga[20];
	hex("feffe9928665731c6d6a8f9467308308", gk);
	hex("cafebabefacedbaddecaf888", giv);
	size_t gpn = hex("d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39", gp);
	hex("feedfacedeadbeeffeedfacedeadbeefabaddad2", ga);
	prueba(cifra(EVP_aes_128_gcm(), 1, gk, giv, 12, ga, 20, gp, (int)gpn, s, &n, e, 16) && n == (int)gpn
		&& igual_hex(s, (size_t)n, "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091")
		&& igual_hex(e, 16, "5bc94fbc3221a5db94fae95ae7121a47"), "AES-128-GCM, caso 4");
	prueba(cifra(EVP_aes_128_gcm(), 0, gk, giv, 12, ga, 20, s, n, d, &m, e, 12) && m == (int)gpn && !memcmp(d, gp, gpn), "AES-128-GCM, descifrar con la etiqueta de 96 bits");
	e[0] ^= 1;
	prueba(!cifra(EVP_aes_128_gcm(), 0, gk, giv, 12, ga, 20, s, n, d, &m, e, 16), "AES-128-GCM con la etiqueta mal: falla");
	hex("000102030405060708090a0b0c0d0e0f", giv);
	prueba(cifra(EVP_aes_128_gcm(), 1, gk, giv, 16, ga, 20, gp, (int)gpn, s, &n, e, 16)
		&& igual_hex(s, (size_t)n, "9333269f7190747c0c40f4e68b264bccb425eb7f01668da81e32f7c519b375249297ea2dc3f88c3a2b1c70373831df37959db053642fb009a4710555")
		&& igual_hex(e, 16, "e2982c8b98fdd5eaa7d9f52990d8978b"), "AES-128-GCM con un IV de 16 octetos");
	/* KW (RFC 3394, 4.1) */
	hex("000102030405060708090a0b0c0d0e0f", k);
	size_t kn = hex("00112233445566778899aabbccddeeff", p);
	prueba(kw(1, k, p, (int)kn, s, &n) && n == 24 && igual_hex(s, 24, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5"), "AES-KW (RFC 3394, 4.1)");
	prueba(kw(0, k, s, n, d, &m) && m == 16 && !memcmp(d, p, 16), "AES-KW, desenvolver");
	s[3] ^= 1;
	prueba(!kw(0, k, s, n, d, &m), "AES-KW con un octeto cambiado: falla");
	/* OCB (RFC 7253, apéndice A) */
	uint8_t on[12];
	hex("bbaa99887766554433221100", on);
	prueba(cifra(EVP_aes_128_ocb(), 1, k, on, 12, NULL, 0, p, 0, s, &n, e, 16) && n == 0 && igual_hex(e, 16, "785407bfffc8ad9edcc5520ac9111ee6"), "AES-128-OCB, A.1 (nada)");
	hex("bbaa99887766554433221101", on);
	hex("0001020304050607", a);
	prueba(cifra(EVP_aes_128_ocb(), 1, k, on, 12, a, 8, a, 8, s, &n, e, 16) && igual_hex(s, 8, "6820b3657b6f615a") && igual_hex(e, 16, "5725bda0d3b4eb3a257c9af1f8f03009"), "AES-128-OCB, A.2");
	uint8_t p40[40];
	for (int i = 0; i < 40; i++) p40[i] = (uint8_t)i;
	hex("bbaa9988776655443322110d", on);
	prueba(cifra(EVP_aes_128_ocb(), 1, k, on, 12, p40, 40, p40, 40, s, &n, e, 16) && n == 40
		&& igual_hex(s, 40, "d5ca91748410c1751ff8a2f618255b68a0a12e093ff454606e59f9c1d0ddc54b65e8628e568bad7a")
		&& igual_hex(e, 16, "ed07ba06a4a69483a7035490c5769e60"), "AES-128-OCB, A.14 (40 octetos)");
	prueba(cifra(EVP_aes_128_ocb(), 0, k, on, 12, p40, 40, s, n, d, &m, e, 16) && m == 40 && !memcmp(d, p40, 40), "AES-128-OCB, descifrar");
	/* ChaCha20-Poly1305 (RFC 8439, 2.8.2) */
	uint8_t ck[32], cn[12], ca[12];
	hex("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", ck);
	hex("070000004041424344454647", cn);
	hex("50515253c0c1c2c3c4c5c6c7", ca);
	const char *cp = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
	int cpn = (int)strlen(cp);
	prueba(cifra(EVP_chacha20_poly1305(), 1, ck, cn, 12, ca, 12, (const uint8_t *)cp, cpn, s, &n, e, 16) && n == cpn
		&& igual_hex(s, (size_t)n, "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116")
		&& igual_hex(e, 16, "1ae10b594f09e26a7e902ecbd0600691"), "ChaCha20-Poly1305 (RFC 8439, 2.8.2)");
	prueba(cifra(EVP_chacha20_poly1305(), 0, ck, cn, 12, ca, 12, s, n, d, &m, e, 16) && m == cpn && !memcmp(d, cp, (size_t)cpn), "ChaCha20-Poly1305, descifrar");
	s[5] ^= 1;
	prueba(!cifra(EVP_chacha20_poly1305(), 0, ck, cn, 12, ca, 12, s, n, d, &m, e, 16), "ChaCha20-Poly1305 con un octeto cambiado: falla");
}

/* ---- derivar claves ---- */
static int kdf(const char *nombre, OSSL_PARAM *p, size_t n, const char *esperado)
{
	uint8_t s[128];
	X *k = EVP_KDF_fetch(NULL, nombre, NULL), *c = k ? EVP_KDF_CTX_new(k) : NULL;
	int ok = c && EVP_KDF_derive(c, s, n, p) == 1 && (!esperado || igual_hex(s, n, esperado));
	EVP_KDF_CTX_free(c);
	EVP_KDF_free(k);
	return ok;
}
static int pbkdf2(const char *res, const char *pw, const char *sal, unsigned iter, size_t n, const char *esperado)
{
	int p5 = 1;
	OSSL_PARAM p[] = { UTF8("digest", res), OCT("pass", pw, strlen(pw)), OCT("salt", sal, strlen(sal)),
		OSSL_PARAM_construct_uint("iter", &iter), OSSL_PARAM_construct_int("pkcs5", &p5), OSSL_PARAM_construct_end() };
	return kdf("PBKDF2", p, n, esperado);
}
static int argon2(const char *nombre, const char *esperado)
{
	uint8_t pw[32], sal[16], sec[8], ad[12];
	memset(pw, 1, 32); memset(sal, 2, 16); memset(sec, 3, 8); memset(ad, 4, 12);
	unsigned pasos = 3, carriles = 4, mem = 32, tam = 32;
	OSSL_PARAM p[] = { OCT("pass", pw, 32), OCT("salt", sal, 16), OSSL_PARAM_construct_uint("iter", &pasos),
		OSSL_PARAM_construct_uint("lanes", &carriles), OSSL_PARAM_construct_uint("memcost", &mem), OSSL_PARAM_construct_uint("size", &tam),
		OCT("secret", sec, 8), OCT("ad", ad, 12), OSSL_PARAM_construct_end() };
	return kdf(nombre, p, 32, esperado);
}
static void derivaciones(void)
{
	prueba(pbkdf2("SHA1", "password", "salt", 1, 20, "0c60c80f961f0e71f3a9b524af6012062fe037a6"), "PBKDF2-HMAC-SHA-1, RFC 6070 (c = 1)");
	prueba(pbkdf2("SHA1", "password", "salt", 4096, 20, "4b007901b765489abead49d926f721d065a429c1"), "PBKDF2-HMAC-SHA-1, RFC 6070 (c = 4096)");
	prueba(pbkdf2("SHA2-256", "passwd", "salt", 1, 64,
		"55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783"), "PBKDF2-HMAC-SHA-256, RFC 7914 (11.)");
	prueba(!pbkdf2("SHA2-256", "passwd", "salt", 0, 32, NULL), "PBKDF2 con 0 vueltas: falla");
	uint8_t ikm[22], sal[13], info[10];
	memset(ikm, 0x0b, 22);
	hex("000102030405060708090a0b0c", sal);
	hex("f0f1f2f3f4f5f6f7f8f9", info);
	OSSL_PARAM p[] = { UTF8("digest", "SHA2-256"), OCT("key", ikm, 22), OCT("salt", sal, 13), OCT("info", info, 10), OSSL_PARAM_construct_end() };
	prueba(kdf("HKDF", p, 42, "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865"), "HKDF-SHA-256, RFC 5869 caso 1");
	prueba(!kdf("HKDF", p, 255 * 32 + 1, NULL), "HKDF de más de 255 bloques: falla");
	prueba(argon2("ARGON2D", "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb"), "Argon2d, RFC 9106 (5.1)");
	prueba(argon2("ARGON2I", "c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8"), "Argon2i, RFC 9106 (5.2)");
	prueba(argon2("ARGON2ID", "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"), "Argon2id, RFC 9106 (5.3)");
}

/* ---- las claves ---- */
static X *de_p8(const uint8_t *der, size_t n)
{
	const uint8_t *q = der;
	X *p8 = d2i_PKCS8_PRIV_KEY_INFO(NULL, &q, (long)n);
	if (!p8 || q != der + n) return NULL;
	uint8_t *re = NULL;
	int l = i2d_PKCS8_PRIV_KEY_INFO(p8, &re);
	int igual = l == (int)n && !memcmp(re, der, n);
	free(re);
	X *k = igual ? EVP_PKCS82PKEY(p8) : NULL;
	PKCS8_PRIV_KEY_INFO_free(p8);
	return k;
}
static X *de_spki(const uint8_t *der, size_t n)
{
	const uint8_t *q = der;
	X *x = d2i_X509_PUBKEY(NULL, &q, (long)n);
	X *k = x ? X509_PUBKEY_get(x) : NULL;
	X509_PUBKEY_free(x);
	return k;
}
static int spki_igual(X *k, const uint8_t *der, size_t n)
{
	uint8_t b[600], *q = b;
	int l = i2d_PUBKEY(k, NULL);
	return l == (int)n && i2d_PUBKEY(k, &q) == l && q == b + l && !memcmp(b, der, n);
}
static int comprueba(X *k, int priv, int pareja)
{
	X *c = EVP_PKEY_CTX_new_from_pkey(NULL, k, NULL);
	int ok = c && EVP_PKEY_public_check(c) == 1 && (!priv || EVP_PKEY_private_check(c) == 1) && (!pareja || EVP_PKEY_pairwise_check(c) == 1);
	EVP_PKEY_CTX_free(c);
	return ok;
}
static int firma_digest(X *k, const char *res, OSSL_PARAM *p, const uint8_t *m, size_t mn, uint8_t *f, size_t *fn)
{
	X *c = EVP_MD_CTX_new();
	int ok = c && EVP_DigestSignInit_ex(c, NULL, res, NULL, NULL, k, p) == 1 && EVP_DigestSign(c, NULL, fn, m, mn) == 1
		&& EVP_DigestSign(c, f, fn, m, mn) == 1;
	EVP_MD_CTX_free(c);
	return ok;
}
static int comprueba_digest(X *k, const char *res, OSSL_PARAM *p, const uint8_t *m, size_t mn, const uint8_t *f, size_t fn)
{
	X *c = EVP_MD_CTX_new();
	int ok = c && EVP_DigestVerifyInit_ex(c, NULL, res, NULL, NULL, k, p) == 1 && EVP_DigestVerify(c, f, fn, m, mn) == 1;
	EVP_MD_CTX_free(c);
	return ok;
}
static int oaep(X *k, int enc, const char *res, const char *etiq, const uint8_t *en, size_t n, uint8_t *sal, size_t *saln)
{
	OSSL_PARAM p[5];
	int i = 0;
	p[i++] = UTF8("pad-mode", "oaep");
	p[i++] = UTF8("digest", res);
	p[i++] = UTF8("mgf1-digest", res);
	if (etiq) p[i++] = OCT("oaep-label", etiq, strlen(etiq));
	p[i] = OSSL_PARAM_construct_end();
	X *c = EVP_PKEY_CTX_new_from_pkey(NULL, k, NULL);
	size_t tam = 0;
	int ok = c && (enc ? EVP_PKEY_encrypt_init_ex(c, p) : EVP_PKEY_decrypt_init_ex(c, p)) == 1
		&& (enc ? EVP_PKEY_encrypt(c, NULL, &tam, en, n) : EVP_PKEY_decrypt(c, NULL, &tam, en, n)) == 1 && tam <= *saln;
	if (ok) { *saln = tam; ok = (enc ? EVP_PKEY_encrypt(c, sal, saln, en, n) : EVP_PKEY_decrypt(c, sal, saln, en, n)) == 1; }
	EVP_PKEY_CTX_free(c);
	return ok;
}
static X *rsa_de_componentes(const uint8_t *n, size_t nn, const uint8_t *e, size_t en, const uint8_t *d, size_t dn)
{
	X *bn = BN_bin2bn(n, (int)nn, NULL), *be = BN_bin2bn(e, (int)en, NULL), *bd = BN_bin2bn(d, (int)dn, NULL);
	X *b = OSSL_PARAM_BLD_new();
	int ok = b && OSSL_PARAM_BLD_push_BN(b, "n", bn) && OSSL_PARAM_BLD_push_BN(b, "e", be) && OSSL_PARAM_BLD_push_BN(b, "d", bd);
	OSSL_PARAM *p = ok ? OSSL_PARAM_BLD_to_param(b) : NULL;
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL), *k = NULL;
	if (!(p && c && EVP_PKEY_fromdata_init(c) == 1 && EVP_PKEY_fromdata(c, &k, EVP_PKEY_KEYPAIR, p) == 1)) k = NULL;
	EVP_PKEY_CTX_free(c);
	OSSL_PARAM_free(p);
	OSSL_PARAM_BLD_free(b);
	BN_clear_free(bn); BN_clear_free(be); BN_clear_free(bd);
	return k;
}
static void claves_rsa(void)
{
	uint8_t f[600], s[600];
	size_t fn = sizeof f, sn = sizeof s;
	X *k = de_p8(RSA_P8, sizeof RSA_P8);
	prueba(k != NULL, "RSA: PKCS #8 leído (y vuelto a escribir igual)");
	if (!k) return;
	prueba(EVP_PKEY_get_bits(k) == 2048 && EVP_PKEY_get_size(k) == 256, "RSA: 2048 bits");
	prueba(comprueba(k, 1, 1), "RSA: las comprobaciones de OpenSSL (pública, privada y la pareja)");
	uint8_t *der = NULL;
	int l = i2d_PrivateKey(k, &der);
	prueba(l > 0 && l < (int)sizeof RSA_P8, "RSA: RSAPrivateKey en DER");
	free(der);
	X *pk = de_spki(RSA_SPKI, sizeof RSA_SPKI);
	prueba(pk && spki_igual(pk, RSA_SPKI, sizeof RSA_SPKI) && spki_igual(k, RSA_SPKI, sizeof RSA_SPKI), "RSA: el SPKI, leído y escrito igual que la otra implementación");
	OSSL_PARAM pk1[] = { UTF8("pad-mode", "pkcs1"), OSSL_PARAM_construct_end() };
	prueba(firma_digest(k, "SHA2-256", pk1, RSA_MSG, sizeof RSA_MSG, f, &fn) && fn == 256 && !memcmp(f, RSA_PKCS1_SHA256, 256),
		"RSA PKCS #1 v1.5 con SHA-256: la misma firma que la otra implementación");
	prueba(pk && comprueba_digest(pk, "SHA2-256", pk1, RSA_MSG, sizeof RSA_MSG, RSA_PKCS1_SHA256, 256), "RSA PKCS #1 v1.5: comprobada");
	f[10] ^= 1;
	prueba(pk && !comprueba_digest(pk, "SHA2-256", pk1, RSA_MSG, sizeof RSA_MSG, f, 256), "RSA PKCS #1 v1.5 cambiada: no vale");
	int sl = 20;
	OSSL_PARAM pss[] = { UTF8("pad-mode", "pss"), UTF8("mgf1-digest", "SHA2-384"), OSSL_PARAM_construct_int("saltlen", &sl), OSSL_PARAM_construct_end() };
	prueba(pk && comprueba_digest(pk, "SHA2-384", pss, RSA_MSG, sizeof RSA_MSG, RSA_PSS_SHA384_20, 256), "RSA-PSS con SHA-384: la firma de la otra implementación, comprobada");
	fn = sizeof f;
	prueba(firma_digest(k, "SHA2-384", pss, RSA_MSG, sizeof RSA_MSG, f, &fn) && pk && comprueba_digest(pk, "SHA2-384", pss, RSA_MSG, sizeof RSA_MSG, f, fn), "RSA-PSS: firmar y comprobar");
	sl = 32;
	prueba(pk && !comprueba_digest(pk, "SHA2-384", pss, RSA_MSG, sizeof RSA_MSG, RSA_PSS_SHA384_20, 256), "RSA-PSS con otra longitud de sal: no vale");
	sl = 0;
	fn = sizeof f;
	OSSL_PARAM pss3[] = { UTF8("pad-mode", "pss"), UTF8("mgf1-digest", "SHA3-256"), OSSL_PARAM_construct_int("saltlen", &sl), OSSL_PARAM_construct_end() };
	prueba(firma_digest(k, "SHA3-256", pss3, RSA_MSG, sizeof RSA_MSG, f, &fn) && pk && comprueba_digest(pk, "SHA3-256", pss3, RSA_MSG, sizeof RSA_MSG, f, fn), "RSA-PSS con SHA3-256 y sin sal");
	sn = sizeof s;
	prueba(oaep(k, 0, "SHA2-256", "etiqueta", RSA_OAEP_SHA256_ETIQ, 256, s, &sn) && sn == sizeof RSA_MSG && !memcmp(s, RSA_MSG, sn), "RSA-OAEP con SHA-256 y etiqueta: descifrado lo de la otra implementación");
	sn = sizeof s;
	prueba(oaep(k, 0, "SHA1", NULL, RSA_OAEP_SHA1, 256, s, &sn) && sn == sizeof RSA_MSG && !memcmp(s, RSA_MSG, sn), "RSA-OAEP con SHA-1");
	sn = sizeof s;
	prueba(!oaep(k, 0, "SHA2-256", "otra", RSA_OAEP_SHA256_ETIQ, 256, s, &sn), "RSA-OAEP con otra etiqueta: falla");
	size_t cn = sizeof f;
	sn = sizeof s;
	prueba(pk && oaep(pk, 1, "SHA3-512", NULL, RSA_MSG, sizeof RSA_MSG, f, &cn) && cn == 256 && oaep(k, 0, "SHA3-512", NULL, f, cn, s, &sn)
		&& sn == sizeof RSA_MSG && !memcmp(s, RSA_MSG, sn), "RSA-OAEP con SHA3-512: cifrar y descifrar");
	/* la misma clave con solo n, e y d (como una JWK sin el CRT): firma igual */
	X *k2 = rsa_de_componentes(RSA_N, sizeof RSA_N, RSA_E, sizeof RSA_E, RSA_D, sizeof RSA_D);
	fn = sizeof f;
	prueba(k2 && firma_digest(k2, "SHA2-256", pk1, RSA_MSG, sizeof RSA_MSG, f, &fn) && !memcmp(f, RSA_PKCS1_SHA256, 256), "RSA con solo n, e y d: la misma firma");
	X *f1 = NULL;
	prueba(k2 && !EVP_PKEY_get_bn_param(k2, "rsa-factor1", &f1) && comprueba(k2, 1, 0), "RSA con solo n, e y d: sin los factores a la vista");
	EVP_PKEY_free(k2);
	/* generar */
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "RSA", NULL), *g = NULL;
	X *b = OSSL_PARAM_BLD_new(), *e = BN_bin2bn((const uint8_t *)"\x01\x00\x01", 3, NULL);
	OSSL_PARAM_BLD_push_uint(b, "bits", 2048);
	OSSL_PARAM_BLD_push_BN(b, "e", e);
	OSSL_PARAM *gp = OSSL_PARAM_BLD_to_param(b);
	prueba(c && EVP_PKEY_keygen_init(c) == 1 && EVP_PKEY_CTX_set_params(c, gp) == 1 && EVP_PKEY_generate(c, &g) == 1 && EVP_PKEY_get_bits(g) == 2048,
		"RSA: generar una de 2048 bits");
	prueba(g && comprueba(g, 1, 1), "RSA: la generada pasa las comprobaciones de OpenSSL");
	fn = sizeof f;
	prueba(g && firma_digest(g, "SHA2-512", pk1, RSA_MSG, sizeof RSA_MSG, f, &fn) && comprueba_digest(g, "SHA2-512", pk1, RSA_MSG, sizeof RSA_MSG, f, fn), "RSA generada: firmar y comprobar");
	EVP_PKEY_free(g);
	EVP_PKEY_CTX_free(c);
	OSSL_PARAM_free(gp);
	OSSL_PARAM_BLD_free(b);
	BN_clear_free(e);
	EVP_PKEY_free(pk);
	EVP_PKEY_free(k);
}
/* una clave EC de su escalar (como ec.rs: EC_POINT_mul y luego fromdata) */
static X *ec_de_escalar(int nid, const char *grupo, const char *dhex)
{
	uint8_t d[66], pt[133];
	size_t dn = hex(dhex, d);
	X *g = EC_GROUP_new_by_curve_name(nid), *bn = BN_bin2bn(d, (int)dn, NULL), *pnt = g ? EC_POINT_new(g) : NULL;
	if (!pnt || EC_POINT_mul(g, pnt, bn, NULL, NULL, NULL) != 1) return NULL;
	size_t pn = EC_POINT_point2oct(g, pnt, 4, NULL, 0, NULL);
	EC_POINT_point2oct(g, pnt, 4, pt, pn, NULL);
	X *b = OSSL_PARAM_BLD_new();
	OSSL_PARAM_BLD_push_utf8_string(b, "group", grupo, 0);
	OSSL_PARAM_BLD_push_BN(b, "priv", bn);
	OSSL_PARAM_BLD_push_octet_string(b, "pub", pt, pn);
	OSSL_PARAM *p = OSSL_PARAM_BLD_to_param(b);
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL), *k = NULL;
	if (!(c && EVP_PKEY_fromdata_init(c) == 1 && EVP_PKEY_fromdata(c, &k, EVP_PKEY_KEYPAIR, p) == 1)) k = NULL;
	EVP_PKEY_CTX_free(c);
	OSSL_PARAM_free(p);
	OSSL_PARAM_BLD_free(b);
	BN_clear_free(bn);
	EC_POINT_free(pnt);
	EC_GROUP_free(g);
	return k;
}
static X *ec_publica(const char *grupo, const uint8_t *pt, size_t n)
{
	OSSL_PARAM p[] = { UTF8("group", grupo), OCT("pub", pt, n), OSSL_PARAM_construct_end() };
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL), *k = NULL;
	if (!(c && EVP_PKEY_fromdata_init(c) == 1 && EVP_PKEY_fromdata(c, &k, EVP_PKEY_PUBLIC_KEY, p) == 1)) k = NULL;
	EVP_PKEY_CTX_free(c);
	return k;
}
/* como openssl.rs: la firma en DER pasa a r||s y de vuelta */
static int ecdsa_firma(X *k, const uint8_t *h, size_t hn, uint8_t *rs, size_t tam)
{
	X *c = EVP_PKEY_CTX_new_from_pkey(NULL, k, NULL);
	uint8_t der[200];
	size_t n = 0;
	int ok = c && EVP_PKEY_sign_init(c) == 1 && EVP_PKEY_sign(c, NULL, &n, h, hn) == 1 && n <= sizeof der && EVP_PKEY_sign(c, der, &n, h, hn) == 1;
	EVP_PKEY_CTX_free(c);
	if (!ok) return 0;
	const uint8_t *q = der;
	X *s = d2i_ECDSA_SIG(NULL, &q, (long)n);
	const X *r = NULL, *ss = NULL;
	if (!s) return 0;
	ECDSA_SIG_get0(s, &r, &ss);
	ok = BN_bn2binpad(r, rs, (int)tam) == (int)tam && BN_bn2binpad(ss, rs + tam, (int)tam) == (int)tam;
	ECDSA_SIG_free(s);
	return ok;
}
static int ecdsa_comprueba(X *k, const uint8_t *h, size_t hn, const uint8_t *rs, size_t tam)
{
	X *s = ECDSA_SIG_new(), *r = BN_bin2bn(rs, (int)tam, NULL), *ss = BN_bin2bn(rs + tam, (int)tam, NULL);
	ECDSA_SIG_set0(s, r, ss);
	uint8_t der[200], *q = der;
	int n = i2d_ECDSA_SIG(s, NULL);
	int ok = n > 0 && n <= (int)sizeof der && i2d_ECDSA_SIG(s, &q) == n;
	ECDSA_SIG_free(s);
	X *c = EVP_PKEY_CTX_new_from_pkey(NULL, k, NULL);
	ok = ok && c && EVP_PKEY_verify_init(c) == 1 && EVP_PKEY_verify(c, der, (size_t)n, h, hn) == 1;
	EVP_PKEY_CTX_free(c);
	return ok;
}
static int ecdh(X *a, X *b, uint8_t *z, size_t *zn)
{
	X *c = EVP_PKEY_CTX_new_from_pkey(NULL, a, NULL);
	int ok = c && EVP_PKEY_derive_init(c) == 1 && EVP_PKEY_derive_set_peer_ex(c, b, 1) == 1 && EVP_PKEY_derive(c, NULL, zn) == 1 && EVP_PKEY_derive(c, z, zn) == 1;
	EVP_PKEY_CTX_free(c);
	return ok;
}
static void claves_ec(void)
{
	uint8_t h[64], rs[132];
	size_t hn = 0;
	/* RFC 6979, A.2.5 y A.2.6: con «sample», BearSSL da la misma firma (el mismo número) */
	X *k = ec_de_escalar(415, "P-256", "c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721");
	X *qx = NULL;
	prueba(k && EVP_PKEY_get_bn_param(k, "qx", &qx) == 1, "P-256: la clave de su escalar (RFC 6979, A.2.5)");
	uint8_t xb[32];
	prueba(qx && BN_bn2binpad(qx, xb, 32) == 32 && igual_hex(xb, 32, "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"), "P-256: su punto, el de la RFC");
	BN_clear_free(qx);
	EVP_Q_digest(NULL, "SHA2-256", NULL, "sample", 6, h, &hn);
	prueba(k && ecdsa_firma(k, h, hn, rs, 32)
		&& igual_hex(rs, 64, "efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8"),
		"ECDSA P-256 con SHA-256: la firma de RFC 6979");
	prueba(k && ecdsa_comprueba(k, h, hn, rs, 32), "ECDSA P-256: comprobada");
	rs[40] ^= 1;
	prueba(k && !ecdsa_comprueba(k, h, hn, rs, 32), "ECDSA P-256 cambiada: no vale");
	prueba(k && comprueba(k, 1, 1), "P-256: las comprobaciones (pública, privada y la pareja)");
	EVP_PKEY_free(k);
	k = ec_de_escalar(715, "P-384", "6b9d3dad2e1b8c1c05b19875b6659f4de23c3b667bf297ba9aa47740787137d896d5724e4c70a825f872c9ea60d2edf5");
	EVP_Q_digest(NULL, "SHA2-384", NULL, "sample", 6, h, &hn);
	prueba(k && ecdsa_firma(k, h, hn, rs, 48)
		&& igual_hex(rs, 96, "94edbb92a5ecb8aad4736e56c691916b3f88140666ce9fa73d64c4ea95ad133c81a648152e44acf96e36dd1e80fabe4699ef4aeb15f178cea1fe40db2603138f130e740a19624526203b6351d0a3a94fa329c145786e679e7b82c71a38628ac8"),
		"ECDSA P-384 con SHA-384: la firma de RFC 6979 (A.2.6)");
	EVP_PKEY_free(k);
	/* P-521: la firma de la otra implementación */
	k = ec_publica("P-521", P521_PUB, sizeof P521_PUB);
	EVP_Q_digest(NULL, "SHA2-512", NULL, RSA_MSG, sizeof RSA_MSG, h, &hn);
	prueba(k && ecdsa_comprueba(k, h, hn, P521_FIRMA, 66), "ECDSA P-521 con SHA-512: la firma de la otra implementación, comprobada");
	EVP_PKEY_free(k);
	/* ECDH, NIST CAVP (KAS ECC CDH, P-256, COUNT = 0) */
	uint8_t q[65], z[66];
	q[0] = 4;
	hex("700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287db71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac", q + 1);
	X *par = ec_publica("P-256", q, 65);
	k = ec_de_escalar(415, "P-256", "7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534");
	size_t zn = sizeof z;
	prueba(par && k && ecdh(k, par, z, &zn) && zn == 32 && igual_hex(z, 32, "46fc62106420ff012e54a434fbdd2d25ccc5852060561e68040dd7778997bd7b"), "ECDH P-256 (NIST CAVP)");
	q[64] ^= 1;
	prueba(!ec_publica("P-256", q, 65), "P-256: un punto que no está en la curva, fuera");
	EVP_PKEY_free(par);
	EVP_PKEY_free(k);
	/* el punto comprimido */
	k = ec_publica("P-256", P256_COMPR, sizeof P256_COMPR);
	X *qy = NULL;
	uint8_t yb[32];
	prueba(k && EVP_PKEY_get_bn_param(k, "qy", &qy) == 1 && BN_bn2binpad(qy, yb, 32) == 32 && !memcmp(yb, P256_SINCOMPR + 33, 32), "P-256: el punto comprimido, descomprimido");
	BN_clear_free(qy);
	EVP_PKEY_free(k);
	/* PKCS #8 y SPKI de P-384 */
	k = de_p8(P384_P8, sizeof P384_P8);
	prueba(k && spki_igual(k, P384_SPKI, sizeof P384_SPKI) && comprueba(k, 1, 1), "P-384: PKCS #8 leído; su SPKI, igual que el de la otra implementación");
	EVP_PKEY_free(k);
	/* generar, firmar y derivar, P-521 */
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL), *a = NULL, *b = NULL;
	OSSL_PARAM gp[] = { UTF8("group", "P-521"), OSSL_PARAM_construct_end() };
	prueba(c && EVP_PKEY_keygen_init(c) == 1 && EVP_PKEY_CTX_set_params(c, gp) == 1 && EVP_PKEY_generate(c, &a) == 1 && EVP_PKEY_generate(c, &b) == 1,
		"P-521: generar dos");
	uint8_t z2[66];
	size_t z2n = sizeof z2;
	zn = sizeof z;
	prueba(a && b && ecdh(a, b, z, &zn) && ecdh(b, a, z2, &z2n) && zn == 66 && z2n == 66 && !memcmp(z, z2, 66), "ECDH P-521: los dos lados, lo mismo");
	EVP_Q_digest(NULL, "SHA2-512", NULL, "x", 1, h, &hn);
	prueba(a && ecdsa_firma(a, h, hn, rs, 66) && ecdsa_comprueba(a, h, hn, rs, 66), "ECDSA P-521: firmar y comprobar");
	EVP_PKEY_free(a);
	EVP_PKEY_free(b);
	EVP_PKEY_CTX_free(c);
}
static void claves_25519(void)
{
	uint8_t sk[32], pk[32], f[64], b[32];
	size_t n = 32, fn = 64;
	hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60", sk);
	X *k = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, sk, 32);
	prueba(k && EVP_PKEY_get_raw_public_key(k, pk, &n) == 1 && n == 32 && igual_hex(pk, 32, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a"), "Ed25519: la clave pública (RFC 8032, 7.1, prueba 1)");
	OSSL_PARAM fin[] = { OSSL_PARAM_construct_end() };
	prueba(k && firma_digest(k, NULL, fin, (const uint8_t *)"", 0, f, &fn) && fn == 64
		&& igual_hex(f, 64, "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"), "Ed25519: la firma de la prueba 1");
	X *p = EVP_PKEY_new_raw_public_key_ex(NULL, "ED25519", NULL, pk, 32);
	prueba(p && comprueba_digest(p, NULL, fin, (const uint8_t *)"", 0, f, 64), "Ed25519: comprobada");
	f[0] ^= 1;
	prueba(p && !comprueba_digest(p, NULL, fin, (const uint8_t *)"", 0, f, 64), "Ed25519 cambiada: no vale");
	EVP_PKEY_free(k);
	EVP_PKEY_free(p);
	hex("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb", sk);
	k = EVP_PKEY_new_raw_private_key_ex(NULL, "ED25519", NULL, sk, 32);
	fn = 64;
	prueba(k && firma_digest(k, NULL, fin, (const uint8_t *)"\x72", 1, f, &fn)
		&& igual_hex(f, 64, "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"), "Ed25519: la prueba 2");
	uint8_t *der = NULL;
	int l = i2d_PUBKEY(k, &der);
	prueba(l == 44 && der && der[0] == 0x30 && !memcmp(der + 12, "\x3d\x40\x17", 3), "Ed25519: su SPKI (RFC 8410)");
	X *k2 = der ? de_spki(der, (size_t)l) : NULL;
	n = 32;
	prueba(k2 && EVP_PKEY_get_raw_public_key(k2, b, &n) == 1 && igual_hex(b, 32, "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c"), "Ed25519: el SPKI, leído");
	free(der);
	EVP_PKEY_free(k2);
	EVP_PKEY_free(k);
	/* X25519 (RFC 7748, 6.1) */
	hex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a", sk);
	X *a = EVP_PKEY_new_raw_private_key_ex(NULL, "X25519", NULL, sk, 32);
	hex("de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f", pk);
	X *bp = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL, pk, 32);
	n = 32;
	prueba(a && EVP_PKEY_get_raw_public_key(a, b, &n) == 1 && igual_hex(b, 32, "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a"), "X25519: la pública de Alicia (RFC 7748, 6.1)");
	uint8_t z[32];
	size_t zn = 32;
	prueba(a && bp && ecdh(a, bp, z, &zn) && zn == 32 && igual_hex(z, 32, "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742"), "X25519: el secreto compartido");
	memset(pk, 0, 32);
	X *cero = EVP_PKEY_new_raw_public_key_ex(NULL, "X25519", NULL, pk, 32);
	zn = 32;
	prueba(cero && !ecdh(a, cero, z, &zn), "X25519 con un punto de orden pequeño (todo ceros): falla, como OpenSSL");
	EVP_PKEY_free(cero);
	EVP_PKEY_free(a);
	EVP_PKEY_free(bp);
	/* generar */
	X *c = EVP_PKEY_CTX_new_from_name(NULL, "ED25519", NULL), *g = NULL;
	prueba(c && EVP_PKEY_keygen_init(c) == 1 && EVP_PKEY_CTX_set_params(c, fin) == 1 && EVP_PKEY_generate(c, &g) == 1, "Ed25519: generar");
	fn = 64;
	prueba(g && firma_digest(g, NULL, fin, (const uint8_t *)"abc", 3, f, &fn) && comprueba_digest(g, NULL, fin, (const uint8_t *)"abc", 3, f, 64), "Ed25519 generada: firmar y comprobar");
	EVP_PKEY_free(g);
	EVP_PKEY_CTX_free(c);
}
static void lo_que_no_hay(void)
{
	prueba(!EVP_PKEY_CTX_new_from_name(NULL, "ED448", NULL) && !EVP_PKEY_CTX_new_from_name(NULL, "X448", NULL)
		&& !EVP_PKEY_CTX_new_from_name(NULL, "ML-DSA-44", NULL) && !EVP_PKEY_CTX_new_from_name(NULL, "ML-KEM-768", NULL)
		&& !EVP_SIGNATURE_fetch(NULL, "ML-DSA-65", NULL) && !EVP_PKEY_new_raw_private_key_ex(NULL, "ED448", NULL, (const uint8_t *)"0123456789", 10),
		"Ed448, X448, ML-DSA y ML-KEM: no hay (nulo, como OpenSSL cuando no puede)");
	/* el azar, los números y el orden de la curva */
	uint8_t a[64], b[64];
	prueba(RAND_bytes(a, 64) == 1 && RAND_bytes(b, 64) == 1 && memcmp(a, b, 64), "RAND_bytes");
	X *x = BN_bin2bn((const uint8_t *)"\x00\xff\xff", 3, NULL), *y = BN_new();
	uint8_t s[4];
	prueba(x && BN_num_bits(x) == 16 && BN_add(y, x, BN_value_one()) == 1 && BN_num_bits(y) == 17 && BN_bn2binpad(y, s, 4) == 4
		&& !memcmp(s, "\x00\x01\x00\x00", 4) && BN_cmp(x, y) < 0 && BN_bn2binpad(y, s, 2) < 0, "BN: bin2bn, add, num_bits, cmp, bn2binpad");
	BN_clear_free(x);
	BN_clear_free(y);
	X *g = EC_GROUP_new_by_curve_name(716);
	uint8_t o[66];
	prueba(g && BN_bn2binpad(EC_GROUP_get0_order(g), o, 66) == 66 && o[0] == 0x01 && o[65] == 0x09, "EC_GROUP: el orden de P-521");
	EC_GROUP_free(g);
}

int main(void)
{
	printf("MCRIPTO: la WebCrypto de Hispa-64 (las funciones de OpenSSL 3 de LibCrypto, sobre BearSSL y monocypher)\n");
	resumenes();
	cifrados();
	derivaciones();
	claves_rsa();
	claves_ec();
	claves_25519();
	lo_que_no_hay();
	if (mal) {
		printf("MCRIPTO: %d mal, %d bien\n", mal, bien);
		return 1;
	}
	printf("MCRIPTO: todo bien (%d pruebas)\n", bien);
	return 0;
}
