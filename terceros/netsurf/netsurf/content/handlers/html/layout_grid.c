/*
 * [HISPA] layout_grid.c - CSS grid para el Navegador completo de Hispa-486
 * (NAVEGA2.EXE, docs/486/NAVEGADOR2.md): lo bastante para las rejillas de
 * Bootstrap 5 y de ClicGest. NetSurf 3.11 no tiene grid; aquí una rejilla
 * (display: grid) va como una caja flex (BOX_FLEX) y layout_flex() la manda
 * aquí. Se entiende:
 *   grid-template-columns: longitudes (px, em, rem, %), fr, auto,
 *     minmax(a, b), repeat(N | auto-fill | auto-fit, ...); sin él, una columna;
 *   gap / row-gap / column-gap;
 *   grid-column: «N», «N / M» (M puede ser -1: hasta el final), «span N»,
 *     «N / span M»;
 *   align-items / align-self (stretch, start, center, end) en cada fila.
 * Las filas, del alto de lo que llevan (grid-auto-rows: auto); los
 * elementos van por orden, fila a fila (grid-auto-flow: row). No: las
 * áreas con nombre (grid-template-areas), grid-template-rows, grid-row,
 * el orden (order) ni «dense».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */

#include <string.h>
#include <stdlib.h>

#include "utils/log.h"
#include "utils/utils.h"
#include "html/box.h"
#include "html/html.h"
#include "html/private.h"
#include "html/box_inspect.h"
#include "html/layout_internal.h"

#define PISTAS_MAX 64
#define FILAS_MAX 4096

/* una columna (pista) */
struct pista {
	int min;        /* lo mínimo (puntos); -1: el del contenido (auto) */
	int max;        /* lo máximo (puntos); -1: el del contenido; -2: fr */
	css_fixed fr;   /* si max == -2 */
	int ancho;      /* lo que se le da */
};

/* lee una parte («230px», «1fr», «auto», «0»...) */
static const char *parte(const char *p, char *t, size_t n)
{
	size_t i = 0;
	while (*p == ' ')
		p++;
	if (*p == '(' || *p == ')' || *p == ',' || *p == '/') {
		t[0] = *p;
		t[1] = 0;
		return p + 1;
	}
	while (*p && *p != ' ' && *p != '(' && *p != ')' && *p != ',' &&
			*p != '/') {
		if (i + 1 < n)
			t[i++] = *p;
		p++;
	}
	/* una función: «repeat(», «minmax(» */
	if (*p == '(' && i + 1 < n) {
		t[i++] = '(';
		p++;
	}
	t[i] = 0;
	return p;
}

/* un número en coma fija; *resto: lo que va después (la unidad) */
static css_fixed numero(const char *t, const char **resto)
{
	int neg = 0;
	int64_t ent = 0, frac = 0, div = 1;
	if (*t == '-') {
		neg = 1;
		t++;
	} else if (*t == '+') {
		t++;
	}
	while (*t >= '0' && *t <= '9')
		ent = ent * 10 + (*t++ - '0');
	if (*t == '.') {
		t++;
		while (*t >= '0' && *t <= '9') {
			if (div < 100000) {
				frac = frac * 10 + (*t - '0');
				div *= 10;
			}
			t++;
		}
	}
	*resto = t;
	{
		css_fixed v = INTTOFIX(ent) + (css_fixed)((frac << 10) / div);
		return neg ? -v : v;
	}
}

static bool es_numero(const char *t)
{
	if (*t == '-' || *t == '+')
		t++;
	return (*t >= '0' && *t <= '9') || (*t == '.' && t[1] >= '0' &&
			t[1] <= '9');
}

/*
 * Un tamaño de pista: una longitud (en puntos), «fr» (-2, con *fr), «auto»,
 * «min-content», «max-content» (-1). false si no se entiende.
 */
static bool tamano(const char *t, const struct box *g,
		const css_unit_ctx *ctx, int ancho, int *v, css_fixed *fr)
{
	const char *u;
	css_fixed n;
	css_unit unidad;

	if (!strcmp(t, "auto") || !strcmp(t, "min-content") ||
			!strcmp(t, "max-content")) {
		*v = -1;
		return true;
	}
	if (!es_numero(t))
		return false;
	n = numero(t, &u);
	if (!strcmp(u, "fr")) {
		*v = -2;
		*fr = n;
		return true;
	}
	if (*u == 0 || !strcmp(u, "px"))
		unidad = CSS_UNIT_PX;
	else if (!strcmp(u, "em"))
		unidad = CSS_UNIT_EM;
	else if (!strcmp(u, "rem"))
		unidad = CSS_UNIT_REM;
	else if (!strcmp(u, "pt"))
		unidad = CSS_UNIT_PT;
	else if (!strcmp(u, "vw"))
		unidad = CSS_UNIT_VW;
	else if (!strcmp(u, "vh"))
		unidad = CSS_UNIT_VH;
	else if (!strcmp(u, "%")) {
		*v = (ancho == AUTO || ancho < 0) ? -1 :
				FPCT_OF_INT_TOINT(n, ancho);
		return true;
	} else
		return false;
	*v = FIXTOINT(css_unit_len2device_px(g->style, ctx, n, unidad));
	if (*v < 0)
		*v = 0;
	return true;
}

/*
 * Lee una pista («1fr», «minmax( 230px , 1fr )», «auto»...) desde p;
 * devuelve dónde sigue, o NULL si no se entiende.
 */
static const char *lee_pista(const char *p, const struct box *g,
		const css_unit_ctx *ctx, int ancho, struct pista *pi)
{
	char t[48];
	int v;
	css_fixed fr = 0;

	p = parte(p, t, sizeof t);
	if (!strcmp(t, "minmax(") || !strcmp(t, "fit-content(")) {
		bool fit = t[0] == 'f';
		char a[48], b[48];
		int va, vb;
		css_fixed fa = 0, fb = 0;
		p = parte(p, a, sizeof a);
		if (fit) {
			strcpy(b, a);
			strcpy(a, "auto");
		} else {
			p = parte(p, t, sizeof t);
			if (strcmp(t, ","))
				return NULL;
			p = parte(p, b, sizeof b);
		}
		p = parte(p, t, sizeof t);
		if (strcmp(t, ")"))
			return NULL;
		if (!tamano(a, g, ctx, ancho, &va, &fa) ||
				!tamano(b, g, ctx, ancho, &vb, &fb))
			return NULL;
		pi->min = va == -2 ? -1 : va;
		pi->max = vb;
		pi->fr = fb;
		return p;
	}
	if (!tamano(t, g, ctx, ancho, &v, &fr))
		return NULL;
	if (v == -2) {
		/* «1fr» es minmax(auto, 1fr) */
		pi->min = -1;
		pi->max = -2;
		pi->fr = fr;
	} else {
		pi->min = v;
		pi->max = v;
	}
	return p;
}

/* lo mínimo que ocupa una pista sin el contenido (para auto-fill) */
static int pista_fija(const struct pista *pi)
{
	if (pi->max >= 0)
		return pi->min >= 0 ? max(pi->min, pi->max) : pi->max;
	return pi->min > 0 ? pi->min : 0;
}

/*
 * Las columnas de la rejilla (grid-template-columns), con las repeticiones
 * hechas; n_elem: cuántos elementos hay (auto-fit). Devuelve cuántas.
 */
static int columnas(const struct box *g, const css_unit_ctx *ctx, int ancho,
		int cgap, int n_elem, struct pista *pi)
{
	lwc_string *txt = NULL;
	const char *p;
	char t[48];
	int n = 0;

	if (g->style == NULL || css_computed_grid_template_columns(g->style,
			&txt) != CSS_GRID_TEMPLATE_COLUMNS_SET || txt == NULL)
		goto una;
	p = lwc_string_data(txt);
	for (;;) {
		const char *q;
		while (*p == ' ')
			p++;
		if (*p == 0)
			break;
		q = parte(p, t, sizeof t);
		if (!strcmp(t, "repeat(")) {
			struct pista rep[16];
			int nr = 0, veces = 0, k, auto_fit = 0, auto_fill = 0;
			p = parte(q, t, sizeof t);
			if (!strcmp(t, "auto-fill"))
				auto_fill = 1;
			else if (!strcmp(t, "auto-fit"))
				auto_fit = 1;
			else if (es_numero(t))
				veces = atoi(t);
			else
				goto una;
			p = parte(p, t, sizeof t);
			if (strcmp(t, ","))
				goto una;
			for (;;) {
				while (*p == ' ')
					p++;
				if (*p == ')') {
					p++;
					break;
				}
				if (nr == 16 || *p == 0)
					goto una;
				p = lee_pista(p, g, ctx, ancho, &rep[nr]);
				if (p == NULL)
					goto una;
				nr++;
			}
			if (nr == 0)
				goto una;
			if (auto_fill || auto_fit) {
				/* cuántas caben (al menos una) */
				int uno = 0;
				for (k = 0; k < nr; k++)
					uno += pista_fija(&rep[k]) + cgap;
				veces = 1;
				if (uno > 0 && ancho != AUTO && ancho > 0)
					veces = (ancho + cgap) / uno;
				if (veces < 1)
					veces = 1;
				/* auto-fit: las que se quedarían vacías, fuera
				 * (así las que hay se estiran) */
				if (auto_fit && veces * nr > n_elem && n_elem > 0)
					veces = (n_elem + nr - 1) / nr;
			}
			if (veces < 1)
				veces = 1;
			for (; veces > 0 && n + nr <= PISTAS_MAX; veces--)
				for (k = 0; k < nr; k++)
					pi[n++] = rep[k];
		} else {
			if (n == PISTAS_MAX)
				break;
			p = lee_pista(p, g, ctx, ancho, &pi[n]);
			if (p == NULL)
				goto una;
			n++;
		}
	}
	if (n > 0)
		return n;
una:
	/* sin columnas (o no se entienden): una que lo ocupa todo */
	pi[0].min = -1;
	pi[0].max = -2;
	pi[0].fr = INTTOFIX(1);
	return 1;
}

/* grid-column de un elemento: la columna (desde 0) y cuántas ocupa;
 * *col = -1 si va donde le toque */
static void columna_de(const struct box *b, int ncol, int *col, int *span)
{
	lwc_string *txt = NULL;
	const char *p;
	char t[48];
	int a = 0, e = 0, sp = 0, hay_barra = 0, sp_fin = 0;

	*col = -1;
	*span = 1;
	if (b->style == NULL || css_computed_grid_column(b->style, &txt) !=
			CSS_GRID_COLUMN_SET || txt == NULL)
		return;
	p = lwc_string_data(txt);
	while (*p) {
		p = parte(p, t, sizeof t);
		if (t[0] == 0)
			break;
		if (!strcmp(t, "/")) {
			hay_barra = 1;
		} else if (!strcmp(t, "span")) {
			char n[48];
			p = parte(p, n, sizeof n);
			if (hay_barra)
				sp_fin = atoi(n);
			else
				sp = atoi(n);
		} else if (es_numero(t)) {
			if (hay_barra)
				e = atoi(t);
			else
				a = atoi(t);
		}
	}
	/* las líneas van de 1 a ncol + 1; las negativas, desde el final */
	if (a < 0)
		a = ncol + 2 + a;
	if (e < 0)
		e = ncol + 2 + e;
	if (a > 0) {
		*col = a - 1;
		if (e > a)
			*span = e - a;
		else if (sp_fin > 0)
			*span = sp_fin;
	} else if (sp > 0) {
		*span = sp;
	} else if (e > 1) {
		*col = e - 2;
	}
	if (*col >= ncol)
		*col = ncol - 1;
	if (*span < 1)
		*span = 1;
	if (*col >= 0 && *col + *span > ncol)
		*span = ncol - *col;
	if (*span > ncol)
		*span = ncol;
}

/* maqueta un elemento con su ancho ya puesto */
static bool maqueta(struct box *b, int ancho, html_content *content)
{
	bool bien;
	switch (b->type) {
	case BOX_BLOCK:
		bien = layout_block_context(b, -1, content);
		break;
	case BOX_TABLE:
		b->float_container = b->parent;
		bien = layout_table(b, ancho, content);
		b->float_container = NULL;
		break;
	case BOX_FLEX:
		b->float_container = b->parent;
		bien = layout_flex(b, ancho, content);
		b->float_container = NULL;
		break;
	default:
		bien = true;
		break;
	}
	return bien;
}

struct elem {
	struct box *b;
	int col, span, fila;
	int min, max; /* del contenido (con márgenes) */
};

void layout_grid_minmax(struct box *grid, const css_unit_ctx *unit_len_ctx,
		int n, int *min, int *max)
{
	struct pista pi[PISTAS_MAX];
	int cgap = lh__gap(unit_len_ctx, grid, false, AUTO);
	int k, nc, mi = 0, ma = 0;
	int hijo_min = *min, hijo_max = *max;

	nc = columnas(grid, unit_len_ctx, AUTO, cgap, n, pi);
	if (nc > n && n > 0)
		nc = n;
	for (k = 0; k < nc; k++) {
		int f = pista_fija(&pi[k]);
		mi += pi[k].min >= 0 ? pi[k].min : hijo_min;
		ma += pi[k].max >= 0 ? max(f, 0) : max(hijo_max, f);
	}
	if (nc > 1) {
		mi += cgap * (nc - 1);
		ma += cgap * (nc - 1);
	}
	*min = mi;
	*max = max(ma, mi);
}

bool layout_grid(struct box *grid, int available_width,
		html_content *content)
{
	const css_unit_ctx *ctx = &content->unit_len_ctx;
	struct pista pi[PISTAS_MAX];
	struct elem *el;
	int *fila_alto, *fila_y;
	int n = 0, nc, k, i, ancho, cgap, rgap, max_height, min_height;
	int nfilas = 0, fila, col, libre, alto;
	css_fixed suma_fr;
	struct box *b;
	bool bien = true;

	layout_find_dimensions(ctx, available_width, -1, grid, grid->style,
			NULL, &grid->height, NULL, NULL, &max_height,
			&min_height, grid->margin, grid->padding, grid->border);
	ancho = min(available_width, grid->width);
	if (ancho == AUTO || ancho < 0)
		ancho = available_width;
	cgap = lh__gap(ctx, grid, false, ancho);
	rgap = lh__gap(ctx, grid, true, ancho);

	for (b = grid->children; b != NULL; b = b->next)
		n++;
	el = calloc(n + 1, sizeof *el);
	fila_alto = calloc(n + 1, sizeof *fila_alto);
	fila_y = calloc(n + 1, sizeof *fila_y);
	if (el == NULL || fila_alto == NULL || fila_y == NULL) {
		free(el);
		free(fila_alto);
		free(fila_y);
		return false;
	}

	/* los elementos: sus medidas y su contenido (mín. y máx.) */
	i = 0;
	for (b = grid->children; b != NULL; b = b->next) {
		struct elem *e = &el[i++];
		e->b = b;
		b->float_container = b->parent;
		layout_find_dimensions(ctx, ancho, -1, b, b->style,
				&b->width, &b->height, NULL, NULL, NULL,
				NULL, b->margin, b->padding, b->border);
		b->float_container = NULL;
		e->min = b->min_width;
		e->max = b->max_width;
	}

	/* las columnas */
	{
		int en_flujo = 0;
		for (k = 0; k < n; k++)
			if (!lh__box_is_absolute(el[k].b))
				en_flujo++;
		nc = columnas(grid, ctx, ancho, cgap, en_flujo, pi);
	}

	/* dónde va cada uno (fila a fila) */
	fila = 0;
	col = 0;
	for (k = 0; k < n; k++) {
		struct elem *e = &el[k];
		int c, sp;
		if (lh__box_is_absolute(e->b)) {
			e->col = -1;
			continue;
		}
		columna_de(e->b, nc, &c, &sp);
		if (c >= 0) {
			if (c < col)
				fila++;
			col = c;
		} else if (col + sp > nc) {
			fila++;
			col = 0;
		}
		e->col = col;
		e->span = sp;
		e->fila = fila;
		col += sp;
		if (col >= nc) {
			col = 0;
			fila++;
		}
		if (e->fila + 1 > nfilas)
			nfilas = e->fila + 1;
	}

	/* el ancho de las columnas: lo fijo; lo de «auto», el contenido; lo
	 * que sobra, a las fr (o, sin fr, a las auto) */
	libre = ancho - (nc > 1 ? cgap * (nc - 1) : 0);
	for (k = 0; k < nc; k++) {
		int cmin = 0, cmax = 0;
		for (i = 0; i < n; i++) {
			if (el[i].col == k && el[i].span == 1) {
				cmin = max(cmin, el[i].min);
				cmax = max(cmax, el[i].max);
			}
		}
		if (pi[k].min < 0)
			pi[k].min = pi[k].max == -2 ? cmin : cmin;
		if (pi[k].max == -1)
			pi[k].ancho = max(pi[k].min, cmax);
		else if (pi[k].max >= 0)
			pi[k].ancho = max(pi[k].min, pi[k].max);
		else
			pi[k].ancho = pi[k].min;
	}
	/* las fr: el reparto, con las que no llegan a su mínimo fuera */
	{
		bool fijo[PISTAS_MAX];
		int vuelta;
		memset(fijo, 0, sizeof fijo);
		for (k = 0; k < nc; k++)
			fijo[k] = pi[k].max != -2;
		for (vuelta = 0; vuelta < 8; vuelta++) {
			int resto = libre;
			bool otra = false;
			suma_fr = 0;
			for (k = 0; k < nc; k++) {
				if (fijo[k])
					resto -= pi[k].ancho;
				else
					suma_fr += pi[k].fr;
			}
			if (suma_fr == 0)
				break;
			if (suma_fr < INTTOFIX(1))
				suma_fr = INTTOFIX(1);
			for (k = 0; k < nc; k++) {
				int v;
				if (fijo[k])
					continue;
				v = resto > 0 ? FIXTOINT(FDIV(FMUL(
						INTTOFIX(resto), pi[k].fr),
						suma_fr)) : 0;
				if (v < pi[k].min) {
					pi[k].ancho = pi[k].min;
					fijo[k] = true;
					otra = true;
				} else {
					pi[k].ancho = v;
				}
			}
			if (!otra)
				break;
		}
		/* sin fr: lo que sobra, a las auto (justify-content: normal) */
		if (suma_fr == 0) {
			int usado = 0, n_auto = 0;
			for (k = 0; k < nc; k++) {
				usado += pi[k].ancho;
				if (pi[k].max == -1)
					n_auto++;
			}
			if (n_auto > 0 && usado < libre) {
				int extra = (libre - usado) / n_auto;
				for (k = 0; k < nc; k++)
					if (pi[k].max == -1)
						pi[k].ancho += extra;
			}
		}
	}

	/* cada elemento, con el ancho de sus columnas */
	for (k = 0; k < n && bien; k++) {
		struct elem *e = &el[k];
		struct box *c = e->b;
		int w = 0, x = 0, j, alto_e;
		if (e->col < 0) {
			c->x = grid->padding[LEFT];
			c->y = grid->padding[TOP];
			continue;
		}
		for (j = 0; j < e->col; j++)
			x += pi[j].ancho + cgap;
		for (j = e->col; j < e->col + e->span && j < nc; j++)
			w += pi[j].ancho + (j > e->col ? cgap : 0);
		if (c->width == AUTO) {
			/* justify-self: stretch */
			c->width = w - lh__delta_outer_width(c);
			if (c->width < 0)
				c->width = 0;
		}
		bien = maqueta(c, c->width, content);
		c->x = grid->padding[LEFT] + x + lh__non_auto_margin(c, LEFT) +
				c->border[LEFT].width;
		alto_e = c->height + lh__delta_outer_height(c);
		if (fila_alto[e->fila] < alto_e)
			fila_alto[e->fila] = alto_e;
	}

	/* las filas */
	alto = 0;
	for (fila = 0; fila < nfilas; fila++) {
		fila_y[fila] = alto;
		alto += fila_alto[fila] + (fila + 1 < nfilas ? rgap : 0);
	}
	for (k = 0; k < n && bien; k++) {
		struct elem *e = &el[k];
		struct box *c = e->b;
		int sobra, dy = 0;
		if (e->col < 0)
			continue;
		sobra = fila_alto[e->fila] - c->height -
				lh__delta_outer_height(c);
		switch (lh__box_align_self(grid, c)) {
		case CSS_ALIGN_SELF_FLEX_END:
			dy = sobra;
			break;
		case CSS_ALIGN_SELF_CENTER:
			dy = sobra / 2;
			break;
		case CSS_ALIGN_SELF_FLEX_START:
		case CSS_ALIGN_SELF_BASELINE:
			break;
		default:
			/* stretch (y normal): si su alto es auto */
			if (sobra > 0 && lh__box_size_cross_is_auto(true, c))
				c->height += sobra;
			break;
		}
		c->y = grid->padding[TOP] + fila_y[e->fila] + dy +
				lh__non_auto_margin(c, TOP) + c->border[TOP].width;
	}

	if (grid->height == AUTO)
		grid->height = alto;
	if (grid->height != AUTO) {
		if (max_height >= 0 && grid->height > max_height)
			grid->height = max_height;
		if (min_height > 0 && grid->height < min_height)
			grid->height = min_height;
	}

	free(el);
	free(fila_alto);
	free(fila_y);
	return bien;
}
