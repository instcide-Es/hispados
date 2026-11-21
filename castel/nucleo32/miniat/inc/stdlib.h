/* stdlib.h mínimo para MINIAT.BIN (stb_image sin biblioteca de C:
 * -ffreestanding): la memoria es la de miniat.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_STDLIB_H
#define HISPA_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *realloc(void *, size_t);
void free(void *);
int abs(int);
#endif
