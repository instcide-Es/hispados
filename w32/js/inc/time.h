/* Cabecera mínima para compilar QuickJS (terceros/quickjs) sin la biblioteca
 * del sistema, para HSPJS.DLL de Hispa-486 (w32/js, docs/486/NAVEGADOR.md).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HJ_TIME_H
#define HJ_TIME_H
#include <stddef.h>
typedef long long time_t;
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; long tm_gmtoff; const char *tm_zone; };
struct timespec { time_t tv_sec; long tv_nsec; };
time_t time(time_t *);
struct tm *localtime_r(const time_t *, struct tm *);
size_t strftime(char *, size_t, const char *, const struct tm *);
#endif
