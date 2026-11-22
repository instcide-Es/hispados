/* libro.c - Cuentas 2: el libro, sus hojas y sus celdas dispersas, los
 * estilos sin repetir y lo de poner y leer lo de las celdas (libro.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "libro.h"
#include "formula.h"
#include "numeros.h"
#include "calculo.h"

const cu_local CU_ES = { ',', ';', '.', 0, 0 };
const cu_local CU_ARCHIVO = { '.', ',', ',', 1, 0 };
const cu_local CU_ODS = { '.', ';', ',', 1, 1 };

const char *const NOMBRES_TRAMA[TR_N] = {
	"Ninguna", "Sólida", "Gris 75%", "Gris 50%", "Gris 25%", "Gris 12,5%", "Rayas horizontales", "Rayas verticales",
	"Rayas diagonales (\\)", "Rayas diagonales (/)", "Cuadrícula", "Enrejado"
};
const char *const NOMBRES_SIMBOLO[SIM_N] = {
	"Ninguno", "Flecha arriba", "Flecha abajo", "Marca de bien", "Aspa", "Aviso", "Estrella", "Punto", "Igual"
};
const char *const SIMBOLO_LETRA[SIM_N] = { "", "\xe2\x96\xb2", "\xe2\x96\xbc", "\xe2\x9c\x93", "\xe2\x9c\x97", "!", "\xe2\x98\x85", "\xe2\x97\x8f", "=" };

/* ---- los errores ---- */
static const char *const ERR_ES[ER_N] = { "", "#¡NULO!", "#¡DIV/0!", "#¡VALOR!", "#¡REF!", "#¿NOMBRE?", "#¡NUM!", "#N/A", "#¡CIRC!" };
static const char *const ERR_EN[ER_N] = { "", "#NULL!", "#DIV/0!", "#VALUE!", "#REF!", "#NAME?", "#NUM!", "#N/A", "#CIRC!" };

const char *cu_error_texto(int e, int ingles)
{
	if (e <= 0 || e >= ER_N)
		e = ER_VALOR;
	return ingles ? ERR_EN[e] : ERR_ES[e];
}

int cu_error_de(const char *s, u32 *lg)
{
	int e;

	for (e = ER_NADA + 1; e < ER_N; e++) {
		u32 a = largo(ERR_ES[e]), b = largo(ERR_EN[e]);

		if (strncmp(s, ERR_ES[e], a) == 0) {
			*lg = a;
			return e;
		}
		if (strncmp(s, ERR_EN[e], b) == 0) {
			*lg = b;
			return e;
		}
	}
	/* los de Cuentas de Hispa-386 */
	if (strncmp(s, "#ERROR", 6) == 0) {
		*lg = 6;
		return ER_VALOR;
	}
	if (strncmp(s, "#CIRC", 5) == 0) {
		*lg = 5;
		return ER_CIRC;
	}
	if (strncmp(s, "#REF", 4) == 0) {
		*lg = 4;
		return ER_REF;
	}
	return 0;
}

/* ---- los valores ---- */
void val_suelta(valor *v)
{
	if (v->t == V_TXT)
		so_suelta(v->s);
	v->s = NULL;
	v->t = V_VACIO;
	v->n = 0;
	v->e = 0;
}

void val_copia(valor *d, const valor *o)
{
	if (d == o)
		return;
	val_suelta(d);
	*d = *o;
	if (o->t == V_TXT) {
		d->s = duplica(o->s ? o->s : "");
		if (!d->s) {
			d->t = V_ERR;
			d->e = ER_VALOR;
		}
	}
}

void val_num(valor *v, double n)
{
	val_suelta(v);
	v->t = V_NUM;
	v->n = n;
}

void val_txt(valor *v, const char *s)
{
	val_suelta(v);
	v->s = duplica(s ? s : "");
	v->t = v->s ? V_TXT : V_ERR;
	if (!v->s)
		v->e = ER_VALOR;
}

void val_err(valor *v, int e)
{
	val_suelta(v);
	v->t = V_ERR;
	v->e = (u8)e;
}

/* ---- las referencias como texto ---- */
void cu_col_texto(u32 c, char *d)
{
	char t[4];
	int n = 0, i;

	c++;
	while (c && n < 3) {
		u32 r = (c - 1) % 26;

		t[n++] = (char)('A' + r);
		c = (c - 1) / 26;
	}
	for (i = 0; i < n; i++)
		d[i] = t[n - 1 - i];
	d[n] = 0;
}

void cu_ref_texto(u32 f, u32 c, char *d)
{
	cu_col_texto(c, d);
	fmt(d + largo(d), 12, "%u", f + 1);
}

void cu_rango_texto(rango r, char *d)
{
	if (r.f0 == 0 && r.f1 == CU_FILAS - 1 && !(r.c0 == 0 && r.c1 == CU_COLS - 1)) {
		cu_col_texto(r.c0, d);
		anade(d, ":", 24);
		cu_col_texto(r.c1, d + largo(d));
		return;
	}
	if (r.c0 == 0 && r.c1 == CU_COLS - 1 && !(r.f0 == 0 && r.f1 == CU_FILAS - 1)) {
		fmt(d, 24, "%u:%u", r.f0 + 1, r.f1 + 1);
		return;
	}
	cu_ref_texto(r.f0, r.c0, d);
	if (r.f0 != r.f1 || r.c0 != r.c1) {
		anade(d, ":", 24);
		cu_ref_texto(r.f1, r.c1, d + largo(d));
	}
}

rango cu_rango(u32 f0, u32 c0, u32 f1, u32 c1)
{
	rango r;

	r.f0 = f0 < f1 ? f0 : f1;
	r.f1 = f0 < f1 ? f1 : f0;
	r.c0 = (u16)(c0 < c1 ? c0 : c1);
	r.c1 = (u16)(c0 < c1 ? c1 : c0);
	if (r.f1 >= CU_FILAS)
		r.f1 = CU_FILAS - 1;
	if (r.c1 >= CU_COLS)
		r.c1 = CU_COLS - 1;
	return r;
}

int rg_dentro(rango r, u32 f, u32 c)
{
	return f >= r.f0 && f <= r.f1 && c >= r.c0 && c <= r.c1;
}

int rg_pisa(rango a, rango b)
{
	return !(a.f1 < b.f0 || b.f1 < a.f0 || a.c1 < b.c0 || b.c1 < a.c0);
}

/* ---- las hojas ---- */
hoja *hoja_nueva(const char *nombre, u16 id)
{
	hoja *h = so_pide(sizeof(hoja));
	u32 i;

	if (!h)
		return NULL;
	memset(h, 0, sizeof *h);
	copia(h->nombre, nombre, sizeof h->nombre);
	h->id = id;
	h->ancho_def = CU_ANCHO;
	for (i = 0; i < CU_COLS; i++)
		h->ancho[i] = CU_ANCHO;
	h->zoom = 100;
	h->pag.escala = 100;
	h->pag.marg[0] = h->pag.marg[2] = 1.8;
	h->pag.marg[1] = h->pag.marg[3] = 1.9;
	copia(h->pag.encabezado, "&A", sizeof h->pag.encabezado);
	copia(h->pag.pie, "Página &P de &N", sizeof h->pag.pie);
	return h;
}

void celda_vacia(celda *x)
{
	val_suelta(&x->v);
	if (x->f) {
		fm_suelta(x->f);
		x->f = NULL;
	}
	x->marca = 0;
}

void hoja_suelta(hoja *h)
{
	u32 i;

	if (!h)
		return;
	for (i = 0; i < h->n_cel; i++)
		celda_vacia(&h->cel[i]);
	so_suelta(h->cel);
	so_suelta(h->tabla);
	so_suelta(h->ix);
	so_suelta(h->alto);
	so_suelta(h->oculta_f);
	so_suelta(h->estilo_fila);
	so_suelta(h->comb);
	for (i = 0; i < h->n_com; i++)
		so_suelta(h->com[i].texto);
	so_suelta(h->com);
	so_suelta(h->cond);
	so_suelta(h->gr);
	so_suelta(h->fcol);
	so_suelta(h->salto_f);
	so_suelta(h->salto_c);
	calc_suelta_hoja(h);
	so_suelta(h);
}

void lib_nombre_libre(libro *l, char *d, u32 cabe)
{
	u32 k;

	for (k = 1; k < 10000; k++) {
		fmt(d, cabe, "Hoja%u", k);
		if (!lib_hoja_nombre(l, d, largo(d)))
			return;
	}
}

libro *lib_nuevo(int hojas)
{
	libro *l = so_pide(sizeof(libro));
	estilo e;
	int i;

	if (!l)
		return NULL;
	memset(l, 0, sizeof *l);
	l->loc = CU_ES;
	l->sig_id = 1;
	copia(l->moneda, "€", sizeof l->moneda);
	estilo_base(&e);
	lib_estilo_id(l, &e);
	lib_fmt_id(l, "General");
	if (!l->n_est || !l->n_fmt || calc_inicia(l)) {
		lib_suelta(l);
		return NULL;
	}
	for (i = 0; i < hojas; i++) {
		char n[32];

		lib_nombre_libre(l, n, sizeof n);
		if (!lib_hoja_nueva(l, n, -1)) {
			lib_suelta(l);
			return NULL;
		}
	}
	return l;
}

void lib_suelta(libro *l)
{
	u32 i;

	if (!l)
		return;
	for (i = 0; i < l->n_h; i++)
		hoja_suelta(l->h[i]);
	so_suelta(l->h);
	so_suelta(l->est);
	so_suelta(l->est_tabla);
	for (i = 0; i < l->n_fmt; i++)
		so_suelta(l->fmt[i]);
	so_suelta(l->fmt);
	for (i = 0; i < l->n_nom; i++)
		fm_suelta(l->nom[i].f);
	so_suelta(l->nom);
	calc_suelta(l);
	so_suelta(l);
}

hoja *lib_hoja(libro *l, u32 i)
{
	return l && i < l->n_h ? l->h[i] : NULL;
}

hoja *lib_hoja_id(libro *l, u16 id)
{
	u32 i;

	if (!l)
		return NULL;
	if (l->activa < l->n_h && l->h[l->activa]->id == id)
		return l->h[l->activa];
	for (i = 0; i < l->n_h; i++)
		if (l->h[i]->id == id)
			return l->h[i];
	return NULL;
}

int lib_hoja_indice(libro *l, u16 id)
{
	u32 i;

	for (i = 0; i < l->n_h; i++)
		if (l->h[i]->id == id)
			return (int)i;
	return -1;
}

/* sin mirar mayúsculas (las de UTF-8 también) */
static int nombre_igual(const char *a, const char *b, u32 nb)
{
	u32 i = 0, j = 0, na = largo(a);

	while (i < na && j < nb) {
		u32 x, y;

		i += (u32)utf8_lee((const u8 *)a + i, na - i, &x);
		j += (u32)utf8_lee((const u8 *)b + j, nb - j, &y);
		if (mayuscula(x) != mayuscula(y))
			return 0;
	}
	return i == na && j == nb;
}

hoja *lib_hoja_nombre(libro *l, const char *nombre, u32 n)
{
	u32 i;

	if (!l)
		return NULL;
	for (i = 0; i < l->n_h; i++)
		if (nombre_igual(l->h[i]->nombre, nombre, n))
			return l->h[i];
	return NULL;
}

void lib_hoja_mete(libro *l, hoja *h, u32 i)
{
	hoja **n;

	n = so_repide(l->h, (l->n_h + 1) * sizeof(hoja *));
	if (!n)
		return;
	l->h = n;
	if (i > l->n_h)
		i = l->n_h;
	memmove(l->h + i + 1, l->h + i, (l->n_h - i) * sizeof(hoja *));
	l->h[i] = h;
	l->n_h++;
	if (h->id >= l->sig_id)
		l->sig_id = (u16)(h->id + 1);
}

hoja *lib_hoja_nueva(libro *l, const char *nombre, int pos)
{
	hoja *h;
	u16 id = l->sig_id;

	if (l->n_h >= CU_MAX_HOJAS)
		return NULL;
	if (id >= 255) {
		/* (los id se acaban: el primero que no lleva nadie) */
		for (id = 1; id < 255 && lib_hoja_id(l, id); id++)
			;
		if (id >= 255)
			return NULL;
	}
	h = hoja_nueva(nombre, id);
	if (!h)
		return NULL;
	lib_hoja_mete(l, h, pos < 0 ? l->n_h : (u32)pos);
	if (l->n_h == 0 || l->h[pos < 0 ? l->n_h - 1 : (u32)pos] != h) {
		hoja_suelta(h);
		return NULL;
	}
	return h;
}

hoja *lib_hoja_saca(libro *l, u32 i)
{
	hoja *h;

	if (i >= l->n_h)
		return NULL;
	h = l->h[i];
	memmove(l->h + i, l->h + i + 1, (l->n_h - i - 1) * sizeof(hoja *));
	l->n_h--;
	if (l->activa >= l->n_h && l->n_h)
		l->activa = l->n_h - 1;
	return h;
}

/* ---- las celdas: la tabla de dispersión ---- */
static u32 mezcla(u32 k)
{
	k ^= k >> 16;
	k *= 0x7feb352du;
	k ^= k >> 15;
	k *= 0x846ca68bu;
	k ^= k >> 16;
	return k;
}

static u32 busca_hueco(const hoja *h, u32 clave)
{
	u32 m = h->cabe_tabla - 1, i = mezcla(clave) & m;

	for (;;) {
		u32 e = h->tabla[i];

		if (!e || h->cel[e - 1].clave == clave)
			return i;
		i = (i + 1) & m;
	}
}

celda *hj_celda_k(const hoja *h, u32 clave)
{
	u32 e;

	if (!h || !h->cabe_tabla)
		return NULL;
	e = h->tabla[busca_hueco(h, clave)];
	return e ? &h->cel[e - 1] : NULL;
}

celda *hj_celda(const hoja *h, u32 f, u32 c)
{
	if (f >= CU_FILAS || c >= CU_COLS)
		return NULL;
	return hj_celda_k(h, CU_CLAVE(f, c));
}

static int retabla(hoja *h, u32 cabe)
{
	u32 *t = so_pide(cabe * sizeof(u32)), i;

	if (!t)
		return 1;
	memset(t, 0, cabe * sizeof(u32));
	so_suelta(h->tabla);
	h->tabla = t;
	h->cabe_tabla = cabe;
	for (i = 0; i < h->n_cel; i++)
		h->tabla[busca_hueco(h, h->cel[i].clave)] = i + 1;
	return 0;
}

celda *hj_celda_crea(hoja *h, u32 f, u32 c)
{
	u32 clave = CU_CLAVE(f, c), i;
	celda *x;

	if (f >= CU_FILAS || c >= CU_COLS)
		return NULL;
	x = hj_celda_k(h, clave);
	if (x)
		return x;
	if (h->n_cel >= h->cabe_cel) {
		u32 cabe = h->cabe_cel ? h->cabe_cel * 2 : 64;
		celda *n = so_repide(h->cel, cabe * sizeof(celda));

		if (!n)
			return NULL;
		h->cel = n;
		h->cabe_cel = cabe;
	}
	if ((h->n_cel + 1) * 2 > h->cabe_tabla && retabla(h, h->cabe_tabla ? h->cabe_tabla * 2 : 128))
		return NULL;
	x = &h->cel[h->n_cel];
	memset(x, 0, sizeof *x);
	x->clave = clave;
	h->ix_bien = 0;
	if (f + 1 > h->lim_f)
		h->lim_f = f + 1;
	if (c + 1 > h->lim_c)
		h->lim_c = c + 1;
	i = busca_hueco(h, clave);
	h->tabla[i] = ++h->n_cel;
	return x;
}

static int saca(hoja *h, u32 f, u32 c, celda *d);

void hj_celda_quita(hoja *h, u32 f, u32 c)
{
	celda x;

	if (saca(h, f, c, &x))
		celda_vacia(&x);
}

int hj_celda_saca(hoja *h, u32 f, u32 c, celda *d)
{
	return saca(h, f, c, d);
}

static int saca(hoja *h, u32 f, u32 c, celda *d)
{
	u32 clave = CU_CLAVE(f, c), i, j, m, e, ult;

	if (!h->cabe_tabla || f >= CU_FILAS || c >= CU_COLS)
		return 0;
	i = busca_hueco(h, clave);
	e = h->tabla[i];
	if (!e)
		return 0;
	*d = h->cel[e - 1];
	h->ix_bien = 0;
	/* el hueco de la tabla se cierra (corriendo los de detrás) */
	m = h->cabe_tabla - 1;
	h->tabla[i] = 0;
	j = i;
	for (;;) {
		u32 k, ideal;

		j = (j + 1) & m;
		k = h->tabla[j];
		if (!k)
			break;
		ideal = mezcla(h->cel[k - 1].clave) & m;
		/* ¿se puede mover k de j a i? (si su sitio ideal no está entre i y j) */
		if ((i <= j) ? (ideal <= i || ideal > j) : (ideal <= i && ideal > j)) {
			h->tabla[i] = k;
			h->tabla[j] = 0;
			i = j;
		}
	}
	/* la celda se va del montón: la última pasa a su sitio */
	ult = h->n_cel - 1;
	if (e - 1 != ult) {
		h->cel[e - 1] = h->cel[ult];
		h->tabla[busca_hueco(h, h->cel[e - 1].clave)] = e;
	}
	h->n_cel--;
	return 1;
}

void hj_usado(const hoja *h, u32 *f, u32 *c)
{
	u32 i, mf = CU_SIN_FILA, mc = 0;

	for (i = 0; i < h->n_cel; i++) {
		const celda *x = &h->cel[i];

		if (x->v.t == V_VACIO && !x->f && !x->estilo)
			continue;
		if (mf == CU_SIN_FILA || CU_FILA(x->clave) > mf)
			mf = CU_FILA(x->clave);
		if (CU_COL(x->clave) > mc)
			mc = CU_COL(x->clave);
	}
	*f = mf;
	*c = mc;
}

static void ordena_u32(u32 *a, u32 n)
{
	/* (por montículo: sin recursión) */
	u32 i, k;

	if (n < 2)
		return;
	for (i = n / 2; i-- > 0;) {
		for (k = i;;) {
			u32 h = 2 * k + 1, t;

			if (h >= n)
				break;
			if (h + 1 < n && a[h + 1] > a[h])
				h++;
			if (a[k] >= a[h])
				break;
			t = a[k];
			a[k] = a[h];
			a[h] = t;
			k = h;
		}
	}
	for (i = n - 1; i > 0; i--) {
		u32 t = a[0];

		a[0] = a[i];
		a[i] = t;
		for (k = 0;;) {
			u32 h = 2 * k + 1;

			if (h >= i)
				break;
			if (h + 1 < i && a[h + 1] > a[h])
				h++;
			if (a[k] >= a[h])
				break;
			t = a[k];
			a[k] = a[h];
			a[h] = t;
			k = h;
		}
	}
}

const u32 *hj_columna(hoja *h, u32 c, u32 *n)
{
	*n = 0;
	if (c >= CU_COLS)
		return NULL;
	if (!h->ix_bien) {
		u32 i, k, cuenta[CU_COLS];
		u32 *ix = so_repide(h->ix, (h->n_cel ? h->n_cel : 1) * sizeof(u32));

		if (!ix)
			return NULL;
		h->ix = ix;
		memset(cuenta, 0, sizeof cuenta);
		for (i = 0; i < h->n_cel; i++)
			cuenta[CU_COL(h->cel[i].clave)]++;
		h->ix_ini[0] = 0;
		for (k = 0; k < CU_COLS; k++)
			h->ix_ini[k + 1] = h->ix_ini[k] + cuenta[k];
		memset(cuenta, 0, sizeof cuenta);
		for (i = 0; i < h->n_cel; i++) {
			u32 col = CU_COL(h->cel[i].clave);

			ix[h->ix_ini[col] + cuenta[col]++] = i;
		}
		/* cada columna, por filas (por montículo: sin recursión) */
		for (k = 0; k < CU_COLS; k++) {
			u32 *a = ix + h->ix_ini[k], m = h->ix_ini[k + 1] - h->ix_ini[k], j, ordenada = 1;

			for (j = 1; j < m && ordenada; j++)
				if (h->cel[a[j - 1]].clave > h->cel[a[j]].clave)
					ordenada = 0;
			if (ordenada)
				continue;
			for (j = m / 2; j-- > 0;) {
				u32 q = j;

				for (;;) {
					u32 hh = 2 * q + 1, t;

					if (hh >= m)
						break;
					if (hh + 1 < m && h->cel[a[hh + 1]].clave > h->cel[a[hh]].clave)
						hh++;
					if (h->cel[a[q]].clave >= h->cel[a[hh]].clave)
						break;
					t = a[q];
					a[q] = a[hh];
					a[hh] = t;
					q = hh;
				}
			}
			for (j = m; j-- > 1;) {
				u32 t = a[0], q = 0;

				a[0] = a[j];
				a[j] = t;
				for (;;) {
					u32 hh = 2 * q + 1;

					if (hh >= j)
						break;
					if (hh + 1 < j && h->cel[a[hh + 1]].clave > h->cel[a[hh]].clave)
						hh++;
					if (h->cel[a[q]].clave >= h->cel[a[hh]].clave)
						break;
					t = a[q];
					a[q] = a[hh];
					a[hh] = t;
					q = hh;
				}
			}
		}
		h->ix_bien = 1;
	}
	*n = h->ix_ini[c + 1] - h->ix_ini[c];
	return h->ix + h->ix_ini[c];
}

u32 *hj_claves_ordenadas(const hoja *h, u32 *n)
{
	u32 *a = so_pide((h->n_cel ? h->n_cel : 1) * sizeof(u32)), i;

	*n = 0;
	if (!a)
		return NULL;
	for (i = 0; i < h->n_cel; i++)
		a[i] = h->cel[i].clave;
	ordena_u32(a, h->n_cel);
	*n = h->n_cel;
	return a;
}

u32 hj_ancho(const hoja *h, u32 c)
{
	if (c >= CU_COLS || h->oculta_c[c])
		return 0;
	return h->ancho[c];
}

u32 hj_alto(const hoja *h, u32 f)
{
	if (f >= CU_FILAS || hj_fila_oculta(h, f))
		return 0;
	if (h->alto && h->alto[f])
		return h->alto[f];
	return CU_ALTO;
}

void hj_pon_ancho(hoja *h, u32 c, u32 an)
{
	if (c >= CU_COLS)
		return;
	if (an > 2000)
		an = 2000;
	if (an == 0) {
		h->oculta_c[c] = 1;
		return;
	}
	h->oculta_c[c] = 0;
	h->ancho[c] = (u16)an;
}

void hj_pon_alto(hoja *h, u32 f, u32 al)
{
	if (f >= CU_FILAS)
		return;
	if (al > 800)
		al = 800;
	if (!h->alto) {
		if (!al)
			return;
		h->alto = so_pide(CU_FILAS * sizeof(u16));
		if (!h->alto)
			return;
		memset(h->alto, 0, CU_FILAS * sizeof(u16));
	}
	h->alto[f] = (u16)al;
}

int hj_fila_oculta(const hoja *h, u32 f)
{
	return h->oculta_f && f < CU_FILAS && (h->oculta_f[f >> 3] >> (f & 7) & 1);
}

void hj_oculta_fila(hoja *h, u32 f, int si)
{
	if (f >= CU_FILAS)
		return;
	if (!h->oculta_f) {
		if (!si)
			return;
		h->oculta_f = so_pide(CU_FILAS / 8);
		if (!h->oculta_f)
			return;
		memset(h->oculta_f, 0, CU_FILAS / 8);
	}
	if (si)
		h->oculta_f[f >> 3] |= (u8)(1 << (f & 7));
	else
		h->oculta_f[f >> 3] &= (u8)~(1 << (f & 7));
}

int hj_combinada(const hoja *h, u32 f, u32 c)
{
	u32 i;

	for (i = 0; i < h->n_comb; i++)
		if (rg_dentro(h->comb[i], f, c))
			return (int)i;
	return -1;
}

void hj_descombina(hoja *h, rango r)
{
	u32 i = 0;

	while (i < h->n_comb)
		if (rg_pisa(h->comb[i], r)) {
			h->comb[i] = h->comb[h->n_comb - 1];
			h->n_comb--;
		} else
			i++;
}

void hj_combina(hoja *h, rango r)
{
	rango *n;

	if (r.f0 == r.f1 && r.c0 == r.c1)
		return;
	hj_descombina(h, r);
	n = so_repide(h->comb, (h->n_comb + 1) * sizeof(rango));
	if (!n)
		return;
	h->comb = n;
	h->comb[h->n_comb++] = r;
}

comentario *hj_comentario(const hoja *h, u32 f, u32 c)
{
	u32 i;

	for (i = 0; i < h->n_com; i++)
		if (h->com[i].f == f && h->com[i].c == c)
			return &h->com[i];
	return NULL;
}

void hj_pon_comentario(hoja *h, u32 f, u32 c, const char *texto, const char *autor)
{
	comentario *x = hj_comentario(h, f, c);

	if (!texto || !texto[0]) {
		if (x) {
			so_suelta(x->texto);
			*x = h->com[h->n_com - 1];
			h->n_com--;
		}
		return;
	}
	if (!x) {
		comentario *n = so_repide(h->com, (h->n_com + 1) * sizeof(comentario));

		if (!n)
			return;
		h->com = n;
		x = &h->com[h->n_com++];
		memset(x, 0, sizeof *x);
		x->f = f;
		x->c = (u16)c;
	}
	so_suelta(x->texto);
	x->texto = duplica(texto);
	copia(x->autor, autor ? autor : "", sizeof x->autor);
}

/* ---- los estilos ---- */
void estilo_base(estilo *e)
{
	memset(e, 0, sizeof *e);
	copia(e->familia, CU_FAMILIA, sizeof e->familia);
	e->tam = CU_TAM;
	e->color = CU_AUTO;
	e->fondo = 0xFFFFFF;
	e->color_trama = 0x000000;
	e->color_borde[0] = e->color_borde[1] = e->color_borde[2] = e->color_borde[3] = 0x000000;
}

static u32 firma_estilo(const estilo *e)
{
	const u8 *p = (const u8 *)e;
	u32 h = 2166136261u, i;

	for (i = 0; i < sizeof *e; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

const estilo *lib_estilo(const libro *l, u32 i)
{
	return &l->est[i < l->n_est ? i : 0];
}

u16 lib_estilo_id(libro *l, const estilo *e0)
{
	estilo e;
	u32 m, i;

	/* (con todo a cero lo que no se usa: para que los iguales casen) */
	memset(&e, 0, sizeof e);
	e = *e0;
	{
		u32 k = largo(e.familia);

		memset(e.familia + k, 0, sizeof e.familia - k);
	}
	if (e.trama == TR_NADA) {
		e.fondo = 0xFFFFFF;
		e.color_trama = 0;
	}
	for (i = 0; i < 4; i++)
		if (e.borde[i] == BR_NADA)
			e.color_borde[i] = 0;
	if (e.numfmt >= l->n_fmt)
		e.numfmt = 0;
	if (l->cabe_est_tabla) {
		m = l->cabe_est_tabla - 1;
		for (i = firma_estilo(&e) & m; l->est_tabla[i]; i = (i + 1) & m)
			if (memcmp(&l->est[l->est_tabla[i] - 1], &e, sizeof e) == 0)
				return (u16)(l->est_tabla[i] - 1);
	}
	if (l->n_est >= 65000)
		return 0;
	if (l->n_est >= l->cabe_est) {
		u32 c = l->cabe_est ? l->cabe_est * 2 : 32;
		estilo *n = so_repide(l->est, c * sizeof(estilo));

		if (!n)
			return 0;
		l->est = n;
		l->cabe_est = c;
	}
	if ((l->n_est + 1) * 2 > l->cabe_est_tabla) {
		u32 c = l->cabe_est_tabla ? l->cabe_est_tabla * 2 : 64, k;
		u32 *t = so_pide(c * sizeof(u32));

		if (!t)
			return 0;
		memset(t, 0, c * sizeof(u32));
		so_suelta(l->est_tabla);
		l->est_tabla = t;
		l->cabe_est_tabla = c;
		m = c - 1;
		for (k = 0; k < l->n_est; k++) {
			for (i = firma_estilo(&l->est[k]) & m; t[i]; i = (i + 1) & m)
				;
			t[i] = k + 1;
		}
	}
	l->est[l->n_est] = e;
	m = l->cabe_est_tabla - 1;
	for (i = firma_estilo(&e) & m; l->est_tabla[i]; i = (i + 1) & m)
		;
	l->est_tabla[i] = l->n_est + 1;
	return (u16)l->n_est++;
}

u16 lib_fmt_id(libro *l, const char *codigo)
{
	u32 i;
	char **n;

	if (!codigo || !codigo[0] || iguales_sin_caja(codigo, "General") || iguales_sin_caja(codigo, "Estándar"))
		codigo = "General";
	for (i = 0; i < l->n_fmt; i++)
		if (iguales(l->fmt[i], codigo))
			return (u16)i;
	if (l->n_fmt >= 4000)
		return 0;
	if (l->n_fmt >= l->cabe_fmt) {
		u32 c = l->cabe_fmt ? l->cabe_fmt * 2 : 16;

		n = so_repide(l->fmt, c * sizeof(char *));
		if (!n)
			return 0;
		l->fmt = n;
		l->cabe_fmt = c;
	}
	l->fmt[l->n_fmt] = duplica(codigo);
	if (!l->fmt[l->n_fmt])
		return 0;
	return (u16)l->n_fmt++;
}

const char *lib_fmt(const libro *l, u32 i)
{
	return i < l->n_fmt ? l->fmt[i] : "General";
}

/* ---- poner lo de las celdas ---- */
void lib_cambio(libro *l)
{
	l->version++;
	l->cambiado = 1;
}

u16 hj_estilo_def(const hoja *h, u32 f, u32 c)
{
	if (h->estilo_fila && f < CU_FILAS && h->estilo_fila[f])
		return h->estilo_fila[f];
	return c < CU_COLS ? h->estilo_col[c] : 0;
}

u16 hj_estilo(const hoja *h, u32 f, u32 c)
{
	const celda *x = hj_celda(h, f, c);

	if (x)
		return x->estilo;
	if (h->estilo_fila && f < CU_FILAS && h->estilo_fila[f])
		return h->estilo_fila[f];
	return c < CU_COLS ? h->estilo_col[c] : 0;
}

static celda *para_poner(libro *l, hoja *h, u32 f, u32 c)
{
	celda *x = hj_celda(h, f, c);

	if (!x) {
		u16 e = hj_estilo(h, f, c);

		x = hj_celda_crea(h, f, c);
		if (x)
			x->estilo = e;
	}
	if (!x)
		return NULL;
	if (x->f) {
		calc_quita_formula(l, h, x);
		fm_suelta(x->f);
		x->f = NULL;
	}
	val_suelta(&x->v);
	return x;
}

/* tras poner algo (o quitarlo): si se ha quedado sin nada, fuera */
static void despues(libro *l, hoja *h, u32 f, u32 c)
{
	celda *x = hj_celda(h, f, c);

	calc_cambia(l, h, f, c);
	if (x && x->v.t == V_VACIO && !x->f && x->estilo == hj_estilo_def(h, f, c))
		hj_celda_quita(h, f, c);
	lib_cambio(l);
}

void lib_pon_valor(libro *l, hoja *h, u32 f, u32 c, const valor *v)
{
	celda *x = para_poner(l, h, f, c);

	if (!x)
		return;
	val_copia(&x->v, v);
	despues(l, h, f, c);
}

void lib_pon_numero(libro *l, hoja *h, u32 f, u32 c, double n)
{
	valor v = { V_NUM, 0, n, NULL };

	lib_pon_valor(l, h, f, c, &v);
}

void lib_pon_texto(libro *l, hoja *h, u32 f, u32 c, const char *t)
{
	celda *x = para_poner(l, h, f, c);

	if (!x)
		return;
	if (t && t[0])
		val_txt(&x->v, t);
	despues(l, h, f, c);
}

void lib_pon_formula(libro *l, hoja *h, u32 f, u32 c, formula *fm)
{
	celda *x = para_poner(l, h, f, c);

	if (!x) {
		fm_suelta(fm);
		return;
	}
	x->f = fm;
	if (fm)
		calc_mete_formula(l, h, x);
	despues(l, h, f, c);
}

void lib_pon_estilo(libro *l, hoja *h, u32 f, u32 c, u16 est)
{
	celda *x = hj_celda(h, f, c);

	if (!x && est == hj_estilo_def(h, f, c))
		return;
	if (!x)
		x = hj_celda_crea(h, f, c);
	if (!x)
		return;
	x->estilo = est;
	if (x->v.t == V_VACIO && !x->f && x->estilo == hj_estilo_def(h, f, c))
		hj_celda_quita(h, f, c);
	lib_cambio(l);
}

void lib_borra(libro *l, hoja *h, u32 f, u32 c)
{
	celda *x = hj_celda(h, f, c);

	if (!x)
		return;
	if (x->f)
		calc_quita_formula(l, h, x);
	celda_vacia(x);
	despues(l, h, f, c);
}

int lib_pon_entrada(libro *l, hoja *h, u32 f, u32 c, const char *t, char *err, u32 cabe)
{
	double n;
	int tipo;
	u32 k = 0;

	if (err && cabe)
		err[0] = 0;
	while (t[k] == ' ')
		k++;
	if (!t[0]) {
		lib_borra(l, h, f, c);
		return 0;
	}
	if (t[0] == '\'') {
		lib_pon_texto(l, h, f, c, t + 1);
		return 0;
	}
	if (t[k] == '=' && t[k + 1]) {
		u32 donde;
		formula *fm = fm_lee(l, h, t + k + 1, largo(t + k + 1), &l->loc, err, cabe, &donde);

		if (!fm)
			return 1;
		lib_pon_formula(l, h, f, c, fm);
		return 0;
	}
	if (iguales_sin_caja(t + k, "VERDADERO") || iguales_sin_caja(t + k, "FALSO") || iguales_sin_caja(t + k, "TRUE") || iguales_sin_caja(t + k, "FALSE")) {
		valor v = { V_LOG, 0, 0, NULL };

		v.n = (t[k] == 'V' || t[k] == 'v' || t[k] == 'T' || t[k] == 't') ? 1 : 0;
		lib_pon_valor(l, h, f, c, &v);
		return 0;
	}
	if (t[k] == '#') {
		u32 lg;
		int e = cu_error_de(t + k, &lg);

		if (e && !t[k + lg]) {
			valor v = { V_ERR, 0, 0, NULL };

			v.e = (u8)e;
			lib_pon_valor(l, h, f, c, &v);
			return 0;
		}
	}
	if (num_lee_entrada(t, &l->loc, l->moneda, &n, &tipo)) {
		lib_pon_numero(l, h, f, c, n);
		if (tipo != NE_NADA) {
			celda *x = hj_celda(h, f, c);
			const estilo *e0 = x ? lib_estilo(l, x->estilo) : NULL;

			/* el formato que trae, si la celda no tiene ninguno (o es de fecha y llega otra fecha...) */
			if (x && (e0->numfmt == 0 || (tipo == NE_PORCENTAJE && !busca(lib_fmt(l, e0->numfmt), "%")))) {
				estilo e = *e0;

				e.numfmt = lib_fmt_id(l, num_fmt_de_entrada(tipo, t, &l->loc, l->moneda));
				lib_pon_estilo(l, h, f, c, lib_estilo_id(l, &e));
			}
		}
		return 0;
	}
	lib_pon_texto(l, h, f, c, t);
	return 0;
}

const valor *lib_valor(const hoja *h, u32 f, u32 c)
{
	static const valor vacio = { V_VACIO, 0, 0, NULL };
	const celda *x = hj_celda(h, f, c);

	return x ? &x->v : &vacio;
}

void lib_texto_edicion(libro *l, hoja *h, u32 f, u32 c, char *d, u32 cabe)
{
	celda *x = hj_celda(h, f, c);

	if (!cabe)
		return;
	d[0] = 0;
	if (!x)
		return;
	if (x->f) {
		d[0] = '=';
		fm_texto(l, x->f, h, &l->loc, d + 1, cabe - 1);
		return;
	}
	switch (x->v.t) {
	case V_NUM:
		num_edicion(x->v.n, lib_fmt(l, lib_estilo(l, x->estilo)->numfmt), &l->loc, d, cabe);
		break;
	case V_TXT: {
		/* (un texto que se leería como número o fórmula lleva «'» delante) */
		double n;
		int t;
		const char *s = x->v.s ? x->v.s : "";

		if (s[0] == '=' || s[0] == '\'' || num_lee_entrada(s, &l->loc, l->moneda, &n, &t) ||
		    iguales_sin_caja(s, "VERDADERO") || iguales_sin_caja(s, "FALSO")) {
			d[0] = '\'';
			copia(d + 1, s, cabe - 1);
		} else
			copia(d, s, cabe);
		break;
	}
	case V_LOG:
		copia(d, x->v.n != 0 ? (l->loc.ingles ? "TRUE" : "VERDADERO") : (l->loc.ingles ? "FALSE" : "FALSO"), cabe);
		break;
	case V_ERR:
		copia(d, cu_error_texto(x->v.e, l->loc.ingles), cabe);
		break;
	}
}

void lib_texto_visible(libro *l, const celda *x, char *d, u32 cabe, u32 *color)
{
	const estilo *e = lib_estilo(l, x->estilo);

	if (color)
		*color = CU_AUTO;
	if (!cabe)
		return;
	d[0] = 0;
	switch (x->v.t) {
	case V_NUM:
		num_formatea(x->v.n, lib_fmt(l, e->numfmt), &l->loc, d, cabe, color);
		break;
	case V_TXT:
		num_formatea_texto(x->v.s ? x->v.s : "", lib_fmt(l, e->numfmt), d, cabe);
		break;
	case V_LOG:
		copia(d, x->v.n != 0 ? (l->loc.ingles ? "TRUE" : "VERDADERO") : (l->loc.ingles ? "FALSE" : "FALSO"), cabe);
		break;
	case V_ERR:
		copia(d, cu_error_texto(x->v.e, l->loc.ingles), cabe);
		break;
	}
}

/* ---- los nombres definidos ---- */
int nombre_valido(const char *n)
{
	u32 f, c, i;
	const char *fin;

	if (!n[0] || largo(n) >= 64)
		return 0;
	if ((n[0] >= '0' && n[0] <= '9') || n[0] == '.')
		return 0;
	for (i = 0; n[i]; i++) {
		u8 k = (u8)n[i];

		if (!((k >= 'A' && k <= 'Z') || (k >= 'a' && k <= 'z') || (k >= '0' && k <= '9') || k == '_' || k == '.' || k >= 0x80))
			return 0;
	}
	if (cu_lee_ref(n, &f, &c, &fin) && !*fin)
		return 0;
	if (iguales_sin_caja(n, "VERDADERO") || iguales_sin_caja(n, "FALSO") || iguales_sin_caja(n, "TRUE") || iguales_sin_caja(n, "FALSE"))
		return 0;
	/* (R1C1, C3...) */
	if ((n[0] == 'R' || n[0] == 'r' || n[0] == 'C' || n[0] == 'c') && n[1] >= '0' && n[1] <= '9')
		return 0;
	return 1;
}

int lib_nombre(const libro *l, const char *n, u32 lg)
{
	u32 i;

	for (i = 0; i < l->n_nom; i++)
		if (nombre_igual(l->nom[i].nombre, n, lg))
			return (int)i;
	return -1;
}

int lib_pon_nombre(libro *l, const char *n, formula *f)
{
	int i = lib_nombre(l, n, largo(n));

	if (!f) {
		if (i >= 0) {
			fm_suelta(l->nom[i].f);
			l->nom[i] = l->nom[l->n_nom - 1];
			l->n_nom--;
		}
		calc_rehaz(l);
		return 0;
	}
	if (!nombre_valido(n)) {
		fm_suelta(f);
		return 1;
	}
	if (i < 0) {
		nombre_def *x = so_repide(l->nom, (l->n_nom + 1) * sizeof(nombre_def));

		if (!x) {
			fm_suelta(f);
			return 1;
		}
		l->nom = x;
		i = (int)l->n_nom++;
		memset(&l->nom[i], 0, sizeof l->nom[i]);
	} else
		fm_suelta(l->nom[i].f);
	copia(l->nom[i].nombre, n, sizeof l->nom[i].nombre);
	l->nom[i].f = f;
	calc_rehaz(l);
	lib_cambio(l);
	return 0;
}
