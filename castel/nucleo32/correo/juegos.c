/* juegos.c - Correo: los juegos de caracteres (de 8 bits a UTF-8, con las
 * tablas de juegos_tablas.h), el UTF-8 por dentro, la página 437 de Hispa
 * (tools/hispa437.py) y las letras planas para buscar. Y lo de interno.h:
 * los textos que crecen y las copias.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdio.h>
#include "mime.h"
#include "interno.h"
#include "juegos_tablas.h"

/* ---- los textos que crecen ---- */
void tx_ini(Texto *t, u32 tope)
{
	t->d = NULL;
	t->n = t->cabe = 0;
	t->tope = tope ? tope : (64u << 20);
	t->malo = 0;
}

static int tx_sitio(Texto *t, u32 n)
{
	u32 hace, c;
	char *q;

	if (t->malo)
		return 0;
	if (n > t->tope || t->n > t->tope - n) {
		t->malo = 2;
		return 0;
	}
	hace = t->n + n + 1;
	if (hace <= t->cabe)
		return 1;
	c = t->cabe ? t->cabe : 64;
	while (c < hace)
		c = c > 0x7fffffffu ? hace : c * 2;
	q = so_repide(t->d, c);
	if (!q) {
		t->malo = 1;
		return 0;
	}
	t->d = q;
	t->cabe = c;
	return 1;
}

int tx_pon(Texto *t, const void *d, u32 n)
{
	if (!n)
		return 1;
	if (!tx_sitio(t, n))
		return 0;
	memcpy(t->d + t->n, d, n);
	t->n += n;
	return 1;
}

int tx_c(Texto *t, int c)
{
	if (t->n + 1 < t->cabe && !t->malo && t->n < t->tope) {
		t->d[t->n++] = (char)c;
		return 1;
	}
	if (!tx_sitio(t, 1))
		return 0;
	t->d[t->n++] = (char)c;
	return 1;
}

int tx_s(Texto *t, const char *s)
{
	return tx_pon(t, s, (u32)strlen(s));
}

int tx_letra(Texto *t, u32 letra)
{
	char b[4];
	int k = utf8_pon(letra, b);

	return tx_pon(t, b, (u32)k);
}

int tx_fmt(Texto *t, const char *f, ...)
{
	char b[512];
	va_list va;
	int k;

	va_start(va, f);
	k = vsnprintf(b, sizeof b, f, va);
	va_end(va);
	if (k < 0)
		return 0;
	if (k >= (int)sizeof b)
		k = sizeof b - 1;
	return tx_pon(t, b, (u32)k);
}

char *tx_fin(Texto *t, u32 *largo)
{
	if (t->malo == 1) {
		tx_suelta(t);
		if (largo)
			*largo = 0;
		return NULL;
	}
	if (!t->d) {
		t->d = so_pide(1);
		if (!t->d) {
			if (largo)
				*largo = 0;
			return NULL;
		}
		t->cabe = 1;
	}
	t->d[t->n] = 0;
	if (largo)
		*largo = t->n;
	return t->d;
}

void tx_suelta(Texto *t)
{
	so_suelta(t->d);
	t->d = NULL;
	t->n = t->cabe = 0;
}

/* ---- comparar y copiar ---- */
int cr_igual(const char *a, const char *b)
{
	while (*a && minuscula((u8)*a) == minuscula((u8)*b)) {
		a++;
		b++;
	}
	return minuscula((u8)*a) == minuscula((u8)*b);
}

int cr_igual_n(const char *a, u32 na, const char *b)
{
	u32 i;

	for (i = 0; i < na; i++)
		if (!b[i] || minuscula((u8)a[i]) != minuscula((u8)b[i]))
			return 0;
	return b[na] == 0;
}

int cr_empieza(const char *a, u32 na, const char *b)
{
	u32 i;

	for (i = 0; b[i]; i++)
		if (i >= na || minuscula((u8)a[i]) != minuscula((u8)b[i]))
			return 0;
	return 1;
}

char *cr_copia(const char *s, u32 n)
{
	char *d = so_pide(n + 1);

	if (!d)
		return NULL;
	if (n)
		memcpy(d, s, n);
	d[n] = 0;
	return d;
}

void cr_copia_en(char *d, int cabe, const char *s, u32 n)
{
	if (cabe <= 0)
		return;
	if (n >= (u32)cabe) {
		n = (u32)cabe - 1;
		/* sin partir una letra */
		while (n > 0 && ((u8)s[n] & 0xC0) == 0x80)
			n--;
	}
	if (n)
		memcpy(d, s, n);
	d[n] = 0;
}

i32 cr_busca(const u8 *a, u32 na, const char *b, u32 nb)
{
	u32 i = 0;

	if (!nb)
		return 0;
	while (i + nb <= na) {
		const u8 *p = memchr(a + i, (u8)b[0], na - nb + 1 - i);

		if (!p)
			return -1;
		i = (u32)(p - a);
		if (!memcmp(a + i, b, nb))
			return (i32)i;
		i++;
	}
	return -1;
}

i32 cr_busca_sin(const u8 *a, u32 na, const char *b, u32 nb)
{
	u32 i, j;
	int c0;

	if (!nb)
		return 0;
	c0 = minuscula((u8)b[0]);
	for (i = 0; i + nb <= na; i++) {
		if (minuscula(a[i]) != c0)
			continue;
		for (j = 1; j < nb; j++)
			if (minuscula(a[i + j]) != minuscula((u8)b[j]))
				break;
		if (j == nb)
			return (i32)i;
	}
	return -1;
}

u32 cr_numero(const char *s, u32 n, u32 *i, int *hay)
{
	u32 v = 0;
	int h = 0;

	while (*i < n && es_cifra((u8)s[*i])) {
		u32 c = (u32)(s[*i] - '0');

		v = v > 400000000u ? 0xFFFFFFFFu : (v * 10 + c < v * 10 ? 0xFFFFFFFFu : v * 10 + c);
		(*i)++;
		h = 1;
	}
	if (hay)
		*hay = h;
	return v;
}

/* ---- UTF-8 ---- */
u32 utf8_letra(const u8 *d, u32 n, u32 *i)
{
	u32 k = *i, c, l, m;
	int s, j;

	if (k >= n) {
		(*i)++;
		return 0xFFFD;
	}
	c = d[k];
	if (c < 0x80) {
		*i = k + 1;
		return c;
	}
	if (c >= 0xC2 && c <= 0xDF) {
		s = 1;
		l = c & 0x1F;
		m = 0x80;
	} else if (c >= 0xE0 && c <= 0xEF) {
		s = 2;
		l = c & 0x0F;
		m = 0x800;
	} else if (c >= 0xF0 && c <= 0xF4) {
		s = 3;
		l = c & 0x07;
		m = 0x10000;
	} else {
		*i = k + 1;
		return 0xFFFD;
	}
	if ((u32)s >= n - k) {	/* no caben */
		*i = k + 1;
		return 0xFFFD;
	}
	for (j = 1; j <= s; j++) {
		u32 b = d[k + j];

		if ((b & 0xC0) != 0x80) {
			*i = k + 1;
			return 0xFFFD;
		}
		l = (l << 6) | (b & 0x3F);
	}
	if (l < m || l > 0x10FFFF || (l >= 0xD800 && l <= 0xDFFF)) {
		*i = k + 1;
		return 0xFFFD;
	}
	*i = k + 1 + (u32)s;
	return l;
}

int utf8_valido(const u8 *d, u32 n)
{
	u32 i = 0;

	while (i < n) {
		if (d[i] < 0x80) {
			i++;
			continue;
		}
		if (d[i] == 0xEF && i + 2 < n && d[i + 1] == 0xBF && d[i + 2] == 0xBD) {
			i += 3;	/* un U+FFFD de verdad */
			continue;
		}
		if (utf8_letra(d, n, &i) == 0xFFFD)
			return 0;
	}
	return 1;
}

int utf8_pon(u32 c, char *d)
{
	if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
		c = 0xFFFD;
	if (c < 0x80) {
		d[0] = (char)c;
		return 1;
	}
	if (c < 0x800) {
		d[0] = (char)(0xC0 | (c >> 6));
		d[1] = (char)(0x80 | (c & 0x3F));
		return 2;
	}
	if (c < 0x10000) {
		d[0] = (char)(0xE0 | (c >> 12));
		d[1] = (char)(0x80 | ((c >> 6) & 0x3F));
		d[2] = (char)(0x80 | (c & 0x3F));
		return 3;
	}
	d[0] = (char)(0xF0 | (c >> 18));
	d[1] = (char)(0x80 | ((c >> 12) & 0x3F));
	d[2] = (char)(0x80 | ((c >> 6) & 0x3F));
	d[3] = (char)(0x80 | (c & 0x3F));
	return 4;
}

u32 utf8_cuenta(const char *t, u32 n)
{
	u32 i, k = 0;

	for (i = 0; i < n; i++)
		if (((u8)t[i] & 0xC0) != 0x80)
			k++;
	return k;
}

/* ---- los juegos ---- */
/* el nombre, normalizado: minúsculas, solo letras y cifras, sin «x-» delante
 * ni lo de detrás de «:» o «*» (el lenguaje de RFC 2231) */
static void normaliza(const char *j, char *d, int cabe)
{
	int k = 0;

	while (*j && (es_espacio((u8)*j) || *j == '"' || *j == '\''))
		j++;
	if ((j[0] == 'x' || j[0] == 'X') && j[1] == '-')
		j += 2;
	for (; *j && *j != ':' && *j != '*' && k < cabe - 1; j++)
		if (es_alnum((u8)*j))
			d[k++] = (char)minuscula((u8)*j);
	d[k] = 0;
}

/* -1 UTF-8, >= 0 una tabla, -3 us-ascii, -2 desconocido */
static int busca_juego(const char *juego)
{
	char b[48];
	int a = 0, z = JT_N_ALIAS - 1;

	if (!juego)
		return -2;
	normaliza(juego, b, sizeof b);
	if (!b[0])
		return -2;
	while (a <= z) {
		int m = (a + z) / 2, c = strcmp(b, jt_alias[m].alias);

		if (!c)
			return jt_alias[m].t;
		if (c < 0)
			z = m - 1;
		else
			a = m + 1;
	}
	return -2;
}

int jc_conocido(const char *juego)
{
	return busca_juego(juego) != -2;
}

static char *de_tabla(int t, const u8 *d, u32 n, u32 *largo)
{
	Texto x;
	u32 i;

	tx_ini(&x, 0);
	if (n > 0x15000000u)
		n = 0x15000000u;
	if (!tx_sitio(&x, n + n / 2 + 8))
		return tx_fin(&x, largo);
	for (i = 0; i < n; i++) {
		u8 c = d[i];

		if (c < 0x80) {
			if (x.n + 1 >= x.cabe && !tx_sitio(&x, 4))
				break;
			x.d[x.n++] = (char)c;
		} else
			tx_letra(&x, jt_tablas[t][c - 0x80]);
	}
	return tx_fin(&x, largo);
}

static char *de_utf8(const u8 *d, u32 n, u32 *largo)
{
	Texto x;
	u32 i = 0;

	tx_ini(&x, 0);
	if (!tx_sitio(&x, n + 8))
		return tx_fin(&x, largo);
	while (i < n) {
		u32 a = i;

		while (i < n && d[i] < 0x80)
			i++;
		tx_pon(&x, d + a, i - a);
		if (i < n) {
			u32 b = i, c = utf8_letra(d, n, &i);

			if (c == 0xFFFD)
				tx_letra(&x, 0xFFFD);
			else
				tx_pon(&x, d + b, i - b);
		}
	}
	return tx_fin(&x, largo);
}

char *jc_a_utf8(const char *juego, const u8 *d, u32 n, u32 *largo)
{
	int t = busca_juego(juego);

	if (t == -1)
		return de_utf8(d, n, largo);
	if (t >= 0) {
		return de_tabla(t, d, n, largo);
	}
	/* us-ascii con 8 bits, o desconocido: UTF-8 si lo es; si no, 1252 */
	if (utf8_valido(d, n))
		return de_utf8(d, n, largo);
	return de_tabla(JT_1252, d, n, largo);
}

/* ---- la 437 de Hispa ---- */
u32 letra_de_437(u8 c)
{
	return c < 0x80 ? c : jt_de437[c - 0x80];
}

static const char *a437(u32 c)
{
	int a = 0, z = JT_N_A437 - 1;

	if (c > 0xFFFF)
		return NULL;
	while (a <= z) {
		int m = (a + z) / 2;

		if (jt_a437[m].letra == c)
			return jt_a437[m].s;
		if (jt_a437[m].letra < c)
			a = m + 1;
		else
			z = m - 1;
	}
	return NULL;
}

u32 utf8_a_437(const char *t, u32 n, char *d, u32 cabe)
{
	u32 i = 0, k = 0;

	if (!cabe)
		return 0;
	while (i < n && k + 1 < cabe) {
		u32 c = utf8_letra((const u8 *)t, n, &i);
		const char *s;

		if (c < 0x80) {
			d[k++] = (char)c;
			continue;
		}
		if (c >= 0x300 && c <= 0x36F)
			continue;	/* las tildes sueltas (lo descompuesto) */
		s = a437(c);
		if (!s)
			s = "?";
		if (k + strlen(s) + 1 > cabe)
			break;
		while (*s)
			d[k++] = *s++;
	}
	d[k] = 0;
	return k;
}

/* ---- las letras planas ---- */
static const char *plano(u32 c)
{
	int a = 0, z = JT_N_PLANO - 1;

	if (c > 0xFFFF)
		return NULL;
	while (a <= z) {
		int m = (a + z) / 2;

		if (jt_plano[m].letra == c)
			return jt_plano[m].s;
		if (jt_plano[m].letra < c)
			a = m + 1;
		else
			z = m - 1;
	}
	return NULL;
}

char *utf8_plano(const char *t, u32 n, u32 *largo)
{
	Texto x;
	u32 i = 0;

	tx_ini(&x, 0);
	if (!tx_sitio(&x, n + 8))
		return tx_fin(&x, largo);
	while (i < n) {
		u32 c = utf8_letra((const u8 *)t, n, &i);
		const char *s;

		if (c < 0x80) {
			if (x.n + 1 >= x.cabe && !tx_sitio(&x, 4))
				break;
			x.d[x.n++] = (char)minuscula((int)c);
			continue;
		}
		if (c >= 0x300 && c <= 0x36F)
			continue;
		s = plano(c);
		if (s)
			tx_s(&x, s);
		else
			tx_letra(&x, c);
	}
	return tx_fin(&x, largo);
}
