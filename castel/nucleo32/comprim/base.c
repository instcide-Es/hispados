/* base.c - la poca biblioteca de C de Comprimidos (COMPRIM.BIN) y del
 * Mirador (MIRADOR.BIN), programas del anillo 3 de Hispa-Pentium sin
 * biblioteca de verdad (-ffreestanding): la memoria (malloc y free, con
 * los trozos libres en una lista por su dirección, que se juntan con los de
 * al lado; crece con la llamada 17), las letras (mem*, str*), los formatos
 * (vsnprintf: %s %c %d %u %x, con l y ll, ancho, «-» y «0»), las cuentas de
 * 64 bits que gcc pide a libgcc (que aquí no hay para 32 bits) y los
 * argumentos, troceados como en el DOS (con comillas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "pentium.h"

void *memcpy(void *d, const void *o, size_t n);

/* ------------------------------------------------------------ la memoria */
typedef struct trozo {
	uint32_t tam;		/* con la cabecera; múltiplo de 16 */
	uint32_t libre;
	struct trozo *sig;	/* (los libres, por su dirección) */
	uint32_t relleno;
} trozo;
#define CAB ((uint32_t)sizeof(trozo))
static trozo *libres;
static uint32_t pedido;		/* lo que se ha pedido al núcleo */

uint32_t memoria_pedida(void) { return pedido; }

static void a_libres(trozo *t)
{
	trozo **p = &libres, *a = NULL;

	t->libre = 1;
	while (*p && *p < t) {
		a = *p;
		p = &(*p)->sig;
	}
	t->sig = *p;
	*p = t;
	/* con el de detrás y con el de delante, si están pegados */
	if (t->sig && (uint8_t *)t + t->tam == (uint8_t *)t->sig) {
		t->tam += t->sig->tam;
		t->sig = t->sig->sig;
	}
	if (a && (uint8_t *)a + a->tam == (uint8_t *)t) {
		a->tam += t->tam;
		a->sig = t->sig;
	}
}

static int crece(uint32_t n)
{
	uint32_t r, t = (n + 0x3FFFF) & ~0x3FFFFu;	/* de 256 K en 256 K */
	trozo *b;

	if (t < n)
		return -1;
	r = sc(SC_MEMORIA, t, 0, 0);
	if (ES_ERROR(r) || r == 0)
		return -1;
	pedido += t;
	b = (trozo *)((r + 15) & ~15u);
	b->tam = (t - ((uint32_t)b - r)) & ~15u;
	a_libres(b);
	return 0;
}

void *malloc(size_t n)
{
	uint32_t t;
	trozo **p, *b, *resto;

	if (n > 0x7FFFFFF0u)
		return NULL;
	t = ((uint32_t)n + CAB + 15) & ~15u;
	for (;;) {
		for (p = &libres; *p; p = &(*p)->sig)
			if ((*p)->tam >= t)
				break;
		if (*p)
			break;
		if (crece(t + CAB))
			return NULL;
	}
	b = *p;
	if (b->tam - t >= 64) {
		resto = (trozo *)((uint8_t *)b + t);
		resto->tam = b->tam - t;
		resto->libre = 1;
		resto->sig = b->sig;
		*p = resto;
		b->tam = t;
	} else
		*p = b->sig;
	b->libre = 0;
	b->sig = NULL;
	return (uint8_t *)b + CAB;
}

void free(void *q)
{
	trozo *t;

	if (!q)
		return;
	t = (trozo *)((uint8_t *)q - CAB);
	if (t->libre)
		return;		/* (dos veces: nada) */
	a_libres(t);
}

void *calloc(size_t a, size_t b)
{
	uint8_t *p;
	size_t i;

	if (b && a > 0x7FFFFFF0u / b)
		return NULL;
	p = malloc(a * b);
	if (p)
		for (i = 0; i < a * b; i++)
			p[i] = 0;
	return p;
}

void *realloc(void *q, size_t n)
{
	trozo *t;
	void *nuevo;

	if (!q)
		return malloc(n);
	t = (trozo *)((uint8_t *)q - CAB);
	if (n + CAB <= t->tam)
		return q;
	nuevo = malloc(n);
	if (nuevo) {
		memcpy(nuevo, q, t->tam - CAB);
		free(q);
	}
	return nuevo;
}

int abs(int x) { return x < 0 ? -x : x; }

/* -------------------------------------------------------------- letras */
void *memcpy(void *d, const void *o, size_t n)
{
	uint8_t *a = d;
	const uint8_t *b = o;

	if (!(((uint32_t)a | (uint32_t)b | n) & 3)) {
		uint32_t *x = d;
		const uint32_t *y = o;

		for (n >>= 2; n; n--)
			*x++ = *y++;
		return d;
	}
	while (n--)
		*a++ = *b++;
	return d;
}

void *memmove(void *d, const void *o, size_t n)
{
	uint8_t *a = d;
	const uint8_t *b = o;

	if (a <= b || a >= b + n)
		return memcpy(d, o, n);
	while (n--)
		a[n] = b[n];
	return d;
}

void *memset(void *d, int c, size_t n)
{
	uint8_t *a = d;

	while (n--)
		*a++ = (uint8_t)c;
	return d;
}

int memcmp(const void *x, const void *y, size_t n)
{
	const uint8_t *a = x, *b = y;

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
	return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (*a != *b)
			return (uint8_t)*a - (uint8_t)*b;
		if (!*a)
			return 0;
	}
	return 0;
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

/* --------------------------------------------- las cuentas de 64 bits */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *resto)
{
	uint64_t q = 0, r = 0;
	int i;

	if (d == 0)
		return 0;
	if (!(n >> 32) && !(d >> 32)) {
		if (resto)
			*resto = (uint32_t)n % (uint32_t)d;
		return (uint32_t)n / (uint32_t)d;
	}
	for (i = 63; i >= 0; i--) {
		r = r << 1 | ((n >> i) & 1);
		if (r >= d) {
			r -= d;
			q |= (uint64_t)1 << i;
		}
	}
	if (resto)
		*resto = r;
	return q;
}
uint64_t __udivdi3(uint64_t n, uint64_t d) { return __udivmoddi4(n, d, NULL); }
uint64_t __umoddi3(uint64_t n, uint64_t d)
{
	uint64_t r;

	__udivmoddi4(n, d, &r);
	return r;
}
int64_t __divdi3(int64_t n, int64_t d)
{
	int neg = (n < 0) != (d < 0);
	uint64_t q = __udivdi3(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d);

	return neg ? -(int64_t)q : (int64_t)q;
}
int64_t __moddi3(int64_t n, int64_t d)
{
	uint64_t r = __umoddi3(n < 0 ? -(uint64_t)n : (uint64_t)n, d < 0 ? -(uint64_t)d : (uint64_t)d);

	return n < 0 ? -(int64_t)r : (int64_t)r;
}

/* ------------------------------------------------------------ formatos */
int vsnprintf(char *s, size_t cabe, const char *f, va_list a)
{
	size_t n = 0;
	char num[24];

#define PON(c) do { if (n + 1 < cabe) s[n] = (c); n++; } while (0)
	for (; *f; f++) {
		int menos = 0, cero = 0, ancho = 0, largo = 0, k = 0, neg = 0, base = 10;
		const char *t = NULL;
		uint64_t v = 0;

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
			f++;
		} else
			while (*f >= '0' && *f <= '9')
				ancho = ancho * 10 + (*f++ - '0');
		while (*f == 'l') {
			largo++;
			f++;
		}
		switch (*f) {
		case 's':
			t = va_arg(a, const char *);
			if (!t)
				t = "(nada)";
			k = (int)strlen(t);
			break;
		case 'c':
			num[0] = (char)va_arg(a, int);
			t = num;
			k = 1;
			break;
		case 'd':
		case 'i':
			if (largo >= 2) {
				int64_t x = va_arg(a, int64_t);

				neg = x < 0;
				v = neg ? -(uint64_t)x : (uint64_t)x;
			} else {
				int32_t x = va_arg(a, int32_t);

				neg = x < 0;
				v = neg ? -(uint64_t)(int64_t)x : (uint64_t)x;
			}
			goto numero;
		case 'x':
		case 'X':
		case 'p':
			base = 16;
			/* (sigue) */
		case 'u':
			v = largo >= 2 ? va_arg(a, uint64_t) : va_arg(a, uint32_t);
numero:
			k = 0;
			do {
				int c = (int)(v % (unsigned)base);

				num[sizeof(num) - 1 - k++] = (char)(c < 10 ? '0' + c : (*f == 'X' ? 'A' : 'a') + c - 10);
				v /= (unsigned)base;
			} while (v);
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

/* --------------------------------------------------- los argumentos */
/* trocea «t» (en su sitio) en palabras; las comillas juntan */
int trocea(char *t, char **argv, int max)
{
	int n = 0;

	while (*t && n < max) {
		while (*t == ' ' || *t == '\t')
			t++;
		if (!*t)
			break;
		if (*t == '"') {
			argv[n++] = ++t;
			while (*t && *t != '"')
				t++;
		} else {
			argv[n++] = t;
			while (*t && *t != ' ' && *t != '\t')
				t++;
		}
		if (*t)
			*t++ = 0;
	}
	return n;
}
