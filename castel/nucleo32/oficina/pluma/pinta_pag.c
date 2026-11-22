/* pinta_pag.c - Pluma 2: el recorrido de una página (pluma/pinta_pag.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pinta_pag.h"
#include "letras.h"
#include "texto.h"

#define MAX_TROZO 512

void pinta_campo(const documento *d, u32 campo, u32 pag, u32 *t, int cabe)
{
	char b[16];
	int k;

	fmt(b, sizeof b, "%u", campo == CAMPO_PAGINAS ? d->n_pag + d->num_inicio - 1 : pag + d->num_inicio);
	for (k = 0; b[k] && k + 1 < cabe; k++)
		t[k] = (u8)b[k];
	t[k] = 0;
}

static int es_esp(u32 c)
{
	return c == ' ' || c == 0x3000;
}

/* lo que mide la letra c con el formato in (en twips; sin tabulaciones) */
static i32 avance_letra(const info_fmt *in, u32 c)
{
	i32 tam = in->tam;
	u32 u = c;

	if (in->mayus && minuscula(c) == c && mayuscula(c) != c) {
		u = mayuscula(c);
		tam = tam * 80 / 100;
	}
	return (i32)(((i64)letra_milesimas(in->cara, u) * tam + 50) / 100) + ((in->falsas & FALSA_NEGRITA) ? tam / 4 : 0);
}

/* da el texto t (n letras, xs) con sus versalitas */
static void da_texto(const pintor *pt, const u32 *t, u32 n, const i32 *xs, i32 y, const fmt_letra *f, const info_fmt *in, u32 par, u32 o0)
{
	u32 i, j;

	if (!n)
		return;
	if (!in->mayus) {
		pt->texto(pt->u, t, n, xs, y - in->sube, f, in, par, o0);
		return;
	}
	/* las minúsculas, en mayúsculas pequeñas (con su formato aparte) */
	for (i = 0; i < n; i = j) {
		int peq = minuscula(t[i]) == t[i] && mayuscula(t[i]) != t[i];
		u32 tmp[MAX_TROZO];
		info_fmt in2 = *in;

		for (j = i; j < n && (minuscula(t[j]) == t[j] && mayuscula(t[j]) != t[j]) == peq; j++)
			tmp[j - i] = peq ? mayuscula(t[j]) : t[j];
		in2.mayus = 0;
		if (peq)
			in2.tam = in->tam * 80 / 100;
		pt->texto(pt->u, tmp, j - i, xs + i, y - in->sube, f, &in2, par, o0 + i);
	}
}

/* una línea del párrafo p (el número «par»; o 0xFFFFFFFF), con el borde
 * del texto en x0 y la línea empezando en y */
static void pinta_linea(documento *d, const parrafo *p, u32 par, u32 nl, i32 x0, i32 y, u32 pag, const pintor *pt)
{
	const linea *l = &p->lin[nl];
	u32 k, n = l->fin - l->ini, i0;
	i32 xs[MAX_TROZO + 1], acum = 0, extra = 0, base = y + l->base;
	u32 tt[MAX_TROZO];

	if (pt->linea)
		pt->linea(pt->u, par, nl, x0 + l->x0, y, x0 + l->x0 + l->ancho, l->alto);
	/* la viñeta o el número, en la primera línea */
	if (nl == 0 && p->p.lista && par != 0xFFFFFFFFu && par < d->n_par) {
		u32 m[16], j;
		i32 xm = p->p.sang_izq + (p->p.sang_primera < 0 ? p->p.sang_primera : p->p.sang_primera), xx[16];
		const info_fmt *in = maq_info(d, par_fmt_en(d, p, 0));
		u16 fi = par_fmt_en(d, p, 0);

		maq_marca_lista(d, par, m, 16);
		for (j = 0; m[j]; j++) {
			xx[j] = x0 + xm;
			xm += avance_letra(in, m[j]);
		}
		if (j) {
			fmt_letra f = d->fl[fi];

			f.subrayado = f.tachado = 0;
			f.resalte = SIN_COLOR;
			pt->texto(pt->u, m, j, xx, base, &f, in, 0xFFFFFFFFu, 0);
		}
	}
	/* las imágenes que flotan (en la primera línea del párrafo) */
	if (nl == 0) {
		for (k = 0; k < p->n; k++)
			if (es_imagen(p->t[k]) && p->t[k] - IMAGEN_0 < d->n_img) {
				const imagen *m = &d->img[p->t[k] - IMAGEN_0];
				i32 ancho_area = p->ancho_maq > 0 ? p->ancho_maq : doc_ancho_texto(d);

				if (m->ajuste == AJUSTE_IZQ)
					pt->imagen(pt->u, d, (int)(p->t[k] - IMAGEN_0), x0 + p->p.sang_izq, y, m->ancho, m->alto);
				else if (m->ajuste == AJUSTE_DER)
					pt->imagen(pt->u, d, (int)(p->t[k] - IMAGEN_0), x0 + ancho_area - p->p.sang_der - m->ancho, y, m->ancho, m->alto);
			}
	}
	if (!n)
		return;
	/* la x de cada letra */
	for (k = 0; k < n && k < MAX_TROZO; k++) {
		u32 c = p->t[l->ini + k];

		xs[k] = x0 + l->x0 + acum + extra / 1000;
		acum += maq_avance(d, p, l->ini + k, l->izq + acum);
		if (l->extra && es_esp(c))
			extra += l->extra;
	}
	xs[k] = x0 + l->x0 + acum + extra / 1000;
	if (n > MAX_TROZO)
		n = MAX_TROZO;
	/* 1. el resaltado */
	for (k = 0; k < n; k++) {
		const fmt_letra *f = &d->fl[p->f[l->ini + k]];

		if (f->resalte != SIN_COLOR && !es_objeto(p->t[l->ini + k])) {
			u32 j = k;
			const info_fmt *in = maq_info(d, p->f[l->ini + k]);

			while (j + 1 < n && d->fl[p->f[l->ini + j + 1]].resalte == f->resalte && !es_objeto(p->t[l->ini + j + 1]))
				j++;
			pt->rect(pt->u, xs[k], base - in->asc, xs[j + 1] - xs[k], in->asc + in->desc, f->resalte);
			k = j;
		}
	}
	/* 2. el texto, por trozos del mismo formato */
	for (k = 0; k < n;) {
		u32 c = p->t[l->ini + k];
		u16 fi = p->f[l->ini + k];
		const info_fmt *in = maq_info(d, fi);

		if (c == SALTO_LINEA || c == '\t' || es_imagen(c) || c == CAMPO_PAGINA || c == CAMPO_PAGINAS || es_nota(c)) {
			if (c == '\t') {
				/* el relleno de la tabulación (puntos...) */
				u32 j;
				i32 pos = xs[k + 1] - x0;

				for (j = 0; j < p->p.n_tabs; j++)
					if (p->p.tabs[j].relleno && p->p.tabs[j].pos >= pos - 400 && p->p.tabs[j].pos <= pos + 4000) {
						u32 r = p->p.tabs[j].relleno, m = 0;
						i32 w = avance_letra(in, r), x = xs[k] + w / 2;
						u32 rel[MAX_TROZO];
						i32 rx[MAX_TROZO];

						if (w <= 0)
							break;
						x = (x + w - 1) / w * w;
						while (x + w <= xs[k + 1] - w / 2 && m < MAX_TROZO) {
							rel[m] = r;
							rx[m++] = x;
							x += w;
						}
						if (m) {
							fmt_letra f = d->fl[fi];

							f.subrayado = 0;
							pt->texto(pt->u, rel, m, rx, base, &f, in, 0xFFFFFFFFu, 0);
						}
						break;
					}
			} else if (es_imagen(c) && c - IMAGEN_0 < d->n_img && d->img[c - IMAGEN_0].ajuste == AJUSTE_LINEA) {
				const imagen *m = &d->img[c - IMAGEN_0];

				pt->imagen(pt->u, d, (int)(c - IMAGEN_0), xs[k], base - m->alto, m->ancho, m->alto);
			} else if (c == CAMPO_PAGINA || c == CAMPO_PAGINAS) {
				u32 v[12], j;
				i32 vx[12], x = xs[k];

				pinta_campo(d, c, pag, v, 12);
				for (j = 0; v[j]; j++) {
					vx[j] = x;
					x += avance_letra(in, v[j]);
				}
				pt->texto(pt->u, v, j, vx, base - in->sube, &d->fl[fi], in, par, l->ini + k);
			} else if (es_nota(c)) {
				u32 v[12], j;
				i32 vx[12], x = xs[k];
				char b[12];
				info_fmt in2 = *in;

				fmt(b, sizeof b, "%u", c - NOTA_0 + 1);
				in2.tam = in->tam * 58 / 100;
				for (j = 0; b[j]; j++) {
					v[j] = (u8)b[j];
					vx[j] = x;
					x += avance_letra(&in2, v[j]);
				}
				pt->texto(pt->u, v, j, vx, base - in->tam * 10 * 33 / 100, &d->fl[fi], &in2, par, l->ini + k);
			}
			k++;
			continue;
		}
		i0 = k;
		while (k < n && p->f[l->ini + k] == fi) {
			u32 c2 = p->t[l->ini + k];

			if (c2 == SALTO_LINEA || c2 == '\t' || es_objeto(c2))
				break;
			tt[k - i0] = c2;
			k++;
		}
		da_texto(pt, tt, k - i0, xs + i0, base, &d->fl[fi], in, par, l->ini + i0);
	}
	/* 3. el subrayado y el tachado */
	for (k = 0; k < n; k++) {
		const fmt_letra *f = &d->fl[p->f[l->ini + k]];
		u32 j;
		const info_fmt *in;
		i32 g, x1;

		if (!f->subrayado && !f->tachado)
			continue;
		in = maq_info(d, p->f[l->ini + k]);
		j = k;
		while (j + 1 < n && p->f[l->ini + j + 1] == p->f[l->ini + k] && p->t[l->ini + j + 1] != SALTO_LINEA)
			j++;
		/* (sin los espacios del final de la línea) */
		x1 = xs[j + 1];
		{
			u32 q = j + 1;

			while (q > k && (es_esp(p->t[l->ini + q - 1]) || p->t[l->ini + q - 1] == SALTO_LINEA))
				q--;
			x1 = xs[q];
		}
		g = in->tam * 10 / 16;
		if (g < 10)
			g = 10;
		if (x1 > xs[k]) {
			u32 col = f->color == SIN_COLOR ? 0 : f->color;

			if (f->subrayado)
				pt->raya(pt->u, xs[k], base - in->sube + in->tam * 10 * 12 / 100, x1, base - in->sube + in->tam * 10 * 12 / 100, g, col);
			if (f->tachado)
				pt->raya(pt->u, xs[k], base - in->sube - in->tam * 10 * 28 / 100, x1, base - in->sube - in->tam * 10 * 28 / 100, g, col);
		}
		k = j;
	}
}

static void pinta_parrafo(documento *d, const parrafo *p, u32 par, u32 l0, u32 l1, i32 x0, i32 y, u32 pag, const pintor *pt)
{
	u32 l;
	i32 y0 = l0 < p->n_lin ? p->lin[l0].y : 0;

	for (l = l0; l < l1 && l < p->n_lin; l++) {
		i32 yl = y + p->lin[l].y - y0;

		if (pt->visible && !pt->visible(pt->u, yl, p->lin[l].alto))
			continue;
		pinta_linea(d, p, par, l, x0, yl, pag, pt);
	}
}

/* una fila de tabla */
static void pinta_fila(documento *d, const trozo *t, i32 ox, i32 oy, u32 pag, const pintor *pt)
{
	u32 i, tn = d->par[t->par].p.tabla;
	const tabla *tb;
	i32 y = oy + t->y, h = t->alto_fila, g;

	if (!tn || tn > d->n_tab)
		return;
	tb = &d->tab[tn - 1];
	if (pt->visible && !pt->visible(pt->u, y, h))
		return;
	g = tb->grosor ? tb->grosor : 10;
	for (i = t->par; i < t->fila_fin && i < d->n_par; i++) {
		const parrafo *p = &d->par[i];
		i32 x = ox + maq_x_col(d, tn - 1, p->p.col), w = maq_ancho_celda(d, p) + 2 * tb->margen, hc = h;
		u32 nf = p->p.abarca_filas ? p->p.abarca_filas : 1;
		int primera_fila = p->p.fila == 0, ultima_fila;

		if (p->p.cubierta)
			continue;
		/* (una celda unida hacia abajo: lo alto de sus filas) */
		if (nf > 1) {
			u32 k = t->fila_fin, f;

			for (f = 1; f < nf && k < d->n_par && d->par[k].p.tabla == tn; f++) {
				u32 fin;

				hc += maq_fila(d, k, &fin);
				k = fin;
			}
		}
		ultima_fila = p->p.fila + nf >= tb->filas;
		if (p->p.fondo != SIN_COLOR)
			pt->rect(pt->u, x, y, w, hc, p->p.fondo);
		else if (tb->cabecera && p->p.fila == 0)
			pt->rect(pt->u, x, y, w, hc, 0xE7ECF4);
		/* el texto (centrado arriba) */
		pinta_parrafo(d, p, i, 0, p->n_lin, x + tb->margen, y + tb->margen + p->p.antes, pag, pt);
		/* los bordes */
		switch (tb->bordes) {
		case BORDE_TODOS:
			pt->raya(pt->u, x, y, x + w, y, g, tb->color);
			pt->raya(pt->u, x, y + hc, x + w, y + hc, g, tb->color);
			pt->raya(pt->u, x, y, x, y + hc, g, tb->color);
			pt->raya(pt->u, x + w, y, x + w, y + hc, g, tb->color);
			break;
		case BORDE_FUERA:
		case BORDE_CABECERA:
			if (primera_fila)
				pt->raya(pt->u, x, y, x + w, y, g, tb->color);
			if (ultima_fila)
				pt->raya(pt->u, x, y + hc, x + w, y + hc, g, tb->color);
			if (tb->bordes == BORDE_CABECERA && primera_fila)
				pt->raya(pt->u, x, y + hc, x + w, y + hc, g, tb->color);
			if (tb->bordes == BORDE_FUERA) {
				if (p->p.col == 0)
					pt->raya(pt->u, x, y, x, y + hc, g, tb->color);
				if (p->p.col + (p->p.abarca_cols ? p->p.abarca_cols : 1) >= tb->cols)
					pt->raya(pt->u, x + w, y, x + w, y + hc, g, tb->color);
			}
			break;
		}
	}
}

void pinta_pagina(documento *d, u32 pag, const pintor *pt)
{
	const pagina *pg;
	u32 k;
	i32 ox = d->marg_izq, oy = d->marg_sup;

	if (pag >= d->n_pag)
		return;
	pg = &d->pag[pag];
	/* el encabezado y el pie */
	if (!(d->enc_no_primera && pag == 0)) {
		if (d->hay_enc && d->enc.n_lin)
			pinta_parrafo(d, &d->enc, 0xFFFFFFFFu, 0, d->enc.n_lin, ox, d->dist_enc, pag, pt);
		if (d->hay_pie && d->pie.n_lin)
			pinta_parrafo(d, &d->pie, 0xFFFFFFFFu, 0, d->pie.n_lin, ox, d->pag_al - d->dist_pie - d->pie.alto, pag, pt);
	}
	for (k = pg->t0; k < pg->t0 + pg->n_t && k < d->n_trz; k++) {
		const trozo *t = &d->trz[k];
		const parrafo *p;

		if (t->par >= d->n_par)
			continue;
		if (t->fila_fin) {
			pinta_fila(d, t, ox, oy, pag, pt);
			continue;
		}
		p = &d->par[t->par];
		pinta_parrafo(d, p, t->par, t->lin0, t->lin1, ox, oy + t->y, pag, pt);
	}
}
