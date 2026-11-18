/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_TIME_H
#define HS_TIME_H
#include <stddef.h>
#ifndef HS_TIME_T
#define HS_TIME_T
typedef long time_t;
#endif
typedef long clock_t;
typedef int clockid_t;
#define CLOCKS_PER_SEC 1000
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; };
struct timespec { time_t tv_sec; long tv_nsec; };
time_t time(time_t *); clock_t clock(void); double difftime(time_t, time_t);
struct tm *gmtime(const time_t *); struct tm *localtime(const time_t *); time_t mktime(struct tm *);
struct tm *gmtime_r(const time_t *, struct tm *); struct tm *localtime_r(const time_t *, struct tm *);
time_t timegm(struct tm *);
size_t strftime(char *, size_t, const char *, const struct tm *);
char *strptime(const char *, const char *, struct tm *);
char *asctime(const struct tm *); char *ctime(const time_t *);
int clock_gettime(clockid_t, struct timespec *);
int nanosleep(const struct timespec *, struct timespec *);
extern long timezone;
void tzset(void);
#endif
