/* util.c - el Reproductor: lo pequeño que usan todos, portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "util.h"

void copia(char *d, int cabe, const char *o)
{
	int i;

	if (cabe <= 0)
		return;
	for (i = 0; i < cabe - 1 && o && o[i]; i++)
		d[i] = o[i];
	d[i] = 0;
	/* (que no quede media letra de UTF-8 al final) */
	if (o && o[i] && i > 0) {
		int k = i;

		while (k > 0 && ((u8)d[k - 1] & 0xC0) == 0x80)
			k--;
		if (k > 0 && (u8)d[k - 1] >= 0xC0) {
			int largo = (u8)d[k - 1] >= 0xF0 ? 4 : (u8)d[k - 1] >= 0xE0 ? 3 : 2;

			if (i - (k - 1) < largo)
				d[k - 1] = 0;
		}
	}
}

void anade(char *d, int cabe, const char *o)
{
	int n = (int)strlen(d);

	if (n < cabe)
		copia(d + n, cabe - n, o);
}

static int pon_num(char *t, u64 v, int base, int mayu)
{
	char b[24];
	int n = 0, i;
	const char *cif = mayu ? "0123456789ABCDEF" : "0123456789abcdef";

	do {
		b[n++] = cif[v % (u64)base];
		v /= (u64)base;
	} while (v);
	for (i = 0; i < n; i++)
		t[i] = b[n - 1 - i];
	return n;
}

int vfmt(char *d, int cabe, const char *f, va_list va)
{
	int k = 0;

#define PON(c) do { if (k < cabe - 1) d[k] = (c); k++; } while (0)
	for (; *f; f++) {
		char num[32], relleno = ' ';
		int ancho = 0, largos = 0, n = 0, neg = 0, izq = 0, i;
		u64 v = 0;
		const char *s;

		if (*f != '%') {
			PON(*f);
			continue;
		}
		f++;
		if (*f == '-') {
			izq = 1;
			f++;
		}
		if (*f == '0') {
			relleno = '0';
			f++;
		}
		while (*f >= '0' && *f <= '9')
			ancho = ancho * 10 + (*f++ - '0');
		while (*f == 'l') {
			largos++;
			f++;
		}
		switch (*f) {
		case 'd':
			if (largos >= 2) {
				i64 x = va_arg(va, i64);

				neg = x < 0;
				v = neg ? (u64)-x : (u64)x;
			} else {
				i32 x = (i32)va_arg(va, int);

				neg = x < 0;
				v = neg ? (u64)(-(i64)x) : (u64)x;
			}
			if (neg)
				num[n++] = '-';
			n += pon_num(num + n, v, 10, 0);
			break;
		case 'u':
		case 'x':
		case 'X':
			v = largos >= 2 ? va_arg(va, u64) : (u64)va_arg(va, unsigned);
			n = pon_num(num, v, *f == 'u' ? 10 : 16, *f == 'X');
			break;
		case 'c':
			num[n++] = (char)va_arg(va, int);
			break;
		case 's':
			s = va_arg(va, const char *);
			if (!s)
				s = "(nada)";
			n = (int)strlen(s);
			if (!izq)
				for (i = n; i < ancho; i++)
					PON(' ');
			for (i = 0; i < n; i++)
				PON(s[i]);
			if (izq)
				for (i = n; i < ancho; i++)
					PON(' ');
			continue;
		case '%':
			PON('%');
			continue;
		default:
			PON('%');
			PON(*f);
			continue;
		}
		if (!izq && relleno == '0' && neg) {
			PON('-');
			for (i = n; i < ancho; i++)
				PON('0');
			for (i = 1; i < n; i++)
				PON(num[i]);
			continue;
		}
		if (!izq)
			for (i = n; i < ancho; i++)
				PON(relleno);
		for (i = 0; i < n; i++)
			PON(num[i]);
		if (izq)
			for (i = n; i < ancho; i++)
				PON(' ');
	}
	if (cabe > 0)
		d[k < cabe ? k : cabe - 1] = 0;
#undef PON
	return k;
}

int fmt(char *d, int cabe, const char *f, ...)
{
	va_list va;
	int n;

	va_start(va, f);
	n = vfmt(d, cabe, f, va);
	va_end(va);
	return n;
}

void reg(const char *f, ...)
{
	char t[400];
	va_list va;

	copia(t, sizeof t, "reproductor: ");
	va_start(va, f);
	vfmt(t + 13, (int)sizeof t - 14, f, va);
	va_end(va);
	anade(t, sizeof t, "\n");
	so_registro(t);
}

int mayus(int c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int igual_sin_caja(const char *a, const char *b)
{
	while (*a && mayus((u8)*a) == mayus((u8)*b))
		a++, b++;
	return !*a && !*b;
}

int empieza_sin_caja(const char *t, const char *p)
{
	while (*p && mayus((u8)*t) == mayus((u8)*p))
		t++, p++;
	return !*p;
}

const char *busca_sin_caja(const char *t, const char *p)
{
	for (; *t; t++)
		if (empieza_sin_caja(t, p))
			return t;
	return NULL;
}

u32 utf8_sig(const char **pp)
{
	const u8 *p = (const u8 *)*pp;
	u32 u;
	int n, i;

	if (!*p)
		return 0;
	if (p[0] < 0x80) {
		*pp += 1;
		return p[0];
	}
	if ((p[0] & 0xE0) == 0xC0)
		n = 1, u = p[0] & 31;
	else if ((p[0] & 0xF0) == 0xE0)
		n = 2, u = p[0] & 15;
	else if ((p[0] & 0xF8) == 0xF0)
		n = 3, u = p[0] & 7;
	else {
		*pp += 1;
		return '?';
	}
	for (i = 1; i <= n; i++) {
		if ((p[i] & 0xC0) != 0x80) {
			*pp += i;
			return '?';
		}
		u = u << 6 | (p[i] & 63);
	}
	*pp += n + 1;
	return u;
}

int utf8_pon(u32 u, char *d)
{
	if (u < 0x80) {
		d[0] = (char)u;
		return 1;
	}
	if (u < 0x800) {
		d[0] = (char)(0xC0 | u >> 6);
		d[1] = (char)(0x80 | (u & 63));
		return 2;
	}
	if (u < 0x10000) {
		d[0] = (char)(0xE0 | u >> 12);
		d[1] = (char)(0x80 | ((u >> 6) & 63));
		d[2] = (char)(0x80 | (u & 63));
		return 3;
	}
	d[0] = (char)(0xF0 | u >> 18);
	d[1] = (char)(0x80 | ((u >> 12) & 63));
	d[2] = (char)(0x80 | ((u >> 6) & 63));
	d[3] = (char)(0x80 | (u & 63));
	return 4;
}

void latin1_a_utf8(const u8 *e, int n, char *d, int cabe)
{
	int i, k = 0;

	for (i = 0; i < n && e[i] && k + 4 < cabe; i++)
		k += utf8_pon(e[i], d + k);
	d[k] = 0;
}

void utf16_a_utf8(const u8 *e, int n, int le, char *d, int cabe)
{
	int i, k = 0;

	for (i = 0; i + 1 < n && k + 5 < cabe; i += 2) {
		u32 u = le ? (u32)e[i] | (u32)e[i + 1] << 8 : (u32)e[i] << 8 | e[i + 1];

		if (!u)
			break;
		if (u >= 0xD800 && u < 0xDC00 && i + 3 < n) {
			u32 v = le ? (u32)e[i + 2] | (u32)e[i + 3] << 8 : (u32)e[i + 2] << 8 | e[i + 3];

			if (v >= 0xDC00 && v < 0xE000) {
				u = 0x10000 + ((u - 0xD800) << 10) + (v - 0xDC00);
				i += 2;
			}
		}
		if (u == 0xFEFF)
			continue;
		k += utf8_pon(u, d + k);
	}
	d[k] = 0;
}

int es_utf8(const u8 *e, int n)
{
	int i = 0, alto = 0;

	while (i < n && e[i]) {
		if (e[i] < 0x80) {
			i++;
			continue;
		}
		{
			int m = (e[i] & 0xE0) == 0xC0 ? 1 : (e[i] & 0xF0) == 0xE0 ? 2 : (e[i] & 0xF8) == 0xF0 ? 3 : -1, j;

			if (m < 0 || i + m >= n + 1)
				return 0;
			for (j = 1; j <= m; j++)
				if (i + j >= n || (e[i + j] & 0xC0) != 0x80)
					return 0;
			i += m + 1;
			alto = 1;
		}
	}
	return alto || 1;
}

/* sin tildes y en minúsculas */
u32 llana(u32 u)
{
	static const char *de = "ÀÁÂÃÄÅàáâãäåÈÉÊËèéêëÌÍÎÏìíîïÒÓÔÕÖòóôõöÙÚÛÜùúûüÑñÇç";
	static const char a[] = "aaaaaaaaaaaaeeeeeeeeiiiiiiiioooooooooouuuuuuuunncc";
	const char *p = de;
	int i = 0;

	if (u >= 'A' && u <= 'Z')
		return u + 32;
	if (u < 0x80)
		return u;
	while (*p) {
		if (utf8_sig(&p) == u)
			return (u8)a[i];
		i++;
	}
	return u;
}

int compara_llano(const char *a, const char *b)
{
	for (;;) {
		u32 x = llana(utf8_sig(&a)), y = llana(utf8_sig(&b));

		if (x != y || !x)
			return (int)x - (int)y;
	}
}

const char *contiene_llano(const char *t, const char *p)
{
	for (; *t; t++) {
		const char *a = t, *b = p;

		for (;;) {
			u32 y;

			if (!*b)
				return t;
			y = llana(utf8_sig(&b));
			if (llana(utf8_sig(&a)) != y)
				break;
		}
	}
	return NULL;
}

void a_tiempo(u32 s, char *d, int cabe)
{
	if (s >= 3600)
		fmt(d, cabe, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
	else
		fmt(d, cabe, "%02u:%02u", s / 60, s % 60);
}

int a_entero(const char *t)
{
	int v = 0, neg = 0;

	while (*t == ' ')
		t++;
	if (*t == '-' || *t == '+')
		neg = *t++ == '-';
	while (*t >= '0' && *t <= '9')
		v = v * 10 + (*t++ - '0');
	return neg ? -v : v;
}

int decimas_de(const char *t)
{
	int v = 0, neg = 0, dec = 0, hay = 0;

	while (*t == ' ')
		t++;
	if (*t == '-' || *t == '+')
		neg = *t++ == '-';
	while (*t >= '0' && *t <= '9')
		v = v * 10 + (*t++ - '0');
	if (*t == ',' || *t == '.') {
		t++;
		if (*t >= '0' && *t <= '9') {
			dec = *t++ - '0';
			hay = 1;
			if (*t >= '5' && *t <= '9')
				dec++;
		}
	}
	(void)hay;
	v = v * 10 + dec;
	return neg ? -v : v;
}

const char *base_de(const char *ruta)
{
	const char *p = ruta + strlen(ruta);

	while (p > ruta && p[-1] != '\\' && p[-1] != '/' && p[-1] != ':')
		p--;
	return p;
}

void carpeta_de(const char *ruta, char *d, int cabe)
{
	const char *b = base_de(ruta);
	int n = (int)(b - ruta);

	if (n >= cabe)
		n = cabe - 1;
	memcpy(d, ruta, (size_t)n);
	d[n] = 0;
}

static u32 semilla = 0x12345678u;

void siembra(u32 s)
{
	semilla = s ? s : 1;
}

u32 azar(void)
{
	/* xorshift32 */
	semilla ^= semilla << 13;
	semilla ^= semilla >> 17;
	semilla ^= semilla << 5;
	return semilla;
}
