/* cuenta.h - para la prueba de woff.c (prueba.c): la memoria que piden woff.c
   y Brotli pasa por aquí para saber el pico. Se pone con -include.
   Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdlib.h>
void *cuenta_malloc(size_t); void *cuenta_calloc(size_t, size_t); void *cuenta_realloc(void *, size_t); void cuenta_free(void *);
#define malloc cuenta_malloc
#define calloc cuenta_calloc
#define realloc cuenta_realloc
#define free cuenta_free
