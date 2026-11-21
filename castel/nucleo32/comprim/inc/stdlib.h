/* stdlib.h de base.c (Comprimidos y Mirador, sin biblioteca de C: -ffreestanding).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_STDLIB_H
#define HISPA_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
int abs(int);
#endif
