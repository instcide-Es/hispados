/* Cabecera mínima para compilar QuickJS (terceros/quickjs) sin la biblioteca
 * del sistema, para HSPJS.DLL de Hispa-486 (w32/js, docs/486/NAVEGADOR.md).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HJ_STDIO_H
#define HJ_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct HJ_FILE FILE;
extern FILE *hj_stdout, *hj_stderr;
#define stdout hj_stdout
#define stderr hj_stderr
#define EOF (-1)
int printf(const char *, ...);
int fprintf(FILE *, const char *, ...);
int vfprintf(FILE *, const char *, va_list);
int sprintf(char *, const char *, ...);
int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int fputc(int, FILE *);
int putchar(int);
int fputs(const char *, FILE *);
int puts(const char *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fflush(FILE *);
#endif
