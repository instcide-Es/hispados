/* vista.c - Pluma 2: lo que se ve del documento (pluma/vista.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "vista.h"
#include "maqueta.h"
#include "pinta_pag.h"
#include "letras.h"
#include "texto.h"
#include "ui.h"

#define FONDO_VISTA 0x9AA6B6
#define COLOR_ELEGIDO 0xB5D3F7
#define NOPAR 0xFFFFFFFFu

void vista_inicia(vista *v, documento *d, editor *e)
{
	memset(v, 0, sizeof *v);
	v->d = d;
	v->e = e;
	v->zoom = 100;
	v->x_deseada = -1;
	v->sigue = 1;
	v->foco = 1;
	v->con_faltas = 1;
}

int vista_px(const vista *v, i32 tw)
{
	return (int)(((i64)tw * v->zoom + (tw >= 0 ? 750 : -750)) / 1500);
}

i32 vista_tw(const vista *v, int px)
{
	return (i32)((i64)px * 1500 / v->zoom);
}

int vista_alto_pag(const vista *v)
{
	return vista_px(v, v->d->pag_al);
}

int vista_ancho_pag(const vista *v)
{
	return vista_px(v, v->d->pag_an);
}

/* (vista previa: cuántas páginas caben a lo ancho) */
static int columnas(const vista *v)
{
	int c;

	if (!v->previa)
		return 1;
	c = (v->an - VISTA_HUECO) / (vista_ancho_pag(v) + VISTA_HUECO);
	return c < 1 ? 1 : c;
}

void vista_sitio_pag(const vista *v, u32 k, int *x, int *y)
{
	int c = columnas(v), w = vista_ancho_pag(v), h = vista_alto_pag(v);
	int total = c * w + (c - 1) * VISTA_HUECO;
	int x0 = (v->an - total) / 2;

	if (x0 < VISTA_HUECO)
		x0 = VISTA_HUECO;
	*x = x0 + (int)(k % (u32)c) * (w + VISTA_HUECO) - v->dx;
	*y = VISTA_HUECO + (int)(k / (u32)c) * (h + VISTA_HUECO) - v->dy;
}

i32 vista_alto_total(const vista *v)
{
	u32 c = (u32)columnas(v), filas = (v->d->n_pag + c - 1) / c;

	return (i32)(filas * (u32)(vista_alto_pag(v) + VISTA_HUECO)) + VISTA_HUECO;
}

i32 vista_ancho_total(const vista *v)
{
	int c = columnas(v);

	return c * (vista_ancho_pag(v) + VISTA_HUECO) + VISTA_HUECO;
}

/* ---- el pintor de la pantalla ---- */
typedef struct {
	vista *v;
	lienzo *l;
	int ox, oy;		/* la esquina del papel, en el lienzo */
	pos a, b;		/* lo elegido */
	int hay;
	u32 pag;
	int img_elegida;
} ctx;

static int q4(const ctx *c, i32 tw)
{
	return c->ox * 4 + (int)(((i64)tw * c->v->zoom * 4 + 750) / 1500);
}

static int px_x(const ctx *c, i32 tw)
{
	return c->ox + vista_px(c->v, tw);
}

static int px_y(const ctx *c, i32 tw)
{
	return c->oy + vista_px(c->v, tw);
}

static int elegido(const ctx *c, u32 par, u32 o)
{
	if (!c->hay || par == NOPAR)
		return 0;
	if (par < c->a.p || par > c->b.p)
		return 0;
	if (par == c->a.p && o < c->a.o)
		return 0;
	if (par == c->b.p && o >= c->b.o)
		return 0;
	return 1;
}

static void p_texto(void *u, const u32 *t, u32 n, const i32 *xs, i32 y, const fmt_letra *f, const info_fmt *in, u32 par, u32 o0)
{
	ctx *c = u;
	int tam = (int)((i64)in->tam * c->v->zoom * 2 / 75), base = px_y(c, y), k;
	u32 col = f->color == SIN_COLOR ? 0x000000 : f->color;
	int x4[514], fin4;
	const u8 *faltas = NULL;

	if (n > 512)
		n = 512;
	for (k = 0; k < (int)n; k++)
		x4[k] = q4(c, xs[k]);
	/* el final de la última letra (lo que avanza) */
	fin4 = x4[n - 1] + lz_letra(NULL, in->cara, tam, in->falsas, x4[n - 1], 0, t[n - 1], 0);
	x4[n] = fin4;
	/* lo elegido, detrás */
	for (k = 0; k < (int)n; k++)
		if (elegido(c, par, o0 + (u32)k)) {
			int j = k;

			while (j + 1 < (int)n && elegido(c, par, o0 + (u32)j + 1))
				j++;
			lz_rect(c->l, x4[k] / 4, base - vista_px(c->v, in->asc + in->sube), (x4[j + 1] - x4[k] + 3) / 4,
				vista_px(c->v, in->asc + in->desc) + 1, COLOR_ELEGIDO);
			k = j;
		}
	if (tam >= 4)
		for (k = 0; k < (int)n; k++) {
			u32 ch = t[k];

			if (c->v->marcas && ch == ' ') {
				/* el espacio, con un punto en medio */
				int cx = (x4[k] + x4[k + 1]) / 8, cy = base - vista_px(c->v, in->asc) * 4 / 10;

				lz_rect(c->l, cx, cy, tam >= 48 ? 2 : 1, tam >= 48 ? 2 : 1, 0x5B7FBF);
				continue;
			}
			lz_letra(c->l, in->cara, tam, in->falsas, x4[k], base, ch, col);
		}
	/* las faltas de ortografía: un trazo ondulado (rojo, pero se ve por la forma) */
	if (par != NOPAR && par < c->v->d->n_par && c->v->con_faltas) {
		const parrafo *p = &c->v->d->par[par];

		faltas = p->faltas && p->faltas_version == p->version ? p->faltas : NULL;
	}
	if (faltas)
		for (k = 0; k < (int)n; k++)
			if (o0 + (u32)k < c->v->d->par[par].n && faltas[o0 + (u32)k]) {
				int j = k;

				while (j + 1 < (int)n && o0 + (u32)j + 1 < c->v->d->par[par].n && faltas[o0 + (u32)j + 1])
					j++;
				lz_ondulada(c->l, x4[k] / 4, base + 2, (x4[j + 1] - x4[k]) / 4, 0xD0201A);
				k = j;
			}
}

static void p_rect(void *u, i32 x, i32 y, i32 an, i32 al, u32 color)
{
	ctx *c = u;
	int x0 = px_x(c, x), y0 = px_y(c, y);

	lz_rect(c->l, x0, y0, px_x(c, x + an) - x0, px_y(c, y + al) - y0, color);
}

static void p_raya(void *u, i32 x0, i32 y0, i32 x1, i32 y1, i32 g, u32 color)
{
	ctx *c = u;
	int gp = vista_px(c->v, g);

	if (gp < 1)
		gp = 1;
	if (y0 == y1)
		lz_rect(c->l, px_x(c, x0), px_y(c, y0) - gp / 2, px_x(c, x1) - px_x(c, x0) + 1, gp, color);
	else
		lz_rect(c->l, px_x(c, x0) - gp / 2, px_y(c, y0), gp, px_y(c, y1) - px_y(c, y0) + 1, color);
}

static void p_imagen(void *u, documento *d, int img, i32 x, i32 y, i32 an, i32 al)
{
	ctx *c = u;
	int x0 = px_x(c, x), y0 = px_y(c, y), w = px_x(c, x + an) - x0, h = px_y(c, y + al) - y0;

	if (w <= 0 || h <= 0)
		return;
	if (doc_imagen_px(d, img))
		lz_imagen(c->l, d->img[img].px, d->img[img].px_an, d->img[img].px_al, x0, y0, w, h, 1);
	else {
		lz_rect(c->l, x0, y0, w, h, 0xE8ECF2);
		lz_marco(c->l, x0, y0, w, h, 0x8794A6);
		lz_ui(c->l, x0 + 4, y0 + lz_ui_base() + 2, "(imagen)", 0x5A6577);
	}
	if (img == c->img_elegida) {
		/* elegida: el marco y los tiradores (y el de abajo a la derecha, para el tamaño) */
		int k;

		lz_marco_grueso(c->l, x0 - 2, y0 - 2, w + 4, h + 4, 2, UI_ACENTO);
		for (k = 0; k < 4; k++) {
			int hx = k & 1 ? x0 + w - 4 : x0 - 4, hy = k & 2 ? y0 + h - 4 : y0 - 4;

			lz_rect(c->l, hx, hy, 8, 8, UI_BLANCO);
			lz_marco(c->l, hx, hy, 8, 8, UI_ACENTO);
		}
	}
}

static void p_linea(void *u, u32 par, u32 lin, i32 x0, i32 y, i32 x1, i32 alto)
{
	ctx *c = u;
	const parrafo *p;

	(void)x0;
	if (!c->v->marcas || par == NOPAR || par >= c->v->d->n_par)
		return;
	p = &c->v->d->par[par];
	/* el ¶ detrás de la última línea (y ↵ detrás de un salto de línea) */
	if (lin + 1 == p->n_lin || (p->lin[lin].fin > p->lin[lin].ini && p->t[p->lin[lin].fin - 1] == SALTO_LINEA)) {
		const info_fmt *in = maq_info(c->v->d, par_fmt_en(c->v->d, p, p->n));
		int tam = (int)((i64)in->tam * c->v->zoom * 2 / 75);
		i32 xe = p->lin[lin].fin > p->lin[lin].ini ? maq_x_de(c->v->d, p, &p->lin[lin], p->lin[lin].fin) : p->lin[lin].x0;
		u32 m = lin + 1 == p->n_lin ? 0xB6 : 0x21B5;
		i32 base = y + p->lin[lin].base;

		(void)x1;
		(void)alto;
		/* (xe es desde el borde del texto de su párrafo: x0 lo trae ya; se pinta en x1) */
		xe = x1 - (p->lin[lin].x0 + p->lin[lin].ancho) + xe;
		lz_letra(c->l, in->cara, tam, 0, q4(c, xe), px_y(c, base), m, 0x5B7FBF);
	}
}

static int p_visible(void *u, i32 y, i32 al)
{
	ctx *c = u;
	int y0 = px_y(c, y), y1 = px_y(c, y + al);

	return y1 >= c->l->ry0 && y0 <= c->l->ry1;
}

static void pinta_hoja(vista *v, lienzo *l, u32 k, int x, int y)
{
	int w = vista_ancho_pag(v), h = vista_alto_pag(v);
	ctx c;
	pintor pt;
	documento *d = v->d;

	/* la sombra y el papel */
	lz_rect_a(l, x + 3, y + 3, w, h, 0x000000, 70);
	lz_rect(l, x, y, w, h, 0xFFFFFF);
	lz_marco(l, x - 1, y - 1, w + 2, h + 2, 0x6F7C8F);
	if (!v->previa) {
		/* las esquinas de los márgenes (como los procesadores de siempre) */
		int mx0 = x + vista_px(v, d->marg_izq), my0 = y + vista_px(v, d->marg_sup);
		int mx1 = x + vista_px(v, d->pag_an - d->marg_der), my1 = y + vista_px(v, d->pag_al - d->marg_inf), m = 10;

		lz_raya_h(l, mx0 - m, my0, m, 0xB8C2CF);
		lz_raya_v(l, mx0, my0 - m, m, 0xB8C2CF);
		lz_raya_h(l, mx1, my0, m, 0xB8C2CF);
		lz_raya_v(l, mx1, my0 - m, m, 0xB8C2CF);
		lz_raya_h(l, mx0 - m, my1, m, 0xB8C2CF);
		lz_raya_v(l, mx0, my1, m, 0xB8C2CF);
		lz_raya_h(l, mx1, my1, m, 0xB8C2CF);
		lz_raya_v(l, mx1, my1, m, 0xB8C2CF);
	}
	memset(&c, 0, sizeof c);
	c.v = v;
	c.l = l;
	c.ox = x;
	c.oy = y;
	c.pag = k;
	c.hay = !v->previa && ed_eleccion(v->e, &c.a, &c.b);
	c.img_elegida = v->previa ? -1 : ed_imagen_elegida(v->e);
	if (c.img_elegida >= 0)
		c.hay = 0;
	pt.u = &c;
	pt.texto = p_texto;
	pt.rect = p_rect;
	pt.raya = p_raya;
	pt.imagen = p_imagen;
	pt.linea = p_linea;
	pt.visible = p_visible;
	{
		int rx0 = l->rx0, ry0 = l->ry0, rx1 = l->rx1, ry1 = l->ry1;

		lz_recorte(l, x > rx0 ? x : rx0, y > ry0 ? y : ry0, (x + w < rx1 ? x + w : rx1) - (x > rx0 ? x : rx0),
			   (y + h < ry1 ? y + h : ry1) - (y > ry0 ? y : ry0));
		pinta_pagina(d, k, &pt);
		l->rx0 = rx0;
		l->ry0 = ry0;
		l->rx1 = rx1;
		l->ry1 = ry1;
	}
	if (v->previa) {
		char t[32];

		fmt(t, sizeof t, "Página %u", k + d->num_inicio);
		lz_ui(l, x, y + h + lz_ui_base() - 2, t, 0x1B2633);
	}
}

/* las faltas de los párrafos que se ven */
static void repasa_faltas(vista *v, u32 k)
{
	const pagina *pg;
	u32 t;

	if (!v->faltas || !v->con_faltas || k >= v->d->n_pag)
		return;
	pg = &v->d->pag[k];
	for (t = pg->t0; t < pg->t0 + pg->n_t && t < v->d->n_trz; t++) {
		u32 i = v->d->trz[t].par, fin = v->d->trz[t].fila_fin ? v->d->trz[t].fila_fin : i + 1;

		for (; i < fin && i < v->d->n_par; i++) {
			parrafo *p = &v->d->par[i];

			if (!p->faltas || p->faltas_version != p->version)
				v->faltas(v->d, p);
		}
	}
}

void vista_pinta(vista *v, lienzo *l)
{
	u32 k;
	int x, y, h;

	maq_documento(v->d);
	lz_recorte(l, v->x, v->y, v->an, v->al);
	lz_rect(l, v->x, v->y, v->an, v->al, FONDO_VISTA);
	h = vista_alto_pag(v);
	for (k = 0; k < v->d->n_pag; k++) {
		vista_sitio_pag(v, k, &x, &y);
		x += v->x;
		y += v->y;
		if (y > v->y + v->al)
			break;
		if (y + h < v->y)
			continue;
		repasa_faltas(v, k);
		pinta_hoja(v, l, k, x, y);
	}
	/* el cursor */
	if (!v->previa) {
		pos a, b;
		int cx, cy, ch;

		if (!ed_eleccion(v->e, &a, &b) && vista_cursor_xy(v, v->e->cur, &cx, &cy, &ch)) {
			int g = v->foco ? 2 : 1;

			lz_rect(l, cx, cy, g, ch, v->foco ? 0x000000 : 0x7A8598);
		}
	}
	lz_sin_recorte(l);
}

/* ---- dónde está cada cosa ---- */
/* la x (en twips, desde el borde del papel) del borde del texto del párrafo i en su trozo */
static i32 borde_x(documento *d, u32 i)
{
	const parrafo *p = &d->par[i];

	if (p->p.tabla && p->p.tabla <= d->n_tab)
		return d->marg_izq + maq_x_col(d, p->p.tabla - 1, p->p.col) + d->tab[p->p.tabla - 1].margen;
	return d->marg_izq;
}

int vista_cursor_xy(vista *v, pos c, int *x, int *y, int *alto)
{
	documento *d = v->d;
	const parrafo *p;
	u32 l, pg, t;
	i32 yp, xp;
	int px, py;

	if (c.p >= d->n_par)
		return 0;
	p = &d->par[c.p];
	if (!p->n_lin)
		return 0;
	l = maq_linea_de(p, c.o, 0);
	if (maq_donde(d, c.p, l, &pg, &t) < 0)
		return 0;
	yp = d->marg_sup + maq_y_parrafo(d, &d->trz[t], c.p) + p->lin[l].y;
	xp = borde_x(d, c.p) + maq_x_de(d, p, &p->lin[l], c.o);
	vista_sitio_pag(v, pg, &px, &py);
	*x = v->x + px + vista_px(v, xp);
	*y = v->y + py + vista_px(v, yp);
	*alto = vista_px(v, p->lin[l].alto);
	if (*alto < 4)
		*alto = 4;
	return 1;
}

int vista_al_cursor(vista *v)
{
	int x, y, h, mov = 0;

	maq_documento(v->d);
	if (!vista_cursor_xy(v, v->e->cur, &x, &y, &h))
		return 0;
	if (y < v->y + 8) {
		v->dy -= v->y + 8 - y + v->al / 4;
		mov = 1;
	} else if (y + h > v->y + v->al - 8) {
		v->dy += y + h - (v->y + v->al - 8) + v->al / 4;
		mov = 1;
	}
	if (v->dy > vista_alto_total(v) - v->al)
		v->dy = vista_alto_total(v) - v->al;
	if (v->dy < 0)
		v->dy = 0;
	if (x < v->x + 4) {
		v->dx -= v->x + 4 - x + 40;
		mov = 1;
	} else if (x > v->x + v->an - 4) {
		v->dx += x - (v->x + v->an - 4) + 40;
		mov = 1;
	}
	if (v->dx > vista_ancho_total(v) - v->an)
		v->dx = vista_ancho_total(v) - v->an;
	if (v->dx < 0)
		v->dx = 0;
	return mov;
}

/* la posición en el párrafo i (con su borde en bx, arriba en y0) de la línea más cerca de ty */
static pos en_parrafo(documento *d, u32 i, u32 l0, u32 l1, i32 y0, i32 tx, i32 ty, i32 bx)
{
	const parrafo *p = &d->par[i];
	u32 l;
	pos r;

	r.p = i;
	r.o = 0;
	if (!p->n_lin)
		return r;
	if (l1 > p->n_lin)
		l1 = p->n_lin;
	for (l = l0; l + 1 < l1; l++)
		if (ty < y0 + p->lin[l].y + p->lin[l].alto)
			break;
	r.o = maq_o_de(d, p, &p->lin[l], tx - bx);
	return r;
}

int vista_pos_en(vista *v, int x, int y, pos *r)
{
	documento *d = v->d;
	u32 k, t, mejor_k = 0;
	int px, py, h = vista_alto_pag(v), w = vista_ancho_pag(v), dist = 1 << 30;
	i32 tx, ty;
	const pagina *pg;

	maq_documento(d);
	/* la página: la que tiene el punto, o la más cerca */
	for (k = 0; k < d->n_pag; k++) {
		int dd;

		vista_sitio_pag(v, k, &px, &py);
		px += v->x;
		py += v->y;
		dd = (y < py ? py - y : y > py + h ? y - py - h : 0) + (x < px ? px - x : x > px + w ? x - px - w : 0);
		if (dd < dist) {
			dist = dd;
			mejor_k = k;
		}
		if (py > y + h)
			break;
	}
	if (!d->n_pag)
		return 0;
	k = mejor_k;
	vista_sitio_pag(v, k, &px, &py);
	tx = vista_tw(v, x - v->x - px);
	ty = vista_tw(v, y - v->y - py) - d->marg_sup;
	pg = &d->pag[k];
	if (!pg->n_t)
		return 0;
	/* el trozo */
	for (t = pg->t0; t + 1 < pg->t0 + pg->n_t; t++) {
		const trozo *z = &d->trz[t];
		i32 fin = z->fila_fin ? z->y + z->alto_fila : z->y + (z->lin1 > z->lin0 ? d->par[z->par].lin[z->lin1 - 1].y + d->par[z->par].lin[z->lin1 - 1].alto - d->par[z->par].lin[z->lin0].y : 0);
		const trozo *s = &d->trz[t + 1];

		if (ty < fin || ty < s->y)
			break;
	}
	{
		const trozo *z = &d->trz[t];

		if (z->fila_fin) {
			/* una fila: la celda por la x */
			u32 i, mejor = z->par;
			i32 x0 = tx - d->marg_izq;

			for (i = z->par; i < z->fila_fin; i++) {
				const parrafo *p = &d->par[i];
				i32 cx;

				if (p->p.cubierta)
					continue;
				cx = maq_x_col(d, p->p.tabla - 1, p->p.col);
				if (x0 >= cx)
					mejor = i;
			}
			*r = en_parrafo(d, mejor, 0, d->par[mejor].n_lin, maq_y_parrafo(d, z, mejor), tx, ty, borde_x(d, mejor));
			return 1;
		}
		*r = en_parrafo(d, z->par, z->lin0, z->lin1, maq_y_parrafo(d, z, z->par), tx, ty, borde_x(d, z->par));
	}
	return 1;
}

pos vista_arriba(vista *v, pos c, int lineas)
{
	documento *d = v->d;
	int x, y, h;
	pos r = c;

	maq_documento(d);
	if (!vista_cursor_xy(v, c, &x, &y, &h))
		return c;
	if (v->x_deseada >= 0)
		x = v->x_deseada;
	else
		v->x_deseada = x;
	/* una línea más arriba (o más abajo): por la pantalla */
	{
		int paso = lineas < 0 ? -1 : 1, n = lineas < 0 ? -lineas : lineas, i;

		for (i = 0; i < n; i++) {
			pos q;
			int y2 = paso < 0 ? y - 3 : y + h + 3, x2, yy, hh, k;

			/* (el hueco entre páginas o entre párrafos: se salta) */
			for (k = 0; k < 400; k += 3) {
				if (!vista_pos_en(v, x, paso < 0 ? y2 - k : y2 + k, &q))
					break;
				if (q.p != r.p || q.o != r.o) {
					vista_cursor_xy(v, q, &x2, &yy, &hh);
					if ((paso < 0 && yy < y) || (paso > 0 && yy > y))
						break;
				}
			}
			if (k >= 400 || (q.p == r.p && q.o == r.o))
				break;
			r = q;
			vista_cursor_xy(v, r, &x2, &y, &h);
		}
	}
	return r;
}

pos vista_inicio_linea(vista *v, pos c)
{
	const parrafo *p = &v->d->par[c.p];
	u32 l;

	maq_documento(v->d);
	if (!p->n_lin)
		return c;
	l = maq_linea_de(p, c.o, 0);
	c.o = p->lin[l].ini;
	return c;
}

pos vista_fin_linea(vista *v, pos c)
{
	const parrafo *p = &v->d->par[c.p];
	u32 l, f;

	maq_documento(v->d);
	if (!p->n_lin)
		return c;
	l = maq_linea_de(p, c.o, 0);
	f = p->lin[l].fin;
	if (!p->lin[l].ultima && f > p->lin[l].ini && (p->t[f - 1] == ' ' || p->t[f - 1] == SALTO_LINEA))
		f--;
	c.o = f;
	return c;
}

u32 vista_pagina_cursor(vista *v)
{
	u32 pg = 0, t;
	const parrafo *p = &v->d->par[v->e->cur.p];

	maq_documento(v->d);
	if (maq_donde(v->d, v->e->cur.p, p->n_lin ? maq_linea_de(p, v->e->cur.o, 0) : 0, &pg, &t) < 0)
		return 0;
	return pg;
}

void vista_zoom(vista *v, int zoom)
{
	int x, y, h, y2;

	if (zoom < 25)
		zoom = 25;
	if (zoom > 400)
		zoom = 400;
	if (zoom == v->zoom)
		return;
	/* (el cursor se queda en el mismo sitio de la pantalla) */
	if (!vista_cursor_xy(v, v->e->cur, &x, &y, &h))
		y = v->y + v->al / 2;
	v->zoom = zoom;
	letras_olvida();
	if (vista_cursor_xy(v, v->e->cur, &x, &y2, &h)) {
		v->dy += y2 - y;
		if (v->dy > vista_alto_total(v) - v->al)
			v->dy = vista_alto_total(v) - v->al;
		if (v->dy < 0)
			v->dy = 0;
	}
	if (v->dx > vista_ancho_total(v) - v->an)
		v->dx = vista_ancho_total(v) - v->an;
	if (v->dx < 0)
		v->dx = 0;
}

int vista_zoom_ancho(vista *v)
{
	return (int)((i64)(v->an - 2 * VISTA_HUECO - 8) * 1500 / v->d->pag_an);
}
