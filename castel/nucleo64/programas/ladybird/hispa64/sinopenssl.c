/*
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/sinopenssl.c - Hispa-64, etapa 10 g (docs/64/HISPA-64.md, sección
 * 24): lo de OpenSSL 3 que nombran Ladybird y libcrypto_rust (la parte de
 * Rust de LibCrypto, Libraries/LibCrypto/Rust/src/sys.rs: la usan la
 * WebCrypto de LibWeb y LibDNS) y que Hispa-64 no lleva: ni OpenSSL (la red
 * cifrada va por curl con BearSSL; la LibCrypto de C++ es la nuestra,
 * hispa64/LibCrypto).
 * Lo que tiene que ir bien, va: RAND_bytes (getrandom), OPENSSL_cleanse
 * (borrar sin que el compilador lo quite), los OSSL_PARAM_construct_* (solo
 * llenan una estructura, que se devuelve por valor), OSSL_set_max_threads.
 * Lo demás falla como falla OpenSSL cuando no puede: nulo o 0 (y nada en las
 * que sueltan o borran errores), así que lo que lo use (la WebCrypto, DNSSEC)
 * dice que no puede. Las demás van sin su prototipo (este archivo no incluye
 * las cabeceras de OpenSSL, que no hay): todas devuelven 0 en rax, que es
 * nulo, 0 o falso para cualquiera de sus tipos (punteros, int, size_t).
 * La lista, la de sys.rs y las que piden RequestServer y WebContent.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/random.h>

typedef struct {
	const char *key;
	unsigned int data_type;
	void *data;
	size_t data_size;
	size_t return_size;
} OSSL_PARAM;
#define OSSL_PARAM_INTEGER 1
#define OSSL_PARAM_UNSIGNED_INTEGER 2
#define OSSL_PARAM_UTF8_STRING 4
#define OSSL_PARAM_OCTET_STRING 5
#define OSSL_PARAM_UNMODIFIED ((size_t)-1)

static OSSL_PARAM param(const char *k, unsigned int t, void *d, size_t n)
{
	OSSL_PARAM p = { k, t, d, n, OSSL_PARAM_UNMODIFIED };
	return p;
}
OSSL_PARAM OSSL_PARAM_construct_octet_string(const char *k, void *d, size_t n) { return param(k, OSSL_PARAM_OCTET_STRING, d, n); }
OSSL_PARAM OSSL_PARAM_construct_utf8_string(const char *k, char *d, size_t n) { return param(k, OSSL_PARAM_UTF8_STRING, d, n ? n : (d ? strlen(d) : 0)); }
OSSL_PARAM OSSL_PARAM_construct_uint(const char *k, unsigned int *d) { return param(k, OSSL_PARAM_UNSIGNED_INTEGER, d, sizeof *d); }
OSSL_PARAM OSSL_PARAM_construct_int(const char *k, int *d) { return param(k, OSSL_PARAM_INTEGER, d, sizeof *d); }
OSSL_PARAM OSSL_PARAM_construct_end(void) { OSSL_PARAM p = { 0, 0, 0, 0, 0 }; return p; }

int RAND_bytes(unsigned char *b, int n)
{
	while (n > 0) {
		ssize_t r = getrandom(b, (size_t)n, 0);
		if (r <= 0) return 0;
		b += r;
		n -= (int)r;
	}
	return 1;
}
void OPENSSL_cleanse(void *p, size_t n)
{
	volatile unsigned char *q = p;
	while (n--) *q++ = 0;
}
int OSSL_set_max_threads(void *ctx, uint64_t max_threads) { (void)ctx; (void)max_threads; return 1; }

/* lo que falla (nulo, 0) o no hace nada */
#define NO_HAY(f) long f(void); long f(void) { return 0; }
NO_HAY(BN_CTX_free)
NO_HAY(BN_CTX_new)
NO_HAY(BN_add)
NO_HAY(BN_bin2bn)
NO_HAY(BN_bn2binpad)
NO_HAY(BN_clear_free)
NO_HAY(BN_cmp)
NO_HAY(BN_is_odd)
NO_HAY(BN_new)
NO_HAY(BN_num_bits)
NO_HAY(BN_secure_new)
NO_HAY(BN_value_one)
NO_HAY(ECDSA_SIG_free)
NO_HAY(ECDSA_SIG_get0)
NO_HAY(ECDSA_SIG_new)
NO_HAY(ECDSA_SIG_set0)
NO_HAY(EC_GROUP_free)
NO_HAY(EC_GROUP_get0_order)
NO_HAY(EC_GROUP_new_by_curve_name)
NO_HAY(EC_POINT_free)
NO_HAY(EC_POINT_mul)
NO_HAY(EC_POINT_new)
NO_HAY(EC_POINT_point2oct)
NO_HAY(ERR_clear_error)
NO_HAY(ERR_print_errors_cb)
NO_HAY(EVP_CIPHER_CTX_ctrl)
NO_HAY(EVP_CIPHER_CTX_free)
NO_HAY(EVP_CIPHER_CTX_get_iv_length)
NO_HAY(EVP_CIPHER_CTX_get_key_length)
NO_HAY(EVP_CIPHER_CTX_new)
NO_HAY(EVP_CIPHER_CTX_set_flags)
NO_HAY(EVP_CIPHER_get_iv_length)
NO_HAY(EVP_CIPHER_get_key_length)
NO_HAY(EVP_CipherFinal_ex)
NO_HAY(EVP_CipherInit_ex)
NO_HAY(EVP_CipherUpdate)
NO_HAY(EVP_DigestFinalXOF)
NO_HAY(EVP_DigestInit_ex)
NO_HAY(EVP_DigestSign)
NO_HAY(EVP_DigestSignInit_ex)
NO_HAY(EVP_DigestUpdate)
NO_HAY(EVP_DigestVerify)
NO_HAY(EVP_DigestVerifyInit_ex)
NO_HAY(EVP_KDF_CTX_free)
NO_HAY(EVP_KDF_CTX_new)
NO_HAY(EVP_KDF_derive)
NO_HAY(EVP_KDF_fetch)
NO_HAY(EVP_KDF_free)
NO_HAY(EVP_MAC_CTX_free)
NO_HAY(EVP_MAC_CTX_new)
NO_HAY(EVP_MAC_fetch)
NO_HAY(EVP_MAC_final)
NO_HAY(EVP_MAC_free)
NO_HAY(EVP_MAC_init)
NO_HAY(EVP_MAC_update)
NO_HAY(EVP_MD_CTX_free)
NO_HAY(EVP_MD_CTX_new)
NO_HAY(EVP_MD_fetch)
NO_HAY(EVP_MD_free)
NO_HAY(EVP_PKCS82PKEY)
NO_HAY(EVP_PKEY_CTX_free)
NO_HAY(EVP_PKEY_CTX_new_from_name)
NO_HAY(EVP_PKEY_CTX_new_from_pkey)
NO_HAY(EVP_PKEY_CTX_set_params)
NO_HAY(EVP_PKEY_decapsulate)
NO_HAY(EVP_PKEY_decapsulate_init)
NO_HAY(EVP_PKEY_decrypt)
NO_HAY(EVP_PKEY_decrypt_init_ex)
NO_HAY(EVP_PKEY_derive)
NO_HAY(EVP_PKEY_derive_init)
NO_HAY(EVP_PKEY_derive_set_peer_ex)
NO_HAY(EVP_PKEY_encapsulate)
NO_HAY(EVP_PKEY_encapsulate_init)
NO_HAY(EVP_PKEY_encrypt)
NO_HAY(EVP_PKEY_encrypt_init_ex)
NO_HAY(EVP_PKEY_free)
NO_HAY(EVP_PKEY_fromdata)
NO_HAY(EVP_PKEY_fromdata_init)
NO_HAY(EVP_PKEY_generate)
NO_HAY(EVP_PKEY_get_bits)
NO_HAY(EVP_PKEY_get_bn_param)
NO_HAY(EVP_PKEY_get_octet_string_param)
NO_HAY(EVP_PKEY_get_raw_private_key)
NO_HAY(EVP_PKEY_get_raw_public_key)
NO_HAY(EVP_PKEY_get_size)
NO_HAY(EVP_PKEY_keygen_init)
NO_HAY(EVP_PKEY_new_raw_private_key_ex)
NO_HAY(EVP_PKEY_new_raw_public_key_ex)
NO_HAY(EVP_PKEY_pairwise_check)
NO_HAY(EVP_PKEY_private_check)
NO_HAY(EVP_PKEY_public_check)
NO_HAY(EVP_PKEY_sign)
NO_HAY(EVP_PKEY_sign_init)
NO_HAY(EVP_PKEY_sign_message_init)
NO_HAY(EVP_PKEY_up_ref)
NO_HAY(EVP_PKEY_verify)
NO_HAY(EVP_PKEY_verify_init)
NO_HAY(EVP_PKEY_verify_message_init)
NO_HAY(EVP_Q_digest)
NO_HAY(EVP_SIGNATURE_fetch)
NO_HAY(EVP_SIGNATURE_free)
NO_HAY(EVP_aes_128_cbc)
NO_HAY(EVP_aes_128_ctr)
NO_HAY(EVP_aes_128_gcm)
NO_HAY(EVP_aes_128_ocb)
NO_HAY(EVP_aes_128_wrap)
NO_HAY(EVP_aes_192_cbc)
NO_HAY(EVP_aes_192_ctr)
NO_HAY(EVP_aes_192_gcm)
NO_HAY(EVP_aes_192_ocb)
NO_HAY(EVP_aes_192_wrap)
NO_HAY(EVP_aes_256_cbc)
NO_HAY(EVP_aes_256_ctr)
NO_HAY(EVP_aes_256_gcm)
NO_HAY(EVP_aes_256_ocb)
NO_HAY(EVP_aes_256_wrap)
NO_HAY(EVP_chacha20_poly1305)
NO_HAY(OBJ_get0_data)
NO_HAY(OBJ_length)
NO_HAY(OSSL_PARAM_BLD_free)
NO_HAY(OSSL_PARAM_BLD_new)
NO_HAY(OSSL_PARAM_BLD_push_BN)
NO_HAY(OSSL_PARAM_BLD_push_octet_string)
NO_HAY(OSSL_PARAM_BLD_push_uint)
NO_HAY(OSSL_PARAM_BLD_push_utf8_string)
NO_HAY(OSSL_PARAM_BLD_to_param)
NO_HAY(OSSL_PARAM_free)
NO_HAY(PKCS8_PRIV_KEY_INFO_free)
NO_HAY(PKCS8_pkey_get0)
NO_HAY(X509_ALGOR_get0)
NO_HAY(X509_PUBKEY_free)
NO_HAY(X509_PUBKEY_get)
NO_HAY(X509_PUBKEY_get0_param)
NO_HAY(X509_free)
NO_HAY(d2i_ECDSA_SIG)
NO_HAY(d2i_PKCS8_PRIV_KEY_INFO)
NO_HAY(d2i_X509_PUBKEY)
NO_HAY(i2d_ECDSA_SIG)
NO_HAY(i2d_PKCS8_PRIV_KEY_INFO)
NO_HAY(i2d_PUBKEY)
NO_HAY(i2d_PrivateKey)
NO_HAY(i2d_X509_PUBKEY)
NO_HAY(is_end)
NO_HAY(OPENSSL_version_major)
NO_HAY(OPENSSL_version_minor)
NO_HAY(OPENSSL_version_patch)
