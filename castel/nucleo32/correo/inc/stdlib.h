/* stdlib.h de Correo en Hispa-Pentium (libc_pentium.c; stb_image lo usa).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_STDLIB_H
#define CORREO_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
int abs(int);
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
#endif
