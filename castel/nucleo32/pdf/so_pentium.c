/* so_pentium.c - Hispa-PDF en Hispa-Pentium: lo de so.h con las llamadas
 * del núcleo (int 0x80; docs/PENTIUM.md, sección 6) y lo poco de la
 * biblioteca de C que piden miniz y stb (sin biblioteca del sistema:
 * -ffreestanding). La memoria: por tamaños (las pequeñas, por potencias de
 * dos, con su lista de libres; las grandes, de 64 KB en 64 KB, que se
 * vuelven a usar y, la de arriba del todo, se devuelve al núcleo). La coma
 * flotante, con el x87 (cada programa tiene el suyo desde la 0.8.3).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <string.h>
#include "hpdf.h"

#define ES_ERROR(r) ((u32)(r) >= 0xFFFFFFF0u)

static u32 llamada(u32 n, u32 a, u32 b, u32 c)
{
	u32 r;

	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* ---- lo de la biblioteca de C ---- */
void *memcpy(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	if ((((u32)a | (u32)b) & 3) == 0)
		for (; n >= 4; n -= 4, a += 4, b += 4)
			*(u32 *)a = *(const u32 *)b;
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
		u32 v = (u8)c * 0x01010101u;

		for (; n >= 4; n -= 4, a += 4)
			*(u32 *)a = v;
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

char *strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == (char)c)
			return (char *)s;
	return c ? NULL : (char *)s;
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

int abs(int x)
{
	return x < 0 ? -x : x;
}

/* ---- las divisiones de 64 bits (las de libgcc; como win32/libc.c) ---- */
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

/* ---- la memoria ---- */
#define CLASES 13		/* de 16 a 64 KB */
#define GRANDE (64u * 1024)
typedef struct trozo {
	u32 tam;		/* lo que cabe (sin la cabecera) */
	u32 marca;		/* «HPDF» en uso; «LIBR» libre */
	struct trozo *sig;	/* (libre) el siguiente de su lista */
	u32 relleno;
} trozo;
#define EN_USO 0x46445048u
#define LIBRE 0x5242494Cu

static trozo *libres[CLASES];
static trozo *grandes;		/* los grandes libres */
static u8 *tope;		/* el final de la memoria del programa */
static u32 pedido, lo_mas;

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
	if (pedido > lo_mas)
		lo_mas = pedido;
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
		return;		/* (no es nuestro, o ya se soltó) */
	t->marca = LIBRE;
	pedido -= t->tam;
	if (t->tam > GRANDE / 2) {
		/* el de arriba del todo, al núcleo */
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

/* ---- los archivos ---- */
u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max, const char **error)
{
	u32 m = llamada(7, (u32)ruta, 0, 0), n, i, k;
	u8 *d;

	if (ES_ERROR(m)) {
		if (error)
			*error = m == 0xFFFFFFFFu ? "no está" : "no se puede abrir";
		return NULL;
	}
	n = llamada(13, m, 0, 0);
	if (ES_ERROR(n) || n > max) {
		llamada(9, m, 0, 0);
		if (error)
			*error = ES_ERROR(n) ? "no se puede leer" : "es demasiado grande";
		return NULL;
	}
	d = so_pide(n + 1);
	if (!d) {
		llamada(9, m, 0, 0);
		if (error)
			*error = "no hay memoria para leerlo";
		return NULL;
	}
	for (i = 0; i < n; i += k) {
		k = llamada(8, m, (u32)(d + i), n - i);
		if (ES_ERROR(k) || k == 0)
			break;
	}
	llamada(9, m, 0, 0);
	if (i < n) {
		so_suelta(d);
		if (error)
			*error = "no se puede leer";
		return NULL;
	}
	*largo = n;
	return d;
}

typedef struct { u32 m; } parchivo;

void *so_crea(const char *ruta, const char **error)
{
	u32 m = llamada(7, (u32)ruta, 2, 0);
	parchivo *f;

	if (ES_ERROR(m)) {
		if (error)
			*error = m == 0xFFFFFFF5u ? "es de solo lectura (o está protegido)" : m == 0xFFFFFFF6u ? "está en uso" :
				 m == 0xFFFFFFF3u ? "el nombre no vale (8+3)" : "no se puede crear";
		return NULL;
	}
	f = so_pide(sizeof(parchivo));
	if (!f) {
		llamada(9, m, 0, 0);
		return NULL;
	}
	f->m = m;
	return f;
}

int so_escribe(void *f, const void *d, u32 n)
{
	parchivo *a = f;
	u32 hecho = 0;

	while (hecho < n) {
		u32 k = llamada(18, a->m, (u32)d + hecho, n - hecho);

		if (ES_ERROR(k) || k == 0)
			return -1;
		hecho += k;
	}
	return 0;
}

int so_cierra(void *f)
{
	parchivo *a = f;
	u32 r = llamada(9, a->m, 0, 0);

	so_suelta(a);
	return ES_ERROR(r) ? -1 : 0;
}

int so_borra(const char *ruta)
{
	return ES_ERROR(llamada(21, (u32)ruta, 0, 0)) ? -1 : 0;
}

/* lo que se dice, de UTF-8 a la página 437 */
void so_dice(const char *t)
{
	u32 n = (u32)strlen(t);
	u8 b[700], *c = n * 3 + 8 <= sizeof b ? b : so_pide(n * 3 + 8);
	u32 k;

	if (!c)
		return;
	k = utf8_a_437((const u8 *)t, n, c);
	llamada(1, (u32)c, k, 0);
	if (c != b)
		so_suelta(c);
}

int so_pagina(void)
{
	return 437;
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

u32 so_tics(void)
{
	return llamada(4, 0, 0, 0);
}

u32 so_hz(void)
{
	u32 h = llamada(15, 9, 0, 0);

	return ES_ERROR(h) || !h ? 100 : h;
}

void so_azar(void *d, u32 n)
{
	static u8 pozo[32];
	static u32 vueltas;
	u8 b[64], h[32];
	u8 *s = d;

	while (n) {
		u32 a, e, k;

		__asm__ volatile("rdtsc" : "=a"(a), "=d"(e));
		memcpy(b, pozo, 32);
		memcpy(b + 32, &a, 4);
		memcpy(b + 36, &e, 4);
		k = so_tics();
		memcpy(b + 40, &k, 4);
		vueltas++;
		memcpy(b + 44, &vueltas, 4);
		k = (u32)(size_t)&k;
		memcpy(b + 48, &k, 4);
		k = llamada(15, 10, 0, 0);
		memcpy(b + 52, &k, 4);
		sha256(b, 56, h);
		memcpy(pozo, h, 32);
		k = n < 16 ? n : 16;
		memcpy(s, h + 16, k);
		s += k;
		n -= k;
	}
}

/* ---- la coma flotante, con el x87 ---- */
double so_raiz(double x)
{
	if (x <= 0)
		return 0;
	__asm__("fsqrt" : "+t"(x));
	return x;
}

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

double so_suelo(double x) { return redondea(x, 0x0400); }
double so_techo(double x) { return redondea(x, 0x0800); }
double so_abs(double x) { return x < 0 ? -x : x; }

double so_resto(double x, double y)
{
	double r;

	if (y == 0)
		return 0;
	__asm__("1: fprem\n\tfnstsw %%ax\n\ttestb $4, %%ah\n\tjnz 1b" : "=t"(r) : "0"(x), "u"(y) : "ax", "cc");
	return r;
}

double so_ln(double x)
{
	double r;

	if (x <= 0)
		return -1e300;
	__asm__("fldln2\n\tfxch\n\tfyl2x" : "=t"(r) : "0"(x));
	return r;
}

static double exp2_(double x)
{
	double ent = redondea(x, 0), fr = x - ent, r;

	__asm__("f2xm1" : "=t"(r) : "0"(fr));
	r += 1;
	__asm__("fscale" : "=t"(r) : "0"(r), "u"(ent));
	return r;
}

double so_pot(double x, double y)
{
	double l;

	if (y == 0)
		return 1;
	if (x == 0)
		return 0;
	if (x < 0) {
		/* (solo con y entero) */
		double r = so_pot(-x, y);
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

double so_cos(double x)
{
	__asm__("fcos" : "+t"(x));
	return x;
}

double so_sen(double x)
{
	__asm__("fsin" : "+t"(x));
	return x;
}

double so_atan2(double y, double x)
{
	double r;

	__asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)");
	return r;
}

double so_acos(double x)
{
	if (x >= 1)
		return 0;
	if (x <= -1)
		return 3.14159265358979323846;
	return so_atan2(so_raiz(1 - x * x), x);
}

/* (lo que pide gcc a veces para las de punto flotante de stb) */
double sqrt(double x) { return so_raiz(x); }
double floor(double x) { return so_suelo(x); }
double ceil(double x) { return so_techo(x); }
double fabs(double x) { return so_abs(x); }
double pow(double x, double y) { return so_pot(x, y); }
double fmod(double x, double y) { return so_resto(x, y); }
double cos(double x) { return so_cos(x); }
double acos(double x) { return so_acos(x); }

/* ---- la entrada ---- */
int hpdf_orden(const char *linea);
void hpdf_fuentes(void);
extern u32 hpdf_max_pixeles;

int hpdf_pentium(void)
{
	static char args[1100];

	so_argumentos(args, 300);
	/* «@ARCHIVO»: la orden, en ese archivo (la ventana: caben más de 128 octetos) */
	if (args[0] == '@') {
		u32 n, i;
		const char *e;
		u8 *t = so_lee_archivo(args + 1, &n, sizeof args - 1, &e);

		if (t) {
			for (i = 0; i < n; i++)
				args[i] = (char)(t[i] == '\r' || t[i] == '\n' ? ' ' : t[i]);
			args[n] = 0;
			so_suelta(t);
		}
	}
	hpdf_max_pixeles = 8u * 1024 * 1024;
	hpdf_fuentes();
	return hpdf_orden(args);
}
