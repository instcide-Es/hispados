/* texto.c - Hispa-PDF: el texto de una página (PDF TEXTO). Se recorre el
 * contenido sin pintar (pinta.c) y cada letra, con su sitio, se pone
 * detrás de la anterior: una línea nueva si cambia de renglón, un espacio
 * si hay hueco. En UTF-8.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pinta.h"

void recorre_texto(pdoc *d, u32 i, pletra_fn letra, void *u);

typedef struct {
	u8 *t;
	u32 n, cap;
	int hay;		/* algo escrito */
	double x_fin, y, tam;	/* dónde acabó la última letra */
	double ex, ey;
	int espacio;		/* un hueco pendiente (TJ) */
	int ultimo_blanco;
} ptxt;

static void mete(ptxt *x, const u8 *s, u32 n)
{
	if (x->n + n + 8 > x->cap) {
		if (x->cap > 64u * 1024 * 1024)
			return;
		while (x->n + n + 8 > x->cap)
			x->cap = x->cap ? x->cap * 2 : 4096;
		x->t = repide(x->t, x->cap);
	}
	memcpy(x->t + x->n, s, n);
	x->n += n;
}

static void letra(void *u, pfuente *f, u32 codigo, const u32 *uni, int n_uni, double x, double y, double ancho, double tam,
		  double ex, double ey)
{
	ptxt *t = u;
	u8 b[48], *p = b;
	int k;

	(void)f;
	if (codigo == 0xFFFFFFFF) {
		t->espacio = 1;
		return;
	}
	if (tam <= 0)
		tam = 1;
	if (t->hay) {
		/* ¿mismo renglón? (en la dirección de la letra) */
		double dx = x - t->x_fin, dy = y - t->y;
		double a_lo_largo = dx * t->ex + dy * t->ey, de_lado = -dx * t->ey + dy * t->ex;
		double ref = tam < t->tam ? tam : t->tam;

		if (de_lado > 0.6 * ref || de_lado < -0.6 * ref || a_lo_largo < -3 * ref) {
			*p++ = '\n';
			if (de_lado > 2.2 * ref)
				*p++ = '\n';
			t->ultimo_blanco = 1;
		} else if ((a_lo_largo > 0.18 * ref || t->espacio) && !t->ultimo_blanco) {
			*p++ = ' ';
			t->ultimo_blanco = 1;
		}
	}
	t->espacio = 0;
	for (k = 0; k < n_uni && k < 8; k++) {
		u32 c = uni[k];

		if (c == 0 || (c < 32 && c != '\t') || (c >= 0xD800 && c < 0xE000) || c > 0x10FFFF)
			continue;
		if (c == 0xA0)
			c = ' ';
		if (c == ' ' || c == '\t') {
			if (t->ultimo_blanco)
				continue;
			t->ultimo_blanco = 1;
		} else
			t->ultimo_blanco = 0;
		a_utf8(c, &p);
	}
	mete(t, b, (u32)(p - b));
	t->hay = 1;
	t->x_fin = x + ancho * ex;
	t->y = y + ancho * ey;
	t->tam = tam;
	t->ex = ex;
	t->ey = ey;
	if (t->ex == 0 && t->ey == 0)
		t->ex = 1;
}

u8 *pagina_texto(pdoc *d, u32 i, u32 *n)
{
	ptxt t;

	memset(&t, 0, sizeof t);
	t.ex = 1;
	recorre_texto(d, i, letra, &t);
	if (t.hay)
		mete(&t, (const u8 *)"\n", 1);
	if (!t.t)
		t.t = pide(1);
	*n = t.n;
	return t.t;
}
