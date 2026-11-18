/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_REGEX_H
#define HS_REGEX_H
#include <stddef.h>
typedef struct { size_t re_nsub; void *p; } regex_t;
typedef int regoff_t;
typedef struct { regoff_t rm_so, rm_eo; } regmatch_t;
#define REG_EXTENDED 1
#define REG_ICASE 2
#define REG_NOSUB 4
#define REG_NEWLINE 8
#define REG_NOMATCH 1
int regcomp(regex_t *, const char *, int); int regexec(const regex_t *, const char *, size_t, regmatch_t *, int);
void regfree(regex_t *); size_t regerror(int, const regex_t *, char *, size_t);
#endif
