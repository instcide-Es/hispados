/* lienzo.c - Correo: (PARPADEO) el lienzo propio de la ventana. Todo lo que
 * pinta Correo (ui_rect, ui_texto...) va primero aquí, fuera de la
 * pantalla, con las mismas cuentas que grafico.cas (los rectángulos, los
 * degradados, las esquinas redondas, la letra del sistema de 8x16 de
 * letra.cas: letra8x16.h), y se vuelca a la ventana de una vez (ui_vuelca:
 * una sola orden OP_MAPA con las filas que han cambiado). Así el escritorio
 * no puede enseñar nunca un fondo a medio pintar (el parpadeo de la 1.0:
 * cada cosa iba a la ventana por separado y el escritorio volcaba lo que
 * hubiera cuando le tocaba). C portable: vale igual en Hispa-64.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "lienzo.h"
#include "letra8x16.h"

int lz_crea(Lienzo *l, int an, int al)
{
	u32 *p;

	if (an < 1 || al < 1 || an > 4096 || al > 4096)
		return -1;
	if (l->p && l->an == an && l->al == al)
		return 0;
	p = so_repide(l->p, (size_t)an * (size_t)al * 4);
	if (!p)
		return -1;
	l->p = p;
	l->an = an;
	l->al = al;
	memset(p, 0xFF, (size_t)an * (size_t)al * 4);
	lz_ensucia(l, 0, al);
	return 0;
}

void lz_suelta(Lienzo *l)
{
	so_suelta(l->p);
	memset(l, 0, sizeof *l);
}

void lz_ensucia(Lienzo *l, int y, int al)
{
	int y1 = y + al;

	if (y < 0)
		y = 0;
	if (y1 > l->al)
		y1 = l->al;
	if (y1 <= y)
		return;
	if (!l->sucio) {
		l->sy0 = y;
		l->sy1 = y1;
		l->sucio = 1;
		return;
	}
	if (y < l->sy0)
		l->sy0 = y;
	if (y1 > l->sy1)
		l->sy1 = y1;
}

static void marca(Lienzo *l, int y, int al)
{
	if (!l->callado)
		lz_ensucia(l, y, al);
}

static u32 mezcla(u32 a, u32 b, u32 k)
{
	u32 q, r, v, z;

	if (k >= 256)
		return b;
	q = 256 - k;
	r = (((a >> 16) & 255) * q + ((b >> 16) & 255) * k) >> 8;
	v = (((a >> 8) & 255) * q + ((b >> 8) & 255) * k) >> 8;
	z = ((a & 255) * q + (b & 255) * k) >> 8;
	return r << 16 | v << 8 | z;
}

static void fila(Lienzo *l, int x, int y, int an, u32 c)
{
	u32 *p;

	if (y < 0 || y >= l->al)
		return;
	if (x < 0) {
		an += x;
		x = 0;
	}
	if (x + an > l->an)
		an = l->an - x;
	if (an <= 0)
		return;
	p = l->p + (size_t)y * (size_t)l->an + (size_t)x;
	while (an--)
		*p++ = c;
}

void lz_rect(Lienzo *l, int x, int y, int an, int al, u32 c)
{
	int j;

	if (an <= 0 || al <= 0)
		return;
	for (j = 0; j < al; j++)
		fila(l, x, y + j, an, c);
	marca(l, y, al);
}

void lz_marco(Lienzo *l, int x, int y, int an, int al, u32 c)
{
	if (an <= 0 || al <= 0)
		return;
	lz_rect(l, x, y, an, 1, c);
	lz_rect(l, x, y + al - 1, an, 1, c);
	lz_rect(l, x, y, 1, al, c);
	lz_rect(l, x + an - 1, y, 1, al, c);
}

static int sangria(int radio, int f)
{
	int dx, dy;

	if (f < 0 || f >= radio)
		return 0;
	dy = radio - 1 - f;
	dx = radio;
	while (dx > 0 && dx * dx + dy * dy > radio * radio)
		dx--;
	return radio - dx;
}

void lz_redondo(Lienzo *l, int x, int y, int an, int al, int radio, u32 arriba, u32 abajo)
{
	int i;

	if (an <= 0 || al <= 0)
		return;
	if (radio < 0)
		radio = 0;
	if (radio > 64)
		radio = 64;
	for (i = 0; i < al; i++) {
		int s = 0;
		u32 c;

		if (i < radio)
			s = sangria(radio, i);
		else if (i >= al - radio)
			s = sangria(radio, al - 1 - i);
		c = arriba == abajo || al < 2 ? arriba : mezcla(arriba, abajo, (u32)(i * 256 / (al - 1)));
		fila(l, x + s, y + i, an - 2 * s, c);
	}
	marca(l, y, al);
}

void lz_degradado(Lienzo *l, int x, int y, int an, int al, u32 arriba, u32 abajo)
{
	lz_redondo(l, x, y, an, al, 0, arriba, abajo);
}

void lz_linea(Lienzo *l, int x1, int y1, int x2, int y2, u32 c)
{
	int dx = x2 > x1 ? x2 - x1 : x1 - x2, dy = y2 > y1 ? y2 - y1 : y1 - y2;
	int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, e = dx - dy, e2, n = 0;
	int ya = y1 < y2 ? y1 : y2;

	for (;;) {
		if (x1 >= 0 && y1 >= 0 && x1 < l->an && y1 < l->al)
			l->p[(size_t)y1 * (size_t)l->an + (size_t)x1] = c;
		if ((x1 == x2 && y1 == y2) || ++n > 20000)
			break;
		e2 = e * 2;
		if (e2 > -dy) {
			e -= dy;
			x1 += sx;
		}
		if (e2 < dx) {
			e += dx;
			y1 += sy;
		}
	}
	marca(l, ya, dy + 1);
}

void lz_mapa(Lienzo *l, int x, int y, int an, int al, const u32 *p)
{
	int j, x0 = x < 0 ? -x : 0, x1 = x + an > l->an ? l->an - x : an;

	if (an <= 0 || al <= 0 || x1 <= x0)
		return;
	for (j = 0; j < al; j++) {
		int yy = y + j;

		if (yy < 0 || yy >= l->al)
			continue;
		memcpy(l->p + (size_t)yy * (size_t)l->an + (size_t)(x + x0), p + (size_t)j * (size_t)an + (size_t)x0,
		       (size_t)(x1 - x0) * 4);
	}
	marca(l, y, al);
}

/* n letras de la 437 de Hispa (solo sus puntos: lo de debajo se queda) */
void lz_texto(Lienzo *l, int x, int y, const u8 *t, int n, u32 c, int negrita)
{
	int i, f, b;

	for (i = 0; i < n; i++, x += 8) {
		const u8 *d = letra8x16[t[i]];

		if (x >= l->an)
			break;
		if (x <= -9)
			continue;
		for (f = 0; f < 16; f++) {
			int yy = y + f;
			u32 bits = d[f], *p;

			if (!bits || yy < 0 || yy >= l->al)
				continue;
			if (negrita)
				bits |= bits >> 1;	/* (la letra, dos veces: la segunda un punto a la derecha) */
			p = l->p + (size_t)yy * (size_t)l->an;
			for (b = 0; b < 8; b++)
				if (bits & (0x80u >> b) && x + b >= 0 && x + b < l->an)
					p[x + b] = c;
			if (negrita && (d[f] & 1) && x + 8 >= 0 && x + 8 < l->an)
				p[x + 8] = c;
		}
	}
	marca(l, y, 16);
}
