/* dibujo.c - el Reproductor: dibujar en un lienzo (ver dibujo.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "dibujo.h"
#include "util.h"

static lienzo *L;
static int rx0, ry0, rx1, ry1;		/* el recorte: [rx0, rx1) x [ry0, ry1) */

void dib_lienzo(lienzo *l)
{
	L = l;
	dib_recorte(0, 0, 0, 0);
}

void dib_recorte(int x, int y, int an, int al)
{
	if (!an || !al) {
		rx0 = ry0 = 0;
		rx1 = L ? L->an : 0;
		ry1 = L ? L->al : 0;
		return;
	}
	rx0 = x < 0 ? 0 : x;
	ry0 = y < 0 ? 0 : y;
	rx1 = x + an > L->an ? L->an : x + an;
	ry1 = y + al > L->al ? L->al : y + al;
}

u32 dib_mezcla_color(u32 a, u32 b, int t)
{
	u32 r = ((a >> 16 & 255) * (256 - t) + (b >> 16 & 255) * t) >> 8;
	u32 g = ((a >> 8 & 255) * (256 - t) + (b >> 8 & 255) * t) >> 8;
	u32 z = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;

	return r << 16 | g << 8 | z;
}

void dib_rect(int x, int y, int an, int al, u32 c)
{
	int x1 = x + an, y1 = y + al, i, j;

	if (x < rx0) x = rx0;
	if (y < ry0) y = ry0;
	if (x1 > rx1) x1 = rx1;
	if (y1 > ry1) y1 = ry1;
	for (j = y; j < y1; j++) {
		u32 *p = L->p + j * L->an;

		for (i = x; i < x1; i++)
			p[i] = c;
	}
}

void dib_borde(int x, int y, int an, int al, u32 c)
{
	dib_rect(x, y, an, 1, c);
	dib_rect(x, y + al - 1, an, 1, c);
	dib_rect(x, y, 1, al, c);
	dib_rect(x + an - 1, y, 1, al, c);
}

void dib_mezcla(int x, int y, u32 c, int a)
{
	u32 *p;

	if (x < rx0 || x >= rx1 || y < ry0 || y >= ry1 || a <= 0)
		return;
	p = L->p + y * L->an + x;
	*p = a >= 255 ? c : dib_mezcla_color(*p, c, a + (a >> 7));
}

/* lo que entra de la esquina redonda en la fila «f» (de 0 a r-1): los puntos
 * de cada lado que no se pintan, y el alfa del de borde */
static void esquina(int r, int f, int *fuera, int *alfa)
{
	/* el círculo de radio r con centro en (r, r): en la fila f, desde dónde */
	int dy = r - f, i;
	int lim = r * r * 16;

	*fuera = r;
	*alfa = 255;
	for (i = 0; i <= r; i++) {
		int dx = r - i, k, dentro = 0;

		/* 4 x 4 muestras en el punto */
		for (k = 0; k < 16; k++) {
			int sx = dx * 4 - (k & 3) - 0, sy = dy * 4 - (k >> 2);

			if (sx * sx + sy * sy <= lim)
				dentro++;
		}
		if (dentro) {
			*fuera = i;
			*alfa = dentro * 255 / 16;
			return;
		}
	}
}

void dib_redondo(int x, int y, int an, int al, int r, u32 c)
{
	int f;

	if (r * 2 > al)
		r = al / 2;
	if (r * 2 > an)
		r = an / 2;
	if (r <= 0) {
		dib_rect(x, y, an, al, c);
		return;
	}
	dib_rect(x, y + r, an, al - 2 * r, c);
	for (f = 0; f < r; f++) {
		int fu, a;

		esquina(r, f, &fu, &a);
		dib_rect(x + fu + 1, y + f, an - 2 * fu - 2, 1, c);
		dib_rect(x + fu + 1, y + al - 1 - f, an - 2 * fu - 2, 1, c);
		dib_mezcla(x + fu, y + f, c, a);
		dib_mezcla(x + an - 1 - fu, y + f, c, a);
		dib_mezcla(x + fu, y + al - 1 - f, c, a);
		dib_mezcla(x + an - 1 - fu, y + al - 1 - f, c, a);
	}
}

void dib_redondo_degradado(int x, int y, int an, int al, int r, u32 a, u32 b)
{
	int f;

	if (r * 2 > al)
		r = al / 2;
	if (r * 2 > an)
		r = an / 2;
	for (f = 0; f < al; f++) {
		u32 c = dib_mezcla_color(a, b, al > 1 ? f * 256 / (al - 1) : 0);
		int fu = 0, al2 = 255, g = f < r ? f : al - 1 - f;

		if (g < r && r > 0)
			esquina(r, g, &fu, &al2);
		else
			fu = -1, al2 = 0;
		if (fu >= 0) {
			dib_rect(x + fu + 1, y + f, an - 2 * fu - 2, 1, c);
			dib_mezcla(x + fu, y + f, c, al2);
			dib_mezcla(x + an - 1 - fu, y + f, c, al2);
		} else
			dib_rect(x, y + f, an, 1, c);
	}
}

void dib_redondo_borde(int x, int y, int an, int al, int r, u32 c)
{
	/* (un redondeado del color, y dentro otro un punto más pequeño del de
	 * debajo: aquí se pinta solo el contorno) */
	int f;

	if (r * 2 > al)
		r = al / 2;
	if (r * 2 > an)
		r = an / 2;
	dib_rect(x + r, y, an - 2 * r, 1, c);
	dib_rect(x + r, y + al - 1, an - 2 * r, 1, c);
	dib_rect(x, y + r, 1, al - 2 * r, c);
	dib_rect(x + an - 1, y + r, 1, al - 2 * r, c);
	for (f = 0; f < r; f++) {
		int fu, a, fu2, a2;

		esquina(r, f, &fu, &a);
		esquina(r, f + 1 < r ? f + 1 : f, &fu2, &a2);
		{
			int i;

			for (i = fu; i <= (fu2 > fu ? fu2 : fu) && i < r; i++) {
				int aa = i == fu ? a : 255;

				dib_mezcla(x + i, y + f, c, aa);
				dib_mezcla(x + an - 1 - i, y + f, c, aa);
				dib_mezcla(x + i, y + al - 1 - f, c, aa);
				dib_mezcla(x + an - 1 - i, y + al - 1 - f, c, aa);
			}
		}
	}
}

void dib_degradado(int x, int y, int an, int al, u32 a, u32 b)
{
	int f;

	for (f = 0; f < al; f++)
		dib_rect(x, y + f, an, 1, dib_mezcla_color(a, b, al > 1 ? f * 256 / (al - 1) : 0));
}

void dib_linea(int x0, int y0, int x1, int y1, u32 c)
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0;
	int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy, k = 0;

	for (;;) {
		dib_mezcla(x0, y0, c, 255);
		if ((x0 == x1 && y0 == y1) || ++k > 20000)
			break;
		{
			int e2 = 2 * e;

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
}

/* un triángulo relleno (con los bordes suavizados: 4 muestras por punto) */
void dib_triangulo(int x0, int y0, int x1, int y1, int x2, int y2, u32 c)
{
	int minx = x0, maxx = x0, miny = y0, maxy = y0, x, y;
	long area = (long)(x1 - x0) * (y2 - y0) - (long)(x2 - x0) * (y1 - y0);

	if (x1 < minx) minx = x1;
	if (x2 < minx) minx = x2;
	if (x1 > maxx) maxx = x1;
	if (x2 > maxx) maxx = x2;
	if (y1 < miny) miny = y1;
	if (y2 < miny) miny = y2;
	if (y1 > maxy) maxy = y1;
	if (y2 > maxy) maxy = y2;
	if (!area)
		return;
	for (y = miny; y <= maxy; y++)
		for (x = minx; x <= maxx; x++) {
			int k, dentro = 0;

			for (k = 0; k < 4; k++) {
				/* (en cuartos de punto) */
				long px = x * 4 + 1 + (k & 1) * 2, py = y * 4 + 1 + (k >> 1) * 2;
				long a = ((long)(x1 - x0) * 4 * (py - y0 * 4) - (long)(y1 - y0) * 4 * (px - x0 * 4));
				long b = ((long)(x2 - x1) * 4 * (py - y1 * 4) - (long)(y2 - y1) * 4 * (px - x1 * 4));
				long d = ((long)(x0 - x2) * 4 * (py - y2 * 4) - (long)(y0 - y2) * 4 * (px - x2 * 4));

				if (area > 0 ? (a >= 0 && b >= 0 && d >= 0) : (a <= 0 && b <= 0 && d <= 0))
					dentro++;
			}
			if (dentro)
				dib_mezcla(x, y, c, dentro * 255 / 4);
		}
}

void dib_circulo(int cx, int cy, int r, u32 c)
{
	int x, y;

	for (y = -r; y <= r; y++)
		for (x = -r; x <= r; x++) {
			int k, dentro = 0;

			for (k = 0; k < 4; k++) {
				int sx = x * 4 + (k & 1) * 2 - 1, sy = y * 4 + (k >> 1) * 2 - 1;

				if (sx * sx + sy * sy <= r * r * 16)
					dentro++;
			}
			if (dentro)
				dib_mezcla(cx + x, cy + y, c, dentro * 255 / 4);
		}
}

/* ------------------------------------------------------------ las letras */
static const letra *busca_letra(const fuente *f, u32 cp)
{
	int a = 0, b = f->n - 1;

	while (a <= b) {
		int m = (a + b) / 2;

		if (f->l[m].cp == cp)
			return &f->l[m];
		if (f->l[m].cp < cp)
			a = m + 1;
		else
			b = m - 1;
	}
	return NULL;
}

static const letra *letra_de(const fuente *f, u32 cp)
{
	const letra *l = busca_letra(f, cp);

	if (!l) {
		u32 ll = llana(cp);

		l = busca_letra(f, ll != cp ? ll : '?');
	}
	return l;
}

int dib_texto(const fuente *f, int x, int y, const char *t, u32 c)
{
	while (*t) {
		u32 cp = utf8_sig(&t);
		const letra *l = letra_de(f, cp);
		int i, j;

		if (!l)
			continue;
		if (x + l->dx < rx1 && x + l->dx + l->an > rx0)
			for (j = 0; j < l->al; j++) {
				int yy = y + l->dy + j;
				const u8 *p = f->p + l->off + j * l->an;

				if (yy < ry0 || yy >= ry1)
					continue;
				for (i = 0; i < l->an; i++)
					if (p[i])
						dib_mezcla(x + l->dx + i, yy, c, p[i]);
			}
		x += l->avance;
	}
	return x;
}

int dib_ancho(const fuente *f, const char *t)
{
	int x = 0;

	while (*t) {
		const letra *l = letra_de(f, utf8_sig(&t));

		if (l)
			x += l->avance;
	}
	return x;
}

int dib_texto_corto(const fuente *f, int x, int y, int an, const char *t, u32 c)
{
	char b[400];
	int n, k;

	if (dib_ancho(f, t) <= an)
		return dib_texto(f, x, y, t, c);
	copia(b, sizeof b - 4, t);
	n = (int)strlen(b);
	k = dib_ancho(f, "…");
	while (n > 0) {
		/* (de letra en letra, sin partir las de UTF-8) */
		do
			n--;
		while (n > 0 && ((u8)b[n] & 0xC0) == 0x80);
		b[n] = 0;
		if (dib_ancho(f, b) + k <= an)
			break;
	}
	while (n > 0 && b[n - 1] == ' ')
		b[--n] = 0;
	anade(b, sizeof b, "…");
	return dib_texto(f, x, y, b, c);
}

void dib_texto_centrado(const fuente *f, int x, int y, int an, const char *t, u32 c)
{
	int w = dib_ancho(f, t);

	if (w > an)
		dib_texto_corto(f, x, y, an, t, c);
	else
		dib_texto(f, x + (an - w) / 2, y, t, c);
}

/* ------------------------------------------------------------ las imágenes */
void dib_imagen(int x, int y, int an, int al, const u32 *img, int ia, int il)
{
	int i, j;

	if (!img || ia <= 0 || il <= 0 || an <= 0 || al <= 0)
		return;
	for (j = 0; j < al; j++) {
		int sy = j * il / al;

		for (i = 0; i < an; i++) {
			u32 p = img[sy * ia + i * ia / an];

			dib_mezcla(x + i, y + j, p & 0xFFFFFF, (int)(p >> 24));
		}
	}
}
