/* json.c - un lector de JSON (RFC 8259) pequeño, sin memoria: cada valor es
 * un trozo del texto, y se busca dentro de él cuando hace falta (ts.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "ts.h"

static u32 blancos(const char *t, u32 n, u32 i)
{
	while (i < n && (t[i] == ' ' || t[i] == '\t' || t[i] == '\r' || t[i] == '\n'))
		i++;
	return i;
}

/* el final del valor que empieza en i (o n+1 si está mal) */
static u32 salta(const char *t, u32 n, u32 i)
{
	int prof = 0;

	i = blancos(t, n, i);
	if (i >= n)
		return n + 1;
	if (t[i] == '"') {
		for (i++; i < n; i++) {
			if (t[i] == '\\')
				i++;
			else if (t[i] == '"')
				return i + 1;
		}
		return n + 1;
	}
	if (t[i] == '{' || t[i] == '[') {
		for (; i < n; i++) {
			char c = t[i];
			if (c == '"') {
				for (i++; i < n && t[i] != '"'; i++)
					if (t[i] == '\\')
						i++;
				continue;
			}
			if (c == '{' || c == '[')
				prof++;
			else if (c == '}' || c == ']') {
				if (--prof == 0)
					return i + 1;
			}
		}
		return n + 1;
	}
	while (i < n && t[i] != ',' && t[i] != '}' && t[i] != ']' && t[i] != ' ' && t[i] != '\r' && t[i] != '\n' && t[i] != '\t')
		i++;
	return i;
}

int j_valor(const char *t, u32 n, struct jv *v)
{
	u32 i = blancos(t, n, 0), f = salta(t, n, i);

	if (f > n)
		return 0;
	v->p = t + i;
	v->n = f - i;
	return 1;
}

/* recorre un objeto o un arreglo: el elemento k (en un objeto, nombre y valor) */
static int recorre(struct jv o, int k, struct jv *nombre, struct jv *v)
{
	const char *t = o.p;
	u32 n = o.n, i = 1, f;
	int obj, cual = 0;

	if (n < 2 || (t[0] != '{' && t[0] != '['))
		return 0;
	obj = t[0] == '{';
	for (;;) {
		i = blancos(t, n, i);
		if (i >= n || t[i] == '}' || t[i] == ']')
			return 0;
		if (obj) {
			f = salta(t, n, i);
			if (f > n)
				return 0;
			if (nombre)
				nombre->p = t + i, nombre->n = f - i;
			i = blancos(t, n, f);
			if (i >= n || t[i] != ':')
				return 0;
			i = blancos(t, n, i + 1);
		}
		f = salta(t, n, i);
		if (f > n)
			return 0;
		if (cual == k) {
			v->p = t + i;
			v->n = f - i;
			return 1;
		}
		cual++;
		i = blancos(t, n, f);
		if (i < n && t[i] == ',')
			i++;
	}
}

int j_elemento(struct jv a, int i, struct jv *v) { return a.n && a.p[0] == '[' && recorre(a, i, 0, v); }
int j_par(struct jv o, int i, struct jv *nombre, struct jv *v) { return o.n && o.p[0] == '{' && recorre(o, i, nombre, v); }

int j_cuantos(struct jv a)
{
	struct jv n, v;
	int i = 0;

	while (recorre(a, i, &n, &v))
		i++;
	return i;
}

int j_es(struct jv v, const char *t)
{
	u32 k = strlen(t);

	return v.n == k + 2 && v.p[0] == '"' && !memcmp(v.p + 1, t, k);
}

int j_campo(struct jv o, const char *nombre, struct jv *v)
{
	struct jv nm;
	int i;

	if (!o.n || o.p[0] != '{')
		return 0;
	for (i = 0; recorre(o, i, &nm, v); i++)
		if (j_es(nm, nombre))
			return 1;
	return 0;
}

int j_ruta(struct jv o, const char *ruta, struct jv *v)
{
	char trozo[64];
	struct jv x = o;

	while (*ruta) {
		int k = 0;
		while (*ruta && *ruta != '.' && k < 63)
			trozo[k++] = *ruta++;
		trozo[k] = 0;
		if (*ruta == '.')
			ruta++;
		if (!j_campo(x, trozo, &x))
			return 0;
	}
	*v = x;
	return 1;
}

static int hex4(const char *p)
{
	int v = 0, i;

	for (i = 0; i < 4; i++) {
		char c = p[i];
		v <<= 4;
		if (c >= '0' && c <= '9')
			v |= c - '0';
		else if (c >= 'a' && c <= 'f')
			v |= c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			v |= c - 'A' + 10;
		else
			return -1;
	}
	return v;
}

int j_texto(struct jv v, char *d, int cabe)
{
	u32 i;
	int k = 0;

	if (v.n < 2 || v.p[0] != '"') {
		if (cabe)
			d[0] = 0;
		return 0;
	}
	for (i = 1; i + 1 < v.n && k < cabe - 1; i++) {
		char c = v.p[i];
		if (c == '\\' && i + 2 < v.n) {
			c = v.p[++i];
			if (c == 'n')
				c = '\n';
			else if (c == 't')
				c = '\t';
			else if (c == 'r')
				c = '\r';
			else if (c == 'b' || c == 'f')
				c = ' ';
			else if (c == 'u' && i + 4 < v.n) {
				int u = hex4(v.p + i + 1);
				i += 4;
				/* (en UTF-8) */
				if (u < 0)
					c = '?';
				else if (u < 0x80)
					c = u;
				else if (u < 0x800 && k + 2 < cabe) {
					d[k++] = 0xC0 | u >> 6;
					c = 0x80 | (u & 63);
				} else if (k + 3 < cabe) {
					d[k++] = 0xE0 | u >> 12;
					d[k++] = 0x80 | (u >> 6 & 63);
					c = 0x80 | (u & 63);
				} else
					c = '?';
			}
		}
		d[k++] = c;
	}
	d[k] = 0;
	return k;
}

long long j_numero(struct jv v)
{
	long long r = 0;
	u32 i = 0;
	int menos = 0;

	if (v.n && v.p[0] == '"')
		i = 1;		/* (los ID van a veces entre comillas) */
	if (i < v.n && v.p[i] == '-')
		menos = 1, i++;
	for (; i < v.n && v.p[i] >= '0' && v.p[i] <= '9'; i++)
		r = r * 10 + v.p[i] - '0';
	return menos ? -r : r;
}

int j_cierto(struct jv v) { return v.n == 4 && !memcmp(v.p, "true", 4); }
int j_nulo(struct jv v) { return v.n == 0 || (v.n == 4 && !memcmp(v.p, "null", 4)); }

int j_cadena(char *d, int cabe, const char *t)
{
	int k = 0;

	if (cabe < 3)
		return 0;
	d[k++] = '"';
	for (; *t && k < cabe - 3; t++) {
		u8 c = *t;
		if (c == '"' || c == '\\') {
			d[k++] = '\\';
			d[k++] = c;
		} else if (c < 32) {
			if (k + 6 >= cabe - 2)
				break;
			k += formatear(d + k, cabe - k, "\\u%04x", c);
		} else
			d[k++] = c;
	}
	d[k++] = '"';
	d[k] = 0;
	return k;
}
