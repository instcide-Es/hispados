/* calculo.c - Cuentas 2: las dependencias, el recálculo de lo que cambia
 * (en el orden de sus dependencias) y el evaluador de las fórmulas
 * (calculo.h y evalua.h; las funciones, en funciones.c).
 *
 * Las dependencias de una celda suelta van en una tabla de dispersión por
 * la celda de la que se depende (clave de libro: hoja << 24 | fila << 8 |
 * columna); las de un rango, en una lista por cada columna de la hoja (o,
 * si el rango es muy ancho, en la de los anchos). Al quitar una fórmula sus
 * apuntes se marcan muertos; cuando hay muchos muertos, se rehace todo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "evalua.h"
#include "numeros.h"

#define MUERTO 0xFFFFFFFFu
#define RANGO_BIT 0x80000000u
#define ANCHO_LISTA 8		/* un rango de más columnas va a la lista de los anchos */
#define HONDO_DEMANDA 600	/* fórmulas pendientes que piden otras, como mucho */

u32 calc_ultimas, calc_tics;

typedef struct {
	u32 de, a, sig;
} dnodo;
typedef struct {
	u32 f0, f1;
	u16 c0, c1;
	u32 a;
} drango;
typedef struct {
	u32 *v;
	u32 n, cabe;
} vec;
typedef struct {
	vec col[CU_COLS];
	vec anchas;
} dep_hoja;
typedef struct {
	dnodo *nod;
	u32 n_nod, cabe_nod, muertos;
	u32 *cab;
	u32 cabe_cab;
	drango *rg;
	u32 n_rg, cabe_rg, rg_muertos;
	vec pend, volatiles, sucias, cola, tmp, listos;
	int rehacer;
	int mal;
} calc_t;

static int vmete(vec *v, u32 x)
{
	if (v->n >= v->cabe) {
		u32 c = v->cabe ? v->cabe * 2 : 16;
		u32 *n = so_repide(v->v, c * sizeof(u32));

		if (!n)
			return 1;
		v->v = n;
		v->cabe = c;
	}
	v->v[v->n++] = x;
	return 0;
}

static void vsuelta(vec *v)
{
	so_suelta(v->v);
	v->v = NULL;
	v->n = v->cabe = 0;
}

int calc_inicia(libro *l)
{
	calc_t *k = so_pide(sizeof(calc_t));

	if (!k)
		return 1;
	memset(k, 0, sizeof *k);
	l->calc = k;
	return 0;
}

static void suelta_dep_hoja(hoja *h)
{
	dep_hoja *d = h->dep;
	u32 i;

	if (!d)
		return;
	for (i = 0; i < CU_COLS; i++)
		vsuelta(&d->col[i]);
	vsuelta(&d->anchas);
	so_suelta(d);
	h->dep = NULL;
}

void calc_suelta_hoja(hoja *h)
{
	suelta_dep_hoja(h);
}

static void vacia_deps(libro *l)
{
	calc_t *k = l->calc;
	u32 i;

	so_suelta(k->nod);
	so_suelta(k->cab);
	so_suelta(k->rg);
	k->nod = NULL;
	k->cab = NULL;
	k->rg = NULL;
	k->n_nod = k->cabe_nod = k->muertos = k->cabe_cab = 0;
	k->n_rg = k->cabe_rg = k->rg_muertos = 0;
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];
		u32 j;

		suelta_dep_hoja(h);
		for (j = 0; j < h->n_cel; j++)
			if (h->cel[j].f) {
				so_suelta(h->cel[j].f->nodos);
				h->cel[j].f->nodos = NULL;
				h->cel[j].f->n_nodos = 0;
			}
	}
}

void calc_suelta(libro *l)
{
	calc_t *k;

	if (!l || !l->calc)
		return;
	k = l->calc;
	so_suelta(k->nod);
	so_suelta(k->cab);
	so_suelta(k->rg);
	vsuelta(&k->pend);
	vsuelta(&k->volatiles);
	vsuelta(&k->sucias);
	vsuelta(&k->cola);
	vsuelta(&k->tmp);
	vsuelta(&k->listos);
	so_suelta(k);
	l->calc = NULL;
}

static u32 mezcla(u32 x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

/* ---- apuntar dependencias ---- */
static int crece_cab(calc_t *k)
{
	u32 c = k->cabe_cab ? k->cabe_cab * 2 : 1024, i;
	u32 *t = so_pide(c * sizeof(u32));

	if (!t)
		return 1;
	memset(t, 0, c * sizeof(u32));
	so_suelta(k->cab);
	k->cab = t;
	k->cabe_cab = c;
	/* las cadenas otra vez (las de los muertos también: se limpian al rehacer) */
	for (i = 0; i < k->n_nod; i++) {
		u32 b = mezcla(k->nod[i].de) & (c - 1);

		k->nod[i].sig = t[b];
		t[b] = i + 1;
	}
	return 0;
}

static int apunta_celda(calc_t *k, u32 de, u32 a, formula *fm)
{
	u32 b, i;

	if (k->n_nod >= k->cabe_nod) {
		u32 c = k->cabe_nod ? k->cabe_nod * 2 : 1024;
		dnodo *n = so_repide(k->nod, c * sizeof(dnodo));

		if (!n)
			return 1;
		k->nod = n;
		k->cabe_nod = c;
	}
	if (k->n_nod + 1 > k->cabe_cab && crece_cab(k))
		return 1;
	i = k->n_nod++;
	b = mezcla(de) & (k->cabe_cab - 1);
	k->nod[i].de = de;
	k->nod[i].a = a;
	k->nod[i].sig = k->cab[b];
	k->cab[b] = i + 1;
	if (fm) {
		u32 *n = so_repide(fm->nodos, (fm->n_nodos + 1) * sizeof(u32));

		if (!n)
			return 1;
		fm->nodos = n;
		fm->nodos[fm->n_nodos++] = i;
	}
	return 0;
}

static int apunta_rango(calc_t *k, hoja *hd, rango r, u32 a, formula *fm)
{
	dep_hoja *d = hd->dep;
	u32 i, c;

	if (!d) {
		d = so_pide(sizeof(dep_hoja));
		if (!d)
			return 1;
		memset(d, 0, sizeof *d);
		hd->dep = d;
	}
	if (k->n_rg >= k->cabe_rg) {
		u32 cc = k->cabe_rg ? k->cabe_rg * 2 : 256;
		drango *n = so_repide(k->rg, cc * sizeof(drango));

		if (!n)
			return 1;
		k->rg = n;
		k->cabe_rg = cc;
	}
	i = k->n_rg++;
	k->rg[i].f0 = r.f0;
	k->rg[i].f1 = r.f1;
	k->rg[i].c0 = r.c0;
	k->rg[i].c1 = r.c1;
	k->rg[i].a = a;
	if ((u32)r.c1 - r.c0 + 1 > ANCHO_LISTA) {
		if (vmete(&d->anchas, i))
			return 1;
	} else
		for (c = r.c0; c <= r.c1; c++)
			if (vmete(&d->col[c], i))
				return 1;
	if (fm) {
		u32 *n = so_repide(fm->nodos, (fm->n_nodos + 1) * sizeof(u32));

		if (!n)
			return 1;
		fm->nodos = n;
		fm->nodos[fm->n_nodos++] = i | RANGO_BIT;
	}
	return 0;
}

/* las referencias de una fórmula (y de los nombres que usa) */
static void apunta_de(libro *l, hoja *h, formula *fm, const formula *de, u32 a, int hondo)
{
	calc_t *k = l->calc;
	u32 i;

	if (hondo > 8)
		return;
	for (i = 0; i < de->n; i++) {
		const cu_tok *t = &de->t[i];

		if (t->t == T_REF || t->t == T_RANGO) {
			hoja *hd = lib_hoja_id(l, t->h);

			if (!hd)
				continue;
			if (t->t == T_REF || (t->u.r.f0 == t->u.r.f1 && t->u.r.c0 == t->u.r.c1)) {
				if (apunta_celda(k, CU_GCLAVE(t->h, t->u.r.f0, t->u.r.c0), a, fm))
					k->mal = 1;
			} else if (apunta_rango(k, hd, cu_rango((u32)t->u.r.f0, t->u.r.c0, (u32)t->u.r.f1, t->u.r.c1), a, fm))
				k->mal = 1;
		} else if (t->t == T_NOMBRE && de->txt) {
			int n = lib_nombre(l, de->txt + t->u.x.i, t->u.x.n);

			if (n >= 0 && l->nom[n].f)
				apunta_de(l, h, fm, l->nom[n].f, a, hondo + 1);
		}
	}
}

static void apunta(libro *l, hoja *h, celda *x)
{
	calc_t *k = l->calc;
	u32 a = CU_GCLAVE(h->id, CU_FILA(x->clave), CU_COL(x->clave));

	if (!x->f)
		return;
	apunta_de(l, h, x->f, x->f, a, 0);
	if (x->f->volatil)
		vmete(&k->volatiles, a);
}

static void desapunta(libro *l, formula *fm)
{
	calc_t *k = l->calc;
	u32 i;

	for (i = 0; i < fm->n_nodos; i++) {
		u32 j = fm->nodos[i];

		if (j & RANGO_BIT) {
			j &= ~RANGO_BIT;
			if (j < k->n_rg && k->rg[j].a != MUERTO) {
				k->rg[j].a = MUERTO;
				k->rg_muertos++;
			}
		} else if (j < k->n_nod && k->nod[j].a != MUERTO) {
			k->nod[j].a = MUERTO;
			k->muertos++;
		}
	}
	so_suelta(fm->nodos);
	fm->nodos = NULL;
	fm->n_nodos = 0;
	/* (demasiados muertos: se rehace todo en el próximo cálculo) */
	if (k->muertos > 4096 && k->muertos > k->n_nod / 2)
		k->rehacer = 1;
	if (k->rg_muertos > 1024 && k->rg_muertos > k->n_rg / 2)
		k->rehacer = 1;
}

void calc_mete_formula(libro *l, hoja *h, celda *x)
{
	calc_t *k = l->calc;

	l->n_formulas++;
	if (!k->rehacer)
		apunta(l, h, x);
	x->marca |= CM_SUCIA;
}

void calc_quita_formula(libro *l, hoja *h, celda *x)
{
	calc_t *k = l->calc;

	(void)h;
	if (l->n_formulas)
		l->n_formulas--;
	if (!k->rehacer)
		desapunta(l, x->f);
	x->marca = 0;
}

void calc_cambia(libro *l, hoja *h, u32 f, u32 c)
{
	calc_t *k = l->calc;

	if (!k->rehacer && vmete(&k->pend, CU_GCLAVE(h->id, f, c)))
		k->rehacer = 1;
}

void calc_rehaz(libro *l)
{
	((calc_t *)l->calc)->rehacer = 1;
}

void calc_hoja_fuera(libro *l, hoja *h)
{
	u32 j;

	for (j = 0; j < h->n_cel; j++)
		if (h->cel[j].f) {
			so_suelta(h->cel[j].f->nodos);
			h->cel[j].f->nodos = NULL;
			h->cel[j].f->n_nodos = 0;
			h->cel[j].marca = 0;
		}
	suelta_dep_hoja(h);
	calc_rehaz(l);
}

/* ---- quién depende de quién ---- */
static celda *celda_g(libro *l, u32 g, hoja **hh)
{
	hoja *h = lib_hoja_id(l, (u16)(g >> 24));

	if (hh)
		*hh = h;
	return h ? hj_celda_k(h, g & 0xFFFFFF) : NULL;
}

/* las que dependen de g, a k->tmp */
static void dependientes(libro *l, u32 g)
{
	calc_t *k = l->calc;
	u32 i;
	hoja *h;

	k->tmp.n = 0;
	if (k->cabe_cab)
		for (i = k->cab[mezcla(g) & (k->cabe_cab - 1)]; i; i = k->nod[i - 1].sig) {
			const dnodo *n = &k->nod[i - 1];

			if (n->de == g && n->a != MUERTO)
				vmete(&k->tmp, n->a);
		}
	h = lib_hoja_id(l, (u16)(g >> 24));
	if (h && h->dep) {
		dep_hoja *d = h->dep;
		u32 f = CU_FILA(g & 0xFFFFFF), c = CU_COL(g), j;
		vec *v = &d->col[c];

		for (j = 0; j < v->n; j++) {
			const drango *r = &k->rg[v->v[j]];

			if (r->a != MUERTO && f >= r->f0 && f <= r->f1)
				vmete(&k->tmp, r->a);
		}
		v = &d->anchas;
		for (j = 0; j < v->n; j++) {
			const drango *r = &k->rg[v->v[j]];

			if (r->a != MUERTO && f >= r->f0 && f <= r->f1 && c >= r->c0 && c <= r->c1)
				vmete(&k->tmp, r->a);
		}
	}
}

/* ---- calcular una celda ---- */
static void calcula_celda(libro *l, hoja *h, celda *x, int hondo);

static void rehaz_todo(libro *l)
{
	calc_t *k = l->calc;
	u32 i, j;

	vacia_deps(l);
	k->volatiles.n = 0;
	k->pend.n = 0;
	k->rehacer = 0;
	k->mal = 0;
	l->n_formulas = 0;
	for (i = 0; i < l->n_h; i++) {
		hoja *h = l->h[i];

		for (j = 0; j < h->n_cel; j++)
			if (h->cel[j].f) {
				apunta(l, h, &h->cel[j]);
				h->cel[j].marca |= CM_SUCIA;
				l->n_formulas++;
				vmete(&k->pend, CU_GCLAVE(h->id, CU_FILA(h->cel[j].clave), CU_COL(h->cel[j].clave)));
			}
	}
	if (k->mal) {
		/* (sin memoria para apuntarlas: se calculan todas cada vez, por orden de pedirlas) */
		k->rehacer = 1;
	}
}

u32 calc_recalcula(libro *l)
{
	calc_t *k = l->calc;
	u32 i, hechas = 0, t0 = so_tics();
	int sin_deps;

	if (k->rehacer)
		rehaz_todo(l);
	sin_deps = k->rehacer;	/* (sin memoria) */
	if (!k->pend.n && !k->volatiles.n)
		return 0;
	l->circular = 0;
	k->sucias.n = 0;
	k->cola.n = 0;
	/* las fuentes: lo cambiado y las volátiles (las que ya no lo son, fuera) */
	for (i = 0; i < k->pend.n; i++)
		vmete(&k->cola, k->pend.v[i]);
	{
		u32 n = 0;

		for (i = 0; i < k->volatiles.n; i++) {
			celda *x = celda_g(l, k->volatiles.v[i], NULL);

			/* (cada una, una vez: CM_VOLATIL dice que ya está) */
			if (x && x->f && x->f->volatil && !(x->marca & CM_VOLATIL)) {
				x->marca |= CM_VOLATIL | CM_SUCIA;
				k->volatiles.v[n++] = k->volatiles.v[i];
				vmete(&k->cola, k->volatiles.v[i]);
			}
		}
		k->volatiles.n = n;
		for (i = 0; i < n; i++) {
			celda *x = celda_g(l, k->volatiles.v[i], NULL);

			if (x)
				x->marca &= (u8)~CM_VOLATIL;
		}
	}
	k->pend.n = 0;
	/* las fórmulas de las fuentes, sucias */
	for (i = 0; i < k->cola.n; i++) {
		celda *x = celda_g(l, k->cola.v[i], NULL);

		if (x && x->f && !(x->marca & CM_EN_COLA)) {
			x->marca |= CM_SUCIA;
			x->grado = 0;
		}
	}
	/* a lo ancho: lo que depende de lo sucio se ensucia; y cuántas sucias pide cada una */
	for (i = 0; i < k->cola.n; i++) {
		u32 g = k->cola.v[i], j;
		celda *x = celda_g(l, g, NULL);
		int es_f = x && x->f && (x->marca & CM_SUCIA);

		if (es_f) {
			if (x->marca & CM_EN_COLA)
				continue;
			x->marca |= CM_EN_COLA;
			vmete(&k->sucias, g);
		}
		if (sin_deps)
			continue;
		dependientes(l, g);
		for (j = 0; j < k->tmp.n; j++) {
			u32 d = k->tmp.v[j];
			celda *y = celda_g(l, d, NULL);

			if (!y || !y->f)
				continue;
			if (!(y->marca & CM_SUCIA)) {
				y->marca |= CM_SUCIA;
				y->grado = 0;
				vmete(&k->cola, d);
			}
			if (es_f)
				y->grado++;
		}
	}
	if (sin_deps) {
		/* sin dependencias: todas, a demanda */
		for (i = 0; i < l->n_h; i++) {
			hoja *h = l->h[i];
			u32 j;

			for (j = 0; j < h->n_cel; j++)
				if (h->cel[j].f)
					h->cel[j].marca |= CM_SUCIA;
			for (j = 0; j < h->n_cel; j++)
				if (h->cel[j].f && (h->cel[j].marca & CM_SUCIA)) {
					calcula_celda(l, h, &h->cel[j], 0);
					hechas++;
				}
			for (j = 0; j < h->n_cel; j++)
				h->cel[j].marca &= (u8)~CM_EN_COLA;
		}
		k->sucias.n = 0;
		calc_ultimas = hechas;
		calc_tics = so_tics() - t0;
		return hechas;
	}
	/* de Kahn: primero las que no piden ninguna sucia */
	k->listos.n = 0;
	for (i = 0; i < k->sucias.n; i++) {
		celda *x = celda_g(l, k->sucias.v[i], NULL);

		if (x && x->grado == 0)
			vmete(&k->listos, k->sucias.v[i]);
	}
	for (i = 0; i < k->listos.n; i++) {
		u32 g = k->listos.v[i], j;
		hoja *h;
		celda *x = celda_g(l, g, &h);

		if (!x || !(x->marca & CM_EN_COLA))
			continue;
		if (x->marca & CM_SUCIA) {
			calcula_celda(l, h, x, 0);
			hechas++;
		}
		x->marca &= (u8)~CM_EN_COLA;
		dependientes(l, g);
		for (j = 0; j < k->tmp.n; j++) {
			celda *y = celda_g(l, k->tmp.v[j], NULL);

			if (y && y->f && (y->marca & CM_EN_COLA) && y->grado) {
				y->grado--;
				if (!y->grado)
					vmete(&k->listos, k->tmp.v[j]);
			}
		}
	}
	/* lo que queda está en un círculo (o depende de uno) */
	for (i = 0; i < k->sucias.n; i++) {
		hoja *h;
		celda *x = celda_g(l, k->sucias.v[i], &h);

		if (!x)
			continue;
		if (x->marca & CM_SUCIA) {
			calcula_celda(l, h, x, 0);
			hechas++;
		}
		x->marca &= (u8)~CM_EN_COLA;
	}
	k->sucias.n = 0;
	calc_ultimas = hechas;
	calc_tics = so_tics() - t0;
	return hechas;
}

u32 calc_todo(libro *l)
{
	calc_rehaz(l);
	return calc_recalcula(l);
}

/* ===================================================================== */
/*  El evaluador                                                         */
/* ===================================================================== */
#define TROZO 16384
struct ev_trozo {
	ev_trozo *sig;
	u32 n, cabe;
	char d[1];
};

char *ev_pide(ctx *x, u32 n)
{
	ev_trozo *t = x->mem;
	char *p;

	n = (n + 7) & ~7u;
	if (!t || t->n + n > t->cabe) {
		u32 c = n > TROZO ? n : TROZO;
		ev_trozo *z = so_pide(sizeof(ev_trozo) + c);

		if (!z)
			return NULL;
		z->sig = t;
		z->n = 0;
		z->cabe = c;
		x->mem = z;
		t = z;
	}
	p = t->d + t->n;
	t->n += n;
	return p;
}

static void suelta_mem(ctx *x)
{
	while (x->mem) {
		ev_trozo *s = x->mem->sig;

		so_suelta(x->mem);
		x->mem = s;
	}
}

void ev_num(ev *r, double n)
{
	memset(r, 0, sizeof *r);
	if (n != n || n > 1.7976931348623157e308 || n < -1.7976931348623157e308) {
		r->t = V_ERR;
		r->e = ER_NUM;
		return;
	}
	r->t = V_NUM;
	r->n = n;
}

void ev_log(ev *r, int v)
{
	memset(r, 0, sizeof *r);
	r->t = V_LOG;
	r->n = v ? 1 : 0;
}

void ev_err(ev *r, int e)
{
	memset(r, 0, sizeof *r);
	r->t = V_ERR;
	r->e = (u8)e;
}

void ev_txt(ctx *x, ev *r, const char *s, u32 n)
{
	char *p;

	memset(r, 0, sizeof *r);
	if (n > 32767) {
		r->t = V_ERR;
		r->e = ER_VALOR;
		return;
	}
	p = ev_pide(x, n + 1);
	if (!p) {
		r->t = V_ERR;
		r->e = ER_VALOR;
		return;
	}
	if (n)
		memcpy(p, s, n);
	p[n] = 0;
	r->t = V_TXT;
	r->s = p;
	r->sn = n;
}

static const valor v_circ = { V_ERR, ER_CIRC, 0, NULL };

const valor *ev_celda(ctx *x, u16 hid, u32 f, u32 c)
{
	static const valor vacio = { V_VACIO, 0, 0, NULL };
	hoja *h = (x->h && x->h->id == hid) ? x->h : lib_hoja_id(x->l, hid);
	celda *k;

	if (!h)
		return &vacio;
	k = hj_celda(h, f, c);
	if (!k)
		return &vacio;
	if (k->f && (k->marca & CM_SUCIA)) {
		if (k->marca & CM_CALCULANDO) {
			if (!x->l->circular)
				x->l->circular = CU_GCLAVE(h->id, f, c) + 1;
			return &v_circ;
		}
		if (x->hondo < HONDO_DEMANDA)
			calcula_celda(x->l, h, k, x->hondo + 1);
	}
	return &k->v;
}

static void de_valor(ctx *x, const valor *v, ev *r)
{
	switch (v->t) {
	case V_NUM:
		ev_num(r, v->n);
		break;
	case V_TXT:
		ev_txt(x, r, v->s ? v->s : "", v->s ? largo(v->s) : 0);
		break;
	case V_LOG:
		ev_log(r, v->n != 0);
		break;
	case V_ERR:
		ev_err(r, v->e);
		break;
	default:
		memset(r, 0, sizeof *r);
		r->t = V_VACIO;
	}
}

void ev_valor(ctx *x, const ev *a, ev *r)
{
	if (a->t != V_REF) {
		if (r != a)
			*r = *a;
		return;
	}
	{
		rango g = a->r;
		u32 f = g.f0, c = g.c0;

		if (g.f0 != g.f1 || g.c0 != g.c1) {
			/* la intersección con la fila o la columna de la fórmula */
			if (g.c0 == g.c1 && x->f >= g.f0 && x->f <= g.f1 && (a->h == (x->h ? x->h->id : 0) || 1))
				f = x->f;
			else if (g.f0 == g.f1 && x->c >= g.c0 && x->c <= g.c1)
				c = x->c;
			else if (g.c0 == g.c1 && g.f0 == g.f1)
				;
			else {
				ev_err(r, ER_VALOR);
				return;
			}
		}
		de_valor(x, ev_celda(x, a->h, f, c), r);
	}
}

int ev_a_num(ctx *x, const ev *a0, double *n)
{
	ev a;

	ev_valor(x, a0, &a);
	switch (a.t) {
	case V_NUM:
	case V_LOG:
		*n = a.n;
		return 0;
	case V_VACIO:
		*n = 0;
		return 0;
	case V_ERR:
		return a.e;
	case V_TXT: {
		int tipo;

		if (a.sn == 0)
			return ER_VALOR;
		if (num_lee_entrada(a.s, &x->l->loc, x->l->moneda, n, &tipo))
			return 0;
		if (x->l->loc.dec != '.' && num_lee_entrada(a.s, &CU_ARCHIVO, "", n, &tipo))
			return 0;
		return ER_VALOR;
	}
	}
	return ER_VALOR;
}

int ev_a_txt(ctx *x, const ev *a0, const char **s, u32 *n)
{
	ev a;

	ev_valor(x, a0, &a);
	switch (a.t) {
	case V_TXT:
		*s = a.s;
		*n = a.sn;
		return 0;
	case V_VACIO:
		*s = "";
		*n = 0;
		return 0;
	case V_NUM: {
		char t[64];
		char *p;

		num_general(a.n, &x->l->loc, 20, t, sizeof t);
		/* (General de las fórmulas: hasta 15 cifras) */
		{
			char e[64];

			num_edicion(a.n, "General", &x->l->loc, e, sizeof e);
			if (!strchr(e, 'E'))
				copia(t, e, sizeof t);
		}
		p = ev_pide(x, largo(t) + 1);
		if (!p)
			return ER_VALOR;
		copia(p, t, largo(t) + 1);
		*s = p;
		*n = largo(t);
		return 0;
	}
	case V_LOG:
		*s = a.n != 0 ? (x->l->loc.ingles ? "TRUE" : "VERDADERO") : (x->l->loc.ingles ? "FALSE" : "FALSO");
		*n = largo(*s);
		return 0;
	case V_ERR:
		return a.e;
	}
	return ER_VALOR;
}

int ev_a_log(ctx *x, const ev *a0, int *v)
{
	ev a;

	ev_valor(x, a0, &a);
	switch (a.t) {
	case V_NUM:
	case V_LOG:
		*v = a.n != 0;
		return 0;
	case V_VACIO:
		*v = 0;
		return 0;
	case V_ERR:
		return a.e;
	case V_TXT:
		if (iguales_sin_caja(a.s, "VERDADERO") || iguales_sin_caja(a.s, "TRUE")) {
			*v = 1;
			return 0;
		}
		if (iguales_sin_caja(a.s, "FALSO") || iguales_sin_caja(a.s, "FALSE")) {
			*v = 0;
			return 0;
		}
		return ER_VALOR;
	}
	return ER_VALOR;
}

void ev_recorre(ctx *x, const ev *ref, int todas, ev_cada cb, void *u)
{
	static const valor vacio = { V_VACIO, 0, 0, NULL };
	hoja *h;
	rango g;
	u64 area;

	if (ref->t != V_REF)
		return;
	h = lib_hoja_id(x->l, ref->h);
	if (!h)
		return;
	g = ref->r;
	/* lo que no se usa de la hoja no hace falta mirarlo */
	if (!todas) {
		if (!h->lim_f || g.f0 >= h->lim_f || g.c0 >= h->lim_c)
			return;
		if (g.f1 >= h->lim_f)
			g.f1 = h->lim_f - 1;
		if (g.c1 >= h->lim_c)
			g.c1 = (u16)(h->lim_c - 1);
	}
	area = (u64)(g.f1 - g.f0 + 1) * (g.c1 - g.c0 + 1);
	if (!todas && area > 8) {
		/* por el índice de columnas: solo las que hay, seguidas */
		u32 c, n, i;
		int bien = 1;

		for (c = g.c0; c <= g.c1 && bien; c++)
			if (!hj_columna(h, c, &n) && h->n_cel)
				bien = 0;
		if (bien) {
			for (c = g.c0; c <= g.c1; c++) {
				const u32 *ix = hj_columna(h, c, &n);
				u32 a = 0, b = n;

				if (!n)
					continue;
				/* la primera con fila >= f0 */
				while (a < b) {
					u32 m = (a + b) / 2;

					if (CU_FILA(h->cel[ix[m]].clave) < g.f0)
						a = m + 1;
					else
						b = m;
				}
				for (i = a; i < n; i++) {
					celda *k = &h->cel[ix[i]];
					u32 f = CU_FILA(k->clave);

					if (f > g.f1)
						break;
					if (cb(u, (k->f && (k->marca & CM_SUCIA)) ? ev_celda(x, h->id, f, c) : &k->v, f, c))
						return;
				}
			}
			return;
		}
	}
	if (todas || area <= 4096 || area <= h->n_cel) {
		u32 f, c;

		if (todas && area > 4u * 1024 * 1024)
			return;
		for (f = g.f0; f <= g.f1; f++)
			for (c = g.c0; c <= g.c1; c++) {
				celda *k = hj_celda(h, f, c);
				const valor *v = &vacio;

				if (k)
					v = (k->f && (k->marca & CM_SUCIA)) ? ev_celda(x, h->id, f, c) : &k->v;
				else if (!todas)
					continue;
				if (cb(u, v, f, c))
					return;
			}
	} else {
		u32 i;

		for (i = 0; i < h->n_cel; i++) {
			u32 kk = h->cel[i].clave, f = CU_FILA(kk), c = CU_COL(kk);

			if (f < g.f0 || f > g.f1 || c < g.c0 || c > g.c1)
				continue;
			if (cb(u, (h->cel[i].f && (h->cel[i].marca & CM_SUCIA)) ? ev_celda(x, h->id, f, c) : &h->cel[i].v, f, c))
				return;
		}
	}
}

/* ---- comparar ---- */
static u32 pliega(u32 c)
{
	return sin_tilde(minuscula(c));
}

int ev_texto_cmp(const char *a, u32 na, const char *b, u32 nb)
{
	u32 i = 0, j = 0;
	int des = 0;

	while (i < na && j < nb) {
		u32 x, y, px, py;

		i += (u32)utf8_lee((const u8 *)a + i, na - i, &x);
		j += (u32)utf8_lee((const u8 *)b + j, nb - j, &y);
		px = pliega(x);
		py = pliega(y);
		if (px != py)
			return px < py ? -1 : 1;
		if (!des && x != y)
			des = minuscula(x) == minuscula(y) ? 0 : (x < y ? -1 : 1);
	}
	if (i < na)
		return 1;
	if (j < nb)
		return -1;
	return des;
}

/* con comodines: ? una letra, * las que sean, ~ quita lo especial */
static int casa(const u8 *a, u32 na, const u8 *b, u32 nb, int hondo)
{
	u32 i = 0, j = 0;

	if (hondo > 64)
		return 0;
	while (j < nb) {
		u32 y;
		int ly = utf8_lee(b + j, nb - j, &y);

		if (y == '*') {
			u32 k;

			j += (u32)ly;
			while (j < nb && b[j] == '*')
				j++;
			if (j >= nb)
				return 1;
			for (k = i; k <= na; k++)
				if (casa(a + k, na - k, b + j, nb - j, hondo + 1))
					return 1;
			return 0;
		}
		if (i >= na)
			return 0;
		{
			u32 x;
			int lx = utf8_lee(a + i, na - i, &x);

			if (y == '~' && j + 1 < nb) {
				j += (u32)ly;
				ly = utf8_lee(b + j, nb - j, &y);
			} else if (y == '?') {
				i += (u32)lx;
				j += (u32)ly;
				continue;
			}
			if (pliega(x) != pliega(y))
				return 0;
			i += (u32)lx;
			j += (u32)ly;
		}
	}
	return i == na;
}

int ev_texto_igual(const char *a, u32 na, const char *b, u32 nb, int comodin)
{
	if (comodin && (memchr(b, '*', nb) || memchr(b, '?', nb) || memchr(b, '~', nb)))
		return casa((const u8 *)a, na, (const u8 *)b, nb, 0);
	{
		u32 i = 0, j = 0;

		while (i < na && j < nb) {
			u32 x, y;

			i += (u32)utf8_lee((const u8 *)a + i, na - i, &x);
			j += (u32)utf8_lee((const u8 *)b + j, nb - j, &y);
			if (pliega(x) != pliega(y))
				return 0;
		}
		return i == na && j == nb;
	}
}

static int clase(const ev *a)
{
	return a->t == V_NUM ? 1 : a->t == V_TXT ? 2 : a->t == V_LOG ? 3 : 0;
}

int ev_compara(ctx *x, const ev *a0, const ev *b0)
{
	ev a, b;
	int ka, kb;

	ev_valor(x, a0, &a);
	ev_valor(x, b0, &b);
	/* el vacío es 0, «» o FALSO según con qué se compare */
	if (a.t == V_VACIO && b.t == V_VACIO)
		return 0;
	if (a.t == V_VACIO) {
		if (b.t == V_TXT)
			return b.sn ? -1 : 0;
		a.t = b.t == V_LOG ? V_LOG : V_NUM;
		a.n = 0;
	}
	if (b.t == V_VACIO) {
		if (a.t == V_TXT)
			return a.sn ? 1 : 0;
		b.t = a.t == V_LOG ? V_LOG : V_NUM;
		b.n = 0;
	}
	ka = clase(&a);
	kb = clase(&b);
	if (ka != kb)
		return ka < kb ? -1 : 1;
	if (ka == 2)
		return ev_texto_cmp(a.s, a.sn, b.s, b.sn) < 0 ? -1 : ev_texto_cmp(a.s, a.sn, b.s, b.sn) > 0 ? 1 :
			(ev_texto_igual(a.s, a.sn, b.s, b.sn, 0) ? 0 : 0);
	{
		/* (con 15 cifras: 0,1 + 0,2 es 0,3) */
		double d = a.n - b.n, m = so_abs(a.n) > so_abs(b.n) ? so_abs(a.n) : so_abs(b.n);

		if (so_abs(d) <= m * 1e-15)
			return 0;
		return d < 0 ? -1 : 1;
	}
}

/* ---- lo de las fórmulas ---- */
#define FALTA_E 0xFF		/* (un argumento que falta: V_VACIO con e = FALTA_E) */

static double redondea15(double v)
{
	/* (lo que da una suma o una resta, con 15 cifras: 0,1 + 0,2 = 0,3 exacto al compararlo) */
	return v;
}

static void evalua_en(ctx *x, const formula *fm, ev *res, int hondo);

static void opera(ctx *x, int op, ev *a, ev *b, ev *r)
{
	if (op >= T_IGUAL && op <= T_MAYOR_IG) {
		ev va, vb;
		int c;

		ev_valor(x, a, &va);
		ev_valor(x, b, &vb);
		if (va.t == V_ERR) {
			ev_err(r, va.e);
			return;
		}
		if (vb.t == V_ERR) {
			ev_err(r, vb.e);
			return;
		}
		c = ev_compara(x, &va, &vb);
		ev_log(r, op == T_IGUAL ? c == 0 : op == T_DISTINTO ? c != 0 : op == T_MENOR ? c < 0 : op == T_MAYOR ? c > 0 : op == T_MENOR_IG ? c <= 0 : c >= 0);
		return;
	}
	if (op == T_UNE) {
		const char *s1, *s2;
		u32 n1, n2;
		int e = ev_a_txt(x, a, &s1, &n1);
		char *p;

		if (e) {
			ev_err(r, e);
			return;
		}
		e = ev_a_txt(x, b, &s2, &n2);
		if (e) {
			ev_err(r, e);
			return;
		}
		if (n1 + n2 > 32767) {
			ev_err(r, ER_VALOR);
			return;
		}
		p = ev_pide(x, n1 + n2 + 1);
		if (!p) {
			ev_err(r, ER_VALOR);
			return;
		}
		memcpy(p, s1, n1);
		memcpy(p + n1, s2, n2);
		p[n1 + n2] = 0;
		memset(r, 0, sizeof *r);
		r->t = V_TXT;
		r->s = p;
		r->sn = n1 + n2;
		return;
	}
	{
		double p, q, v = 0;
		int e = ev_a_num(x, a, &p);

		if (e) {
			ev_err(r, e);
			return;
		}
		e = ev_a_num(x, b, &q);
		if (e) {
			ev_err(r, e);
			return;
		}
		switch (op) {
		case T_SUMA:
			v = redondea15(p + q);
			break;
		case T_RESTA:
			v = redondea15(p - q);
			break;
		case T_POR:
			v = p * q;
			break;
		case T_ENTRE:
			if (q == 0) {
				ev_err(r, ER_DIV0);
				return;
			}
			v = p / q;
			break;
		case T_POT:
			if (p == 0 && q == 0) {
				ev_err(r, ER_NUM);
				return;
			}
			if (p == 0 && q < 0) {
				ev_err(r, ER_DIV0);
				return;
			}
			if (p < 0 && q != so_suelo(q)) {
				ev_err(r, ER_NUM);
				return;
			}
			v = pow(p, q);
			break;
		}
		ev_num(r, v);
	}
}

static void evalua_en(ctx *x, const formula *fm, ev *res, int hondo)
{
	ev local[48], *pila = local;
	u32 np = 0, i;

	if (fm->n > 48) {
		pila = so_pide(fm->n * sizeof(ev));
		if (!pila) {
			ev_err(res, ER_VALOR);
			return;
		}
	}
	for (i = 0; i < fm->n; i++) {
		const cu_tok *t = &fm->t[i];
		ev *r = &pila[np];

		switch (t->t) {
		case T_NUM:
			ev_num(r, t->u.n);
			np++;
			break;
		case T_TXT:
			memset(r, 0, sizeof *r);
			r->t = V_TXT;
			r->s = fm->txt ? fm->txt + t->u.x.i : "";
			r->sn = t->u.x.n;
			np++;
			break;
		case T_LOG:
			ev_log(r, t->u.n != 0);
			np++;
			break;
		case T_ERR:
			ev_err(r, (int)t->u.x.i);
			np++;
			break;
		case T_REF:
		case T_RANGO:
			memset(r, 0, sizeof *r);
			if (!lib_hoja_id(x->l, t->h)) {
				ev_err(r, ER_REF);
			} else {
				r->t = V_REF;
				r->h = t->h;
				r->r = cu_rango((u32)t->u.r.f0, t->u.r.c0, (u32)t->u.r.f1, t->u.r.c1);
				if (t->t == T_REF)
					r->r.f1 = r->r.f0, r->r.c1 = r->r.c0;
			}
			np++;
			break;
		case T_NOMBRE: {
			int k = fm->txt ? lib_nombre(x->l, fm->txt + t->u.x.i, t->u.x.n) : -1;

			if (k < 0 || !x->l->nom[k].f || hondo > 16)
				ev_err(r, ER_NOMBRE);
			else
				evalua_en(x, x->l->nom[k].f, r, hondo + 1);
			np++;
			break;
		}
		case T_FALTA:
			memset(r, 0, sizeof *r);
			r->t = V_VACIO;
			r->e = FALTA_E;
			np++;
			break;
		case T_PAREN:
			break;
		case T_DOSP:
			if (np >= 2) {
				ev *a = &pila[np - 2], *b = &pila[np - 1];

				if (a->t == V_ERR)
					;
				else if (b->t == V_ERR)
					*a = *b;
				else if (a->t != V_REF || b->t != V_REF || a->h != b->h)
					ev_err(a, ER_VALOR);
				else {
					rango r = a->r;

					if (b->r.f0 < r.f0)
						r.f0 = b->r.f0;
					if (b->r.f1 > r.f1)
						r.f1 = b->r.f1;
					if (b->r.c0 < r.c0)
						r.c0 = b->r.c0;
					if (b->r.c1 > r.c1)
						r.c1 = b->r.c1;
					a->r = r;
				}
				np--;
			}
			break;
		case T_NEG:
		case T_POS:
		case T_PORC:
			if (np < 1)
				break;
			{
				double v;
				int e = ev_a_num(x, &pila[np - 1], &v);

				if (e)
					ev_err(&pila[np - 1], e);
				else
					ev_num(&pila[np - 1], t->t == T_NEG ? -v : t->t == T_PORC ? v / 100 : v);
			}
			break;
		case T_FUNC: {
			u32 na = t->u.x.n;

			if (na > np)
				na = np;
			np -= na;
			if (t->h >= CU_N_FUNCIONES)
				ev_err(&pila[np], ER_NOMBRE);
			else {
				ev rr;

				fn_llama(x, t->h, &pila[np], (int)na, &rr);
				pila[np] = rr;
			}
			np++;
			break;
		}
		default:
			if (t->t >= T_SUMA && t->t <= T_MAYOR_IG && np >= 2) {
				ev rr;

				opera(x, t->t, &pila[np - 2], &pila[np - 1], &rr);
				np -= 2;
				pila[np++] = rr;
			}
			break;
		}
	}
	if (np)
		*res = pila[np - 1];
	else
		ev_err(res, ER_VALOR);
	if (pila != local)
		so_suelta(pila);
}

/* el resultado, a la celda (un rango da su valor; el vacío, 0) */
static void a_valor(ctx *x, const ev *r0, valor *v)
{
	ev r;

	ev_valor(x, r0, &r);
	switch (r.t) {
	case V_NUM:
		val_num(v, r.n);
		break;
	case V_TXT: {
		char *s = so_pide(r.sn + 1);

		val_suelta(v);
		if (!s) {
			v->t = V_ERR;
			v->e = ER_VALOR;
			break;
		}
		memcpy(s, r.s, r.sn);
		s[r.sn] = 0;
		v->t = V_TXT;
		v->s = s;
		break;
	}
	case V_LOG:
		val_suelta(v);
		v->t = V_LOG;
		v->n = r.n;
		break;
	case V_ERR:
		val_err(v, r.e);
		break;
	default:
		val_num(v, 0);
	}
}

static void calcula_celda(libro *l, hoja *h, celda *k, int hondo)
{
	ctx x;
	ev r;

	memset(&x, 0, sizeof x);
	x.l = l;
	x.h = h;
	x.f = CU_FILA(k->clave);
	x.c = CU_COL(k->clave);
	x.hondo = hondo;
	k->marca |= CM_CALCULANDO;
	evalua_en(&x, k->f, &r, 0);
	a_valor(&x, &r, &k->v);
	if (k->v.t == V_ERR && k->v.e == ER_CIRC && !l->circular)
		l->circular = CU_GCLAVE(h->id, x.f, x.c) + 1;
	k->marca &= (u8)~(CM_SUCIA | CM_CALCULANDO);
	suelta_mem(&x);
}

void calc_evalua(libro *l, hoja *h, u32 f, u32 c, const formula *fm, valor *res)
{
	ctx x;
	ev r;

	memset(&x, 0, sizeof x);
	x.l = l;
	x.h = h;
	x.f = f;
	x.c = c;
	res->t = V_VACIO;
	res->s = NULL;
	evalua_en(&x, fm, &r, 0);
	a_valor(&x, &r, res);
	suelta_mem(&x);
}
