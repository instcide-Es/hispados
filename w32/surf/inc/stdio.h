/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_STDIO_H
#define HS_STDIO_H
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
typedef struct HS_FILE FILE;
typedef long long fpos_t;
FILE *__acrt_iob_func(unsigned);
#define stdin (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))
#define EOF (-1)
#define BUFSIZ 512
#define FILENAME_MAX 260
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IONBF 4
#define _IOLBF 64
#define _IOFBF 0
#define L_tmpnam 14
FILE *fopen(const char *, const char *); FILE *freopen(const char *, const char *, FILE *);
int fclose(FILE *); size_t fread(void *, size_t, size_t, FILE *); size_t fwrite(const void *, size_t, size_t, FILE *);
char *fgets(char *, int, FILE *); int fputs(const char *, FILE *); int fputc(int, FILE *); int fgetc(FILE *);
int getc(FILE *); int putc(int, FILE *); int ungetc(int, FILE *); int getchar(void); int putchar(int); int puts(const char *);
int printf(const char *, ...); int fprintf(FILE *, const char *, ...); int vfprintf(FILE *, const char *, va_list);
int vprintf(const char *, va_list);
int sprintf(char *, const char *, ...); int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list); int vsprintf(char *, const char *, va_list);
int sscanf(const char *, const char *, ...); int fscanf(FILE *, const char *, ...); int vsscanf(const char *, const char *, va_list);
int fseek(FILE *, long, int); long ftell(FILE *); void rewind(FILE *); int feof(FILE *); int ferror(FILE *);
void clearerr(FILE *); int fflush(FILE *); int remove(const char *); int rename(const char *, const char *);
void perror(const char *); int setvbuf(FILE *, char *, int, size_t); void setbuf(FILE *, char *);
FILE *tmpfile(void); char *tmpnam(char *); int _fileno(FILE *);
#define fileno _fileno
int fgetpos(FILE *, fpos_t *); int fsetpos(FILE *, const fpos_t *);
int vasprintf(char **, const char *, va_list); int asprintf(char **, const char *, ...);
ssize_t getline(char **, size_t *, FILE *);
FILE *fdopen(int, const char *);
#endif
