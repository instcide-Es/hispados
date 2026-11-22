/* operar.c - Cuentas 2: lo que se hace con el libro, cada cosa con su paso
 * de deshacer (operar.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "operar.h"
#include "formula.h"
#include "calculo.h"
#include "deshacer.h"
#include "numeros.h"
#include "evalua.h"

static void hecho(libro *l)
{
	calc_recalcula(l);
	lib_cambio(l);
}

/* ---- escribir ---- */
int op_entrada(libro *l, hoja *h, u32 f, u32 c, const char *t, char *err, u32 cabe)
{
	char n[64], r[16];
	int e;

	cu_ref_texto(f, c, r);
	fmt(n, sizeof n, "Escribir en %s", r);
	dh_empieza(l, n);
	dh_celda(l, h, f, c);
	e = lib_pon_entrada(l, h, f, c, t, err, cabe);
	dh_acaba(l);
	hecho(l);
	return e;
}

int op_entrada_rango(libro *l, hoja *h, rango r, const char *t, char *err, u32 cabe)
{
	u32 f, c;
	int e = 0;
	formula *base = NULL;

	dh_empieza(l, "Escribir en lo elegido");
	dh_region(l, h, r);
	/* (una fórmula, corrida a cada celda, como si se copiara) */
	if (t[0] == '=') {
		u32 donde;

		base = fm_lee(l, h, t + 1, largo(t + 1), &l->loc, err, cabe, &donde);
		if (!base) {
			dh_acaba(l);
			return 1;
		}
	}
	for (f = r.f0; f <= r.f1 && !e; f++)
		for (c = r.c0; c <= r.c1; c++) {
			if (base) {
				formula *fm = fm_copia_desplaza(base, (int)(f - r.f0), (int)(c - r.c0));

				if (fm)
					lib_pon_formula(l, h, f, c, fm);
			} else
				e = lib_pon_entrada(l, h, f, c, t, err, cabe);
		}
	fm_suelta(base);
	dh_acaba(l);
	hecho(l);
	return e;
}

/* ---- recorrer lo que hay en un rango ---- */
static u32 *claves(hoja *h, rango r, u32 *n)
{
	u64 area = (u64)(r.f1 - r.f0 + 1) * (r.c1 - r.c0 + 1);
	u32 *k = so_pide((h->n_cel ? h->n_cel : 1) * sizeof(u32)), m = 0, i;

	*n = 0;
	if (!k)
		return NULL;
	if (area < h->n_cel) {
		u32 f, c;

		for (f = r.f0; f <= r.f1; f++)
			for (c = r.c0; c <= r.c1; c++)
				if (hj_celda(h, f, c))
					k[m++] = CU_CLAVE(f, c);
	} else
		for (i = 0; i < h->n_cel; i++)
			if (rg_dentro(r, CU_FILA(h->cel[i].clave), CU_COL(h->cel[i].clave)))
				k[m++] = h->cel[i].clave;
	*n = m;
	return k;
}

static int columnas_enteras(rango r)
{
	return r.f0 == 0 && r.f1 == CU_FILAS - 1;
}

static int filas_enteras(rango r)
{
	return r.c0 == 0 && r.c1 == CU_COLS - 1;
}

void op_borra(libro *l, hoja *h, rango r, int que)
{
	u32 n, i, *k;

	dh_empieza(l, que == BR_FORMATO ? "Borrar formatos" : que == BR_COMENTARIOS ? "Borrar comentarios" : "Borrar");
	dh_region(l, h, r);
	if (que & (BR_FORMATO | BR_COMENTARIOS))
		dh_meta(l, h);
	k = claves(h, r, &n);
	for (i = 0; k && i < n; i++) {
		u32 f = CU_FILA(k[i]), c = CU_COL(k[i]);

		if (que & BR_CONTENIDO)
			lib_borra(l, h, f, c);
		if (que & BR_FORMATO)
			lib_pon_estilo(l, h, f, c, 0);
	}
	so_suelta(k);
	if (que & BR_FORMATO) {
		if (columnas_enteras(r)) {
			u32 c;

			for (c = r.c0; c <= r.c1; c++)
				h->estilo_col[c] = 0;
		}
		if (filas_enteras(r) && h->estilo_fila) {
			u32 f;

			dh_altos(l, h);
			for (f = r.f0; f <= r.f1; f++)
				h->estilo_fila[f] = 0;
		}
		hj_descombina(h, r);
		op_quita_condicional(l, h, r);
	}
	if (que & BR_COMENTARIOS) {
		u32 j = 0;

		while (j < h->n_com)
			if (rg_dentro(r, h->com[j].f, h->com[j].c))
				hj_pon_comentario(h, h->com[j].f, h->com[j].c, NULL, NULL);
			else
				j++;
	}
	dh_acaba(l);
	hecho(l);
}

/* ---- los formatos ---- */
typedef struct {
	u16 de[64], a[64];
	u32 n;
	libro *l;
	op_cambia_estilo cambia;
	const void *u;
} cache_est;

static u16 nuevo_estilo(cache_est *k, u16 viejo)
{
	u32 i;
	estilo e;
	u16 r;

	for (i = 0; i < k->n; i++)
		if (k->de[i] == viejo)
			return k->a[i];
	e = *lib_estilo(k->l, viejo);
	k->cambia(k->l, &e, k->u);
	r = lib_estilo_id(k->l, &e);
	if (k->n < 64) {
		k->de[k->n] = viejo;
		k->a[k->n++] = r;
	}
	return r;
}

void op_formato(libro *l, hoja *h, rango r, op_cambia_estilo cambia, const void *u, const char *nombre)
{
	cache_est K;
	u32 f, c, n, i, *k;

	memset(&K, 0, sizeof K);
	K.l = l;
	K.cambia = cambia;
	K.u = u;
	dh_empieza(l, nombre ? nombre : "Formato");
	dh_region(l, h, r);
	if (columnas_enteras(r) || filas_enteras(r)) {
		/* las filas o columnas enteras: su estilo, y el de las celdas que ya hay */
		if (columnas_enteras(r)) {
			dh_meta(l, h);
			for (c = r.c0; c <= r.c1; c++)
				h->estilo_col[c] = nuevo_estilo(&K, h->estilo_col[c]);
		}
		if (filas_enteras(r) && !(columnas_enteras(r) && !h->estilo_fila)) {
			dh_altos(l, h);
			if (!h->estilo_fila) {
				h->estilo_fila = so_pide(CU_FILAS * sizeof(u16));
				if (h->estilo_fila)
					memset(h->estilo_fila, 0, CU_FILAS * sizeof(u16));
			}
			if (h->estilo_fila)
				for (f = r.f0; f <= r.f1; f++)
					h->estilo_fila[f] = nuevo_estilo(&K, h->estilo_fila[f] ? h->estilo_fila[f] : h->estilo_col[0]);
		} else if (filas_enteras(r) && h->estilo_fila) {
			dh_altos(l, h);
			for (f = 0; f < CU_FILAS; f++)
				if (h->estilo_fila[f])
					h->estilo_fila[f] = nuevo_estilo(&K, h->estilo_fila[f]);
		}
		k = claves(h, r, &n);
		for (i = 0; k && i < n; i++) {
			celda *x = hj_celda_k(h, k[i]);

			if (x)
				lib_pon_estilo(l, h, CU_FILA(k[i]), CU_COL(k[i]), nuevo_estilo(&K, x->estilo));
		}
		so_suelta(k);
	} else {
		for (f = r.f0; f <= r.f1; f++)
			for (c = r.c0; c <= r.c1; c++)
				lib_pon_estilo(l, h, f, c, nuevo_estilo(&K, hj_estilo(h, f, c)));
	}
	dh_acaba(l);
	lib_cambio(l);
}

void op_ancho(libro *l, hoja *h, u32 c0, u32 c1, u32 an)
{
	u32 c;

	dh_empieza(l, "Ancho de columna");
	dh_meta(l, h);
	for (c = c0; c <= c1 && c < CU_COLS; c++)
		hj_pon_ancho(h, c, an ? an : h->ancho_def);
	dh_acaba(l);
	lib_cambio(l);
}

void op_alto(libro *l, hoja *h, u32 f0, u32 f1, u32 al)
{
	u32 f;

	dh_empieza(l, "Alto de fila");
	dh_altos(l, h);
	for (f = f0; f <= f1 && f < CU_FILAS; f++)
		hj_pon_alto(h, f, al);
	dh_acaba(l);
	lib_cambio(l);
}

void op_oculta(libro *l, hoja *h, int eje, u32 de, u32 a, int ocultar)
{
	u32 i;

	dh_empieza(l, ocultar ? "Ocultar" : "Mostrar");
	if (eje == 0) {
		dh_altos(l, h);
		for (i = de; i <= a && i < CU_FILAS; i++)
			hj_oculta_fila(h, i, ocultar);
		calc_rehaz(l);
	} else {
		dh_meta(l, h);
		for (i = de; i <= a && i < CU_COLS; i++)
			h->oculta_c[i] = (u8)ocultar;
	}
	dh_acaba(l);
	hecho(l);
}

void op_autoancho(libro *l, hoja *h, u32 c0, u32 c1, op_mide mide)
{
	u32 i, c;
	u32 an[CU_COLS];

	memset(an, 0, sizeof an);
	for (i = 0; i < h->n_cel; i++) {
		u32 k = h->cel[i].clave;

		c = CU_COL(k);
		if (c < c0 || c > c1 || hj_combinada(h, CU_FILA(k), c) >= 0)
			continue;
		{
			u32 m = mide(l, h, &h->cel[i]);

			if (m > an[c])
				an[c] = m;
		}
	}
	dh_empieza(l, "Autoajustar");
	dh_meta(l, h);
	for (c = c0; c <= c1; c++)
		if (an[c])
			hj_pon_ancho(h, c, an[c] + 8 < 12 ? 12 : an[c] + 8);
		else if (c0 == c1)
			hj_pon_ancho(h, c, h->ancho_def);
	dh_acaba(l);
	lib_cambio(l);
}

/* ---- meter y quitar filas y columnas ---- */
/* corre [a, b] al meter (n > 0) o quitar (n < 0) desde «desde»; 0 si desaparece */
static int corre(u32 *a, u32 *b, u32 desde, int n, u32 tope)
{
	i64 x = *a, y = *b;

	if (n > 0) {
		if (x >= desde)
			x += n;
		if (y >= desde)
			y += n;
		if (x >= tope)
			return 0;
		if (y >= tope)
			y = tope - 1;
	} else {
		i64 k = -n, fin = (i64)desde + k;

		if (x >= desde && y < fin)
			return 0;
		if (x >= fin)
			x -= k;
		else if (x >= desde)
			x = desde;
		if (y >= fin)
			y -= k;
		else if (y >= desde)
			y = (i64)desde - 1;
		if (x > y)
			return 0;
	}
	*a = (u32)x;
	*b = (u32)y;
	return 1;
}

static int corre_rango(rango *r, int eje, u32 desde, int n)
{
	if (eje == 0) {
		if (columnas_enteras(*r))
			return 1;
		return corre(&r->f0, &r->f1, desde, n, CU_FILAS);
	} else {
		u32 a = r->c0, b = r->c1;

		if (filas_enteras(*r))
			return 1;
		if (!corre(&a, &b, desde, n, CU_COLS))
			return 0;
		r->c0 = (u16)a;
		r->c1 = (u16)b;
		return 1;
	}
}

static int corre_pos(u32 *p, int eje, u32 desde, int n)
{
	u32 a = *p, b = *p;

	if (!corre(&a, &b, desde, n, eje == 0 ? CU_FILAS : CU_COLS))
		return 0;
	*p = a;
	return 1;
}

/* las fórmulas de todo el libro (las de fuera de la región que se mueve) */
static void ajusta_formulas(libro *l, hoja *h, int eje, u32 desde, int n, rango movida)
{
	u32 i, j;

	for (i = 0; i < l->n_h; i++) {
		hoja *g = l->h[i];

		for (j = 0; j < g->n_cel; j++) {
			celda *x = &g->cel[j];
			formula *p;

			if (!x->f)
				continue;
			if (g == h && rg_dentro(movida, CU_FILA(x->clave), CU_COL(x->clave)))
				continue;	/* (esa se arregla al moverla) */
			p = fm_copia(x->f);
			if (!p)
				continue;
			if (fm_ajusta(p, g->id, h->id, eje, desde, n)) {
				u32 f = CU_FILA(x->clave), c = CU_COL(x->clave);

				dh_celda(l, g, f, c);
				x = hj_celda(g, f, c);
				fm_suelta(x->f);
				x->f = p;
				calc_cambia(l, g, f, c);
			} else
				fm_suelta(p);
		}
		/* los gráficos de otras hojas con datos de esta */
		for (j = 0; j < g->n_gr; j++)
			if (g->gr[j].hoja == h->id) {
				dh_meta(l, g);
				if (!corre_rango(&g->gr[j].datos, eje, desde, n))
					g->gr[j].datos = cu_rango(0, 0, 0, 0);
			}
	}
	for (i = 0; i < l->n_nom; i++) {
		formula *p = fm_copia(l->nom[i].f);

		if (p && fm_ajusta(p, h->id, h->id, eje, desde, n)) {
			dh_libro(l);
			fm_suelta(l->nom[i].f);
			l->nom[i].f = p;
		} else
			fm_suelta(p);
	}
}

static int estructura(libro *l, hoja *h, int eje, u32 desde, int n)
{
	u32 tope = eje == 0 ? CU_FILAS : CU_COLS, i, m, *k;
	rango mov;
	celda *movidas;
	u32 nm = 0;

	if (desde >= tope || n == 0)
		return 1;
	if (n < 0 && desde + (u32)(-n) > tope)
		n = -(int)(tope - desde);
	mov = eje == 0 ? cu_rango(desde, 0, CU_FILAS - 1, CU_COLS - 1) : cu_rango(0, desde, CU_FILAS - 1, CU_COLS - 1);
	dh_empieza(l, n > 0 ? (eje == 0 ? "Insertar filas" : "Insertar columnas") : (eje == 0 ? "Eliminar filas" : "Eliminar columnas"));
	dh_region(l, h, mov);
	dh_meta(l, h);
	if (eje == 0)
		dh_altos(l, h);
	ajusta_formulas(l, h, eje, desde, n, mov);
	/* las celdas de la región: fuera, corridas, y otra vez dentro */
	k = claves(h, mov, &m);
	movidas = so_pide((m ? m : 1) * sizeof(celda));
	if (!k || !movidas) {
		so_suelta(k);
		so_suelta(movidas);
		dh_acaba(l);
		return 1;
	}
	for (i = 0; i < m; i++) {
		celda x;
		u32 f = CU_FILA(k[i]), c = CU_COL(k[i]), p = eje == 0 ? f : c;

		if (!hj_celda_saca(h, f, c, &x))
			continue;
		calc_cambia(l, h, f, c);
		if (!corre_pos(&p, eje, desde, n)) {
			celda_vacia(&x);
			continue;
		}
		if (x.f)
			fm_ajusta(x.f, h->id, h->id, eje, desde, n);
		x.clave = eje == 0 ? CU_CLAVE(p, c) : CU_CLAVE(f, p);
		movidas[nm++] = x;
	}
	for (i = 0; i < nm; i++) {
		celda *x = hj_celda_crea(h, CU_FILA(movidas[i].clave), CU_COL(movidas[i].clave));

		if (!x) {
			celda_vacia(&movidas[i]);
			continue;
		}
		*x = movidas[i];
		calc_cambia(l, h, CU_FILA(x->clave), CU_COL(x->clave));
	}
	so_suelta(movidas);
	so_suelta(k);
	/* anchos y altos */
	if (eje == 1) {
		u16 a[CU_COLS], e[CU_COLS];
		u8 o[CU_COLS];
		u32 c;

		for (c = 0; c < CU_COLS; c++) {
			a[c] = (u16)h->ancho_def;
			e[c] = 0;
			o[c] = 0;
		}
		for (c = 0; c < CU_COLS; c++) {
			u32 p = c;

			if (c < desde) {
				a[c] = h->ancho[c];
				e[c] = h->estilo_col[c];
				o[c] = h->oculta_c[c];
			} else if (corre_pos(&p, 1, desde, n)) {
				a[p] = h->ancho[c];
				e[p] = h->estilo_col[c];
				o[p] = h->oculta_c[c];
			}
		}
		/* (las nuevas, como la de su izquierda) */
		if (n > 0 && desde > 0)
			for (c = desde; c < desde + (u32)n && c < CU_COLS; c++) {
				a[c] = h->ancho[desde - 1];
				e[c] = h->estilo_col[desde - 1];
			}
		memcpy(h->ancho, a, sizeof a);
		memcpy(h->estilo_col, e, sizeof e);
		memcpy(h->oculta_c, o, sizeof o);
	} else {
		u32 cuantas = n > 0 ? (u32)n : (u32)(-n);

		if (h->alto) {
			if (n > 0) {
				memmove(h->alto + desde + cuantas, h->alto + desde, (CU_FILAS - desde - cuantas) * sizeof(u16));
				for (i = desde; i < desde + cuantas; i++)
					h->alto[i] = 0;
			} else {
				memmove(h->alto + desde, h->alto + desde + cuantas, (CU_FILAS - desde - cuantas) * sizeof(u16));
				for (i = CU_FILAS - cuantas; i < CU_FILAS; i++)
					h->alto[i] = 0;
			}
		}
		if (h->estilo_fila) {
			if (n > 0) {
				memmove(h->estilo_fila + desde + cuantas, h->estilo_fila + desde, (CU_FILAS - desde - cuantas) * sizeof(u16));
				for (i = desde; i < desde + cuantas; i++)
					h->estilo_fila[i] = desde ? h->estilo_fila[desde - 1] : 0;
			} else {
				memmove(h->estilo_fila + desde, h->estilo_fila + desde + cuantas, (CU_FILAS - desde - cuantas) * sizeof(u16));
				for (i = CU_FILAS - cuantas; i < CU_FILAS; i++)
					h->estilo_fila[i] = 0;
			}
		}
		if (h->oculta_f) {
			/* (bit a bit) */
			u8 *o = so_pide(CU_FILAS / 8);

			if (o) {
				memset(o, 0, CU_FILAS / 8);
				for (i = 0; i < CU_FILAS; i++)
					if (hj_fila_oculta(h, i)) {
						u32 p = i;

						if (i < desde || corre_pos(&p, 0, desde, n))
							o[p >> 3] |= (u8)(1 << (p & 7));
					}
				so_suelta(h->oculta_f);
				h->oculta_f = o;
			}
		}
	}
	/* lo demás de la hoja */
	i = 0;
	while (i < h->n_comb)
		if (!corre_rango(&h->comb[i], eje, desde, n))
			h->comb[i] = h->comb[--h->n_comb];
		else
			i++;
	i = 0;
	while (i < h->n_com) {
		u32 p = eje == 0 ? h->com[i].f : h->com[i].c;

		if (!corre_pos(&p, eje, desde, n)) {
			so_suelta(h->com[i].texto);
			h->com[i] = h->com[--h->n_com];
			continue;
		}
		if (eje == 0)
			h->com[i].f = p;
		else
			h->com[i].c = (u16)p;
		i++;
	}
	i = 0;
	while (i < h->n_cond)
		if (!corre_rango(&h->cond[i].r, eje, desde, n))
			h->cond[i] = h->cond[--h->n_cond];
		else
			i++;
	for (i = 0; i < h->n_gr; i++) {
		grafico *g = &h->gr[i];
		u32 p = eje == 0 ? g->f : g->c;

		if (corre_pos(&p, eje, desde, n)) {
			if (eje == 0)
				g->f = p;
			else
				g->c = (u16)p;
		}
		if (g->hoja == h->id && !corre_rango(&g->datos, eje, desde, n))
			g->datos = cu_rango(0, 0, 0, 0);
	}
	if (h->hay_filtro && !corre_rango(&h->filtro, eje, desde, n))
		h->hay_filtro = 0;
	if (h->hay_filtro && eje == 1) {
		i = 0;
		while (i < h->n_fcol) {
			u32 p = h->fcol[i].c;

			if (!corre_pos(&p, 1, desde, n)) {
				h->fcol[i] = h->fcol[--h->n_fcol];
				continue;
			}
			h->fcol[i].c = (u16)p;
			i++;
		}
	}
	if (h->hay_area && !corre_rango(&h->area, eje, desde, n))
		h->hay_area = 0;
	{
		u32 **s = eje == 0 ? &h->salto_f : &h->salto_c, *ns = eje == 0 ? &h->n_salto_f : &h->n_salto_c;

		i = 0;
		while (i < *ns) {
			u32 p = (*s)[i];

			if (!corre_pos(&p, eje, desde, n) || p == 0) {
				(*s)[i] = (*s)[--*ns];
				continue;
			}
			(*s)[i] = p;
			i++;
		}
	}
	/* el cursor */
	{
		u32 p = eje == 0 ? h->cur_f : h->cur_c;

		if (corre_pos(&p, eje, desde, n)) {
			if (eje == 0)
				h->cur_f = p;
			else
				h->cur_c = p;
		}
	}
	calc_rehaz(l);
	dh_acaba(l);
	hecho(l);
	return 0;
}

int op_inserta(libro *l, hoja *h, int eje, u32 desde, u32 n)
{
	return estructura(l, h, eje, desde, (int)n);
}

int op_elimina(libro *l, hoja *h, int eje, u32 desde, u32 n)
{
	return estructura(l, h, eje, desde, -(int)n);
}

/* ---- el portapapeles de Cuentas ---- */
typedef struct {
	int hay;
	u32 al, an;		/* lo que mide */
	celda *cel;		/* las claves, desde (0, 0) */
	u32 n;
	comentario *com;
	u32 n_com;
	rango *comb;
	u32 n_comb;
	u16 anchos[CU_COLS];
	libro *de;		/* (los estilos son del libro de donde se copió) */
	u16 hoja;
	rango origen;
	int cortado;
	u32 version;		/* (si el libro cambia, lo cortado ya no se mueve) */
} recorte;
static recorte R;

void op_recorte_olvida(void)
{
	u32 i;

	for (i = 0; i < R.n; i++)
		celda_vacia(&R.cel[i]);
	so_suelta(R.cel);
	for (i = 0; i < R.n_com; i++)
		so_suelta(R.com[i].texto);
	so_suelta(R.com);
	so_suelta(R.comb);
	memset(&R, 0, sizeof R);
}

int op_hay_recorte(void)
{
	return R.hay;
}

int op_recorte_cortado(rango *r, u16 *h)
{
	if (!R.hay || !R.cortado)
		return 0;
	*r = R.origen;
	*h = R.hoja;
	return 1;
}

static int copia_celda(celda *d, const celda *o)
{
	*d = *o;
	d->marca = 0;
	d->grado = 0;
	d->v.s = NULL;
	d->f = NULL;
	if (o->v.t == V_TXT)
		d->v.s = duplica(o->v.s ? o->v.s : "");
	if (o->f)
		d->f = fm_copia(o->f);
	return 0;
}

char *op_copia(libro *l, hoja *h, rango r, int cortar, u32 *nt)
{
	u32 n, i, *k;
	bufer b = { 0 };

	op_recorte_olvida();
	/* (lo usado, como mucho: copiar la columna A entera es copiar lo que tiene) */
	if (h->lim_f && r.f1 >= h->lim_f && r.f1 - r.f0 > 1000)
		r.f1 = h->lim_f - 1 < r.f0 ? r.f0 : h->lim_f - 1;
	if (h->lim_c && r.c1 >= h->lim_c && r.c1 - r.c0 > 50)
		r.c1 = (u16)(h->lim_c - 1 < r.c0 ? r.c0 : h->lim_c - 1);
	R.al = r.f1 - r.f0 + 1;
	R.an = (u32)r.c1 - r.c0 + 1;
	R.de = l;
	R.hoja = h->id;
	R.origen = r;
	R.cortado = cortar;
	R.version = l->version;
	k = claves(h, r, &n);
	if (n) {
		R.cel = so_pide(n * sizeof(celda));
		for (i = 0; R.cel && i < n; i++) {
			celda *x = hj_celda_k(h, k[i]);

			copia_celda(&R.cel[R.n], x);
			R.cel[R.n].clave = CU_CLAVE(CU_FILA(k[i]) - r.f0, CU_COL(k[i]) - r.c0);
			R.n++;
		}
	}
	so_suelta(k);
	for (i = 0; i < h->n_com; i++)
		if (rg_dentro(r, h->com[i].f, h->com[i].c)) {
			comentario *c = so_repide(R.com, (R.n_com + 1) * sizeof(comentario));

			if (!c)
				break;
			R.com = c;
			c[R.n_com] = h->com[i];
			c[R.n_com].f -= r.f0;
			c[R.n_com].c = (u16)(c[R.n_com].c - r.c0);
			c[R.n_com].texto = duplica(h->com[i].texto ? h->com[i].texto : "");
			R.n_com++;
		}
	for (i = 0; i < h->n_comb; i++)
		if (h->comb[i].f0 >= r.f0 && h->comb[i].f1 <= r.f1 && h->comb[i].c0 >= r.c0 && h->comb[i].c1 <= r.c1) {
			rango *c = so_repide(R.comb, (R.n_comb + 1) * sizeof(rango));

			if (!c)
				break;
			R.comb = c;
			c[R.n_comb] = cu_rango(h->comb[i].f0 - r.f0, h->comb[i].c0 - r.c0, h->comb[i].f1 - r.f0, h->comb[i].c1 - r.c0);
			R.n_comb++;
		}
	for (i = 0; i < R.an && i < CU_COLS; i++)
		R.anchos[i] = (u16)hj_ancho(h, r.c0 + i);
	R.hay = 1;
	/* el texto, para los demás programas (lo que se ve, con tabuladores) */
	{
		u32 f, c;

		if ((u64)R.al * R.an <= 200000)
			for (f = r.f0; f <= r.f1; f++) {
				for (c = r.c0; c <= r.c1; c++) {
					const celda *x = hj_celda(h, f, c);

					if (c > r.c0)
						buf_c(&b, '\t');
					if (x && x->v.t != V_VACIO) {
						char t[400];

						lib_texto_visible(l, x, t, sizeof t, NULL);
						buf_txt(&b, t);
					}
				}
				buf_txt(&b, "\r\n");
			}
	}
	buf_c(&b, 0);
	if (nt)
		*nt = b.n ? b.n - 1 : 0;
	return (char *)b.d;
}

/* el estilo de otro libro, a este */
static u16 estilo_aqui(libro *l, libro *de, u16 e)
{
	estilo x;

	if (de == l)
		return e;
	if (!de)
		return 0;
	x = *lib_estilo(de, e);
	x.numfmt = lib_fmt_id(l, lib_fmt(de, x.numfmt));
	return lib_estilo_id(l, &x);
}

int op_pega(libro *l, hoja *h, u32 f, u32 c, int modo, int transponer, rango *pegado, char *err, u32 cabe)
{
	u32 al, an, i;
	rango d;
	hoja *ho = NULL;

	if (!R.hay) {
		copia(err, "No hay nada copiado", cabe);
		return 1;
	}
	al = transponer ? R.an : R.al;
	an = transponer ? R.al : R.an;
	if (f + al > CU_FILAS || c + an > CU_COLS) {
		copia(err, "Lo copiado no cabe ahí: se sale de la hoja", cabe);
		return 1;
	}
	d = cu_rango(f, c, f + al - 1, c + an - 1);
	if (R.cortado) {
		if (R.de != l || R.version != l->version || !(ho = lib_hoja_id(l, R.hoja)))
			R.cortado = 0;
	}
	if (R.cortado && !transponer && modo == PG_TODO)
		return op_mueve(l, ho, R.origen, f, c, err, cabe);
	dh_empieza(l, modo == PG_VALORES ? "Pegar valores" : modo == PG_FORMATOS ? "Pegar formatos" : "Pegar");
	dh_region(l, h, d);
	dh_meta(l, h);
	/* lo que había, fuera (según lo que se pega) */
	if (modo == PG_TODO || modo == PG_FORMULAS || modo == PG_VALORES) {
		u32 n, *k = claves(h, d, &n);

		for (i = 0; k && i < n; i++)
			lib_borra(l, h, CU_FILA(k[i]), CU_COL(k[i]));
		so_suelta(k);
	}
	if (modo == PG_TODO || modo == PG_FORMATOS) {
		u32 ff, cc;

		for (ff = d.f0; ff <= d.f1; ff++)
			for (cc = d.c0; cc <= d.c1; cc++)
				lib_pon_estilo(l, h, ff, cc, 0);
		hj_descombina(h, d);
	}
	for (i = 0; i < R.n; i++) {
		const celda *x = &R.cel[i];
		u32 rf = CU_FILA(x->clave), rc = CU_COL(x->clave);
		u32 df = transponer ? rc : rf, dc = transponer ? rf : rc;
		u32 tf = f + df, tc = c + dc;

		if (modo == PG_TODO || modo == PG_FORMULAS) {
			if (x->f) {
				/* (corrida como al copiar) */
				int mf = (int)tf - (int)(R.origen.f0 + rf), mc = (int)tc - (int)(R.origen.c0 + rc);
				formula *fm = fm_copia_desplaza(x->f, mf, mc);

				if (fm)
					lib_pon_formula(l, h, tf, tc, fm);
			} else if (x->v.t != V_VACIO)
				lib_pon_valor(l, h, tf, tc, &x->v);
		} else if (modo == PG_VALORES) {
			if (x->v.t != V_VACIO)
				lib_pon_valor(l, h, tf, tc, &x->v);
		}
		if (modo == PG_TODO || modo == PG_FORMATOS) {
			estilo e = *lib_estilo(R.de == l ? l : R.de, x->estilo);

			if (transponer) {
				/* (los bordes se dan la vuelta) */
				u8 b0 = e.borde[BO_IZQ], b1 = e.borde[BO_ARRIBA], b2 = e.borde[BO_DER], b3 = e.borde[BO_ABAJO];
				u32 k0 = e.color_borde[BO_IZQ], k1 = e.color_borde[BO_ARRIBA], k2 = e.color_borde[BO_DER], k3 = e.color_borde[BO_ABAJO];

				e.borde[BO_IZQ] = b1, e.borde[BO_ARRIBA] = b0, e.borde[BO_DER] = b3, e.borde[BO_ABAJO] = b2;
				e.color_borde[BO_IZQ] = k1, e.color_borde[BO_ARRIBA] = k0, e.color_borde[BO_DER] = k3, e.color_borde[BO_ABAJO] = k2;
				if (R.de != l)
					e.numfmt = lib_fmt_id(l, lib_fmt(R.de, e.numfmt));
				lib_pon_estilo(l, h, tf, tc, lib_estilo_id(l, &e));
			} else
				lib_pon_estilo(l, h, tf, tc, estilo_aqui(l, R.de, x->estilo));
		} else if (modo == PG_VALORES && x->estilo) {
			/* (el formato del número sí: un 21% pegado como valor sigue siendo 21%) */
			estilo e = *lib_estilo(l, hj_estilo(h, tf, tc));
			const estilo *o = lib_estilo(R.de == l ? l : R.de, x->estilo);

			if (e.numfmt == 0 && o->numfmt) {
				e.numfmt = R.de == l ? o->numfmt : lib_fmt_id(l, lib_fmt(R.de, o->numfmt));
				lib_pon_estilo(l, h, tf, tc, lib_estilo_id(l, &e));
			}
		}
	}
	if (modo == PG_TODO || modo == PG_FORMATOS)
		for (i = 0; i < R.n_comb; i++) {
			rango q = R.comb[i];

			if (transponer)
				q = cu_rango(q.c0, q.f0, q.c1, q.f1);
			hj_combina(h, cu_rango(f + q.f0, c + q.c0, f + q.f1, c + q.c1));
		}
	if (modo == PG_TODO || modo == PG_COMENTARIOS)
		for (i = 0; i < R.n_com; i++) {
			u32 rf = R.com[i].f, rc = R.com[i].c;

			hj_pon_comentario(h, f + (transponer ? rc : rf), c + (transponer ? rf : rc), R.com[i].texto, R.com[i].autor);
		}
	dh_acaba(l);
	hecho(l);
	if (pegado)
		*pegado = d;
	return 0;
}

int op_mueve(libro *l, hoja *h, rango de, u32 f, u32 c, char *err, u32 cabe)
{
	u32 al = de.f1 - de.f0 + 1, an = (u32)de.c1 - de.c0 + 1, i, j, n, *k;
	int df = (int)f - (int)de.f0, dc = (int)c - (int)de.c0;
	hoja *hd = lib_hoja(l, l->activa);
	rango d;
	celda *fuera;
	u32 nf = 0;

	if (!hd)
		hd = h;
	if (f + al > CU_FILAS || c + an > CU_COLS) {
		copia(err, "No cabe ahí: se sale de la hoja", cabe);
		return 1;
	}
	d = cu_rango(f, c, f + al - 1, c + an - 1);
	if (hd == h && df == 0 && dc == 0)
		return 0;
	dh_empieza(l, "Mover");
	dh_region(l, h, de);
	dh_region(l, hd, d);
	dh_meta(l, h);
	if (hd != h)
		dh_meta(l, hd);
	/* lo que apuntaba a lo movido, a donde va (en todo el libro) */
	for (i = 0; i < l->n_h; i++) {
		hoja *g = l->h[i];

		for (j = 0; j < g->n_cel; j++) {
			celda *x = &g->cel[j];
			formula *p;

			if (!x->f || (g == h && rg_dentro(de, CU_FILA(x->clave), CU_COL(x->clave))))
				continue;
			p = fm_copia(x->f);
			if (p && fm_mueve(p, g->id, h->id, de, df, dc, hd->id)) {
				u32 ff = CU_FILA(x->clave), cc = CU_COL(x->clave);

				dh_celda(l, g, ff, cc);
				x = hj_celda(g, ff, cc);
				fm_suelta(x->f);
				x->f = p;
				calc_cambia(l, g, ff, cc);
			} else
				fm_suelta(p);
		}
	}
	/* las celdas: fuera de su sitio, y a donde van (lo que había allí se pierde) */
	k = claves(h, de, &n);
	fuera = so_pide((n ? n : 1) * sizeof(celda));
	for (i = 0; k && fuera && i < n; i++) {
		celda x;

		if (!hj_celda_saca(h, CU_FILA(k[i]), CU_COL(k[i]), &x))
			continue;
		calc_cambia(l, h, CU_FILA(k[i]), CU_COL(k[i]));
		if (x.f)
			fm_mueve(x.f, h->id, h->id, de, df, dc, hd->id);
		x.clave = CU_CLAVE(CU_FILA(x.clave) + (u32)df, CU_COL(x.clave) + (u32)dc);
		fuera[nf++] = x;
	}
	so_suelta(k);
	k = claves(hd, d, &n);
	for (i = 0; k && i < n; i++) {
		celda *x = hj_celda_k(hd, k[i]);

		if (x && x->f)
			calc_quita_formula(l, hd, x);
		hj_celda_quita(hd, CU_FILA(k[i]), CU_COL(k[i]));
		calc_cambia(l, hd, CU_FILA(k[i]), CU_COL(k[i]));
	}
	so_suelta(k);
	for (i = 0; fuera && i < nf; i++) {
		celda *x = hj_celda_crea(hd, CU_FILA(fuera[i].clave), CU_COL(fuera[i].clave));

		if (!x) {
			celda_vacia(&fuera[i]);
			continue;
		}
		*x = fuera[i];
		x->marca = 0;
		if (x->f)
			x->f->n_nodos = 0, x->f->nodos = NULL;
		calc_cambia(l, hd, CU_FILA(x->clave), CU_COL(x->clave));
	}
	so_suelta(fuera);
	/* los comentarios y las combinadas van con ellas */
	for (i = 0; i < h->n_com; i++)
		if (rg_dentro(de, h->com[i].f, h->com[i].c)) {
			h->com[i].f += (u32)df;
			h->com[i].c = (u16)(h->com[i].c + dc);
		}
	for (i = 0; i < h->n_comb; i++)
		if (h->comb[i].f0 >= de.f0 && h->comb[i].f1 <= de.f1 && h->comb[i].c0 >= de.c0 && h->comb[i].c1 <= de.c1) {
			h->comb[i].f0 += (u32)df;
			h->comb[i].f1 += (u32)df;
			h->comb[i].c0 = (u16)(h->comb[i].c0 + dc);
			h->comb[i].c1 = (u16)(h->comb[i].c1 + dc);
		}
	if (hd != h) {
		/* (a otra hoja: los comentarios pasan a ella) */
		i = 0;
		while (i < h->n_com)
			if (rg_dentro(d, h->com[i].f, h->com[i].c)) {
				hj_pon_comentario(hd, h->com[i].f, h->com[i].c, h->com[i].texto, h->com[i].autor);
				so_suelta(h->com[i].texto);
				h->com[i] = h->com[--h->n_com];
			} else
				i++;
	}
	calc_rehaz(l);
	R.cortado = 0;
	dh_acaba(l);
	hecho(l);
	return 0;
}

void op_pega_texto(libro *l, hoja *h, u32 f, u32 c, const char *t, u32 n, rango *pegado)
{
	u32 i = 0, ff = f, cc = c, maxc = c;
	bufer b = { 0 };

	dh_empieza(l, "Pegar texto");
	/* (la región: se apunta celda a celda, que no se sabe lo que mide) */
	while (i <= n) {
		char k = i < n ? t[i] : 0;

		if (k == '\t' || k == '\n' || k == 0) {
			buf_c(&b, 0);
			if (ff < CU_FILAS && cc < CU_COLS && !(k == 0 && b.n <= 1 && cc == c)) {
				char e[200];

				dh_celda(l, h, ff, cc);
				lib_pon_entrada(l, h, ff, cc, b.d ? (char *)b.d : "", e, sizeof e);
				if (cc > maxc)
					maxc = cc;
			}
			b.n = 0;
			if (k == '\t')
				cc++;
			else {
				if (k == 0)
					break;
				ff++;
				cc = c;
			}
		} else if (k != '\r')
			buf_c(&b, (u8)k);
		i++;
	}
	buf_suelta(&b);
	dh_acaba(l);
	hecho(l);
	if (pegado)
		*pegado = cu_rango(f, c, ff > f ? ff - (t[n ? n - 1 : 0] == '\n' ? 1 : 0) : f, maxc);
}

/* ---- rellenar ---- */
static const char *const *const LISTAS[] = { MESES, MESES_CORTOS, DIAS, DIAS_CORTOS };
static const int LARGOS[] = { 12, 12, 7, 7 };
static const char *const MESES_EN[] = { "january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december" };
static const char *const MESES_EN_C[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
static const char *const DIAS_EN[] = { "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday" };
static const char *const DIAS_EN_C[] = { "sun", "mon", "tue", "wed", "thu", "fri", "sat" };

/* ¿es un nombre de mes o de día? (la lista, la posición y cómo va de mayúsculas: 0 minúsculas, 1 Primera, 2 TODO) */
static int de_lista(const char *s, int *lista, int *pos, int *caja)
{
	const char *const *L[8] = { MESES, MESES_CORTOS, DIAS, DIAS_CORTOS, MESES_EN, MESES_EN_C, DIAS_EN, DIAS_EN_C };
	static const int N[8] = { 12, 12, 7, 7, 12, 12, 7, 7 };
	int k, i;

	(void)LISTAS;
	(void)LARGOS;
	for (k = 0; k < 8; k++)
		for (i = 0; i < N[k]; i++) {
			const char *a = L[k][i];
			u32 x = 0, y = 0, na = largo(a), ns = largo(s);
			int todas_may = 1, primera_may = 0, igual = 1;

			while (x < ns && y < na) {
				u32 p, q;

				x += (u32)utf8_lee((const u8 *)s + x, ns - x, &p);
				y += (u32)utf8_lee((const u8 *)a + y, na - y, &q);
				if (minuscula(p) != q) {
					igual = 0;
					break;
				}
				if (p == q)
					todas_may = 0;
				else if (x <= 2 || (x <= 3 && q > 127))
					primera_may = 1;
			}
			if (igual && x == ns && y == na) {
				*lista = k;
				*pos = i;
				*caja = todas_may ? 2 : primera_may ? 1 : 0;
				return 1;
			}
		}
	return 0;
}

static void de_lista_texto(int lista, int pos, int caja, char *d, u32 cabe)
{
	const char *const *L[8] = { MESES, MESES_CORTOS, DIAS, DIAS_CORTOS, MESES_EN, MESES_EN_C, DIAS_EN, DIAS_EN_C };
	static const int N[8] = { 12, 12, 7, 7, 12, 12, 7, 7 };
	const char *a = L[lista][((pos % N[lista]) + N[lista]) % N[lista]];
	bufer b = { 0 };
	u32 i = 0, n = largo(a);
	int primera = 1;

	while (i < n) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)a + i, n - i, &c);
		if (caja == 2 || (caja == 1 && primera))
			c = mayuscula(c);
		primera = 0;
		buf_u32(&b, c);
	}
	buf_c(&b, 0);
	copia(d, b.d ? (char *)b.d : "", cabe);
	buf_suelta(&b);
}

/* «Producto 7» -> «Producto », 7 */
static int texto_con_numero(const char *s, char *prefijo, u32 cabe, i64 *v, int *cifras)
{
	u32 n = largo(s), i = n;

	while (i > 0 && s[i - 1] >= '0' && s[i - 1] <= '9')
		i--;
	if (i == n || n - i > 15 || i == 0)
		return 0;
	copia(prefijo, s, i + 1 < cabe ? i + 1 : cabe);
	*v = 0;
	{
		u32 k;

		for (k = i; k < n; k++)
			*v = *v * 10 + (s[k] - '0');
	}
	*cifras = (int)(n - i);
	return 1;
}

/* una fila o columna de la fuente, rellena en «cuantas» posiciones más (hacia atrás si «atras») */
static void rellena_vector(libro *l, hoja *h, u32 *pf, u32 *pc, u32 nf, u32 *df, u32 *dc, u32 nd, int atras)
{
	const celda *src[1024];
	u32 i, n = nf < 1024 ? nf : 1024;
	int todos_num = 1, todos_txt_num = 1, todos_lista = 1, hay = 0;
	double a = 0, b = 1;
	char pref[200], pref0[200];
	i64 nv0 = 0, nv1 = 0;
	int cif = 0, lista0 = -1, pos0 = 0, pos1 = 0, caja = 0;

	for (i = 0; i < n; i++) {
		src[i] = hj_celda(h, pf[i], pc[i]);
		if (!src[i] || src[i]->v.t == V_VACIO || src[i]->f) {
			todos_num = todos_txt_num = todos_lista = 0;
			if (src[i] && src[i]->f)
				hay = 1;
			continue;
		}
		hay = 1;
		if (src[i]->v.t != V_NUM)
			todos_num = 0;
		if (src[i]->v.t != V_TXT) {
			todos_txt_num = todos_lista = 0;
			continue;
		}
		{
			i64 v;
			int k;

			if (!texto_con_numero(src[i]->v.s, pref, sizeof pref, &v, &k))
				todos_txt_num = 0;
			else {
				if (i == 0) {
					copia(pref0, pref, sizeof pref0);
					nv0 = v;
					cif = k;
				} else if (!iguales(pref, pref0))
					todos_txt_num = 0;
				nv1 = v;
			}
		}
		{
			int li, po, ca;

			if (!de_lista(src[i]->v.s, &li, &po, &ca))
				todos_lista = 0;
			else {
				if (i == 0) {
					lista0 = li;
					pos0 = po;
					caja = ca;
				} else if (li != lista0)
					todos_lista = 0;
				pos1 = po;
			}
		}
	}
	(void)hay;
	if (todos_num) {
		/* la recta que mejor pasa por los números (con uno solo: el mismo; si es fecha, un día más) */
		if (n == 1) {
			a = src[0]->v.n;
			b = fmt_es_fecha(lib_fmt(l, lib_estilo(l, src[0]->estilo)->numfmt)) ? 1 : 0;
		} else {
			double sx = 0, sy = 0, sxx = 0, sxy = 0;

			for (i = 0; i < n; i++) {
				sx += i;
				sy += src[i]->v.n;
				sxx += (double)i * i;
				sxy += (double)i * src[i]->v.n;
			}
			b = (n * sxy - sx * sy) / (n * sxx - sx * sx);
			a = (sy - b * sx) / n;
		}
	}
	for (i = 0; i < nd; i++) {
		/* la que toca de la fuente (en ciclo) y lo lejos que está */
		u32 k = atras ? (n - 1 - (i % n)) : (i % n);
		i64 paso = atras ? -(i64)(i + 1) : (i64)(n + i);	/* (la posición, contando desde la primera de la fuente) */
		const celda *s = src[k];
		u32 tf = df[i], tc = dc[i];

		dh_celda(l, h, tf, tc);
		if (!s) {
			lib_borra(l, h, tf, tc);
			lib_pon_estilo(l, h, tf, tc, hj_estilo(h, pf[k], pc[k]));
			continue;
		}
		if (s->f) {
			formula *fm = fm_copia_desplaza(s->f, (int)tf - (int)pf[k], (int)tc - (int)pc[k]);

			if (fm)
				lib_pon_formula(l, h, tf, tc, fm);
		} else if (todos_num) {
			lib_pon_numero(l, h, tf, tc, num_redondea(a + b * (double)paso, 12));
		} else if (todos_txt_num && n >= 1) {
			char t[260];
			i64 paso_t = n > 1 ? (nv1 - nv0) / (i64)(n - 1) : 1, v = nv0 + paso_t * paso;

			if (v < 0)
				v = 0;
			fmt(t, sizeof t, "%s%0*u", pref0, cif, (u32)v);
			lib_pon_texto(l, h, tf, tc, t);
		} else if (todos_lista) {
			char t[64];
			int pl = n > 1 ? (pos1 - pos0) / (int)(n - 1) : 1;

			de_lista_texto(lista0, pos0 + pl * (int)paso, caja, t, sizeof t);
			lib_pon_texto(l, h, tf, tc, t);
		} else
			lib_pon_valor(l, h, tf, tc, &s->v);
		lib_pon_estilo(l, h, tf, tc, s->estilo);
	}
}

void op_rellena(libro *l, hoja *h, rango fu, rango de)
{
	u32 pf[1024], pc[1024], df[4096], dc[4096];
	int vertical, atras;
	u32 i, k;

	if (de.f1 > fu.f1) {
		vertical = 1;
		atras = 0;
	} else if (de.f0 < fu.f0) {
		vertical = 1;
		atras = 1;
	} else if (de.c1 > fu.c1) {
		vertical = 0;
		atras = 0;
	} else if (de.c0 < fu.c0) {
		vertical = 0;
		atras = 1;
	} else
		return;
	dh_empieza(l, "Rellenar");
	if (vertical) {
		u32 nd = atras ? fu.f0 - de.f0 : de.f1 - fu.f1, n = fu.f1 - fu.f0 + 1;

		for (k = fu.c0; k <= fu.c1; k++) {
			u32 hecho_ = 0;

			for (i = 0; i < n && i < 1024; i++) {
				pf[i] = fu.f0 + i;
				pc[i] = k;
			}
			while (hecho_ < nd) {
				u32 m = nd - hecho_ > 4096 ? 4096 : nd - hecho_, j;

				for (j = 0; j < m; j++) {
					u32 o = hecho_ + j;

					df[j] = atras ? fu.f0 - 1 - o : fu.f1 + 1 + o;
					dc[j] = k;
				}
				/* (los trozos siguientes siguen la serie: se corre la «fuente» de mentira) */
				if (hecho_ == 0)
					rellena_vector(l, h, pf, pc, n, df, dc, m, atras);
				else {
					/* desde lo ya rellenado */
					u32 pf2[1024], pc2[1024], j2;

					for (j2 = 0; j2 < n && j2 < 1024; j2++) {
						pf2[j2] = atras ? df[0] + 1 + (n - 1 - j2) : df[0] - n + j2;
						pc2[j2] = k;
					}
					rellena_vector(l, h, pf2, pc2, n, df, dc, m, atras);
				}
				hecho_ += m;
			}
		}
	} else {
		u32 nd = atras ? fu.c0 - de.c0 : de.c1 - fu.c1, n = (u32)fu.c1 - fu.c0 + 1;

		for (k = fu.f0; k <= fu.f1; k++) {
			for (i = 0; i < n && i < 1024; i++) {
				pf[i] = k;
				pc[i] = fu.c0 + i;
			}
			for (i = 0; i < nd && i < 4096; i++) {
				df[i] = k;
				dc[i] = atras ? fu.c0 - 1 - i : fu.c1 + 1 + i;
			}
			rellena_vector(l, h, pf, pc, n, df, dc, nd < 4096 ? nd : 4096, atras);
		}
	}
	dh_acaba(l);
	hecho(l);
}

void op_series(libro *l, hoja *h, rango r, int en_columnas, int tipo, double paso, int con_limite, double limite)
{
	u32 i, k, n;

	dh_empieza(l, "Series");
	dh_region(l, h, r);
	n = en_columnas ? r.f1 - r.f0 + 1 : (u32)r.c1 - r.c0 + 1;
	for (k = en_columnas ? r.c0 : r.f0; k <= (en_columnas ? (u32)r.c1 : r.f1); k++) {
		u32 f0 = en_columnas ? r.f0 : k, c0 = en_columnas ? k : r.c0;
		const valor *v0 = lib_valor(h, f0, c0);
		double x;

		if (v0->t != V_NUM)
			continue;
		x = v0->n;
		for (i = 1; i < n; i++) {
			u32 f = en_columnas ? r.f0 + i : k, c = en_columnas ? k : r.c0 + i;

			switch (tipo) {
			case SE_LINEAL: x += paso; break;
			case SE_GEOMETRICA: x *= paso; break;
			case SE_DIAS: x += paso; break;
			case SE_LABORABLES: {
				int p = (int)paso, j;

				for (j = 0; j < (p < 0 ? -p : p); j++) {
					x += p < 0 ? -1 : 1;
					while (dia_semana(x) == 0 || dia_semana(x) == 6)
						x += p < 0 ? -1 : 1;
				}
				break;
			}
			case SE_MESES:
			case SE_ANOS: {
				int a, m, d, m2;

				serie_fecha(v0->n, &a, &m, &d);
				m2 = m + (int)(tipo == SE_MESES ? paso * i : paso * 12 * i);
				{
					int a2 = a + (m2 - 1 >= 0 ? (m2 - 1) / 12 : -((12 - m2) / 12));

					m2 = ((m2 - 1) % 12 + 12) % 12 + 1;
					x = fecha_serie(a2, m2, d > dias_mes(a2, m2) ? dias_mes(a2, m2) : d);
				}
				break;
			}
			}
			if (con_limite && ((paso >= 0 && x > limite) || (paso < 0 && x < limite)))
				break;
			lib_pon_numero(l, h, f, c, num_redondea(x, 12));
			lib_pon_estilo(l, h, f, c, hj_estilo(h, f0, c0));
		}
	}
	dh_acaba(l);
	hecho(l);
}

/* ---- ordenar ---- */
typedef struct {
	u32 fila;
	const valor *v[3];
} fila_orden;

static int clase_orden(const valor *v)
{
	/* números, textos, lógicos, errores; lo vacío, siempre al final */
	switch (v->t) {
	case V_NUM: return 0;
	case V_TXT: return (v->s && v->s[0]) ? 1 : 4;
	case V_LOG: return 2;
	case V_ERR: return 3;
	}
	return 4;
}

static int compara_orden(const fila_orden *a, const fila_orden *b, const int desc[3])
{
	int k;

	for (k = 0; k < 3; k++) {
		const valor *x = a->v[k], *y = b->v[k];
		int cx, cy, r = 0;

		if (!x || !y)
			continue;
		cx = clase_orden(x);
		cy = clase_orden(y);
		if (cx == 4 || cy == 4) {
			if (cx != cy)
				return cx == 4 ? 1 : -1;	/* (lo vacío al final, también de mayor a menor) */
			continue;
		}
		if (cx != cy)
			r = cx < cy ? -1 : 1;
		else if (cx == 0 || cx == 2)
			r = x->n < y->n ? -1 : x->n > y->n ? 1 : 0;
		else if (cx == 1)
			r = ev_texto_cmp(x->s, largo(x->s), y->s, largo(y->s));
		else
			r = x->e < y->e ? -1 : x->e > y->e ? 1 : 0;
		if (r)
			return desc[k] ? -r : r;
	}
	return a->fila < b->fila ? -1 : 1;	/* (estable) */
}

void op_ordena(libro *l, hoja *h, rango r, const int col[3], const int desc[3], int con_titulos)
{
	u32 f0 = r.f0 + (con_titulos ? 1 : 0), n, i, j, m, *k;
	fila_orden *F, t;
	celda *fuera;
	u32 nf = 0, *nueva;
	rango zona;

	if (f0 > r.f1)
		return;
	/* (lo usado, como mucho) */
	if (h->lim_f && r.f1 >= h->lim_f)
		r.f1 = h->lim_f - 1;
	if (f0 > r.f1)
		return;
	n = r.f1 - f0 + 1;
	F = so_pide(n * sizeof(fila_orden));
	nueva = so_pide(n * sizeof(u32));
	if (!F || !nueva) {
		so_suelta(F);
		so_suelta(nueva);
		return;
	}
	for (i = 0; i < n; i++) {
		F[i].fila = f0 + i;
		for (j = 0; j < 3; j++)
			F[i].v[j] = col[j] >= 0 ? lib_valor(h, f0 + i, (u32)col[j]) : NULL;
	}
	/* por montículo, con la fila de antes para que no cambie el orden de las iguales */
	for (i = 1; i < n; i++) {
		/* (por inserción en trozos pequeños; montículo si son muchas) */
		if (n > 64)
			break;
		t = F[i];
		for (j = i; j > 0 && compara_orden(&F[j - 1], &t, desc) > 0; j--)
			F[j] = F[j - 1];
		F[j] = t;
	}
	if (n > 64) {
		/* montículo */
		u32 a, b;

		for (a = n / 2; a-- > 0;)
			for (b = a;;) {
				u32 c = 2 * b + 1;

				if (c >= n)
					break;
				if (c + 1 < n && compara_orden(&F[c + 1], &F[c], desc) > 0)
					c++;
				if (compara_orden(&F[b], &F[c], desc) >= 0)
					break;
				t = F[b];
				F[b] = F[c];
				F[c] = t;
				b = c;
			}
		for (a = n - 1; a > 0; a--) {
			t = F[0];
			F[0] = F[a];
			F[a] = t;
			for (b = 0;;) {
				u32 c = 2 * b + 1;

				if (c >= a)
					break;
				if (c + 1 < a && compara_orden(&F[c + 1], &F[c], desc) > 0)
					c++;
				if (compara_orden(&F[b], &F[c], desc) >= 0)
					break;
				t = F[b];
				F[b] = F[c];
				F[c] = t;
				b = c;
			}
		}
	}
	/* a qué fila va cada una */
	for (i = 0; i < n; i++)
		nueva[F[i].fila - f0] = f0 + i;
	zona = cu_rango(f0, r.c0, r.f1, r.c1);
	dh_empieza(l, "Ordenar");
	dh_region(l, h, zona);
	k = claves(h, zona, &m);
	fuera = so_pide((m ? m : 1) * sizeof(celda));
	for (i = 0; k && fuera && i < m; i++) {
		celda x;
		u32 f = CU_FILA(k[i]), c = CU_COL(k[i]);

		if (!hj_celda_saca(h, f, c, &x))
			continue;
		calc_cambia(l, h, f, c);
		{
			u32 nf2 = nueva[f - f0];

			if (x.f) {
				formula *fm = fm_copia_desplaza(x.f, (int)nf2 - (int)f, 0);

				if (fm) {
					fm_suelta(x.f);
					x.f = fm;
				}
			}
			x.clave = CU_CLAVE(nf2, c);
		}
		fuera[nf++] = x;
	}
	for (i = 0; fuera && i < nf; i++) {
		celda *x = hj_celda_crea(h, CU_FILA(fuera[i].clave), CU_COL(fuera[i].clave));

		if (!x) {
			celda_vacia(&fuera[i]);
			continue;
		}
		*x = fuera[i];
		x->marca = 0;
		calc_cambia(l, h, CU_FILA(x->clave), CU_COL(x->clave));
	}
	so_suelta(fuera);
	so_suelta(k);
	so_suelta(F);
	so_suelta(nueva);
	calc_rehaz(l);
	dh_acaba(l);
	hecho(l);
}

/* ---- el autofiltro ---- */
static int pasa_filtro(libro *l, hoja *h, u32 f, const filtro_col *fc, const double *umbral)
{
	const valor *v = lib_valor(h, f, fc->c);
	char t[400];

	switch (fc->tipo) {
	case FI_TODO:
		return 1;
	case FI_VACIAS:
		return v->t == V_VACIO || (v->t == V_TXT && (!v->s || !v->s[0]));
	case FI_NO_VACIAS:
		return !(v->t == V_VACIO || (v->t == V_TXT && (!v->s || !v->s[0])));
	case FI_MAYOR:
		return v->t == V_NUM && v->n > fc->v;
	case FI_MENOR:
		return v->t == V_NUM && v->n < fc->v;
	case FI_DIEZ_MAS:
		return v->t == V_NUM && v->n >= *umbral;
	case FI_DIEZ_MENOS:
		return v->t == V_NUM && v->n <= *umbral;
	case FI_IGUAL:
	case FI_CONTIENE: {
		const celda *x = hj_celda(h, f, fc->c);

		if (!x || x->v.t == V_VACIO)
			t[0] = 0;
		else
			lib_texto_visible(l, x, t, sizeof t, NULL);
		if (fc->tipo == FI_IGUAL)
			return ev_texto_igual(t, largo(t), fc->texto, largo(fc->texto), 1);
		{
			char p[110];

			fmt(p, sizeof p, "*%s*", fc->texto);
			return ev_texto_igual(t, largo(t), p, largo(p), 1);
		}
	}
	}
	return 1;
}

static double umbral_diez(hoja *h, const rango *r, const filtro_col *fc)
{
	double v[10];
	u32 n = 0, f, i;
	int mas = fc->tipo == FI_DIEZ_MAS;
	u32 cuantos = fc->v >= 1 && fc->v <= 10 ? (u32)fc->v : 10;

	for (f = r->f0 + 1; f <= r->f1; f++) {
		const valor *x = lib_valor(h, f, fc->c);

		if (x->t != V_NUM)
			continue;
		if (n < cuantos) {
			v[n++] = x->n;
		} else {
			/* el peor de los que hay, fuera */
			u32 p = 0;

			for (i = 1; i < n; i++)
				if (mas ? v[i] < v[p] : v[i] > v[p])
					p = i;
			if (mas ? x->n > v[p] : x->n < v[p])
				v[p] = x->n;
		}
	}
	if (!n)
		return mas ? 1e308 : -1e308;
	{
		double u = v[0];

		for (i = 1; i < n; i++)
			if (mas ? v[i] < u : v[i] > u)
				u = v[i];
		return u;
	}
}

void op_filtro_aplica(libro *l, hoja *h)
{
	u32 f, i;
	double umb[64];

	if (!h->hay_filtro)
		return;
	for (i = 0; i < h->n_fcol && i < 64; i++)
		umb[i] = (h->fcol[i].tipo == FI_DIEZ_MAS || h->fcol[i].tipo == FI_DIEZ_MENOS) ? umbral_diez(h, &h->filtro, &h->fcol[i]) : 0;
	for (f = h->filtro.f0 + 1; f <= h->filtro.f1; f++) {
		int bien = 1;

		for (i = 0; i < h->n_fcol && i < 64 && bien; i++)
			bien = pasa_filtro(l, h, f, &h->fcol[i], &umb[i]);
		hj_oculta_fila(h, f, !bien);
	}
	calc_rehaz(l);
}

void op_autofiltro(libro *l, hoja *h, const rango *r)
{
	dh_empieza(l, r ? "Autofiltro" : "Quitar el autofiltro");
	dh_meta(l, h);
	dh_altos(l, h);
	if (h->hay_filtro) {
		u32 f;

		for (f = h->filtro.f0 + 1; f <= h->filtro.f1; f++)
			hj_oculta_fila(h, f, 0);
	}
	so_suelta(h->fcol);
	h->fcol = NULL;
	h->n_fcol = 0;
	h->hay_filtro = r != NULL;
	if (r) {
		h->filtro = *r;
		/* (hasta donde lleguen los datos) */
		if (h->filtro.f1 == h->filtro.f0 && h->lim_f > h->filtro.f0 + 1) {
			u32 f = h->filtro.f0 + 1;

			while (f < h->lim_f) {
				u32 c;
				int hay = 0;

				for (c = h->filtro.c0; c <= h->filtro.c1 && !hay; c++)
					if (lib_valor(h, f, c)->t != V_VACIO)
						hay = 1;
				if (!hay)
					break;
				f++;
			}
			h->filtro.f1 = f - 1;
		}
	}
	calc_rehaz(l);
	dh_acaba(l);
	hecho(l);
}

void op_filtra(libro *l, hoja *h, const filtro_col *fc)
{
	u32 i;

	if (!h->hay_filtro)
		return;
	dh_empieza(l, "Filtrar");
	dh_meta(l, h);
	dh_altos(l, h);
	for (i = 0; i < h->n_fcol; i++)
		if (h->fcol[i].c == fc->c)
			break;
	if (fc->tipo == FI_TODO) {
		if (i < h->n_fcol)
			h->fcol[i] = h->fcol[--h->n_fcol];
	} else {
		if (i == h->n_fcol) {
			filtro_col *n = so_repide(h->fcol, (h->n_fcol + 1) * sizeof(filtro_col));

			if (n) {
				h->fcol = n;
				h->n_fcol++;
			} else
				i = (u32)-1;
		}
		if (i != (u32)-1)
			h->fcol[i] = *fc;
	}
	op_filtro_aplica(l, h);
	dh_acaba(l);
	hecho(l);
}

/* ---- combinar, comentarios, condicionales, gráficos, nombres ---- */
void op_combina(libro *l, hoja *h, rango r, int centrar)
{
	u32 n, i, *k;

	if (r.f0 == r.f1 && r.c0 == r.c1)
		return;
	dh_empieza(l, "Combinar celdas");
	dh_region(l, h, r);
	dh_meta(l, h);
	/* solo queda lo de la primera */
	k = claves(h, r, &n);
	for (i = 0; k && i < n; i++)
		if (k[i] != CU_CLAVE(r.f0, r.c0))
			lib_borra(l, h, CU_FILA(k[i]), CU_COL(k[i]));
	so_suelta(k);
	hj_combina(h, r);
	if (centrar) {
		estilo e = *lib_estilo(l, hj_estilo(h, r.f0, r.c0));

		e.alin_h = AH_CENTRO;
		lib_pon_estilo(l, h, r.f0, r.c0, lib_estilo_id(l, &e));
	}
	dh_acaba(l);
	hecho(l);
}

void op_descombina(libro *l, hoja *h, rango r)
{
	dh_empieza(l, "Separar celdas");
	dh_meta(l, h);
	hj_descombina(h, r);
	dh_acaba(l);
	lib_cambio(l);
}

void op_comentario(libro *l, hoja *h, u32 f, u32 c, const char *t)
{
	dh_empieza(l, t && t[0] ? "Comentario" : "Quitar el comentario");
	dh_meta(l, h);
	hj_pon_comentario(h, f, c, t, l->autor);
	dh_acaba(l);
	lib_cambio(l);
}

void op_quita_condicional(libro *l, hoja *h, rango r)
{
	u32 i = 0;

	dh_meta(l, h);
	while (i < h->n_cond)
		if (rg_pisa(h->cond[i].r, r))
			h->cond[i] = h->cond[--h->n_cond];
		else
			i++;
}

void op_condicional(libro *l, hoja *h, const condicional *c)
{
	condicional *n;

	dh_empieza(l, "Formato condicional");
	dh_meta(l, h);
	n = so_repide(h->cond, (h->n_cond + 1) * sizeof(condicional));
	if (n) {
		h->cond = n;
		h->cond[h->n_cond++] = *c;
	}
	dh_acaba(l);
	lib_cambio(l);
}

int op_grafico(libro *l, hoja *h, int i, const grafico *g)
{
	dh_empieza(l, !g ? "Quitar el gráfico" : i < 0 ? "Gráfico nuevo" : "Gráfico");
	dh_meta(l, h);
	if (!g) {
		if (i >= 0 && (u32)i < h->n_gr) {
			memmove(&h->gr[i], &h->gr[i + 1], (h->n_gr - (u32)i - 1) * sizeof(grafico));
			h->n_gr--;
		}
		i = -1;
	} else if (i < 0) {
		grafico *n = so_repide(h->gr, (h->n_gr + 1) * sizeof(grafico));

		if (n) {
			h->gr = n;
			h->gr[h->n_gr] = *g;
			i = (int)h->n_gr++;
		}
	} else if ((u32)i < h->n_gr)
		h->gr[i] = *g;
	dh_acaba(l);
	lib_cambio(l);
	return i;
}

int op_nombre(libro *l, hoja *h, const char *nombre, const char *ref, char *err, u32 cabe)
{
	formula *f = NULL;

	if (!nombre_valido(nombre)) {
		copia(err, "Ese nombre no vale: empieza por una letra, sin espacios, y no puede parecer una celda (A1)", cabe);
		return 1;
	}
	if (ref) {
		u32 donde;

		f = fm_lee(l, h, ref[0] == '=' ? ref + 1 : ref, largo(ref[0] == '=' ? ref + 1 : ref), &l->loc, err, cabe, &donde);
		if (!f)
			return 1;
		/* (las referencias de un nombre, siempre con su hoja y fijas) */
		{
			u32 i;

			for (i = 0; i < f->n; i++)
				if (f->t[i].t == T_REF || f->t[i].t == T_RANGO)
					f->t[i].a |= RA_HOJA;
		}
	}
	dh_empieza(l, ref ? "Definir nombre" : "Quitar el nombre");
	dh_libro(l);
	lib_pon_nombre(l, nombre, f);
	dh_acaba(l);
	hecho(l);
	return 0;
}

void op_inmoviliza(libro *l, hoja *h, u32 f, u32 c)
{
	dh_empieza(l, f || c ? "Inmovilizar paneles" : "Movilizar paneles");
	dh_meta(l, h);
	h->inm_f = f;
	h->inm_c = c;
	dh_acaba(l);
	lib_cambio(l);
}

void op_area(libro *l, hoja *h, const rango *r)
{
	dh_empieza(l, r ? "Área de impresión" : "Quitar el área de impresión");
	dh_meta(l, h);
	h->hay_area = r != NULL;
	if (r)
		h->area = *r;
	dh_acaba(l);
	lib_cambio(l);
}

void op_salto(libro *l, hoja *h, int eje, u32 pos, int poner)
{
	u32 **s = eje == 0 ? &h->salto_f : &h->salto_c, *n = eje == 0 ? &h->n_salto_f : &h->n_salto_c, i;

	dh_empieza(l, poner ? "Salto de página" : "Quitar el salto de página");
	dh_meta(l, h);
	for (i = 0; i < *n; i++)
		if ((*s)[i] == pos)
			break;
	if (poner && i == *n && pos > 0) {
		u32 *x = so_repide(*s, (*n + 1) * sizeof(u32));

		if (x) {
			*s = x;
			x[(*n)++] = pos;
		}
	} else if (!poner && i < *n)
		(*s)[i] = (*s)[--*n];
	dh_acaba(l);
	lib_cambio(l);
}

void op_pagina(libro *l, hoja *h, const pagina *p)
{
	dh_empieza(l, "Preparar página");
	dh_meta(l, h);
	h->pag = *p;
	dh_acaba(l);
	lib_cambio(l);
}

/* ---- las hojas ---- */
hoja *op_hoja_nueva(libro *l, int pos)
{
	char n[32];
	hoja *h;

	lib_nombre_libre(l, n, sizeof n);
	dh_empieza(l, "Hoja nueva");
	dh_libro(l);
	h = lib_hoja_nueva(l, n, pos);
	dh_acaba(l);
	lib_cambio(l);
	return h;
}

int op_hoja_quita(libro *l, u32 i)
{
	hoja *h;
	u32 k, j;

	if (l->n_h <= 1 || i >= l->n_h)
		return 1;
	h = l->h[i];
	dh_empieza(l, "Eliminar hoja");
	dh_libro(l);
	/* lo que la pedía, #¡REF! */
	for (k = 0; k < l->n_h; k++) {
		hoja *g = l->h[k];

		if (g == h)
			continue;
		for (j = 0; j < g->n_cel; j++) {
			celda *x = &g->cel[j];
			formula *p;

			if (!x->f)
				continue;
			p = fm_copia(x->f);
			if (p && fm_sin_hoja(p, g->id, h->id)) {
				u32 f = CU_FILA(x->clave), c = CU_COL(x->clave);

				dh_celda(l, g, f, c);
				x = hj_celda(g, f, c);
				fm_suelta(x->f);
				x->f = p;
				calc_cambia(l, g, f, c);
			} else
				fm_suelta(p);
		}
	}
	for (k = 0; k < l->n_nom; k++)
		fm_sin_hoja(l->nom[k].f, 0, h->id);
	lib_hoja_saca(l, i);
	calc_hoja_fuera(l, h);
	if (l->activa >= l->n_h)
		l->activa = l->n_h - 1;
	dh_acaba(l);
	/* (si no se ha podido apuntar para deshacer, se suelta aquí) */
	if (!dh_hay(l, 0) || !iguales(dh_nombre(l, 0), "Eliminar hoja"))
		hoja_suelta(h);
	hecho(l);
	return 0;
}

int op_hoja_nombre(libro *l, u32 i, const char *n, char *err, u32 cabe)
{
	hoja *h = lib_hoja(l, i), *otra;
	const char *p;

	if (!h)
		return 1;
	if (!n[0] || utf8_cuenta(n, largo(n)) > 31) {
		copia(err, "El nombre de la hoja tiene que tener de 1 a 31 letras", cabe);
		return 1;
	}
	for (p = n; *p; p++)
		if (strchr("[]:*?/\\", *p)) {
			copia(err, "El nombre de la hoja no puede llevar [ ] : * ? / \\", cabe);
			return 1;
		}
	if (n[0] == '\'') {
		copia(err, "El nombre de la hoja no puede empezar por '", cabe);
		return 1;
	}
	otra = lib_hoja_nombre(l, n, largo(n));
	if (otra && otra != h) {
		copia(err, "Ya hay una hoja con ese nombre", cabe);
		return 1;
	}
	dh_empieza(l, "Nombre de la hoja");
	dh_meta(l, h);
	copia(h->nombre, n, sizeof h->nombre);
	dh_acaba(l);
	lib_cambio(l);
	return 0;
}

void op_hoja_mueve(libro *l, u32 de, u32 a)
{
	hoja *h;

	if (de >= l->n_h || a >= l->n_h || de == a)
		return;
	dh_empieza(l, "Mover la hoja");
	dh_libro(l);
	h = lib_hoja_saca(l, de);
	lib_hoja_mete(l, h, a);
	l->activa = a;
	dh_acaba(l);
	lib_cambio(l);
}

hoja *op_hoja_copia(libro *l, u32 i, int pos)
{
	hoja *o = lib_hoja(l, i), *h;
	char n[110];
	u32 j, k;

	if (!o)
		return NULL;
	for (k = 2; k < 1000; k++) {
		fmt(n, sizeof n, "%s (%u)", o->nombre, k);
		if (utf8_cuenta(n, largo(n)) <= 31 && !lib_hoja_nombre(l, n, largo(n)))
			break;
	}
	dh_empieza(l, "Copiar la hoja");
	dh_libro(l);
	h = lib_hoja_nueva(l, n, pos);
	if (h) {
		u16 id = h->id;
		char nom[96];

		copia(nom, h->nombre, sizeof nom);
		/* lo de la hoja (la parte de arriba de la estructura), y las celdas */
		memcpy(h->ancho, o->ancho, sizeof h->ancho);
		memcpy(h->oculta_c, o->oculta_c, sizeof h->oculta_c);
		memcpy(h->estilo_col, o->estilo_col, sizeof h->estilo_col);
		h->ancho_def = o->ancho_def;
		h->inm_f = o->inm_f;
		h->inm_c = o->inm_c;
		h->pag = o->pag;
		h->sin_rejilla = o->sin_rejilla;
		if (o->alto && (h->alto = so_pide(CU_FILAS * 2)))
			memcpy(h->alto, o->alto, CU_FILAS * 2);
		if (o->estilo_fila && (h->estilo_fila = so_pide(CU_FILAS * 2)))
			memcpy(h->estilo_fila, o->estilo_fila, CU_FILAS * 2);
		for (j = 0; j < o->n_comb; j++)
			hj_combina(h, o->comb[j]);
		for (j = 0; j < o->n_com; j++)
			hj_pon_comentario(h, o->com[j].f, o->com[j].c, o->com[j].texto, o->com[j].autor);
		if (o->n_cond && (h->cond = so_pide(o->n_cond * sizeof(condicional)))) {
			memcpy(h->cond, o->cond, o->n_cond * sizeof(condicional));
			h->n_cond = o->n_cond;
		}
		if (o->n_gr && (h->gr = so_pide(o->n_gr * sizeof(grafico)))) {
			memcpy(h->gr, o->gr, o->n_gr * sizeof(grafico));
			h->n_gr = o->n_gr;
			for (j = 0; j < h->n_gr; j++)
				if (h->gr[j].hoja == o->id)
					h->gr[j].hoja = id;
		}
		for (j = 0; j < o->n_cel; j++) {
			const celda *x = &o->cel[j];
			celda *y = hj_celda_crea(h, CU_FILA(x->clave), CU_COL(x->clave));

			if (!y)
				break;
			x = &o->cel[j];
			copia_celda(y, x);
			if (y->f) {
				for (k = 0; k < y->f->n; k++)
					if ((y->f->t[k].t == T_REF || y->f->t[k].t == T_RANGO) && y->f->t[k].h == o->id)
						y->f->t[k].h = id;
				calc_mete_formula(l, h, y);
			}
		}
		copia(h->nombre, nom, sizeof h->nombre);
		calc_rehaz(l);
	}
	dh_acaba(l);
	hecho(l);
	return h;
}

/* ---- buscar y reemplazar ---- */
static int casa_busca(libro *l, hoja *h, const celda *x, const char *que, int opc)
{
	char t[1200];

	if (opc & BU_FORMULAS)
		lib_texto_edicion(l, h, CU_FILA(x->clave), CU_COL(x->clave), t, sizeof t);
	else
		lib_texto_visible(l, x, t, sizeof t, NULL);
	if (!t[0])
		return 0;
	if (opc & BU_ENTERA) {
		if (opc & BU_CAJA)
			return iguales(t, que);
		return ev_texto_igual(t, largo(t), que, largo(que), 1);
	}
	if (opc & BU_CAJA)
		return busca(t, que) != NULL;
	{
		char p[300];

		fmt(p, sizeof p, "*%s*", que);
		return ev_texto_igual(t, largo(t), p, largo(p), 1);
	}
}

int op_busca(libro *l, hoja *h, const char *que, int opc, u32 *f, u32 *c)
{
	u32 n, i, *k, desde, mejor = 0xFFFFFFFF, primero = 0xFFFFFFFF;

	if (!que[0])
		return 0;
	k = hj_claves_ordenadas(h, &n);
	if (!k)
		return 0;
	/* (por columnas: la clave al revés) */
	desde = (opc & BU_COLUMNAS) ? (*c << 16 | *f) : CU_CLAVE(*f, *c);
	for (i = 0; i < n; i++) {
		celda *x = hj_celda_k(h, k[i]);
		u32 orden = (opc & BU_COLUMNAS) ? (CU_COL(k[i]) << 16 | CU_FILA(k[i])) : k[i];

		if (!x || !casa_busca(l, h, x, que, opc))
			continue;
		if (opc & BU_ATRAS) {
			if (orden < desde && (mejor == 0xFFFFFFFF || orden > mejor))
				mejor = orden;
			if (primero == 0xFFFFFFFF || orden > primero)
				primero = orden;
		} else {
			if (orden > desde && orden < mejor)
				mejor = orden;
			if (orden < primero)
				primero = orden;
		}
	}
	so_suelta(k);
	if (mejor == 0xFFFFFFFF)
		mejor = primero;	/* (vuelta al principio) */
	if (mejor == 0xFFFFFFFF)
		return 0;
	if (opc & BU_COLUMNAS) {
		*c = mejor >> 16;
		*f = mejor & 0xFFFF;
	} else {
		*f = CU_FILA(mejor);
		*c = CU_COL(mejor);
	}
	return 1;
}

/* cambia en t (sin mirar mayúsculas si no se pide) que por por */
static void reemplaza_texto(const char *t, const char *que, const char *por, int caja, bufer *b)
{
	u32 n = largo(t), q = largo(que), i = 0;

	while (i < n) {
		int casa = 0;

		if (i + q <= n) {
			if (caja)
				casa = memcmp(t + i, que, q) == 0;
			else
				casa = ev_texto_igual(t + i, q, que, q, 0);
		}
		if (casa && q) {
			buf_txt(b, por);
			i += q;
		} else
			buf_c(b, (u8)t[i++]);
	}
	buf_c(b, 0);
}

int op_reemplaza(libro *l, hoja *h, const char *que, const char *por, int opc, int todas, u32 f, u32 c)
{
	u32 n, i, *k, hechas = 0;

	if (!que[0])
		return 0;
	dh_empieza(l, "Reemplazar");
	k = todas ? hj_claves_ordenadas(h, &n) : NULL;
	if (!todas) {
		static u32 una;

		una = CU_CLAVE(f, c);
		k = &una;
		n = 1;
	}
	for (i = 0; i < n; i++) {
		celda *x = hj_celda_k(h, k[i]);
		char t[1200], e[200];
		bufer b = { 0 };

		if (!x || !casa_busca(l, h, x, que, opc | BU_FORMULAS))
			continue;
		lib_texto_edicion(l, h, CU_FILA(k[i]), CU_COL(k[i]), t, sizeof t);
		if (opc & BU_ENTERA)
			buf_txt(&b, por), buf_c(&b, 0);
		else
			reemplaza_texto(t, que, por, opc & BU_CAJA, &b);
		dh_celda(l, h, CU_FILA(k[i]), CU_COL(k[i]));
		if (!b.mal && lib_pon_entrada(l, h, CU_FILA(k[i]), CU_COL(k[i]), (char *)b.d, e, sizeof e) == 0)
			hechas++;
		buf_suelta(&b);
	}
	if (todas)
		so_suelta(k);
	dh_acaba(l);
	hecho(l);
	return (int)hechas;
}

typedef struct {
	double suma;
	u32 numeros, llenas;
} suma_t;

void op_suma_rango(libro *l, hoja *h, rango r, double *suma, u32 *numeros, u32 *llenas)
{
	u32 n, i, *k = claves(h, r, &n);

	(void)l;
	*suma = 0;
	*numeros = *llenas = 0;
	for (i = 0; k && i < n; i++) {
		const celda *x = hj_celda_k(h, k[i]);

		if (!x || x->v.t == V_VACIO)
			continue;
		if (hj_fila_oculta(h, CU_FILA(k[i])))
			continue;
		(*llenas)++;
		if (x->v.t == V_NUM) {
			*suma += x->v.n;
			(*numeros)++;
		}
	}
	so_suelta(k);
}
