/* ttf.c - la oficina de 32 bits: las TrueType sin fiarse de ellas
 * (oficina/comun/ttf.h). La comprobación viene de Hispa-PDF
 * (castel/nucleo32/pdf/fuente.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ttf.h"
#include "texto.h"

static u32 rd16(const u8 *d, u32 n, u32 o) { return o + 2 <= n && o + 2 > o ? (u32)d[o] << 8 | d[o + 1] : 0; }
static u32 rd32(const u8 *d, u32 n, u32 o) { return o + 4 <= n && o + 4 > o ? (u32)d[o] << 24 | (u32)d[o + 1] << 16 | (u32)d[o + 2] << 8 | d[o + 3] : 0; }

/* una tabla: su sitio y lo que mide (0 si no está o se sale) */
static u32 tabla(const u8 *d, u32 n, const char *nombre, u32 *largo_)
{
	u32 nt = rd16(d, n, 4), i;

	*largo_ = 0;
	if (12 + 16 * nt > n)
		return 0;
	for (i = 0; i < nt; i++) {
		u32 r = 12 + 16 * i, off = rd32(d, n, r + 8), l = rd32(d, n, r + 12);

		if (memcmp(d + r, nombre, 4) == 0) {
			if (off > n || l > n - off)
				return 0;
			*largo_ = l;
			return off;
		}
	}
	return 0;
}

/* el glifo del carácter c en la subtabla cmap de «off» (formatos 0, 4, 6 y 12), mirando los límites */
static u32 cmap_busca(const u8 *d, u32 n, u32 off, u32 c)
{
	u32 f;

	if (!off || off >= n)
		return 0;
	f = rd16(d, n, off);
	if (f == 0)
		return c < 256 && off + 6 + c < n ? d[off + 6 + c] : 0;
	if (f == 6) {
		u32 primero = rd16(d, n, off + 6), cuantos = rd16(d, n, off + 8);

		if (c < primero || c >= primero + cuantos)
			return 0;
		return rd16(d, n, off + 10 + 2 * (c - primero));
	}
	if (f == 4) {
		u32 seg2 = rd16(d, n, off + 6), segs = seg2 / 2, i;
		u32 fines = off + 14, inis = fines + seg2 + 2, deltas = inis + seg2, rangos = deltas + seg2;

		if (c > 0xFFFF || rangos + seg2 > n)
			return 0;
		for (i = 0; i < segs; i++) {
			u32 fin_ = rd16(d, n, fines + 2 * i), ini = rd16(d, n, inis + 2 * i);

			if (c > fin_)
				continue;
			if (c < ini)
				return 0;
			{
				u32 delta = rd16(d, n, deltas + 2 * i), ro = rd16(d, n, rangos + 2 * i), g;

				if (!ro)
					return (c + delta) & 0xFFFF;
				g = rd16(d, n, rangos + 2 * i + ro + 2 * (c - ini));
				return g ? (g + delta) & 0xFFFF : 0;
			}
		}
		return 0;
	}
	if (f == 12) {
		u32 grupos = rd32(d, n, off + 12), i;

		if (grupos > (n - off) / 12)
			return 0;
		for (i = 0; i < grupos; i++) {
			u32 a = rd32(d, n, off + 16 + 12 * i), b = rd32(d, n, off + 20 + 12 * i), g = rd32(d, n, off + 24 + 12 * i);

			if (c >= a && c <= b)
				return g + (c - a);
		}
	}
	return 0;
}

/* un nombre de la tabla «name» (de Windows en UTF-16, o de Mac en latín) */
static void nombre_de(const u8 *d, u32 n, u32 tb, u32 ln, u32 id, char *sal, u32 cabe)
{
	u32 cuantos = rd16(d, n, tb + 2), base = tb + rd16(d, n, tb + 4), i, mejor = 0, mac = 0;

	sal[0] = 0;
	if (6 + 12 * cuantos > ln)
		return;
	for (i = 0; i < cuantos; i++) {
		u32 r = tb + 6 + 12 * i, pl = rd16(d, n, r), en = rd16(d, n, r + 2);

		if (rd16(d, n, r + 6) != id)
			continue;
		if (pl == 3 && (en == 1 || en == 10)) {
			mejor = r;
			if (rd16(d, n, r + 4) == 0x409)
				break;
		} else if (pl == 1 && !mac)
			mac = r;
	}
	if (mejor) {
		u32 len = rd16(d, n, mejor + 8), off = base + rd16(d, n, mejor + 10), k = 0, j;

		for (j = 0; j + 1 < len && off + j + 1 < n && k + 4 < cabe; j += 2) {
			u32 c = rd16(d, n, off + j);

			k += (u32)utf8_pon(c >= 0xD800 && c < 0xE000 ? '?' : c, sal + k);
		}
		sal[k] = 0;
	} else if (mac) {
		u32 len = rd16(d, n, mac + 8), off = base + rd16(d, n, mac + 10), k = 0, j;

		for (j = 0; j < len && off + j < n && k + 2 < cabe; j++)
			sal[k++] = d[off + j] < 0x80 ? (char)d[off + j] : '?';
		sal[k] = 0;
	}
}

int ttf_abre(ttf *t, const u8 *d, u32 n)
{
	u32 l, head, hhea, hmtx, maxp, loca, glyf, cmap, lh, lx, ll, lg, lm, lc, nl, ng, i, nt, os2, lo, nm, lnm;

	memset(t, 0, sizeof *t);
	if (n < 12)
		return 0;
	/* (una colección .TTC: la primera) */
	if (rd32(d, n, 0) == 0x74746366u) {
		u32 o = rd32(d, n, 12);

		if (o >= n)
			return 0;
		/* las tablas de una TTC van desde el principio del archivo: no se
		 * puede recortar; no se admiten */
		return 0;
	}
	if (rd32(d, n, 0) != 0x00010000u && rd32(d, n, 0) != 0x74727565u)
		return 0;	/* (las OpenType con CFF, no) */
	nt = rd16(d, n, 4);
	if (nt == 0 || 12 + 16 * nt > n)
		return 0;
	for (i = 0; i < nt; i++) {
		u32 off = rd32(d, n, 12 + 16 * i + 8), largo_ = rd32(d, n, 12 + 16 * i + 12);

		if (off > n || largo_ > n - off)
			return 0;
	}
	head = tabla(d, n, "head", &l);
	if (!head || l < 54)
		return 0;
	hhea = tabla(d, n, "hhea", &lh);
	hmtx = tabla(d, n, "hmtx", &lx);
	maxp = tabla(d, n, "maxp", &lm);
	loca = tabla(d, n, "loca", &ll);
	glyf = tabla(d, n, "glyf", &lg);
	cmap = tabla(d, n, "cmap", &lc);
	if (!hhea || lh < 36 || !hmtx || !maxp || lm < 6 || !loca || !glyf || !cmap || lc < 4)
		return 0;
	ng = rd16(d, n, maxp + 4);
	nl = rd16(d, n, hhea + 34);
	if (ng == 0 || nl == 0 || nl > ng)
		return 0;
	if ((u64)4 * nl + 2 * (u64)(ng - nl) > lx)
		return 0;
	t->loca_larga = rd16(d, n, head + 50);
	if (t->loca_larga > 1)
		return 0;
	if ((u64)(ng + 1) * (t->loca_larga ? 4 : 2) > ll)
		return 0;
	if (4 + 8 * rd16(d, n, cmap + 2) > lc)
		return 0;
	{
		u32 antes = 0;

		for (i = 0; i <= ng; i++) {
			u32 v = t->loca_larga ? rd32(d, n, loca + 4 * i) : 2 * rd16(d, n, loca + 2 * i);

			if (v < antes || v > lg)
				return 0;
			antes = v;
		}
	}
	/* la subtabla de Unicode */
	{
		u32 k = rd16(d, n, cmap + 2), mejor = 0, prio = 0;

		for (i = 0; i < k; i++) {
			u32 r = cmap + 4 + 8 * i, pl = rd16(d, n, r), en = rd16(d, n, r + 2), off = rd32(d, n, r + 4), p = 0;

			if (off >= lc)
				continue;
			if (pl == 3 && en == 10)
				p = 4;
			else if (pl == 3 && en == 1)
				p = 3;
			else if (pl == 0)
				p = 2;
			else if (pl == 3 && en == 0)
				p = 1;
			if (p > prio) {
				prio = p;
				mejor = cmap + off;
			}
		}
		if (!mejor)
			return 0;
		t->cmap_uni = mejor;
	}
	t->d = d;
	t->n = n;
	t->n_glifos = ng;
	t->loca = loca;
	t->glyf = glyf;
	t->glyf_n = lg;
	t->em = rd16(d, n, head + 18);
	if (t->em < 16 || t->em > 16384)
		return 0;
	t->x0 = (int16_t)rd16(d, n, head + 36);
	t->y0 = (int16_t)rd16(d, n, head + 38);
	t->x1 = (int16_t)rd16(d, n, head + 40);
	t->y1 = (int16_t)rd16(d, n, head + 42);
	t->asc = (int16_t)rd16(d, n, hhea + 4);
	t->desc = (int16_t)rd16(d, n, hhea + 6);
	t->salto = (int16_t)rd16(d, n, hhea + 8);
	{
		u32 ms = rd16(d, n, head + 44);

		t->negrita = (ms & 1) != 0;
		t->cursiva = (ms & 2) != 0;
	}
	os2 = tabla(d, n, "OS/2", &lo);
	t->cap = t->asc * 7 / 10;
	if (os2 && lo >= 78) {
		u32 sel = rd16(d, n, os2 + 62);
		int ta = (int16_t)rd16(d, n, os2 + 68), td = (int16_t)rd16(d, n, os2 + 70), tg = (int16_t)rd16(d, n, os2 + 72);

		t->negrita |= (sel & 0x20) != 0;
		t->cursiva |= (sel & 0x01) != 0;
		if (rd16(d, n, os2 + 4) >= 600)
			t->negrita = 1;
		/* (USE_TYPO_METRICS) */
		if ((sel & 0x80) && ta > 0) {
			t->asc = ta;
			t->desc = td;
			t->salto = tg;
		}
		if (lo >= 90 && rd16(d, n, os2) >= 2 && (int16_t)rd16(d, n, os2 + 88) > 0)
			t->cap = (int16_t)rd16(d, n, os2 + 88);
	}
	if (t->asc <= 0) {
		t->asc = (int)t->em * 8 / 10;
		t->desc = -(int)t->em * 2 / 10;
	}
	nm = tabla(d, n, "name", &lnm);
	if (nm && lnm >= 6) {
		nombre_de(d, n, nm, lnm, 16, t->familia, sizeof t->familia);
		if (!t->familia[0])
			nombre_de(d, n, nm, lnm, 1, t->familia, sizeof t->familia);
		nombre_de(d, n, nm, lnm, 17, t->estilo, sizeof t->estilo);
		if (!t->estilo[0])
			nombre_de(d, n, nm, lnm, 2, t->estilo, sizeof t->estilo);
		nombre_de(d, n, nm, lnm, 6, t->postscript, sizeof t->postscript);
	}
	if (!t->familia[0])
		copia(t->familia, "Sin nombre", sizeof t->familia);
	if (!t->postscript[0]) {
		u32 j, k = 0;

		for (j = 0; t->familia[j] && k < 60; j++)
			if ((u8)t->familia[j] > ' ' && (u8)t->familia[j] < 0x7F && t->familia[j] != '(' && t->familia[j] != ')' && t->familia[j] != '/')
				t->postscript[k++] = t->familia[j];
		t->postscript[k] = 0;
	}
	/* (el nombre PostScript solo con lo que el PDF deja en un nombre) */
	for (i = 0; t->postscript[i]; i++) {
		u8 c = (u8)t->postscript[i];

		if (c <= ' ' || c >= 0x7F || strchr("()<>[]{}/%#", c))
			t->postscript[i] = '-';
	}
	t->ok = so_pide(ng);
	if (!t->ok)
		return 0;
	memset(t->ok, 0, ng);
	return 1;
}

void ttf_cierra(ttf *t)
{
	so_suelta(t->ok);
	t->ok = NULL;
}

u32 ttf_glifo(const ttf *t, u32 c)
{
	u32 g = cmap_busca(t->d, t->n, t->cmap_uni, c);

	return g < t->n_glifos ? g : 0;
}

static int glifo_bien(ttf *t, u32 g, int hondo)
{
	const u8 *d = t->d;
	u32 n = t->n, a, b, p, fin;
	int nc;

	if (g >= t->n_glifos || hondo > 4)
		return 0;
	if (t->ok[g])
		return t->ok[g] == 1;
	a = t->loca_larga ? rd32(d, n, t->loca + 4 * g) : 2 * rd16(d, n, t->loca + 2 * g);
	b = t->loca_larga ? rd32(d, n, t->loca + 4 * g + 4) : 2 * rd16(d, n, t->loca + 2 * g + 2);
	if (a == b)
		return 1;	/* (vacío) */
	if (b < a || b - a < 10)
		return 0;
	p = t->glyf + a;
	fin = t->glyf + b;
	nc = (int)(int16_t)rd16(d, n, p);
	if (nc >= 0) {
		u32 q = p + 10, puntos, ins, i, xs = 0, ys = 0, ultimo = 0;

		if (q + 2 * (u32)nc + 2 > fin)
			return 0;
		for (i = 0; i < (u32)nc; i++) {
			u32 e = rd16(d, n, q + 2 * i);

			if (i && e < ultimo)
				return 0;
			ultimo = e;
		}
		puntos = nc ? ultimo + 1 : 0;
		ins = rd16(d, n, q + 2 * (u32)nc);
		q += 2 * (u32)nc + 2 + ins;
		for (i = 0; i < puntos;) {
			u32 fl, rep = 1;

			if (q >= fin)
				return 0;
			fl = d[q++];
			if (fl & 8) {
				if (q >= fin)
					return 0;
				rep += d[q++];
			}
			if (i + rep > puntos)
				rep = puntos - i;
			xs += rep * ((fl & 2) ? 1 : (fl & 16) ? 0 : 2);
			ys += rep * ((fl & 4) ? 1 : (fl & 32) ? 0 : 2);
			i += rep;
		}
		if ((u64)q + xs + ys > fin)
			return 0;
		return 1;
	} else {
		u32 q = p + 10, fl;
		int k = 0;

		do {
			u32 sub;

			if (q + 4 > fin || ++k > 64)
				return 0;
			fl = rd16(d, n, q);
			sub = rd16(d, n, q + 2);
			q += 4 + ((fl & 1) ? 4 : 2);
			if (fl & 8)
				q += 2;
			else if (fl & 0x40)
				q += 4;
			else if (fl & 0x80)
				q += 8;
			if (q > fin)
				return 0;
			if (sub == g || !glifo_bien(t, sub, hondo + 1))
				return 0;
		} while (fl & 0x20);
		return 1;
	}
}

int ttf_glifo_vale(ttf *t, u32 g)
{
	int r;

	if (g >= t->n_glifos)
		return 0;
	if (t->ok[g])
		return t->ok[g] == 1;
	r = glifo_bien(t, g, 0);
	t->ok[g] = (u8)(r ? 1 : 2);
	return r;
}

int ttf_avance(const ttf *t, u32 g)
{
	u32 l, hhea = tabla(t->d, t->n, "hhea", &l), hmtx = tabla(t->d, t->n, "hmtx", &l), nl = rd16(t->d, t->n, hhea + 34);

	if (g >= nl)
		g = nl - 1;
	return (int)rd16(t->d, t->n, hmtx + 4 * g);
}

/* ---- el subconjunto ---- */
static void marca_compuestos(const ttf *t, u8 *usa, u32 g, int hondo)
{
	const u8 *d = t->d;
	u32 n = t->n, a, b, q, fl;
	int k = 0;

	if (g >= t->n_glifos || hondo > 4 || t->ok[g] == 2)
		return;
	usa[g >> 3] |= (u8)(1 << (g & 7));
	a = t->loca_larga ? rd32(d, n, t->loca + 4 * g) : 2 * rd16(d, n, t->loca + 2 * g);
	b = t->loca_larga ? rd32(d, n, t->loca + 4 * g + 4) : 2 * rd16(d, n, t->loca + 2 * g + 2);
	if (b - a < 10 || (int16_t)rd16(d, n, t->glyf + a) >= 0)
		return;
	q = t->glyf + a + 10;
	do {
		u32 sub;

		if (++k > 64)
			return;
		fl = rd16(d, n, q);
		sub = rd16(d, n, q + 2);
		q += 4 + ((fl & 1) ? 4 : 2);
		if (fl & 8)
			q += 2;
		else if (fl & 0x40)
			q += 4;
		else if (fl & 0x80)
			q += 8;
		marca_compuestos(t, usa, sub, hondo + 1);
	} while (fl & 0x20);
}

static u32 suma(const u8 *d, u32 n)
{
	u32 s = 0, i;

	for (i = 0; i + 3 < n; i += 4)
		s += (u32)d[i] << 24 | (u32)d[i + 1] << 16 | (u32)d[i + 2] << 8 | d[i + 3];
	if (n & 3) {
		u8 r[4] = { 0, 0, 0, 0 };

		memcpy(r, d + (n & ~3u), n & 3);
		s += (u32)r[0] << 24 | (u32)r[1] << 16 | (u32)r[2] << 8 | r[3];
	}
	return s;
}

static void be16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static void be32(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

u8 *ttf_subconjunto(const ttf *t, const u8 *usados, u32 *largo_)
{
	static const char *const guarda[] = { "OS/2", "cmap", "cvt ", "fpgm", "glyf", "head", "hhea", "hmtx", "loca", "maxp", "name", "post", "prep" };
	const u8 *d = t->d;
	u32 n = t->n, ng = t->n_glifos, i, k, nt = 0, lg, ll, tam, pos;
	u8 *usa, *glyf, *loca, *sal;
	struct { const char *nombre; const u8 *datos; u32 n; } tb[13];

	usa = so_pide(ng / 8 + 1);
	if (!usa)
		return NULL;
	memset(usa, 0, ng / 8 + 1);
	for (i = 0; i < ng; i++)
		if (i == 0 || (usados[i >> 3] >> (i & 7) & 1))
			marca_compuestos(t, usa, i, 0);
	usa[0] |= 1;
	/* glyf y loca nuevos (larga: 4 octetos) */
	lg = 0;
	for (i = 0; i < ng; i++)
		if (usa[i >> 3] >> (i & 7) & 1) {
			u32 a = t->loca_larga ? rd32(d, n, t->loca + 4 * i) : 2 * rd16(d, n, t->loca + 2 * i);
			u32 b = t->loca_larga ? rd32(d, n, t->loca + 4 * i + 4) : 2 * rd16(d, n, t->loca + 2 * i + 2);

			lg += ((b - a) + 3) & ~3u;
		}
	glyf = so_pide(lg + 4);
	ll = (ng + 1) * 4;
	loca = so_pide(ll);
	if (!glyf || !loca) {
		so_suelta(usa);
		so_suelta(glyf);
		so_suelta(loca);
		return NULL;
	}
	pos = 0;
	for (i = 0; i < ng; i++) {
		be32(loca + 4 * i, pos);
		if (usa[i >> 3] >> (i & 7) & 1) {
			u32 a = t->loca_larga ? rd32(d, n, t->loca + 4 * i) : 2 * rd16(d, n, t->loca + 2 * i);
			u32 b = t->loca_larga ? rd32(d, n, t->loca + 4 * i + 4) : 2 * rd16(d, n, t->loca + 2 * i + 2);
			u32 m = b - a;

			memcpy(glyf + pos, d + t->glyf + a, m);
			while (m & 3)
				glyf[pos + m++] = 0;
			pos += m;
		}
	}
	be32(loca + 4 * ng, pos);
	so_suelta(usa);
	/* las tablas que se quedan */
	for (k = 0; k < 13; k++) {
		u32 l, o;

		if (iguales(guarda[k], "glyf")) {
			tb[nt].nombre = guarda[k];
			tb[nt].datos = glyf;
			tb[nt++].n = pos;
			continue;
		}
		if (iguales(guarda[k], "loca")) {
			tb[nt].nombre = guarda[k];
			tb[nt].datos = loca;
			tb[nt++].n = ll;
			continue;
		}
		o = tabla(d, n, guarda[k], &l);
		if (!o)
			continue;
		if (iguales(guarda[k], "post") && l > 32)
			l = 32;		/* (formato 3: sin los nombres de las letras) */
		tb[nt].nombre = guarda[k];
		tb[nt].datos = d + o;
		tb[nt++].n = l;
	}
	tam = 12 + 16 * nt;
	for (k = 0; k < nt; k++)
		tam += (tb[k].n + 3) & ~3u;
	sal = so_pide(tam);
	if (!sal) {
		so_suelta(glyf);
		so_suelta(loca);
		return NULL;
	}
	memset(sal, 0, tam);
	be32(sal, 0x00010000u);
	be16(sal + 4, nt);
	{
		u32 e = 0, p2 = 1;

		while (p2 * 2 <= nt) {
			p2 *= 2;
			e++;
		}
		be16(sal + 6, p2 * 16);
		be16(sal + 8, e);
		be16(sal + 10, nt * 16 - p2 * 16);
	}
	pos = 12 + 16 * nt;
	for (k = 0; k < nt; k++) {
		u8 *r = sal + 12 + 16 * k;

		memcpy(sal + pos, tb[k].datos, tb[k].n);
		if (iguales(tb[k].nombre, "head")) {
			be32(sal + pos + 8, 0);		/* checkSumAdjustment, luego */
			be16(sal + pos + 50, 1);	/* loca larga */
		}
		if (iguales(tb[k].nombre, "post"))
			be32(sal + pos, 0x00030000u);
		memcpy(r, tb[k].nombre, 4);
		be32(r + 4, suma(sal + pos, tb[k].n));
		be32(r + 8, pos);
		be32(r + 12, tb[k].n);
		pos += (tb[k].n + 3) & ~3u;
	}
	for (k = 0; k < nt; k++)
		if (iguales(tb[k].nombre, "head")) {
			u32 o = rd32(sal, tam, 12 + 16 * k + 8);

			be32(sal + o + 8, 0xB1B0AFBAu - suma(sal, tam));
		}
	so_suelta(glyf);
	so_suelta(loca);
	*largo_ = tam;
	return sal;
}
