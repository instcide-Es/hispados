/* cifra.h - ZipCrypto y el AES de WinZip (cifra.c), para Comprimidos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CIFRA_H
#define CIFRA_H
#include <stdint.h>
#include <stddef.h>

typedef struct { uint32_t k0, k1, k2; } zc_llaves;
void zc_empieza(zc_llaves *k, const char *clave);
void zc_descifra(zc_llaves *k, uint8_t *p, uint32_t n);

typedef struct { uint8_t w[240]; int vueltas; } aes_ctx;
void aes_llave(aes_ctx *c, const uint8_t *llave, int n);
void aes_cifra(const aes_ctx *c, uint8_t *b);

typedef struct { uint32_t h[5]; uint64_t n; uint8_t buf[64]; uint32_t lleno; } sha1_ctx;
void sha1_empieza(sha1_ctx *c);
void sha1_pon(sha1_ctx *c, const void *p, size_t n);
void sha1_acaba(sha1_ctx *c, uint8_t sale[20]);

typedef struct { sha1_ctx dentro; uint8_t fuera[64]; } hmac_ctx;
void hmac_empieza(hmac_ctx *h, const uint8_t *llave, size_t n);
void hmac_pon(hmac_ctx *h, const void *p, size_t n);
void hmac_acaba(hmac_ctx *h, uint8_t sale[20]);
void pbkdf2_sha1(const char *clave, const uint8_t *sal, size_t sal_n, uint32_t vueltas, uint8_t *sale, size_t n);

typedef struct { aes_ctx aes; hmac_ctx mac; uint8_t cont[16], flujo[16]; int usado; } wz_ctx;
int wz_empieza(wz_ctx *w, int fuerza, const char *clave, const uint8_t *sal, const uint8_t verifica[2]);
void wz_descifra(wz_ctx *w, uint8_t *p, uint32_t n);
int wz_comprueba(wz_ctx *w, const uint8_t firma[10]);
#endif
