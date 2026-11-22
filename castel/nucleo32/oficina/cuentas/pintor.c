/* pintor.c - Cuentas 2: el pintor de un lienzo y el de un PDF, y lo que se
 * pinta igual en los dos: las tramas, los marcadores y las rayas de trazos
 * (pintor.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintor.h"
#include "letras.h"
#include "texto.h"
#include "libro.h"

/* ===================================================================== */
/*  El lienzo                                                            */
/* ===================================================================== */
static int redondo(double v)
{
	return v >= 0 ? (int)(v + 0.5) : -(int)(-v + 0.5);
}

static int suelo_i(double v)
{
	int i = (int)v;

	return (v < 0 && (double)i != v) ? i - 1 : i;
}

static void lz_rect_p(pintor *p, double x, double y, double an, double al, u32 c)
{
	int x0 = redondo(x), y0 = redondo(y), x1 = redondo(x + an), y1 = redondo(y + al);

	if (x1 <= x0 && an > 0)
		x1 = x0 + 1;
	if (y1 <= y0 && al > 0)
		y1 = y0 + 1;
	lz_rect(p->u, x0, y0, x1 - x0, y1 - y0, c);
}

static void lz_raya_p(pintor *p, double x0, double y0, double x1, double y1, double g, u32 c)
{
	lienzo *l = p->u;

	if (g <= 0)
		g = 1;
	/* (las rectas de lado, nítidas: sin suavizar) */
	if (y0 == y1) {
		int gi = redondo(g) < 1 ? 1 : redondo(g), a = redondo(x0 < x1 ? x0 : x1), b = redondo(x0 < x1 ? x1 : x0);

		lz_rect(l, a, suelo_i(y0 - (gi - 1) / 2.0), b - a < 1 ? 1 : b - a, gi, c);
		return;
	}
	if (x0 == x1) {
		int gi = redondo(g) < 1 ? 1 : redondo(g), a = redondo(y0 < y1 ? y0 : y1), b = redondo(y0 < y1 ? y1 : y0);

		lz_rect(l, suelo_i(x0 - (gi - 1) / 2.0), a, gi, b - a < 1 ? 1 : b - a, c);
		return;
	}
	lz_linea(l, redondo(x0 * 16), redondo(y0 * 16), redondo(x1 * 16), redondo(y1 * 16), redondo(g * 16), c);
}

/* un polígono grande, por filas (con las puntas de cada tramo suavizadas) */
static void lz_pol_filas(lienzo *l, const double *xy, int n, u32 c)
{
	double y0 = 1e30, y1 = -1e30;
	int i, y, ya, yb;

	for (i = 0; i < n; i++) {
		if (xy[2 * i + 1] < y0)
			y0 = xy[2 * i + 1];
		if (xy[2 * i + 1] > y1)
			y1 = xy[2 * i + 1];
	}
	ya = suelo_i(y0);
	yb = suelo_i(y1) + 1;
	if (ya < l->ry0)
		ya = l->ry0;
	if (yb > l->ry1)
		yb = l->ry1;
	for (y = ya; y < yb; y++) {
		double cx[64], yc = y + 0.5;
		int k = 0, j, a;

		for (i = 0, j = n - 1; i < n; j = i++) {
			double yi = xy[2 * i + 1], yj = xy[2 * j + 1];

			if ((yi > yc) != (yj > yc) && k < 64)
				cx[k++] = xy[2 * i] + (xy[2 * j] - xy[2 * i]) * (yc - yi) / (yj - yi);
		}
		for (i = 1; i < k; i++) {
			double t = cx[i];

			for (a = i; a > 0 && cx[a - 1] > t; a--)
				cx[a] = cx[a - 1];
			cx[a] = t;
		}
		for (i = 0; i + 1 < k; i += 2) {
			double xa = cx[i], xb = cx[i + 1];
			int pa = suelo_i(xa), pb = suelo_i(xb), x;

			if (pa == pb) {
				lz_punto_a(l, pa, y, c, (int)((xb - xa) * 255));
				continue;
			}
			lz_punto_a(l, pa, y, c, (int)((pa + 1 - xa) * 255));
			for (x = pa + 1; x < pb; x++)
				lz_punto(l, x, y, c);
			lz_punto_a(l, pb, y, c, (int)((xb - pb) * 255));
		}
	}
}

static void lz_pol_p(pintor *p, const double *xy, int n, u32 c)
{
	int v[128], i;
	double ax = 1e30, bx = -1e30, ay = 1e30, by = -1e30;

	if (n < 3 || n > 64)
		return;
	for (i = 0; i < n; i++) {
		if (xy[2 * i] < ax) ax = xy[2 * i];
		if (xy[2 * i] > bx) bx = xy[2 * i];
		if (xy[2 * i + 1] < ay) ay = xy[2 * i + 1];
		if (xy[2 * i + 1] > by) by = xy[2 * i + 1];
	}
	if ((bx - ax) * (by - ay) > 900) {
		lz_pol_filas(p->u, xy, n, c);
		return;
	}
	for (i = 0; i < 2 * n; i++)
		v[i] = redondo(xy[i] * 16);
	lz_poligono(p->u, v, n, c);
}

static void lz_texto_p(pintor *p, int cara, int falsas, double tam, double x, double y, const char *t, u32 n, u32 c)
{
	char b[512];

	if (n >= sizeof b)
		n = sizeof b - 1;
	memcpy(b, t, n);
	b[n] = 0;
	lz_texto(p->u, cara, redondo(tam * 4), falsas, redondo(x), redondo(y), b, c);
}

static void lz_recorta_p(pintor *p)
{
	int x0 = redondo(p->rx0), y0 = redondo(p->ry0), x1 = redondo(p->rx1), y1 = redondo(p->ry1);

	lz_recorte(p->u, x0, y0, x1 - x0 < 0 ? 0 : x1 - x0, y1 - y0 < 0 ? 0 : y1 - y0);
}

void pt_de_lienzo(pintor *p, lienzo *l)
{
	memset(p, 0, sizeof *p);
	p->u = l;
	p->rect = lz_rect_p;
	p->raya = lz_raya_p;
	p->poligono = lz_pol_p;
	p->texto = lz_texto_p;
	p->recorta = lz_recorta_p;
	p->rx0 = l->rx0;
	p->ry0 = l->ry0;
	p->rx1 = l->rx1;
	p->ry1 = l->ry1;
}

/* ===================================================================== */
/*  El PDF                                                               */
/* ===================================================================== */
static void pdf_rect_p(pintor *p, double x, double y, double an, double al, u32 c)
{
	pdf_rect(p->u, x, y, an, al, c);
}

static void pdf_raya_p(pintor *p, double x0, double y0, double x1, double y1, double g, u32 c)
{
	pdf_raya(p->u, x0, y0, x1, y1, g, c);
}

static void pdf_pol_p(pintor *p, const double *xy, int n, u32 c)
{
	pdf_poligono(p->u, xy, n, c);
}

static void pdf_texto_p(pintor *p, int cara, int falsas, double tam, double x, double y, const char *t, u32 n, u32 c)
{
	u32 u[256];
	double xs[256], pos = x;
	u32 i = 0, k = 0;

	while (i < n) {
		u32 ch;

		i += (u32)utf8_lee((const u8 *)t + i, n - i, &ch);
		if (k == 256) {
			pdf_texto(p->u, cara, falsas, tam, c, u, k, xs, y);
			k = 0;
		}
		u[k] = ch;
		xs[k] = pos;
		pos += letra_milesimas(cara, ch) * tam / 1000.0;
		k++;
	}
	if (k)
		pdf_texto(p->u, cara, falsas, tam, c, u, k, xs, y);
}

static void pdf_recorta_p(pintor *p)
{
	if (p->abierto)
		pdf_restaura(p->u);
	pdf_guarda(p->u);
	pdf_recorta_rect(p->u, p->rx0, p->ry0, p->rx1 - p->rx0, p->ry1 - p->ry0);
	p->abierto = 1;
}

void pt_de_pdf(pintor *p, pdfsal *s, double an_pag, double al_pag)
{
	memset(p, 0, sizeof *p);
	p->u = s;
	p->pdf = 1;
	p->rect = pdf_rect_p;
	p->raya = pdf_raya_p;
	p->poligono = pdf_pol_p;
	p->texto = pdf_texto_p;
	p->recorta = pdf_recorta_p;
	p->rx1 = an_pag;
	p->ry1 = al_pag;
	p->an_pag = an_pag;
	p->al_pag = al_pag;
}

void pt_acaba(pintor *p)
{
	if (p->pdf && p->abierto) {
		pdf_restaura(p->u);
		p->abierto = 0;
	}
	p->n_pila = 0;
}

/* ===================================================================== */
/*  Lo común                                                             */
/* ===================================================================== */
void pt_recorte(pintor *p, double x, double y, double an, double al)
{
	if (p->n_pila < 16) {
		p->pila_r[p->n_pila][0] = p->rx0;
		p->pila_r[p->n_pila][1] = p->ry0;
		p->pila_r[p->n_pila][2] = p->rx1;
		p->pila_r[p->n_pila][3] = p->ry1;
	}
	p->n_pila++;
	if (x > p->rx0)
		p->rx0 = x;
	if (y > p->ry0)
		p->ry0 = y;
	if (x + an < p->rx1)
		p->rx1 = x + an;
	if (y + al < p->ry1)
		p->ry1 = y + al;
	if (p->rx1 < p->rx0)
		p->rx1 = p->rx0;
	if (p->ry1 < p->ry0)
		p->ry1 = p->ry0;
	p->recorta(p);
}

void pt_vuelve(pintor *p)
{
	if (p->n_pila <= 0)
		return;
	p->n_pila--;
	if (p->n_pila < 16) {
		p->rx0 = p->pila_r[p->n_pila][0];
		p->ry0 = p->pila_r[p->n_pila][1];
		p->rx1 = p->pila_r[p->n_pila][2];
		p->ry1 = p->pila_r[p->n_pila][3];
	}
	p->recorta(p);
}

double pt_ancho(int cara, int falsas, double tam, const char *t, u32 n)
{
	u32 i = 0;
	i64 m = 0;

	while (i < n) {
		u32 c;

		i += (u32)utf8_lee((const u8 *)t + i, n - i, &c);
		m += letra_milesimas(cara, c);
	}
	return m * tam / 1000.0 + ((falsas & FALSA_NEGRITA) ? tam * 0.04 : 0);
}

/* ---- las tramas ---- */
/* ¿lleva tinta de la trama el punto (x, y) (de la pantalla)? */
static int punto_trama(int t, int x, int y)
{
	switch (t) {
	case TR_GRIS75: return !((x & 1) && (y & 1));
	case TR_GRIS50: return ((x + y) & 1) == 0;
	case TR_GRIS25: return !(x & 1) && !(y & 1);
	case TR_GRIS12: return !(x & 3) && !(y & 3);
	case TR_HORIZ: return (y & 3) == 0;
	case TR_VERT: return (x & 3) == 0;
	case TR_DIAG_ABAJO: return ((x - y) & 3) == 0;
	case TR_DIAG_ARRIBA: return ((x + y) & 3) == 0;
	case TR_CUADRICULA: return (x & 3) == 0 || (y & 3) == 0;
	case TR_ENREJADO: return ((x - y) & 3) == 0 || ((x + y) & 3) == 0;
	}
	return 0;
}

/* (PDF) la trama en lo recortado de ahora: rayas o cuadraditos sobre el rectángulo */
static void trama_pdf(pintor *p, double x, double y, double an, double al, int t, u32 c)
{
	pdfsal *s = p->u;
	double d = 3, g = 0.6, k;

	switch (t) {
	case TR_HORIZ:
	case TR_CUADRICULA:
		for (k = suelo_i(y / d) * d; k <= y + al; k += d)
			pdf_raya(s, x, k, x + an, k, g, c);
		if (t == TR_HORIZ)
			break;
		/* fall through */
	case TR_VERT:
		for (k = suelo_i(x / d) * d; k <= x + an; k += d)
			pdf_raya(s, k, y, k, y + al, g, c);
		break;
	case TR_DIAG_ABAJO:
	case TR_DIAG_ARRIBA:
	case TR_ENREJADO: {
		/* rayas a 45º que cruzan el rectángulo */
		double a0 = suelo_i((x - y - al) / d) * d, a1 = x + an - y;

		if (t != TR_DIAG_ARRIBA)
			for (k = a0; k <= a1 + d; k += d)	/* (x - y = k) */
				pdf_raya(s, k + y, y, k + y + al, y + al, g, c);
		if (t != TR_DIAG_ABAJO) {
			a0 = suelo_i((x + y) / d) * d;
			a1 = x + an + y + al;
			for (k = a0; k <= a1 + d; k += d)	/* (x + y = k) */
				pdf_raya(s, k - y, y, k - y - al, y + al, g, c);
		}
		break;
	}
	default: {
		/* los grises: cuadraditos */
		double q = t == TR_GRIS12 ? 4 : 2, lado = t == TR_GRIS12 ? 0.9 : t == TR_GRIS25 ? 0.9 : 1.0, yy, xx;
		int fi = 0;

		if (t == TR_GRIS50) {
			q = 1.4;
			lado = 0.7;
		}
		for (yy = suelo_i(y / q) * q; yy < y + al; yy += q, fi++)
			for (xx = suelo_i(x / q) * q + ((t == TR_GRIS50 && (fi & 1)) ? q / 2 : 0); xx < x + an; xx += q)
				pdf_rect(s, xx, yy, lado, lado, c);
	}
	}
}

void pt_trama(pintor *p, double x, double y, double an, double al, int t, u32 fondo, u32 c)
{
	if (t == TR_NADA || an <= 0 || al <= 0)
		return;
	if (t == TR_GRIS75) {
		/* (la tinta por fondo y el fondo por puntos) */
		p->rect(p, x, y, an, al, c);
		t = TR_GRIS25;
		{
			u32 k = fondo;

			fondo = c;
			c = k;
		}
		if (p->pdf) {
			pt_recorte(p, x, y, an, al);
			trama_pdf(p, x, y, an, al, t, c);
			pt_vuelve(p);
			return;
		}
	} else
		p->rect(p, x, y, an, al, fondo);
	if (t == TR_SOLIDO)
		return;
	if (p->pdf) {
		pt_recorte(p, x, y, an, al);
		trama_pdf(p, x, y, an, al, t, c);
		pt_vuelve(p);
		return;
	}
	{
		lienzo *l = p->u;
		int x0 = redondo(x), y0 = redondo(y), x1 = redondo(x + an), y1 = redondo(y + al), xx, yy;

		if (x0 < l->rx0) x0 = l->rx0;
		if (y0 < l->ry0) y0 = l->ry0;
		if (x1 > l->rx1) x1 = l->rx1;
		if (y1 > l->ry1) y1 = l->ry1;
		for (yy = y0; yy < y1; yy++) {
			u32 *f = l->p + yy * l->paso;

			for (xx = x0; xx < x1; xx++)
				if (punto_trama(t, xx, yy))
					f[xx] = c;
		}
	}
}

/* ¿está (x, y) dentro del polígono? (par-impar) */
static int dentro(const double *xy, int n, double x, double y)
{
	int i, j, c = 0;

	for (i = 0, j = n - 1; i < n; j = i++) {
		double yi = xy[2 * i + 1], yj = xy[2 * j + 1];

		if ((yi > y) != (yj > y) && x < xy[2 * i] + (xy[2 * j] - xy[2 * i]) * (y - yi) / (yj - yi))
			c = !c;
	}
	return c;
}

void pt_trama_pol(pintor *p, const double *xy, int n, int t, u32 fondo, u32 c)
{
	double ax = 1e30, bx = -1e30, ay = 1e30, by = -1e30;
	int i;

	if (t == TR_NADA || n < 3)
		return;
	for (i = 0; i < n; i++) {
		if (xy[2 * i] < ax) ax = xy[2 * i];
		if (xy[2 * i] > bx) bx = xy[2 * i];
		if (xy[2 * i + 1] < ay) ay = xy[2 * i + 1];
		if (xy[2 * i + 1] > by) by = xy[2 * i + 1];
	}
	if (t == TR_GRIS75) {
		p->poligono(p, xy, n, c);
		t = TR_GRIS25;
		{
			u32 k = fondo;

			fondo = c;
			c = k;
		}
	} else
		p->poligono(p, xy, n, fondo);
	if (t == TR_SOLIDO)
		return;
	if (p->pdf) {
		pdf_guarda(p->u);
		pdf_recorta_poligono(p->u, xy, n);
		trama_pdf(p, ax, ay, bx - ax, by - ay, t, c);
		pdf_restaura(p->u);
		return;
	}
	{
		lienzo *l = p->u;
		int x0 = suelo_i(ax), y0 = suelo_i(ay), x1 = suelo_i(bx) + 1, y1 = suelo_i(by) + 1, xx, yy;

		if (x0 < l->rx0) x0 = l->rx0;
		if (y0 < l->ry0) y0 = l->ry0;
		if (x1 > l->rx1) x1 = l->rx1;
		if (y1 > l->ry1) y1 = l->ry1;
		for (yy = y0; yy < y1; yy++)
			for (xx = x0; xx < x1; xx++)
				if (punto_trama(t, xx, yy) && dentro(xy, n, xx + 0.5, yy + 0.5))
					l->p[yy * l->paso + xx] = c;
	}
}

/* ---- los marcadores ---- */
static void forma(pintor *p, int tipo, double cx, double cy, double r, u32 c)
{
	double v[40];
	int n = 0, i;

	switch (tipo) {
	case MK_CUADRADO:
		v[0] = cx - r, v[1] = cy - r, v[2] = cx + r, v[3] = cy - r, v[4] = cx + r, v[5] = cy + r, v[6] = cx - r, v[7] = cy + r;
		n = 4;
		break;
	case MK_CIRCULO: {
		/* (un polígono de 16 lados) */
		static const double co[16] = { 1, .92388, .70711, .38268, 0, -.38268, -.70711, -.92388, -1, -.92388, -.70711, -.38268, 0, .38268, .70711, .92388 };

		for (i = 0; i < 16; i++) {
			v[2 * i] = cx + r * co[i];
			v[2 * i + 1] = cy + r * co[(i + 4) % 16];
		}
		n = 16;
		break;
	}
	case MK_TRIANGULO:
		v[0] = cx, v[1] = cy - r * 1.15, v[2] = cx + r * 1.1, v[3] = cy + r * 0.8, v[4] = cx - r * 1.1, v[5] = cy + r * 0.8;
		n = 3;
		break;
	case MK_TRIANGULO_ABAJO:
		v[0] = cx, v[1] = cy + r * 1.15, v[2] = cx - r * 1.1, v[3] = cy - r * 0.8, v[4] = cx + r * 1.1, v[5] = cy - r * 0.8;
		n = 3;
		break;
	case MK_ROMBO:
		v[0] = cx, v[1] = cy - r * 1.3, v[2] = cx + r * 1.3, v[3] = cy, v[4] = cx, v[5] = cy + r * 1.3, v[6] = cx - r * 1.3, v[7] = cy;
		n = 4;
		break;
	case MK_ESTRELLA: {
		static const double ex[10] = { 0, .224, .951, .363, .588, 0, -.588, -.363, -.951, -.224 };
		static const double ey[10] = { -1, -.309, -.309, .118, .809, .382, .809, .118, -.309, -.309 };

		for (i = 0; i < 10; i++) {
			v[2 * i] = cx + r * 1.3 * ex[i];
			v[2 * i + 1] = cy + r * 1.3 * ey[i];
		}
		n = 10;
		break;
	}
	case MK_ASPA:
	case MK_CRUZ: {
		/* dos barras gruesas */
		double g = r * 0.45, a = r * 1.15;

		if (tipo == MK_CRUZ) {
			double h1[8] = { cx - a, cy - g, cx + a, cy - g, cx + a, cy + g, cx - a, cy + g };
			double h2[8] = { cx - g, cy - a, cx + g, cy - a, cx + g, cy + a, cx - g, cy + a };

			p->poligono(p, h1, 4, c);
			p->poligono(p, h2, 4, c);
		} else {
			double k = 0.7071;
			double h1[8] = { cx - a * k - g * k, cy - a * k + g * k, cx - a * k + g * k, cy - a * k - g * k, cx + a * k + g * k, cy + a * k - g * k, cx + a * k - g * k, cy + a * k + g * k };
			double h2[8] = { cx + a * k - g * k, cy - a * k - g * k, cx + a * k + g * k, cy - a * k + g * k, cx - a * k + g * k, cy + a * k + g * k, cx - a * k - g * k, cy + a * k - g * k };

			p->poligono(p, h1, 4, c);
			p->poligono(p, h2, 4, c);
		}
		return;
	}
	default:
		return;
	}
	p->poligono(p, v, n, c);
}

void pt_marcador(pintor *p, int tipo, double cx, double cy, double r, u32 c)
{
	double b = r * 0.28 < 0.8 ? 0.8 : r * 0.28;

	if (tipo <= MK_NADA || tipo >= MK_N)
		return;
	/* un borde oscuro (la misma forma, un poco mayor) y dentro el color */
	forma(p, tipo, cx, cy, r + b, 0x1B2430);
	forma(p, tipo, cx, cy, r, c);
}

void pt_raya_trazos(pintor *p, double x0, double y0, double x1, double y1, double g, u32 c, int estilo)
{
	double dx = x1 - x0, dy = y1 - y0, d = so_raiz(dx * dx + dy * dy), t, on, off;
	int k = 0;

	if (estilo == 0 || d <= 0) {
		p->raya(p, x0, y0, x1, y1, g, c);
		return;
	}
	if (g < 1 && !p->pdf)
		g = 1;
	/* 1 puntos, 2 guiones, 3 guion y punto */
	on = estilo == 1 ? g : g * 4;
	off = estilo == 1 ? g * 1.6 : g * 2.2;
	if (on < 1 && !p->pdf)
		on = 1;
	for (t = 0; t < d; k++) {
		double l = (estilo == 3 && (k & 1)) ? g : on, e = t + l > d ? d : t + l;

		p->raya(p, x0 + dx * t / d, y0 + dy * t / d, x0 + dx * e / d, y0 + dy * e / d, g, c);
		t = e + off;
	}
}
