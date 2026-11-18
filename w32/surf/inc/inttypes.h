/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_INTTYPES_H
#define HS_INTTYPES_H
#include <stdint.h>
#define PRId8 "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"
#define PRIi32 "i"
#define PRIi64 "lli"
#define PRIu8 "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"
#define PRIx8 "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"
#define PRIX32 "X"
#define PRIX64 "llX"
#define PRIxPTR "x"
#define PRIuPTR "u"
#define PRIdPTR "d"
#define SCNu32 "u"
#define SCNd32 "d"
#define SCNx32 "x"
#define SCNu64 "llu"
#define SCNd64 "lld"
#define SCNx64 "llx"
long long strtoll(const char *, char **, int);
unsigned long long strtoull(const char *, char **, int);
#define strtoimax strtoll
#define strtoumax strtoull
#endif
