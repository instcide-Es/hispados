/* editar.c - Pluma 2: los cambios del documento y deshacer (pluma/editar.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "editar.h"
#include "texto.h"
#include "maqueta.h"

#define PASO_TECLEO 1
#define PASO_BORRA 2
#define PASO_OTRO 3

struct paso {
	u32 p0;
	u32 n_ahora;			/* los párrafos que hay ahora en el documento en p0 */
	parrafo *otros;			/* los de la otra versión (los de antes, o los de después) */
	u32 n_otros;
	tabla *tabs;			/* (PASO_TABLAS) las tablas de la otra versión */
	u32 n_tabs;
	struct {
		i32 pag_an, pag_al, marg_izq, marg_der, marg_sup, marg_inf, dist_enc, dist_pie;
		u8 hay_enc, hay_pie, enc_no_primera;
		u32 num_inicio;
		parrafo enc, pie;
	} *pagina;			/* (PASO_PAGINA) */
	pos cur_otro, ancla_otro;	/* el cursor de la otra versión */
	int clase;
	u32 version;			/* (la del documento tras el paso) */
};

/* ---- lo elegido ---- */
int pos_menor(pos a, pos b)
{
	return a.p < b.p || (a.p == b.p && a.o < b.o);
}

static int pos_igual(pos a, pos b)
{
	return a.p == b.p && a.o == b.o;
}

static pos pos_de(u32 p, u32 o)
{
	pos r;

	r.p = p;
	r.o = o;
	return r;
}

static void ajusta(const documento *d, pos *c)
{
	if (c->p >= d->n_par)
		*c = pos_de(d->n_par - 1, d->par[d->n_par - 1].n);
	if (c->o > d->par[c->p].n)
		c->o = d->par[c->p].n;
}

int ed_eleccion(const editor *e, pos *a, pos *b)
{
	if (pos_menor(e->cur, e->ancla)) {
		*a = e->cur;
		*b = e->ancla;
	} else {
		*a = e->ancla;
		*b = e->cur;
	}
	return !pos_igual(*a, *b);
}

void ed_elige(editor *e, pos a, pos b)
{
	ajusta(e->d, &a);
	ajusta(e->d, &b);
	e->ancla = a;
	e->cur = b;
	e->hay_pendiente = 0;
}

void ed_elige_todo(editor *e)
{
	ed_elige(e, pos_de(0, 0), pos_de(e->d->n_par - 1, e->d->par[e->d->n_par - 1].n));
}

void ed_cursor(editor *e, pos c, int ampliar)
{
	ajusta(e->d, &c);
	e->cur = c;
	if (!ampliar)
		e->ancla = c;
	e->hay_pendiente = 0;
}

void ed_inicia(editor *e, documento *d)
{
	memset(e, 0, sizeof *e);
	e->d = d;
	e->version_guardada = d->version;
}

static void suelta_paso(paso *s)
{
	u32 i;

	for (i = 0; i < s->n_otros; i++)
		par_suelta(&s->otros[i]);
	so_suelta(s->otros);
	so_suelta(s->tabs);
	if (s->pagina) {
		par_suelta(&s->pagina->enc);
		par_suelta(&s->pagina->pie);
		so_suelta(s->pagina);
	}
	memset(s, 0, sizeof *s);
}

void ed_suelta(editor *e)
{
	u32 i;

	for (i = 0; i < e->n_pasos; i++)
		suelta_paso(&e->pasos[i]);
	so_suelta(e->pasos);
	e->pasos = NULL;
	e->n_pasos = e->hechos = e->cabe_pasos = 0;
}

/* ---- deshacer ---- */
void ed_empieza(editor *e, u32 p0, u32 p1, int clase)
{
	documento *d = e->d;
	paso *s;
	u32 i;

	if (p1 > d->n_par)
		p1 = d->n_par;
	if (p0 > p1)
		p0 = p1;
	/* lo que se había deshecho ya no se puede rehacer */
	for (i = e->hechos; i < e->n_pasos; i++)
		suelta_paso(&e->pasos[i]);
	e->n_pasos = e->hechos;
	if (e->n_pasos >= e->cabe_pasos) {
		u32 c = e->cabe_pasos ? e->cabe_pasos * 2 : 32;
		paso *n = so_repide(e->pasos, c * sizeof(paso));

		if (!n) {
			e->abierto = 0;
			d->sin_memoria = 1;
			return;
		}
		e->pasos = n;
		e->cabe_pasos = c;
	}
	s = &e->pasos[e->n_pasos];
	memset(s, 0, sizeof *s);
	s->p0 = p0;
	s->n_otros = p1 - p0;
	s->clase = clase;
	s->cur_otro = e->cur;
	s->ancla_otro = e->ancla;
	if (s->n_otros) {
		s->otros = so_pide(s->n_otros * sizeof(parrafo));
		if (!s->otros) {
			d->sin_memoria = 1;
			return;
		}
		for (i = 0; i < s->n_otros; i++)
			if (!par_copia(&s->otros[i], &d->par[p0 + i])) {
				u32 k;

				for (k = 0; k < i; k++)
					par_suelta(&s->otros[k]);
				so_suelta(s->otros);
				s->otros = NULL;
				d->sin_memoria = 1;
				return;
			}
	}
	if (clase & PASO_TABLAS) {
		s->tabs = so_pide((d->n_tab + 1) * sizeof(tabla));
		if (s->tabs) {
			if (d->n_tab)
				memcpy(s->tabs, d->tab, d->n_tab * sizeof(tabla));
			s->n_tabs = d->n_tab;
		}
	}
	if (clase & PASO_PAGINA) {
		s->pagina = so_pide(sizeof *s->pagina);
		if (s->pagina) {
			s->pagina->pag_an = d->pag_an;
			s->pagina->pag_al = d->pag_al;
			s->pagina->marg_izq = d->marg_izq;
			s->pagina->marg_der = d->marg_der;
			s->pagina->marg_sup = d->marg_sup;
			s->pagina->marg_inf = d->marg_inf;
			s->pagina->dist_enc = d->dist_enc;
			s->pagina->dist_pie = d->dist_pie;
			s->pagina->hay_enc = d->hay_enc;
			s->pagina->hay_pie = d->hay_pie;
			s->pagina->enc_no_primera = d->enc_no_primera;
			s->pagina->num_inicio = d->num_inicio;
			par_copia(&s->pagina->enc, &d->enc);
			par_copia(&s->pagina->pie, &d->pie);
		}
	}
	e->abierto = 1;
}

void ed_acaba(editor *e, u32 n_ahora)
{
	paso *s;

	if (!e->abierto)
		return;
	e->abierto = 0;
	s = &e->pasos[e->n_pasos];
	s->n_ahora = n_ahora;
	e->n_pasos++;
	e->hechos = e->n_pasos;
	e->d->version++;
	e->d->cambiado = 1;
	s->version = e->d->version;
}

/* cambia la versión del documento por la del paso (deshacer y rehacer son lo mismo) */
static void cambia_paso(editor *e, paso *s)
{
	documento *d = e->d;
	parrafo *ahora = NULL;
	u32 i, n = s->n_ahora;
	pos c, a;

	if (n) {
		ahora = so_pide(n * sizeof(parrafo));
		if (!ahora) {
			d->sin_memoria = 1;
			return;
		}
		memcpy(ahora, d->par + s->p0, n * sizeof(parrafo));
	}
	/* (se sacan sin soltarlos: pasan al paso) */
	memmove(d->par + s->p0, d->par + s->p0 + n, (d->n_par - s->p0 - n) * sizeof(parrafo));
	d->n_par -= n;
	if (s->n_otros) {
		if (!doc_inserta_parrafos(d, s->p0, s->n_otros)) {
			/* (sin memoria: se deja como estaba) */
			memmove(d->par + s->p0 + n, d->par + s->p0, (d->n_par - s->p0) * sizeof(parrafo));
			memcpy(d->par + s->p0, ahora, n * sizeof(parrafo));
			d->n_par += n;
			so_suelta(ahora);
			return;
		}
		memcpy(d->par + s->p0, s->otros, s->n_otros * sizeof(parrafo));
		for (i = 0; i < s->n_otros; i++)
			d->par[s->p0 + i].ancho_maq = -1;
	}
	so_suelta(s->otros);
	s->otros = ahora;
	s->n_ahora = s->n_otros;
	s->n_otros = n;
	if (s->tabs) {
		tabla *t = so_pide((d->n_tab + 1) * sizeof(tabla));

		if (t) {
			u32 k = d->n_tab;

			if (d->n_tab)
				memcpy(t, d->tab, d->n_tab * sizeof(tabla));
			if (s->n_tabs > d->cabe_tab) {
				tabla *nt = so_repide(d->tab, s->n_tabs * sizeof(tabla));

				if (nt) {
					d->tab = nt;
					d->cabe_tab = s->n_tabs;
				}
			}
			if (s->n_tabs <= d->cabe_tab) {
				memcpy(d->tab, s->tabs, s->n_tabs * sizeof(tabla));
				d->n_tab = s->n_tabs;
			}
			so_suelta(s->tabs);
			s->tabs = t;
			s->n_tabs = k;
		}
		/* (las celdas, a maquetar otra vez) */
		for (i = 0; i < d->n_par; i++)
			if (d->par[i].p.tabla)
				d->par[i].ancho_maq = -1;
	}
	if (s->pagina) {
		__typeof__(*s->pagina) v = *s->pagina;

		s->pagina->pag_an = d->pag_an;
		s->pagina->pag_al = d->pag_al;
		s->pagina->marg_izq = d->marg_izq;
		s->pagina->marg_der = d->marg_der;
		s->pagina->marg_sup = d->marg_sup;
		s->pagina->marg_inf = d->marg_inf;
		s->pagina->dist_enc = d->dist_enc;
		s->pagina->dist_pie = d->dist_pie;
		s->pagina->hay_enc = d->hay_enc;
		s->pagina->hay_pie = d->hay_pie;
		s->pagina->enc_no_primera = d->enc_no_primera;
		s->pagina->num_inicio = d->num_inicio;
		s->pagina->enc = d->enc;
		s->pagina->pie = d->pie;
		d->pag_an = v.pag_an;
		d->pag_al = v.pag_al;
		d->marg_izq = v.marg_izq;
		d->marg_der = v.marg_der;
		d->marg_sup = v.marg_sup;
		d->marg_inf = v.marg_inf;
		d->dist_enc = v.dist_enc;
		d->dist_pie = v.dist_pie;
		d->hay_enc = v.hay_enc;
		d->hay_pie = v.hay_pie;
		d->enc_no_primera = v.enc_no_primera;
		d->num_inicio = v.num_inicio;
		d->enc = v.enc;
		d->pie = v.pie;
		d->enc.ancho_maq = d->pie.ancho_maq = -1;
		for (i = 0; i < d->n_par; i++)
			d->par[i].ancho_maq = -1;
	}
	c = s->cur_otro;
	a = s->ancla_otro;
	s->cur_otro = e->cur;
	s->ancla_otro = e->ancla;
	e->cur = c;
	e->ancla = a;
	ajusta(d, &e->cur);
	ajusta(d, &e->ancla);
	e->hay_pendiente = 0;
	d->version++;
	d->cambiado = 1;
	/* (las listas que siguen: los números pueden cambiar) */
	for (i = s->p0; i < d->n_par && i < s->p0 + s->n_ahora + 50; i++)
		if (d->par[i].p.lista)
			d->par[i].ancho_maq = -1;
}

int ed_deshacer(editor *e)
{
	if (!e->hechos)
		return 0;
	e->hechos--;
	cambia_paso(e, &e->pasos[e->hechos]);
	return 1;
}

int ed_rehacer(editor *e)
{
	if (e->hechos >= e->n_pasos)
		return 0;
	cambia_paso(e, &e->pasos[e->hechos]);
	e->hechos++;
	return 1;
}

int ed_se_deshace(const editor *e)
{
	return e->hechos > 0;
}

int ed_se_rehace(const editor *e)
{
	return e->hechos < e->n_pasos;
}

/* ¿el tecleo sigue el paso de antes? (así una palabra entera se deshace de una vez) */
static int sigue_tecleo(editor *e, int clase)
{
	paso *s;

	if (!e->hechos || e->hechos != e->n_pasos)
		return 0;
	s = &e->pasos[e->hechos - 1];
	return s->clase == clase && s->version == e->d->version && s->n_ahora == 1 && s->p0 == e->cur.p &&
	       pos_igual(e->cur, e->ancla) && e->d->par[e->cur.p].n < 4000;
}

/* (hay que volver a maquetar las listas que siguen al párrafo i: sus números) */
static void listas_detras(documento *d, u32 i)
{
	u32 k;

	for (k = i; k < d->n_par && k < i + 400; k++) {
		if (d->par[k].p.lista)
			d->par[k].ancho_maq = -1;
		else if (d->par[k].n && k > i + 1)
			break;
	}
}

/* ---- borrar lo elegido (sin abrir paso: lo abre quien llama) ---- */
static void borra_rango(editor *e, pos a, pos b)
{
	documento *d = e->d;
	u32 i, hay_tabla = 0;

	if (!pos_menor(a, b))
		return;
	for (i = a.p; i <= b.p; i++)
		if (d->par[i].p.tabla)
			hay_tabla = 1;
	if (a.p == b.p) {
		par_quita(&d->par[a.p], a.o, b.o);
		par_cambia(d, &d->par[a.p]);
	} else if (!hay_tabla) {
		parrafo *pa = &d->par[a.p], *pb = &d->par[b.p];

		par_quita(pa, a.o, pa->n);
		if (!par_inserta(pa, pa->n, pb->t + b.o, pb->f + b.o, pb->n - b.o))
			d->sin_memoria = 1;
		par_cambia(d, pa);
		doc_quita_parrafos(d, a.p + 1, b.p - a.p);
	} else {
		/* con tablas: las tablas enteras se quitan; de las celdas tocadas
		 * se quita el texto; los párrafos de fuera, como siempre */
		u32 k = a.p;

		while (k <= b.p && k < d->n_par) {
			parrafo *p = &d->par[k];
			u32 o0 = k == a.p ? a.o : 0, o1 = k == b.p ? b.o : p->n;

			if (p->p.tabla) {
				u32 n, ini = doc_tabla_ini(d, k, &n);

				if (ini >= a.p && (ini > a.p || a.o == 0) && ini + n - 1 <= b.p &&
				    (ini + n - 1 < b.p || b.o >= d->par[ini + n - 1].n) && !(ini == a.p && ini + n - 1 == b.p)) {
					d->tab[p->p.tabla - 1].viva = 0;
					doc_quita_parrafos(d, ini, n);
					b.p -= n;
					if (a.p > ini)
						a.p = ini;
					k = ini;
					continue;
				}
				par_quita(p, o0, o1);
				par_cambia(d, p);
				k++;
				continue;
			}
			if (k != a.p && o0 == 0 && o1 == p->n && k != b.p) {
				doc_quita_parrafos(d, k, 1);
				b.p--;
				continue;
			}
			par_quita(p, o0, o1);
			par_cambia(d, p);
			k++;
		}
		if (a.p < d->n_par && b.p < d->n_par && a.p != b.p && !d->par[a.p].p.tabla && !d->par[b.p].p.tabla && b.p == a.p + 1) {
			parrafo *pa = &d->par[a.p], *pb = &d->par[b.p];

			par_inserta(pa, pa->n, pb->t, pb->f, pb->n);
			par_cambia(d, pa);
			doc_quita_parrafos(d, b.p, 1);
		}
		if (!d->n_par)
			doc_inserta_parrafos(d, 0, 1);
	}
	e->cur = e->ancla = a;
	ajusta(d, &e->cur);
	ajusta(d, &e->ancla);
	listas_detras(d, a.p);
}

void ed_borra_eleccion(editor *e)
{
	pos a, b;

	if (!ed_eleccion(e, &a, &b))
		return;
	ed_empieza(e, a.p, b.p + 1, PASO_BORRA | PASO_TABLAS);
	{
		u32 n0 = e->d->n_par;

		borra_rango(e, a, b);
		ed_acaba(e, b.p + 1 - a.p - (n0 - e->d->n_par));
	}
}

/* ---- escribir ---- */
static u16 fmt_para_escribir(editor *e)
{
	if (e->hay_pendiente)
		return doc_fmt(e->d, &e->pendiente);
	return par_fmt_en(e->d, &e->d->par[e->cur.p], e->cur.o);
}

void ed_escribe(editor *e, const u32 *t, u32 n)
{
	documento *d = e->d;
	pos a, b;
	u16 f;
	u16 tmp[256];
	u32 k;

	if (!n)
		return;
	if (ed_eleccion(e, &a, &b)) {
		u16 f0 = par_fmt_en(d, &d->par[a.p], a.o + 1 <= d->par[a.p].n ? a.o + 1 : a.o);
		u32 n0 = d->n_par;

		ed_empieza(e, a.p, b.p + 1, PASO_OTRO | PASO_TABLAS);
		borra_rango(e, a, b);
		f = e->hay_pendiente ? doc_fmt(d, &e->pendiente) : f0;
		for (k = 0; k < n;) {
			u32 m = n - k > 256 ? 256 : n - k, j;

			for (j = 0; j < m; j++)
				tmp[j] = f;
			if (!par_inserta(&d->par[e->cur.p], e->cur.o, t + k, tmp, m)) {
				d->sin_memoria = 1;
				break;
			}
			e->cur.o += m;
			k += m;
		}
		e->ancla = e->cur;
		par_cambia(d, &d->par[e->cur.p]);
		ed_acaba(e, b.p + 1 - a.p - (n0 - d->n_par));
		return;
	}
	f = fmt_para_escribir(e);
	if (!sigue_tecleo(e, PASO_TECLEO) || (n == 1 && t[0] == ' ' && e->cur.o && d->par[e->cur.p].t[e->cur.o - 1] != ' ' && 0))
		ed_empieza(e, e->cur.p, e->cur.p + 1, PASO_TECLEO);
	else {
		/* (se sigue el paso de antes: se reabre) */
		e->hechos--;
		e->n_pasos--;
		e->abierto = 1;
	}
	for (k = 0; k < n;) {
		u32 m = n - k > 256 ? 256 : n - k, j;

		for (j = 0; j < m; j++)
			tmp[j] = f;
		if (!par_inserta(&d->par[e->cur.p], e->cur.o, t + k, tmp, m)) {
			d->sin_memoria = 1;
			break;
		}
		e->cur.o += m;
		k += m;
	}
	e->ancla = e->cur;
	par_cambia(d, &d->par[e->cur.p]);
	ed_acaba(e, 1);
	/* (una palabra por paso: un espacio cierra el tecleo) */
	if (n == 1 && (t[0] == ' ' || t[0] == '.' || t[0] == ','))
		e->pasos[e->hechos - 1].clase = PASO_OTRO;
}

void ed_escribe_letra(editor *e, u32 c)
{
	ed_escribe(e, &c, 1);
}

void ed_intro(editor *e)
{
	documento *d = e->d;
	parrafo *p, *q;
	u32 i;
	pos a, b;

	if (ed_eleccion(e, &a, &b))
		ed_borra_eleccion(e);
	p = &d->par[e->cur.p];
	if (p->p.tabla) {
		/* en una celda: un salto de línea (la celda es un párrafo) */
		ed_salto_linea(e);
		return;
	}
	/* Intro en un elemento de lista vacío: se acaba la lista */
	if (p->p.lista && p->n == 0) {
		fmt_parrafo v = p->p;

		v.lista = LISTA_NO;
		v.sang_izq = 0;
		v.sang_primera = 0;
		ed_formato_parrafo(e, CP_LISTA | CP_SANG_IZQ | CP_SANG_PRIMERA, &v);
		return;
	}
	i = e->cur.p;
	ed_empieza(e, i, i + 1, PASO_OTRO);
	if (!doc_inserta_parrafos(d, i + 1, 1)) {
		ed_acaba(e, 1);
		return;
	}
	p = &d->par[i];
	q = &d->par[i + 1];
	q->p = p->p;
	q->p.salto_antes = 0;
	if (!par_inserta(q, 0, p->t + e->cur.o, p->f + e->cur.o, p->n - e->cur.o))
		d->sin_memoria = 1;
	par_quita(p, e->cur.o, p->n);
	/* al final de un título: el párrafo de después, con el estilo siguiente */
	if (q->n == 0 && p->p.estilo < d->n_est && d->est[p->p.estilo].siguiente != p->p.estilo) {
		u16 s = d->est[p->p.estilo].siguiente;

		if (s < d->n_est) {
			fmt_parrafo np = d->est[s].p;

			np.lista = p->p.lista;
			np.nivel = p->p.nivel;
			np.lista_id = p->p.lista_id;
			if (p->p.lista) {
				np.sang_izq = p->p.sang_izq;
				np.sang_primera = p->p.sang_primera;
			}
			np.estilo = s;
			q->p = np;
			if (e->hay_pendiente)
				e->hay_pendiente = 0;
		}
	}
	q->p.inicio_lista = 0;
	if (q->n == 0 && p->n > 0 && !e->hay_pendiente && q->p.estilo == p->p.estilo) {
		/* (lo que se escriba, con el formato del final del otro) */
		e->pendiente = d->fl[p->f[p->n - 1]];
		e->hay_pendiente = 1;
	} else if (q->n == 0 && q->p.estilo != p->p.estilo)
		e->hay_pendiente = 0;
	par_cambia(d, p);
	par_cambia(d, q);
	{
		int pend = e->hay_pendiente;
		fmt_letra pf = e->pendiente;

		e->cur = e->ancla = pos_de(i + 1, 0);
		e->hay_pendiente = pend;
		e->pendiente = pf;
	}
	listas_detras(d, i);
	ed_acaba(e, 2);
}

void ed_salto_linea(editor *e)
{
	u32 c = SALTO_LINEA;

	ed_escribe(e, &c, 1);
	e->pasos[e->hechos - 1].clase = PASO_OTRO;
}

void ed_salto_pagina(editor *e)
{
	documento *d = e->d;
	u32 i;

	if (d->par[e->cur.p].p.tabla)
		return;
	ed_intro(e);
	i = e->cur.p;
	ed_empieza(e, i, i + 1, PASO_OTRO);
	d->par[i].p.salto_antes = 1;
	par_cambia(d, &d->par[i]);
	ed_acaba(e, 1);
}

/* ---- moverse ---- */
static int es_de_palabra(u32 c)
{
	return es_letra(c) || c == '_' || c == '\'' || c == 0x2019;
}

pos ed_izquierda(const editor *e, pos c, int palabra)
{
	const documento *d = e->d;

	if (c.o == 0) {
		if (c.p > 0) {
			c.p--;
			c.o = d->par[c.p].n;
		}
		return c;
	}
	if (!palabra) {
		c.o--;
		return c;
	}
	{
		const u32 *t = d->par[c.p].t;

		while (c.o > 0 && !es_de_palabra(t[c.o - 1]))
			c.o--;
		while (c.o > 0 && es_de_palabra(t[c.o - 1]))
			c.o--;
	}
	return c;
}

pos ed_derecha(const editor *e, pos c, int palabra)
{
	const documento *d = e->d;
	const parrafo *p = &d->par[c.p];

	if (c.o >= p->n) {
		if (c.p + 1 < d->n_par) {
			c.p++;
			c.o = 0;
		}
		return c;
	}
	if (!palabra) {
		c.o++;
		return c;
	}
	while (c.o < p->n && es_de_palabra(p->t[c.o]))
		c.o++;
	while (c.o < p->n && !es_de_palabra(p->t[c.o]))
		c.o++;
	return c;
}

void ed_palabra(const editor *e, pos c, pos *a, pos *b)
{
	const parrafo *p = &e->d->par[c.p];
	u32 i = c.o, j = c.o;

	if (i < p->n && !es_de_palabra(p->t[i]) && i > 0 && es_de_palabra(p->t[i - 1]))
		i--, j--;
	if (i < p->n && es_de_palabra(p->t[i])) {
		while (i > 0 && es_de_palabra(p->t[i - 1]))
			i--;
		while (j < p->n && es_de_palabra(p->t[j]))
			j++;
		/* (y los espacios de detrás, como en todos los procesadores) */
		while (j < p->n && p->t[j] == ' ')
			j++;
	} else if (j < p->n)
		j++;
	*a = pos_de(c.p, i);
	*b = pos_de(c.p, j);
}

/* ---- borrar ---- */
void ed_retroceso(editor *e, int palabra)
{
	documento *d = e->d;
	pos a, b;

	if (ed_eleccion(e, &a, &b)) {
		ed_borra_eleccion(e);
		return;
	}
	if (e->cur.o == 0) {
		parrafo *p = &d->par[e->cur.p];

		/* al principio de un elemento de lista: primero se quita la lista */
		if (p->p.lista) {
			fmt_parrafo v = p->p;

			v.lista = LISTA_NO;
			v.sang_primera = 0;
			ed_formato_parrafo(e, CP_LISTA | CP_SANG_PRIMERA, &v);
			return;
		}
		if (e->cur.p == 0 || p->p.tabla || d->par[e->cur.p - 1].p.tabla) {
			/* (en una celda, o detrás de una tabla: nada que juntar) */
			if (e->cur.p > 0 && !p->p.tabla && d->par[e->cur.p - 1].p.tabla && p->n == 0) {
				/* un párrafo vacío detrás de una tabla: se va al final de la última celda */
				ed_cursor(e, pos_de(e->cur.p - 1, d->par[e->cur.p - 1].n), 0);
			}
			return;
		}
		/* juntar con el de antes */
		{
			u32 i = e->cur.p - 1;
			parrafo *pa = &d->par[i];
			u32 o = pa->n;

			ed_empieza(e, i, i + 2, PASO_OTRO);
			if (pa->n == 0) {
				/* (el de antes está vacío: se queda el formato de este) */
				fmt_parrafo fp = p->p;

				fp.salto_antes = pa->p.salto_antes;
				pa->p = fp;
			}
			if (!par_inserta(pa, pa->n, p->t, p->f, p->n))
				d->sin_memoria = 1;
			par_cambia(d, pa);
			doc_quita_parrafos(d, i + 1, 1);
			e->cur = e->ancla = pos_de(i, o);
			listas_detras(d, i);
			ed_acaba(e, 1);
		}
		return;
	}
	a = ed_izquierda(e, e->cur, palabra);
	if (!sigue_tecleo(e, PASO_BORRA))
		ed_empieza(e, e->cur.p, e->cur.p + 1, PASO_BORRA);
	else {
		e->hechos--;
		e->n_pasos--;
		e->abierto = 1;
	}
	par_quita(&d->par[e->cur.p], a.o, e->cur.o);
	par_cambia(d, &d->par[e->cur.p]);
	e->cur = e->ancla = a;
	ed_acaba(e, 1);
}

void ed_suprime(editor *e, int palabra)
{
	documento *d = e->d;
	pos a, b;
	parrafo *p;

	if (ed_eleccion(e, &a, &b)) {
		ed_borra_eleccion(e);
		return;
	}
	p = &d->par[e->cur.p];
	if (e->cur.o >= p->n) {
		u32 i = e->cur.p;

		if (i + 1 >= d->n_par || p->p.tabla || d->par[i + 1].p.tabla)
			return;
		ed_empieza(e, i, i + 2, PASO_OTRO);
		if (!par_inserta(p, p->n, d->par[i + 1].t, d->par[i + 1].f, d->par[i + 1].n))
			d->sin_memoria = 1;
		par_cambia(d, p);
		doc_quita_parrafos(d, i + 1, 1);
		listas_detras(d, i);
		ed_acaba(e, 1);
		return;
	}
	b = ed_derecha(e, e->cur, palabra);
	if (b.p != e->cur.p)
		b = pos_de(e->cur.p, p->n);
	ed_empieza(e, e->cur.p, e->cur.p + 1, PASO_OTRO);
	par_quita(p, e->cur.o, b.o);
	par_cambia(d, p);
	ed_acaba(e, 1);
}

/* ---- el formato de la letra ---- */
static void cambia_fmt(fmt_letra *f, u32 m, const fmt_letra *v)
{
	if (m & CF_FAMILIA)
		copia(f->familia, v->familia, sizeof f->familia);
	if (m & CF_TAM)
		f->tam = v->tam;
	if (m & CF_NEGRITA)
		f->negrita = v->negrita;
	if (m & CF_CURSIVA)
		f->cursiva = v->cursiva;
	if (m & CF_SUBRAYADO)
		f->subrayado = v->subrayado;
	if (m & CF_TACHADO)
		f->tachado = v->tachado;
	if (m & CF_COLOR)
		f->color = v->color;
	if (m & CF_RESALTE)
		f->resalte = v->resalte;
	if (m & CF_VERT)
		f->vert = v->vert;
	if (m & CF_VERSALITAS)
		f->versalitas = v->versalitas;
}

fmt_letra ed_formato_actual(editor *e)
{
	documento *d = e->d;
	pos a, b;

	if (e->hay_pendiente)
		return e->pendiente;
	if (ed_eleccion(e, &a, &b)) {
		const parrafo *p = &d->par[a.p];

		if (a.o < p->n)
			return d->fl[p->f[a.o]];
	}
	return d->fl[par_fmt_en(d, &d->par[e->cur.p], e->cur.o)];
}

void ed_formato(editor *e, u32 m, const fmt_letra *v)
{
	documento *d = e->d;
	pos a, b;
	u32 i, k;
	u16 viejo = 0xFFFF, nuevo = 0;

	if (!ed_eleccion(e, &a, &b)) {
		/* sin nada elegido: para lo que se escriba */
		fmt_letra f = ed_formato_actual(e);

		cambia_fmt(&f, m, v);
		e->pendiente = f;
		e->hay_pendiente = 1;
		return;
	}
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO);
	for (i = a.p; i <= b.p; i++) {
		parrafo *p = &d->par[i];
		u32 o0 = i == a.p ? a.o : 0, o1 = i == b.p ? b.o : p->n;

		for (k = o0; k < o1; k++) {
			if (p->f[k] != viejo) {
				fmt_letra f = d->fl[p->f[k]];

				viejo = p->f[k];
				cambia_fmt(&f, m, v);
				nuevo = doc_fmt(d, &f);
			}
			p->f[k] = nuevo;
		}
		par_cambia(d, p);
	}
	ed_acaba(e, b.p + 1 - a.p);
}

void ed_copia_formato(editor *e, const fmt_letra *f, const fmt_parrafo *pp)
{
	pos a, b;

	if (!ed_eleccion(e, &a, &b))
		return;
	ed_formato(e, CF_TODO, f);
	if (pp)
		ed_formato_parrafo(e, CP_ALIN | CP_SANG_IZQ | CP_SANG_DER | CP_SANG_PRIMERA | CP_ANTES | CP_DESPUES | CP_INTER, pp);
}

/* ---- el formato del párrafo ---- */
static void cambia_par(fmt_parrafo *p, u32 m, const fmt_parrafo *v)
{
	if (m & CP_ALIN)
		p->alin = v->alin;
	if (m & CP_SANG_IZQ)
		p->sang_izq = v->sang_izq;
	if (m & CP_SANG_DER)
		p->sang_der = v->sang_der;
	if (m & CP_SANG_PRIMERA)
		p->sang_primera = v->sang_primera;
	if (m & CP_ANTES)
		p->antes = v->antes;
	if (m & CP_DESPUES)
		p->despues = v->despues;
	if (m & CP_INTER) {
		p->inter_tipo = v->inter_tipo;
		p->inter = v->inter;
	}
	if (m & CP_TABS) {
		p->n_tabs = v->n_tabs;
		memcpy(p->tabs, v->tabs, sizeof p->tabs);
	}
	if (m & CP_SALTO)
		p->salto_antes = v->salto_antes;
	if (m & CP_JUNTOS)
		p->juntos = v->juntos;
	if (m & CP_LISTA) {
		p->lista = v->lista;
		p->nivel = v->nivel;
		p->lista_id = v->lista_id;
		p->inicio_lista = v->inicio_lista;
	}
	if (m & CP_FONDO)
		p->fondo = v->fondo;
}

void ed_formato_parrafo(editor *e, u32 m, const fmt_parrafo *v)
{
	documento *d = e->d;
	pos a, b;
	u32 i;

	ed_eleccion(e, &a, &b);
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO);
	for (i = a.p; i <= b.p; i++) {
		cambia_par(&d->par[i].p, m, v);
		par_cambia(d, &d->par[i]);
	}
	listas_detras(d, a.p);
	ed_acaba(e, b.p + 1 - a.p);
}

void ed_estilo(editor *e, int est)
{
	documento *d = e->d;
	pos a, b;
	u32 i, k;
	u16 f;

	if (est < 0 || (u32)est >= d->n_est)
		return;
	ed_eleccion(e, &a, &b);
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO);
	f = doc_fmt(d, &d->est[est].letra);
	for (i = a.p; i <= b.p; i++) {
		parrafo *p = &d->par[i];
		fmt_parrafo n = d->est[est].p;
		const estilo *viejo = p->p.estilo < d->n_est ? &d->est[p->p.estilo] : NULL;

		/* lo de la tabla y la lista se queda */
		n.tabla = p->p.tabla;
		n.fila = p->p.fila;
		n.col = p->p.col;
		n.abarca_cols = p->p.abarca_cols;
		n.abarca_filas = p->p.abarca_filas;
		n.cubierta = p->p.cubierta;
		n.fondo = p->p.fondo;
		n.salto_antes = p->p.salto_antes;
		if (p->p.lista && !n.lista) {
			n.lista = p->p.lista;
			n.nivel = p->p.nivel;
			n.lista_id = p->p.lista_id;
			n.sang_izq = p->p.sang_izq;
			n.sang_primera = p->p.sang_primera;
		}
		n.estilo = (u16)est;
		p->p = n;
		/* las letras: las del estilo, con la negrita, la cursiva y el
		 * subrayado puestos a mano (los que no eran del estilo de antes) */
		for (k = 0; k < p->n; k++) {
			const fmt_letra *o = &d->fl[p->f[k]];
			fmt_letra nf = d->est[est].letra;

			if (viejo) {
				if (o->negrita != viejo->letra.negrita)
					nf.negrita = o->negrita;
				if (o->cursiva != viejo->letra.cursiva)
					nf.cursiva = o->cursiva;
				if (o->subrayado != viejo->letra.subrayado)
					nf.subrayado = o->subrayado;
				nf.vert = o->vert;
			}
			p->f[k] = fmt_iguales(&nf, &d->est[est].letra) ? f : doc_fmt(d, &nf);
		}
		par_cambia(d, p);
	}
	e->hay_pendiente = 0;
	ed_acaba(e, b.p + 1 - a.p);
}

void ed_lista(editor *e, int tipo)
{
	documento *d = e->d;
	pos a, b;
	fmt_parrafo v;
	u32 m = CP_LISTA | CP_SANG_IZQ | CP_SANG_PRIMERA;

	ed_eleccion(e, &a, &b);
	v = d->par[a.p].p;
	if (v.lista == tipo) {
		v.lista = LISTA_NO;
		v.sang_izq = 0;
		v.sang_primera = 0;
	} else {
		/* la misma lista que la de antes, si es del mismo tipo */
		u16 id = 0;

		if (a.p > 0 && d->par[a.p - 1].p.lista == tipo)
			id = d->par[a.p - 1].p.lista_id;
		else {
			u32 i;

			for (i = 0; i < d->n_par; i++)
				if (d->par[i].p.lista_id >= id)
					id = (u16)(d->par[i].p.lista_id + 1);
		}
		v.lista = (u8)tipo;
		v.lista_id = id;
		v.inicio_lista = 0;
		if (!d->par[a.p].p.lista) {
			v.nivel = 0;
			v.sang_izq = 720;
			v.sang_primera = -360;
		}
	}
	ed_formato_parrafo(e, m, &v);
}

void ed_sangria(editor *e, int mas)
{
	documento *d = e->d;
	pos a, b;
	u32 i;

	ed_eleccion(e, &a, &b);
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO);
	for (i = a.p; i <= b.p; i++) {
		parrafo *p = &d->par[i];

		if (p->p.lista) {
			/* en una lista: el nivel */
			if (mas && p->p.nivel < 8) {
				p->p.nivel++;
				p->p.sang_izq += 360;
			} else if (!mas && p->p.nivel > 0) {
				p->p.nivel--;
				p->p.sang_izq -= 360;
			}
		} else {
			i32 s = p->p.sang_izq + (mas ? TAB_DEFECTO : -TAB_DEFECTO);

			s = s / TAB_DEFECTO * TAB_DEFECTO;
			if (s < 0)
				s = 0;
			if (s > doc_ancho_texto(d) - 1440)
				s = p->p.sang_izq;
			p->p.sang_izq = s;
		}
		par_cambia(d, p);
	}
	listas_detras(d, a.p);
	ed_acaba(e, b.p + 1 - a.p);
}

void ed_mayusculas(editor *e, int modo)
{
	documento *d = e->d;
	pos a, b;
	u32 i, k;

	if (!ed_eleccion(e, &a, &b)) {
		ed_palabra(e, e->cur, &a, &b);
		if (!pos_menor(a, b))
			return;
	}
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO);
	for (i = a.p; i <= b.p; i++) {
		parrafo *p = &d->par[i];
		u32 o0 = i == a.p ? a.o : 0, o1 = i == b.p ? b.o : p->n;
		int ini_palabra = 1, ini_frase = (o0 == 0);

		if (o0 > 0) {
			u32 q = o0;

			while (q > 0 && p->t[q - 1] == ' ')
				q--;
			ini_frase = q == 0 || p->t[q - 1] == '.' || p->t[q - 1] == '?' || p->t[q - 1] == '!';
			ini_palabra = !es_de_palabra(p->t[o0 - 1]);
		}
		for (k = o0; k < o1; k++) {
			u32 c = p->t[k];

			if (es_objeto(c))
				continue;
			switch (modo) {
			case 0: p->t[k] = minuscula(c); break;
			case 1: p->t[k] = mayuscula(c); break;
			case 2: p->t[k] = ini_frase && es_letra(c) ? mayuscula(c) : minuscula(c); break;
			default: p->t[k] = ini_palabra ? mayuscula(c) : minuscula(c); break;
			}
			if (es_letra(c))
				ini_frase = 0;
			if (c == '.' || c == '?' || c == '!' || c == 0xBF || c == 0xA1)
				ini_frase = 1;
			ini_palabra = !es_de_palabra(c);
		}
		par_cambia(d, p);
	}
	e->ancla = a;
	e->cur = b;
	ed_acaba(e, b.p + 1 - a.p);
}

/* ---- las tablas ---- */
void ed_tabla(editor *e, int filas, int cols)
{
	documento *d = e->d;
	u32 i, f, c, n;
	int t;
	u16 fmt_c;

	if (filas < 1 || cols < 1 || filas > 500 || cols > MAX_COLS)
		return;
	if (d->par[e->cur.p].p.tabla)
		return;		/* (tablas dentro de tablas, no) */
	ed_borra_eleccion(e);
	i = e->cur.p;
	ed_empieza(e, i, i + 1, PASO_OTRO | PASO_TABLAS);
	t = doc_nueva_tabla(d, (u16)filas, (u16)cols, doc_ancho_texto(d));
	if (t < 0) {
		ed_acaba(e, 1);
		return;
	}
	/* el párrafo se parte donde está el cursor (si no está al principio:
	 * entonces la tabla va delante, y él se queda detrás para seguir) */
	if (e->cur.o > 0) {
		parrafo *p;

		if (!doc_inserta_parrafos(d, i + 1, 1)) {
			ed_acaba(e, 1);
			return;
		}
		p = &d->par[i];
		d->par[i + 1].p = p->p;
		d->par[i + 1].p.salto_antes = 0;
		par_inserta(&d->par[i + 1], 0, p->t + e->cur.o, p->f + e->cur.o, p->n - e->cur.o);
		par_quita(p, e->cur.o, p->n);
		par_cambia(d, p);
		par_cambia(d, &d->par[i + 1]);
		n = 2;
		/* (un párrafo vacío delante de la tabla, si estaba vacío: la tabla va en su sitio) */
		if (d->par[i].n == 0 && e->cur.o == 0 && i > 0) {
			;
		}
		i++;
	} else
		n = 1;
	fmt_c = par_fmt_en(d, &d->par[i], 0);
	if (!doc_inserta_parrafos(d, i, (u32)(filas * cols))) {
		ed_acaba(e, n);
		return;
	}
	for (f = 0; f < (u32)filas; f++)
		for (c = 0; c < (u32)cols; c++) {
			parrafo *p = &d->par[i + f * (u32)cols + c];

			p->p = d->est[EST_SIN_ESPACIO].p;
			p->p.estilo = EST_SIN_ESPACIO;
			p->p.tabla = (u16)(t + 1);
			p->p.fila = (u16)f;
			p->p.col = (u16)c;
			(void)fmt_c;
		}
	e->cur = e->ancla = pos_de(i, 0);
	ed_acaba(e, n + (u32)(filas * cols));
}

/* la tabla del cursor: su número (desde 0), su primer párrafo y cuántos; -1 si no hay */
static int tabla_cursor(editor *e, u32 *ini, u32 *n)
{
	parrafo *p = &e->d->par[e->cur.p];

	if (!p->p.tabla)
		return -1;
	*ini = doc_tabla_ini(e->d, e->cur.p, n);
	return p->p.tabla - 1;
}

/* rehace los números de fila y columna de una tabla por su orden */
static void numera_tabla(documento *d, u32 ini, u32 n, u32 cols)
{
	u32 k;

	for (k = 0; k < n; k++) {
		d->par[ini + k].p.fila = (u16)(k / cols);
		d->par[ini + k].p.col = (u16)(k % cols);
		d->par[ini + k].ancho_maq = -1;
	}
}

void ed_fila(editor *e, int debajo)
{
	documento *d = e->d;
	u32 ini, n, cols, f, k, en;
	int t = tabla_cursor(e, &ini, &n);
	parrafo *modelo;

	if (t < 0)
		return;
	cols = d->tab[t].cols;
	f = d->par[e->cur.p].p.fila;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	en = ini + (f + (debajo ? 1 : 0)) * cols;
	if (!doc_inserta_parrafos(d, en, cols)) {
		ed_acaba(e, n);
		return;
	}
	modelo = &d->par[debajo ? en - cols : en + cols];
	for (k = 0; k < cols; k++) {
		d->par[en + k].p = modelo[k].p;
		d->par[en + k].p.abarca_filas = 0;
		d->par[en + k].p.cubierta = 0;
	}
	d->tab[t].filas++;
	numera_tabla(d, ini, n + cols, cols);
	e->cur = e->ancla = pos_de(en + d->par[e->cur.p < en ? e->cur.p : e->cur.p].p.col * 0, 0);
	e->cur = e->ancla = pos_de(en, 0);
	ed_acaba(e, n + cols);
}

void ed_quita_fila(editor *e)
{
	documento *d = e->d;
	u32 ini, n, cols, f;
	int t = tabla_cursor(e, &ini, &n);

	if (t < 0)
		return;
	cols = d->tab[t].cols;
	if (d->tab[t].filas <= 1) {
		ed_quita_tabla(e);
		return;
	}
	f = d->par[e->cur.p].p.fila;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	doc_quita_parrafos(d, ini + f * cols, cols);
	d->tab[t].filas--;
	numera_tabla(d, ini, n - cols, cols);
	{
		u32 k;

		/* (las uniones que pasaban por la fila, sin ella) */
		for (k = ini; k < ini + n - cols; k++) {
			d->par[k].p.cubierta = 0;
			d->par[k].p.abarca_cols = d->par[k].p.abarca_filas = 0;
		}
	}
	e->cur = e->ancla = pos_de(ini + (f < d->tab[t].filas ? f : f - 1) * cols, 0);
	ed_acaba(e, n - cols);
}

void ed_columna(editor *e, int derecha)
{
	documento *d = e->d;
	u32 ini, n, cols, c, f, filas;
	int t = tabla_cursor(e, &ini, &n);
	tabla *tb;

	if (t < 0)
		return;
	tb = &d->tab[t];
	cols = tb->cols;
	if (cols >= MAX_COLS)
		return;
	filas = n / cols;
	c = d->par[e->cur.p].p.col + (derecha ? 1 : 0);
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	for (f = filas; f-- > 0;) {
		u32 en = ini + f * cols + c;
		parrafo *q;

		if (!doc_inserta_parrafos(d, en, 1))
			break;
		q = &d->par[en];
		q->p = d->par[en + (c < cols ? 1 : -1) + (c < cols ? 0 : 0)].p;
		q->p.abarca_cols = q->p.abarca_filas = 0;
		q->p.cubierta = 0;
	}
	/* los anchos: la columna nueva parte la de al lado */
	{
		u32 k, de = c < cols ? c : cols - 1;
		i32 w = tb->ancho[de] / 2;

		for (k = cols; k > c; k--)
			tb->ancho[k] = tb->ancho[k - 1];
		tb->ancho[c] = w;
		tb->ancho[c == de ? c + 1 : de] -= w;
		if (tb->ancho[c == de ? c + 1 : de] < 400)
			tb->ancho[c == de ? c + 1 : de] = 400;
	}
	tb->cols++;
	numera_tabla(d, ini, n + filas, cols + 1);
	for (f = 0; f < filas * (cols + 1); f++) {
		d->par[ini + f].p.cubierta = 0;
		d->par[ini + f].p.abarca_cols = d->par[ini + f].p.abarca_filas = 0;
	}
	e->cur = e->ancla = pos_de(ini + c, 0);
	ed_acaba(e, n + filas);
}

void ed_quita_columna(editor *e)
{
	documento *d = e->d;
	u32 ini, n, cols, c, f, filas, k;
	int t = tabla_cursor(e, &ini, &n);
	tabla *tb;

	if (t < 0)
		return;
	tb = &d->tab[t];
	cols = tb->cols;
	if (cols <= 1) {
		ed_quita_tabla(e);
		return;
	}
	filas = n / cols;
	c = d->par[e->cur.p].p.col;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	for (f = filas; f-- > 0;)
		doc_quita_parrafos(d, ini + f * cols + c, 1);
	for (k = c; k + 1 < cols; k++)
		tb->ancho[k] = tb->ancho[k + 1];
	tb->cols--;
	numera_tabla(d, ini, n - filas, cols - 1);
	for (f = 0; f < filas * (cols - 1); f++) {
		d->par[ini + f].p.cubierta = 0;
		d->par[ini + f].p.abarca_cols = d->par[ini + f].p.abarca_filas = 0;
	}
	e->cur = e->ancla = pos_de(ini + (c < cols - 1 ? c : c - 1), 0);
	ed_acaba(e, n - filas);
}

void ed_quita_tabla(editor *e)
{
	documento *d = e->d;
	u32 ini, n;
	int t = tabla_cursor(e, &ini, &n);

	if (t < 0)
		return;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	d->tab[t].viva = 0;
	doc_quita_parrafos(d, ini, n);
	if (ini >= d->n_par) {
		doc_inserta_parrafos(d, d->n_par, 1);
		ed_acaba(e, 1);
		e->cur = e->ancla = pos_de(ini, 0);
		return;
	}
	e->cur = e->ancla = pos_de(ini, 0);
	ed_acaba(e, 0);
}

void ed_une_celdas(editor *e)
{
	documento *d = e->d;
	pos a, b;
	u32 ini, n, f0, f1, c0, c1, k, cols;
	int t;
	parrafo *ancla;

	if (!ed_eleccion(e, &a, &b))
		return;
	t = d->par[a.p].p.tabla;
	if (!t || d->par[b.p].p.tabla != t)
		return;
	t--;
	ini = doc_tabla_ini(d, a.p, &n);
	cols = d->tab[t].cols;
	f0 = d->par[a.p].p.fila;
	f1 = d->par[b.p].p.fila;
	c0 = d->par[a.p].p.col;
	c1 = d->par[b.p].p.col;
	if (c0 > c1) {
		u32 x = c0;

		c0 = c1;
		c1 = x;
	}
	if (f0 == f1 && c0 == c1)
		return;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	ancla = &d->par[ini + f0 * cols + c0];
	for (k = 0; k < n; k++) {
		parrafo *p = &d->par[ini + k];
		u32 f = p->p.fila, c = p->p.col;

		if (f < f0 || f > f1 || c < c0 || c > c1 || p == ancla)
			continue;
		/* su texto, a la celda que queda (detrás, en otra línea) */
		if (p->n) {
			if (ancla->n)
				par_inserta_uno(ancla, ancla->n, SALTO_LINEA, ancla->f[ancla->n - 1]);
			par_inserta(ancla, ancla->n, p->t, p->f, p->n);
			par_quita(p, 0, p->n);
		}
		p->p.cubierta = 1;
		p->p.abarca_cols = p->p.abarca_filas = 0;
		par_cambia(d, p);
	}
	ancla->p.abarca_cols = (u8)(c1 - c0 + 1);
	ancla->p.abarca_filas = (u8)(f1 - f0 + 1);
	ancla->p.cubierta = 0;
	par_cambia(d, ancla);
	e->cur = e->ancla = pos_de((u32)(ancla - d->par), 0);
	ed_acaba(e, n);
}

void ed_divide_celda(editor *e)
{
	documento *d = e->d;
	u32 ini, n, cols, k, f0, c0, nf, nc;
	int t = tabla_cursor(e, &ini, &n);
	parrafo *p;

	if (t < 0)
		return;
	p = &d->par[e->cur.p];
	if (p->p.abarca_cols <= 1 && p->p.abarca_filas <= 1)
		return;
	cols = d->tab[t].cols;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	f0 = p->p.fila;
	c0 = p->p.col;
	nf = p->p.abarca_filas ? p->p.abarca_filas : 1;
	nc = p->p.abarca_cols ? p->p.abarca_cols : 1;
	for (k = 0; k < n; k++) {
		parrafo *q = &d->par[ini + k];

		if (q->p.fila >= f0 && q->p.fila < f0 + nf && q->p.col >= c0 && q->p.col < c0 + nc) {
			q->p.cubierta = 0;
			par_cambia(d, q);
		}
	}
	p->p.abarca_cols = p->p.abarca_filas = 0;
	par_cambia(d, p);
	(void)cols;
	ed_acaba(e, n);
}

void ed_bordes(editor *e, int bordes)
{
	u32 ini, n;
	int t = tabla_cursor(e, &ini, &n);

	if (t < 0)
		return;
	ed_empieza(e, ini, ini + n, PASO_OTRO | PASO_TABLAS);
	e->d->tab[t].bordes = (u8)bordes;
	{
		u32 k;

		for (k = ini; k < ini + n; k++)
			e->d->par[k].ancho_maq = -1;
	}
	ed_acaba(e, n);
}

void ed_celda_siguiente(editor *e, int atras)
{
	documento *d = e->d;
	u32 ini, n, k;
	int t = tabla_cursor(e, &ini, &n);

	if (t < 0)
		return;
	k = e->cur.p;
	do {
		if (atras) {
			if (k == ini)
				return;
			k--;
		} else {
			k++;
			if (k >= ini + n) {
				/* al final: una fila nueva */
				e->cur = e->ancla = pos_de(ini + n - 1, 0);
				ed_fila(e, 1);
				return;
			}
		}
	} while (d->par[k].p.cubierta);
	/* toda la celda elegida (como en los demás procesadores) */
	e->ancla = pos_de(k, 0);
	e->cur = pos_de(k, d->par[k].n);
}

/* ---- las imágenes ---- */
void ed_imagen(editor *e, u8 *datos, u32 n)
{
	int i;
	u32 c;

	i = doc_nueva_imagen(e->d, datos, n);
	if (i < 0) {
		so_suelta(datos);
		return;
	}
	c = IMAGEN_0 + (u32)i;
	ed_escribe(e, &c, 1);
	e->pasos[e->hechos - 1].clase = PASO_OTRO;
}

int ed_imagen_elegida(const editor *e)
{
	pos a, b;
	const parrafo *p;

	if (!ed_eleccion(e, &a, &b) || a.p != b.p || b.o != a.o + 1)
		return -1;
	p = &e->d->par[a.p];
	if (!es_imagen(p->t[a.o]) || p->t[a.o] - IMAGEN_0 >= e->d->n_img)
		return -1;
	return (int)(p->t[a.o] - IMAGEN_0);
}

void ed_ajuste_imagen(editor *e, int ajuste)
{
	int i = ed_imagen_elegida(e);
	pos a, b;

	if (i < 0)
		return;
	ed_eleccion(e, &a, &b);
	/* (la imagen no está en los párrafos guardados: se cambia aquí, y el
	 * paso guarda su párrafo para que se rehaga la maqueta) */
	ed_empieza(e, a.p, a.p + 1, PASO_OTRO);
	e->d->img[i].ajuste = (u8)ajuste;
	par_cambia(e->d, &e->d->par[a.p]);
	ed_acaba(e, 1);
}

void ed_tamano_imagen(editor *e, i32 ancho, i32 alto)
{
	int i = ed_imagen_elegida(e);
	pos a, b;

	if (i < 0 || ancho < 100 || alto < 100)
		return;
	ed_eleccion(e, &a, &b);
	ed_empieza(e, a.p, a.p + 1, PASO_OTRO);
	e->d->img[i].ancho = ancho;
	e->d->img[i].alto = alto;
	par_cambia(e->d, &e->d->par[a.p]);
	ed_acaba(e, 1);
}

void ed_campo(editor *e, u32 campo)
{
	ed_escribe(e, &campo, 1);
	e->pasos[e->hechos - 1].clase = PASO_OTRO;
}

void ed_nota(editor *e)
{
	int n = doc_nueva_nota(e->d);
	u32 c;

	if (n < 0)
		return;
	c = NOTA_0 + (u32)n;
	ed_escribe(e, &c, 1);
	e->pasos[e->hechos - 1].clase = PASO_OTRO;
}

/* ---- copiar y pegar ---- */
static parrafo *frag;		/* lo copiado, con su formato (del mismo documento) */
static u32 n_frag;
static char *frag_txt;		/* su texto (para saber si lo que se pega es lo de aquí) */
static const documento *frag_doc;

static void suelta_frag(void)
{
	u32 i;

	for (i = 0; i < n_frag; i++)
		par_suelta(&frag[i]);
	so_suelta(frag);
	so_suelta(frag_txt);
	frag = NULL;
	frag_txt = NULL;
	n_frag = 0;
}

char *ed_copia(editor *e)
{
	documento *d = e->d;
	pos a, b;
	bufer t = { 0 };
	u32 i, k;

	if (!ed_eleccion(e, &a, &b))
		return NULL;
	suelta_frag();
	frag = so_pide((b.p - a.p + 1) * sizeof(parrafo));
	if (frag) {
		for (i = a.p; i <= b.p; i++) {
			parrafo *p = &d->par[i];
			u32 o0 = i == a.p ? a.o : 0, o1 = i == b.p ? b.o : p->n;

			memset(&frag[n_frag], 0, sizeof(parrafo));
			frag[n_frag].p = p->p;
			frag[n_frag].p.tabla = 0;
			frag[n_frag].p.cubierta = 0;
			frag[n_frag].p.abarca_cols = frag[n_frag].p.abarca_filas = 0;
			frag[n_frag].ancho_maq = -1;
			par_inserta(&frag[n_frag], 0, p->t + o0, p->f + o0, o1 - o0);
			n_frag++;
		}
	}
	frag_doc = d;
	for (i = a.p; i <= b.p; i++) {
		parrafo *p = &d->par[i];
		u32 o0 = i == a.p ? a.o : 0, o1 = i == b.p ? b.o : p->n;

		for (k = o0; k < o1; k++) {
			u32 c = p->t[k];

			if (c == SALTO_LINEA)
				buf_c(&t, '\n');
			else if (!es_objeto(c))
				buf_u32(&t, c);
		}
		if (i < b.p)
			buf_c(&t, p->p.tabla && i + 1 <= b.p && d->par[i + 1].p.tabla == p->p.tabla && d->par[i + 1].p.fila == p->p.fila ? '\t' : '\n');
	}
	buf_c(&t, 0);
	if (t.mal) {
		buf_suelta(&t);
		return NULL;
	}
	frag_txt = duplica((char *)t.d);
	return (char *)t.d;
}

void ed_corta(editor *e)
{
	char *t = ed_copia(e);

	so_suelta(t);
	ed_borra_eleccion(e);
}

/* mira si dos textos son el mismo sin mirar los finales de línea ni lo que no está en la 437 */
static int mismo_texto(const char *a, u32 na, const char *b)
{
	u32 i = 0, j = 0, nb = largo(b);

	while (i < na && j < nb) {
		u32 ca, cb;

		if (a[i] == '\r') {
			i++;
			continue;
		}
		if (b[j] == '\r') {
			j++;
			continue;
		}
		i += (u32)utf8_lee((const u8 *)a + i, na - i, &ca);
		j += (u32)utf8_lee((const u8 *)b + j, nb - j, &cb);
		if (ca != cb && a437(ca) != a437(cb))
			return 0;
	}
	while (i < na && (a[i] == '\r' || a[i] == '\n'))
		i++;
	while (j < nb && (b[j] == '\r' || b[j] == '\n'))
		j++;
	return i >= na && j >= nb;
}

void ed_pega(editor *e, const char *utf8, u32 n)
{
	documento *d = e->d;
	u32 i, n0;
	pos a, b;

	if (!utf8 || !n)
		return;
	ed_eleccion(e, &a, &b);
	n0 = d->n_par;
	ed_empieza(e, a.p, b.p + 1, PASO_OTRO | PASO_TABLAS);
	borra_rango(e, a, b);
	b.p = a.p + (d->n_par - n0) + (b.p - a.p);	/* (lo que queda del paso) */
	{
		u32 p0 = e->cur.p, quedan_antes = d->n_par;

		if (frag && frag_doc == d && frag_txt && mismo_texto(utf8, n, frag_txt) && !(n_frag > 1 && d->par[e->cur.p].p.tabla)) {
			/* lo copiado de aquí, con su formato */
			for (i = 0; i < n_frag; i++) {
				parrafo *p = &d->par[e->cur.p];

				if (!par_inserta(p, e->cur.o, frag[i].t, frag[i].f, frag[i].n)) {
					d->sin_memoria = 1;
					break;
				}
				e->cur.o += frag[i].n;
				if (n_frag > 1 && i == 0 && e->cur.o == frag[i].n)
					p->p = frag[0].p, p->p.tabla = 0;
				par_cambia(d, p);
				if (i + 1 < n_frag) {
					/* partir aquí */
					parrafo *q;

					if (!doc_inserta_parrafos(d, e->cur.p + 1, 1))
						break;
					p = &d->par[e->cur.p];
					q = &d->par[e->cur.p + 1];
					q->p = frag[i + 1].p;
					par_inserta(q, 0, p->t + e->cur.o, p->f + e->cur.o, p->n - e->cur.o);
					par_quita(p, e->cur.o, p->n);
					par_cambia(d, p);
					par_cambia(d, q);
					e->cur = pos_de(e->cur.p + 1, 0);
				}
			}
		} else {
			/* texto llano: con el formato de donde se pega */
			u16 f = fmt_para_escribir(e);
			u32 k = 0, c;

			while (k < n) {
				k += (u32)utf8_lee((const u8 *)utf8 + k, n - k, &c);
				if (c == '\r')
					continue;
				if (c == '\n') {
					parrafo *p = &d->par[e->cur.p], *q;

					if (p->p.tabla) {
						par_inserta_uno(p, e->cur.o++, SALTO_LINEA, f);
						continue;
					}
					if (!doc_inserta_parrafos(d, e->cur.p + 1, 1))
						break;
					p = &d->par[e->cur.p];
					q = &d->par[e->cur.p + 1];
					q->p = p->p;
					q->p.salto_antes = 0;
					par_inserta(q, 0, p->t + e->cur.o, p->f + e->cur.o, p->n - e->cur.o);
					par_quita(p, e->cur.o, p->n);
					par_cambia(d, p);
					par_cambia(d, q);
					e->cur = pos_de(e->cur.p + 1, 0);
					continue;
				}
				if (c < 0x20 && c != '\t')
					continue;
				if (!par_inserta_uno(&d->par[e->cur.p], e->cur.o, c, f)) {
					d->sin_memoria = 1;
					break;
				}
				e->cur.o++;
			}
			par_cambia(d, &d->par[e->cur.p]);
		}
		e->ancla = e->cur;
		listas_detras(d, p0);
		ed_acaba(e, (b.p + 1 - a.p) + (d->n_par - quedan_antes));
	}
}

/* ---- buscar ---- */
static u32 pliega(u32 c, int exacto)
{
	return exacto ? c : minuscula(sin_tilde(c));
}

static int casa(const parrafo *p, u32 o, const u32 *t, u32 n, int exacto)
{
	u32 k;

	if (o + n > p->n)
		return 0;
	for (k = 0; k < n; k++)
		if (pliega(p->t[o + k], exacto) != pliega(t[k], exacto))
			return 0;
	return 1;
}

int ed_busca(editor *e, const u32 *t, u32 n, int exacto, int atras)
{
	documento *d = e->d;
	pos a, b;
	u32 i, k, vueltas;

	if (!n)
		return 0;
	ed_eleccion(e, &a, &b);
	i = atras ? a.p : b.p;
	k = atras ? a.o : b.o;
	for (vueltas = 0; vueltas <= d->n_par; vueltas++) {
		parrafo *p = &d->par[i];

		if (!atras) {
			for (; k + n <= p->n; k++)
				if (casa(p, k, t, n, exacto)) {
					ed_elige(e, pos_de(i, k), pos_de(i, k + n));
					return 1;
				}
			i = (i + 1) % d->n_par;
			k = 0;
		} else {
			while (k-- > 0)
				if (casa(p, k, t, n, exacto)) {
					ed_elige(e, pos_de(i, k), pos_de(i, k + n));
					return 1;
				}
			i = i ? i - 1 : d->n_par - 1;
			k = d->par[i].n;
		}
	}
	return 0;
}

int ed_reemplaza(editor *e, const u32 *t, u32 n, const u32 *por, u32 m, int exacto)
{
	pos a, b;

	if (ed_eleccion(e, &a, &b) && a.p == b.p && b.o - a.o == n && casa(&e->d->par[a.p], a.o, t, n, exacto)) {
		if (m)
			ed_escribe(e, por, m);
		else
			ed_borra_eleccion(e);
		if (e->hechos)
			e->pasos[e->hechos - 1].clase = PASO_OTRO;
	}
	return ed_busca(e, t, n, exacto, 0);
}

int ed_reemplaza_todo(editor *e, const u32 *t, u32 n, const u32 *por, u32 m, int exacto)
{
	documento *d = e->d;
	u32 i, k, cuantos = 0;

	if (!n)
		return 0;
	ed_empieza(e, 0, d->n_par, PASO_OTRO);
	for (i = 0; i < d->n_par; i++) {
		parrafo *p = &d->par[i];
		int cambio = 0;

		for (k = 0; k + n <= p->n;) {
			if (casa(p, k, t, n, exacto)) {
				u16 f = p->f[k];
				u16 tmp[64];
				u32 j;

				par_quita(p, k, k + n);
				for (j = 0; j < m && j < 64; j++)
					tmp[j] = f;
				par_inserta(p, k, por, tmp, m < 64 ? m : 64);
				k += m < 64 ? m : 64;
				cuantos++;
				cambio = 1;
			} else
				k++;
		}
		if (cambio)
			par_cambia(d, p);
	}
	ajusta(d, &e->cur);
	ajusta(d, &e->ancla);
	ed_acaba(e, d->n_par);
	if (!cuantos) {
		/* (no ha cambiado nada: fuera el paso) */
		ed_deshacer(e);
		e->n_pasos = e->hechos;
	}
	return (int)cuantos;
}

/* ---- contar ---- */
void ed_cuenta(const documento *d, u32 *palabras, u32 *letras, u32 *sin_esp, u32 *parrafos)
{
	u32 i, k, pal = 0, le = 0, se = 0, pa = 0;

	for (i = 0; i < d->n_par; i++) {
		const parrafo *p = &d->par[i];
		int dentro = 0;

		if (p->n)
			pa++;
		for (k = 0; k < p->n; k++) {
			u32 c = p->t[k];

			if (es_objeto(c))
				continue;
			le++;
			if (!es_espacio(c) && c != SALTO_LINEA)
				se++;
			if (es_de_palabra(c)) {
				if (!dentro)
					pal++;
				dentro = 1;
			} else if (c != '-' || !dentro)
				dentro = 0;
		}
	}
	*palabras = pal;
	*letras = le;
	*sin_esp = se;
	*parrafos = pa;
}
