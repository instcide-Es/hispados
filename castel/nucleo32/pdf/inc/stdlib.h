/* stdlib.h mínimo para Hispa-PDF en Hispa-Pentium (sin biblioteca de C: -ffreestanding).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_STDLIB_H
#define HPDF_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *realloc(void *, size_t);
void *calloc(size_t, size_t);
void free(void *);
int abs(int);
#endif
