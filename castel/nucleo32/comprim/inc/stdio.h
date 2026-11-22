/* stdio.h de base.c (Comprimidos y Mirador): solo dar formato a textos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_STDIO_H
#define HISPA_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int vsnprintf(char *, size_t, const char *, va_list);
int snprintf(char *, size_t, const char *, ...);
#endif
