/* celdas.c - Cuentas 2: pintar un trozo de hoja en un pintor (celdas.h):
 * el fondo, la cuadrícula, los rellenos con su trama, las combinadas, los
 * textos (con lo que se sale a las vacías de al lado), los bordes, el
 * formato condicional con su símbolo y la marca de los comentarios. Lo
 * mismo para la ventana, la vista preliminar y el PDF.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "celdas.h"
#include "letras.h"
#include "texto.h"
#include "numeros.h"
#include "evalua.h"

#define MAX_FILAS 4096
#define VECINAS 32		/* columnas de los lados que se miran (lo que se sale) */

static double cxs[CU_COLS + 1];		/* la x de cada columna (de ca a cb + 1) */
static double rys[MAX_FILAS + 1];	/* la y de cada fila (desde f0) */
static u32 ca, cb;

int cel_cara(const estilo *e, int *falsas)
{
	int f = 0, c = letra_cara(e->familia[0] ? e->familia : CU_FAMILIA, e->negrita, e->cursiva, &f);

	if (falsas)
		*falsas = f;
	return c;
}

/* ---- el formato condicional ---- */
static int cumple(libro *l, const condicional *k, const valor *v)
{
	(void)l;
	if (k->cond == FC_TEXTO) {
		char p[80];

		if (v->t != V_TXT || !v->s || !k->texto[0])
			return 0;
		fmt(p, sizeof p, "*%s*", k->texto);
		return ev_texto_igual(v->s, largo(v->s), p, largo(p), 1);
	}
	if (v->t != V_NUM)
		return 0;
	switch (k->cond) {
	case FC_MAYOR: return v->n > k->v1;
	case FC_MENOR: return v->n < k->v1;
	case FC_ENTRE: return v->n >= (k->v1 < k->v2 ? k->v1 : k->v2) && v->n <= (k->v1 < k->v2 ? k->v2 : k->v1);
	case FC_IGUAL: return v->n == k->v1;
	case FC_DISTINTO: return v->n != k->v1;
	case FC_MAYOR_IGUAL: return v->n >= k->v1;
	case FC_MENOR_IGUAL: return v->n <= k->v1;
	}
	return 0;
}

const condicional *cel_condicional(libro *l, hoja *h, u32 f, u32 c)
{
	u32 i;
	const valor *v = NULL;

	for (i = 0; i < h->n_cond; i++) {
		const condicional *k = &h->cond[i];

		if (!rg_dentro(k->r, f, c))
			continue;
		if (!v)
			v = lib_valor(h, f, c);
		if (cumple(l, k, v))
			return k;
	}
	return NULL;
}

/* ---- las medidas ---- */
static double x_col(const trozo_hoja *t, u32 c)
{
	if (c >= ca && c <= cb + 1)
		return cxs[c];
	{
		double x;
		u32 k;

		if (c > cb + 1) {
			x = cxs[cb + 1];
			for (k = cb + 1; k < c; k++)
				x += hj_ancho(t->h, k) * t->esc;
		} else {
			x = cxs[ca];
			for (k = ca; k > c; k--)
				x -= hj_ancho(t->h, k - 1) * t->esc;
		}
		return x;
	}
}

static double y_fila(const trozo_hoja *t, u32 f)
{
	if (f >= t->f0 && f <= t->f1 + 1)
		return rys[f - t->f0];
	{
		double y;
		u32 k;

		if (f > t->f1 + 1) {
			y = rys[t->f1 + 1 - t->f0];
			for (k = t->f1 + 1; k < f; k++)
				y += hj_alto(t->h, k) * t->esc;
		} else {
			y = rys[0];
			for (k = t->f0; k > f; k--)
				y -= hj_alto(t->h, k - 1) * t->esc;
		}
		return y;
	}
}

/* el tamaño de la letra del estilo, en unidades del pintor */
static double em_de(const estilo *e, double esc)
{
	return (e->tam ? e->tam : CU_TAM) / 2.0 * 96.0 / 72.0 * esc;
}

static int vacia(const celda *x)
{
	return !x || (x->v.t == V_VACIO && !x->f);
}

/* ---- el texto de una celda ---- */
typedef struct {
	char t[520];
	u32 n;
	const estilo *e;
	const condicional *k;
	u32 color;
	int cara, falsas;
	double em, ancho;
	int alin;		/* AH_IZQ, AH_CENTRO, AH_DER, AH_RELLENAR */
	int es_num;
} txt_celda;

static void prepara(libro *l, hoja *h, u32 f, u32 c, const celda *x, double esc, txt_celda *z)
{
	u32 col = CU_AUTO;
	const char *sim = "";

	z->e = lib_estilo(l, x->estilo);
	z->k = h->n_cond ? cel_condicional(l, h, f, c) : NULL;
	lib_texto_visible(l, x, z->t, sizeof z->t - 8, &col);
	if (z->k && z->k->simbolo && z->k->simbolo < SIM_N && z->k->simbolo != SIM_ESTRELLA) {
		/* el símbolo delante (lo que distingue, no solo el color) */
		char b[530];

		sim = SIMBOLO_LETRA[z->k->simbolo];
		fmt(b, sizeof b, "%s %s", sim, z->t);
		copia(z->t, b, sizeof z->t);
	}
	z->n = largo(z->t);
	{
		estilo e2 = *z->e;

		if (z->k && z->k->negrita)
			e2.negrita = 1;
		if (z->k && z->k->cursiva)
			e2.cursiva = 1;
		z->cara = cel_cara(&e2, &z->falsas);
	}
	z->em = em_de(z->e, esc);
	z->color = z->e->color == CU_AUTO ? 0x000000 : z->e->color;
	if (col != CU_AUTO)
		z->color = col;
	if (z->k && z->k->color != CU_AUTO)
		z->color = z->k->color;
	z->es_num = x->v.t == V_NUM;
	switch (z->e->alin_h) {
	case AH_IZQ: case AH_JUSTIFICAR: z->alin = AH_IZQ; break;
	case AH_CENTRO: case AH_CENTRO_SEL: z->alin = AH_CENTRO; break;
	case AH_DER: z->alin = AH_DER; break;
	case AH_RELLENAR: z->alin = AH_RELLENAR; break;
	default:
		z->alin = x->v.t == V_NUM ? AH_DER : (x->v.t == V_LOG || x->v.t == V_ERR) ? AH_CENTRO : AH_IZQ;
	}
	z->ancho = pt_ancho(z->cara, z->falsas, z->em, z->t, z->n);
	if (z->k && z->k->simbolo == SIM_ESTRELLA)
		z->ancho += z->em;
}

/* una línea de texto con su subrayado y su tachado */
static void linea(pintor *p, const txt_celda *z, double x, double y, const char *s, u32 n, double an)
{
	if (z->k && z->k->simbolo == SIM_ESTRELLA) {
		pt_marcador(p, MK_ESTRELLA, x + z->em * 0.4, y - z->em * 0.33, z->em * 0.28, 0xE8B33A);
		x += z->em;
		an -= z->em;
	}
	p->texto(p, z->cara, z->falsas, z->em, x, y, s, n, z->color);
	if (z->e->subrayado) {
		double g = z->em / 14 < 1 && !p->pdf ? 1 : z->em / 14;

		p->raya(p, x, y + z->em * 0.12, x + an, y + z->em * 0.12, g, z->color);
	}
	if (z->e->tachado) {
		double g = z->em / 14 < 1 && !p->pdf ? 1 : z->em / 14;

		p->raya(p, x, y - z->em * 0.3, x + an, y - z->em * 0.3, g, z->color);
	}
}

/* pinta el texto en la caja (x0..x1, y0..y1) de la celda; puede salirse
 * hasta (sx0..sx1) (lo de los lados que está vacío) */
static void pinta_texto(pintor *p, libro *l, const celda *x, txt_celda *z, double x0, double y0, double x1, double y1, double sx0, double sx1, double esc)
{
	double pad = 3 * esc, padv = 1.5 * esc, w = x1 - x0, lh = z->em * 1.17, top, bx;
	const estilo *e = z->e;
	double sang = e->sangria * 9 * esc;

	if (!z->n && !(z->k && z->k->simbolo == SIM_ESTRELLA))
		return;
	/* un número que no cabe: menos cifras (si es General) o almohadillas */
	if (z->es_num && z->ancho > w - 2 * pad) {
		const char *cod = lib_fmt(l, e->numfmt);

		if (fmt_categoria(cod) == CAT_GENERAL) {
			double cero = pt_ancho(z->cara, z->falsas, z->em, "0", 1);
			int m = cero > 0 ? (int)((w - 2 * pad) / cero) : 1;

			if (m >= 1) {
				num_general(x->v.n, &l->loc, m, z->t, sizeof z->t);
				z->n = largo(z->t);
				z->ancho = pt_ancho(z->cara, z->falsas, z->em, z->t, z->n);
			}
		}
		if (z->ancho > w - 2 * pad) {
			double alm = pt_ancho(z->cara, z->falsas, z->em, "#", 1);
			u32 k = alm > 0 ? (u32)((w - 2 * pad) / alm) : 1, i;

			if (k < 1)
				k = 1;
			if (k > sizeof z->t - 1)
				k = sizeof z->t - 1;
			for (i = 0; i < k; i++)
				z->t[i] = '#';
			z->t[k] = 0;
			z->n = k;
			z->ancho = alm * k;
		}
		sx0 = x0;
		sx1 = x1;
	}
	pt_recorte(p, sx0, y0, sx1 - sx0, y1 - y0);
	if (e->ajustar && !z->es_num) {
		/* en líneas: por palabras */
		const char *s = z->t;
		u32 ini[64], lar[64], nl = 0, i = 0, n = z->n;
		double avail = w - 2 * pad - sang;

		while (i < n && nl < 64) {
			u32 a = i, ult_esp = 0, j = i;

			while (j < n && s[j] != '\n') {
				u32 c, k = (u32)utf8_lee((const u8 *)s + j, n - j, &c);

				if (pt_ancho(z->cara, z->falsas, z->em, s + a, j + k - a) > avail && j > a) {
					if (ult_esp > a)
						j = ult_esp;
					break;
				}
				if (c == ' ')
					ult_esp = j;
				j += k;
			}
			ini[nl] = a;
			lar[nl] = j - a;
			nl++;
			i = j;
			while (i < n && (s[i] == ' ' || s[i] == '\n'))
				i++;
		}
		top = e->alin_v == AV_ARRIBA ? y0 + padv : e->alin_v == AV_CENTRO ? (y0 + y1 - nl * lh) / 2 : y1 - padv - nl * lh;
		if (top < y0 + padv && e->alin_v != AV_ABAJO)
			top = y0 + padv;
		for (i = 0; i < nl; i++) {
			double aw = pt_ancho(z->cara, z->falsas, z->em, s + ini[i], lar[i]);

			bx = z->alin == AH_DER ? x1 - pad - aw : z->alin == AH_CENTRO ? (x0 + x1 - aw) / 2 : x0 + pad + sang;
			linea(p, z, bx, top + i * lh + z->em * 0.93, s + ini[i], lar[i], aw);
		}
		pt_vuelve(p);
		return;
	}
	top = e->alin_v == AV_ARRIBA ? y0 + padv : e->alin_v == AV_CENTRO ? (y0 + y1 - lh) / 2 : y1 - padv - lh;
	{
		double base = top + z->em * 0.93;

		if (z->alin == AH_RELLENAR && z->ancho > 0) {
			/* repetido hasta llenar la celda */
			double xx = x0 + pad;

			while (xx + z->ancho <= x1 - pad + 0.01) {
				linea(p, z, xx, base, z->t, z->n, z->ancho);
				xx += z->ancho;
			}
		} else {
			bx = z->alin == AH_DER ? x1 - pad - z->ancho - (z->alin == AH_DER ? sang : 0) :
			     z->alin == AH_CENTRO ? (x0 + x1 - z->ancho) / 2 : x0 + pad + sang;
			linea(p, z, bx, base, z->t, z->n, z->ancho);
		}
	}
	pt_vuelve(p);
}

/* ---- los bordes ---- */
static double grosor(int b, double esc, int pdf)
{
	double g = b == BR_MEDIO ? 2 : b == BR_GRUESO ? 3 : 1;

	g *= esc;
	if (!pdf && g < 1)
		g = 1;
	return g;
}

static void borde(pintor *p, int b, u32 c, double x0, double y0, double x1, double y1, double esc)
{
	double g = grosor(b, esc, p->pdf);

	if (b == BR_NADA)
		return;
	if (c == CU_AUTO)
		c = 0;
	switch (b) {
	case BR_DOBLE: {
		double d = (p->pdf ? 1.2 : 1.5) * (esc < 1 ? 1 : esc), g1 = p->pdf ? 0.5 * esc : 1;

		if (y0 == y1) {
			p->raya(p, x0, y0 - d, x1, y1 - d, g1, c);
			p->raya(p, x0, y0 + d, x1, y1 + d, g1, c);
		} else {
			p->raya(p, x0 - d, y0, x1 - d, y1, g1, c);
			p->raya(p, x0 + d, y0, x1 + d, y1, g1, c);
		}
		break;
	}
	case BR_PUNTOS:
		pt_raya_trazos(p, x0, y0, x1, y1, g, c, 1);
		break;
	case BR_GUIONES:
		pt_raya_trazos(p, x0, y0, x1, y1, g, c, 2);
		break;
	default:
		if (p->pdf && b == BR_FINO)
			g = 0.6 * esc;
		p->raya(p, x0, y0, x1, y1, g, c);
	}
}

/* ---- las combinadas que tocan el trozo ---- */
static u32 comb[256], n_comb;

static int en_comb(const hoja *h, u32 f, u32 c)
{
	u32 i;

	for (i = 0; i < n_comb; i++)
		if (rg_dentro(h->comb[comb[i]], f, c))
			return (int)comb[i];
	return -1;
}

void cel_pinta(pintor *p, const trozo_hoja *t)
{
	hoja *h = t->h;
	libro *l = t->l;
	u32 f, c, f1 = t->f1, i;
	double esc = t->esc;
	double X0, X1, Y0, Y1;
	u32 grid = t->color_rejilla ? t->color_rejilla : 0xD4D9E0;

	if (t->f0 > t->f1 || t->c0 > t->c1 || t->c1 >= CU_COLS)
		return;
	if (f1 - t->f0 >= MAX_FILAS)
		f1 = t->f0 + MAX_FILAS - 1;
	/* las medidas */
	ca = t->c0 > VECINAS ? t->c0 - VECINAS : 0;
	cb = t->c1 + VECINAS < CU_COLS ? t->c1 + VECINAS : CU_COLS - 1;
	cxs[t->c0] = t->x;
	for (c = t->c0; c <= cb; c++)
		cxs[c + 1] = cxs[c] + hj_ancho(h, c) * esc;
	for (c = t->c0; c > ca; c--)
		cxs[c - 1] = cxs[c] - hj_ancho(h, c - 1) * esc;
	rys[0] = t->y;
	for (f = t->f0; f <= f1; f++)
		rys[f - t->f0 + 1] = rys[f - t->f0] + hj_alto(h, f) * esc;
	X0 = cxs[t->c0];
	X1 = cxs[t->c1 + 1];
	Y0 = rys[0];
	Y1 = rys[f1 - t->f0 + 1];
	pt_recorte(p, X0, Y0, X1 - X0, Y1 - Y0);
	/* las combinadas que tocan */
	n_comb = 0;
	{
		rango zona = cu_rango(t->f0, t->c0, f1, t->c1);

		for (i = 0; i < h->n_comb && n_comb < 256; i++)
			if (rg_pisa(h->comb[i], zona))
				comb[n_comb++] = i;
	}
	/* 1. el fondo y la cuadrícula */
	if (!t->sin_fondo)
		p->rect(p, X0, Y0, X1 - X0, Y1 - Y0, 0xFFFFFF);
	if (t->rejilla) {
		double g = p->pdf ? 0.3 : 1;

		for (c = t->c0; c <= t->c1; c++)
			if (hj_ancho(h, c))
				p->raya(p, cxs[c + 1] - (p->pdf ? 0 : 1), Y0, cxs[c + 1] - (p->pdf ? 0 : 1), Y1, g, grid);
		for (f = t->f0; f <= f1; f++)
			if (hj_alto(h, f))
				p->raya(p, X0, rys[f - t->f0 + 1] - (p->pdf ? 0 : 1), X1, rys[f - t->f0 + 1] - (p->pdf ? 0 : 1), g, grid);
		if (p->pdf) {
			p->raya(p, X0, Y0, X1, Y0, g, grid);
			p->raya(p, X0, Y0, X0, Y1, g, grid);
		}
	}
	/* 2. los rellenos */
	for (f = t->f0; f <= f1; f++) {
		double y0 = rys[f - t->f0], y1 = rys[f - t->f0 + 1];

		if (y1 <= y0)
			continue;
		for (c = t->c0; c <= t->c1; c++) {
			double x0 = cxs[c], x1 = cxs[c + 1];
			const estilo *e;
			const condicional *k = NULL;
			u16 ei;

			if (x1 <= x0)
				continue;
			if (n_comb && en_comb(h, f, c) >= 0)
				continue;
			ei = hj_estilo(h, f, c);
			e = lib_estilo(l, ei);
			if (h->n_cond)
				k = cel_condicional(l, h, f, c);
			if (k && k->fondo != CU_AUTO)
				pt_trama(p, x0, y0, x1 - x0, y1 - y0, k->trama ? k->trama : TR_SOLIDO, k->fondo, 0x1B2430);
			else if (e->trama != TR_NADA)
				pt_trama(p, x0, y0, x1 - x0, y1 - y0, e->trama, e->fondo, e->color_trama);
		}
	}
	/* 3. las combinadas: su relleno (o blanco, sin cuadrícula dentro) y su texto */
	for (i = 0; i < n_comb; i++) {
		rango r = h->comb[comb[i]];
		double x0 = x_col(t, r.c0), x1 = x_col(t, r.c1 + 1u), y0 = y_fila(t, r.f0), y1 = y_fila(t, r.f1 + 1);
		const celda *x = hj_celda(h, r.f0, r.c0);
		const estilo *e = lib_estilo(l, hj_estilo(h, r.f0, r.c0));
		const condicional *k = h->n_cond ? cel_condicional(l, h, r.f0, r.c0) : NULL;
		double q = (t->rejilla && !p->pdf) ? 1 : 0;

		if (k && k->fondo != CU_AUTO)
			pt_trama(p, x0, y0, x1 - x0, y1 - y0, k->trama ? k->trama : TR_SOLIDO, k->fondo, 0x1B2430);
		else if (e->trama != TR_NADA)
			pt_trama(p, x0, y0, x1 - x0, y1 - y0, e->trama, e->fondo, e->color_trama);
		else if (!t->sin_fondo)
			p->rect(p, x0, y0, x1 - x0 - q, y1 - y0 - q, 0xFFFFFF);
		if (!vacia(x) && x->v.t != V_VACIO) {
			txt_celda z;

			prepara(l, h, r.f0, r.c0, x, esc, &z);
			pinta_texto(p, l, x, &z, x0, y0, x1, y1, x0, x1, esc);
		}
	}
	/* 4. los textos (también los que vienen de las columnas de los lados) */
	for (f = t->f0; f <= f1; f++) {
		double y0 = rys[f - t->f0], y1 = rys[f - t->f0 + 1];
		u32 desde = ca, hasta = cb;

		if (y1 <= y0)
			continue;
		/* (de los lados, solo la primera con algo puede salirse hasta aquí) */
		desde = t->c0;
		for (c = t->c0; c > ca; c--)
			if (!vacia(hj_celda(h, f, c - 1))) {
				desde = c - 1;
				break;
			}
		hasta = t->c1;
		for (c = t->c1 + 1; c <= cb; c++)
			if (!vacia(hj_celda(h, f, c))) {
				hasta = c;
				break;
			}
		for (c = desde; c <= hasta; c++) {
			const celda *x = hj_celda(h, f, c);
			double x0 = cxs[c], x1 = cxs[c + 1], sx0, sx1;
			txt_celda z;
			u32 k;

			if (vacia(x) || x->v.t == V_VACIO || x1 <= x0)
				continue;
			if (n_comb && en_comb(h, f, c) >= 0)
				continue;
			if (h->n_comb && hj_combinada(h, f, c) >= 0)
				continue;
			prepara(l, h, f, c, x, esc, &z);
			sx0 = x0;
			sx1 = x1;
			/* lo que no cabe se sale a las vacías (los textos, sin partir en líneas) */
			if (!z.es_num && !z.e->ajustar && z.alin != AH_RELLENAR && z.ancho > x1 - x0 - 6 * esc) {
				if (z.alin == AH_IZQ) {
					for (k = c + 1; k <= cb && sx1 - x0 < z.ancho + 6 * esc; k++) {
						if (!vacia(hj_celda(h, f, k)) || (h->n_comb && hj_combinada(h, f, k) >= 0))
							break;
						sx1 = cxs[k + 1];
					}
				} else if (z.alin == AH_DER) {
					for (k = c; k > ca && x1 - sx0 < z.ancho + 6 * esc; k--) {
						if (!vacia(hj_celda(h, f, k - 1)) || (h->n_comb && hj_combinada(h, f, k - 1) >= 0))
							break;
						sx0 = cxs[k - 1];
					}
				} else {
					u32 a = c, b = c;
					double cen = (x0 + x1) / 2;

					while (2 * (cen - sx0 < sx1 - cen ? cen - sx0 : sx1 - cen) < z.ancho + 6 * esc) {
						int crece = 0;

						if (a > ca && vacia(hj_celda(h, f, a - 1)) && !(h->n_comb && hj_combinada(h, f, a - 1) >= 0)) {
							a--;
							sx0 = cxs[a];
							crece = 1;
						}
						if (b < cb && vacia(hj_celda(h, f, b + 1)) && !(h->n_comb && hj_combinada(h, f, b + 1) >= 0)) {
							b++;
							sx1 = cxs[b + 1];
							crece = 1;
						}
						if (!crece)
							break;
					}
				}
			}
			if (sx1 <= X0 || sx0 >= X1)
				continue;
			pinta_texto(p, l, x, &z, x0, y0, x1, y1, sx0, sx1, esc);
		}
	}
	/* 5. los bordes: cada raya, una vez (la de la celda o la de su vecina) */
	for (f = t->f0; f <= f1; f++) {
		double y0 = rys[f - t->f0], y1 = rys[f - t->f0 + 1];

		if (y1 <= y0)
			continue;
		for (c = t->c0; c <= t->c1; c++) {
			double x0 = cxs[c], x1 = cxs[c + 1];
			const estilo *e, *d, *ab;
			int m = n_comb ? en_comb(h, f, c) : -1;
			rango r = cu_rango(f, c, f, c);

			if (x1 <= x0)
				continue;
			if (m >= 0) {
				r = h->comb[m];
				e = lib_estilo(l, hj_estilo(h, r.f0, r.c0));
			} else
				e = lib_estilo(l, hj_estilo(h, f, c));
			/* la derecha: si es el borde de la combinada (o no lo es) */
			if (c == r.c1) {
				int b = e->borde[BO_DER];
				u32 col = e->color_borde[BO_DER];

				if (!b && c + 1 < CU_COLS) {
					d = lib_estilo(l, hj_estilo(h, f, c + 1));
					b = d->borde[BO_IZQ];
					col = d->color_borde[BO_IZQ];
				}
				if (b)
					borde(p, b, col, x1 - (p->pdf ? 0 : 1), y0, x1 - (p->pdf ? 0 : 1), y1, esc);
			}
			if (f == r.f1) {
				int b = e->borde[BO_ABAJO];
				u32 col = e->color_borde[BO_ABAJO];

				if (!b && f + 1 < CU_FILAS) {
					ab = lib_estilo(l, hj_estilo(h, f + 1, c));
					b = ab->borde[BO_ARRIBA];
					col = ab->color_borde[BO_ARRIBA];
				}
				if (b)
					borde(p, b, col, x0, y1 - (p->pdf ? 0 : 1), x1, y1 - (p->pdf ? 0 : 1), esc);
			}
			/* la izquierda y la de arriba: solo en la primera del trozo (las demás las pone la vecina) */
			if (c == t->c0 && c == r.c0) {
				int b = e->borde[BO_IZQ];
				u32 col = e->color_borde[BO_IZQ];

				if (!b && c > 0) {
					d = lib_estilo(l, hj_estilo(h, f, c - 1));
					b = d->borde[BO_DER];
					col = d->color_borde[BO_DER];
				}
				if (b)
					borde(p, b, col, x0, y0, x0, y1, esc);
			}
			if (f == t->f0 && f == r.f0) {
				int b = e->borde[BO_ARRIBA];
				u32 col = e->color_borde[BO_ARRIBA];

				if (!b && f > 0) {
					ab = lib_estilo(l, hj_estilo(h, f - 1, c));
					b = ab->borde[BO_ABAJO];
					col = ab->color_borde[BO_ABAJO];
				}
				if (b)
					borde(p, b, col, x0, y0, x1, y0, esc);
			}
		}
	}
	/* 6. la marca de los comentarios: un triángulo en la esquina */
	if (t->comentarios)
		for (i = 0; i < h->n_com; i++) {
			const comentario *k = &h->com[i];
			double x1, y0, s = 6 * esc;

			if (k->f < t->f0 || k->f > f1 || k->c < t->c0 || k->c > t->c1)
				continue;
			{
				int m = n_comb ? en_comb(h, k->f, k->c) : -1;

				x1 = m >= 0 ? x_col(t, h->comb[m].c1 + 1u) : cxs[k->c + 1];
			}
			y0 = rys[k->f - t->f0];
			if (s < 5)
				s = 5;
			{
				double v[6] = { x1 - s - 1, y0, x1 - 1, y0, x1 - 1, y0 + s + 1 };
				double w[6] = { x1 - s + 1, y0 + 1, x1 - 2, y0 + 1, x1 - 2, y0 + s - 1 };

				p->poligono(p, v, 3, 0x7A1010);
				p->poligono(p, w, 3, 0xE53935);
			}
		}
	pt_vuelve(p);
}

u32 cel_ancho_texto(libro *l, hoja *h, const celda *x)
{
	txt_celda z;

	if (vacia(x) || x->v.t == V_VACIO)
		return 0;
	prepara(l, h, CU_FILA(x->clave), CU_COL(x->clave), x, 1.0, &z);
	if (z.e->ajustar)
		return 0;
	return (u32)(z.ancho + 6.5);
}
