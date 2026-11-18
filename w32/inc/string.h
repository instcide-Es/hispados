/* string.h mínimo para compilar código de C sin biblioteca (-ffreestanding)
 * que se enlaza con MSVCRT.DLL de Hispa-486 (BearSSL solo pide esto).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_STRING_H
#define HISPA_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
#endif
