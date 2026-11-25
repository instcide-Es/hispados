/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/cxx/puente.h - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
 * sección 16): lo que las cabeceras de la libstdc++ del sistema (la de GCC,
 * configurada para glibc) esperan de la biblioteca de C y musl no tiene:
 * __GLIBC_PREREQ (como la 2.34, sin *f128), __locale_t, los bits de <ctype.h> de glibc (_ISupper...:
 * la tabla de __ctype_b_loc de musl es la misma) y las esperas con el reloj
 * elegido de glibc 2.30 (pthread_cond_clockwait...: puente.c). Lo pone
 * cxx.sh delante de todo (-include). */
#ifndef HISPA64_PUENTE_H
#define HISPA64_PUENTE_H
/* (como si fuera la glibc 2.34 —todo pthread en la biblioteca de C, que es lo
 * de musl: sin los símbolos débiles—, pero sin las funciones *f128 de la 2.26) */
#define __GLIBC_PREREQ(a, b) ((a) == 2 && ((b) == 15 || (b) == 23 || (b) == 27 || (b) == 34))
#include <locale.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
typedef locale_t __locale_t;
#define _ISbit(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
enum { _ISupper = _ISbit(0), _ISlower = _ISbit(1), _ISalpha = _ISbit(2), _ISdigit = _ISbit(3), _ISxdigit = _ISbit(4), _ISspace = _ISbit(5),
       _ISprint = _ISbit(6), _ISgraph = _ISbit(7), _ISblank = _ISbit(8), _IScntrl = _ISbit(9), _ISpunct = _ISbit(10), _ISalnum = _ISbit(11) };
#ifdef __cplusplus
extern "C" {
#endif
int pthread_cond_clockwait(pthread_cond_t *, pthread_mutex_t *, clockid_t, const struct timespec *);
int pthread_mutex_clocklock(pthread_mutex_t *, clockid_t, const struct timespec *);
int pthread_rwlock_clockrdlock(pthread_rwlock_t *, clockid_t, const struct timespec *);
int pthread_rwlock_clockwrlock(pthread_rwlock_t *, clockid_t, const struct timespec *);
int sem_clockwait(sem_t *, clockid_t, const struct timespec *);
#ifdef __cplusplus
}
#endif
#endif
