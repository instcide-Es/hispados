/* lamina.c - Pintura 2: la lámina y la mezcla de colores (lamina.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "lamina.h"

u64 lam_octetos(int an, int al)
{
	return (u64)(an > 0 ? an : 0) * (u64)(al > 0 ? al : 0) * 4;
}

int lam_nueva(lamina *l, int an, int al, u32 fondo)
{
	u32 i, n;

	l->p = NULL;
	l->an = l->al = 0;
	if (an < 1 || al < 1 || an > LAM_MAX_LADO || al > LAM_MAX_LADO)
		return 0;
	n = (u32)an * (u32)al;
	l->p = so_pide((size_t)n * 4);
	if (!l->p)
		return 0;
	if (fondo == 0)
		memset(l->p, 0, (size_t)n * 4);
	else
		for (i = 0; i < n; i++)
			l->p[i] = fondo;
	l->an = an;
	l->al = al;
	return 1;
}

void lam_suelta(lamina *l)
{
	so_suelta(l->p);
	l->p = NULL;
	l->an = l->al = 0;
}

int lam_trozo(lamina *d, const lamina *o, int x, int y, int an, int al)
{
	int f, x0, x1;

	if (!lam_nueva(d, an, al, 0))
		return 0;
	x0 = x < 0 ? -x : 0;
	x1 = x + an > o->an ? o->an - x : an;
	if (x1 <= x0)
		return 1;
	for (f = 0; f < al; f++) {
		int oy = y + f;

		if (oy < 0 || oy >= o->al)
			continue;
		memcpy(d->p + (size_t)f * an + x0, o->p + (size_t)oy * o->an + x + x0, (size_t)(x1 - x0) * 4);
	}
	return 1;
}

int lam_transparente(const u32 *p, u32 n)
{
	u32 i;

	for (i = 0; i < n; i++)
		if (ALFA(p[i]) != 255)
			return 1;
	return 0;
}

u32 lam_sobre(u32 fondo, u32 tinta, int a)
{
	u32 sa = (ALFA(tinta) * (u32)a + 127) / 255, da, oa, r, g, b, d;

	if (sa == 0)
		return fondo;
	if (sa >= 255)
		return OPACO(tinta);
	da = ALFA(fondo);
	if (da == 255) {
		u32 na = 255 - sa;

		r = (((tinta >> 16) & 255) * sa + ((fondo >> 16) & 255) * na + 127) / 255;
		g = (((tinta >> 8) & 255) * sa + ((fondo >> 8) & 255) * na + 127) / 255;
		b = ((tinta & 255) * sa + (fondo & 255) * na + 127) / 255;
		return 0xFF000000u | r << 16 | g << 8 | b;
	}
	if (da == 0)
		return sa << 24 | (tinta & 0xFFFFFF);
	{
		u32 dn = da * (255 - sa);	/* (en 255 x 255) */

		oa = sa + (dn + 127) / 255;
		d = oa * 255;
		r = (((tinta >> 16) & 255) * sa * 255 + ((fondo >> 16) & 255) * dn + d / 2) / d;
		g = (((tinta >> 8) & 255) * sa * 255 + ((fondo >> 8) & 255) * dn + d / 2) / d;
		b = ((tinta & 255) * sa * 255 + (fondo & 255) * dn + d / 2) / d;
		if (oa > 255)
			oa = 255;
		return oa << 24 | (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
	}
}

u32 lam_borra(u32 fondo, int a)
{
	u32 da = ALFA(fondo), na = (da * (u32)(255 - a) + 127) / 255;

	return na ? na << 24 | (fondo & 0xFFFFFF) : 0;
}

u32 lam_sobre_opaco(u32 f, u32 c)
{
	u32 a = ALFA(c), na, r, g, b;

	if (a == 255)
		return c & 0xFFFFFF;
	if (a == 0)
		return f & 0xFFFFFF;
	na = 255 - a;
	r = (((c >> 16) & 255) * a + ((f >> 16) & 255) * na + 127) / 255;
	g = (((c >> 8) & 255) * a + ((f >> 8) & 255) * na + 127) / 255;
	b = ((c & 255) * a + (f & 255) * na + 127) / 255;
	return r << 16 | g << 8 | b;
}
