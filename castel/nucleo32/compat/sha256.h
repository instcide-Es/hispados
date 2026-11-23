/* sha256.h - (COMPAT) SHA-256 (FIPS 180-4), para las sumas de los .EXE y
 * las .DLL del informe y de lo que se baja. C99 portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_SHA256_H
#define COMPAT_SHA256_H
#include "so.h"
typedef struct { u32 h[8]; u8 b[64]; u32 n; u64 total; } sha256;
void sha_empieza(sha256 *s);
void sha_anade(sha256 *s, const void *d, u32 n);
void sha_acaba(sha256 *s, u8 r[32]);
void sha_hex(const u8 r[32], char d[65]);
#endif
