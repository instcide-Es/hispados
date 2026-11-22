/* so_pentium.c - el Reproductor en Hispa-Pentium: lo de so.h con las
 * llamadas del núcleo (int 0x80; docs/PENTIUM.md, sección 6; el sonido,
 * 190 a 193, §14 decies; el CD de música, la 134; la bandeja, la 135; lo
 * soltado desde Mi equipo, la 136) y lo poco de la biblioteca de C que
 * piden minimp3, stb_vorbis, dr_flac, libxmp-lite y stb_image (sin
 * biblioteca del sistema: -ffreestanding). La memoria, como la de Hispa-PDF
 * (por tamaños; las grandes, de 64 K en 64 K, y la de arriba del todo
 * vuelve al núcleo). La coma flotante, con el x87 (sin FCOMI ni CMOV: el
 * Pentium de verdad).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "so.h"
#include "util.h"

#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFE0u)
#define NO_HAY 0xFFFFFFFFu

static u32 llamada(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* =================================================================== la biblioteca de C */
void *memcpy(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	if ((((u32)a | (u32)b) & 3) == 0) {
		u32 k = (u32)n >> 2;

		__asm__ volatile("rep movsl" : "+D"(a), "+S"(b), "+c"(k) : : "memory");
		n &= 3;
	}
	while (n--)
		*a++ = *b++;
	return d;
}

void *memmove(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	if (a <= b || a >= b + n)
		return memcpy(d, o, n);
	while (n--)
		a[n] = b[n];
	return d;
}

void *memset(void *d, int c, size_t n)
{
	u8 *a = d;

	if (((u32)a & 3) == 0) {
		u32 v = (u8)c * 0x01010101u, k = (u32)n >> 2;

		__asm__ volatile("rep stosl" : "+D"(a), "+c"(k) : "a"(v) : "memory");
		n &= 3;
	}
	while (n--)
		*a++ = (u8)c;
	return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
	const u8 *a = x, *b = y;

	for (; n; n--, a++, b++)
		if (*a != *b)
			return *a - *b;
	return 0;
}

void *memchr(const void *s, int c, size_t n)
{
	const u8 *p = s;

	for (; n; n--, p++)
		if (*p == (u8)c)
			return (void *)p;
	return NULL;
}

size_t strlen(const char *s)
{
	size_t n = 0;

	while (s[n])
		n++;
	return n;
}

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return (u8)*a - (u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (u8)*a - (u8)*b;
		if (!*a)
			return 0;
	}
	return 0;
}

int strcasecmp(const char *a, const char *b)
{
	while (*a && mayus((u8)*a) == mayus((u8)*b))
		a++, b++;
	return mayus((u8)*a) - mayus((u8)*b);
}

int strncasecmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (mayus((u8)*a) != mayus((u8)*b))
			return mayus((u8)*a) - mayus((u8)*b);
		if (!*a)
			return 0;
	}
	return 0;
}

char *strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == (char)c)
			return (char *)s;
	return c ? NULL : (char *)s;
}

char *strrchr(const char *s, int c)
{
	const char *r = NULL;

	for (; *s; s++)
		if (*s == (char)c)
			r = s;
	return c ? (char *)r : (char *)s;
}

char *strstr(const char *a, const char *b)
{
	size_t n = strlen(b);

	for (; *a; a++)
		if (!strncmp(a, b, n))
			return (char *)a;
	return n ? NULL : (char *)a;
}

char *strcpy(char *d, const char *o)
{
	char *r = d;

	while ((*d++ = *o++))
		;
	return r;
}

char *strncpy(char *d, const char *o, size_t n)
{
	size_t i;

	for (i = 0; i < n && o[i]; i++)
		d[i] = o[i];
	for (; i < n; i++)
		d[i] = 0;
	return d;
}

char *strcat(char *d, const char *o)
{
	strcpy(d + strlen(d), o);
	return d;
}

char *strdup(const char *s)
{
	size_t n = strlen(s) + 1;
	char *d = malloc(n);

	if (d)
		memcpy(d, s, n);
	return d;
}

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
int toupper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
int tolower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isalpha(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z'; }
int isalnum(int c) { return isdigit(c) || isalpha(c); }
int isprint(int c) { return c >= 32 && c < 127; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isxdigit(int c) { return isdigit(c) || ((c | 32) >= 'a' && (c | 32) <= 'f'); }
int errno;

long strtol(const char *s, char **fin, int base)
{
	long v = 0;
	int neg = 0;

	while (isspace((u8)*s))
		s++;
	if (*s == '-' || *s == '+')
		neg = *s++ == '-';
	if (!base)
		base = 10;
	for (;; s++) {
		int d = isdigit((u8)*s) ? *s - '0' : isalpha((u8)*s) ? tolower((u8)*s) - 'a' + 10 : 99;

		if (d >= base)
			break;
		v = v * base + d;
	}
	if (fin)
		*fin = (char *)s;
	return neg ? -v : v;
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
int rand(void) { return (int)(azar() & 0x7FFFFFFF); }
void srand(unsigned s) { siembra(s); }

/* qsort: por inserción con el montón (lo usa stb_vorbis al empezar) */
void qsort(void *base, size_t n, size_t t, int (*cmp)(const void *, const void *))
{
	u8 *b = base, tmp[64], *x = t <= sizeof tmp ? tmp : malloc(t);
	size_t i, j;

	if (!x)
		return;
	for (i = 1; i < n; i++) {
		memcpy(x, b + i * t, t);
		for (j = i; j > 0 && cmp(b + (j - 1) * t, x) > 0; j--)
			memcpy(b + j * t, b + (j - 1) * t, t);
		memcpy(b + j * t, x, t);
	}
	if (x != tmp)
		free(x);
}

/* (libxmp lleva lo de cargar de un archivo: aquí nunca se usa) */
FILE *fopen(const char *n, const char *m) { (void)n; (void)m; return NULL; }
int fclose(FILE *f) { (void)f; return 0; }
size_t fread(void *p, size_t a, size_t b, FILE *f) { (void)p; (void)a; (void)b; (void)f; return 0; }
size_t fwrite(const void *p, size_t a, size_t b, FILE *f) { (void)p; (void)a; (void)b; (void)f; return 0; }
int fseek(FILE *f, long o, int w) { (void)f; (void)o; (void)w; return -1; }
long ftell(FILE *f) { (void)f; return -1; }
int fgetc(FILE *f) { (void)f; return -1; }
int ferror(FILE *f) { (void)f; return 1; }
int feof(FILE *f) { (void)f; return 1; }
int fflush(FILE *f) { (void)f; return 0; }
int fprintf(FILE *f, const char *x, ...) { (void)f; (void)x; return 0; }
int fputs(const char *s, FILE *f) { (void)s; (void)f; return 0; }
FILE *stderr, *stdout;
int vsnprintf(char *d, size_t n, const char *f, va_list va) { return vfmt(d, (int)n, f, va); }
int snprintf(char *d, size_t n, const char *f, ...)
{
	va_list va;
	int k;

	va_start(va, f);
	k = vfmt(d, (int)n, f, va);
	va_end(va);
	return k;
}
int sprintf(char *d, const char *f, ...)
{
	va_list va;
	int k;

	va_start(va, f);
	k = vfmt(d, 4096, f, va);
	va_end(va);
	return k;
}
int printf(const char *f, ...) { (void)f; return 0; }
int remove(const char *r) { (void)r; return -1; }
int fileno(FILE *f) { (void)f; return -1; }
void abort(void) { llamada(2, 255, 0, 0); for (;;) ; }
void exit(int c) { llamada(2, (u32)c, 0, 0); for (;;) ; }
long time(long *t) { if (t) *t = 0; return 0; }

/* ---- las cuentas de 64 bits (las de libgcc, que aquí no hay para 32 bits) ---- */
u64 __udivmoddi4(u64 n, u64 d, u64 *r)
{
	u64 q = 0;
	int k;

	if (!d)
		return 0;
	if (!(d >> 32) && !(n >> 32)) {
		if (r)
			*r = (u32)n % (u32)d;
		return (u32)n / (u32)d;
	}
	for (k = 63; k >= 0; k--)
		if ((n >> k) >= d) {
			n -= d << k;
			q |= 1ull << k;
		}
	if (r)
		*r = n;
	return q;
}
u64 __udivdi3(u64 n, u64 d) { return __udivmoddi4(n, d, 0); }
u64 __umoddi3(u64 n, u64 d) { u64 r; __udivmoddi4(n, d, &r); return r; }
i64 __divdi3(i64 n, i64 d)
{
	int neg = (n < 0) ^ (d < 0);
	u64 q = __udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, 0);

	return neg ? -(i64)q : (i64)q;
}
i64 __moddi3(i64 n, i64 d)
{
	u64 r;

	__udivmoddi4(n < 0 ? -(u64)n : (u64)n, d < 0 ? -(u64)d : (u64)d, &r);
	return n < 0 ? -(i64)r : (i64)r;
}
i64 __divmoddi4(i64 n, i64 d, i64 *r)
{
	i64 q = __divdi3(n, d);

	if (r)
		*r = n - q * d;
	return q;
}
u64 __fixunsdfdi(double x)
{
	if (x <= 0)
		return 0;
	if (x >= 9223372036854775808.0)
		return (u64)(i64)(x - 9223372036854775808.0) + 0x8000000000000000ull;
	return (u64)(i64)x;
}
u64 __fixunssfdi(float x) { return __fixunsdfdi(x); }
double __floatundidf(u64 x)
{
	double h = (double)(u32)(x >> 32), l = (double)(u32)x;

	return h * 4294967296.0 + l;
}
float __floatundisf(u64 x) { return (float)__floatundidf(x); }
int __popcountsi2(u32 x)
{
	int n = 0;

	while (x) {
		n += x & 1;
		x >>= 1;
	}
	return n;
}
int __clzsi2(u32 x)
{
	int n = 0;

	if (!x)
		return 32;
	while (!(x & 0x80000000u)) {
		x <<= 1;
		n++;
	}
	return n;
}
int __ctzsi2(u32 x)
{
	int n = 0;

	if (!x)
		return 32;
	while (!(x & 1)) {
		x >>= 1;
		n++;
	}
	return n;
}
int __clzdi2(u64 x) { return (x >> 32) ? __clzsi2((u32)(x >> 32)) : 32 + __clzsi2((u32)x); }
int __ctzdi2(u64 x) { return (u32)x ? __ctzsi2((u32)x) : 32 + __ctzsi2((u32)(x >> 32)); }

/* =================================================================== la coma flotante (x87) */
double sqrt(double x)
{
	if (x <= 0)
		return 0;
	__asm__("fsqrt" : "+t"(x));
	return x;
}
float sqrtf(float x) { return (float)sqrt(x); }

static double redondea(double x, u16 modo)
{
	u16 antes, cw;

	__asm__ volatile("fnstcw %0" : "=m"(antes));
	cw = (u16)((antes & ~0x0C00) | modo);
	__asm__ volatile("fldcw %0" : : "m"(cw));
	__asm__ volatile("frndint" : "+t"(x));
	__asm__ volatile("fldcw %0" : : "m"(antes));
	return x;
}
double floor(double x) { return redondea(x, 0x0400); }
double ceil(double x) { return redondea(x, 0x0800); }
float floorf(float x) { return (float)floor(x); }
float ceilf(float x) { return (float)ceil(x); }
double fabs(double x) { return x < 0 ? -x : x; }
float fabsf(float x) { return x < 0 ? -x : x; }
long lrintf(float x) { return (long)redondea(x, 0); }
double round(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }

double fmod(double x, double y)
{
	double r;

	if (y == 0)
		return 0;
	__asm__("1: fprem\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
	return r;
}

double log(double x)
{
	double r;

	if (x <= 0)
		return -1e300;
	__asm__("fldln2\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
	return r;
}
float logf(float x) { return (float)log(x); }
double log10(double x) { return log(x) * 0.43429448190325182765; }

static double exp2_(double x)
{
	double ent = redondea(x, 0), fr = x - ent, r;

	__asm__("f2xm1" : "=t"(r) : "0"(fr));
	r += 1;
	__asm__("fscale" : "=t"(r) : "0"(r), "u"(ent));
	return r;
}
double exp(double x)
{
	if (x > 700)
		return 1e300;
	if (x < -700)
		return 0;
	return exp2_(x * 1.44269504088896340736);
}
float expf(float x) { return (float)exp(x); }

double pow(double x, double y)
{
	double l;

	if (y == 0)
		return 1;
	if (x == 0)
		return 0;
	if (x < 0) {
		double r = pow(-x, y);
		i32 e = (i32)y;

		return (e & 1) ? -r : r;
	}
	__asm__("fyl2x" : "=t"(l) : "0"(x), "u"(y) : "st(1)");
	if (l > 1000)
		return 1e300;
	if (l < -1000)
		return 0;
	return exp2_(l);
}
float powf(float x, float y) { return (float)pow(x, y); }

double cos(double x)
{
	__asm__("fcos" : "+t"(x));
	return x;
}
double sin(double x)
{
	__asm__("fsin" : "+t"(x));
	return x;
}
float cosf(float x) { return (float)cos(x); }
float sinf(float x) { return (float)sin(x); }
double atan2(double y, double x)
{
	double r;

	__asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}
double atan(double x) { return atan2(x, 1); }
double ldexp(double x, int e)
{
	double r, k = e;

	__asm__("fscale" : "=t"(r) : "0"(x), "u"(k));
	return r;
}
float ldexpf(float x, int e) { return (float)ldexp(x, e); }
double frexp(double x, int *e)
{
	int k = 0;

	if (x == 0) {
		*e = 0;
		return 0;
	}
	while (fabs(x) >= 1) {
		x /= 2;
		k++;
	}
	while (fabs(x) < 0.5) {
		x *= 2;
		k--;
	}
	*e = k;
	return x;
}
double modf(double x, double *i)
{
	double t = x < 0 ? ceil(x) : floor(x);

	*i = t;
	return x - t;
}

/* =================================================================== la memoria */
#define CLASES 13
#define GRANDE (64u * 1024)
typedef struct trozo {
	u32 tam;
	u32 marca;
	struct trozo *sig;
	u32 relleno;
} trozo;
#define EN_USO 0x50455248u		/* «REPP» */
#define LIBRE 0x5242494Cu

static trozo *libres[CLASES];
static trozo *grandes;
static u8 *tope;
static u32 pedido;

static void *mas(u32 n)
{
	u32 r = llamada(17, n, 0, 0);

	if (ES_ERROR(r) || r == 0)
		return NULL;
	tope = (u8 *)r + n;
	return (void *)r;
}

void *so_pide(size_t n)
{
	trozo *t;
	u32 tam;
	int c;

	if (n > 0x30000000u)
		return NULL;
	if (n <= GRANDE / 2) {
		for (c = 0, tam = 16; tam < n; c++, tam *= 2)
			;
		if (libres[c]) {
			t = libres[c];
			libres[c] = t->sig;
		} else {
			t = mas(tam + sizeof(trozo));
			if (!t)
				return NULL;
			t->tam = tam;
		}
	} else {
		trozo **p, **mejor = NULL;

		tam = ((u32)n + GRANDE - 1) & ~(GRANDE - 1);
		for (p = &grandes; *p; p = &(*p)->sig)
			if ((*p)->tam >= tam && (!mejor || (*p)->tam < (*mejor)->tam))
				mejor = p;
		if (mejor && (*mejor)->tam <= tam + tam / 2) {
			t = *mejor;
			*mejor = t->sig;
		} else {
			t = mas(tam + sizeof(trozo));
			if (!t)
				return NULL;
			t->tam = tam;
		}
	}
	t->marca = EN_USO;
	pedido += t->tam;
	return t + 1;
}

void so_suelta(void *q)
{
	trozo *t;
	int c;
	u32 tam;

	if (!q)
		return;
	t = (trozo *)q - 1;
	if (t->marca != EN_USO)
		return;
	t->marca = LIBRE;
	pedido -= t->tam;
	if (t->tam > GRANDE / 2) {
		if ((u8 *)(t + 1) + t->tam == tope) {
			u32 n = t->tam + sizeof(trozo);

			if (!ES_ERROR(llamada(17, (u32)-(i32)n, 0, 0))) {
				tope -= n;
				return;
			}
		}
		t->sig = grandes;
		grandes = t;
		return;
	}
	for (c = 0, tam = 16; tam < t->tam; c++, tam *= 2)
		;
	t->sig = libres[c];
	libres[c] = t;
}

void *so_repide(void *q, size_t n)
{
	trozo *t;
	void *r;

	if (!q)
		return so_pide(n);
	t = (trozo *)q - 1;
	if (n <= t->tam)
		return q;
	r = so_pide(n);
	if (!r)
		return NULL;
	memcpy(r, q, t->tam);
	so_suelta(q);
	return r;
}

u32 so_memoria_usada(void) { return pedido; }
void *malloc(size_t n) { return so_pide(n); }
void free(void *p) { so_suelta(p); }
void *realloc(void *p, size_t n) { return so_repide(p, n); }
void *calloc(size_t a, size_t b)
{
	void *p;

	if (b && a > 0x30000000u / b)
		return NULL;
	p = so_pide(a * b);
	if (p)
		memset(p, 0, a * b);
	return p;
}

/* =================================================================== los archivos */
struct so_archivo { u32 m; u64 pos, largo; int escribe; };

so_archivo *so_abre(const char *ruta)
{
	u32 m = llamada(7, (u32)ruta, 0, 0), n;
	so_archivo *a;

	if (ES_ERROR(m))
		return NULL;
	n = llamada(13, m, 0, 0);
	if (ES_ERROR(n)) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	a = so_pide(sizeof *a);
	if (!a) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	a->m = m;
	a->pos = 0;
	a->largo = n;
	a->escribe = 0;
	return a;
}

int so_lee(so_archivo *a, void *d, u32 n)
{
	u32 hecho = 0;

	while (hecho < n) {
		u32 k = llamada(8, a->m, (u32)d + hecho, n - hecho);

		if (ES_ERROR(k))
			return hecho ? (int)hecho : -1;
		if (!k)
			break;
		hecho += k;
	}
	a->pos += hecho;
	return (int)hecho;
}

int so_salta_a(so_archivo *a, u64 pos)
{
	u32 r;

	if (pos > 0x7FFFFFFFu)
		return -1;
	if (pos == a->pos)
		return 0;
	r = llamada(19, a->m, (u32)pos, 0);
	if (ES_ERROR(r))
		return -1;
	a->pos = r;
	return r == pos ? 0 : -1;
}

u64 so_largo(so_archivo *a) { return a->largo; }
u64 so_donde(so_archivo *a) { return a->pos; }

void so_cierra(so_archivo *a)
{
	if (!a)
		return;
	llamada(9, a->m, 0, 0);
	so_suelta(a);
}

so_archivo *so_crea(const char *ruta)
{
	u32 m = llamada(7, (u32)ruta, 2, 0);
	so_archivo *a;

	if (ES_ERROR(m))
		return NULL;
	a = so_pide(sizeof *a);
	if (!a) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	a->m = m;
	a->pos = a->largo = 0;
	a->escribe = 1;
	return a;
}

int so_escribe(so_archivo *a, const void *d, u32 n)
{
	u32 hecho = 0;

	while (hecho < n) {
		u32 k = llamada(18, a->m, (u32)d + hecho, n - hecho);

		if (ES_ERROR(k) || k == 0)
			return -1;
		hecho += k;
	}
	a->pos += n;
	return 0;
}

int so_borra(const char *ruta) { return ES_ERROR(llamada(21, (u32)ruta, 0, 0)) ? -1 : 0; }
int so_haz_carpeta(const char *ruta) { return ES_ERROR(llamada(23, (u32)ruta, 0, 0)) ? -1 : 0; }

int so_existe(const char *ruta, int *c)
{
	u32 m = llamada(7, (u32)ruta, 0, 0), r;
	struct { char nombre[13]; u8 atr; u16 res; u32 tam; u16 f, h; u32 cl, li; } e;
	char largo[256];

	if (ES_ERROR(m))
		return 0;
	/* ¿es una carpeta? se puede leer su primera entrada */
	(void)r;
	llamada(9, m, 0, 0);
	if (c) {
		/* (la 13 de un manejador de carpeta da 0; un archivo vacío, también:
		 * se mira por el atributo de la propia carpeta, en la de arriba) */
		char arriba[MAX_RUTA], *b;
		const char *nom;
		u32 k;

		*c = 0;
		copia(arriba, sizeof arriba, ruta);
		b = strrchr(arriba, '\\');
		if (!b)
			return 1;
		nom = ruta + (b - arriba) + 1;
		if (!*nom) {
			*c = 1;
			return 1;
		}
		if (b == arriba || (b == arriba + 2 && arriba[1] == ':'))
			b[1] = 0;
		else
			*b = 0;
		k = llamada(7, (u32)arriba, 0, 0);
		if (!ES_ERROR(k)) {
			while (llamada(28, k, (u32)&e, (u32)largo) == 1)
				if (igual_sin_caja(e.nombre, nom) || igual_sin_caja(largo, nom)) {
					*c = (e.atr & 0x10) != 0;
					break;
				}
			llamada(9, k, 0, 0);
		}
	}
	return 1;
}

u8 *so_lee_todo(const char *ruta, u32 *largo, u32 max)
{
	so_archivo *a = so_abre(ruta);
	u8 *d;
	int k;

	if (!a)
		return NULL;
	if (a->largo > max) {
		so_cierra(a);
		return NULL;
	}
	d = so_pide((u32)a->largo + 1);
	if (!d) {
		so_cierra(a);
		return NULL;
	}
	k = so_lee(a, d, (u32)a->largo);
	so_cierra(a);
	if (k < 0) {
		so_suelta(d);
		return NULL;
	}
	*largo = (u32)k;
	d[k] = 0;
	return d;
}

/* ---- las carpetas ---- */
typedef struct { u32 m; } carpeta;

void *so_abre_carpeta(const char *ruta)
{
	u32 m = llamada(7, (u32)ruta, 0, 0);
	carpeta *c;

	if (ES_ERROR(m))
		return NULL;
	c = so_pide(sizeof *c);
	if (!c) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	c->m = m;
	return c;
}

int so_siguiente(void *cc, so_entrada *s)
{
	carpeta *c = cc;
	struct { char nombre[13]; u8 atr; u16 res; u32 tam; u16 f, h; u32 cl, li; } e;
	char largo[256];

	for (;;) {
		largo[0] = 0;
		if (llamada(28, c->m, (u32)&e, (u32)largo) != 1)
			return 0;
		e.nombre[12] = 0;
		largo[255] = 0;
		if (!e.nombre[0] || e.atr & 0x08)
			continue;	/* (el volumen) */
		if (e.nombre[0] == '.' && (!e.nombre[1] || (e.nombre[1] == '.' && !e.nombre[2])))
			continue;
		if (e.atr & 0x02)
			continue;	/* (las ocultas: la Papelera...) */
		memset(s, 0, sizeof *s);
		copia(s->nombre, sizeof s->nombre, e.nombre);
		so_a_utf8(largo[0] ? largo : e.nombre, s->visto, sizeof s->visto);
		s->carpeta = (e.atr & 0x10) != 0;
		s->tam = e.tam;
		return 1;
	}
}

void so_cierra_carpeta(void *cc)
{
	carpeta *c = cc;

	if (!c)
		return;
	llamada(9, c->m, 0, 0);
	so_suelta(c);
}

typedef struct {
	u8 letra, clase, medio, solo;
	u32 total_k, libre_k, del_sistema;
	char nombre[41], etiqueta[12];
	u8 quitable, relleno[2];
} info_unidad;

int so_unidades(char *letras, char *que, int cabe)
{
	info_unidad u;
	int i, n = 0;

	for (i = 0; i < 40 && n < cabe; i++) {
		memset(&u, 0, sizeof u);
		if (llamada(43, (u32)i, (u32)&u, 0) != 1)
			break;
		if (!u.letra)
			continue;
		letras[n] = (char)u.letra;
		que[n] = u.medio == 4 ? 'D' : u.medio == 5 ? 'I' : u.medio == 3 ? 'U' : u.medio == 0 ? 'M' : 'C';
		n++;
	}
	return n;
}

char so_separador(void) { return '\\'; }

/* la unidad del sistema (\HISPA instalado; si no, la raíz de la memoria) */
static char letra_sistema(void)
{
	info_unidad u;
	int i;

	for (i = 0; i < 40; i++) {
		memset(&u, 0, sizeof u);
		if (llamada(43, (u32)i, (u32)&u, 0) != 1)
			break;
		if (u.del_sistema && u.letra)
			return (char)u.letra;
	}
	return 'C';
}

void so_carpeta_propia(char *d, int cabe)
{
	char r[16];
	int c = 0;

	fmt(r, sizeof r, "%c:\\HISPA", letra_sistema());
	if (so_existe(r, &c) && c)
		fmt(d, cabe, "%c:\\HISPA\\REPRODUC\\", r[0]);
	else
		fmt(d, cabe, "%c:\\REPRODUC\\", r[0]);
}

void so_carpeta_musica(char *d, int cabe)
{
	static const char *probar[] = { "MUSICA", "MÚSICA", "MUSIC", "DOCUMENT\\MUSICA", 0 };
	char r[64];
	int i, c = 0;
	char l = letra_sistema();

	for (i = 0; probar[i]; i++) {
		char s[48];

		so_de_utf8(probar[i], s, sizeof s);
		fmt(r, sizeof r, "%c:\\%s", l, s);
		if (so_existe(r, &c) && c) {
			copia(d, cabe, r);
			return;
		}
	}
	fmt(d, cabe, "%c:\\", l);
}

/* ---- la página 437 y el UTF-8 ---- */
static const u16 alto437[128] = { 0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7, 0xEA, 0xEB, 0xE8, 0xEF, 0xEE, 0xEC,
	0xC4, 0xC5, 0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9, 0xFF, 0xD6, 0xDC, 0xA2, 0xA3, 0xA5, 0x20A7, 0x192,
	0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF, 0x2310, 0xAC, 0xBD, 0xBC, 0xA1, 0xAB, 0xBB, 0x2591, 0x2592,
	0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550,
	0x256C, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588,
	0x2584, 0x258C, 0x2590, 0x2580, 0x3B1, 0xDF, 0x393, 0x3C0, 0x3A3, 0x3C3, 0xB5, 0x3C4, 0x3A6, 0x398, 0x3A9, 0x3B4,
	0x221E, 0x3C6, 0x3B5, 0x2229, 0x2261, 0xB1, 0x2265, 0x2264, 0x2320, 0x2321, 0xF7, 0x2248, 0xB0, 0x2219, 0xB7,
	0x221A, 0x207F, 0xB2, 0x25A0, 0xA0 };

void so_a_utf8(const char *s, char *d, int cabe)
{
	int k = 0;

	for (; *s && k + 4 < cabe; s++) {
		u8 c = (u8)*s;

		k += utf8_pon(c < 128 ? c : alto437[c - 128], d + k);
	}
	d[k] = 0;
}

static int a437(u32 u)
{
	int i;

	if (u < 128)
		return (int)u;
	if (u == 0x2013 || u == 0x2014)
		return '-';
	if (u == 0x2018 || u == 0x2019)
		return '\'';
	if (u == 0x201C || u == 0x201D)
		return '"';
	if (u == 0x2026)
		return '.';
	for (i = 0; i < 128; i++)
		if (alto437[i] == u)
			return 128 + i;
	/* (lo que no está: sin tilde) */
	u = llana(u);
	return u < 128 ? (int)u : '_';
}

void so_de_utf8(const char *s, char *d, int cabe)
{
	int k = 0;

	while (*s && k < cabe - 1) {
		u32 u = utf8_sig(&s);

		d[k++] = (char)a437(u);
	}
	d[k] = 0;
}

/* =================================================================== el tiempo */
static u32 hz_reloj;

u32 so_ms(void)
{
	u32 t = llamada(4, 0, 0, 0);

	if (!hz_reloj) {
		hz_reloj = llamada(15, 9, 0, 0);
		if (ES_ERROR(hz_reloj) || !hz_reloj)
			hz_reloj = 100;
	}
	return (u32)((u64)t * 1000 / hz_reloj);
}

void so_duerme(u32 ms)
{
	u32 t;

	if (!hz_reloj)
		so_ms();
	t = (ms * hz_reloj + 999) / 1000;
	llamada(3, t ? t : 1, 0, 0);
}

u32 so_ms_propios(void)
{
	u32 t = llamada(15, 16, 0, 0);

	if (ES_ERROR(t))
		return 0;
	if (!hz_reloj)
		so_ms();
	return (u32)((u64)t * 1000 / hz_reloj);
}

u32 so_ciclos(void)
{
	u32 a, d;

	__asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
	(void)d;
	return a;
}

/* =================================================================== lo que dice */
void so_dice(const char *t)
{
	char b[600];

	so_de_utf8(t, b, sizeof b);
	llamada(1, (u32)b, (u32)strlen(b), 0);
}

void so_registro(const char *t)
{
	char b[600];

	so_de_utf8(t, b, sizeof b);
	llamada(61, (u32)b, (u32)strlen(b), 0);
}

int so_argumentos(char *d, int cabe)
{
	u32 n = llamada(14, (u32)d, (u32)cabe - 1, 0);

	if (ES_ERROR(n))
		n = 0;
	if (n >= (u32)cabe)
		n = (u32)cabe - 1;
	d[n] = 0;
	return (int)n;
}

int so_hay_escritorio(void)
{
	u32 r = llamada(15, 13, 0, 0);

	return !ES_ERROR(r) && r == 1;
}

void so_cabe_ventana(int *an, int *al)
{
	u32 a = llamada(15, 11, 0, 0), b = llamada(15, 14, 0, 0);

	*an = ES_ERROR(a) || !a ? 800 : (int)a - 8;
	*al = ES_ERROR(b) || !b ? 560 : (int)b - 28;
}

/* (REPRODUCTOR) el dato 15: ¿va en un terminal? (la Consola, SSH) */
int so_en_consola(void)
{
	u32 r = llamada(15, 15, 0, 0);

	if (!so_hay_escritorio())
		return 1;
	return !ES_ERROR(r) && r == 1;
}

int so_tecla(void)
{
	u32 t = llamada(6, 1, 0, 0);

	return ES_ERROR(t) ? 0 : (int)t;
}

/* =================================================================== el sonido */
#define C_CERRAR 0
#define C_VOLUMEN 1
#define C_PAUSA 2
#define C_VACIAR 3
#define C_EN_COLA 4
#define C_TOCADOS 6

static u32 canal;
static u32 tocados_antes, tocados_alto;

struct estado_sonido {
	u32 version, tarjeta, hz, pci, dma_isa, salida, detecta, auriculares;
	u32 volumen, silencio, sistema, voces, cambios, puertos, memoria, irq;
	char nombre[48];
	char codec[48];
	char detalle[80];
};

u32 so_hz_tarjeta(void)
{
	struct estado_sonido e;

	memset(&e, 0, sizeof e);
	if (llamada(193, 0, (u32)&e, 0) != 1 || !e.hz)
		return 0;
	return e.hz;
}

int so_sonido_abre(u32 hz)
{
	u32 c = llamada(190, hz, 2 | 16 << 8, 0);

	if (ES_ERROR(c) || !c)
		return 0;
	canal = c;
	tocados_antes = tocados_alto = 0;
	return 1;
}

u32 so_sonido_da(const i16 *d, u32 n)
{
	u32 k;

	if (!canal)
		return n;
	k = llamada(191, canal | 0x80000000u, (u32)d, n * 4);
	if (ES_ERROR(k))
		return 0;
	return k / 4;
}

u32 so_sonido_en_cola(void)
{
	u32 k;

	if (!canal)
		return 0;
	k = llamada(192, canal, C_EN_COLA, 0);
	return ES_ERROR(k) ? 0 : k / 4;
}

u64 so_sonido_tocadas(void)
{
	u32 k;

	if (!canal)
		return 0;
	k = llamada(192, canal, C_TOCADOS, 0);
	if (ES_ERROR(k))
		return (u64)tocados_alto << 32 | tocados_antes;
	if (k < tocados_antes)
		tocados_alto++;	/* (ha dado la vuelta: 4 GB, seis horas) */
	tocados_antes = k;
	return ((u64)tocados_alto << 32 | k) / 4;
}

void so_sonido_vacia(void) { if (canal) llamada(192, canal, C_VACIAR, 0); }
void so_sonido_pausa(int si) { if (canal) llamada(192, canal, C_PAUSA, (u32)si); }
void so_sonido_volumen(int v) { if (canal) llamada(192, canal, C_VOLUMEN, (u32)v); }

int so_sonido_volumen_general(int v)
{
	u32 r = llamada(193, 1, 0, v < 0 ? 0xFFFFFFFFu : (u32)v);

	return ES_ERROR(r) ? 0 : (int)r;
}

void so_sonido_cierra(void)
{
	if (canal)
		llamada(192, canal, C_CERRAR, 0);
	canal = 0;
}

/* =================================================================== el CD (la llamada 134) */
typedef struct {
	u32 pistas, primera;
	struct { u32 lba; u8 control, numero, r1, r2; } e[100];
} toc_nucleo;
typedef struct { u32 lba, n; void *destino; } pide_cdda;

int so_lectores_cd(char *letras, int cabe)
{
	char l[32], q[32];
	int n = so_unidades(l, q, 32), i, k = 0;

	for (i = 0; i < n && k < cabe; i++)
		if (q[i] == 'D')
			letras[k++] = l[i];
	return k;
}

int so_cd_toc(char letra, so_toc *t)
{
	static toc_nucleo tn;
	u32 r, i;

	memset(t, 0, sizeof *t);
	memset(&tn, 0, sizeof tn);
	r = llamada(134, (u8)letra, 0, (u32)&tn);
	if (ES_ERROR(r) || r != 0 || tn.pistas == 0 || tn.pistas > 99)
		return -1;
	t->pistas = (int)tn.pistas;
	t->primera = (int)tn.primera;
	for (i = 0; i <= tn.pistas; i++) {
		t->pista[i].lba = tn.e[i].lba;
		t->pista[i].control = tn.e[i].control;
		t->pista[i].numero = tn.e[i].numero;
	}
	return 0;
}

int so_cd_lee(char letra, u32 lba, u32 n, void *d)
{
	pide_cdda p;

	p.lba = lba;
	p.n = n;
	p.destino = d;
	return llamada(134, (u8)letra, 1, (u32)&p) == 0 ? 0 : -1;
}

int so_cd_lee_crudo(char letra, u32 lba, u32 n, void *d)
{
	pide_cdda p;

	p.lba = lba;
	p.n = n;
	p.destino = d;
	return llamada(134, (u8)letra, 2, (u32)&p) == 0 ? 0 : -1;
}

int so_cd_expulsa(char letra) { return llamada(132, (u8)letra, 0, 0) == 0 ? 0 : -1; }

/* =================================================================== las ventanas */
struct orden { u32 op; int x, y, ancho, alto; u32 tinta, tinta2; const void *datos; int n; };

u32 so_ventana(const char *titulo, int an, int al)
{
	char t[100];
	u32 v;

	so_de_utf8(titulo, t, sizeof t);
	v = llamada(32, (u32)t, (u32)an, (u32)al);
	if (ES_ERROR(v))
		return 0;
	llamada(80, v, 0xFFFFFFFFu, 0);	/* (en el centro, encima de la barra de tareas) */
	return v;
}

void so_titulo(u32 v, const char *t)
{
	char b[100];

	so_de_utf8(t, b, sizeof b);
	llamada(36, v, (u32)b, 0);
}

static u32 *trozo_vuelco;
static u32 cabe_vuelco;

void so_vuelca(u32 v, const lienzo *l, int x, int y, int an, int al)
{
	struct orden o;
	int f;

	if (an <= 0 || al <= 0)
		return;
	if (x == 0 && an == l->an) {
		o.datos = l->p + y * l->an;
	} else {
		/* (un trozo: seguido, en un búfer aparte) */
		if ((u32)(an * al) > cabe_vuelco) {
			so_suelta(trozo_vuelco);
			cabe_vuelco = (u32)(an * al);
			trozo_vuelco = so_pide(cabe_vuelco * 4);
			if (!trozo_vuelco) {
				cabe_vuelco = 0;
				return;
			}
		}
		for (f = 0; f < al; f++)
			memcpy(trozo_vuelco + f * an, l->p + (y + f) * l->an + x, (size_t)an * 4);
		o.datos = trozo_vuelco;
	}
	o.op = 7;
	o.x = x;
	o.y = y;
	o.ancho = an;
	o.alto = al;
	o.tinta = o.tinta2 = 0;
	o.n = 0;
	llamada(34, v, (u32)&o, 0);
}

int so_suceso_de(u32 v, so_suceso *s)
{
	u32 e[4];

	if (llamada(35, v, (u32)e, 0) != 1)
		return 0;
	s->clase = e[0];
	s->a = e[1];
	s->b = e[2];
	s->c = e[3];
	return 1;
}

/* (la 182 de WIN32: colocar y medir, traer delante) */
struct pide_ventana { const char *nombre; int x, y, ancho, alto; u32 marcas; };

void so_cambia_tamano(u32 v, int an, int al)
{
	int d[6];
	struct pide_ventana p;

	memset(d, 0, sizeof d);
	if (llamada(182, 4, v, (u32)d))
		return;
	/* d: la esquina y lo que mide por fuera (x, y, ancho, alto) */
	memset(&p, 0, sizeof p);
	p.x = d[0];
	p.y = d[1];
	p.ancho = an;
	p.alto = al;
	llamada(182, 1, v, (u32)&p);
}

void so_trae(u32 v) { llamada(182, 3, v, 0); }

struct pide_menu { int x, y; const char *opciones; u32 inactivas, etiqueta; };

int so_menu(u32 v, int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
	struct pide_menu p;
	char b[768];

	so_de_utf8(opciones, b, sizeof b);
	p.x = x;
	p.y = y;
	p.opciones = b;
	p.inactivas = grises;
	p.etiqueta = etiqueta;
	return llamada(39, v, (u32)&p, 0) == 0 ? 0 : -1;
}

int so_soltado(char *ruta, int cabe)
{
	u32 n = llamada(136, (u32)ruta, (u32)cabe, 0);

	if (ES_ERROR(n) || !n) {
		ruta[0] = 0;
		return 0;
	}
	ruta[cabe - 1] = 0;
	return 1;
}

/* (REPRODUCTOR) la 135: el icono de la bandeja */
static struct { u32 puntos[256]; char ayuda[48]; char opciones[160]; } icono_bandeja;

void so_bandeja(u32 v, const u32 *icono, const char *ayuda, const char *opciones)
{
	if (!icono) {
		llamada(135, v, 0, 0);
		return;
	}
	memcpy(icono_bandeja.puntos, icono, sizeof icono_bandeja.puntos);
	so_de_utf8(ayuda, icono_bandeja.ayuda, sizeof icono_bandeja.ayuda);
	so_de_utf8(opciones, icono_bandeja.opciones, sizeof icono_bandeja.opciones);
	llamada(135, v, (u32)&icono_bandeja, 0);
}

void so_al_portapapeles(const char *t)
{
	char b[1024];

	so_de_utf8(t, b, sizeof b);
	llamada(40, 0, (u32)b, (u32)strlen(b));
}

void so_lanza(const char *programa, const char *args)
{
	llamada(41, (u32)programa, (u32)args, 0);
}

/* =================================================================== una sola copia */
/* El que está en marcha tiene REPRODUC.LCK abierto para escribir (nadie más
 * lo puede abrir: «está en uso»). El que llega deja lo pedido en
 * REPRODUC.BUZ y se acaba; el que está mira si hay buzón cada medio segundo. */
static so_archivo *cerrojo;

int so_otro_en_marcha(const char *pedido)
{
	char c[MAX_RUTA], r[MAX_RUTA];
	so_archivo *a;
	u32 m;

	so_carpeta_propia(c, sizeof c);
	if (!so_existe(c, NULL))
		so_haz_carpeta(c);
	copia(r, sizeof r, c);
	anade(r, sizeof r, "REPRODUC.LCK");
	m = llamada(7, (u32)r, 2, 0);
	if (!ES_ERROR(m)) {
		/* no hay otro: este es el que se queda (con el cerrojo abierto) */
		cerrojo = so_pide(sizeof *cerrojo);
		if (cerrojo) {
			cerrojo->m = m;
			cerrojo->pos = cerrojo->largo = 0;
			cerrojo->escribe = 1;
		}
		return 0;
	}
	if (m != 0xFFFFFFF6u)
		return 0;	/* (no se puede escribir ahí: sin una sola copia) */
	if (!pedido || !pedido[0])
		pedido = "/MOSTRAR";	/* (hay otro y no se pide nada: que se vea el que hay) */
	copia(r, sizeof r, c);
	anade(r, sizeof r, "REPRODUC.BUZ");
	a = so_crea(r);
	if (!a)
		return 0;
	so_escribe(a, pedido, (u32)strlen(pedido));
	so_cierra(a);
	return 1;
}

int so_pedido_nuevo(char *d, int cabe)
{
	char r[MAX_RUTA];
	u32 n;
	u8 *t;

	so_carpeta_propia(r, sizeof r);
	anade(r, sizeof r, "REPRODUC.BUZ");
	t = so_lee_todo(r, &n, (u32)cabe - 1);
	if (!t)
		return 0;
	copia(d, cabe, (char *)t);
	so_suelta(t);
	so_borra(r);
	return 1;
}
