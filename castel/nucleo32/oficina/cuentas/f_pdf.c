/* f_pdf.c - Cuentas 2: exportar a PDF y la vista preliminar (f_pdf.h y
 * pdf_exporta de archivo.h). La hoja se reparte en páginas del papel que
 * diga «Preparar página» (A4, carta, A3, A5 u oficio; vertical o apaisada,
 * los márgenes, la escala o «encajar en N páginas»), con el área de
 * impresión y los saltos de página puestos a mano, primero hacia abajo y
 * luego a la derecha; cada página, con las celdas (celdas.c) y los gráficos
 * que caen en ella (grafico.c), y su encabezado y su pie.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "cuentas.h"		/* (0.8.8) CUENTAS_VERSION */
#include "f_pdf.h"
#include "celdas.h"
#include "grafico.h"
#include "letras.h"
#include "texto.h"
#include "numeros.h"

#define CM 28.3464567

static void papel(const pagina *pg, double *an, double *al)
{
	static const double t[5][2] = { { 595.28, 841.89 }, { 612, 792 }, { 841.89, 1190.55 }, { 419.53, 595.28 }, { 612, 1008 } };
	int k = pg->papel < 5 ? pg->papel : 0;

	*an = t[k][0];
	*al = t[k][1];
	if (pg->horizontal) {
		double x = *an;

		*an = *al;
		*al = x;
	}
}

static double marg(const pagina *pg, int i)
{
	double m = pg->marg[i];

	if (m < 0 || m > 20)
		m = i & 1 ? 1.9 : 1.8;
	return m * CM;
}

/* la columna (o fila) donde acaba algo que empieza en «desde» y mide «px» */
static u32 hasta_col(const hoja *h, u32 desde, i32 px)
{
	u32 c = desde;

	while (c < CU_COLS - 1 && px > (i32)hj_ancho(h, c)) {
		px -= (i32)hj_ancho(h, c);
		c++;
	}
	return c;
}

static u32 hasta_fila(const hoja *h, u32 desde, i32 px)
{
	u32 f = desde;

	while (f < CU_FILAS - 1 && px > (i32)hj_alto(h, f)) {
		px -= (i32)hj_alto(h, f);
		f++;
	}
	return f;
}

static int hay_salto(const u32 *s, u32 n, u32 p)
{
	u32 i;

	for (i = 0; i < n; i++)
		if (s[i] == p)
			return 1;
	return 0;
}

int pdf_plan_hoja(libro *l, hoja *h, pdf_plan *pl)
{
	const pagina *pg = &h->pag;
	double W, H, s;
	u32 cb[CU_COLS + 1], ncb = 0, *fb = NULL, nfb = 0, c, f, i, j;
	rango z;

	(void)l;
	memset(pl, 0, sizeof *pl);
	papel(pg, &pl->an, &pl->al);
	pl->mx = marg(pg, 0);
	pl->my = marg(pg, 1);
	W = pl->an - marg(pg, 0) - marg(pg, 2);
	H = pl->al - marg(pg, 1) - marg(pg, 3);
	if (W < 36)
		W = 36;
	if (H < 36)
		H = 36;
	/* lo que se imprime */
	if (h->hay_area)
		z = h->area;
	else {
		u32 uf, uc;

		hj_usado(h, &uf, &uc);
		if (uf == CU_SIN_FILA)
			uf = 0, uc = 0;
		z = cu_rango(0, 0, uf, uc);
		/* (las combinadas que empiezan dentro, enteras) */
		for (i = 0; i < h->n_comb; i++)
			if (h->comb[i].f0 <= z.f1 && h->comb[i].c0 <= z.c1) {
				if (h->comb[i].f1 > z.f1)
					z.f1 = h->comb[i].f1;
				if (h->comb[i].c1 > z.c1)
					z.c1 = h->comb[i].c1;
			}
		for (i = 0; i < h->n_gr; i++) {
			const grafico *g = &h->gr[i];
			u32 gc = hasta_col(h, g->c, g->dx + g->an), gf = hasta_fila(h, g->f, g->dy + g->al);

			if (gc > z.c1)
				z.c1 = (u16)gc;
			if (gf > z.f1)
				z.f1 = gf;
		}
	}
	pl->zona = z;
	/* la escala */
	s = pg->escala ? pg->escala / 100.0 : 1;
	if (s < 0.1)
		s = 0.1;
	if (s > 4)
		s = 4;
	if (pg->encajar_an || pg->encajar_al) {
		double tw = 0, th = 0, k = 1;

		for (c = z.c0; c <= z.c1; c++)
			tw += hj_ancho(h, c) * 0.75;
		for (f = z.f0; f <= z.f1; f++)
			th += hj_alto(h, f) * 0.75;
		if (pg->encajar_an && tw > 0 && W * pg->encajar_an / tw < k)
			k = W * pg->encajar_an / tw * 0.999;
		if (pg->encajar_al && th > 0 && H * pg->encajar_al / th < k)
			k = H * pg->encajar_al / th * 0.999;
		s = k < 0.1 ? 0.1 : k;
	}
	pl->esc = 0.75 * s;
	/* los trozos de columnas */
	{
		double x = 0;

		cb[ncb++] = z.c0;
		for (c = z.c0; c <= z.c1; c++) {
			double w = hj_ancho(h, c) * pl->esc;

			if (c > cb[ncb - 1] && ((x > 0 && x + w > W + 0.01) || hay_salto(h->salto_c, h->n_salto_c, c))) {
				cb[ncb++] = c;
				x = 0;
			}
			x += w;
		}
		cb[ncb] = z.c1 + 1u;
	}
	/* los de filas */
	{
		double y = 0;
		u32 cabe = 64;

		fb = so_pide(cabe * sizeof(u32));
		if (!fb)
			return 1;
		fb[nfb++] = z.f0;
		for (f = z.f0; f <= z.f1; f++) {
			double a = hj_alto(h, f) * pl->esc;

			if (f > fb[nfb - 1] && ((y > 0 && y + a > H + 0.01) || hay_salto(h->salto_f, h->n_salto_f, f))) {
				if (nfb + 2 >= cabe) {
					u32 *n = so_repide(fb, cabe * 2 * sizeof(u32));

					if (!n) {
						so_suelta(fb);
						return 1;
					}
					fb = n;
					cabe *= 2;
				}
				fb[nfb++] = f;
				y = 0;
			}
			y += a;
		}
		fb[nfb] = z.f1 + 1;
	}
	pl->t = so_pide(ncb * nfb * sizeof(pdf_trozo));
	if (!pl->t) {
		so_suelta(fb);
		return 1;
	}
	for (i = 0; i < ncb; i++)
		for (j = 0; j < nfb; j++) {
			pdf_trozo *t = &pl->t[pl->n++];

			t->c0 = cb[i];
			t->c1 = cb[i + 1] - 1;
			t->f0 = fb[j];
			t->f1 = fb[j + 1] - 1;
		}
	so_suelta(fb);
	return 0;
}

void pdf_plan_suelta(pdf_plan *pl)
{
	so_suelta(pl->t);
	pl->t = NULL;
	pl->n = 0;
}

int pdf_paginas(libro *l, hoja *h)
{
	pdf_plan pl;
	int n;

	if (pdf_plan_hoja(l, h, &pl))
		return 0;
	n = (int)pl.n;
	pdf_plan_suelta(&pl);
	return n;
}

/* el encabezado o el pie con lo de dentro cambiado */
static void sustituye(const char *s, hoja *h, const char *archivo, u32 pag, u32 total, char *d, u32 cabe)
{
	bufer b = { 0 };

	for (; *s; s++) {
		if (*s == '&' && s[1]) {
			s++;
			switch (*s) {
			case 'P': case 'p': buf_fmt(&b, "%u", pag); break;
			case 'N': case 'n': buf_fmt(&b, "%u", total); break;
			case 'A': case 'a': buf_txt(&b, h->nombre); break;
			case 'F': case 'f': buf_txt(&b, archivo ? archivo : ""); break;
			case 'D': case 'd': {
				int a, m, di, ho, mi, se;

				so_ahora(&a, &m, &di, &ho, &mi, &se);
				buf_fmt(&b, "%02d/%02d/%d", di, m, a);
				break;
			}
			case 'T': case 't': {
				int a, m, di, ho, mi, se;

				so_ahora(&a, &m, &di, &ho, &mi, &se);
				buf_fmt(&b, "%02d:%02d", ho, mi);
				break;
			}
			case '&': buf_c(&b, '&'); break;
			default: buf_c(&b, '&'); buf_c(&b, (u8)*s);
			}
		} else
			buf_c(&b, (u8)*s);
	}
	buf_c(&b, 0);
	copia(d, b.d && !b.mal ? (char *)b.d : "", cabe);
	buf_suelta(&b);
}

void pdf_pinta_pagina(pintor *p, libro *l, hoja *h, const pdf_plan *pl, u32 pag, double x, double y, double k,
		      const char *archivo, u32 primera, u32 total)
{
	const pagina *pg = &h->pag;
	const pdf_trozo *t;
	double W = pl->an - marg(pg, 0) - marg(pg, 2), H = pl->al - marg(pg, 1) - marg(pg, 3);
	double bw = 0, bh = 0, ox, oy, bx = 0, by = 0;
	u32 c, f, i;
	trozo_hoja th;
	int cara = letra_cara(FAM_SANS, 0, 0, NULL);

	if (pag >= pl->n)
		return;
	t = &pl->t[pag];
	p->rect(p, x, y, pl->an * k, pl->al * k, 0xFFFFFF);
	for (c = t->c0; c <= t->c1; c++)
		bw += hj_ancho(h, c);
	for (f = t->f0; f <= t->f1; f++)
		bh += hj_alto(h, f);
	ox = x + (marg(pg, 0) + (pg->centrar_h && bw * pl->esc < W ? (W - bw * pl->esc) / 2 : 0)) * k;
	oy = y + (marg(pg, 1) + (pg->centrar_v && bh * pl->esc < H ? (H - bh * pl->esc) / 2 : 0)) * k;
	memset(&th, 0, sizeof th);
	th.l = l;
	th.h = h;
	th.f0 = t->f0;
	th.f1 = t->f1;
	th.c0 = t->c0;
	th.c1 = t->c1;
	th.x = ox;
	th.y = oy;
	th.esc = pl->esc * k;
	th.rejilla = pg->rejilla;
	th.color_rejilla = 0xB0B6BE;
	th.sin_fondo = 1;
	cel_pinta(p, &th);
	/* los gráficos que caen en esta página */
	for (c = 0; c < t->c0; c++)
		bx += hj_ancho(h, c);
	if (h->n_gr)
		for (f = 0; f < t->f0; f++)
			by += hj_alto(h, f);
	for (i = 0; i < h->n_gr; i++) {
		const grafico *g = &h->gr[i];
		double gx = g->dx, gy = g->dy;

		for (c = 0; c < g->c; c++)
			gx += hj_ancho(h, c);
		for (f = 0; f < g->f; f++)
			gy += hj_alto(h, f);
		if (gx + g->an <= bx || gx >= bx + bw || gy + g->al <= by || gy >= by + bh)
			continue;
		pt_recorte(p, ox, oy, bw * pl->esc * k, bh * pl->esc * k);
		gr_pinta(p, l, g, ox + (gx - bx) * pl->esc * k, oy + (gy - by) * pl->esc * k, g->an * pl->esc * k, g->al * pl->esc * k, pl->esc * k);
		pt_vuelve(p);
	}
	/* el encabezado y el pie */
	{
		char s[300];
		double tam = 9 * k, w;

		if (pg->encabezado[0]) {
			sustituye(pg->encabezado, h, archivo, primera + pag + 1, total, s, sizeof s);
			w = pt_ancho(cara, 0, tam, s, largo(s));
			p->texto(p, cara, 0, tam, x + (pl->an * k - w) / 2, y + marg(pg, 1) * k / 2 + tam * 0.35, s, largo(s), 0x202020);
		}
		if (pg->pie[0]) {
			sustituye(pg->pie, h, archivo, primera + pag + 1, total, s, sizeof s);
			w = pt_ancho(cara, 0, tam, s, largo(s));
			p->texto(p, cara, 0, tam, x + (pl->an * k - w) / 2, y + (pl->al - marg(pg, 3) / 2) * k + tam * 0.35, s, largo(s), 0x202020);
		}
	}
}

int pdf_exporta(libro *l, int hoja_i, const char *titulo, bufer *b, char *err, u32 cabe)
{
	pdfsal *s = pdf_nuevo();
	u32 i, total = 0, hechas = 0;
	int r;

	if (!s) {
		copia(err, "No hay memoria para el PDF", cabe);
		return 1;
	}
	pdf_datos(s, titulo, l->autor, NULL);
	pdf_productor(s, "Cuentas " CUENTAS_VERSION " (Hispa-Pentium)");
	/* cuántas páginas (para &N) */
	for (i = 0; i < l->n_h; i++) {
		if ((hoja_i >= 0 && (u32)hoja_i != i) || (hoja_i < 0 && l->h[i]->oculta))
			continue;
		total += (u32)pdf_paginas(l, l->h[i]);
	}
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];
		pdf_plan pl;
		u32 k;

		if ((hoja_i >= 0 && (u32)hoja_i != i) || (hoja_i < 0 && h->oculta))
			continue;
		if (pdf_plan_hoja(l, h, &pl)) {
			pdf_suelta(s);
			copia(err, "No hay memoria para repartir la hoja en páginas", cabe);
			return 1;
		}
		for (k = 0; k < pl.n; k++) {
			pintor p;
			int n = pdf_pagina(s, pl.an, pl.al);

			if (k == 0 && (hoja_i < 0 || l->n_h > 1))
				pdf_marcador(s, h->nombre, 1, n, 0);
			pt_de_pdf(&p, s, pl.an, pl.al);
			pdf_pinta_pagina(&p, l, h, &pl, k, 0, 0, 1, titulo, hechas, total);
			pt_acaba(&p);
		}
		hechas += pl.n;
		pdf_plan_suelta(&pl);
	}
	if (!hechas) {
		pdf_suelta(s);
		copia(err, "No hay nada que exportar", cabe);
		return 1;
	}
	r = pdf_acaba(s, b);
	pdf_suelta(s);
	if (r) {
		copia(err, "No hay memoria para el PDF", cabe);
		return 1;
	}
	return 0;
}
