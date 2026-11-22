/* texto.c - la oficina de 32 bits: los textos (oficina/comun/texto.h).
 * La página 437 es la de Hispa Ventanas y del escritorio de Hispa-Pentium
 * (tools/hispa437.py): la de siempre con Á, Í, Ó, Ú, À, È, €... en los
 * recuadros mezclados, que no se pintan.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "texto.h"

/* ---- UTF-8 ---- */
int utf8_lee(const u8 *s, u32 n, u32 *c)
{
	u32 v;
	int k, i;

	if (!n) {
		*c = 0;
		return 0;
	}
	if (s[0] < 0x80) {
		*c = s[0];
		return 1;
	}
	if ((s[0] & 0xE0) == 0xC0) {
		k = 2;
		v = s[0] & 0x1F;
	} else if ((s[0] & 0xF0) == 0xE0) {
		k = 3;
		v = s[0] & 0x0F;
	} else if ((s[0] & 0xF8) == 0xF0) {
		k = 4;
		v = s[0] & 0x07;
	} else {
		*c = 0xFFFD;
		return 1;
	}
	if ((u32)k > n) {
		*c = 0xFFFD;
		return 1;
	}
	for (i = 1; i < k; i++) {
		if ((s[i] & 0xC0) != 0x80) {
			*c = 0xFFFD;
			return 1;
		}
		v = v << 6 | (s[i] & 0x3F);
	}
	if (v > 0x10FFFF || (v >= 0xD800 && v < 0xE000) || (k == 2 && v < 0x80) || (k == 3 && v < 0x800) || (k == 4 && v < 0x10000))
		v = 0xFFFD;
	*c = v;
	return k;
}

int utf8_pon(u32 c, char *d)
{
	if (c > 0x10FFFF)
		c = 0xFFFD;
	if (c < 0x80) {
		d[0] = (char)c;
		return 1;
	}
	if (c < 0x800) {
		d[0] = (char)(0xC0 | c >> 6);
		d[1] = (char)(0x80 | (c & 63));
		return 2;
	}
	if (c < 0x10000) {
		d[0] = (char)(0xE0 | c >> 12);
		d[1] = (char)(0x80 | (c >> 6 & 63));
		d[2] = (char)(0x80 | (c & 63));
		return 3;
	}
	d[0] = (char)(0xF0 | c >> 18);
	d[1] = (char)(0x80 | (c >> 12 & 63));
	d[2] = (char)(0x80 | (c >> 6 & 63));
	d[3] = (char)(0x80 | (c & 63));
	return 4;
}

u32 utf8_cuenta(const char *s, u32 n)
{
	u32 i = 0, k = 0, c;

	while (i < n) {
		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		k++;
	}
	return k;
}

/* ---- la página 437 de Hispa Ventanas ---- */
static const u16 t437[128] = {
	0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
	0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
	0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x2563, 0x2551, 0x2557, 0x255D, 0x00C8, 0x00CA, 0x2510,
	0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x00CB, 0x00CC, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x00CD,
	0x00CE, 0x00CF, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D9, 0x00DA, 0x00DB, 0x2518, 0x250C, 0x2588, 0x2584, 0x00E3, 0x00F5, 0x2580,
	0x03B1, 0x00DF, 0x0152, 0x03C0, 0x03A3, 0x0153, 0x00B5, 0x00A9, 0x00AE, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
	0x2261, 0x00B1, 0x2265, 0x2264, 0x20AC, 0x2026, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
};

u32 de437(u8 c)
{
	return c < 0x80 ? c : t437[c - 0x80];
}

u8 a437(u32 c)
{
	int i;

	if (c < 0x80)
		return (u8)c;
	for (i = 0; i < 128; i++)
		if (t437[i] == c)
			return (u8)(0x80 + i);
	switch (c) {
	case 0x2018: case 0x2019: case 0x201A: return '\'';
	case 0x201C: case 0x201D: case 0x201E: return '"';
	case 0x2013: case 0x2014: case 0x2212: return '-';
	case 0x2022: return 0xF9;
	case 0x2028: return '\n';
	}
	c = sin_tilde(c);
	return c < 0x80 ? (u8)c : '?';
}

u32 utf8_a_437(const char *s, char *d, u32 cabe)
{
	u32 n = largo(s), i = 0, k = 0, c;

	if (!cabe)
		return 0;
	while (i < n && k + 1 < cabe) {
		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		d[k++] = (char)a437(c);
	}
	d[k] = 0;
	return k;
}

u32 de437_a_utf8(const u8 *s, u32 n, char *d, u32 cabe)
{
	u32 i, k = 0;
	char t[4];

	if (!cabe)
		return 0;
	for (i = 0; i < n; i++) {
		int m = utf8_pon(de437(s[i]), t);

		if (k + (u32)m + 1 > cabe)
			break;
		memcpy(d + k, t, (size_t)m);
		k += (u32)m;
	}
	d[k] = 0;
	return k;
}

/* ---- letras ---- */
u32 minuscula(u32 c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
		return c + 32;
	if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x178 && c != 0x17F) {
		if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
			return (c & 1) ? c + 1 : c;
		return (c & 1) ? c : c + 1;
	}
	if (c == 0x178)
		return 0xFF;
	if (c >= 0x391 && c <= 0x3A9)
		return c + 32;
	if (c >= 0x410 && c <= 0x42F)
		return c + 32;
	return c;
}

u32 mayuscula(u32 c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7))
		return c - 32;
	if (c == 0xFF)
		return 0x178;
	if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x17F) {
		if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E))
			return (c & 1) ? c : c - 1;
		return (c & 1) ? c - 1 : c;
	}
	if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2)
		return c - 32;
	if (c >= 0x430 && c <= 0x44F)
		return c - 32;
	return c;
}

u32 sin_tilde(u32 c)
{
	static const char m[] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";

	if (c >= 0xC0 && c <= 0xFF) {
		char r = m[c - 0xC0];

		return (r == 'x' || r == '/' || r == 'T' || r == 't' || r == 's') ? c : (u32)(u8)r;
	}
	if (c == 0x152)
		return 'O';
	if (c == 0x153)
		return 'o';
	return c;
}

int es_letra(u32 c)
{
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
		return 1;
	if (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7)
		return 1;
	if (c == 0xAA || c == 0xBA || c == 0xB5)
		return 1;
	if (c >= 0x370 && c <= 0x52F)
		return 1;
	return 0;
}

int es_espacio(u32 c)
{
	return c == ' ' || c == '\t' || c == 0xA0 || c == 0x2028 || (c >= 0x2000 && c <= 0x200B);
}

/* ---- cadenas de C ---- */
u32 largo(const char *s)
{
	u32 n = 0;

	while (s[n])
		n++;
	return n;
}

void copia(char *d, const char *s, u32 cabe)
{
	u32 i = 0;

	if (!cabe)
		return;
	while (s[i] && i + 1 < cabe) {
		d[i] = s[i];
		i++;
	}
	d[i] = 0;
}

void anade(char *d, const char *s, u32 cabe)
{
	u32 n = largo(d);

	if (n < cabe)
		copia(d + n, s, cabe - n);
}

int iguales(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static int mayus_ascii(int c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int iguales_sin_caja(const char *a, const char *b)
{
	while (*a && mayus_ascii((u8)*a) == mayus_ascii((u8)*b))
		a++, b++;
	return mayus_ascii((u8)*a) == mayus_ascii((u8)*b);
}

int empieza(const char *s, const char *p)
{
	while (*p)
		if (*s++ != *p++)
			return 0;
	return 1;
}

const char *busca(const char *s, const char *p)
{
	u32 n = largo(p), m = largo(s), i;

	if (!n)
		return s;
	for (i = 0; i + n <= m; i++)
		if (!memcmp(s + i, p, n))
			return s + i;
	return NULL;
}

char *duplica_n(const char *s, u32 n)
{
	char *d = so_pide(n + 1);

	if (!d)
		return NULL;
	memcpy(d, s, n);
	d[n] = 0;
	return d;
}

char *duplica(const char *s)
{
	return duplica_n(s, largo(s));
}

void extension(const char *ruta, char *ext)
{
	const char *p = NULL, *s;
	int k = 0;

	for (s = ruta; *s; s++)
		if (*s == '.')
			p = s + 1;
		else if (*s == '\\' || *s == '/')
			p = NULL;
	if (p)
		for (; *p && k < 7; p++)
			ext[k++] = (char)mayus_ascii((u8)*p);
	ext[k] = 0;
}

const char *sin_carpeta(const char *ruta)
{
	const char *r = ruta, *s;

	for (s = ruta; *s; s++)
		if (*s == '\\' || *s == '/' || *s == ':')
			r = s + 1;
	return r;
}

long numero(const char *s, const char **fin)
{
	long v = 0;
	int neg = 0;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-' || *s == '+')
		neg = *s++ == '-';
	while (*s >= '0' && *s <= '9') {
		if (v < 100000000L)
			v = v * 10 + (*s - '0');
		s++;
	}
	if (fin)
		*fin = s;
	return neg ? -v : v;
}

double numero_real(const char *s, const char **fin)
{
	double v = 0, f = 0.1;
	int neg = 0;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-' || *s == '+')
		neg = *s++ == '-';
	while (*s >= '0' && *s <= '9')
		v = v * 10 + (*s++ - '0');
	if (*s == '.' || *s == ',') {
		s++;
		while (*s >= '0' && *s <= '9') {
			v += (*s++ - '0') * f;
			f /= 10;
		}
	}
	if (*s == 'e' || *s == 'E') {
		const char *q = s + 1;
		long e = numero(q, &q);

		if (q != s + 1) {
			s = q;
			while (e > 0 && e--)
				v *= 10;
			while (e < 0 && e++)
				v /= 10;
		}
	}
	if (fin)
		*fin = s;
	return neg ? -v : v;
}

/* ---- printf pequeño ---- */
typedef struct {
	char *d;
	u32 cabe, n;
} salida;

static void sal_c(salida *s, char c)
{
	if (s->n + 1 < s->cabe)
		s->d[s->n] = c;
	s->n++;
}

static void sal_num(salida *s, u32 v, int base, int neg, int ancho, int cero, int izq, int mayus)
{
	char t[12];
	int k = 0, i, l;
	const char *dig = mayus ? "0123456789ABCDEF" : "0123456789abcdef";

	do {
		t[k++] = dig[v % (u32)base];
		v /= (u32)base;
	} while (v);
	l = k + neg;
	if (!izq && !cero)
		for (i = l; i < ancho; i++)
			sal_c(s, ' ');
	if (neg)
		sal_c(s, '-');
	if (!izq && cero)
		for (i = l; i < ancho; i++)
			sal_c(s, '0');
	while (k)
		sal_c(s, t[--k]);
	if (izq)
		for (i = l; i < ancho; i++)
			sal_c(s, ' ');
}

int vfmt(char *d, u32 cabe, const char *f, va_list a)
{
	salida s = { d, cabe, 0 };

	for (; *f; f++) {
		int ancho = 0, cero = 0, izq = 0, prec = -1;

		if (*f != '%') {
			sal_c(&s, *f);
			continue;
		}
		f++;
		if (*f == '-') {
			izq = 1;
			f++;
		}
		if (*f == '0') {
			cero = 1;
			f++;
		}
		if (*f == '*') {
			ancho = va_arg(a, int);
			f++;
		}
		while (*f >= '0' && *f <= '9')
			ancho = ancho * 10 + (*f++ - '0');
		if (*f == '.') {
			f++;
			prec = 0;
			if (*f == '*') {
				prec = va_arg(a, int);
				f++;
			}
			while (*f >= '0' && *f <= '9')
				prec = prec * 10 + (*f++ - '0');
		}
		if (*f == 'l')
			f++;
		switch (*f) {
		case 'd': {
			int v = va_arg(a, int);

			sal_num(&s, v < 0 ? (u32)-(i64)v : (u32)v, 10, v < 0, ancho, cero, izq, 0);
			break;
		}
		case 'u':
			sal_num(&s, va_arg(a, u32), 10, 0, ancho, cero, izq, 0);
			break;
		case 'x':
		case 'X':
			sal_num(&s, va_arg(a, u32), 16, 0, ancho, cero, izq, *f == 'X');
			break;
		case 'c':
			sal_c(&s, (char)va_arg(a, int));
			break;
		case 's': {
			const char *t = va_arg(a, const char *);
			int l, i;

			if (!t)
				t = "(nada)";
			l = (int)largo(t);
			if (prec >= 0 && l > prec)
				l = prec;
			if (!izq)
				for (i = l; i < ancho; i++)
					sal_c(&s, ' ');
			for (i = 0; i < l; i++)
				sal_c(&s, t[i]);
			if (izq)
				for (i = l; i < ancho; i++)
					sal_c(&s, ' ');
			break;
		}
		case 'S': {
			const u32 *t = va_arg(a, const u32 *);
			char u[4];

			for (; t && *t; t++) {
				int m = utf8_pon(*t, u), i;

				for (i = 0; i < m; i++)
					sal_c(&s, u[i]);
			}
			break;
		}
		case 'f': {
			double v = va_arg(a, double);
			u32 e, m = 1;
			int i, neg = v < 0;

			if (prec < 0)
				prec = 6;
			if (prec > 9)
				prec = 9;
			for (i = 0; i < prec; i++)
				m *= 10;
			if (neg)
				v = -v;
			if (v > 4e9)
				v = 4e9;
			v = v * m + 0.5;
			if (v > 4.2e9) {
				/* (no cabe con los decimales: sin ellos) */
				prec = 0;
				m = 1;
				v = v / 1;
			}
			e = (u32)(v / m);
			sal_num(&s, e, 10, neg && (e || (u32)v), ancho > prec + 1 ? ancho - prec - (prec > 0) : 0, cero, 0, 0);
			if (prec) {
				u32 r = (u32)v - e * m;

				sal_c(&s, '.');
				sal_num(&s, r, 10, 0, prec, 1, 0, 0);
			}
			break;
		}
		case '%':
			sal_c(&s, '%');
			break;
		default:
			if (!*f)
				f--;
			break;
		}
	}
	if (s.cabe)
		s.d[s.n < s.cabe ? s.n : s.cabe - 1] = 0;
	return (int)s.n;
}

int fmt(char *d, u32 cabe, const char *f, ...)
{
	va_list a;
	int r;

	va_start(a, f);
	r = vfmt(d, cabe, f, a);
	va_end(a);
	return r;
}

void dice(const char *f, ...)
{
	char t[600];
	va_list a;

	va_start(a, f);
	vfmt(t, sizeof t - 1, f, a);
	va_end(a);
	anade(t, "\n", sizeof t);
	so_dice(t);
}

/* ---- el búfer que crece ---- */
void buf_pon(bufer *b, const void *d, u32 n)
{
	if (b->mal || !n)
		return;
	if (b->n + n > b->cabe) {
		u32 c = b->cabe ? b->cabe : 256;
		u8 *nd;

		while (c < b->n + n) {
			if (c > 0x40000000u) {
				b->mal = 1;
				return;
			}
			c *= 2;
		}
		nd = so_repide(b->d, c);
		if (!nd) {
			b->mal = 1;
			return;
		}
		b->d = nd;
		b->cabe = c;
	}
	memcpy(b->d + b->n, d, n);
	b->n += n;
}

void buf_txt(bufer *b, const char *s)
{
	buf_pon(b, s, largo(s));
}

void buf_c(bufer *b, u8 c)
{
	buf_pon(b, &c, 1);
}

void buf_fmt(bufer *b, const char *f, ...)
{
	char t[1024];
	va_list a;
	int n;

	va_start(a, f);
	n = vfmt(t, sizeof t, f, a);
	va_end(a);
	if (n >= (int)sizeof t)
		n = sizeof t - 1;
	buf_pon(b, t, (u32)n);
}

void buf_xml(bufer *b, const char *s, u32 n)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++) {
		const char *r = NULL;

		switch (s[i]) {
		case '&': r = "&amp;"; break;
		case '<': r = "&lt;"; break;
		case '>': r = "&gt;"; break;
		case '"': r = "&quot;"; break;
		}
		if ((u8)s[i] < 0x20 && s[i] != '\t' && s[i] != '\n' && s[i] != '\r')
			r = "";		/* (lo que XML no deja) */
		if (r) {
			buf_pon(b, s + k, i - k);
			buf_txt(b, r);
			k = i + 1;
		}
	}
	buf_pon(b, s + k, n - k);
}

void buf_u32(bufer *b, u32 c)
{
	char t[4];

	buf_pon(b, t, (u32)utf8_pon(c, t));
}

void buf_le16(bufer *b, u32 v)
{
	u8 t[2] = { (u8)v, (u8)(v >> 8) };

	buf_pon(b, t, 2);
}

void buf_le32(bufer *b, u32 v)
{
	u8 t[4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) };

	buf_pon(b, t, 4);
}

void buf_suelta(bufer *b)
{
	so_suelta(b->d);
	b->d = NULL;
	b->n = b->cabe = 0;
	b->mal = 0;
}

/* ---- UTF-32 ---- */
u32 u32_largo(const u32 *s)
{
	u32 n = 0;

	while (s[n])
		n++;
	return n;
}

u32 *u32_de_utf8(const char *s, u32 bytes, u32 *n)
{
	u32 *d = so_pide((bytes + 1) * 4), i = 0, k = 0;

	if (!d)
		return NULL;
	while (i < bytes) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)s + i, bytes - i, &c);
		d[k++] = c;
	}
	d[k] = 0;
	if (n)
		*n = k;
	return d;
}

char *utf8_de_u32(const u32 *s, u32 n)
{
	char *d = so_pide(n * 4 + 1);
	u32 i, k = 0;

	if (!d)
		return NULL;
	for (i = 0; i < n; i++)
		k += (u32)utf8_pon(s[i], d + k);
	d[k] = 0;
	return d;
}
