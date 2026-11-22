/* lienzo.c - la oficina de 32 bits: dibujar (oficina/comun/lienzo.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "lienzo.h"
#include "letras.h"
#include "texto.h"

int lz_nuevo(lienzo *l, int an, int al)
{
	if (an < 1)
		an = 1;
	if (al < 1)
		al = 1;
	l->p = so_pide((size_t)an * al * 4);
	if (!l->p)
		return 0;
	l->an = l->paso = an;
	l->al = al;
	lz_sin_recorte(l);
	return 1;
}

void lz_suelta(lienzo *l)
{
	so_suelta(l->p);
	l->p = NULL;
	l->an = l->al = 0;
}

void lz_sin_recorte(lienzo *l)
{
	l->rx0 = l->ry0 = 0;
	l->rx1 = l->an;
	l->ry1 = l->al;
}

void lz_recorte(lienzo *l, int x, int y, int an, int al)
{
	l->rx0 = x < 0 ? 0 : x;
	l->ry0 = y < 0 ? 0 : y;
	l->rx1 = x + an > l->an ? l->an : x + an;
	l->ry1 = y + al > l->al ? l->al : y + al;
	if (l->rx1 < l->rx0)
		l->rx1 = l->rx0;
	if (l->ry1 < l->ry0)
		l->ry1 = l->ry0;
}

u32 lz_mezcla_color(u32 f, u32 t, int a)
{
	u32 rb, g;

	if (a <= 0)
		return f;
	if (a >= 255)
		return t;
	rb = ((f & 0xFF00FF) * (u32)(256 - a) + (t & 0xFF00FF) * (u32)a) >> 8;
	g = ((f & 0x00FF00) * (u32)(256 - a) + (t & 0x00FF00) * (u32)a) >> 8;
	return (rb & 0xFF00FF) | (g & 0x00FF00);
}

void lz_punto(lienzo *l, int x, int y, u32 c)
{
	if (x >= l->rx0 && x < l->rx1 && y >= l->ry0 && y < l->ry1)
		l->p[y * l->paso + x] = c;
}

void lz_punto_a(lienzo *l, int x, int y, u32 c, int a)
{
	if (x >= l->rx0 && x < l->rx1 && y >= l->ry0 && y < l->ry1) {
		u32 *p = &l->p[y * l->paso + x];

		*p = lz_mezcla_color(*p, c, a);
	}
}

static int recorta(const lienzo *l, int *x, int *y, int *an, int *al)
{
	if (*x < l->rx0) {
		*an -= l->rx0 - *x;
		*x = l->rx0;
	}
	if (*y < l->ry0) {
		*al -= l->ry0 - *y;
		*y = l->ry0;
	}
	if (*x + *an > l->rx1)
		*an = l->rx1 - *x;
	if (*y + *al > l->ry1)
		*al = l->ry1 - *y;
	return *an > 0 && *al > 0;
}

void lz_rect(lienzo *l, int x, int y, int an, int al, u32 c)
{
	int f, i;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++) {
		u32 *p = l->p + (y + f) * l->paso + x;

		for (i = 0; i < an; i++)
			p[i] = c;
	}
}

void lz_rect_a(lienzo *l, int x, int y, int an, int al, u32 c, int a)
{
	int f, i;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++) {
		u32 *p = l->p + (y + f) * l->paso + x;

		for (i = 0; i < an; i++)
			p[i] = lz_mezcla_color(p[i], c, a);
	}
}

void lz_raya_h(lienzo *l, int x, int y, int an, u32 c)
{
	lz_rect(l, x, y, an, 1, c);
}

void lz_raya_v(lienzo *l, int x, int y, int al, u32 c)
{
	lz_rect(l, x, y, 1, al, c);
}

void lz_punteada_h(lienzo *l, int x, int y, int an, u32 c)
{
	int i;

	for (i = 0; i < an; i += 2)
		lz_punto(l, x + i, y, c);
}

void lz_punteada_v(lienzo *l, int x, int y, int al, u32 c)
{
	int i;

	for (i = 0; i < al; i += 2)
		lz_punto(l, x, y + i, c);
}

void lz_marco(lienzo *l, int x, int y, int an, int al, u32 c)
{
	lz_rect(l, x, y, an, 1, c);
	lz_rect(l, x, y + al - 1, an, 1, c);
	lz_rect(l, x, y, 1, al, c);
	lz_rect(l, x + an - 1, y, 1, al, c);
}

void lz_marco_grueso(lienzo *l, int x, int y, int an, int al, int g, u32 c)
{
	lz_rect(l, x, y, an, g, c);
	lz_rect(l, x, y + al - g, an, g, c);
	lz_rect(l, x, y, g, al, c);
	lz_rect(l, x + an - g, y, g, al, c);
}

/* lo que cubre de un punto (dentro del círculo de radio r) la esquina */
static int cubre_esquina(int dx, int dy, int r)
{
	int k = 0, i, j;

	for (j = 0; j < 4; j++)
		for (i = 0; i < 4; i++) {
			int x = dx * 4 + i, y = dy * 4 + j;

			if (x * x + y * y <= r * r * 16)
				k++;
		}
	return k * 255 / 16;
}

void lz_redondeado(lienzo *l, int x, int y, int an, int al, int r, u32 c)
{
	int j, i;

	if (r * 2 > an)
		r = an / 2;
	if (r * 2 > al)
		r = al / 2;
	if (r <= 0) {
		lz_rect(l, x, y, an, al, c);
		return;
	}
	lz_rect(l, x, y + r, an, al - 2 * r, c);
	for (j = 0; j < r; j++) {
		lz_rect(l, x + r, y + j, an - 2 * r, 1, c);
		lz_rect(l, x + r, y + al - 1 - j, an - 2 * r, 1, c);
		for (i = 0; i < r; i++) {
			int a = cubre_esquina(r - i - 1, r - j - 1, r);

			lz_punto_a(l, x + i, y + j, c, a);
			lz_punto_a(l, x + an - 1 - i, y + j, c, a);
			lz_punto_a(l, x + i, y + al - 1 - j, c, a);
			lz_punto_a(l, x + an - 1 - i, y + al - 1 - j, c, a);
		}
	}
}

void lz_marco_redondeado(lienzo *l, int x, int y, int an, int al, int r, u32 c)
{
	int i, j;

	if (r * 2 > an)
		r = an / 2;
	if (r * 2 > al)
		r = al / 2;
	lz_rect(l, x + r, y, an - 2 * r, 1, c);
	lz_rect(l, x + r, y + al - 1, an - 2 * r, 1, c);
	lz_rect(l, x, y + r, 1, al - 2 * r, c);
	lz_rect(l, x + an - 1, y + r, 1, al - 2 * r, c);
	for (j = 0; j < r; j++)
		for (i = 0; i < r; i++) {
			int a = cubre_esquina(r - i - 1, r - j - 1, r) - cubre_esquina(r - i - 1, r - j - 1, r - 1);

			if (a < 0)
				a = 0;
			lz_punto_a(l, x + i, y + j, c, a);
			lz_punto_a(l, x + an - 1 - i, y + j, c, a);
			lz_punto_a(l, x + i, y + al - 1 - j, c, a);
			lz_punto_a(l, x + an - 1 - i, y + al - 1 - j, c, a);
		}
}

void lz_degradado(lienzo *l, int x, int y, int an, int al, u32 a, u32 b)
{
	int f;

	for (f = 0; f < al; f++)
		lz_rect(l, x, y + f, an, 1, lz_mezcla_color(a, b, al > 1 ? f * 255 / (al - 1) : 0));
}

/* ---- lo suavizado: 4 x 4 muestras por punto ---- */
static int dentro_pol(const int *xy, int n, int px, int py)
{
	int i, j, c = 0;

	for (i = 0, j = n - 1; i < n; j = i++) {
		int xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];

		if ((yi > py) != (yj > py)) {
			i64 xc = xi + (i64)(xj - xi) * (py - yi) / (yj - yi);

			if (px < xc)
				c = !c;
		}
	}
	return c;
}

void lz_poligono(lienzo *l, const int *xy, int n, u32 c)
{
	int x0 = 1 << 30, y0 = 1 << 30, x1 = -(1 << 30), y1 = -(1 << 30), i, x, y;

	for (i = 0; i < n; i++) {
		if (xy[2 * i] < x0) x0 = xy[2 * i];
		if (xy[2 * i] > x1) x1 = xy[2 * i];
		if (xy[2 * i + 1] < y0) y0 = xy[2 * i + 1];
		if (xy[2 * i + 1] > y1) y1 = xy[2 * i + 1];
	}
	x0 >>= 4;
	y0 >>= 4;
	x1 = (x1 + 15) >> 4;
	y1 = (y1 + 15) >> 4;
	if (x0 < l->rx0) x0 = l->rx0;
	if (y0 < l->ry0) y0 = l->ry0;
	if (x1 > l->rx1) x1 = l->rx1;
	if (y1 > l->ry1) y1 = l->ry1;
	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++) {
			int k = 0, a, b;

			for (b = 0; b < 4; b++)
				for (a = 0; a < 4; a++)
					k += dentro_pol(xy, n, x * 16 + a * 4 + 2, y * 16 + b * 4 + 2);
			if (k)
				lz_punto_a(l, x, y, c, k * 255 / 16);
		}
}

void lz_linea(lienzo *l, int x0, int y0, int x1, int y1, int g, u32 c)
{
	int dx = x1 - x0, dy = y1 - y0, nx, ny, xy[8];
	double d = so_raiz((double)dx * dx + (double)dy * dy);

	if (d < 1)
		return;
	nx = (int)(-dy * g / 2 / d);
	ny = (int)(dx * g / 2 / d);
	xy[0] = x0 + nx; xy[1] = y0 + ny;
	xy[2] = x1 + nx; xy[3] = y1 + ny;
	xy[4] = x1 - nx; xy[5] = y1 - ny;
	xy[6] = x0 - nx; xy[7] = y0 - ny;
	lz_poligono(l, xy, 4, c);
}

void lz_circulo(lienzo *l, int cx, int cy, int r, u32 c)
{
	int x, y, a, b;
	int x0 = (cx - r) >> 4, x1 = (cx + r + 15) >> 4, y0 = (cy - r) >> 4, y1 = (cy + r + 15) >> 4;

	for (y = y0; y < y1; y++)
		for (x = x0; x < x1; x++) {
			int k = 0;

			for (b = 0; b < 4; b++)
				for (a = 0; a < 4; a++) {
					int px = x * 16 + a * 4 + 2 - cx, py = y * 16 + b * 4 + 2 - cy;

					if (px * px + py * py <= r * r)
						k++;
				}
			if (k)
				lz_punto_a(l, x, y, c, k * 255 / 16);
		}
}

void lz_ondulada(lienzo *l, int x, int y, int an, u32 c)
{
	int i;

	/* una onda de 4 puntos de largo y 2 de alto (como los procesadores de
	 * siempre; además del color, su forma: se ve sin distinguir el rojo) */
	for (i = 0; i < an; i++) {
		int f = i & 3, dy = f == 0 ? 0 : f == 1 ? 1 : f == 2 ? 2 : 1;

		lz_punto(l, x + i, y + dy, c);
		lz_punto_a(l, x + i, y + dy + 1, c, 90);
	}
}

void lz_imagen(lienzo *l, const u32 *px, int an, int al, int x, int y, int dan, int dal, int alfa)
{
	int f, i;

	if (dan <= 0 || dal <= 0 || an <= 0 || al <= 0)
		return;
	for (f = 0; f < dal; f++) {
		int yy = y + f, sy;
		u32 *d;

		if (yy < l->ry0 || yy >= l->ry1)
			continue;
		sy = (int)((i64)f * al / dal);
		d = l->p + yy * l->paso;
		for (i = 0; i < dan; i++) {
			int xx = x + i;
			u32 v;

			if (xx < l->rx0 || xx >= l->rx1)
				continue;
			if (dan < an && dal < al) {
				/* reduciendo: la media de lo que cae en el punto (2 x 2 muestras) */
				int sx0 = (int)((i64)i * an / dan), sx1 = (int)((i64)(i + 1) * an / dan) - 1;
				int sy1 = (int)((i64)(f + 1) * al / dal) - 1;
				u32 a = px[sy * an + sx0], b = px[sy * an + (sx1 > sx0 ? sx1 : sx0)];
				u32 cc = px[(sy1 > sy ? sy1 : sy) * an + sx0], dd = px[(sy1 > sy ? sy1 : sy) * an + (sx1 > sx0 ? sx1 : sx0)];
				u32 rb = ((a & 0xFF00FF) + (b & 0xFF00FF) + (cc & 0xFF00FF) + (dd & 0xFF00FF)) >> 2;
				u32 g = ((a & 0x00FF00) + (b & 0x00FF00) + (cc & 0x00FF00) + (dd & 0x00FF00)) >> 2;
				u32 al4 = ((a >> 24) + (b >> 24) + (cc >> 24) + (dd >> 24)) >> 2;

				v = (rb & 0xFF00FF) | (g & 0x00FF00) | al4 << 24;
			} else
				v = px[sy * an + (int)((i64)i * an / dan)];
			if (alfa)
				d[xx] = lz_mezcla_color(d[xx], v & 0xFFFFFF, (int)(v >> 24));
			else
				d[xx] = v & 0xFFFFFF;
		}
	}
}

void lz_copia(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al)
{
	int f;

	if (ox < 0) { an += ox; x -= ox; ox = 0; }
	if (oy < 0) { al += oy; y -= oy; oy = 0; }
	if (ox + an > o->an) an = o->an - ox;
	if (oy + al > o->al) al = o->al - oy;
	if (x < d->rx0) { an -= d->rx0 - x; ox += d->rx0 - x; x = d->rx0; }
	if (y < d->ry0) { al -= d->ry0 - y; oy += d->ry0 - y; y = d->ry0; }
	if (x + an > d->rx1) an = d->rx1 - x;
	if (y + al > d->ry1) al = d->ry1 - y;
	if (an <= 0 || al <= 0)
		return;
	for (f = 0; f < al; f++)
		memmove(d->p + (y + f) * d->paso + x, o->p + (oy + f) * o->paso + ox, (size_t)an * 4);
}

/* ---- las letras ---- */
static void pinta_glifo(lienzo *l, const glifo_px *g, int x, int y, u32 c)
{
	int f, i;

	if (!g || !g->an || !g->al)
		return;
	x += g->x0;
	y += g->y0;
	for (f = 0; f < g->al; f++) {
		int yy = y + f;
		const u8 *s;
		u32 *d;

		if (yy < l->ry0 || yy >= l->ry1)
			continue;
		s = g->gris + f * g->an;
		d = l->p + yy * l->paso;
		for (i = 0; i < g->an; i++) {
			int xx = x + i;

			if (s[i] && xx >= l->rx0 && xx < l->rx1)
				d[xx] = s[i] == 255 ? c : lz_mezcla_color(d[xx], c, s[i]);
		}
	}
}

int lz_letra(lienzo *l, int cara, int tam, int falsas, int x4, int y, u32 letra, u32 c)
{
	int real;
	u32 g = letra_glifo(cara, letra, &real);
	int adv = letra_avance(real, g);
	/* x4 en cuartos: el punto entero y el tercio que sobra */
	int px = x4 >> 2, sub = ((x4 & 3) * 3) >> 2;

	if (l && letra > ' ')
		pinta_glifo(l, letra_dibujo(real, g, tam, sub, falsas), px, y, c);
	return (int)(((i64)adv * tam + letra_em(real) / 2) / letra_em(real)) + ((falsas & FALSA_NEGRITA) ? 2 : 0);
}

int lz_texto(lienzo *l, int cara, int tam, int falsas, int x, int y, const char *t, u32 c)
{
	u32 n = largo(t), i = 0, u;
	int x4 = x * 4;

	while (i < n) {
		i += (u32)utf8_lee((const u8 *)t + i, n - i, &u);
		x4 += lz_letra(l, cara, tam, falsas, x4, y, u, c);
	}
	return (x4 + 3) / 4 - x;
}

int lz_ancho(int cara, int tam, int falsas, const char *t)
{
	return lz_texto(NULL, cara, tam, falsas, 0, 0, t, 0);
}

static int ui_tam = 13;

void lz_ui_tamano(int puntos)
{
	ui_tam = puntos < 9 ? 9 : puntos > 24 ? 24 : puntos;
}

static int ui_cara(int negrita)
{
	return letra_cara(FAM_SANS, negrita, 0, NULL);
}

int lz_ui(lienzo *l, int x, int y, const char *t, u32 c)
{
	return lz_texto(l, ui_cara(0), ui_tam * 4, 0, x, y, t, c);
}

int lz_ui_negrita(lienzo *l, int x, int y, const char *t, u32 c)
{
	return lz_texto(l, ui_cara(1), ui_tam * 4, 0, x, y, t, c);
}

int lz_ui_ancho(const char *t)
{
	return lz_ancho(ui_cara(0), ui_tam * 4, 0, t);
}

int lz_ui_ancho_negrita(const char *t)
{
	return lz_ancho(ui_cara(1), ui_tam * 4, 0, t);
}

int lz_ui_alto(void)
{
	return ui_tam * 17 / 10 + 1;
}

int lz_ui_base(void)
{
	return (lz_ui_alto() - ui_tam) / 2 + ui_tam * 92 / 100;
}
