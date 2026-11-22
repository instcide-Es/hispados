/* stdio.h de Correo en Hispa-Pentium (libc_pentium.c): solo dar formato a
 * textos (%d %u %x %s %c, con ancho, «-», «0», «.*» y «ll»; sin coma flotante).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_STDIO_H
#define CORREO_STDIO_H
#include <stddef.h>
#include <stdarg.h>
int vsnprintf(char *, size_t, const char *, va_list);
int snprintf(char *, size_t, const char *, ...);
#endif
