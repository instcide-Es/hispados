/* libc_pentium.c - Correo en Hispa-Pentium: lo poco de la biblioteca de C
 * que usan el núcleo del correo y stb_image (sin biblioteca del sistema:
 * -ffreestanding), y la memoria (so_pide, so_repide, so_suelta). La memoria y
 * las cadenas, las de Hispa-PDF (castel/nucleo32/pdf/so_pentium.c: por
 * tamaños, las pequeñas por potencias de dos con su lista de libres, las
 * grandes de 64 KB en 64 KB, y la de arriba del todo vuelve al núcleo); los
 * formatos, los de Comprimidos (comprim/base.c), con la precisión («%.*s»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include "so.h"

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
	u32 marca;		/* «HPDF» en uso (el nombre, el de Hispa-PDF); «LIBR» libre */
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


u32 so_memoria_usada(void) { return pedido; }

char *strncpy(char *d, const char *o, size_t n)
{
	size_t i = 0;

	for (; i < n && o[i]; i++)
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

char *strncat(char *d, const char *o, size_t n)
{
	char *p = d + strlen(d);

	while (n-- && *o)
		*p++ = *o++;
	*p = 0;
	return d;
}

char *strrchr(const char *s, int c)
{
	const char *r = NULL;

	for (;; s++) {
		if (*s == (char)c)
			r = s;
		if (!*s)
			return (char *)r;
	}
}

void *memchr(const void *s, int c, size_t n)
{
	const u8 *p = s;

	for (; n; n--, p++)
		if (*p == (u8)c)
			return (void *)p;
	return NULL;
}

/* (stb_image y lo nuestro: ordenar con el de Shell, sin recursión) */
void qsort(void *b, size_t n, size_t t, int (*cmp)(const void *, const void *))
{
	u8 *a = b, tmp[256];
	size_t h = 1, i, j;

	if (t > sizeof tmp)
		return;
	while (h < n / 3)
		h = 3 * h + 1;
	for (; h; h /= 3)
		for (i = h; i < n; i++) {
			memcpy(tmp, a + i * t, t);
			for (j = i; j >= h && cmp(a + (j - h) * t, tmp) > 0; j -= h)
				memcpy(a + j * t, a + (j - h) * t, t);
			memcpy(a + j * t, tmp, t);
		}
}

/* ------------------------------------------------------------ formatos */
int vsnprintf(char *s, size_t cabe, const char *f, va_list a)
{
	size_t n = 0;
	char num[24];

#define PON(c) do { if (n + 1 < cabe) s[n] = (c); n++; } while (0)
	for (; *f; f++) {
		int menos = 0, cero = 0, ancho = 0, largo = 0, k = 0, neg = 0, base = 10, prec = -1;
		const char *t = NULL;
		u64 v = 0;

		if (*f != '%') {
			PON(*f);
			continue;
		}
		f++;
		for (;; f++) {
			if (*f == '-')
				menos = 1;
			else if (*f == '0')
				cero = 1;
			else
				break;
		}
		if (*f == '*') {
			ancho = va_arg(a, int);
			if (ancho < 0) {
				menos = 1;
				ancho = -ancho;
			}
			f++;
		} else
			while (*f >= '0' && *f <= '9')
				ancho = ancho * 10 + (*f++ - '0');
		if (*f == '.') {
			f++;
			prec = 0;
			if (*f == '*') {
				prec = va_arg(a, int);
				if (prec < 0)
					prec = -1;
				f++;
			} else
				while (*f >= '0' && *f <= '9')
					prec = prec * 10 + (*f++ - '0');
		}
		while (*f == 'l' || *f == 'z' || *f == 'h') {
			if (*f == 'l')
				largo++;
			f++;
		}
		switch (*f) {
		case 's':
			t = va_arg(a, const char *);
			if (!t)
				t = "(nada)";
			for (k = 0; t[k] && (prec < 0 || k < prec); k++)
				;
			break;
		case 'c':
			num[0] = (char)va_arg(a, int);
			t = num;
			k = 1;
			break;
		case 'd':
		case 'i':
			if (largo >= 2) {
				i64 x = va_arg(a, i64);

				neg = x < 0;
				v = neg ? -(u64)x : (u64)x;
			} else {
				i32 x = va_arg(a, i32);

				neg = x < 0;
				v = neg ? -(u64)(i64)x : (u64)x;
			}
			goto numero;
		case 'x':
		case 'X':
		case 'p':
			base = 16;
			/* fall through */
		case 'u':
			v = largo >= 2 ? va_arg(a, u64) : va_arg(a, u32);
numero:
			k = 0;
			do {
				int c = (int)(v % (unsigned)base);

				num[sizeof(num) - 1 - k++] = (char)(c < 10 ? '0' + c : (*f == 'X' ? 'A' : 'a') + c - 10);
				v /= (unsigned)base;
			} while (v);
			while (prec > k && k < 20)
				num[sizeof(num) - 1 - k++] = '0';
			if (neg)
				num[sizeof(num) - 1 - k++] = '-';
			t = num + sizeof(num) - k;
			break;
		case '%':
			PON('%');
			continue;
		default:
			if (!*f)
				f--;
			continue;
		}
		if (!menos)
			while (ancho > k) {
				PON(cero ? '0' : ' ');
				ancho--;
			}
		while (k--) {
			PON(*t);
			t++;
			ancho--;
		}
		while (ancho-- > 0)
			PON(' ');
	}
	if (cabe)
		s[n < cabe ? n : cabe - 1] = 0;
	return (int)n;
#undef PON
}

int snprintf(char *s, size_t cabe, const char *f, ...)
{
	va_list a;
	int r;

	va_start(a, f);
	r = vsnprintf(s, cabe, f, a);
	va_end(a);
	return r;
}
