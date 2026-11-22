/* util.c - Hispa-PDF: formatear textos (sin la biblioteca de C: %d, %u, %x,
 * %s, %c, %f con sus decimales, %%; ancho y ceros), decir cosas y comparar
 * sin mayúsculas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "hpdf.h"

static int pon(char *d, int k, int cabe, char c)
{
	if (k < cabe - 1)
		d[k] = c;
	return k + 1;
}

int vfmt(char *d, int cabe, const char *p, va_list va)
{
	int k = 0;

	for (; *p; p++) {
		char t[48];
		int ancho = 0, ceros = 0, dec = -1, i, n = 0, izq = 0;

		if (*p != '%') {
			k = pon(d, k, cabe, *p);
			continue;
		}
		p++;
		if (*p == '-') {
			izq = 1;
			p++;
		}
		if (*p == '0') {
			ceros = 1;
			p++;
		}
		while (*p >= '0' && *p <= '9')
			ancho = ancho * 10 + (*p++ - '0');
		if (*p == '.') {
			dec = 0;
			p++;
			while (*p >= '0' && *p <= '9')
				dec = dec * 10 + (*p++ - '0');
		}
		if (*p == 'l')
			p++;
		switch (*p) {
		case 'd':
		case 'u':
		case 'x': {
			u32 v;
			int menos = 0, base = *p == 'x' ? 16 : 10;

			if (*p == 'd') {
				i32 s = va_arg(va, i32);

				if (s < 0) {
					menos = 1;
					v = (u32)(-(s + 1)) + 1;
				} else
					v = (u32)s;
			} else
				v = va_arg(va, u32);
			do {
				t[n++] = "0123456789abcdef"[v % base];
				v /= base;
			} while (v);
			if (menos)
				t[n++] = '-';
			break;
		}
		case 'f': {
			double v = va_arg(va, double);
			int menos = v < 0, j;
			u32 ent;
			double fr;

			if (dec < 0)
				dec = 6;
			if (dec > 9)
				dec = 9;
			if (menos)
				v = -v;
			{
				double r = 0.5;

				for (j = 0; j < dec; j++)
					r /= 10;
				v += r;
			}
			if (v > 4e9)
				v = 4e9;
			ent = (u32)v;
			fr = v - ent;
			/* los decimales, al revés en t (se da la vuelta abajo) */
			{
				char df[12];

				for (j = 0; j < dec; j++) {
					fr *= 10;
					df[j] = (char)('0' + (int)fr);
					fr -= (int)fr;
				}
				for (j = dec - 1; j >= 0; j--)
					t[n++] = df[j];
			}
			if (dec > 0)
				t[n++] = '.';
			do {
				t[n++] = (char)('0' + ent % 10);
				ent /= 10;
			} while (ent);
			if (menos)
				t[n++] = '-';
			break;
		}
		case 's': {
			const char *s = va_arg(va, const char *);
			int l;

			if (!s)
				s = "(nada)";
			l = (int)strlen(s);
			if (!izq)
				for (i = l; i < ancho; i++)
					k = pon(d, k, cabe, ' ');
			while (*s)
				k = pon(d, k, cabe, *s++);
			if (izq)
				for (i = l; i < ancho; i++)
					k = pon(d, k, cabe, ' ');
			continue;
		}
		case 'c':
			k = pon(d, k, cabe, (char)va_arg(va, int));
			continue;
		case '%':
			k = pon(d, k, cabe, '%');
			continue;
		default:
			continue;
		}
		if (!izq)
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ceros ? '0' : ' ');
		while (n > 0)
			k = pon(d, k, cabe, t[--n]);
		if (izq)
			for (i = n; i < ancho; i++)
				k = pon(d, k, cabe, ' ');
	}
	if (cabe > 0)
		d[k < cabe ? k : cabe - 1] = 0;
	return k;
}

int fmt(char *d, int cabe, const char *p, ...)
{
	va_list va;
	int k;

	va_start(va, p);
	k = vfmt(d, cabe, p, va);
	va_end(va);
	return k;
}

void dice(const char *p, ...)
{
	char t[600];
	va_list va;

	va_start(va, p);
	vfmt(t, sizeof t, p, va);
	va_end(va);
	so_dice(t);
}

static int mayus(int c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

int igual_sin_caja(const char *a, const char *b)
{
	while (*a && mayus((u8)*a) == mayus((u8)*b))
		a++, b++;
	return *a == 0 && *b == 0;
}

int empieza_sin_caja(const char *a, const char *b)
{
	while (*b && mayus((u8)*a) == mayus((u8)*b))
		a++, b++;
	return *b == 0;
}

void a_utf8(u32 c, u8 **p)
{
	u8 *s = *p;

	if (c < 0x80)
		*s++ = (u8)c;
	else if (c < 0x800) {
		*s++ = (u8)(0xC0 | c >> 6);
		*s++ = (u8)(0x80 | (c & 63));
	} else if (c < 0x10000) {
		*s++ = (u8)(0xE0 | c >> 12);
		*s++ = (u8)(0x80 | ((c >> 6) & 63));
		*s++ = (u8)(0x80 | (c & 63));
	} else {
		*s++ = (u8)(0xF0 | c >> 18);
		*s++ = (u8)(0x80 | ((c >> 12) & 63));
		*s++ = (u8)(0x80 | ((c >> 6) & 63));
		*s++ = (u8)(0x80 | (c & 63));
	}
	*p = s;
}

/* ---- la página 437 (la del sistema en Hispa-Pentium) ---- */
static const u16 alto437[128] = {0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5, 0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192, 0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510, 0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580, 0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229, 0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0};

u32 de_437(u8 c)
{
	return c < 128 ? c : alto437[c - 128];
}

/* lo de UTF-8 a la página 437 (lo que no está: «?»; las comillas y los
 * guiones tipográficos, a los de siempre); da el largo */
u32 utf8_a_437(const u8 *s, u32 n, u8 *d)
{
	u32 i = 0, k = 0;

	while (i < n) {
		u32 c = s[i], m = 1, j;

		if (c >= 0xF0 && i + 3 < n) {
			c = (c & 7) << 18 | (u32)(s[i + 1] & 63) << 12 | (u32)(s[i + 2] & 63) << 6 | (s[i + 3] & 63);
			m = 4;
		} else if (c >= 0xE0 && i + 2 < n) {
			c = (c & 15) << 12 | (u32)(s[i + 1] & 63) << 6 | (s[i + 2] & 63);
			m = 3;
		} else if (c >= 0xC0 && i + 1 < n) {
			c = (c & 31) << 6 | (s[i + 1] & 63);
			m = 2;
		}
		i += m;
		if (c < 128) {
			d[k++] = (u8)c;
			continue;
		}
		switch (c) {
		case 0x2018: case 0x2019: case 0x201A: case 0x2032: d[k++] = '\''; continue;
		case 0x201C: case 0x201D: case 0x201E: d[k++] = '"'; continue;
		case 0x2013: case 0x2014: case 0x2212: d[k++] = '-'; continue;
		case 0x2022: d[k++] = 0xF9; continue;
		case 0x2026: d[k++] = '.'; d[k++] = '.'; d[k++] = '.'; continue;
		case 0xFB01: d[k++] = 'f'; d[k++] = 'i'; continue;
		case 0xFB02: d[k++] = 'f'; d[k++] = 'l'; continue;
		case 0x20AC: d[k++] = 'E'; d[k++] = 'U'; d[k++] = 'R'; continue;
		case 0xC1: d[k++] = 'A'; continue;
		case 0xCD: d[k++] = 'I'; continue;
		case 0xD3: d[k++] = 'O'; continue;
		case 0xDA: d[k++] = 'U'; continue;
		case 0xC0: d[k++] = 'A'; continue;
		case 0xC8: d[k++] = 'E'; continue;
		case 0xD2: d[k++] = 'O'; continue;
		case 0xCF: d[k++] = 'I'; continue;
		}
		for (j = 0; j < 128; j++)
			if (alto437[j] == c)
				break;
		d[k++] = j < 128 ? (u8)(128 + j) : '?';
	}
	return k;
}
