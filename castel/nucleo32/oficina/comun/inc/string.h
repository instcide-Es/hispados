/* string.h mínimo para la oficina de 32 bits en Hispa-Pentium (sin biblioteca de C: -ffreestanding).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_STRING_H
#define OFI_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
char *strchr(const char *, int);
char *strstr(const char *, const char *);
char *strcpy(char *, const char *);
#endif
