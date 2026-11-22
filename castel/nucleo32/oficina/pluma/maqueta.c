/* maqueta.c - Pluma 2: la maquetación (pluma/maqueta.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "maqueta.h"
#include "letras.h"
#include "texto.h"

/* ---- lo de cada formato ---- */
static info_fmt *infos;
static u32 n_infos, cabe_infos;
static const documento *de_doc;

const info_fmt *maq_info(documento *d, u16 f)
{
	if (de_doc != d) {
		n_infos = 0;
		de_doc = d;
	}
	if (f >= d->n_fl)
		f = 0;
	while (n_infos <= f) {
		const fmt_letra *l = &d->fl[n_infos];
		info_fmt *i;
		const ttf *t;

		if (n_infos >= cabe_infos) {
			u32 c = cabe_infos ? cabe_infos * 2 : 64;
			info_fmt *n = so_repide(infos, c * sizeof(info_fmt));

			if (!n) {
				static info_fmt cero;

				cero.cara = letra_cara(FAM_SERIF, 0, 0, &cero.falsas);
				cero.tam = 24;
				cero.asc = 220;
				cero.desc = 60;
				return &cero;
			}
			infos = n;
			cabe_infos = c;
		}
		i = &infos[n_infos];
		memset(i, 0, sizeof *i);
		i->cara = letra_cara(l->familia, l->negrita, l->cursiva, &i->falsas);
		i->tam = l->tam ? l->tam : 24;
		if (l->vert) {
			i->sube = l->vert == 1 ? i->tam * 10 * 33 / 100 : -(i->tam * 10 * 14 / 100);
			i->tam = i->tam * 58 / 100;
		}
		if (l->versalitas) {
			i->mayus = 1;
		}
		t = letra_ttf(i->cara);
		i->asc = (i32)((i64)t->asc * i->tam * 10 / t->em) + (i->sube > 0 ? i->sube : 0);
		i->desc = (i32)((i64)-t->desc * i->tam * 10 / t->em) + (i->sube < 0 ? -i->sube : 0);
		if (l->vert) {
			/* (la línea no crece por un índice: lo alto, el de la letra normal) */
			const ttf *t2 = t;
			i32 tn = l->tam ? l->tam : 24;

			i->asc = (i32)((i64)t2->asc * tn * 10 / t2->em);
			i->desc = (i32)((i64)-t2->desc * tn * 10 / t2->em);
		}
		n_infos++;
	}
	return &infos[f];
}

/* lo que mide el trozo de texto de o (sin incluir) a la tabulación o el final siguiente */
static i32 trozo_hasta_tab(documento *d, const parrafo *p, u32 o)
{
	i32 w = 0;
	u32 k;

	for (k = o + 1; k < p->n && p->t[k] != '\t' && p->t[k] != SALTO_LINEA; k++)
		w += maq_avance(d, p, k, 0);
	return w;
}

i32 maq_avance(documento *d, const parrafo *p, u32 o, i32 x)
{
	u32 c = p->t[o];
	const info_fmt *in = maq_info(d, p->f[o]);

	if (c == '\t') {
		u32 k;
		i32 pos = 0;
		u8 tipo = TAB_IZQ;

		for (k = 0; k < p->p.n_tabs; k++)
			if (p->p.tabs[k].pos > x) {
				pos = p->p.tabs[k].pos;
				tipo = p->p.tabs[k].tipo;
				break;
			}
		if (!pos) {
			/* las de siempre: cada 1,25 cm (detrás de la sangría) */
			pos = (x / TAB_DEFECTO + 1) * TAB_DEFECTO;
			if (p->p.sang_izq > x && p->p.sang_izq < pos)
				pos = p->p.sang_izq;
		}
		if (tipo == TAB_DER || tipo == TAB_CENTRO || tipo == TAB_DECIMAL) {
			i32 w = trozo_hasta_tab(d, p, o);

			if (tipo == TAB_CENTRO)
				w /= 2;
			if (tipo == TAB_DECIMAL) {
				/* hasta la coma (o el punto) */
				u32 k2;
				i32 w2 = 0;

				for (k2 = o + 1; k2 < p->n && p->t[k2] != '\t' && p->t[k2] != ',' && p->t[k2] != '.'; k2++)
					w2 += maq_avance(d, p, k2, 0);
				w = w2;
			}
			if (pos - w > x)
				return pos - w - x;
			return 40;
		}
		return pos - x > 0 ? pos - x : 40;
	}
	if (c == SALTO_LINEA || c == '\n' || c == '\r' || c == 0x0C)
		return 0;
	if (es_imagen(c)) {
		u32 n = c - IMAGEN_0;

		if (n >= d->n_img || d->img[n].ajuste != AJUSTE_LINEA)
			return 0;
		return d->img[n].ancho;
	}
	if (c == CAMPO_PAGINA || c == CAMPO_PAGINAS)
		return (i32)(letra_milesimas(in->cara, '0') * 2 * in->tam / 100);
	if (es_nota(c))
		return (i32)(letra_milesimas(in->cara, '0') * in->tam * 58 / 10000) + 20;
	{
		u32 u = c;
		i32 tam = in->tam, a;

		if (in->mayus && minuscula(c) == c && mayuscula(c) != c) {
			u = mayuscula(c);
			tam = tam * 80 / 100;
		}
		a = (i32)(((i64)letra_milesimas(in->cara, u) * tam + 50) / 100);
		if (in->falsas & FALSA_NEGRITA)
			a += tam / 4;
		return a;
	}
}

/* ---- las listas ---- */
void maq_marca_lista(documento *d, u32 i, u32 *t, int cabe)
{
	const parrafo *p = &d->par[i];
	char b[16];
	u32 n = 1, k, j;

	t[0] = 0;
	if (!p->p.lista)
		return;
	if (p->p.lista == LISTA_VINETAS) {
		static const u32 vin[] = { 0x2022, 0x25E6, 0x25AA };
		/* (las DejaVu recortadas traen • y ▪; ◦ no: se usa –) */
		u32 v = vin[p->p.nivel % 3];

		if (v == 0x25E6)
			v = 0x2013;
		t[0] = v;
		t[1] = 0;
		return;
	}
	/* el número: los de antes con la misma lista y el mismo nivel, hasta uno
	 * de nivel más bajo o que no es de la lista */
	for (k = i; k-- > 0;) {
		const parrafo *q = &d->par[k];

		if (!q->p.lista || q->p.tabla != p->p.tabla) {
			if (q->n == 0 && !q->p.lista)
				continue;	/* (un párrafo vacío no corta la lista) */
			break;
		}
		if (q->p.lista_id != p->p.lista_id)
			break;
		if (q->p.nivel < p->p.nivel)
			break;
		if (q->p.nivel == p->p.nivel) {
			n++;
			if (q->p.inicio_lista) {
				n += q->p.inicio_lista - 1;
				break;
			}
		}
	}
	if (p->p.inicio_lista && n == 1)
		n = p->p.inicio_lista;
	doc_numero_lista(p->p.lista, n, b, p->p.nivel);
	for (j = 0; b[j] && (int)j + 1 < cabe; j++)
		t[j] = (u8)b[j];
	t[j] = 0;
}

/* lo que mide la marca de la lista (con su hueco) */
static i32 ancho_marca(documento *d, parrafo *p, u32 i)
{
	u32 t[16], k;
	i32 w = 0;
	const info_fmt *in = maq_info(d, par_fmt_en(d, p, 0));

	if (!p->p.lista)
		return 0;
	maq_marca_lista(d, i, t, 16);
	for (k = 0; t[k]; k++)
		w += (i32)(((i64)letra_milesimas(in->cara, t[k]) * in->tam + 50) / 100);
	return w + 120;
}

/* ---- partir un párrafo en líneas ---- */
static int mete_linea(parrafo *p, const linea *l)
{
	if (p->n_lin >= p->cabe_lin) {
		u32 c = p->cabe_lin ? p->cabe_lin * 2 : 4;
		linea *n = so_repide(p->lin, c * sizeof(linea));

		if (!n)
			return 0;
		p->lin = n;
		p->cabe_lin = c;
	}
	p->lin[p->n_lin++] = *l;
	return 1;
}


/* las imágenes que flotan ancladas en el párrafo: lo alto de la más alta,
 * y lo que quitan a cada lado */
static i32 flota(documento *d, const parrafo *p, i32 *izq, i32 *der)
{
	u32 k;
	i32 alto = 0;

	*izq = *der = 0;
	for (k = 0; k < p->n; k++)
		if (es_imagen(p->t[k])) {
			u32 n = p->t[k] - IMAGEN_0;
			const imagen *m;

			if (n >= d->n_img)
				continue;
			m = &d->img[n];
			if (m->ajuste == AJUSTE_LINEA)
				continue;
			if (m->alto > alto)
				alto = m->alto;
			if (m->ajuste == AJUSTE_IZQ && m->ancho + HUECO_FLOTA > *izq)
				*izq = m->ancho + HUECO_FLOTA;
			if (m->ajuste == AJUSTE_DER && m->ancho + HUECO_FLOTA > *der)
				*der = m->ancho + HUECO_FLOTA;
		}
	return alto;
}

static int es_espacio_linea(u32 c)
{
	return c == ' ' || c == 0x3000;
}

void maq_parrafo(documento *d, parrafo *p, i32 ancho)
{
	u32 o = 0, idx;
	i32 y = 0, f_izq, f_der, f_alto, marca = 0;
	int primera = 1;

	if (p->ancho_maq == ancho && p->n_lin)
		return;
	p->n_lin = 0;
	idx = (u32)(p - d->par);
	if (p >= d->par && idx < d->n_par)
		marca = ancho_marca(d, p, idx);
	else
		idx = 0xFFFFFFFFu;
	f_alto = flota(d, p, &f_izq, &f_der);
	do {
		linea l;
		i32 x_ini, disp, x = 0, ancho_hasta_corte = 0, asc = 0, desc = 0, natural;
		u32 k, corte = 0, fin;
		u16 espacios = 0, esp_corte = 0;
		const info_fmt *in;

		memset(&l, 0, sizeof l);
		x_ini = p->p.sang_izq + (primera ? p->p.sang_primera : 0);
		if (primera && p->p.lista) {
			/* la marca va en la sangría francesa; el texto, detrás */
			if (p->p.sang_primera < 0)
				x_ini = p->p.sang_izq;
			else
				x_ini += marca;
			if (x_ini < p->p.sang_izq + p->p.sang_primera + marca)
				x_ini = p->p.sang_izq + p->p.sang_primera + marca;
		}
		if (y < f_alto)
			x_ini += f_izq;
		if (x_ini < 0)
			x_ini = 0;
		disp = ancho - p->p.sang_der - (y < f_alto ? f_der : 0);
		if (disp < x_ini + 200)
			disp = x_ini + 200;
		/* las letras que caben */
		for (k = o; k < p->n; k++) {
			u32 c = p->t[k];
			i32 a;

			if (c == SALTO_LINEA) {
				k++;
				corte = k;
				ancho_hasta_corte = x;
				esp_corte = espacios;
				l.ultima = 1;
				break;
			}
			a = maq_avance(d, p, k, x_ini + x);
			if (es_espacio_linea(c)) {
				/* (los espacios del final no cuentan) */
				x += a;
				espacios++;
				corte = k + 1;
				ancho_hasta_corte = x - a;
				esp_corte = (u16)(espacios - 1);
				continue;
			}
			if (x_ini + x + a > disp && k > o) {
				if (!corte || corte <= o) {
					corte = k;	/* (una palabra que no cabe: se parte) */
					ancho_hasta_corte = x;
					esp_corte = espacios;
				}
				break;
			}
			x += a;
			if (c == '-' || c == 0x2010 || c == 0x2013 || c == 0x2014 || c == '/') {
				corte = k + 1;
				ancho_hasta_corte = x;
				esp_corte = espacios;
			}
		}
		if (k >= p->n && (!l.ultima)) {
			corte = p->n;
			ancho_hasta_corte = x;
			/* (sin los espacios del final) */
			{
				u32 q = p->n;

				while (q > o && es_espacio_linea(p->t[q - 1])) {
					q--;
					ancho_hasta_corte -= maq_avance(d, p, q, 0);
				}
			}
			esp_corte = espacios;
			l.ultima = 1;
		}
		fin = corte > o ? corte : (p->n > o ? o + 1 : o);
		l.ini = o;
		l.fin = fin;
		/* lo alto: la letra más alta y la que más baja (y las imágenes) */
		for (k = o; k < fin; k++) {
			u32 c = p->t[k];

			if (es_imagen(c) && c - IMAGEN_0 < d->n_img && d->img[c - IMAGEN_0].ajuste == AJUSTE_LINEA) {
				if (d->img[c - IMAGEN_0].alto > asc)
					asc = d->img[c - IMAGEN_0].alto;
				continue;
			}
			in = maq_info(d, p->f[k]);
			if (in->asc > asc)
				asc = in->asc;
			if (in->desc > desc)
				desc = in->desc;
		}
		if (!asc && !desc) {
			in = maq_info(d, par_fmt_en(d, p, o < p->n ? o + 1 : p->n));
			asc = in->asc;
			desc = in->desc;
		}
		natural = asc + desc;
		switch (p->p.inter_tipo) {
		case INTER_EXACTO:
			l.alto = p->p.inter > 40 ? p->p.inter : natural;
			l.base = l.alto - desc;
			break;
		case INTER_MINIMO:
			l.alto = natural > p->p.inter ? natural : p->p.inter;
			l.base = l.alto - desc;
			break;
		default: {
			i32 m = p->p.inter ? p->p.inter : 100;

			l.alto = (i32)((i64)natural * m / 100);
			if (l.alto < natural / 2)
				l.alto = natural / 2;
			l.base = asc + (l.alto > natural ? (l.alto - natural) * 6 / 10 : 0);
			if (l.alto < natural)
				l.base = l.alto - desc;
		}
		}
		l.y = y;
		l.izq = x_ini;
		l.ancho = ancho_hasta_corte;
		l.espacios = esp_corte;
		{
			i32 libre = disp - x_ini - ancho_hasta_corte;

			if (libre < 0)
				libre = 0;
			switch (p->p.alin) {
			case AL_CENTRO: l.x0 = x_ini + libre / 2; break;
			case AL_DER: l.x0 = x_ini + libre; break;
			case AL_JUST:
				l.x0 = x_ini;
				if (!l.ultima && esp_corte)
					l.extra = (i32)((i64)libre * 1000 / esp_corte);
				break;
			default: l.x0 = x_ini;
			}
		}
		if (!mete_linea(p, &l)) {
			d->sin_memoria = 1;
			break;
		}
		y += l.alto;
		o = fin;
		primera = 0;
		/* (detrás de un salto de línea al final, otra línea vacía) */
		if (l.ultima && o < p->n)
			l.ultima = 0;
		else if (l.ultima)
			break;
	} while (o < p->n);
	if (p->n_lin && p->n && p->t[p->n - 1] == SALTO_LINEA) {
		/* el salto del final deja una línea vacía detrás */
		linea l = p->lin[p->n_lin - 1];

		l.ini = l.fin = p->n;
		l.y = y;
		l.ancho = 0;
		l.extra = 0;
		l.espacios = 0;
		l.ultima = 1;
		if (mete_linea(p, &l))
			y += l.alto;
	}
	p->alto = y > f_alto ? y : f_alto;
	p->ancho_maq = ancho;
}

/* ---- las tablas ---- */
i32 maq_x_col(const documento *d, u32 t, u32 c)
{
	const tabla *tb = &d->tab[t];
	i32 x = 0, total = 0;
	u32 k;

	for (k = 0; k < tb->cols; k++)
		total += tb->ancho[k];
	if (tb->alin == AL_CENTRO)
		x = (doc_ancho_texto(d) - total) / 2;
	else if (tb->alin == AL_DER)
		x = doc_ancho_texto(d) - total;
	if (x < 0)
		x = 0;
	for (k = 0; k < c && k < tb->cols; k++)
		x += tb->ancho[k];
	return x;
}

i32 maq_ancho_celda(const documento *d, const parrafo *p)
{
	const tabla *tb;
	i32 w = 0;
	u32 k, n;

	if (!p->p.tabla || p->p.tabla > d->n_tab)
		return doc_ancho_texto(d);
	tb = &d->tab[p->p.tabla - 1];
	n = p->p.abarca_cols ? p->p.abarca_cols : 1;
	for (k = p->p.col; k < p->p.col + n && k < tb->cols; k++)
		w += tb->ancho[k];
	w -= 2 * tb->margen;
	return w < 200 ? 200 : w;
}

i32 maq_ancho_de(documento *d, u32 i)
{
	return d->par[i].p.tabla ? maq_ancho_celda(d, &d->par[i]) : doc_ancho_texto(d);
}

i32 maq_fila(documento *d, u32 i, u32 *fin)
{
	parrafo *p = &d->par[i];
	u32 t = p->p.tabla, fila = p->p.fila, k;
	i32 alto = 0, m;

	m = t && t <= d->n_tab ? d->tab[t - 1].margen : 100;
	for (k = i; k < d->n_par && d->par[k].p.tabla == t && d->par[k].p.fila == fila; k++) {
		parrafo *c = &d->par[k];
		i32 h;

		if (c->p.cubierta)
			continue;
		maq_parrafo(d, c, maq_ancho_celda(d, c));
		h = c->alto + c->p.antes + c->p.despues + 2 * m;
		if (c->p.abarca_filas > 1)
			h /= c->p.abarca_filas;
		if (h > alto)
			alto = h;
	}
	*fin = k;
	return alto < 2 * m + 200 ? 2 * m + 200 : alto;
}

/* ---- las páginas ---- */
static pagina *nueva_pagina(documento *d)
{
	if (d->n_pag >= d->cabe_pag) {
		u32 c = d->cabe_pag ? d->cabe_pag * 2 : 16;
		pagina *n = so_repide(d->pag, c * sizeof(pagina));

		if (!n)
			return NULL;
		d->pag = n;
		d->cabe_pag = c;
	}
	memset(&d->pag[d->n_pag], 0, sizeof(pagina));
	d->pag[d->n_pag].t0 = d->n_trz;
	return &d->pag[d->n_pag++];
}

static int nuevo_trozo(documento *d, u32 par, u32 l0, u32 l1, i32 y)
{
	trozo *t;

	if (d->n_trz >= d->cabe_trz) {
		u32 c = d->cabe_trz ? d->cabe_trz * 2 : 64;
		trozo *n = so_repide(d->trz, c * sizeof(trozo));

		if (!n)
			return 0;
		d->trz = n;
		d->cabe_trz = c;
	}
	t = &d->trz[d->n_trz++];
	memset(t, 0, sizeof *t);
	t->par = par;
	t->lin0 = l0;
	t->lin1 = l1;
	t->y = y;
	d->pag[d->n_pag - 1].n_t++;
	return 1;
}

void maq_documento(documento *d)
{
	i32 ancho = doc_ancho_texto(d), alto = doc_alto_texto(d), y = 0;
	u32 i;

	for (i = 0; i < d->n_par; i++)
		if (!d->par[i].p.tabla)
			maq_parrafo(d, &d->par[i], ancho);
	maq_parrafo(d, &d->enc, ancho);
	maq_parrafo(d, &d->pie, ancho);
	d->n_pag = 0;
	d->n_trz = 0;
	if (!nueva_pagina(d)) {
		d->sin_memoria = 1;
		return;
	}
	i = 0;
	while (i < d->n_par) {
		parrafo *p = &d->par[i];
		u32 l;

		if (p->p.tabla) {
			u32 fin;
			i32 h = maq_fila(d, i, &fin);

			if (y + h > alto && y > 0) {
				if (!nueva_pagina(d))
					break;
				y = 0;
			}
			if (!nuevo_trozo(d, i, 0, 0, y))
				break;
			d->trz[d->n_trz - 1].fila_fin = fin;
			d->trz[d->n_trz - 1].alto_fila = h;
			y += h;
			/* (detrás de la tabla, un poco de aire) */
			if (fin >= d->n_par || d->par[fin].p.tabla != p->p.tabla)
				y += 120;
			i = fin;
			continue;
		}
		if ((p->p.salto_antes || (p->n == 1 && p->t[0] == 0x0C)) && y > 0 && !(i > 0 && d->par[i - 1].n == 1 && d->par[i - 1].t[0] == 0x0C)) {
			if (!nueva_pagina(d))
				break;
			y = 0;
		}
		if (y > 0)
			y += p->p.antes;
		/* con el siguiente: si no caben este y la primera línea del otro, a la otra página */
		if (p->p.juntos && y > 0 && i + 1 < d->n_par && !d->par[i + 1].p.tabla && d->par[i + 1].n_lin) {
			if (y + p->alto + p->p.despues + d->par[i + 1].p.antes + d->par[i + 1].lin[0].alto > alto && p->alto < alto / 2) {
				if (!nueva_pagina(d))
					break;
				y = 0;
			}
		}
		l = 0;
		while (l < p->n_lin) {
			u32 l0 = l;
			i32 y0 = y;

			while (l < p->n_lin && (y + p->lin[l].alto <= alto || (y == 0 && l == l0))) {
				y += p->lin[l].alto;
				l++;
			}
			if (l > l0 && !nuevo_trozo(d, i, l0, l, y0))
				break;
			if (l < p->n_lin) {
				if (!nueva_pagina(d))
					break;
				y = 0;
			}
		}
		/* (lo que sobra de una imagen que flota) */
		if (p->n_lin && p->alto > p->lin[p->n_lin - 1].y + p->lin[p->n_lin - 1].alto)
			y += p->alto - (p->lin[p->n_lin - 1].y + p->lin[p->n_lin - 1].alto);
		y += p->p.despues;
		/* un salto de página solo en su párrafo: lo de después, en la otra */
		if (p->n == 1 && p->t[0] == 0x0C && i + 1 < d->n_par) {
			if (!nueva_pagina(d))
				break;
			y = 0;
		}
		i++;
	}
	if (d->n_trz == 0)
		nuevo_trozo(d, 0, 0, d->n_par ? d->par[0].n_lin : 0, 0);
}

/* ---- dónde está cada letra ---- */
i32 maq_x_de(documento *d, const parrafo *p, const linea *l, u32 o)
{
	i32 acum = 0, extra = 0;
	u32 k;

	for (k = l->ini; k < o && k < l->fin; k++) {
		u32 c = p->t[k];

		acum += maq_avance(d, p, k, l->izq + acum);
		if (l->extra && es_espacio_linea(c))
			extra += l->extra;
	}
	return l->x0 + acum + extra / 1000;
}

u32 maq_o_de(documento *d, const parrafo *p, const linea *l, i32 x)
{
	i32 acum = 0, extra = 0;
	u32 k, fin = l->fin;

	/* (no detrás del salto de línea o del espacio que parte la línea) */
	if (fin > l->ini && (p->t[fin - 1] == SALTO_LINEA || (!l->ultima && es_espacio_linea(p->t[fin - 1]))))
		fin--;
	for (k = l->ini; k < fin; k++) {
		i32 a = maq_avance(d, p, k, l->izq + acum), xa = l->x0 + acum + extra / 1000;

		if (l->extra && es_espacio_linea(p->t[k]))
			a += l->extra / 1000;
		if (x < xa + a / 2)
			return k;
		acum += maq_avance(d, p, k, l->izq + acum);
		if (l->extra && es_espacio_linea(p->t[k]))
			extra += l->extra;
	}
	return fin;
}

u32 maq_linea_de(const parrafo *p, u32 o, int al_final)
{
	u32 a = 0, b = p->n_lin;

	if (!p->n_lin)
		return 0;
	/* la última con ini <= o */
	while (b - a > 1) {
		u32 m = (a + b) / 2;

		if (p->lin[m].ini <= o)
			a = m;
		else
			b = m;
	}
	if (al_final && a > 0 && p->lin[a].ini == o && p->lin[a - 1].fin == o && p->t && o > 0 && p->t[o - 1] != SALTO_LINEA)
		a--;
	return a;
}

int maq_donde(const documento *d, u32 i, u32 l, u32 *pagina_, u32 *trz)
{
	u32 a = 0, b = d->n_trz, k, pg;

	if (!d->n_trz)
		return -1;
	/* el último trozo con par <= i */
	while (b - a > 1) {
		u32 m = (a + b) / 2;

		if (d->trz[m].par <= i)
			a = m;
		else
			b = m;
	}
	/* (un párrafo partido en varias páginas: el trozo de su línea) */
	k = a;
	while (k > 0 && d->trz[k].par == i && d->trz[k].lin0 > l && !d->trz[k].fila_fin)
		k--;
	if (d->trz[k].par > i)
		return -1;
	/* (una celda: el trozo de su fila empieza antes) */
	*trz = k;
	pg = 0;
	{
		u32 x = 0, y = d->n_pag;

		while (y - x > 1) {
			u32 m = (x + y) / 2;

			if (d->pag[m].t0 <= k)
				x = m;
			else
				y = m;
		}
		pg = x;
	}
	*pagina_ = pg;
	return 0;
}

i32 maq_y_parrafo(documento *d, const trozo *t, u32 i)
{
	const parrafo *p = &d->par[i];

	if (t->fila_fin) {
		const tabla *tb = p->p.tabla && p->p.tabla <= d->n_tab ? &d->tab[p->p.tabla - 1] : NULL;

		return t->y + (tb ? tb->margen : 100) + p->p.antes;
	}
	return t->y - (t->lin0 < p->n_lin ? p->lin[t->lin0].y : 0);
}
