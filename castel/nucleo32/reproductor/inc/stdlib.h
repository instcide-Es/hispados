/* stdlib.h mínimo para el Reproductor en Hispa-Pentium (-ffreestanding). Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_STDLIB_H
#define REPRO_STDLIB_H
#include <stddef.h>
void *malloc(size_t);
void *realloc(void *, size_t);
void *calloc(size_t, size_t);
void free(void *);
int abs(int);
long labs(long);
int atoi(const char *);
long strtol(const char *, char **, int);
int rand(void);
void srand(unsigned);
#define RAND_MAX 0x7FFFFFFF
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
#define alloca __builtin_alloca
#endif
