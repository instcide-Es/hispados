/* Cabecera mínima para compilar QuickJS (terceros/quickjs) sin la biblioteca
 * del sistema, para HSPJS.DLL de Hispa-486 (w32/js, docs/486/NAVEGADOR.md).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HJ_SYS_TIME_H
#define HJ_SYS_TIME_H
#include <time.h>
struct timeval { time_t tv_sec; long tv_usec; };
int gettimeofday(struct timeval *, void *);
#endif
