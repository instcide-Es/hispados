/* f_pdf.c - Pluma 2: exportar a PDF. Cada página se recorre con
 * pinta_pagina (pluma/pinta_pag.c), lo mismo que la pantalla, con un
 * «pintor» que escribe al PDF (oficina/comun/pdfsal.c): la x de cada letra
 * es la de la pantalla, así lo que se ve es lo que sale. Los títulos (los
 * párrafos con nivel de título) son los marcadores del PDF.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "archivo.h"
#include "pluma.h"		/* (0.8.8) PLUMA_VERSION */
#include "maqueta.h"
#include "pinta_pag.h"
#include "pdfsal.h"
#include "letras.h"

#define PT(tw) ((double)(tw) / 20.0)

typedef struct {
	pdfsal *s;
	documento *d;
	int *img;		/* el número de cada imagen del documento en el PDF (-2: sin poner) */
} ppdf;

static void p_texto(void *u, const u32 *t, u32 n, const i32 *xs, i32 y, const fmt_letra *f, const info_fmt *in, u32 par, u32 o0)
{
	ppdf *p = u;
	double x[512];
	u32 k;

	(void)par;
	(void)o0;
	if (n > 512)
		n = 512;
	for (k = 0; k < n; k++)
		x[k] = PT(xs[k]);
	pdf_texto(p->s, in->cara, in->falsas, in->tam / 2.0, f->color == SIN_COLOR ? 0 : f->color, t, n, x, PT(y));
}

static void p_rect(void *u, i32 x, i32 y, i32 an, i32 al, u32 c)
{
	pdf_rect(((ppdf *)u)->s, PT(x), PT(y), PT(an), PT(al), c);
}

static void p_raya(void *u, i32 x0, i32 y0, i32 x1, i32 y1, i32 g, u32 c)
{
	pdf_raya(((ppdf *)u)->s, PT(x0), PT(y0), PT(x1), PT(y1), PT(g), c);
}

static void p_imagen(void *u, documento *d, int i, i32 x, i32 y, i32 an, i32 al)
{
	ppdf *p = u;

	if (i < 0 || (u32)i >= d->n_img)
		return;
	if (p->img[i] == -2)
		p->img[i] = pdf_imagen(p->s, d->img[i].datos, d->img[i].n);
	if (p->img[i] >= 0)
		pdf_pon_imagen(p->s, p->img[i], PT(x), PT(y), PT(an), PT(al));
}

int pdf_exporta(documento *d, bufer *b)
{
	pdfsal *s;
	ppdf p;
	pintor pt;
	u32 i;
	int r;

	maq_documento(d);
	if (d->sin_memoria)
		return -1;
	s = pdf_nuevo();
	if (!s)
		return -1;
	memset(&p, 0, sizeof p);
	p.s = s;
	p.d = d;
	p.img = so_pide((d->n_img + 1) * sizeof(int));
	if (!p.img) {
		pdf_suelta(s);
		return -1;
	}
	for (i = 0; i < d->n_img; i++)
		p.img[i] = -2;
	memset(&pt, 0, sizeof pt);
	pt.u = &p;
	pt.texto = p_texto;
	pt.rect = p_rect;
	pt.raya = p_raya;
	pt.imagen = p_imagen;
	{
		/* el título: el del documento o, si no lo tiene, el primer título */
		char tit[120];

		copia(tit, d->titulo, sizeof tit);
		pdf_datos(s, tit, d->autor, d->asunto);
		pdf_productor(s, "Pluma " PLUMA_VERSION " (Hispa-Pentium)");
	}
	for (i = 0; i < d->n_pag; i++) {
		pdf_pagina(s, PT(d->pag_an), PT(d->pag_al));
		pinta_pagina(d, i, &pt);
	}
	/* los marcadores: los títulos */
	for (i = 0; i < d->n_par; i++) {
		const parrafo *q = &d->par[i];
		u32 pg, tz, k, m = 0;
		char t[200];

		if (!q->p.nivel_titulo || !q->n || q->p.tabla)
			continue;
		if (maq_donde(d, i, 0, &pg, &tz) < 0)
			continue;
		for (k = 0; k < q->n && m + 5 < sizeof t; k++) {
			u32 c = q->t[k];

			if (es_objeto(c))
				continue;
			if (c == SALTO_LINEA || c == '\t')
				c = ' ';
			m += (u32)utf8_pon(c, t + m);
		}
		t[m] = 0;
		pdf_marcador(s, t, q->p.nivel_titulo, (int)pg, PT(d->marg_sup + maq_y_parrafo(d, &d->trz[tz], i)));
	}
	r = pdf_acaba(s, b);
	pdf_suelta(s);
	so_suelta(p.img);
	return r;
}
