/* libc.c - WIN32.BIN: lo poco de la biblioteca de C que usan sus piezas
 * (sin biblioteca del sistema: -ffreestanding) y el formato de wsprintfA.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "w32.h"

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
int mayus(int c)
{
	c &= 255;
	if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7))
		return c - 32;
	return c;
}
int minus(int c)
{
	c &= 255;
	if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
		return c + 32;
	return c;
}
int stricmp_(const char *a, const char *b)
{
	while (*a && mayus(*a) == mayus(*b))
		a++, b++;
	return mayus(*a) - mayus(*b);
}
int strnicmp_(const char *a, const char *b, size_t n)
{
	for (; n; n--, a++, b++) {
		if (mayus(*a) != mayus(*b))
			return mayus(*a) - mayus(*b);
		if (!*a)
			return 0;
	}
	return 0;
}
char *strcpy(char *d, const char *s)
{
	char *r = d;
	while ((*d++ = *s++))
		;
	return r;
}
char *strcat(char *d, const char *s)
{
	strcpy(d + strlen(d), s);
	return d;
}
char *strchr(const char *s, int c)
{
	for (; *s; s++)
		if (*s == (char)c)
			return (char *)s;
	return c ? 0 : (char *)s;
}
char *strrchr(const char *s, int c)
{
	const char *r = 0;
	for (; *s; s++)
		if (*s == (char)c)
			r = s;
	return (char *)r;
}
void copia_n(char *d, const char *s, u32 cabe)
{
	u32 i;
	if (!cabe)
		return;
	for (i = 0; i + 1 < cabe && s && s[i]; i++)
		d[i] = s[i];
	d[i] = 0;
}

/* ---- el formato de wsprintfA (y de lo nuestro): %[-][0][#][ancho][.prec][h|l]
 * s c d i u x X p; sin coma flotante, como el de Windows ---- */
static void pon(char *d, u32 *k, u32 cabe, char c)
{
	if (*k + 1 < cabe)
		d[*k] = c;
	(*k)++;
}
int formato_win(char *d, u32 cabe, const char *p, va_list va)
{
	u32 k = 0;
	for (; *p; p++) {
		char t[16];
		int izq = 0, ceros = 0, alt = 0, ancho = 0, prec = -1, i, n;
		u32 v;
		const char *s;
		if (*p != '%') {
			pon(d, &k, cabe, *p);
			continue;
		}
		p++;
		for (;; p++) {
			if (*p == '-') izq = 1;
			else if (*p == '0') ceros = 1;
			else if (*p == '#') alt = 1;
			else if (*p == ' ' || *p == '+') ;
			else break;
		}
		if (*p == '*') { ancho = va_arg(va, int); p++; }
		while (*p >= '0' && *p <= '9')
			ancho = ancho * 10 + (*p++ - '0');
		if (*p == '.') {
			p++;
			prec = 0;
			if (*p == '*') { prec = va_arg(va, int); p++; }
			while (*p >= '0' && *p <= '9')
				prec = prec * 10 + (*p++ - '0');
		}
		while (*p == 'l' || *p == 'h' || *p == 'L' || *p == 'w')
			p++;
		switch (*p) {
		case 's': case 'S':
			s = va_arg(va, const char *);
			if (!s)
				s = "(null)";
			for (n = 0; s[n] && (prec < 0 || n < prec); n++)
				;
			if (!izq)
				for (i = n; i < ancho; i++)
					pon(d, &k, cabe, ' ');
			for (i = 0; i < n; i++)
				pon(d, &k, cabe, s[i]);
			if (izq)
				for (i = n; i < ancho; i++)
					pon(d, &k, cabe, ' ');
			break;
		case 'c': case 'C':
			pon(d, &k, cabe, (char)va_arg(va, int));
			break;
		case 'd': case 'i': case 'u': case 'x': case 'X': case 'p': {
			int neg = 0, b = (*p == 'x' || *p == 'X' || *p == 'p') ? 16 : 10;
			const char *dig = *p == 'x' ? "0123456789abcdef" : "0123456789ABCDEF";
			v = va_arg(va, u32);
			if (*p == 'p') { ceros = 1; if (ancho < 8) ancho = 8; }
			if ((*p == 'd' || *p == 'i') && (i32)v < 0) { neg = 1; v = -(i32)v; }
			n = 0;
			do { t[n++] = dig[v % b]; v /= b; } while (v);
			while (prec > n && n < 15)
				t[n++] = '0';
			if (alt && b == 16) { t[n++] = *p == 'x' ? 'x' : 'X'; t[n++] = '0'; }
			if (neg)
				ancho--;
			if (!izq && !ceros)
				for (i = n; i < ancho; i++)
					pon(d, &k, cabe, ' ');
			if (neg)
				pon(d, &k, cabe, '-');
			if (!izq && ceros)
				for (i = n; i < ancho; i++)
					pon(d, &k, cabe, '0');
			for (i = n; i; i--)
				pon(d, &k, cabe, t[i - 1]);
			if (izq)
				for (i = n; i < ancho; i++)
					pon(d, &k, cabe, ' ');
			break;
		}
		case 0:
			p--;
			break;
		default:
			pon(d, &k, cabe, *p);
		}
	}
	if (cabe)
		d[k < cabe ? k : cabe - 1] = 0;
	return k < cabe ? (int)k : (int)cabe - 1;
}
int fmt(char *d, u32 cabe, const char *f, ...)
{
	va_list va;
	int r;
	va_start(va, f);
	r = formato_win(d, cabe, f, va);
	va_end(va);
	return r;
}

int traza_si;
void traza(const char *f, ...)
{
	char b[300];
	va_list va;
	if (!traza_si)
		return;
	va_start(va, f);
	formato_win(b, sizeof b - 2, f, va);
	va_end(va);
	strcat(b, "\n");
	sys_serie("[w32] ");
	sys_serie(b);
}
void aviso_serie(const char *f, ...)
{
	char b[300];
	va_list va;
	va_start(va, f);
	formato_win(b, sizeof b - 2, f, va);
	va_end(va);
	strcat(b, "\n");
	sys_serie("[w32] ");
	sys_serie(b);
}

/* ---- las divisiones de 64 bits que pide gcc (no hay libgcc de 32 bits) ---- */
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
	for (k = 63; k >= 0; k--) {
		if ((n >> k) >= d) {
			n -= d << k;
			q |= 1ull << k;
		}
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
