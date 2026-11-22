/* vista.c - Pintura 2: lo que se ve del lienzo: la lámina a su zoom (de
 * 12,5 % a 3200 %; al alejar, cada punto de la pantalla toma el de la
 * lámina que le cae; al acercar, los puntos tal cual), lo transparente
 * sobre cuadros grises, la rejilla con zoom grande, lo elegido con su raya
 * de hormigas (negra y blanca: se ve sobre cualquier color), la caja de
 * texto, las asas para cambiar el tamaño del lienzo, el contorno del
 * pincel y las reglas en puntos de la imagen.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pintura.h"
#include "letras.h"
#include "operaciones.h"

vista_t VI;
int cursor_x, cursor_y, cursor_r = -1;
int asa_an, asa_al;

#define MARGEN 14
#define FONDO_VISTA 0xB9C3CF
#define REGLA 18

static const int zooms[] = { 125, 250, 333, 500, 667, 1000, 1500, 2000, 3000, 4000, 6000, 8000, 12000, 16000, 24000, 32000 };
#define NZ ((int)(sizeof zooms / sizeof zooms[0]))

static int divs(int a, int b)		/* a / b hacia abajo (b > 0) */
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

static i64 divs64(i64 a, i64 b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

void vista_coloca(int x, int y, int an, int al)
{
	VI.x = x;
	VI.y = y;
	VI.an = an > 1 ? an : 1;
	VI.al = al > 1 ? al : 1;
	VI.san = (int)(((i64)P.img.an * P.zoom + 500) / 1000);
	VI.sal = (int)(((i64)P.img.al * P.zoom + 500) / 1000);
	if (VI.san < 1)
		VI.san = 1;
	if (VI.sal < 1)
		VI.sal = 1;
	vista_limita();
	if (VI.san + 2 * MARGEN <= VI.an)
		VI.ox = x + (VI.an - VI.san) / 2;
	else
		VI.ox = x + MARGEN - P.dx;
	if (VI.sal + 2 * MARGEN <= VI.al)
		VI.oy = y + (VI.al - VI.sal) / 2;
	else
		VI.oy = y + MARGEN - P.dy;
}

int vista_total_x(void)
{
	return VI.san + 2 * MARGEN;
}

int vista_total_y(void)
{
	return VI.sal + 2 * MARGEN;
}

void vista_limita(void)
{
	int mx = VI.san + 2 * MARGEN - VI.an, my = VI.sal + 2 * MARGEN - VI.al;

	if (P.dx > mx)
		P.dx = mx;
	if (P.dy > my)
		P.dy = my;
	if (P.dx < 0)
		P.dx = 0;
	if (P.dy < 0)
		P.dy = 0;
}

void vista_a_lamina(int sx, int sy, int *ix, int *iy)
{
	*ix = (int)divs64((i64)(sx - VI.ox) * 1000, P.zoom);
	*iy = (int)divs64((i64)(sy - VI.oy) * 1000, P.zoom);
}

void vista_a_lamina_f(int sx, int sy, float *fx, float *fy)
{
	*fx = (float)((sx - VI.ox + 0.5) * 1000.0 / P.zoom);
	*fy = (float)((sy - VI.oy + 0.5) * 1000.0 / P.zoom);
	/* (con zoom pequeño, al centro del punto de la lámina) */
	if (P.zoom <= 1000) {
		int ix, iy;

		vista_a_lamina(sx, sy, &ix, &iy);
		*fx = (float)ix + 0.5f;
		*fy = (float)iy + 0.5f;
	}
}

int vista_de_lamina_x(int ix)
{
	return VI.ox + (int)divs64((i64)ix * P.zoom, 1000);
}

int vista_de_lamina_y(int iy)
{
	return VI.oy + (int)divs64((i64)iy * P.zoom, 1000);
}

void vista_zona(const int zona[4], int *x0, int *y0, int *x1, int *y1)
{
	*x0 = vista_de_lamina_x(zona[0]) - 1;
	*y0 = vista_de_lamina_y(zona[1]) - 1;
	*x1 = vista_de_lamina_x(zona[2]) + 2;
	*y1 = vista_de_lamina_y(zona[3]) + 2;
}

int vista_zoom_siguiente(int z, int mas)
{
	int i;

	if (mas) {
		for (i = 0; i < NZ; i++)
			if (zooms[i] > z)
				return zooms[i];
		return zooms[NZ - 1];
	}
	for (i = NZ - 1; i >= 0; i--)
		if (zooms[i] < z)
			return zooms[i];
	return zooms[0];
}

int vista_zoom_ajustado(void)
{
	i64 a = (i64)(VI.an - 2 * MARGEN) * 1000 / (P.img.an ? P.img.an : 1);
	i64 b = (i64)(VI.al - 2 * MARGEN) * 1000 / (P.img.al ? P.img.al : 1);
	i64 z = a < b ? a : b;

	if (z > 1000)
		z = 1000;
	if (z < zooms[0])
		z = zooms[0];
	return (int)z;
}

void vista_zoom_en(int zoom, int sx, int sy)
{
	float fx, fy;

	if (zoom < zooms[0])
		zoom = zooms[0];
	if (zoom > zooms[NZ - 1])
		zoom = zooms[NZ - 1];
	vista_a_lamina_f(sx, sy, &fx, &fy);
	fx = (float)((sx - VI.ox + 0.5) * 1000.0 / P.zoom);
	fy = (float)((sy - VI.oy + 0.5) * 1000.0 / P.zoom);
	P.zoom = zoom;
	VI.san = (int)(((i64)P.img.an * zoom + 500) / 1000);
	VI.sal = (int)(((i64)P.img.al * zoom + 500) / 1000);
	/* que (fx, fy) siga debajo de (sx, sy): ox = sx - fx*zoom; ox = VI.x + MARGEN - dx */
	P.dx = VI.x + MARGEN - (sx - (int)(fx * zoom / 1000));
	P.dy = VI.y + MARGEN - (sy - (int)(fy * zoom / 1000));
	vista_coloca(VI.x, VI.y, VI.an, VI.al);
}

/* ---- las rayas de hormigas: negra y blanca a trozos de 4 ---- */
static void punto(lienzo *L, int x, int y, u32 c)
{
	if (x >= L->rx0 && x < L->rx1 && y >= L->ry0 && y < L->ry1)
		L->p[(size_t)y * L->paso + x] = c;
}

static void hormigas(lienzo *L, int x0, int y0, int x1, int y1)
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
	int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, k = 0;

	for (;;) {
		int e2;

		punto(L, x0, y0, (k++ >> 2) & 1 ? 0xFFFFFF : 0x000000);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * e;
		if (e2 >= dy) {
			e += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			e += dx;
			y0 += sy;
		}
	}
}

static void caja_hormigas(lienzo *L, int x0, int y0, int x1, int y1)
{
	hormigas(L, x0, y0, x1, y0);
	hormigas(L, x1, y0, x1, y1);
	hormigas(L, x1, y1, x0, y1);
	hormigas(L, x0, y1, x0, y0);
}

static void asa(lienzo *L, int x, int y)
{
	lz_rect(L, x, y, 7, 7, 0xFFFFFF);
	lz_marco(L, x, y, 7, 7, 0x1B2633);
}

int vista_asa_en(int sx, int sy)
{
	int ax = VI.ox + VI.san + 1, ay = VI.oy + VI.sal + 1;

	if (sx >= ax - 1 && sx <= ax + 8 && sy >= ay - 1 && sy <= ay + 8)
		return 3;
	if (sx >= ax - 1 && sx <= ax + 8 && sy >= VI.oy + VI.sal / 2 - 4 && sy <= VI.oy + VI.sal / 2 + 5)
		return 1;
	if (sy >= ay - 1 && sy <= ay + 8 && sx >= VI.ox + VI.san / 2 - 4 && sx <= VI.ox + VI.san / 2 + 5)
		return 2;
	return 0;
}

void vista_cursor_zona(int *x0, int *y0, int *x1, int *y1)
{
	*x0 = cursor_x - cursor_r - 2;
	*y0 = cursor_y - cursor_r - 2;
	*x1 = cursor_x + cursor_r + 3;
	*y1 = cursor_y + cursor_r + 3;
}

static int *columnas;
static int n_columnas;

void vista_pinta(lienzo *L, int x0, int y0, int x1, int y1)
{
	int c, r, prev_iy = -0x7FFFFFFF;
	const lamina *im = &P.img;
	const seleccion_t *s = &P.sel;
	const caja_texto *t = &P.tx;
	int rejilla = P.ver_rejilla && P.zoom >= 4000;
	u32 sin = P.c2 & 0xFFFFFF;

	if (x0 < VI.x)
		x0 = VI.x;
	if (y0 < VI.y)
		y0 = VI.y;
	if (x1 > VI.x + VI.an)
		x1 = VI.x + VI.an;
	if (y1 > VI.y + VI.al)
		y1 = VI.y + VI.al;
	if (x1 > L->an)
		x1 = L->an;
	if (y1 > L->al)
		y1 = L->al;
	if (x1 <= x0 || y1 <= y0)
		return;
	if (n_columnas < L->an + 1) {
		so_suelta(columnas);
		columnas = so_pide((size_t)(L->an + 1) * sizeof *columnas);
		n_columnas = columnas ? L->an + 1 : 0;
		if (!columnas)
			return;
	}
	for (c = x0 - 1; c < x1; c++) {
		int ix = (int)divs64((i64)(c - VI.ox) * 1000, P.zoom);

		columnas[c - x0 + 1] = ix >= 0 && ix < im->an ? ix : -1;
	}
	if (y0 > VI.y)
		prev_iy = (int)divs64((i64)(y0 - 1 - VI.oy) * 1000, P.zoom);
	for (r = y0; r < y1; r++) {
		int iy = (int)divs64((i64)(r - VI.oy) * 1000, P.zoom), raya_y = rejilla && iy != prev_iy;
		u32 *d = L->p + (size_t)r * L->paso;
		const u32 *fila;
		int en_flota, en_texto;

		prev_iy = iy;
		if (iy < 0 || iy >= im->al) {
			for (c = x0; c < x1; c++)
				d[c] = FONDO_VISTA;
			continue;
		}
		fila = im->p + (size_t)iy * im->an;
		en_flota = s->flota && iy >= s->y && iy < s->y + s->flotante.al;
		en_texto = t->activa && t->dibujo.p && iy >= t->y && iy < t->y + t->dibujo.al;
		for (c = x0; c < x1; c++) {
			int ix = columnas[c - x0 + 1];
			u32 p;

			if (ix < 0) {
				d[c] = FONDO_VISTA;
				continue;
			}
			p = fila[ix];
			if (en_flota && ix >= s->x && ix < s->x + s->flotante.an) {
				u32 q = s->flotante.p[(size_t)(iy - s->y) * s->flotante.an + (ix - s->x)];

				if (!(P.sel_transparente && (q & 0xFFFFFF) == sin && ALFA(q) == 255))
					p = lam_sobre(p, q, 255);
			}
			if (en_texto && ix >= t->x && ix < t->x + t->dibujo.an)
				p = lam_sobre(p, t->dibujo.p[(size_t)(iy - t->y) * t->dibujo.an + (ix - t->x)], 255);
			if (ALFA(p) != 255) {
				u32 cuadro = ((((c - VI.ox) >> 3) ^ ((r - VI.oy) >> 3)) & 1) ? 0xCDCDCD : 0xFFFFFF;

				p = lam_sobre_opaco(cuadro, p);
			} else
				p &= 0xFFFFFF;
			if (rejilla && (raya_y || ix != columnas[c - x0]))
				p = lz_mezcla_color(p, 0x7A8494, 110);
			d[c] = p;
		}
	}
	/* lo de encima, recortado a la zona */
	lz_recorte(L, x0, y0, x1 - x0, y1 - y0);
	/* el marco de la imagen y sus asas */
	lz_marco(L, VI.ox - 1, VI.oy - 1, VI.san + 2, VI.sal + 2, 0x6E7A8A);
	if (!s->flota) {
		asa(L, VI.ox + VI.san + 1, VI.oy + VI.sal / 2 - 3);
		asa(L, VI.ox + VI.san / 2 - 3, VI.oy + VI.sal + 1);
		asa(L, VI.ox + VI.san + 1, VI.oy + VI.sal + 1);
	}
	if (asa_an > 0 && asa_al > 0) {
		int ex = vista_de_lamina_x(asa_an), ey = vista_de_lamina_y(asa_al);

		caja_hormigas(L, VI.ox - 1, VI.oy - 1, ex, ey);
	}
	/* lo elegido */
	if (s->activa) {
		if (s->libre && !s->flota && s->n_libre > 1) {
			int i;

			for (i = 0; i < s->n_libre; i++) {
				int j = (i + 1) % s->n_libre;

				hormigas(L, vista_de_lamina_x(s->libre[2 * i]), vista_de_lamina_y(s->libre[2 * i + 1]),
					 vista_de_lamina_x(s->libre[2 * j]), vista_de_lamina_y(s->libre[2 * j + 1]));
			}
		} else {
			int an = s->flota ? s->flotante.an : s->an, al = s->flota ? s->flotante.al : s->al;

			caja_hormigas(L, vista_de_lamina_x(s->x) - 1, vista_de_lamina_y(s->y) - 1, vista_de_lamina_x(s->x + an), vista_de_lamina_y(s->y + al));
		}
	}
	/* la caja de texto */
	if (t->activa) {
		int bx0 = vista_de_lamina_x(t->x) - 2, by0 = vista_de_lamina_y(t->y) - 2;
		int bx1 = vista_de_lamina_x(t->x + (t->an > 4 ? t->an : 4)) + 2, by1 = vista_de_lamina_y(t->y + t->al) + 2;

		caja_hormigas(L, bx0, by0, bx1, by1);
		herr_texto_pinta_cursor(L);
	}
	/* el contorno del pincel (negro y blanco) */
	if (cursor_r > 0) {
		int i, n = cursor_r * 6 + 12;

		for (i = 0; i < n; i++) {
			double a = 6.283185307 * i / n;
			int px = cursor_x + (int)(cursor_r * op_coseno(a) + (op_coseno(a) >= 0 ? 0.5 : -0.5));
			int py = cursor_y + (int)(cursor_r * op_coseno(a - 1.5707963) + (op_coseno(a - 1.5707963) >= 0 ? 0.5 : -0.5));

			punto(L, px, py, 0x000000);
			punto(L, px + (px > cursor_x ? -1 : 1), py, 0xFFFFFF);
		}
	}
	lz_sin_recorte(L);
}

/* ---- las reglas ---- */
int regla_alto(void)
{
	return REGLA;
}

int regla_ancho(void)
{
	return REGLA;
}

static void cifra(lienzo *L, int x, int y, const char *t)
{
	int cara = letra_cara(FAM_SANS, 0, 0, NULL);

	lz_texto(L, cara, 9 * 4, 0, x, y, t, 0x3A4656);
}

void regla_pinta(lienzo *L, int arriba, int x, int y, int an, int al, int raton)
{
	static const int pasos[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000 };
	int paso = 2000, i, menor, v0, v1, v;
	int largo_img = arriba ? P.img.an : P.img.al;

	lz_rect(L, x, y, an, al, UI_FONDO2);
	if (arriba)
		lz_raya_h(L, x, y + al - 1, an, UI_RAYA_FUERTE);
	else
		lz_raya_v(L, x + an - 1, y, al, UI_RAYA_FUERTE);
	for (i = 0; i < (int)(sizeof pasos / sizeof pasos[0]); i++)
		if ((i64)pasos[i] * P.zoom / 1000 >= 46) {
			paso = pasos[i];
			break;
		}
	menor = paso >= 10 ? paso / 5 : paso >= 2 ? paso / 2 : 1;
	if (paso == 25 || paso == 250)
		menor = paso / 5;
	lz_recorte(L, x, y, an, al);
	/* lo que se ve, en puntos de la lámina */
	if (arriba) {
		int a, b;

		vista_a_lamina(x, y, &v0, &a);
		vista_a_lamina(x + an, y, &v1, &b);
	} else {
		int a, b;

		vista_a_lamina(x, y, &a, &v0);
		vista_a_lamina(x, y + al, &b, &v1);
	}
	if (v0 < 0)
		v0 = 0;
	if (v1 > largo_img)
		v1 = largo_img;
	v0 = v0 / menor * menor;
	for (v = v0; v <= v1; v += menor) {
		int s = arriba ? vista_de_lamina_x(v) : vista_de_lamina_y(v), mayor = v % paso == 0, l = mayor ? al - 3 : al / 3;

		if (arriba)
			lz_raya_v(L, s, y + al - 1 - (mayor ? al - 2 : al / 3), mayor ? al - 2 : al / 3, 0x5A6676);
		else
			lz_raya_h(L, x + an - 1 - (mayor ? an - 2 : an / 3), s, mayor ? an - 2 : an / 3, 0x5A6676);
		(void)l;
		if (mayor) {
			char t[16];

			fmt(t, sizeof t, "%d", v);
			if (arriba)
				cifra(L, s + 2, y + 9, t);
			else {
				/* (las cifras, una encima de otra) */
				int k;

				for (k = 0; t[k]; k++) {
					char d[2] = { t[k], 0 };

					cifra(L, x + 2, s + 9 + 8 * k, d);
				}
			}
		}
	}
	/* dónde está el ratón */
	if (raton >= 0) {
		if (arriba) {
			int xy[6] = { (raton - 4) * 16, (y + 1) * 16, (raton + 5) * 16, (y + 1) * 16, raton * 16 + 8, (y + 7) * 16 };

			lz_raya_v(L, raton, y, al, UI_ACENTO);
			lz_poligono(L, xy, 3, UI_ACENTO);
		} else {
			int xy[6] = { (x + 1) * 16, (raton - 4) * 16, (x + 1) * 16, (raton + 5) * 16, (x + 7) * 16, raton * 16 + 8 };

			lz_raya_h(L, x, raton, an, UI_ACENTO);
			lz_poligono(L, xy, 3, UI_ACENTO);
		}
	}
	lz_sin_recorte(L);
}
