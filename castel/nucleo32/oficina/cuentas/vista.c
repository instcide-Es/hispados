/* vista.c - Cuentas 2: la hoja en la ventana: los rótulos de las columnas
 * (A, B... IV) y de las filas (1... 65536), las celdas (celdas.c, en un
 * pintor de lienzo: lo mismo que sale en el PDF), lo inmovilizado arriba y a
 * la izquierda, el zoom, lo elegido (un velo, un marco grueso y el
 * cuadradito de rellenar), lo cortado (un marco de trazos), lo señalado al
 * escribir una fórmula, los botones del autofiltro, los gráficos y la celda
 * que se está corrigiendo. Lo elegido se ve sin el color: el marco grueso y
 * los rótulos hundidos y en negrita.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ventana.h"
#include "celdas.h"
#include "grafico.h"
#include "pintor.h"
#include "operar.h"
#include "letras.h"

#define BORDE 3			/* lo que coge el ratón alrededor de un borde */

hoja *cu_hoja(void);

/* ---- la geometría: las posiciones, con la misma cuenta que celdas.c
 * (el principio más lo que miden las de antes por la escala, redondeado) ---- */
static double esc(void)
{
	return C.g.zoom / 100.0;
}

static int redondo(double v)
{
	return (int)(v >= 0 ? v + 0.5 : v - 0.5);
}

int vs_ancho_px(u32 c)
{
	return redondo(hj_ancho(cu_hoja(), c) * esc());
}

int vs_alto_px(u32 f)
{
	return redondo(hj_alto(cu_hoja(), f) * esc());
}

void vs_coloca(int x, int y, int an, int al)
{
	hoja *h = cu_hoja();
	double e, s = 0;
	u32 k;

	C.g.zoom = h->zoom ? h->zoom : 100;
	C.g.x = x;
	C.g.y = y;
	C.g.an = an;
	C.g.al = al;
	C.g.rot_an = lz_ui_ancho("65536") + 12;
	C.g.rot_al = lz_ui_alto() + 4;
	C.g.cx = x + C.g.rot_an;
	C.g.cy = y + C.g.rot_al;
	C.g.can = an - C.g.rot_an;
	C.g.cal = al - C.g.rot_al;
	if (C.g.can < 10)
		C.g.can = 10;
	if (C.g.cal < 10)
		C.g.cal = 10;
	e = esc();
	for (k = 0; k < h->inm_c && k < CU_COLS; k++)
		s += hj_ancho(h, k) * e;
	C.g.fx = C.g.cx + redondo(s);
	s = 0;
	for (k = 0; k < h->inm_f && k < CU_FILAS; k++)
		s += hj_alto(h, k) * e;
	C.g.fy = C.g.cy + redondo(s);
	/* (lo inmovilizado no puede taparlo todo) */
	if (C.g.fx > C.g.cx + C.g.can - 20)
		C.g.fx = C.g.cx + C.g.can - 20;
	if (C.g.fy > C.g.cy + C.g.cal - 20)
		C.g.fy = C.g.cy + C.g.cal - 20;
	if (h->vista_c < h->inm_c)
		h->vista_c = h->inm_c;
	if (h->vista_f < h->inm_f)
		h->vista_f = h->inm_f;
}

/* dónde empieza la columna c (en x) y si se ve */
int vs_col_x(u32 c, int *x)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 k;

	if (c < h->inm_c) {
		for (k = 0; k < c; k++)
			s += hj_ancho(h, k) * e;
		*x = C.g.cx + redondo(s);
		return *x < C.g.fx;
	}
	if (c < h->vista_c)
		return 0;
	for (k = h->vista_c; k < c; k++) {
		s += hj_ancho(h, k) * e;
		if (C.g.fx + s > C.g.cx + C.g.can)
			return 0;
	}
	*x = C.g.fx + redondo(s);
	return *x < C.g.cx + C.g.can;
}

int vs_fila_y(u32 f, int *y)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 k;

	if (f < h->inm_f) {
		for (k = 0; k < f; k++)
			s += hj_alto(h, k) * e;
		*y = C.g.cy + redondo(s);
		return *y < C.g.fy;
	}
	if (f < h->vista_f)
		return 0;
	for (k = h->vista_f; k < f; k++) {
		s += hj_alto(h, k) * e;
		if (C.g.fy + s > C.g.cy + C.g.cal)
			return 0;
	}
	*y = C.g.fy + redondo(s);
	return *y < C.g.cy + C.g.cal;
}

/* el rectángulo de la celda (o de la combinada que la tiene), aunque no se vea entera */
int vs_celda_rect(u32 f, u32 c, int *x, int *y, int *an, int *al)
{
	hoja *h = cu_hoja();
	int i = hj_combinada(h, f, c), x1, y1, ok;
	u32 f1 = f, c1 = c, k;
	double e = esc(), s;

	if (i >= 0) {
		f = h->comb[i].f0;
		c = h->comb[i].c0;
		f1 = h->comb[i].f1;
		c1 = h->comb[i].c1;
	}
	ok = vs_col_x(c, x) && vs_fila_y(f, y);
	if (!ok) {
		/* (la esquina no se ve: quizá se ve un trozo de la combinada) */
		if (i < 0)
			return 0;
		{
			u32 cc, ff;
			int hay = 0;

			for (cc = c; cc <= c1 && !hay; cc++)
				for (ff = f; ff <= f1 && !hay; ff++)
					if (vs_col_x(cc, x) && vs_fila_y(ff, y)) {
						hay = 1;
						c = cc;
						f = ff;
					}
			if (!hay)
				return 0;
		}
	}
	s = 0;
	for (k = c; k <= c1; k++)
		s += hj_ancho(h, k) * e;
	x1 = *x + redondo(s);
	s = 0;
	for (k = f; k <= f1; k++)
		s += hj_alto(h, k) * e;
	y1 = *y + redondo(s);
	*an = x1 - *x;
	*al = y1 - *y;
	return 1;
}

u32 vs_filas_vistas(void)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 f = h->vista_f, n = 0;

	while (f < CU_FILAS && C.g.fy + s < C.g.cy + C.g.cal) {
		s += hj_alto(h, f) * e;
		f++;
		n++;
	}
	return n ? n : 1;
}

u32 vs_cols_vistas(void)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 c = h->vista_c, n = 0;

	while (c < CU_COLS && C.g.fx + s < C.g.cx + C.g.can) {
		s += hj_ancho(h, c) * e;
		c++;
		n++;
	}
	return n ? n : 1;
}

void vs_a_la_vista(u32 f, u32 c)
{
	hoja *h = cu_hoja();
	double e = esc();

	if (f >= h->inm_f) {
		if (f < h->vista_f)
			h->vista_f = f;
		else {
			/* que quepa entera por abajo */
			double s = 0, hueco = C.g.cy + C.g.cal - C.g.fy;
			u32 k = f + 1;

			while (k > h->inm_f && s + hj_alto(h, k - 1) * e <= hueco) {
				s += hj_alto(h, k - 1) * e;
				k--;
			}
			if (k > h->vista_f)
				h->vista_f = k <= f ? k : f;
		}
	}
	if (c >= h->inm_c) {
		if (c < h->vista_c)
			h->vista_c = c;
		else {
			double s = 0, hueco = C.g.cx + C.g.can - C.g.fx;
			u32 k = c + 1;

			while (k > h->inm_c && s + hj_ancho(h, k - 1) * e <= hueco) {
				s += hj_ancho(h, k - 1) * e;
				k--;
			}
			if (k > h->vista_c)
				h->vista_c = k <= c ? k : c;
		}
	}
}

/* la columna en x (CU_COLS si no hay); *borde: si está en el borde derecho */
static u32 col_en(int x, int *borde)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 k;

	if (borde)
		*borde = 0;
	if (x < C.g.cx)
		return CU_COLS;
	if (x < C.g.fx) {
		for (k = 0; k < h->inm_c; k++) {
			double w = hj_ancho(h, k) * e;

			if (borde && w > 0 && so_abs(C.g.cx + s + w - x) <= BORDE)
				*borde = 1;
			if (x < C.g.cx + redondo(s + w) && (w > 0 || 0))
				return k;
			s += w;
		}
		return h->inm_c ? h->inm_c - 1 : 0;
	}
	for (k = h->vista_c; k < CU_COLS; k++) {
		double w = hj_ancho(h, k) * e;

		if (borde && w > 0 && so_abs(C.g.fx + s + w - x) <= BORDE)
			*borde = 1;
		if (x < C.g.fx + redondo(s + w))
			return k;
		s += w;
		if (C.g.fx + s > C.g.cx + C.g.can + 2000)
			break;
	}
	return CU_COLS - 1;
}

static u32 fila_en(int y, int *borde)
{
	hoja *h = cu_hoja();
	double e = esc(), s = 0;
	u32 k;

	if (borde)
		*borde = 0;
	if (y < C.g.cy)
		return CU_FILAS;
	if (y < C.g.fy) {
		for (k = 0; k < h->inm_f; k++) {
			double w = hj_alto(h, k) * e;

			if (borde && w > 0 && so_abs(C.g.cy + s + w - y) <= BORDE)
				*borde = 1;
			if (y < C.g.cy + redondo(s + w))
				return k;
			s += w;
		}
		return h->inm_f ? h->inm_f - 1 : 0;
	}
	for (k = h->vista_f; k < CU_FILAS; k++) {
		double w = hj_alto(h, k) * e;

		if (borde && w > 0 && so_abs(C.g.fy + s + w - y) <= BORDE)
			*borde = 1;
		if (y < C.g.fy + redondo(s + w))
			return k;
		s += w;
		if (C.g.fy + s > C.g.cy + C.g.cal + 2000)
			break;
	}
	return CU_FILAS - 1;
}

void vs_grafico_rect(const grafico *g, int *x, int *y, int *an, int *al)
{
	hoja *h = cu_hoja();
	double e = esc(), s;
	u32 k;
	int x0, y0;

	/* (la esquina: aunque su celda no se vea, se cuenta desde lo que se ve) */
	s = 0;
	if (g->c < h->inm_c) {
		for (k = 0; k < g->c; k++)
			s += hj_ancho(h, k) * e;
		x0 = C.g.cx + redondo(s);
	} else if (g->c >= h->vista_c) {
		for (k = h->vista_c; k < g->c && s < 100000; k++)
			s += hj_ancho(h, k) * e;
		x0 = C.g.fx + redondo(s);
	} else {
		for (k = g->c; k < h->vista_c && s < 100000; k++)
			s += hj_ancho(h, k) * e;
		x0 = C.g.fx - redondo(s);
	}
	s = 0;
	if (g->f < h->inm_f) {
		for (k = 0; k < g->f; k++)
			s += hj_alto(h, k) * e;
		y0 = C.g.cy + redondo(s);
	} else if (g->f >= h->vista_f) {
		for (k = h->vista_f; k < g->f && s < 100000; k++)
			s += hj_alto(h, k) * e;
		y0 = C.g.fy + redondo(s);
	} else {
		for (k = g->f; k < h->vista_f && s < 100000; k++)
			s += hj_alto(h, k) * e;
		y0 = C.g.fy - redondo(s);
	}
	*x = x0 + redondo(g->dx * e);
	*y = y0 + redondo(g->dy * e);
	*an = redondo(g->an * e);
	*al = redondo(g->al * e);
}

int vs_en(int x, int y, u32 *f, u32 *c, int *i)
{
	hoja *h = cu_hoja();
	int bc, bf;
	u32 k;

	*f = CU_FILAS;
	*c = CU_COLS;
	if (i)
		*i = -1;
	if (x < C.g.x || y < C.g.y || x >= C.g.x + C.g.an || y >= C.g.y + C.g.al)
		return VS_NADA;
	/* los gráficos, encima de todo (el último, el de más arriba) */
	for (k = h->n_gr; k-- > 0;) {
		int gx, gy, gan, gal;

		vs_grafico_rect(&h->gr[k], &gx, &gy, &gan, &gal);
		if (x >= gx && y >= gy && x < gx + gan && y < gy + gal && x >= C.g.cx && y >= C.g.cy) {
			if (i)
				*i = (int)k;
			if ((int)k == C.grafico && x >= gx + gan - 9 && y >= gy + gal - 9)
				return VS_GRAF_TAM;
			return VS_GRAFICO;
		}
	}
	if (x < C.g.cx && y < C.g.cy)
		return VS_ESQUINA;
	if (y < C.g.cy) {
		*c = col_en(x, &bc);
		if (bc) {
			/* (el borde de la derecha de la columna de la izquierda, si se está justo antes) */
			int x0;

			if (vs_col_x(*c, &x0) && x - x0 <= BORDE && *c > 0)
				(*c)--;
			return VS_BORDE_COL;
		}
		return VS_ROT_COL;
	}
	if (x < C.g.cx) {
		*f = fila_en(y, &bf);
		if (bf) {
			int y0;

			if (vs_fila_y(*f, &y0) && y - y0 <= BORDE && *f > 0)
				(*f)--;
			return VS_BORDE_FILA;
		}
		return VS_ROT_FILA;
	}
	*c = col_en(x, NULL);
	*f = fila_en(y, NULL);
	/* el cuadradito de rellenar, abajo a la derecha de lo elegido */
	{
		rango s = cu_seleccion();
		int sx, sy, san, sal, ex, ey, ean, eal;

		if (vs_celda_rect(s.f1, s.c1, &ex, &ey, &ean, &eal) && !C.editando) {
			sx = ex + ean;
			sy = ey + eal;
			if (x >= sx - 4 && x <= sx + 3 && y >= sy - 4 && y <= sy + 3)
				return VS_RELLENO;
		}
		/* el borde de lo elegido (para moverlo) */
		if (!C.editando && vs_celda_rect(s.f0, s.c0, &sx, &sy, &san, &sal) && vs_celda_rect(s.f1, s.c1, &ex, &ey, &ean, &eal)) {
			int x1 = ex + ean, y1 = ey + eal;
			int cerca_x = (so_abs(x - sx) <= 2 || so_abs(x - x1) <= 2) && y >= sy && y <= y1;
			int cerca_y = (so_abs(y - sy) <= 2 || so_abs(y - y1) <= 2) && x >= sx && x <= x1;

			if (cerca_x || cerca_y)
				return VS_BORDE_SEL;
		}
	}
	/* el botón del autofiltro */
	if (h->hay_filtro && *f == h->filtro.f0 && *c >= h->filtro.c0 && *c <= h->filtro.c1) {
		int cx0, cy0, can0, cal0;

		if (vs_celda_rect(*f, *c, &cx0, &cy0, &can0, &cal0) && x >= cx0 + can0 - 16 && y >= cy0 + cal0 - 16)
			return VS_FILTRO;
	}
	return VS_CELDA;
}

/* ---- pintar ---- */
static void rotulo(lienzo *l, int x, int y, int an, int al, const char *t, int elegido, int todo)
{
	u32 fondo = todo ? 0x9DB4D6 : elegido ? 0xC9D6EA : 0xE9EDF3;
	int w = lz_ui_ancho(t);

	if (an <= 0 || al <= 0)
		return;
	lz_rect(l, x, y, an, al, fondo);
	lz_raya_v(l, x + an - 1, y, al, 0xA8B3C4);
	lz_raya_h(l, x, y + al - 1, an, 0xA8B3C4);
	if (elegido) {
		/* (hundido y en negrita: no solo el color) */
		lz_raya_h(l, x, y, an, 0x7A8699);
		lz_raya_v(l, x, y, al, 0x7A8699);
	}
	lz_recorte(l, x + 1, y, an - 2, al);
	if (elegido)
		lz_ui_negrita(l, x + (an - lz_ui_ancho_negrita(t)) / 2, y + (al - lz_ui_alto()) / 2 + lz_ui_base(), t, 0x101820);
	else
		lz_ui(l, x + (an - w) / 2, y + (al - lz_ui_alto()) / 2 + lz_ui_base(), t, 0x2A3442);
	lz_sin_recorte(l);
}

static void marco(lienzo *l, int x, int y, int an, int al, int g, u32 c)
{
	lz_rect(l, x, y, an, g, c);
	lz_rect(l, x, y + al - g, an, g, c);
	lz_rect(l, x, y, g, al, c);
	lz_rect(l, x + an - g, y, g, al, c);
}

static void marco_trazos(lienzo *l, int x, int y, int an, int al, u32 c, int fase)
{
	int k;

	for (k = 0; k < an; k++)
		if (((k + fase) / 4) % 2 == 0) {
			lz_punto(l, x + k, y, c);
			lz_punto(l, x + k, y + 1, c);
			lz_punto(l, x + k, y + al - 1, c);
			lz_punto(l, x + k, y + al - 2, c);
		}
	for (k = 0; k < al; k++)
		if (((k + fase) / 4) % 2 == 0) {
			lz_punto(l, x, y + k, c);
			lz_punto(l, x + 1, y + k, c);
			lz_punto(l, x + an - 1, y + k, c);
			lz_punto(l, x + an - 2, y + k, c);
		}
}

/* el rectángulo de pantalla de un rango (lo que se ve de él); 0 si nada */
static int rect_rango(rango r, int *x, int *y, int *an, int *al)
{
	hoja *h = cu_hoja();
	u32 c0 = r.c0, f0 = r.f0, c1 = r.c1, f1 = r.f1;
	int xa, ya, xb, yb, w, hh;

	/* (lo de antes de lo que se ve, desde lo primero que se ve) */
	if (c0 >= h->inm_c && c0 < h->vista_c)
		c0 = h->vista_c;
	if (f0 >= h->inm_f && f0 < h->vista_f)
		f0 = h->vista_f;
	if (c0 > c1 || f0 > f1)
		return 0;
	if (c0 < h->inm_c && c1 >= h->inm_c && c1 < h->vista_c)
		c1 = h->inm_c - 1;
	if (f0 < h->inm_f && f1 >= h->inm_f && f1 < h->vista_f)
		f1 = h->inm_f - 1;
	if (!vs_col_x(c0, &xa) || !vs_fila_y(f0, &ya))
		return 0;
	/* el final: el de la última que se ve */
	{
		u32 k = c1;

		while (k > c0 && !vs_col_x(k, &xb))
			k--;
		if (!vs_col_x(k, &xb))
			return 0;
		w = vs_ancho_px(k);
		xb += w;
		if (k < c1 && c1 >= h->vista_c)
			xb = C.g.cx + C.g.can;
		k = f1;
		while (k > f0 && !vs_fila_y(k, &yb))
			k--;
		if (!vs_fila_y(k, &yb))
			return 0;
		hh = vs_alto_px(k);
		yb += hh;
		if (k < f1 && f1 >= h->vista_f)
			yb = C.g.cy + C.g.cal;
	}
	*x = xa;
	*y = ya;
	*an = xb - xa;
	*al = yb - ya;
	return *an > 0 && *al > 0;
}

/* las celdas de un cuarto (inmovilizado o no) */
static void cuarto(pintor *p, lienzo *l, u32 f0, u32 c0, int x, int y, int x1, int y1)
{
	hoja *h = cu_hoja();
	trozo_hoja t;
	double e = esc(), s;
	u32 f, c;

	if (x1 <= x || y1 <= y)
		return;
	memset(&t, 0, sizeof t);
	t.l = C.lib;
	t.h = h;
	t.f0 = f0;
	t.c0 = c0;
	t.x = x;
	t.y = y;
	t.esc = e;
	t.rejilla = !h->sin_rejilla;
	t.comentarios = 1;
	t.color_rejilla = 0xD4D9E1;
	for (s = 0, f = f0; f < CU_FILAS && y + s < y1; f++)
		s += hj_alto(h, f) * e;
	t.f1 = f > f0 ? f - 1 : f0;
	for (s = 0, c = c0; c < CU_COLS && x + s < x1; c++)
		s += hj_ancho(h, c) * e;
	t.c1 = c > c0 ? c - 1 : c0;
	pt_recorte(p, x, y, x1 - x, y1 - y);
	cel_pinta(p, &t);
	pt_vuelve(p);
	(void)l;
}

/* el texto que se corrige, en su celda (crece a la derecha si no cabe) */
static void pinta_editor(lienzo *l)
{
	hoja *h = cu_hoja();
	int x, y, an, al, cara, falsas, tam, w, xc, i;
	const estilo *e;
	char *t;
	u32 cur8 = 0;

	if (C.editando != 1 || C.ed_hoja != h->id || !vs_celda_rect(C.ed_f, C.ed_c, &x, &y, &an, &al))
		return;
	e = lib_estilo(C.lib, hj_estilo(h, C.ed_f, C.ed_c));
	cara = cel_cara(e, &falsas);
	tam = (int)(e->tam / 2.0 * 96.0 / 72.0 * esc() * 4 + 0.5);
	t = utf8_de_u32(C.ed, C.ed_n);
	if (!t)
		return;
	/* (lo que mide, para crecer) */
	w = lz_ancho(cara, tam, falsas, t) + 8;
	if (w > an)
		an = w;
	if (x + an > C.g.cx + C.g.can)
		an = C.g.cx + C.g.can - x;
	lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
	lz_rect(l, x, y, an, al, 0xFFFFFF);
	marco(l, x - 1, y - 1, an + 2, al + 2, 2, 0x1B3A6B);
	{
		int base = y + al - 4 - (al > 24 ? (al - 20) / 2 : 0);

		/* lo elegido, de fondo */
		if (C.ed_ancla != C.ed_cur) {
			u32 a = C.ed_ancla < C.ed_cur ? C.ed_ancla : C.ed_cur, b = C.ed_ancla < C.ed_cur ? C.ed_cur : C.ed_ancla;
			char *p0 = utf8_de_u32(C.ed, a), *p1 = utf8_de_u32(C.ed, b);

			if (p0 && p1) {
				int xa = x + 3 + lz_ancho(cara, tam, falsas, p0), xb = x + 3 + lz_ancho(cara, tam, falsas, p1);

				lz_rect(l, xa, y + 2, xb - xa, al - 4, 0xB5CCF0);
			}
			so_suelta(p0);
			so_suelta(p1);
		}
		lz_texto(l, cara, tam, falsas, x + 3, base, t, 0x101820);
		/* el cursor */
		for (i = 0; i < (int)C.ed_cur && i < (int)C.ed_n; i++) {
			char b[8];

			cur8 += (u32)utf8_pon(C.ed[i], b);
		}
		{
			char *p0 = utf8_de_u32(C.ed, C.ed_cur);

			xc = x + 3 + (p0 ? lz_ancho(cara, tam, falsas, p0) : 0);
			so_suelta(p0);
		}
		lz_rect(l, xc, y + 2, 2, al - 4, 0x101820);
	}
	lz_sin_recorte(l);
	so_suelta(t);
	(void)cur8;
}

static void boton_filtro(lienzo *l, int x, int y, int lado, int activo)
{
	lz_rect(l, x, y, lado, lado, 0xE9EDF3);
	lz_marco(l, x, y, lado, lado, 0x5A6678);
	if (activo) {
		/* filtrado: un embudo (no solo otro color) */
		int xy[12] = { (x + 3) * 16, (y + 3) * 16, (x + lado - 3) * 16, (y + 3) * 16, (x + lado / 2 + 1) * 16, (y + lado / 2) * 16,
			       (x + lado / 2 + 1) * 16, (y + lado - 3) * 16, (x + lado / 2 - 1) * 16, (y + lado - 3) * 16, (x + lado / 2 - 1) * 16, (y + lado / 2) * 16 };

		lz_poligono(l, xy, 6, 0x1F4E9E);
	} else {
		int cx = x + lado / 2, cy = y + lado / 2;
		int xy[6] = { (cx - 4) * 16, (cy - 2) * 16, (cx + 4) * 16, (cy - 2) * 16, cx * 16, (cy + 3) * 16 };

		lz_poligono(l, xy, 3, 0x101820);
	}
}

void vs_pinta(lienzo *l)
{
	hoja *h = cu_hoja();
	pintor p;
	rango sel = cu_seleccion();
	int x, y, an, al;
	u32 k;
	double e = esc();
	int x1 = C.g.cx + C.g.can, y1 = C.g.cy + C.g.cal;

	pt_de_lienzo(&p, l);
	lz_rect(l, C.g.x, C.g.y, C.g.an, C.g.al, 0xFFFFFF);
	/* las celdas: los cuatro cuartos */
	cuarto(&p, l, 0, 0, C.g.cx, C.g.cy, C.g.fx, C.g.fy);
	cuarto(&p, l, 0, h->vista_c, C.g.fx, C.g.cy, x1, C.g.fy);
	cuarto(&p, l, h->vista_f, 0, C.g.cx, C.g.fy, C.g.fx, y1);
	cuarto(&p, l, h->vista_f, h->vista_c, C.g.fx, C.g.fy, x1, y1);
	/* los botones del autofiltro */
	if (h->hay_filtro)
		for (k = h->filtro.c0; k <= h->filtro.c1; k++) {
			int cx0, cy0, can0, cal0, lado;
			u32 j;
			int activo = 0;

			if (!vs_celda_rect(h->filtro.f0, k, &cx0, &cy0, &can0, &cal0))
				continue;
			for (j = 0; j < h->n_fcol; j++)
				if (h->fcol[j].c == k && h->fcol[j].tipo != FI_TODO)
					activo = 1;
			lado = cal0 - 2 < 16 ? cal0 - 2 : 16;
			if (lado < 8)
				continue;
			lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
			boton_filtro(l, cx0 + can0 - lado - 1, cy0 + cal0 - lado - 1, lado, activo);
			lz_sin_recorte(l);
		}
	/* los gráficos */
	for (k = 0; k < h->n_gr; k++) {
		int gx, gy, gan, gal;

		vs_grafico_rect(&h->gr[k], &gx, &gy, &gan, &gal);
		if (gx >= x1 || gy >= y1 || gx + gan <= C.g.cx || gy + gal <= C.g.cy)
			continue;
		pt_recorte(&p, C.g.cx, C.g.cy, C.g.can, C.g.cal);
		gr_pinta(&p, C.lib, &h->gr[k], gx, gy, gan, gal, e);
		pt_vuelve(&p);
		if ((int)k == C.grafico) {
			/* elegido: el marco y los cuadraditos de las esquinas (el de abajo a la derecha, el tamaño) */
			lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
			marco(l, gx - 1, gy - 1, gan + 2, gal + 2, 2, 0x1B3A6B);
			lz_rect(l, gx - 3, gy - 3, 7, 7, 0x1B3A6B);
			lz_rect(l, gx + gan - 4, gy - 3, 7, 7, 0x1B3A6B);
			lz_rect(l, gx - 3, gy + gal - 4, 7, 7, 0x1B3A6B);
			lz_rect(l, gx + gan - 7, gy + gal - 7, 9, 9, 0x1B3A6B);
			lz_rect(l, gx + gan - 5, gy + gal - 5, 5, 5, 0xFFFFFF);
			lz_sin_recorte(l);
		}
	}
	/* lo elegido: un velo azul, un marco grueso, y la celda activa en blanco */
	if (C.grafico < 0 && rect_rango(sel, &x, &y, &an, &al)) {
		int ax, ay, aan, aal;

		lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
		if (sel.f0 != sel.f1 || sel.c0 != sel.c1) {
			lz_rect_a(l, x, y, an, al, 0x2563C9, 46);
			if (vs_celda_rect(h->cur_f, h->cur_c, &ax, &ay, &aan, &aal) && !C.editando) {
				/* (la activa, sin velo: se repinta encima) */
				pintor q;
				trozo_hoja t;

				pt_de_lienzo(&q, l);
				memset(&t, 0, sizeof t);
				t.l = C.lib;
				t.h = h;
				t.f0 = t.f1 = h->cur_f;
				t.c0 = t.c1 = h->cur_c;
				{
					int i = hj_combinada(h, h->cur_f, h->cur_c);

					if (i >= 0) {
						t.f0 = h->comb[i].f0;
						t.f1 = h->comb[i].f1;
						t.c0 = h->comb[i].c0;
						t.c1 = h->comb[i].c1;
					}
				}
				t.x = ax;
				t.y = ay;
				t.esc = e;
				t.rejilla = !h->sin_rejilla;
				t.comentarios = 1;
				t.color_rejilla = 0xD4D9E1;
				pt_recorte(&q, ax, ay, aan, aal);
				cel_pinta(&q, &t);
				pt_vuelve(&q);
			}
		}
		marco(l, x - 1, y - 1, an + 2, al + 2, 3, 0x1B3A6B);
		if (vs_celda_rect(h->cur_f, h->cur_c, &ax, &ay, &aan, &aal) && (sel.f0 != sel.f1 || sel.c0 != sel.c1))
			marco(l, ax, ay, aan, aal, 1, 0x1B3A6B);
		/* el cuadradito de rellenar */
		if (!C.editando && vs_celda_rect(sel.f1, sel.c1, &ax, &ay, &aan, &aal)) {
			lz_rect(l, ax + aan - 4, ay + aal - 4, 8, 8, 0xFFFFFF);
			lz_rect(l, ax + aan - 3, ay + aal - 3, 6, 6, 0x1B3A6B);
		}
		lz_sin_recorte(l);
	}
	/* lo que se rellena arrastrando, o adonde se mueve */
	if ((C.arrastra == AR_RELLENO || C.arrastra == AR_MOVER) && rect_rango(C.arr_r, &x, &y, &an, &al)) {
		lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
		marco_trazos(l, x - 1, y - 1, an + 2, al + 2, 0x101820, 0);
		lz_sin_recorte(l);
	}
	/* lo cortado: un marco de trazos */
	{
		rango r;
		u16 hid;

		if (op_recorte_cortado(&r, &hid) && hid == h->id && rect_rango(r, &x, &y, &an, &al)) {
			lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
			marco_trazos(l, x - 1, y - 1, an + 2, al + 2, 0x101820, (int)(so_tics() / 4));
			lz_sin_recorte(l);
		}
	}
	/* lo señalado al escribir una fórmula: trazos de color y un borde doble */
	if (C.editando && C.senala) {
		rango r = cu_rango(C.sen_f0, C.sen_c0, C.sen_f, C.sen_c);

		if (rect_rango(r, &x, &y, &an, &al)) {
			lz_recorte(l, C.g.cx, C.g.cy, C.g.can, C.g.cal);
			marco_trazos(l, x - 1, y - 1, an + 2, al + 2, 0xB03A2E, 0);
			marco(l, x + 2, y + 2, an - 4, al - 4, 1, 0xB03A2E);
			lz_sin_recorte(l);
		}
	}
	/* las rayas de lo inmovilizado */
	if (h->inm_c)
		lz_rect(l, C.g.fx - 1, C.g.cy, 2, C.g.cal, 0x26313F);
	if (h->inm_f)
		lz_rect(l, C.g.cx, C.g.fy - 1, C.g.can, 2, 0x26313F);
	/* la celda que se corrige */
	pinta_editor(l);
	/* los rótulos */
	{
		int todo_col = sel.f0 == 0 && sel.f1 == CU_FILAS - 1, todo_fila = sel.c0 == 0 && sel.c1 == CU_COLS - 1;
		char t[16];

		lz_recorte(l, C.g.cx, C.g.y, C.g.can, C.g.rot_al);
		for (k = 0; k < CU_COLS; k++) {
			int cx0;

			if (k >= h->inm_c && k < h->vista_c)
				continue;
			if (!vs_col_x(k, &cx0)) {
				if (k >= h->vista_c)
					break;
				continue;
			}
			cu_col_texto(k, t);
			if (k < h->inm_c)
				lz_recorte(l, C.g.cx, C.g.y, C.g.fx - C.g.cx, C.g.rot_al);
			else
				lz_recorte(l, C.g.fx, C.g.y, x1 - C.g.fx, C.g.rot_al);
			rotulo(l, cx0, C.g.y, vs_ancho_px(k), C.g.rot_al, t, k >= sel.c0 && k <= sel.c1, todo_col && k >= sel.c0 && k <= sel.c1);
		}
		lz_sin_recorte(l);
		for (k = 0; k < CU_FILAS; k++) {
			int fy0;

			if (k >= h->inm_f && k < h->vista_f) {
				k = h->vista_f - 1;
				continue;
			}
			if (!vs_fila_y(k, &fy0)) {
				if (k >= h->vista_f)
					break;
				continue;
			}
			fmt(t, sizeof t, "%u", k + 1);
			if (k < h->inm_f)
				lz_recorte(l, C.g.x, C.g.cy, C.g.rot_an, C.g.fy - C.g.cy);
			else
				lz_recorte(l, C.g.x, C.g.fy, C.g.rot_an, y1 - C.g.fy);
			rotulo(l, C.g.x, fy0, C.g.rot_an, vs_alto_px(k), t, k >= sel.f0 && k <= sel.f1, todo_fila && k >= sel.f0 && k <= sel.f1);
		}
		lz_sin_recorte(l);
		/* la esquina: elegirlo todo */
		rotulo(l, C.g.x, C.g.y, C.g.rot_an, C.g.rot_al, "", 0, 0);
		{
			int xy[6] = { (C.g.x + C.g.rot_an - 4) * 16, (C.g.y + 4) * 16, (C.g.x + C.g.rot_an - 4) * 16,
				      (C.g.y + C.g.rot_al - 4) * 16, (C.g.x + C.g.rot_an - C.g.rot_al + 4) * 16, (C.g.y + C.g.rot_al - 4) * 16 };

			lz_poligono(l, xy, 3, 0x8A94A3);
		}
	}
	/* el comentario de la celda que tiene el ratón encima */
	{
		u32 f, c;
		int i;

		if (!C.editando && !C.arrastra && vs_en(C.raton_x, C.raton_y, &f, &c, &i) == VS_CELDA) {
			comentario *cm = hj_comentario(h, f, c);

			if (cm && cm->texto && vs_celda_rect(f, c, &x, &y, &an, &al)) {
				/* un recuadro amarillo con su borde y el autor en negrita */
				int lw = 0, lineas = 0, bx = x + an + 8, by = y;
				const char *s = cm->texto;
				char linea[200];

				while (*s) {
					u32 j = 0;

					while (*s && *s != '\n' && j + 1 < sizeof linea)
						linea[j++] = *s++;
					linea[j] = 0;
					if (*s == '\n')
						s++;
					if (lz_ui_ancho(linea) > lw)
						lw = lz_ui_ancho(linea);
					lineas++;
				}
				if (cm->autor[0] && lz_ui_ancho_negrita(cm->autor) + 4 > lw)
					lw = lz_ui_ancho_negrita(cm->autor) + 4;
				lw += 14;
				an = lw;
				al = (lineas + (cm->autor[0] ? 1 : 0)) * lz_ui_alto() + 10;
				if (bx + an > C.g.x + C.g.an)
					bx = x - an - 8;
				lz_rect(l, bx, by, an, al, 0xFFF4C2);
				marco(l, bx, by, an, al, 1, 0x5A4A10);
				lz_linea(l, (x + an > bx ? x : x + an) * 16 + (bx > x ? an * 16 : 0), (y + 2) * 16, bx * 16, (by + 6) * 16, 16, 0x5A4A10);
				{
					int yy = by + 5 + lz_ui_base();

					if (cm->autor[0]) {
						lz_ui_negrita(l, bx + 7, yy, cm->autor, 0x101820);
						yy += lz_ui_alto();
					}
					s = cm->texto;
					while (*s) {
						u32 j = 0;

						while (*s && *s != '\n' && j + 1 < sizeof linea)
							linea[j++] = *s++;
						linea[j] = 0;
						if (*s == '\n')
							s++;
						lz_ui(l, bx + 7, yy, linea, 0x101820);
						yy += lz_ui_alto();
					}
				}
			}
		}
	}
}
