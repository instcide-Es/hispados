/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_STDLIB_H
#define HS_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 0x7FFF
#define MB_CUR_MAX 1
void *malloc(size_t); void *calloc(size_t, size_t); void *realloc(void *, size_t); void free(void *);
void abort(void) __attribute__((noreturn)); void exit(int) __attribute__((noreturn)); int atexit(void (*)(void));
void qsort(void *, size_t, size_t, int (*)(const void *, const void *));
void *bsearch(const void *, const void *, size_t, size_t, int (*)(const void *, const void *));
int atoi(const char *); long atol(const char *); double atof(const char *);
long strtol(const char *, char **, int); unsigned long strtoul(const char *, char **, int);
long long strtoll(const char *, char **, int); unsigned long long strtoull(const char *, char **, int);
double strtod(const char *, char **); float strtof(const char *, char **);
char *getenv(const char *); int setenv(const char *, const char *, int); int unsetenv(const char *);
int abs(int); long labs(long); long long llabs(long long);
int rand(void); void srand(unsigned);
long random(void); void srandom(unsigned);
char *realpath(const char *, char *);
int mkstemp(char *);
char *mkdtemp(char *);
int system(const char *);
typedef struct { int quot, rem; } div_t;
div_t div(int, int);
#define alloca __builtin_alloca
#endif
