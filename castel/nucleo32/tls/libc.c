/* libc.c - TLS.BIN: lo poco de la biblioteca de C que piden BearSSL y tls.c
 * (sin biblioteca del sistema: -ffreestanding), y un formatear() sencillo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stddef.h>
#include <stdarg.h>
#include "tls.h"

void *memcpy(void *d, const void *o, size_t n)
{
	u8 *a = d;
	const u8 *b = o;

	if ((((u32)a | (u32)b) & 3) == 0) {
		for (; n >= 4; n -= 4, a += 4, b += 4)
			*(u32 *)a = *(const u32 *)b;
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

/* ---- formatear: %s %u %d %x %02u %c (y %%) ---- */
static int pon(char *d, int k, int cabe, char c)
{
	if (k < cabe - 1)
		d[k] = c;
	return k + 1;
}

int vformatear(char *d, int cabe, const char *p, va_list va)
{
	int k = 0;

	for (; *p; p++) {
		char t[12];
		int ancho = 0, ceros = 0, i, n;
		u32 v;
		const char *s;

		if (*p != '%') {
			k = pon(d, k, cabe, *p);
			continue;
		}
		p++;
		if (*p == '0')
			ceros = 1, p++;
		while (*p >= '0' && *p <= '9')
			ancho = ancho * 10 + (*p++ - '0');
		switch (*p) {
		case 's':
			s = va_arg(va, const char *);
			for (n = 0; s[n]; n++) ;
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ' ');
			for (i = 0; i < n; i++)
				k = pon(d, k, cabe, s[i]);
			break;
		case 'c':
			k = pon(d, k, cabe, (char)va_arg(va, int));
			break;
		case 'd':
		case 'u':
		case 'x':
			v = va_arg(va, u32);
			n = 0;
			if (*p == 'd' && (int)v < 0) {
				k = pon(d, k, cabe, '-');
				v = -(int)v;
			}
			do {
				u32 b = *p == 'x' ? 16 : 10;
				t[n++] = "0123456789abcdef"[v % b];
				v /= b;
			} while (v);
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ceros ? '0' : ' ');
			while (n)
				k = pon(d, k, cabe, t[--n]);
			break;
		default:
			k = pon(d, k, cabe, *p);
			if (!*p)
				p--;
		}
	}
	if (cabe > 0)
		d[k < cabe ? k : cabe - 1] = 0;
	return k < cabe ? k : cabe - 1;
}

int formatear(char *d, int cabe, const char *p, ...)
{
	va_list va;
	int r;

	va_start(va, p);
	r = vformatear(d, cabe, p, va);
	va_end(va);
	return r;
}
