/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_STRING_H
#define HS_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t); void *memmove(void *, const void *, size_t); void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t); void *memchr(const void *, int, size_t);
void *memrchr(const void *, int, size_t); void *memmem(const void *, size_t, const void *, size_t);
size_t strlen(const char *); size_t strnlen(const char *, size_t);
char *strcpy(char *, const char *); char *strncpy(char *, const char *, size_t);
char *strcat(char *, const char *); char *strncat(char *, const char *, size_t);
int strcmp(const char *, const char *); int strncmp(const char *, const char *, size_t); int strcoll(const char *, const char *);
char *strchr(const char *, int); char *strrchr(const char *, int); char *strstr(const char *, const char *);
char *strcasestr(const char *, const char *);
size_t strspn(const char *, const char *); size_t strcspn(const char *, const char *); char *strpbrk(const char *, const char *);
char *strtok(char *, const char *); char *strtok_r(char *, const char *, char **);
char *strdup(const char *); char *strndup(const char *, size_t); char *strerror(int);
char *strchrnul(const char *, int);
size_t strlcpy(char *, const char *, size_t); size_t strlcat(char *, const char *, size_t);
char *stpcpy(char *, const char *);
char *strsep(char **, const char *);
#include <strings.h>
#endif
