/* operaciones.c - Pintura 2: lo del menú Imagen y Colores (operaciones.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "operaciones.h"

#define PI 3.14159265358979323846

double op_coseno(double x)
{
	return cos(x);
}

double op_seno(double x)
{
	return cos(x - PI / 2);
}

/* ---- cambiar el tamaño: dos pasadas (a lo ancho y a lo alto), cada punto
 * nuevo con sus pesos de los viejos; con el color multiplicado por su alfa
 * para que lo transparente no tiña los bordes ---- */
typedef struct {
	int de;			/* el primer punto viejo */
	int n;			/* cuántos */
	int peso0;		/* dónde empiezan sus pesos (en «pesos») */
} toma;

static int pesos_de(int viejo, int nuevo, toma *t, float **pesos)
{
	double s = (double)viejo / nuevo;
	int i, k, total = 0, maxn = s > 1 ? (int)s + 3 : 2;
	float *w = so_pide((size_t)nuevo * (size_t)maxn * sizeof(float));

	if (!w)
		return 0;
	for (i = 0; i < nuevo; i++) {
		t[i].peso0 = total;
		if (s > 1) {
			/* reducir: la media del área [i*s, (i+1)*s) */
			double a = i * s, b = (i + 1) * s, suma = 0;
			int k0 = (int)a, k1 = (int)ceil(b);

			if (k1 > viejo)
				k1 = viejo;
			t[i].de = k0;
			t[i].n = 0;
			for (k = k0; k < k1 && t[i].n < maxn; k++) {
				double lo = k > a ? k : a, hi = k + 1 < b ? k + 1 : b;

				w[total + t[i].n] = (float)(hi - lo);
				suma += hi - lo;
				t[i].n++;
			}
			for (k = 0; k < t[i].n; k++)
				w[total + k] = (float)(w[total + k] / suma);
		} else {
			/* ampliar: el bilineal */
			double x = (i + 0.5) * s - 0.5, f;
			int k0 = (int)floor(x);

			f = x - k0;
			if (k0 < 0) {
				k0 = 0;
				f = 0;
			}
			if (k0 >= viejo - 1) {
				k0 = viejo - 1;
				f = 0;
			}
			t[i].de = k0;
			if (f > 0) {
				t[i].n = 2;
				w[total] = (float)(1 - f);
				w[total + 1] = (float)f;
			} else {
				t[i].n = 1;
				w[total] = 1;
			}
		}
		total += t[i].n;
	}
	*pesos = w;
	return 1;
}

static u32 empaqueta(float a, float r, float g, float b)
{
	int ia, ir, ig, ib;

	if (a < 0.5f)
		return 0;
	r /= a;
	g /= a;
	b /= a;
	ia = (int)(a + 0.5f);
	ir = (int)(r * 255 + 0.5f);
	ig = (int)(g * 255 + 0.5f);
	ib = (int)(b * 255 + 0.5f);
	ia = ia > 255 ? 255 : ia;
	ir = ir > 255 ? 255 : ir < 0 ? 0 : ir;
	ig = ig > 255 ? 255 : ig < 0 ? 0 : ig;
	ib = ib > 255 ? 255 : ib < 0 ? 0 : ib;
	return (u32)ia << 24 | (u32)ir << 16 | (u32)ig << 8 | (u32)ib;
}

int op_escala(const lamina *o, lamina *d, int an, int al)
{
	toma *tx, *ty;
	float *wx = NULL, *wy = NULL, *med;
	int x, y, k;

	if (an < 1 || al < 1 || an > LAM_MAX_LADO || al > LAM_MAX_LADO)
		return 0;
	if (an == o->an && al == o->al) {
		if (!lam_nueva(d, an, al, 0))
			return 0;
		memcpy(d->p, o->p, (size_t)an * al * 4);
		return 1;
	}
	tx = so_pide((size_t)an * sizeof *tx);
	ty = so_pide((size_t)al * sizeof *ty);
	/* lo de en medio: an x o->al, cuatro números por punto (premultiplicados) */
	med = so_pide((size_t)an * (size_t)o->al * 4 * sizeof(float));
	if (!tx || !ty || !med || !pesos_de(o->an, an, tx, &wx) || !pesos_de(o->al, al, ty, &wy) || !lam_nueva(d, an, al, 0)) {
		so_suelta(tx);
		so_suelta(ty);
		so_suelta(med);
		so_suelta(wx);
		so_suelta(wy);
		return 0;
	}
	for (y = 0; y < o->al; y++) {
		const u32 *f = o->p + (size_t)y * o->an;
		float *m = med + (size_t)y * an * 4;

		for (x = 0; x < an; x++) {
			float a = 0, r = 0, g = 0, b = 0;

			for (k = 0; k < tx[x].n; k++) {
				u32 c = f[tx[x].de + k];
				float w = wx[tx[x].peso0 + k], ca = (float)ALFA(c) * w;

				a += ca;
				r += ca * ((c >> 16) & 255) / 255.0f;
				g += ca * ((c >> 8) & 255) / 255.0f;
				b += ca * (c & 255) / 255.0f;
			}
			m[4 * x] = a;
			m[4 * x + 1] = r;
			m[4 * x + 2] = g;
			m[4 * x + 3] = b;
		}
	}
	for (y = 0; y < al; y++) {
		u32 *f = d->p + (size_t)y * an;

		for (x = 0; x < an; x++) {
			float a = 0, r = 0, g = 0, b = 0;

			for (k = 0; k < ty[y].n; k++) {
				const float *m = med + ((size_t)(ty[y].de + k) * an + x) * 4;
				float w = wy[ty[y].peso0 + k];

				a += m[0] * w;
				r += m[1] * w;
				g += m[2] * w;
				b += m[3] * w;
			}
			f[x] = empaqueta(a, r, g, b);
		}
	}
	so_suelta(tx);
	so_suelta(ty);
	so_suelta(med);
	so_suelta(wx);
	so_suelta(wy);
	return 1;
}

/* el punto (x, y) de o con el bilineal (fuera: «fondo») */
static u32 muestra(const lamina *o, double x, double y, u32 fondo)
{
	int x0 = (int)floor(x - 0.5), y0 = (int)floor(y - 0.5), i;
	double fx = x - 0.5 - x0, fy = y - 0.5 - y0;
	float a = 0, r = 0, g = 0, b = 0;

	if (x < -1 || y < -1 || x > o->an + 1 || y > o->al + 1)
		return fondo;
	for (i = 0; i < 4; i++) {
		int px = x0 + (i & 1), py = y0 + (i >> 1);
		double w = ((i & 1) ? fx : 1 - fx) * ((i >> 1) ? fy : 1 - fy);
		u32 c = px < 0 || py < 0 || px >= o->an || py >= o->al ? fondo : o->p[(size_t)py * o->an + px];
		float ca = (float)(ALFA(c) * w);

		a += ca;
		r += ca * ((c >> 16) & 255) / 255.0f;
		g += ca * ((c >> 8) & 255) / 255.0f;
		b += ca * (c & 255) / 255.0f;
	}
	return empaqueta(a, r, g, b);
}

int op_inclina(const lamina *o, lamina *d, int gh, int gv, u32 fondo)
{
	double th, tv;
	int an, al, x, y;
	double dx0, dy0;

	if (gh < -89 || gh > 89 || gv < -89 || gv > 89)
		return 0;
	th = op_seno(gh * PI / 180) / op_coseno(gh * PI / 180);
	tv = op_seno(gv * PI / 180) / op_coseno(gv * PI / 180);
	/* x' = x + th*y; y' = y + tv*x' */
	{
		double w1 = o->an + fabs(th) * o->al;
		double h1 = o->al + fabs(tv) * w1;

		if (w1 > LAM_MAX_LADO || h1 > LAM_MAX_LADO)
			return 0;
		an = (int)ceil(w1 - 0.001);
		al = (int)ceil(h1 - 0.001);
	}
	if (!lam_nueva(d, an, al, fondo))
		return 0;
	dx0 = th < 0 ? -th * o->al : 0;
	dy0 = tv < 0 ? -tv * (o->an + fabs(th) * o->al) : 0;
	/* al revés (de lo nuevo a lo viejo): x1 = X; y = Y - dy0 - tv*x1; x = x1 - dx0 - th*y */
	for (y = 0; y < al; y++)
		for (x = 0; x < an; x++) {
			double ys = (y + 0.5) - dy0 - tv * (x + 0.5), xs = (x + 0.5) - dx0 - th * ys;

			d->p[(size_t)y * an + x] = muestra(o, xs, ys, fondo);
		}
	return 1;
}

int op_gira90(const lamina *o, lamina *d, int veces)
{
	int x, y, an, al;

	veces = ((veces % 4) + 4) % 4;
	an = veces % 2 ? o->al : o->an;
	al = veces % 2 ? o->an : o->al;
	if (!lam_nueva(d, an, al, 0))
		return 0;
	for (y = 0; y < o->al; y++)
		for (x = 0; x < o->an; x++) {
			u32 c = o->p[(size_t)y * o->an + x];
			int nx, ny;

			switch (veces) {
			case 1: nx = o->al - 1 - y; ny = x; break;
			case 2: nx = o->an - 1 - x; ny = o->al - 1 - y; break;
			case 3: nx = y; ny = o->an - 1 - x; break;
			default: nx = x; ny = y; break;
			}
			d->p[(size_t)ny * an + nx] = c;
		}
	return 1;
}

int op_gira(const lamina *o, lamina *d, double grados, u32 fondo)
{
	double a = grados * PI / 180, c = op_coseno(a), s = op_seno(a);
	double w = fabs(o->an * c) + fabs(o->al * s), h = fabs(o->an * s) + fabs(o->al * c);
	int an, al, x, y;
	double cx, cy, ox = o->an / 2.0, oy = o->al / 2.0;

	{
		/* los múltiplos de 90, exactos */
		double r = fmod(grados, 90.0);

		if (r < 0)
			r += 90;
		if (r < 1e-9 || 90 - r < 1e-9) {
			long v = (long)floor(grados / 90 + 0.5);

			return op_gira90(o, d, (int)(((v % 4) + 4) % 4));
		}
	}
	if (w > LAM_MAX_LADO || h > LAM_MAX_LADO)
		return 0;
	an = (int)ceil(w - 1e-6);
	al = (int)ceil(h - 1e-6);
	if (!lam_nueva(d, an, al, fondo))
		return 0;
	cx = an / 2.0;
	cy = al / 2.0;
	for (y = 0; y < al; y++)
		for (x = 0; x < an; x++) {
			double px = x + 0.5 - cx, py = y + 0.5 - cy;
			/* (al revés: de lo nuevo a lo viejo) */
			double sx = px * c + py * s + ox, sy = -px * s + py * c + oy;

			d->p[(size_t)y * an + x] = muestra(o, sx, sy, fondo);
		}
	return 1;
}

int op_lienzo(const lamina *o, lamina *d, int an, int al, u32 fondo)
{
	int y, w = an < o->an ? an : o->an;

	if (!lam_nueva(d, an, al, fondo))
		return 0;
	for (y = 0; y < al && y < o->al; y++)
		memcpy(d->p + (size_t)y * an, o->p + (size_t)y * o->an, (size_t)w * 4);
	return 1;
}

void op_voltea(lamina *l, int horizontal)
{
	int x, y;

	if (horizontal) {
		for (y = 0; y < l->al; y++) {
			u32 *f = l->p + (size_t)y * l->an;

			for (x = 0; x < l->an / 2; x++) {
				u32 t = f[x];

				f[x] = f[l->an - 1 - x];
				f[l->an - 1 - x] = t;
			}
		}
	} else {
		for (y = 0; y < l->al / 2; y++) {
			u32 *a = l->p + (size_t)y * l->an, *b = l->p + (size_t)(l->al - 1 - y) * l->an;

			for (x = 0; x < l->an; x++) {
				u32 t = a[x];

				a[x] = b[x];
				b[x] = t;
			}
		}
	}
}

void op_invierte(lamina *l)
{
	u32 i, n = (u32)l->an * (u32)l->al;

	for (i = 0; i < n; i++)
		l->p[i] ^= 0x00FFFFFF;
}

void op_grises(lamina *l)
{
	u32 i, n = (u32)l->an * (u32)l->al;

	for (i = 0; i < n; i++) {
		u32 c = l->p[i];
		/* (la luminancia de la BT.601: 0,299 R + 0,587 G + 0,114 B) */
		u32 v = (((c >> 16) & 255) * 299 + ((c >> 8) & 255) * 587 + (c & 255) * 114 + 500) / 1000;

		l->p[i] = (c & 0xFF000000u) | v << 16 | v << 8 | v;
	}
}

void op_brillo(lamina *l, int brillo, int contraste)
{
	u8 t[256];
	u32 i, n = (u32)l->an * (u32)l->al;
	double f;

	if (brillo < -100)
		brillo = -100;
	if (brillo > 100)
		brillo = 100;
	if (contraste < -100)
		contraste = -100;
	if (contraste > 100)
		contraste = 99;
	/* el contraste, alrededor del gris medio; el brillo, sumado */
	f = contraste >= 0 ? 100.0 / (100 - contraste) : (100.0 + contraste) / 100;
	for (i = 0; i < 256; i++) {
		double v = (i - 127.5) * f + 127.5 + brillo * 2.55;

		t[i] = (u8)(v < 0 ? 0 : v > 255 ? 255 : (int)(v + 0.5));
	}
	for (i = 0; i < n; i++) {
		u32 c = l->p[i];

		l->p[i] = (c & 0xFF000000u) | (u32)t[(c >> 16) & 255] << 16 | (u32)t[(c >> 8) & 255] << 8 | t[c & 255];
	}
}
