/* cripto.h - lo que no trae Monocypher (terceros/monocypher) y hace falta
 * para SSH, WireGuard y Tailscale: SHA-256 y HMAC-SHA-256 (FIPS 180-4, RFC
 * 2104), BLAKE2s (RFC 7693) con su HMAC y el HKDF de Noise, y la «secretbox»
 * de NaCl (XSalsa20-Poly1305: la de DERP). Escrito aquí a partir de las
 * normas (no copiado de ningún sitio).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_CRIPTO_H
#define HISPA_CRIPTO_H
#include "hispa.h"

typedef struct { u32 h[8]; u64 n; u8 b[64]; u32 k; } sha256_ctx;
void sha256_inicia(sha256_ctx *c);
void sha256_mas(sha256_ctx *c, const void *d, u32 n);
void sha256_fin(sha256_ctx *c, u8 r[32]);
void sha256(const void *d, u32 n, u8 r[32]);
void hmac_sha256(const u8 *clave, u32 nc, const void *d, u32 n, u8 r[32]);
void pbkdf2_sha256(const u8 *clave, u32 nc, const u8 *sal, u32 ns, u32 vueltas, u8 r[32]);

typedef struct { u32 h[8], t[2], f; u8 b[64]; u32 k, largo; } blake2s_ctx;
void blake2s_inicia(blake2s_ctx *c, u32 largo, const u8 *clave, u32 nc);
void blake2s_mas(blake2s_ctx *c, const void *d, u32 n);
void blake2s_fin(blake2s_ctx *c, u8 *r);
void blake2s(u8 *r, u32 largo, const u8 *clave, u32 nc, const void *d, u32 n);
void hmac_blake2s(const u8 *clave, u32 nc, const void *d, u32 n, u8 r[32]);
/* el HKDF de Noise con BLAKE2s: de la clave de cadena «ck» y lo de entrada,
 * dos (o tres) salidas de 32 */
void hkdf_blake2s(const u8 ck[32], const u8 *ent, u32 n, u8 *s1, u8 *s2, u8 *s3);

/* NaCl: crypto_secretbox (XSalsa20-Poly1305) y crypto_box con X25519 */
void hsalsa20(u8 r[32], const u8 clave[32], const u8 n16[16]);
void secretbox_cierra(u8 *sale, const u8 *ent, u32 n, const u8 nonce[24], const u8 clave[32]);	/* sale: 16 + n (la marca delante) */
int secretbox_abre(u8 *sale, const u8 *ent, u32 n, const u8 nonce[24], const u8 clave[32]);	/* ent: 16 + datos; 0 si vale */
void box_clave(u8 r[32], const u8 suya[32], const u8 mia_secreta[32]);	/* crypto_box_beforenm */
#endif
