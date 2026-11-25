/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/cxx/puente.c - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
 * sección 16): lo que la libstdc++.a del sistema (compilada para glibc)
 * pide al enlazar y musl no tiene, hecho con lo de musl: __libc_single_threaded
 * (a 0: siempre con las operaciones atómicas), las funciones _chk de
 * _FORTIFY_SOURCE (con su comprobación: si se pasa, se para), las de 64 bits
 * de LFS (fopen64...: en musl, off_t ya es de 64), arc4random (getrandom),
 * _dl_find_object (-1: el desenrollador de libgcc busca con
 * dl_iterate_phdr), los destructores de thread_local (__cxa_thread_atexit_impl)
 * y las esperas con reloj de glibc 2.30 (el plazo, pasado
 * a CLOCK_REALTIME). Es el puente mientras no haya una libstdc++ compilada
 * para musl (con las fuentes de GCC: etapa 11). */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/random.h>
#include <stdint.h>
char __libc_single_threaded = 0;
unsigned long __isoc23_strtoul(const char *s, char **f, int b) { return strtoul(s, f, b); }
static void desborda(void) { static const char m[] = "*** desbordamiento de un búfer (_chk) ***\n"; write(2, m, sizeof m - 1); abort(); }
void *__memcpy_chk(void *d, const void *s, size_t n, size_t dn) { if (n > dn) desborda(); return memcpy(d, s, n); }
wchar_t *__wmemcpy_chk(wchar_t *d, const wchar_t *s, size_t n, size_t dn) { if (n > dn) desborda(); return wmemcpy(d, s, n); }
wchar_t *__wmemset_chk(wchar_t *d, wchar_t c, size_t n, size_t dn) { if (n > dn) desborda(); return wmemset(d, c, n); }
size_t __mbsrtowcs_chk(wchar_t *d, const char **s, size_t n, mbstate_t *st, size_t dn) { if (d && n > dn) desborda(); return mbsrtowcs(d, s, n, st); }
size_t __mbsnrtowcs_chk(wchar_t *d, const char **s, size_t m, size_t n, mbstate_t *st, size_t dn) { if (d && n > dn) desborda(); return mbsnrtowcs(d, s, m, n, st); }
ssize_t __read_chk(int fd, void *b, size_t n, size_t bn) { if (n > bn) desborda(); return read(fd, b, n); }
int __sprintf_chk(char *s, int fl, size_t sn, const char *f, ...) { va_list a; int r; (void)fl; va_start(a, f); r = vsnprintf(s, sn, f, a); va_end(a); if (r >= 0 && (size_t)r >= sn) desborda(); return r; }
int __snprintf_chk(char *s, size_t n, int fl, size_t sn, const char *f, ...) { va_list a; int r; (void)fl; if (n > sn) desborda(); va_start(a, f); r = vsnprintf(s, n, f, a); va_end(a); return r; }
int __vsnprintf_chk(char *s, size_t n, int fl, size_t sn, const char *f, va_list a) { (void)fl; if (n > sn) desborda(); return vsnprintf(s, n, f, a); }
struct dl_find_object;
int _dl_find_object(void *pc, struct dl_find_object *r) { (void)pc; (void)r; return -1; }
uint32_t arc4random(void) { uint32_t v = 0; while (getrandom(&v, sizeof v, 0) != sizeof v) ; return v; }
FILE *fopen64(const char *n, const char *m) { return fopen(n, m); }
int fseeko64(FILE *f, off_t o, int w) { return fseeko(f, o, w); }
off_t ftello64(FILE *f) { return ftello(f); }
off_t lseek64(int fd, off_t o, int w) { return lseek(fd, o, w); }
int fstat64(int fd, struct stat *st) { return fstat(fd, st); }
/* las de glibc 2.30 con el reloj elegido: el plazo, pasado a CLOCK_REALTIME */
static struct timespec a_real(clockid_t c, const struct timespec *t)
{
	struct timespec ahora, real, r;
	if (c == CLOCK_REALTIME) return *t;
	clock_gettime(c, &ahora);
	clock_gettime(CLOCK_REALTIME, &real);
	r.tv_sec = real.tv_sec + (t->tv_sec - ahora.tv_sec);
	r.tv_nsec = real.tv_nsec + (t->tv_nsec - ahora.tv_nsec);
	while (r.tv_nsec < 0) { r.tv_nsec += 1000000000; r.tv_sec--; }
	while (r.tv_nsec >= 1000000000) { r.tv_nsec -= 1000000000; r.tv_sec++; }
	return r;
}
int pthread_cond_clockwait(pthread_cond_t *c, pthread_mutex_t *m, clockid_t k, const struct timespec *t) { struct timespec r = a_real(k, t); return pthread_cond_timedwait(c, m, &r); }
int pthread_mutex_clocklock(pthread_mutex_t *m, clockid_t k, const struct timespec *t) { struct timespec r = a_real(k, t); return pthread_mutex_timedlock(m, &r); }
int pthread_rwlock_clockrdlock(pthread_rwlock_t *l, clockid_t k, const struct timespec *t) { struct timespec r = a_real(k, t); return pthread_rwlock_timedrdlock(l, &r); }
int pthread_rwlock_clockwrlock(pthread_rwlock_t *l, clockid_t k, const struct timespec *t) { struct timespec r = a_real(k, t); return pthread_rwlock_timedwrlock(l, &r); }
int sem_clockwait(sem_t *s, clockid_t k, const struct timespec *t) { struct timespec r = a_real(k, t); return sem_timedwait(s, &r); }
/* los destructores de thread_local (__cxa_thread_atexit de la libstdc++):
 * una lista por hilo, en una clave de pthread (al acabar el hilo, del último
 * al primero; los que se apunten mientras, en la vuelta siguiente de musl) */
struct destructor { void (*f)(void *); void *obj; struct destructor *sig; };
static pthread_key_t clave_dtor;
static pthread_once_t una_dtor = PTHREAD_ONCE_INIT;
static void corre_dtor(void *p)
{
	struct destructor *d = p, *s;
	while (d) {
		s = d->sig;
		d->f(d->obj);
		free(d);
		d = s;
	}
}
static void crea_clave(void) { pthread_key_create(&clave_dtor, corre_dtor); }
int __cxa_thread_atexit_impl(void (*f)(void *), void *obj, void *dso)
{
	struct destructor *d;
	(void)dso;
	pthread_once(&una_dtor, crea_clave);
	d = malloc(sizeof *d);
	if (!d) return -1;
	d->f = f;
	d->obj = obj;
	d->sig = pthread_getspecific(clave_dtor);
	pthread_setspecific(clave_dtor, d);
	return 0;
}
/* Los locale_t. La libstdc++ de glibc mira dentro del suyo: ctype<char> toma
 * la tabla de clases de __locale_struct.__ctype_b (y tolower y toupper), que
 * en glibc va detrás de los 13 punteros de sus categorías; el de musl son
 * solo 6 punteros (48 octetos). newlocale y duplocale, envueltas (cxx.sh
 * enlaza con --wrap): dan un bloque con los 6 de musl delante (musl lo usa
 * como suyo: lo cambia en su sitio y lo suelta con free) y, donde los busca
 * glibc, las tablas de musl (__ctype_b_loc y las otras: las mismas tablas
 * que las de glibc) y los nombres. */
#include <locale.h>
#include <ctype.h>
const unsigned short **__ctype_b_loc(void);
const int32_t **__ctype_tolower_loc(void);
const int32_t **__ctype_toupper_loc(void);
struct local_glibc {
	void *cat[13];                  /* los 6 de musl, y lo que no se usa */
	const unsigned short *ctype_b;
	const int32_t *ctype_tolower;
	const int32_t *ctype_toupper;
	const char *nombres[13];
};
locale_t __real_newlocale(int, const char *, locale_t);
locale_t __real___newlocale(int, const char *, locale_t);
#define MAGIA ((void *)0x48697370613634UL)     /* «Hispa64», en cat[12]: es de los de aquí */
static int es_c(const char *n) { return !n || !*n || !strcmp(n, "C") || !strcmp(n, "POSIX"); }
static int es_nuestro(locale_t l) { return l && l != LC_GLOBAL_LOCALE && ((struct local_glibc *)l)->cat[12] == MAGIA; }
static struct local_glibc *nuevo_glibc(locale_t de)
{
	struct local_glibc *g = calloc(1, sizeof *g);
	int i;
	if (!g) return 0;
	if (de == LC_GLOBAL_LOCALE) de = uselocale(0);
	if (de) memcpy(g->cat, de, 6 * sizeof(void *));
	g->cat[12] = MAGIA;
	g->ctype_b = *__ctype_b_loc();
	g->ctype_tolower = *__ctype_tolower_loc();
	g->ctype_toupper = *__ctype_toupper_loc();
	for (i = 0; i < 13; i++) g->nombres[i] = es_nuestro(de) ? ((struct local_glibc *)de)->nombres[i] : "C";
	return g;
}
static locale_t a_glibc(int mascara, const char *nombre, locale_t base, int doble)
{
	struct local_glibc *g;
	locale_t r;
	int i;
	if (es_nuestro(base)) g = (struct local_glibc *)base;       /* (newlocale se queda con la base: en su sitio, como musl) */
	else if (!(g = nuevo_glibc(base))) return 0;
	r = doble ? __real___newlocale(mascara, nombre, (locale_t)g) : __real_newlocale(mascara, nombre, (locale_t)g);
	if (r != (locale_t)g) {             /* (no se ha podido: esa categoría no está) */
		if (g != (struct local_glibc *)base) free(g);
		return 0;
	}
	for (i = 0; i < 13; i++)
		if (mascara & (1 << i)) g->nombres[i] = es_c(nombre) ? "C" : "C.UTF-8";
	return (locale_t)g;
}
locale_t __wrap_newlocale(int m, const char *n, locale_t b) { return a_glibc(m, n, b, 0); }
locale_t __wrap___newlocale(int m, const char *n, locale_t b) { return a_glibc(m, n, b, 1); }
locale_t __wrap___duplocale(locale_t l) { return (locale_t)nuevo_glibc(l); }
locale_t __wrap_duplocale(locale_t l) { return (locale_t)nuevo_glibc(l); }
/* strerror_r: la libstdc++ quiere la de GNU (da el texto); la de musl es la
 * de POSIX (da un número). Envuelta (cxx.sh: --wrap=strerror_r). */
int __real_strerror_r(int, char *, size_t);
char *__wrap_strerror_r(int e, char *b, size_t n)
{
	if (__real_strerror_r(e, b, n) != 0 && n > 0) snprintf(b, n, "error %d", e);
	return b;
}
