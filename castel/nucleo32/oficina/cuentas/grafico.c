/* grafico.c - Cuentas 2: los gráficos de la hoja (grafico.h): columnas,
 * barras, líneas, sectores y dispersión, con su título, sus ejes con una
 * escala de números redondos, las rayas guía y la leyenda. Cada serie lleva
 * su color, su trama y su marcador (y su raya, en las líneas): se distinguen
 * también en gris.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "grafico.h"
#include "letras.h"
#include "texto.h"
#include "numeros.h"
#include "mate.h"

#define MAX_SERIES 16
#define MAX_CAT 1000

/* colores claros o medios (la trama va encima en tinta oscura: se ve) */
static const u32 COLORES[] = { 0x4F81BD, 0xE8A33A, 0x7FB069, 0xC0504D, 0x8064A2, 0x4BACC6, 0xB7A27A, 0xD87FB0 };
static const int TRAMAS[] = { TR_SOLIDO, TR_DIAG_ARRIBA, TR_HORIZ, TR_CUADRICULA, TR_DIAG_ABAJO, TR_VERT, TR_GRIS25, TR_ENREJADO };
static const int MARCAS[] = { MK_CUADRADO, MK_CIRCULO, MK_TRIANGULO, MK_ROMBO, MK_ASPA, MK_CRUZ, MK_ESTRELLA, MK_TRIANGULO_ABAJO };
#define TINTA 0x1B2430
#define TINTA_TRAMA 0x1E2633

int gr_trama_serie(int i)
{
	return TRAMAS[(i % 8 + 8) % 8];
}

int gr_marcador_serie(int i)
{
	return MARCAS[(i % 8 + 8) % 8];
}

u32 gr_color_serie(int i)
{
	return COLORES[(i % 8 + 8) % 8];
}

const char *gr_nombre_tipo(int t)
{
	static const char *const n[GR_N] = { "Columnas", "Barras", "Líneas", "Sectores", "Dispersión" };

	return t >= 0 && t < GR_N ? n[t] : "?";
}

void gr_nuevo(grafico *g, hoja *h, rango r, int tipo)
{
	memset(g, 0, sizeof *g);
	g->tipo = (u8)(tipo >= 0 && tipo < GR_N ? tipo : GR_COLUMNAS);
	g->hoja = h ? h->id : 0;
	g->datos = r;
	g->en_filas = (r.c1 - r.c0) > (u16)(r.f1 - r.f0);
	g->rotulos = 1;
	g->leyenda = LY_DERECHA;
	g->rejilla = 1;
	g->an = 480;
	g->al = 300;
	/* debajo de los datos, a la izquierda */
	g->f = r.f1 + 2 < CU_FILAS ? r.f1 + 2 : r.f1;
	g->c = r.c0;
}

/* ---- los datos ---- */
typedef struct {
	int ns, nc;		/* series y categorías */
	double v[MAX_SERIES][MAX_CAT];
	u8 hay[MAX_SERIES][MAX_CAT];
	double x[MAX_CAT];	/* (dispersión) las X */
	u8 hay_x[MAX_CAT];
	char nombre[MAX_SERIES][48];
	char cat[MAX_CAT][24];
} datos_t;
static datos_t D;

static const valor *val(hoja *h, const grafico *g, int serie_i, int cat_i)
{
	/* (en_filas: la serie es la fila) */
	u32 f, c;

	if (g->en_filas) {
		f = g->datos.f0 + (u32)serie_i;
		c = g->datos.c0 + (u32)cat_i;
	} else {
		f = g->datos.f0 + (u32)cat_i;
		c = g->datos.c0 + (u32)serie_i;
	}
	return lib_valor(h, f, c);
}

static void texto_de(libro *l, const valor *v, char *d, u32 cabe)
{
	d[0] = 0;
	if (v->t == V_TXT)
		copia(d, v->s ? v->s : "", cabe);
	else if (v->t == V_NUM)
		num_general(v->n, &l->loc, 10, d, cabe);
	else if (v->t == V_LOG)
		copia(d, v->n ? "VERDADERO" : "FALSO", cabe);
}

/* saca las series del rango; en dispersión, la primera serie son las X */
static void saca(libro *l, const grafico *g)
{
	hoja *h = lib_hoja_id(l, g->hoja);
	int alto, ancho, ns, nc, s0 = 0, k0 = 0, i, j;
	int cab_series = 0, cab_cat = 0;

	D.ns = D.nc = 0;
	if (!h)
		return;
	alto = (int)(g->datos.f1 - g->datos.f0 + 1);
	ancho = (int)(g->datos.c1 - g->datos.c0 + 1);
	ns = g->en_filas ? alto : ancho;
	nc = g->en_filas ? ancho : alto;
	if (g->rotulos) {
		/* ¿la primera «fila» de cada serie es su nombre? (si tiene algún texto) */
		for (i = 0; i < ns && !cab_series; i++)
			if (val(h, g, i, 0)->t == V_TXT)
				cab_series = 1;
		/* ¿la primera serie son los rótulos de las categorías? */
		for (j = cab_series; j < nc && !cab_cat; j++)
			if (val(h, g, 0, j)->t == V_TXT)
				cab_cat = 1;
		if (g->tipo == GR_DISPERSION)
			cab_cat = 0;
	}
	s0 = cab_cat ? 1 : 0;
	k0 = cab_series ? 1 : 0;
	if (g->tipo == GR_DISPERSION) {
		/* las X: la primera serie */
		int n = 0;

		for (j = k0; j < nc && n < MAX_CAT; j++, n++) {
			const valor *v = val(h, g, 0, j);

			D.hay_x[n] = v->t == V_NUM;
			D.x[n] = v->n;
		}
		s0 = 1;
	}
	D.nc = nc - k0 > MAX_CAT ? MAX_CAT : nc - k0;
	D.ns = ns - s0 > MAX_SERIES ? MAX_SERIES : ns - s0;
	if (D.ns < 0)
		D.ns = 0;
	if (D.nc < 0)
		D.nc = 0;
	for (i = 0; i < D.ns; i++) {
		if (cab_series)
			texto_de(l, val(h, g, i + s0, 0), D.nombre[i], sizeof D.nombre[i]);
		else
			D.nombre[i][0] = 0;
		if (!D.nombre[i][0])
			fmt(D.nombre[i], sizeof D.nombre[i], "Serie %d", i + 1);
		for (j = 0; j < D.nc; j++) {
			const valor *v = val(h, g, i + s0, j + k0);

			D.hay[i][j] = v->t == V_NUM;
			D.v[i][j] = v->t == V_NUM ? v->n : 0;
		}
	}
	for (j = 0; j < D.nc; j++) {
		if (cab_cat)
			texto_de(l, val(h, g, 0, j + k0), D.cat[j], sizeof D.cat[j]);
		else
			fmt(D.cat[j], sizeof D.cat[j], "%d", j + 1);
	}
}

/* ---- las escalas ---- */
static double paso_redondo(double rango_, int marcas)
{
	double bruto = rango_ / (marcas > 0 ? marcas : 5), p = 1, f;

	if (bruto <= 0)
		return 1;
	while (p * 10 <= bruto)
		p *= 10;
	while (p > bruto)
		p /= 10;
	f = bruto / p;
	return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 5 ? 5 : 10) * p;
}

static void escala(double mn, double mx, int marcas, double *a, double *b, double *paso)
{
	if (mx < mn) {
		double t = mx;

		mx = mn;
		mn = t;
	}
	if (mx == mn) {
		if (mx == 0)
			mx = 1;
		else if (mx > 0)
			mn = 0;
		else
			mx = 0;
	}
	*paso = paso_redondo(mx - mn, marcas);
	*a = so_suelo(mn / *paso) * *paso;
	*b = so_techo(mx / *paso) * *paso;
	if (*b <= *a)
		*b = *a + *paso;
}

/* ---- el texto ---- */
static int CARA, CARA_N;

static void texto(pintor *p, int negrita, double tam, double x, double y, const char *t, u32 c, int alin)
{
	u32 n = largo(t);
	double w = pt_ancho(negrita ? CARA_N : CARA, 0, tam, t, n);

	if (alin == 1)
		x -= w / 2;
	else if (alin == 2)
		x -= w;
	p->texto(p, negrita ? CARA_N : CARA, 0, tam, x, y, t, n, c);
}

static double ancho(int negrita, double tam, const char *t)
{
	return pt_ancho(negrita ? CARA_N : CARA, 0, tam, t, largo(t));
}

static void numero_eje(libro *l, double v, char *d, u32 cabe)
{
	if (so_abs(v) < 1e-12)
		v = 0;
	num_general(v, &l->loc, 9, d, cabe);
}

/* ---- la muestra de una serie (en la leyenda) ---- */
static void muestra(pintor *p, int tipo, int i, double x, double y, double t, double esc)
{
	if (tipo == GR_LINEAS || tipo == GR_DISPERSION) {
		if (tipo == GR_LINEAS)
			pt_raya_trazos(p, x, y + t / 2, x + t * 1.8, y + t / 2, 2 * esc, gr_color_serie(i), i % 4);
		pt_marcador(p, gr_marcador_serie(i), x + t * 0.9, y + t / 2, t * 0.32, gr_color_serie(i));
	} else {
		pt_trama(p, x, y, t * 1.3, t, gr_trama_serie(i), gr_color_serie(i), TINTA_TRAMA);
		p->raya(p, x, y, x + t * 1.3, y, p->pdf ? 0.5 : 1, TINTA);
		p->raya(p, x, y + t, x + t * 1.3, y + t, p->pdf ? 0.5 : 1, TINTA);
		p->raya(p, x, y, x, y + t, p->pdf ? 0.5 : 1, TINTA);
		p->raya(p, x + t * 1.3, y, x + t * 1.3, y + t, p->pdf ? 0.5 : 1, TINTA);
	}
}

static void caja(pintor *p, double x, double y, double an, double al, double g, u32 c)
{
	p->raya(p, x, y, x + an, y, g, c);
	p->raya(p, x, y + al, x + an, y + al, g, c);
	p->raya(p, x, y, x, y + al, g, c);
	p->raya(p, x + an, y, x + an, y + al, g, c);
}

void gr_pinta(pintor *p, libro *l, const grafico *g, double x, double y, double an, double al, double esc)
{
	double em = 9 * 96.0 / 72 * esc, emt = 11.5 * 96.0 / 72 * esc, pad = 8 * esc, g1 = p->pdf ? 0.5 * esc / 0.75 : 1;
	double px0, py0, px1, py1;	/* lo de dentro de los ejes */
	int i, j, sectores = g->tipo == GR_SECTORES, nleyenda;
	char t[64];

	if (an < 20 || al < 20)
		return;
	if (g1 < 0.4)
		g1 = 0.4;
	CARA = letra_cara(FAM_SANS, 0, 0, NULL);
	CARA_N = letra_cara(FAM_SANS, 1, 0, NULL);
	pt_recorte(p, x, y, an, al);
	p->rect(p, x, y, an, al, 0xFFFFFF);
	caja(p, x, y, an - (p->pdf ? 0 : 1), al - (p->pdf ? 0 : 1), g1, 0x8794A6);
	saca(l, g);
	px0 = x + pad;
	py0 = y + pad;
	px1 = x + an - pad;
	py1 = y + al - pad;
	/* el título */
	if (g->titulo[0]) {
		texto(p, 1, emt, x + an / 2, py0 + emt * 0.9, g->titulo, TINTA, 1);
		py0 += emt * 1.4;
	}
	{
		int hay = 0;

		for (i = 0; i < D.ns && !hay; i++)
			for (j = 0; j < D.nc && !hay; j++)
				hay = D.hay[i][j];
		if (!hay || (sectores && D.ns < 1)) {
			texto(p, 0, em, x + an / 2, y + al / 2, "Sin números que dibujar en los datos", 0x5A6472, 1);
			pt_vuelve(p);
			return;
		}
	}
	/* la leyenda */
	nleyenda = sectores ? D.nc : D.ns;
	if (nleyenda > 24)
		nleyenda = 24;
	if (g->leyenda == LY_DERECHA) {
		double w = 0, lh = em * 1.5, ly;

		for (i = 0; i < nleyenda; i++) {
			double k = ancho(0, em, sectores ? D.cat[i] : D.nombre[i]);

			if (k > w)
				w = k;
		}
		w += em * 2.6;
		if (w > (px1 - px0) * 0.4)
			w = (px1 - px0) * 0.4;
		ly = (py0 + py1) / 2 - nleyenda * lh / 2;
		if (ly < py0)
			ly = py0;
		pt_recorte(p, px1 - w, py0, w, py1 - py0);
		for (i = 0; i < nleyenda; i++) {
			double yy = ly + i * lh;

			if (sectores) {
				pt_trama(p, px1 - w, yy, em * 1.3, em, gr_trama_serie(i), gr_color_serie(i), TINTA_TRAMA);
				caja(p, px1 - w, yy, em * 1.3, em, g1, TINTA);
			} else
				muestra(p, g->tipo, i, px1 - w, yy, em, esc);
			texto(p, 0, em, px1 - w + em * 2.1, yy + em * 0.85, sectores ? D.cat[i] : D.nombre[i], TINTA, 0);
		}
		pt_vuelve(p);
		px1 -= w + pad;
	} else if (g->leyenda == LY_ABAJO) {
		double xx = px0, lh = em * 1.5;
		int filas = 1;

		for (i = 0; i < nleyenda; i++) {
			double w = ancho(0, em, sectores ? D.cat[i] : D.nombre[i]) + em * 3.4;

			if (xx + w > px1 && xx > px0) {
				xx = px0;
				filas++;
			}
			xx += w;
		}
		{
			double yy = py1 - filas * lh;

			xx = px0;
			for (i = 0; i < nleyenda; i++) {
				const char *s = sectores ? D.cat[i] : D.nombre[i];
				double w = ancho(0, em, s) + em * 3.4;

				if (xx + w > px1 && xx > px0) {
					xx = px0;
					yy += lh;
				}
				if (sectores) {
					pt_trama(p, xx, yy, em * 1.3, em, gr_trama_serie(i), gr_color_serie(i), TINTA_TRAMA);
					caja(p, xx, yy, em * 1.3, em, g1, TINTA);
				} else
					muestra(p, g->tipo, i, xx, yy, em, esc);
				texto(p, 0, em, xx + em * 2.1, yy + em * 0.85, s, TINTA, 0);
				xx += w;
			}
		}
		py1 -= filas * lh + pad / 2;
	}
	/* ---- los sectores ---- */
	if (sectores) {
		double total = 0, cx = (px0 + px1) / 2, cy = (py0 + py1) / 2, r = (px1 - px0 < py1 - py0 ? px1 - px0 : py1 - py0) / 2 - em, a0 = -CU_PI / 2;

		for (j = 0; j < D.nc; j++)
			if (D.hay[0][j] && D.v[0][j] > 0)
				total += D.v[0][j];
		if (total <= 0 || r < 4) {
			texto(p, 0, em, x + an / 2, y + al / 2, "Sin números positivos que dibujar", 0x5A6472, 1);
			pt_vuelve(p);
			return;
		}
		for (j = 0; j < D.nc; j++) {
			double v = D.hay[0][j] && D.v[0][j] > 0 ? D.v[0][j] : 0, a1, am;
			int trozos, k;

			if (v <= 0)
				continue;
			a1 = a0 + v / total * 2 * CU_PI;
			/* en trozos de hasta 45º (cada uno convexo) */
			trozos = (int)((a1 - a0) / (CU_PI / 4)) + 1;
			for (k = 0; k < trozos; k++) {
				double b0 = a0 + (a1 - a0) * k / trozos, b1 = a0 + (a1 - a0) * (k + 1) / trozos, xy[2 * 12];
				int n = 0, m;

				xy[n++] = cx;
				xy[n++] = cy;
				for (m = 0; m <= 8; m++) {
					double a = b0 + (b1 - b0) * m / 8;

					xy[n++] = cx + r * cu_cos(a);
					xy[n++] = cy + r * cu_sen(a);
				}
				pt_trama_pol(p, xy, n / 2, gr_trama_serie(j), gr_color_serie(j), TINTA_TRAMA);
			}
			/* los bordes de la porción */
			p->raya(p, cx, cy, cx + r * cu_cos(a0), cy + r * cu_sen(a0), g1 * 1.5, 0xFFFFFF);
			p->raya(p, cx, cy, cx + r * cu_cos(a1), cy + r * cu_sen(a1), g1 * 1.5, 0xFFFFFF);
			am = (a0 + a1) / 2;
			{
				double pc = v / total * 100, lx, lyy, k2 = pc >= 8 ? 0.62 : 1.12;
				char s[32];

				fmt(s, sizeof s, "%d %%", (int)(pc + 0.5));
				if (pc < 1)
					fmt(s, sizeof s, "<1 %%");
				lx = cx + r * k2 * cu_cos(am);
				lyy = cy + r * k2 * cu_sen(am) + em * 0.35;
				if (pc >= 8) {
					/* (sobre la trama: con un fondo blanco detrás, que se lea) */
					double w = ancho(1, em, s);

					p->rect(p, lx - w / 2 - 2 * esc, lyy - em * 0.9, w + 4 * esc, em * 1.15, 0xFFFFFF);
				}
				texto(p, 1, em, lx, lyy, s, TINTA, 1);
			}
			a0 = a1;
		}
		/* el borde del círculo */
		{
			double ax = cx + r, ay = cy;
			int m;

			for (m = 1; m <= 64; m++) {
				double a = 2 * CU_PI * m / 64, bx = cx + r * cu_cos(a), by = cy + r * cu_sen(a);

				p->raya(p, ax, ay, bx, by, g1, TINTA);
				ax = bx;
				ay = by;
			}
		}
		pt_vuelve(p);
		return;
	}
	/* ---- con ejes ---- */
	{
		int barras = g->tipo == GR_BARRAS, disp = g->tipo == GR_DISPERSION;
		int apil = g->apiladas && (g->tipo == GR_COLUMNAS || barras);
		double mn = 0, mx = 0, a, b, paso, xa = 0, xb = 1, xpaso = 1;
		int primero = 1;
		double eje_lab_h = em * 1.6, ylab_w = 0, v;
		char s[40];

		/* los límites de los valores */
		for (j = 0; j < D.nc; j++) {
			double pos = 0, neg = 0;

			for (i = 0; i < D.ns; i++) {
				if (!D.hay[i][j])
					continue;
				if (apil) {
					if (D.v[i][j] >= 0)
						pos += D.v[i][j];
					else
						neg += D.v[i][j];
				} else {
					if (primero || D.v[i][j] < mn)
						mn = D.v[i][j];
					if (primero || D.v[i][j] > mx)
						mx = D.v[i][j];
					primero = 0;
				}
			}
			if (apil) {
				if (pos > mx)
					mx = pos;
				if (neg < mn)
					mn = neg;
			}
		}
		if (g->tipo == GR_COLUMNAS || barras) {
			if (mn > 0)
				mn = 0;
			if (mx < 0)
				mx = 0;
		}
		escala(mn, mx, 5, &a, &b, &paso);
		if (disp) {
			int pr = 1;
			double xm = 0, xM = 0;

			for (j = 0; j < D.nc; j++)
				if (D.hay_x[j]) {
					if (pr || D.x[j] < xm)
						xm = D.x[j];
					if (pr || D.x[j] > xM)
						xM = D.x[j];
					pr = 0;
				}
			escala(xm, xM, 5, &xa, &xb, &xpaso);
		}
		/* lo que miden los números del eje de valores */
		for (v = a; v <= b + paso / 2; v += paso) {
			double w;

			numero_eje(l, v, s, sizeof s);
			w = ancho(0, em, s);
			if (w > ylab_w)
				ylab_w = w;
		}
		/* los rótulos de los ejes */
		if (g->eje_y[0]) {
			texto(p, 1, em, px0, py0 + em * 0.9, g->eje_y, TINTA, 0);
			py0 += em * 1.5;
		}
		if (g->eje_x[0]) {
			texto(p, 1, em, (px0 + px1) / 2, py1 - em * 0.25, g->eje_x, TINTA, 1);
			py1 -= em * 1.5;
		}
		py0 += em * 0.5;
		if (barras) {
			/* las categorías a la izquierda; los valores abajo */
			double cw = 0;

			for (j = 0; j < D.nc; j++) {
				double w = ancho(0, em, D.cat[j]);

				if (w > cw)
					cw = w;
			}
			if (cw > (px1 - px0) * 0.3)
				cw = (px1 - px0) * 0.3;
			px0 += cw + em * 0.6;
			py1 -= eje_lab_h;
			px1 -= ylab_w / 2;
		} else {
			px0 += ylab_w + em * 0.6;
			py1 -= eje_lab_h;
			px1 -= disp ? em : 0;
		}
		if (px1 - px0 < 10 || py1 - py0 < 10) {
			pt_vuelve(p);
			return;
		}
		/* las rayas guía y los números del eje de valores */
		for (v = a; v <= b + paso / 2; v += paso) {
			double k = (v - a) / (b - a);

			numero_eje(l, v, s, sizeof s);
			if (barras) {
				double xx = px0 + k * (px1 - px0);

				if (g->rejilla)
					p->raya(p, xx, py0, xx, py1, g1, 0xD5DAE1);
				texto(p, 0, em, xx, py1 + em * 1.2, s, TINTA, 1);
			} else {
				double yy = py1 - k * (py1 - py0);

				if (g->rejilla)
					p->raya(p, px0, yy, px1, yy, g1, 0xD5DAE1);
				texto(p, 0, em, px0 - em * 0.4, yy + em * 0.35, s, TINTA, 2);
			}
		}
		if (disp) {
			for (v = xa; v <= xb + xpaso / 2; v += xpaso) {
				double xx = px0 + (v - xa) / (xb - xa) * (px1 - px0);

				numero_eje(l, v, s, sizeof s);
				if (g->rejilla)
					p->raya(p, xx, py0, xx, py1, g1, 0xE3E7EC);
				texto(p, 0, em, xx, py1 + em * 1.2, s, TINTA, 1);
			}
		}
		/* las series */
		if (g->tipo == GR_COLUMNAS || barras) {
			double cero_k = (0 - a) / (b - a), largo_cat = (barras ? py1 - py0 : px1 - px0) / (D.nc ? D.nc : 1);
			double grupo = largo_cat * 0.72, w = apil ? grupo : grupo / (D.ns ? D.ns : 1);

			for (j = 0; j < D.nc; j++) {
				double base = (barras ? py0 : px0) + j * largo_cat + (largo_cat - grupo) / 2, pos = 0, neg = 0;

				for (i = 0; i < D.ns; i++) {
					double v0, v1, k0, k1, ra, rb;

					if (!D.hay[i][j])
						continue;
					if (apil) {
						if (D.v[i][j] >= 0) {
							v0 = pos;
							pos += D.v[i][j];
							v1 = pos;
						} else {
							v0 = neg;
							neg += D.v[i][j];
							v1 = neg;
						}
					} else {
						v0 = 0;
						v1 = D.v[i][j];
					}
					k0 = (v0 - a) / (b - a);
					k1 = (v1 - a) / (b - a);
					if (k0 > k1) {
						double tt = k0;

						k0 = k1;
						k1 = tt;
					}
					ra = apil ? base : base + i * w;
					rb = ra + w * (apil ? 1 : 0.92);
					if (barras) {
						double xx0 = px0 + k0 * (px1 - px0), xx1 = px0 + k1 * (px1 - px0);

						pt_trama(p, xx0, ra, xx1 - xx0, rb - ra, gr_trama_serie(i), gr_color_serie(i), TINTA_TRAMA);
						caja(p, xx0, ra, xx1 - xx0, rb - ra, g1, TINTA);
					} else {
						double yy0 = py1 - k1 * (py1 - py0), yy1 = py1 - k0 * (py1 - py0);

						pt_trama(p, ra, yy0, rb - ra, yy1 - yy0, gr_trama_serie(i), gr_color_serie(i), TINTA_TRAMA);
						caja(p, ra, yy0, rb - ra, yy1 - yy0, g1, TINTA);
					}
				}
				/* el rótulo de la categoría */
				pt_recorte(p, barras ? x : base - (largo_cat - grupo) / 2, y, barras ? px0 - x - em * 0.3 : largo_cat, al);
				if (barras)
					texto(p, 0, em, px0 - em * 0.4, base + grupo / 2 + em * 0.35, D.cat[j], TINTA, 2);
				else
					texto(p, 0, em, base + grupo / 2, py1 + em * 1.2, D.cat[j], TINTA, 1);
				pt_vuelve(p);
			}
			/* el eje del cero */
			if (barras) {
				double xx = px0 + cero_k * (px1 - px0);

				p->raya(p, xx, py0, xx, py1, g1 * 1.5, TINTA);
				p->raya(p, px0, py1, px1, py1, g1, TINTA);
			} else {
				double yy = py1 - cero_k * (py1 - py0);

				p->raya(p, px0, yy, px1, yy, g1 * 1.5, TINTA);
				p->raya(p, px0, py0, px0, py1, g1, TINTA);
			}
		} else {
			/* líneas y dispersión */
			double largo_cat = (px1 - px0) / (D.nc ? D.nc : 1);

			p->raya(p, px0, py1, px1, py1, g1 * 1.5, TINTA);
			p->raya(p, px0, py0, px0, py1, g1, TINTA);
			if (!disp)
				for (j = 0; j < D.nc; j++) {
					double cxx = px0 + (j + 0.5) * largo_cat;

					pt_recorte(p, px0 + j * largo_cat, y, largo_cat, al);
					texto(p, 0, em, cxx, py1 + em * 1.2, D.cat[j], TINTA, 1);
					pt_vuelve(p);
					p->raya(p, cxx, py1, cxx, py1 + 3 * esc, g1, TINTA);
				}
			pt_recorte(p, px0 - em, py0 - em, px1 - px0 + 2 * em, py1 - py0 + 2 * em);
			for (i = 0; i < D.ns; i++) {
				double ux = 0, uy = 0;
				int hay_ant = 0;

				for (j = 0; j < D.nc; j++) {
					double xx, yy;

					if (!D.hay[i][j] || (disp && !D.hay_x[j])) {
						hay_ant = 0;
						continue;
					}
					xx = disp ? px0 + (D.x[j] - xa) / (xb - xa) * (px1 - px0) : px0 + (j + 0.5) * largo_cat;
					yy = py1 - (D.v[i][j] - a) / (b - a) * (py1 - py0);
					if (!disp && hay_ant)
						pt_raya_trazos(p, ux, uy, xx, yy, 2 * esc, gr_color_serie(i), i % 4);
					ux = xx;
					uy = yy;
					hay_ant = 1;
				}
				for (j = 0; j < D.nc; j++) {
					double xx, yy;

					if (!D.hay[i][j] || (disp && !D.hay_x[j]))
						continue;
					xx = disp ? px0 + (D.x[j] - xa) / (xb - xa) * (px1 - px0) : px0 + (j + 0.5) * largo_cat;
					yy = py1 - (D.v[i][j] - a) / (b - a) * (py1 - py0);
					pt_marcador(p, gr_marcador_serie(i), xx, yy, 3.2 * esc, gr_color_serie(i));
				}
			}
			pt_vuelve(p);
		}
		(void)t;
	}
	pt_vuelve(p);
}
