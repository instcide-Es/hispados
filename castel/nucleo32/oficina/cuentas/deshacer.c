/* deshacer.c - Cuentas 2: deshacer y rehacer sin límite (deshacer.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "deshacer.h"
#include "formula.h"
#include "calculo.h"

enum { DR_REGION, DR_META, DR_ALTOS, DR_LIBRO };

typedef struct {
	char nombre[96];
	u16 ancho[CU_COLS];
	u8 oculta_c[CU_COLS];
	u16 estilo_col[CU_COLS];
	u32 ancho_def, inm_f, inm_c;
	rango *comb;
	u32 n_comb;
	comentario *com;
	u32 n_com;
	condicional *cond;
	u32 n_cond;
	grafico *gr;
	u32 n_gr;
	int hay_filtro;
	rango filtro;
	filtro_col *fcol;
	u32 n_fcol;
	int hay_area;
	rango area;
	u32 *salto_f, *salto_c;
	u32 n_salto_f, n_salto_c;
	pagina pag;
	u8 sin_rejilla, oculta;
} d_meta;

typedef struct {
	u8 tipo;
	u16 hoja;
	rango r;
	celda *cel;		/* (región) */
	u32 n;
	d_meta *m;		/* (meta) */
	u16 *alto;		/* (altos) */
	u8 *oculta_f;
	u16 *estilo_fila;
	hoja **hs;		/* (libro) */
	u32 n_h, activa;
	nombre_def *nom;
	u32 n_nom;
} d_rec;

typedef struct {
	char nombre[64];
	d_rec *r;
	u32 n, cabe;
	u16 hoja;		/* dónde estaba el cursor */
	u32 cur_f, cur_c;
	rango sel;
	u32 memoria;
} paso;

typedef struct {
	paso *p;
	u32 n, cabe, pos;	/* [0, pos): se pueden deshacer; [pos, n): rehacer */
	int hondo;		/* (dh_empieza dentro de otro) */
	int haciendo;		/* (deshaciendo: no se apunta nada) */
	paso cur;
	int abierto;
} dh_t;

static dh_t *dh(libro *l)
{
	if (!l->deshacer) {
		dh_t *d = so_pide(sizeof(dh_t));

		if (!d)
			return NULL;
		memset(d, 0, sizeof *d);
		l->deshacer = d;
	}
	return l->deshacer;
}

/* ---- copiar celdas ---- */
static int copia_celda(celda *d, const celda *o)
{
	*d = *o;
	d->marca = 0;
	d->grado = 0;
	d->v.s = NULL;
	d->f = NULL;
	if (o->v.t == V_TXT) {
		d->v.s = duplica(o->v.s ? o->v.s : "");
		if (!d->v.s) {
			d->v.t = V_VACIO;
			return 1;
		}
	}
	if (o->f) {
		d->f = fm_copia(o->f);
		if (!d->f)
			return 1;
	}
	return 0;
}

/* las claves de las celdas que hay en r (de so_pide) */
static u32 *claves_en(hoja *h, rango r, u32 *n)
{
	u64 area = (u64)(r.f1 - r.f0 + 1) * (r.c1 - r.c0 + 1);
	u32 *k, m = 0, i;

	*n = 0;
	k = so_pide((h->n_cel ? h->n_cel : 1) * sizeof(u32));
	if (!k)
		return NULL;
	if (area < h->n_cel) {
		u32 f, c;

		for (f = r.f0; f <= r.f1; f++)
			for (c = r.c0; c <= r.c1; c++)
				if (hj_celda(h, f, c))
					k[m++] = CU_CLAVE(f, c);
	} else
		for (i = 0; i < h->n_cel; i++) {
			u32 x = h->cel[i].clave;

			if (rg_dentro(r, CU_FILA(x), CU_COL(x)))
				k[m++] = x;
		}
	*n = m;
	return k;
}

static d_rec *nuevo_rec(dh_t *d, int tipo)
{
	paso *p = &d->cur;
	d_rec *r;

	if (p->n >= p->cabe) {
		u32 c = p->cabe ? p->cabe * 2 : 8;
		d_rec *n = so_repide(p->r, c * sizeof(d_rec));

		if (!n)
			return NULL;
		p->r = n;
		p->cabe = c;
	}
	r = &p->r[p->n++];
	memset(r, 0, sizeof *r);
	r->tipo = (u8)tipo;
	return r;
}

static int grabando(libro *l)
{
	dh_t *d = dh(l);

	return d && d->abierto && !d->haciendo;
}

void dh_region(libro *l, hoja *h, rango r)
{
	dh_t *d = dh(l);
	d_rec *x;
	u32 n, *k, i;

	if (!grabando(l))
		return;
	x = nuevo_rec(d, DR_REGION);
	if (!x)
		return;
	x->hoja = h->id;
	x->r = r;
	k = claves_en(h, r, &n);
	if (!k)
		return;
	if (n) {
		x->cel = so_pide(n * sizeof(celda));
		if (x->cel)
			for (i = 0; i < n; i++) {
				const celda *c = hj_celda_k(h, k[i]);

				if (c && !copia_celda(&x->cel[x->n], c))
					x->n++;
			}
	}
	d->cur.memoria += n * (u32)sizeof(celda) + 32;
	so_suelta(k);
}

void dh_celda(libro *l, hoja *h, u32 f, u32 c)
{
	dh_region(l, h, cu_rango(f, c, f, c));
}

/* ---- lo de la hoja ---- */
static void *dup(const void *p, u32 n)
{
	void *q;

	if (!p || !n)
		return NULL;
	q = so_pide(n);
	if (q)
		memcpy(q, p, n);
	return q;
}

static d_meta *meta_de(const hoja *h)
{
	d_meta *m = so_pide(sizeof(d_meta));
	u32 i;

	if (!m)
		return NULL;
	memset(m, 0, sizeof *m);
	memcpy(m->nombre, h->nombre, sizeof m->nombre);
	memcpy(m->ancho, h->ancho, sizeof m->ancho);
	memcpy(m->oculta_c, h->oculta_c, sizeof m->oculta_c);
	memcpy(m->estilo_col, h->estilo_col, sizeof m->estilo_col);
	m->ancho_def = h->ancho_def;
	m->inm_f = h->inm_f;
	m->inm_c = h->inm_c;
	m->comb = dup(h->comb, h->n_comb * sizeof(rango));
	m->n_comb = m->comb ? h->n_comb : 0;
	m->com = dup(h->com, h->n_com * sizeof(comentario));
	m->n_com = m->com ? h->n_com : 0;
	for (i = 0; i < m->n_com; i++)
		m->com[i].texto = duplica(h->com[i].texto ? h->com[i].texto : "");
	m->cond = dup(h->cond, h->n_cond * sizeof(condicional));
	m->n_cond = m->cond ? h->n_cond : 0;
	m->gr = dup(h->gr, h->n_gr * sizeof(grafico));
	m->n_gr = m->gr ? h->n_gr : 0;
	m->hay_filtro = h->hay_filtro;
	m->filtro = h->filtro;
	m->fcol = dup(h->fcol, h->n_fcol * sizeof(filtro_col));
	m->n_fcol = m->fcol ? h->n_fcol : 0;
	m->hay_area = h->hay_area;
	m->area = h->area;
	m->salto_f = dup(h->salto_f, h->n_salto_f * sizeof(u32));
	m->n_salto_f = m->salto_f ? h->n_salto_f : 0;
	m->salto_c = dup(h->salto_c, h->n_salto_c * sizeof(u32));
	m->n_salto_c = m->salto_c ? h->n_salto_c : 0;
	m->pag = h->pag;
	m->sin_rejilla = h->sin_rejilla;
	m->oculta = h->oculta;
	return m;
}

static void meta_suelta(d_meta *m)
{
	u32 i;

	if (!m)
		return;
	so_suelta(m->comb);
	for (i = 0; i < m->n_com; i++)
		so_suelta(m->com[i].texto);
	so_suelta(m->com);
	so_suelta(m->cond);
	so_suelta(m->gr);
	so_suelta(m->fcol);
	so_suelta(m->salto_f);
	so_suelta(m->salto_c);
	so_suelta(m);
}

#define CAMBIA(a, b) do { __typeof__(a) t_ = (a); (a) = (b); (b) = t_; } while (0)

static void meta_cambia(hoja *h, d_meta *m)
{
	char t[96];
	u16 a[CU_COLS];
	u8 o[CU_COLS];

	memcpy(t, h->nombre, sizeof t);
	memcpy(h->nombre, m->nombre, sizeof t);
	memcpy(m->nombre, t, sizeof t);
	memcpy(a, h->ancho, sizeof a);
	memcpy(h->ancho, m->ancho, sizeof a);
	memcpy(m->ancho, a, sizeof a);
	memcpy(a, h->estilo_col, sizeof a);
	memcpy(h->estilo_col, m->estilo_col, sizeof a);
	memcpy(m->estilo_col, a, sizeof a);
	memcpy(o, h->oculta_c, sizeof o);
	memcpy(h->oculta_c, m->oculta_c, sizeof o);
	memcpy(m->oculta_c, o, sizeof o);
	CAMBIA(h->ancho_def, m->ancho_def);
	CAMBIA(h->inm_f, m->inm_f);
	CAMBIA(h->inm_c, m->inm_c);
	CAMBIA(h->comb, m->comb);
	CAMBIA(h->n_comb, m->n_comb);
	CAMBIA(h->com, m->com);
	CAMBIA(h->n_com, m->n_com);
	CAMBIA(h->cond, m->cond);
	CAMBIA(h->n_cond, m->n_cond);
	CAMBIA(h->gr, m->gr);
	CAMBIA(h->n_gr, m->n_gr);
	CAMBIA(h->hay_filtro, m->hay_filtro);
	CAMBIA(h->filtro, m->filtro);
	CAMBIA(h->fcol, m->fcol);
	CAMBIA(h->n_fcol, m->n_fcol);
	CAMBIA(h->hay_area, m->hay_area);
	CAMBIA(h->area, m->area);
	CAMBIA(h->salto_f, m->salto_f);
	CAMBIA(h->n_salto_f, m->n_salto_f);
	CAMBIA(h->salto_c, m->salto_c);
	CAMBIA(h->n_salto_c, m->n_salto_c);
	CAMBIA(h->pag, m->pag);
	CAMBIA(h->sin_rejilla, m->sin_rejilla);
	CAMBIA(h->oculta, m->oculta);
}

/* (una vez por paso y hoja) */
static int ya_hay(dh_t *d, int tipo, u16 hoja)
{
	u32 i;

	for (i = 0; i < d->cur.n; i++)
		if (d->cur.r[i].tipo == tipo && d->cur.r[i].hoja == hoja)
			return 1;
	return 0;
}

void dh_meta(libro *l, hoja *h)
{
	dh_t *d = dh(l);
	d_rec *x;

	if (!grabando(l) || ya_hay(d, DR_META, h->id))
		return;
	x = nuevo_rec(d, DR_META);
	if (!x)
		return;
	x->hoja = h->id;
	x->m = meta_de(h);
	d->cur.memoria += (u32)sizeof(d_meta) + h->n_com * 64 + h->n_gr * (u32)sizeof(grafico);
}

void dh_altos(libro *l, hoja *h)
{
	dh_t *d = dh(l);
	d_rec *x;

	if (!grabando(l) || ya_hay(d, DR_ALTOS, h->id))
		return;
	x = nuevo_rec(d, DR_ALTOS);
	if (!x)
		return;
	x->hoja = h->id;
	x->alto = dup(h->alto, h->alto ? CU_FILAS * 2 : 0);
	x->oculta_f = dup(h->oculta_f, h->oculta_f ? CU_FILAS / 8 : 0);
	x->estilo_fila = dup(h->estilo_fila, h->estilo_fila ? CU_FILAS * 2 : 0);
	d->cur.memoria += (h->alto ? CU_FILAS * 2 : 0) + (h->oculta_f ? CU_FILAS / 8 : 0) + (h->estilo_fila ? CU_FILAS * 2 : 0);
}

static nombre_def *copia_nombres(const libro *l)
{
	nombre_def *n;
	u32 i;

	if (!l->n_nom)
		return NULL;
	n = so_pide(l->n_nom * sizeof(nombre_def));
	if (!n)
		return NULL;
	for (i = 0; i < l->n_nom; i++) {
		n[i] = l->nom[i];
		n[i].f = fm_copia(l->nom[i].f);
	}
	return n;
}

void dh_libro(libro *l)
{
	dh_t *d = dh(l);
	d_rec *x;

	if (!grabando(l) || ya_hay(d, DR_LIBRO, 0))
		return;
	x = nuevo_rec(d, DR_LIBRO);
	if (!x)
		return;
	x->hs = dup(l->h, l->n_h * sizeof(hoja *));
	x->n_h = l->n_h;
	x->activa = l->activa;
	x->nom = copia_nombres(l);
	x->n_nom = x->nom ? l->n_nom : 0;
}

void dh_vista(libro *l, hoja *h)
{
	dh_t *d = dh(l);

	if (!grabando(l))
		return;
	d->cur.hoja = h->id;
	d->cur.cur_f = h->cur_f;
	d->cur.cur_c = h->cur_c;
	d->cur.sel = h->sel;
}

/* ---- soltar ---- */
static int en_libro(const libro *l, const hoja *h)
{
	u32 i;

	for (i = 0; i < l->n_h; i++)
		if (l->h[i] == h)
			return 1;
	return 0;
}

static void rec_suelta(libro *l, d_rec *x, hoja ***sueltas, u32 *n_sueltas)
{
	u32 i;

	switch (x->tipo) {
	case DR_REGION:
		for (i = 0; i < x->n; i++)
			celda_vacia(&x->cel[i]);
		so_suelta(x->cel);
		break;
	case DR_META:
		meta_suelta(x->m);
		break;
	case DR_ALTOS:
		so_suelta(x->alto);
		so_suelta(x->oculta_f);
		so_suelta(x->estilo_fila);
		break;
	case DR_LIBRO:
		/* las hojas que no están en el libro son de aquí (se sueltan una vez) */
		for (i = 0; i < x->n_h; i++)
			if (!en_libro(l, x->hs[i])) {
				u32 k;
				int ya = 0;

				for (k = 0; k < *n_sueltas; k++)
					if ((*sueltas)[k] == x->hs[i])
						ya = 1;
				if (!ya) {
					hoja **nn = so_repide(*sueltas, (*n_sueltas + 1) * sizeof(hoja *));

					if (nn) {
						*sueltas = nn;
						nn[(*n_sueltas)++] = x->hs[i];
					}
				}
			}
		so_suelta(x->hs);
		for (i = 0; i < x->n_nom; i++)
			fm_suelta(x->nom[i].f);
		so_suelta(x->nom);
		break;
	}
}

static void pasos_suelta(libro *l, dh_t *d, u32 desde)
{
	hoja **sueltas = NULL;
	u32 n_sueltas = 0, i, k;

	for (i = desde; i < d->n; i++) {
		for (k = 0; k < d->p[i].n; k++)
			rec_suelta(l, &d->p[i].r[k], &sueltas, &n_sueltas);
		so_suelta(d->p[i].r);
	}
	d->n = desde;
	if (d->pos > d->n)
		d->pos = d->n;
	for (i = 0; i < n_sueltas; i++)
		hoja_suelta(sueltas[i]);
	so_suelta(sueltas);
}

void dh_olvida(libro *l)
{
	dh_t *d = l->deshacer;

	if (!d)
		return;
	pasos_suelta(l, d, 0);
	d->pos = 0;
}

void dh_suelta(libro *l)
{
	dh_t *d = l->deshacer;

	if (!d)
		return;
	pasos_suelta(l, d, 0);
	so_suelta(d->p);
	so_suelta(d->cur.r);
	so_suelta(d);
	l->deshacer = NULL;
}

/* ---- empezar y acabar ---- */
void dh_empieza(libro *l, const char *nombre)
{
	dh_t *d = dh(l);

	if (!d || d->haciendo)
		return;
	if (d->hondo++ > 0)
		return;
	memset(&d->cur, 0, sizeof d->cur);
	copia(d->cur.nombre, nombre, sizeof d->cur.nombre);
	d->abierto = 1;
	{
		hoja *h = lib_hoja(l, l->activa);

		if (h)
			dh_vista(l, h);
	}
}

void dh_acaba(libro *l)
{
	dh_t *d = dh(l);

	if (!d || d->haciendo || !d->hondo)
		return;
	if (--d->hondo > 0)
		return;
	d->abierto = 0;
	if (!d->cur.n) {
		so_suelta(d->cur.r);
		memset(&d->cur, 0, sizeof d->cur);
		return;
	}
	/* lo que se podía rehacer ya no */
	pasos_suelta(l, d, d->pos);
	if (d->n >= d->cabe) {
		u32 c = d->cabe ? d->cabe * 2 : 32;
		paso *n = so_repide(d->p, c * sizeof(paso));

		if (!n) {
			/* (sin memoria: este paso no se podrá deshacer) */
			u32 k;
			hoja **s = NULL;
			u32 ns = 0;

			for (k = 0; k < d->cur.n; k++)
				rec_suelta(l, &d->cur.r[k], &s, &ns);
			so_suelta(d->cur.r);
			so_suelta(s);
			memset(&d->cur, 0, sizeof d->cur);
			return;
		}
		d->p = n;
		d->cabe = c;
	}
	d->p[d->n++] = d->cur;
	d->pos = d->n;
	memset(&d->cur, 0, sizeof d->cur);
}

/* ---- cambiar lo de ahora por lo guardado ---- */
static void cambia_region(libro *l, d_rec *x)
{
	hoja *h = lib_hoja_id(l, x->hoja);
	u32 n, *k, i, m = 0;
	celda *fuera = NULL;

	if (!h)
		return;
	k = claves_en(h, x->r, &n);
	if (!k)
		return;
	if (n) {
		fuera = so_pide(n * sizeof(celda));
		if (!fuera) {
			so_suelta(k);
			return;
		}
	}
	for (i = 0; i < n; i++) {
		celda c;

		if (!hj_celda_saca(h, CU_FILA(k[i]), CU_COL(k[i]), &c))
			continue;
		if (c.f)
			calc_quita_formula(l, h, &c);
		c.marca = 0;
		fuera[m++] = c;
		calc_cambia(l, h, CU_FILA(k[i]), CU_COL(k[i]));
	}
	for (i = 0; i < x->n; i++) {
		celda *c = hj_celda_crea(h, CU_FILA(x->cel[i].clave), CU_COL(x->cel[i].clave));

		if (!c) {
			celda_vacia(&x->cel[i]);
			continue;
		}
		*c = x->cel[i];
		c->marca = 0;
		if (c->f)
			calc_mete_formula(l, h, c);
		calc_cambia(l, h, CU_FILA(c->clave), CU_COL(c->clave));
	}
	so_suelta(x->cel);
	x->cel = fuera;
	x->n = m;
	so_suelta(k);
}

static void cambia(libro *l, d_rec *x)
{
	switch (x->tipo) {
	case DR_REGION:
		cambia_region(l, x);
		break;
	case DR_META: {
		hoja *h = lib_hoja_id(l, x->hoja);

		if (h && x->m)
			meta_cambia(h, x->m);
		break;
	}
	case DR_ALTOS: {
		hoja *h = lib_hoja_id(l, x->hoja);

		if (h) {
			CAMBIA(h->alto, x->alto);
			CAMBIA(h->oculta_f, x->oculta_f);
			CAMBIA(h->estilo_fila, x->estilo_fila);
			calc_rehaz(l);
		}
		break;
	}
	case DR_LIBRO: {
		u32 i;

		CAMBIA(l->h, x->hs);
		CAMBIA(l->n_h, x->n_h);
		CAMBIA(l->activa, x->activa);
		CAMBIA(l->nom, x->nom);
		CAMBIA(l->n_nom, x->n_nom);
		if (l->activa >= l->n_h)
			l->activa = l->n_h ? l->n_h - 1 : 0;
		for (i = 0; i < x->n_h; i++)
			if (!en_libro(l, x->hs[i]))
				calc_hoja_fuera(l, x->hs[i]);
		calc_rehaz(l);
		break;
	}
	}
}

static void a_la_vista(libro *l, const paso *p)
{
	int i = lib_hoja_indice(l, p->hoja);
	hoja *h;

	if (i < 0)
		return;
	l->activa = (u32)i;
	h = l->h[i];
	h->cur_f = p->cur_f;
	h->cur_c = p->cur_c;
	h->sel = p->sel;
}

int dh_deshace(libro *l)
{
	dh_t *d = dh(l);
	paso *p;
	u32 i;

	if (!d || !d->pos)
		return 0;
	p = &d->p[--d->pos];
	d->haciendo = 1;
	for (i = p->n; i-- > 0;)
		cambia(l, &p->r[i]);
	d->haciendo = 0;
	a_la_vista(l, p);
	lib_cambio(l);
	calc_recalcula(l);
	return 1;
}

int dh_rehace(libro *l)
{
	dh_t *d = dh(l);
	paso *p;
	u32 i;

	if (!d || d->pos >= d->n)
		return 0;
	p = &d->p[d->pos++];
	d->haciendo = 1;
	for (i = 0; i < p->n; i++)
		cambia(l, &p->r[i]);
	d->haciendo = 0;
	a_la_vista(l, p);
	lib_cambio(l);
	calc_recalcula(l);
	return 1;
}

int dh_hay(const libro *l, int rehacer)
{
	const dh_t *d = l->deshacer;

	if (!d)
		return 0;
	return rehacer ? d->pos < d->n : d->pos > 0;
}

const char *dh_nombre(const libro *l, int rehacer)
{
	const dh_t *d = l->deshacer;

	if (!dh_hay(l, rehacer))
		return "";
	return rehacer ? d->p[d->pos].nombre : d->p[d->pos - 1].nombre;
}

u32 dh_memoria(const libro *l)
{
	const dh_t *d = l->deshacer;
	u32 i, m = 0;

	if (!d)
		return 0;
	for (i = 0; i < d->n; i++)
		m += d->p[i].memoria;
	return m;
}
