/* ui.c - la oficina de 32 bits: la cara común (oficina/comun/ui.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "ui.h"
#include "texto.h"
#include "letras.h"

/* ---- el «&» ---- */
void ui_sin_amp(const char *s, char *d, u32 cabe, int *sub)
{
	u32 k = 0, letras = 0, i;

	if (sub)
		*sub = -1;
	for (i = 0; s[i] && k + 1 < cabe; i++) {
		if (s[i] == '&' && s[i + 1] && s[i + 1] != '&') {
			if (sub)
				*sub = (int)letras;
			continue;
		}
		if (s[i] == '&')
			i++;
		d[k++] = s[i];
		if (((u8)s[i] & 0xC0) != 0x80)
			letras++;
	}
	d[k] = 0;
}

void ui_texto_amp(lienzo *l, int x, int y, const char *s, u32 c, int negrita)
{
	char t[200], pre[200];
	int sub, w0, w1;

	ui_sin_amp(s, t, sizeof t, &sub);
	if (negrita)
		lz_ui_negrita(l, x, y, t, c);
	else
		lz_ui(l, x, y, t, c);
	if (sub >= 0) {
		/* la raya debajo de su letra */
		u32 i = 0, k = 0, u;

		while (t[i] && (int)k < sub) {
			i += (u32)utf8_lee((u8 *)t + i, largo(t + i), &u);
			k++;
		}
		copia(pre, t, i + 1 < sizeof pre ? i + 1 : sizeof pre);
		w0 = negrita ? lz_ui_ancho_negrita(pre) : lz_ui_ancho(pre);
		{
			u32 j = i + (u32)utf8_lee((u8 *)t + i, largo(t + i), &u);

			copia(pre, t, j + 1 < sizeof pre ? j + 1 : sizeof pre);
		}
		w1 = negrita ? lz_ui_ancho_negrita(pre) : lz_ui_ancho(pre);
		lz_raya_h(l, x + w0, y + 2, w1 - w0, c);
	}
}

int ui_ancho_amp(const char *s)
{
	char t[200];

	ui_sin_amp(s, t, sizeof t, NULL);
	return lz_ui_ancho(t);
}

/* ---- la barra de menús ---- */
int ui_alto_menus(void)
{
	return lz_ui_alto() + 4;
}

/* el rótulo i de «a|b|c» (a d) */
static int trozo(const char *s, int i, char *d, u32 cabe)
{
	int k = 0;
	u32 j = 0;

	while (*s && k < i) {
		if (*s == '|')
			k++;
		s++;
	}
	if (k < i)
		return 0;
	while (*s && *s != '|' && j + 1 < cabe)
		d[j++] = *s++;
	d[j] = 0;
	return 1;
}

int ui_barra_menus(lienzo *l, int x, int y, int an, const char *rot, int abierto, int encima)
{
	int h = ui_alto_menus(), i, cx = x + 6;
	char t[64];

	lz_rect(l, x, y, an, h, UI_FONDO2);
	for (i = 0; trozo(rot, i, t, sizeof t); i++) {
		int w = ui_ancho_amp(t) + 16;

		if (i == abierto) {
			lz_redondeado(l, cx, y + 2, w, h - 4, 3, UI_PULSADO);
			lz_marco_redondeado(l, cx, y + 2, w, h - 4, 3, UI_ACENTO);
		} else if (i == encima)
			lz_redondeado(l, cx, y + 2, w, h - 4, 3, UI_ENCIMA);
		ui_texto_amp(l, cx + 8, y + (h - lz_ui_alto()) / 2 + lz_ui_base(), t, UI_TINTA, 0);
		cx += w;
	}
	return h;
}

int ui_menu_en(const char *rot, int x0, int x, int *xi)
{
	int i, cx = x0 + 6;
	char t[64];

	for (i = 0; trozo(rot, i, t, sizeof t); i++) {
		int w = ui_ancho_amp(t) + 16;

		if (x >= cx && x < cx + w) {
			if (xi)
				*xi = cx;
			return i;
		}
		cx += w;
	}
	return -1;
}

int ui_menu_letra(const char *rot, u32 letra)
{
	int i;
	char t[64];

	letra = minuscula(sin_tilde(letra));
	for (i = 0; trozo(rot, i, t, sizeof t); i++) {
		const char *a = strchr(t, '&');
		u32 c;

		if (!a)
			continue;
		utf8_lee((const u8 *)a + 1, largo(a + 1), &c);
		if (minuscula(sin_tilde(c)) == letra)
			return i;
	}
	return -1;
}

/* ---- las barras de herramientas ---- */
int ui_alto_barra(void)
{
	return lz_ui_alto() + 12;
}

int ui_barra(lienzo *l, int x, int y, int an, ui_boton *b, int n, int encima, int pulsado)
{
	int h = ui_alto_barra(), i, cx = x + 4, lado = h - 6, ic = lz_ui_alto() - 4;

	lz_rect(l, x, y, an, h, UI_FONDO);
	lz_raya_h(l, x, y + h - 1, an, UI_RAYA);
	for (i = 0; i < n; i++) {
		ui_boton *p = &b[i];

		p->y = y + 3;
		p->al = lado;
		p->x = cx;
		switch (p->tipo) {
		case UB_SEPARA:
			p->an = 9;
			lz_raya_v(l, cx + 4, y + 6, h - 12, UI_RAYA);
			break;
		case UB_LISTA: {
			int w = p->ancho, fx;
			char t[64];

			p->an = w;
			lz_rect(l, cx, p->y + 1, w, lado - 2, p->apagado ? UI_FONDO2 : UI_BLANCO);
			lz_marco(l, cx, p->y + 1, w, lado - 2, i == encima ? UI_ACENTO : UI_RAYA_FUERTE);
			/* el texto, recortado */
			copia(t, p->texto, sizeof t);
			while (t[0] && lz_ui_ancho(t) > w - lado - 6) {
				u32 k = largo(t);

				while (k > 0 && ((u8)t[k - 1] & 0xC0) == 0x80)
					k--;
				if (k)
					t[k - 1] = 0;
			}
			lz_recorte(l, cx + 1, p->y, w - lado, lado);
			lz_ui(l, cx + 5, p->y + (lado - lz_ui_alto()) / 2 + lz_ui_base(), t, p->apagado ? UI_TINTA_GRIS : UI_TINTA);
			lz_sin_recorte(l);
			/* la flecha */
			fx = cx + w - lado + 2;
			lz_rect(l, fx, p->y + 2, lado - 3, lado - 4, i == encima ? UI_ENCIMA : UI_FONDO);
			lz_raya_v(l, fx - 1, p->y + 2, lado - 4, UI_RAYA);
			{
				int cxf = fx + (lado - 3) / 2, cyf = p->y + lado / 2;
				int xy[6] = { (cxf - 4) * 16, (cyf - 2) * 16, (cxf + 4) * 16, (cyf - 2) * 16, cxf * 16, (cyf + 3) * 16 };

				lz_poligono(l, xy, 3, UI_TINTA);
			}
			break;
		}
		default: {
			int w = p->tipo == UB_COLOR ? lado + 12 : lado;

			p->an = w;
			if (p->activo || i == pulsado) {
				lz_redondeado(l, cx, p->y, w, lado, 3, UI_PULSADO);
				lz_marco_redondeado(l, cx, p->y, w, lado, 3, UI_ACENTO);
				/* (además del color: una raya abajo, que se ve sin distinguirlo) */
				if (p->activo)
					lz_rect(l, cx + 4, p->y + lado - 3, w - 8, 2, UI_ACENTO);
			} else if (i == encima && !p->apagado) {
				lz_redondeado(l, cx, p->y, w, lado, 3, UI_ENCIMA);
				lz_marco_redondeado(l, cx, p->y, w, lado, 3, UI_RAYA_FUERTE);
			}
			ui_icono(l, p->icono, cx + (lado - ic) / 2, p->y + (lado - ic) / 2 - (p->tipo == UB_COLOR ? 2 : 0), ic, p->apagado);
			if (p->tipo == UB_COLOR) {
				int cxf = cx + w - 7, cyf = p->y + lado / 2;
				int xy[6] = { (cxf - 3) * 16, (cyf - 1) * 16, (cxf + 3) * 16, (cyf - 1) * 16, cxf * 16, (cyf + 3) * 16 };

				lz_rect(l, cx + (lado - ic) / 2, p->y + lado - 6, ic, 4, p->color == 0xFF000000u ? UI_BLANCO : p->color);
				lz_marco(l, cx + (lado - ic) / 2, p->y + lado - 6, ic, 4, UI_RAYA_FUERTE);
				lz_poligono(l, xy, 3, UI_TINTA);
				lz_raya_v(l, cx + w - 13, p->y + 4, lado - 8, UI_RAYA);
			}
		}
		}
		cx += p->an + 2;
		(void)an;
	}
	return h;
}

int ui_boton_en(const ui_boton *b, int n, int x, int y, int *flecha)
{
	int i;

	if (flecha)
		*flecha = 0;
	for (i = 0; i < n; i++)
		if (b[i].tipo != UB_SEPARA && x >= b[i].x && x < b[i].x + b[i].an && y >= b[i].y && y < b[i].y + b[i].al) {
			if (flecha) {
				if (b[i].tipo == UB_LISTA)
					*flecha = x >= b[i].x + b[i].an - b[i].al;
				else if (b[i].tipo == UB_COLOR)
					*flecha = x >= b[i].x + b[i].an - 13;
			}
			return i;
		}
	return -1;
}

ui_boton *ui_boton_id(ui_boton *b, int n, int id)
{
	int i;

	for (i = 0; i < n; i++)
		if (b[i].id == id)
			return &b[i];
	return NULL;
}

/* ---- la regla ---- */
int ui_alto_regla(void)
{
	return lz_ui_alto() + 4;
}

int ui_regla_px(const ui_regla *r, i32 tw)
{
	return r->x_pag + (int)(((i64)(tw + r->marg_izq) * r->zoom + 750) / 1500);
}

i32 ui_regla_tw(const ui_regla *r, int x)
{
	return (i32)((i64)(x - r->x_pag) * 1500 / r->zoom) - r->marg_izq;
}

static void marca_tab(lienzo *l, int x, int y, int tipo, u32 c)
{
	/* └ ┴ ┘ y el de decimales (con su punto): se distinguen por la forma */
	switch (tipo) {
	case 0: /* izquierda */
		lz_rect(l, x, y, 2, 7, c);
		lz_rect(l, x, y + 5, 7, 2, c);
		break;
	case 1: /* centro */
		lz_rect(l, x, y, 2, 7, c);
		lz_rect(l, x - 4, y + 5, 10, 2, c);
		break;
	case 2: /* derecha */
		lz_rect(l, x, y, 2, 7, c);
		lz_rect(l, x - 5, y + 5, 7, 2, c);
		break;
	default: /* decimal */
		lz_rect(l, x, y, 2, 7, c);
		lz_rect(l, x - 4, y + 5, 10, 2, c);
		lz_rect(l, x + 4, y + 1, 2, 2, c);
	}
}

void ui_regla_pinta(lienzo *l, int x, int y, int an, const ui_regla *r, int arr)
{
	int h = ui_alto_regla(), m0 = ui_regla_px(r, 0), m1 = ui_regla_px(r, r->ancho_pag - r->marg_izq - r->marg_der);
	int p0 = ui_regla_px(r, -r->marg_izq), p1 = ui_regla_px(r, r->ancho_pag - r->marg_izq), k;
	i32 cm;
	int zx0 = r->fin > r->inicio ? ui_regla_px(r, r->inicio) : m0, zx1 = r->fin > r->inicio ? ui_regla_px(r, r->fin) : m1;

	(void)arr;
	lz_rect(l, x, y, an, h, UI_FONDO);
	lz_raya_h(l, x, y + h - 1, an, UI_RAYA);
	/* el botón del tipo de tabulación, a la izquierda */
	lz_rect(l, x + 2, y + 3, h - 6, h - 6, UI_BLANCO);
	lz_marco(l, x + 2, y + 3, h - 6, h - 6, UI_RAYA_FUERTE);
	marca_tab(l, x + 2 + (h - 6) / 2 - 1, y + h / 2 - 4, r->tab_nuevo, UI_TINTA);
	lz_recorte(l, x + h, y, an - h, h);
	/* el papel: gris en los márgenes, blanco lo de escribir */
	lz_rect(l, p0, y + 4, p1 - p0, h - 9, 0xD5DBE4);
	lz_rect(l, zx0, y + 4, zx1 - zx0, h - 9, UI_BLANCO);
	lz_marco(l, p0, y + 4, p1 - p0, h - 9, UI_RAYA_FUERTE);
	/* los centímetros (desde el margen) y las medias */
	for (cm = -((r->marg_izq / 567) + 1) * 2; ; cm++) {
		i32 tw = cm * 567 / 2;
		int px = ui_regla_px(r, tw);

		if (px > p1)
			break;
		if (px < p0)
			continue;
		if (cm % 2 == 0 && cm != 0) {
			char t[8];
			int w;

			fmt(t, sizeof t, "%d", cm < 0 ? -cm / 2 : cm / 2);
			w = lz_ui_ancho(t) * 3 / 4;
			lz_texto(l, letra_cara(FAM_SANS, 0, 0, NULL), 40, 0, px - w / 2, y + h - 8, t, UI_TINTA);
		} else if (cm != 0)
			lz_raya_v(l, px, y + h / 2 - 1, 3, UI_TINTA_GRIS);
	}
	/* las sangrías: la de la primera línea arriba (▽), la izquierda y la derecha abajo (△) */
	{
		int xp = ui_regla_px(r, r->inicio + r->sang_izq + r->sang_primera), xi = ui_regla_px(r, r->inicio + r->sang_izq);
		int xd = ui_regla_px(r, (r->fin > r->inicio ? r->fin : r->ancho_pag - r->marg_izq - r->marg_der) - r->sang_der);
		int a[6] = { (xp - 5) * 16, (y + 2) * 16, (xp + 5) * 16, (y + 2) * 16, xp * 16, (y + 8) * 16 };
		int b[6] = { (xi - 5) * 16, (y + h - 3) * 16, (xi + 5) * 16, (y + h - 3) * 16, xi * 16, (y + h - 9) * 16 };
		int c[6] = { (xd - 5) * 16, (y + h - 3) * 16, (xd + 5) * 16, (y + h - 3) * 16, xd * 16, (y + h - 9) * 16 };

		lz_poligono(l, a, 3, UI_ACENTO);
		lz_poligono(l, b, 3, UI_ACENTO);
		lz_rect(l, xi - 5, y + h - 3, 11, 2, UI_ACENTO);	/* (la de la izquierda lleva una base: mueve las dos) */
		lz_poligono(l, c, 3, UI_ACENTO);
	}
	for (k = 0; k < r->n_tabs; k++)
		marca_tab(l, ui_regla_px(r, r->inicio + r->tabs[k]) - 1, y + h - 11, r->tipo_tab[k], UI_TINTA);
	lz_sin_recorte(l);
}

int ui_regla_en(const ui_regla *r, int x, int yr, int *cual)
{
	int h = ui_alto_regla(), k;
	int xp = ui_regla_px(r, r->inicio + r->sang_izq + r->sang_primera), xi = ui_regla_px(r, r->inicio + r->sang_izq);
	int xd = ui_regla_px(r, (r->fin > r->inicio ? r->fin : r->ancho_pag - r->marg_izq - r->marg_der) - r->sang_der);

	if (cual)
		*cual = -1;
	if (x < h)
		return RG_TIPO;
	if (yr < h / 2 && x >= xp - 6 && x <= xp + 6)
		return RG_PRIMERA;
	if (yr >= h / 2 && x >= xi - 6 && x <= xi + 6)
		return RG_IZQ;
	if (yr >= h / 2 && x >= xd - 6 && x <= xd + 6)
		return RG_DER;
	for (k = 0; k < r->n_tabs; k++) {
		int t = ui_regla_px(r, r->inicio + r->tabs[k]);

		if (x >= t - 5 && x <= t + 6) {
			if (cual)
				*cual = k;
			return RG_TAB;
		}
	}
	if (x > ui_regla_px(r, 0) && x < ui_regla_px(r, r->ancho_pag - r->marg_izq - r->marg_der))
		return RG_ZONA;
	return RG_NADA;
}

/* ---- las barras de desplazamiento ---- */
static void flecha(lienzo *l, int cx, int cy, int dir, u32 c)
{
	int xy[6];

	switch (dir) {
	case 0: xy[0] = cx - 4; xy[1] = cy + 2; xy[2] = cx + 4; xy[3] = cy + 2; xy[4] = cx; xy[5] = cy - 3; break;
	case 1: xy[0] = cx - 4; xy[1] = cy - 2; xy[2] = cx + 4; xy[3] = cy - 2; xy[4] = cx; xy[5] = cy + 3; break;
	case 2: xy[0] = cx + 2; xy[1] = cy - 4; xy[2] = cx + 2; xy[3] = cy + 4; xy[4] = cx - 3; xy[5] = cy; break;
	default: xy[0] = cx - 2; xy[1] = cy - 4; xy[2] = cx - 2; xy[3] = cy + 4; xy[4] = cx + 3; xy[5] = cy; break;
	}
	{
		int k;

		for (k = 0; k < 6; k++)
			xy[k] *= 16;
	}
	lz_poligono(l, xy, 3, c);
}

static void tirador(const ui_desp *d, int *t0, int *t1)
{
	int vert = d->al > d->an, largo_ = (vert ? d->al : d->an) - 2 * UI_DESP, t;

	if (d->total <= d->visto || largo_ < 10) {
		*t0 = 0;
		*t1 = largo_;
		return;
	}
	t = (int)((i64)largo_ * d->visto / d->total);
	if (t < 18)
		t = 18;
	if (t > largo_)
		t = largo_;
	*t0 = (int)((i64)(largo_ - t) * d->pos / (d->total - d->visto));
	if (*t0 < 0)
		*t0 = 0;
	if (*t0 > largo_ - t)
		*t0 = largo_ - t;
	*t1 = *t0 + t;
}

void ui_desp_pinta(lienzo *l, const ui_desp *d, int encima)
{
	int vert = d->al > d->an, t0, t1;

	lz_rect(l, d->x, d->y, d->an, d->al, UI_FONDO);
	if (vert) {
		lz_raya_v(l, d->x, d->y, d->al, UI_RAYA);
		flecha(l, d->x + d->an / 2, d->y + UI_DESP / 2, 0, UI_TINTA);
		flecha(l, d->x + d->an / 2, d->y + d->al - UI_DESP / 2, 1, UI_TINTA);
		tirador(d, &t0, &t1);
		lz_redondeado(l, d->x + 3, d->y + UI_DESP + t0, d->an - 5, t1 - t0, 4, encima == 5 ? UI_RAYA_FUERTE : 0xB4BFCD);
	} else {
		lz_raya_h(l, d->x, d->y, d->an, UI_RAYA);
		flecha(l, d->x + UI_DESP / 2, d->y + d->al / 2, 2, UI_TINTA);
		flecha(l, d->x + d->an - UI_DESP / 2, d->y + d->al / 2, 3, UI_TINTA);
		tirador(d, &t0, &t1);
		lz_redondeado(l, d->x + UI_DESP + t0, d->y + 3, t1 - t0, d->al - 5, 4, encima == 5 ? UI_RAYA_FUERTE : 0xB4BFCD);
	}
}

int ui_desp_en(const ui_desp *d, int x, int y)
{
	int vert = d->al > d->an, t0, t1, p;

	if (x < d->x || x >= d->x + d->an || y < d->y || y >= d->y + d->al)
		return 0;
	p = vert ? y - d->y : x - d->x;
	if (p < UI_DESP)
		return 1;
	if (p >= (vert ? d->al : d->an) - UI_DESP)
		return 2;
	tirador(d, &t0, &t1);
	p -= UI_DESP;
	if (p < t0)
		return 3;
	if (p >= t1)
		return 4;
	return 5;
}

i32 ui_desp_arrastra(const ui_desp *d, i32 pos0, int de, int a)
{
	int vert = d->al > d->an, t0, t1, largo_ = (vert ? d->al : d->an) - 2 * UI_DESP;
	i32 p;

	tirador(d, &t0, &t1);
	if (largo_ - (t1 - t0) <= 0)
		return 0;
	p = pos0 + (i32)((i64)(a - de) * (d->total - d->visto) / (largo_ - (t1 - t0)));
	if (p > d->total - d->visto)
		p = d->total - d->visto;
	if (p < 0)
		p = 0;
	return p;
}

/* ---- la barra de estado ---- */
int ui_alto_estado(void)
{
	return lz_ui_alto() + 4;
}

void ui_estado(lienzo *l, int x, int y, int an, const char *tr)
{
	int h = ui_alto_estado(), i, n = 1, cx = x + 8;
	char t[200];
	const char *s;

	for (s = tr; *s; s++)
		if (*s == '|')
			n++;
	lz_rect(l, x, y, an, h, UI_FONDO);
	lz_raya_h(l, x, y, an, UI_RAYA);
	for (i = 0; i < n && trozo(tr, i, t, sizeof t); i++) {
		int w = lz_ui_ancho(t);

		if (i == n - 1 && n > 1) {
			lz_raya_v(l, x + an - w - 18, y + 4, h - 8, UI_RAYA);
			lz_ui(l, x + an - w - 8, y + (h - lz_ui_alto()) / 2 + lz_ui_base(), t, UI_TINTA);
			break;
		}
		lz_ui(l, cx, y + (h - lz_ui_alto()) / 2 + lz_ui_base(), t, UI_TINTA);
		cx += w + 12;
		lz_raya_v(l, cx - 6, y + 4, h - 8, UI_RAYA);
	}
}

/* ---- los cuadros ---- */
int ui_alto_control(void)
{
	return lz_ui_alto() + 8;
}

ui_control *ui_control_id(ui_cuadro *q, int id)
{
	int i;

	for (i = 0; i < q->n; i++)
		if (q->c[i].id == id)
			return &q->c[i];
	return NULL;
}

int ui_numero(const ui_control *c)
{
	const char *s = c->valor;
	double v = numero_real(s, NULL);

	return (int)(v * 10 + (v < 0 ? -0.5 : 0.5));
}

void ui_pon_numero(ui_control *c, int dec)
{
	if (dec < c->min)
		dec = c->min;
	if (dec > c->max)
		dec = c->max;
	if (c->decimales && dec % 10)
		fmt(c->valor, sizeof c->valor, "%d,%d", dec / 10, (dec < 0 ? -dec : dec) % 10);
	else
		fmt(c->valor, sizeof c->valor, "%d", dec / 10);
	c->cursor = c->ancla = (int)utf8_cuenta(c->valor, largo(c->valor));
}

static int recibe_foco(const ui_control *c)
{
	return !c->apagado && c->tipo != UC_ROTULO && c->tipo != UC_TEXTO && c->tipo != UC_MUESTRA;
}

/* la opción i (de «a|b|c») */
static void opcion(const char *ops, int i, char *d, u32 cabe)
{
	if (!ops || !trozo(ops, i, d, cabe))
		d[0] = 0;
}

static int n_opciones(const char *ops)
{
	int n = 1;

	if (!ops || !*ops)
		return 0;
	for (; *ops; ops++)
		if (*ops == '|')
			n++;
	return n;
}

/* el byte de la letra k del texto */
static u32 byte_de(const char *s, int k)
{
	u32 i = 0, c;
	int j = 0;

	while (s[i] && j < k) {
		i += (u32)utf8_lee((const u8 *)s + i, largo(s + i), &c);
		j++;
	}
	return i;
}

static void pinta_campo(lienzo *l, ui_control *c, int x, int y, int foco)
{
	int h = c->al ? c->al : ui_alto_control(), tx = x + 6, base = y + (h - lz_ui_alto()) / 2 + lz_ui_base();
	char pre[260];
	int a = c->ancla < c->cursor ? c->ancla : c->cursor, b = c->ancla < c->cursor ? c->cursor : c->ancla;
	int an = c->tipo == UC_NUMERO ? c->an - 18 : c->an;

	lz_rect(l, x, y, an, h, c->apagado ? UI_FONDO : UI_BLANCO);
	if (foco)
		lz_marco_grueso(l, x, y, an, h, 2, UI_ACENTO);
	else
		lz_marco(l, x, y, an, h, UI_RAYA_FUERTE);
	lz_recorte(l, x + 2, y + 2, an - 4, h - 4);
	if (foco && a != b) {
		int x0, x1;

		copia(pre, c->valor, byte_de(c->valor, a) + 1);
		x0 = lz_ui_ancho(pre);
		copia(pre, c->valor, byte_de(c->valor, b) + 1);
		x1 = lz_ui_ancho(pre);
		lz_rect(l, tx + x0, y + 4, x1 - x0, h - 8, UI_PULSADO);
	}
	lz_ui(l, tx, base, c->valor, c->apagado ? UI_TINTA_GRIS : UI_TINTA);
	if (foco) {
		copia(pre, c->valor, byte_de(c->valor, c->cursor) + 1);
		lz_rect(l, tx + lz_ui_ancho(pre), y + 4, 2, h - 8, UI_TINTA);
	}
	lz_sin_recorte(l);
	if (c->tipo == UC_NUMERO) {
		/* las flechitas de subir y bajar */
		int fx = x + an + 2, m = h / 2;

		lz_rect(l, fx, y, 16, h, UI_FONDO);
		lz_marco(l, fx, y, 16, m, UI_RAYA_FUERTE);
		lz_marco(l, fx, y + m, 16, h - m, UI_RAYA_FUERTE);
		flecha(l, fx + 8, y + m / 2, 0, UI_TINTA);
		flecha(l, fx + 8, y + m + (h - m) / 2, 1, UI_TINTA);
		if (c->unidad)
			lz_ui(l, fx + 22, base, c->unidad, UI_TINTA);
	}
}

void ui_cuadro_pinta(lienzo *l, ui_cuadro *q)
{
	int th = lz_ui_alto() + 10, i, x, y;

	q->x = (l->an - q->an) / 2;
	q->y = (l->al - q->al - th) / 2;
	if (q->x < 0)
		q->x = 0;
	if (q->y < 0)
		q->y = 0;
	x = q->x;
	y = q->y;
	/* la sombra, el marco y el título */
	lz_rect_a(l, x + 4, y + 4, q->an, q->al + th, 0x000000, 60);
	lz_rect(l, x, y, q->an, q->al + th, UI_FONDO2);
	lz_degradado(l, x, y, q->an, th, 0x3B74D6, 0x2558B5);
	lz_ui_negrita(l, x + 10, y + (th - lz_ui_alto()) / 2 + lz_ui_base(), q->titulo, UI_BLANCO);
	lz_marco(l, x, y, q->an, q->al + th, 0x1E4A99);
	y += th;
	for (i = 0; i < q->n; i++) {
		ui_control *c = &q->c[i];
		int cx = x + c->x, cy = y + c->y, h = c->al ? c->al : ui_alto_control(), foco = i == q->foco;
		int base = cy + (h - lz_ui_alto()) / 2 + lz_ui_base();
		u32 tinta = c->apagado ? UI_TINTA_GRIS : UI_TINTA;

		switch (c->tipo) {
		case UC_ROTULO:
			ui_texto_amp(l, cx, base, c->rotulo, tinta, 0);
			break;
		case UC_TEXTO: {
			/* un texto de varias líneas (separadas por \n) */
			const char *s = c->rotulo;
			int ly = cy + lz_ui_base();

			while (s && *s) {
				const char *e = strchr(s, '\n');
				char t[260];
				u32 k = e ? (u32)(e - s) : largo(s);

				copia(t, s, k + 1 < sizeof t ? k + 1 : sizeof t);
				lz_ui(l, cx, ly, t, tinta);
				ly += lz_ui_alto();
				s = e ? e + 1 : NULL;
			}
			break;
		}
		case UC_CAMPO:
		case UC_NUMERO:
			pinta_campo(l, c, cx, cy, foco);
			break;
		case UC_BOTON: {
			int pulsado = q->pulsado == i;
			u32 fondo = c->principal ? 0x2F6FDB : 0xFFFFFF;

			if (c->apagado)
				fondo = UI_FONDO;
			else if (pulsado)
				fondo = c->principal ? 0x1F55B0 : UI_PULSADO;
			lz_redondeado(l, cx, cy, c->an, h, 4, fondo);
			lz_marco_redondeado(l, cx, cy, c->an, h, 4, c->principal ? 0x1E4A99 : UI_RAYA_FUERTE);
			if (foco)
				lz_marco_grueso(l, cx - 3, cy - 3, c->an + 6, h + 6, 2, UI_ACENTO);
			{
				int w = ui_ancho_amp(c->rotulo);

				ui_texto_amp(l, cx + (c->an - w) / 2, base, c->rotulo, c->principal && !c->apagado ? UI_BLANCO : tinta, c->principal);
			}
			break;
		}
		case UC_CASILLA:
		case UC_OPCION: {
			int lado = lz_ui_alto() - 6, by = cy + (h - lado) / 2;

			if (c->tipo == UC_CASILLA) {
				lz_rect(l, cx, by, lado, lado, UI_BLANCO);
				lz_marco(l, cx, by, lado, lado, UI_RAYA_FUERTE);
				if (c->marcado) {
					/* una marca de verdad (✓), no solo color */
					lz_linea(l, (cx + 3) * 16, (by + lado / 2) * 16, (cx + lado / 2 - 1) * 16, (by + lado - 4) * 16, 40, UI_TINTA);
					lz_linea(l, (cx + lado / 2 - 1) * 16, (by + lado - 4) * 16, (cx + lado - 3) * 16, (by + 3) * 16, 40, UI_TINTA);
				}
			} else {
				lz_circulo(l, (cx + lado / 2) * 16, (by + lado / 2) * 16, lado * 8, UI_RAYA_FUERTE);
				lz_circulo(l, (cx + lado / 2) * 16, (by + lado / 2) * 16, lado * 8 - 16, UI_BLANCO);
				if (c->marcado)
					lz_circulo(l, (cx + lado / 2) * 16, (by + lado / 2) * 16, lado * 4, UI_TINTA);
			}
			ui_texto_amp(l, cx + lado + 6, base, c->rotulo, tinta, 0);
			if (foco)
				lz_marco(l, cx - 3, cy + 1, c->an + 3, h - 2, UI_ACENTO);
			break;
		}
		case UC_DESPLEGABLE: {
			char t[200];
			int lado = h;

			opcion(c->opciones, c->elegida, t, sizeof t);
			lz_rect(l, cx, cy, c->an, h, c->apagado ? UI_FONDO : UI_BLANCO);
			if (foco)
				lz_marco_grueso(l, cx, cy, c->an, h, 2, UI_ACENTO);
			else
				lz_marco(l, cx, cy, c->an, h, UI_RAYA_FUERTE);
			lz_recorte(l, cx + 2, cy, c->an - lado - 2, h);
			lz_ui(l, cx + 6, base, t, tinta);
			lz_sin_recorte(l);
			lz_rect(l, cx + c->an - lado + 1, cy + 1, lado - 2, h - 2, UI_FONDO);
			flecha(l, cx + c->an - lado / 2, cy + h / 2, 1, UI_TINTA);
			break;
		}
		case UC_LISTA: {
			int fila = lz_ui_alto() + 2, n = n_opciones(c->opciones), vis = (c->al - 4) / fila, k;

			lz_rect(l, cx, cy, c->an, c->al, UI_BLANCO);
			if (foco)
				lz_marco_grueso(l, cx, cy, c->an, c->al, 2, UI_ACENTO);
			else
				lz_marco(l, cx, cy, c->an, c->al, UI_RAYA_FUERTE);
			if (c->elegida < c->primera)
				c->primera = c->elegida;
			if (c->elegida >= c->primera + vis)
				c->primera = c->elegida - vis + 1;
			if (c->primera < 0)
				c->primera = 0;
			lz_recorte(l, cx + 2, cy + 2, c->an - 4, c->al - 4);
			for (k = 0; k < vis && c->primera + k < n; k++) {
				char t[200];
				int ry = cy + 2 + k * fila, sel = c->primera + k == c->elegida;

				opcion(c->opciones, c->primera + k, t, sizeof t);
				if (sel) {
					lz_rect(l, cx + 2, ry, c->an - 4, fila, UI_ACENTO);
					/* (y una flecha delante: no solo el color) */
					flecha(l, cx + 7, ry + fila / 2, 3, UI_BLANCO);
				}
				lz_ui(l, cx + 14, ry + 1 + lz_ui_base(), t, sel ? UI_BLANCO : UI_TINTA);
			}
			lz_sin_recorte(l);
			break;
		}
		case UC_COLOR:
			lz_rect(l, cx, cy, c->an, h, UI_BLANCO);
			lz_marco(l, cx, cy, c->an, h, foco ? UI_ACENTO : UI_RAYA_FUERTE);
			if (c->color == 0xFF000000u) {
				lz_ui(l, cx + 6, base, "Automático", UI_TINTA);
			} else {
				lz_rect(l, cx + 4, cy + 4, h - 8, h - 8, c->color);
				lz_marco(l, cx + 4, cy + 4, h - 8, h - 8, UI_TINTA);
				if (c->rotulo)
					lz_ui(l, cx + h + 2, base, c->rotulo, UI_TINTA);
			}
			break;
		case UC_MUESTRA:
			lz_rect(l, cx, cy, c->an, c->al, UI_BLANCO);
			lz_marco(l, cx, cy, c->an, c->al, UI_RAYA_FUERTE);
			if (q->pinta_extra) {
				lz_recorte(l, cx + 1, cy + 1, c->an - 2, c->al - 2);
				q->pinta_extra(q->u, l, c, cx, cy);
				lz_sin_recorte(l);
			}
			break;
		}
	}
}

static void pasa_foco(ui_cuadro *q, int atras)
{
	int i, k = q->foco;

	for (i = 0; i < q->n; i++) {
		k = atras ? (k + q->n - 1) % q->n : (k + 1) % q->n;
		if (recibe_foco(&q->c[k])) {
			q->foco = k;
			if (q->c[k].tipo == UC_CAMPO || q->c[k].tipo == UC_NUMERO) {
				q->c[k].ancla = 0;
				q->c[k].cursor = (int)utf8_cuenta(q->c[k].valor, largo(q->c[k].valor));
			}
			return;
		}
	}
}

static void marca_opcion(ui_cuadro *q, ui_control *c)
{
	int i;

	for (i = 0; i < q->n; i++)
		if (q->c[i].tipo == UC_OPCION && q->c[i].grupo == c->grupo)
			q->c[i].marcado = 0;
	c->marcado = 1;
}

static void borra_eleccion(ui_control *c)
{
	int a = c->ancla < c->cursor ? c->ancla : c->cursor, b = c->ancla < c->cursor ? c->cursor : c->ancla;
	u32 ba = byte_de(c->valor, a), bb = byte_de(c->valor, b);

	memmove(c->valor + ba, c->valor + bb, largo(c->valor + bb) + 1);
	c->cursor = c->ancla = a;
}

static int tecla_campo(ui_control *c, const so_suceso *s)
{
	u32 t = s->tecla;
	int n = (int)utf8_cuenta(c->valor, largo(c->valor)), mayus = (s->mods & SO_CON_MAYUS) != 0;

	if (t == SO_T_IZQUIERDA || t == SO_T_DERECHA || t == SO_T_INICIO || t == SO_T_FIN) {
		int p = c->cursor;

		if (t == SO_T_IZQUIERDA && p > 0)
			p--;
		else if (t == SO_T_DERECHA && p < n)
			p++;
		else if (t == SO_T_INICIO)
			p = 0;
		else if (t == SO_T_FIN)
			p = n;
		c->cursor = p;
		if (!mayus)
			c->ancla = p;
		return 1;
	}
	if (t == SO_RETROCESO || t == SO_T_SUPR) {
		if (c->ancla != c->cursor)
			borra_eleccion(c);
		else if (t == SO_RETROCESO && c->cursor > 0) {
			c->ancla = c->cursor - 1;
			borra_eleccion(c);
		} else if (t == SO_T_SUPR && c->cursor < n) {
			c->ancla = c->cursor + 1;
			borra_eleccion(c);
		}
		return 1;
	}
	if (t == 1 && (s->mods & SO_CON_CONTROL)) {	/* Ctrl+A: todo (en los campos, como en todas partes) */
		c->ancla = 0;
		c->cursor = n;
		return 1;
	}
	if (t == 22 && (s->mods & SO_CON_CONTROL)) {	/* Ctrl+V */
		u32 m;
		char *p = so_del_portapapeles(&m);

		if (p) {
			u32 i = 0;

			if (c->ancla != c->cursor)
				borra_eleccion(c);
			while (i < m) {
				u32 u, b2;
				int k = utf8_lee((u8 *)p + i, m - i, &u);
				char e[4];

				if (u < 0x20)
					break;
				b2 = byte_de(c->valor, c->cursor);
				if (largo(c->valor) + (u32)k + 1 >= sizeof c->valor)
					break;
				utf8_pon(u, e);
				memmove(c->valor + b2 + k, c->valor + b2, largo(c->valor + b2) + 1);
				memcpy(c->valor + b2, e, (size_t)k);
				c->cursor++;
				i += (u32)k;
			}
			c->ancla = c->cursor;
			so_suelta(p);
		}
		return 1;
	}
	if (t >= 0x20 && t < SO_T_BASE && !(s->mods & SO_CON_CONTROL)) {
		char e[4];
		int k = utf8_pon(t, e);
		u32 b;

		if (c->tipo == UC_NUMERO && !((t >= '0' && t <= '9') || t == ',' || t == '.' || t == '-'))
			return 0;
		if (c->ancla != c->cursor)
			borra_eleccion(c);
		if (largo(c->valor) + (u32)k + 1 >= sizeof c->valor)
			return 0;
		b = byte_de(c->valor, c->cursor);
		memmove(c->valor + b + k, c->valor + b, largo(c->valor + b) + 1);
		memcpy(c->valor + b, e, (size_t)k);
		c->cursor++;
		c->ancla = c->cursor;
		return 1;
	}
	return 0;
}

static void cierra(ui_cuadro *q, int id)
{
	q->cerrado = id;
}

int ui_cuadro_suceso(ui_cuadro *q, int v, const so_suceso *s)
{
	int th = lz_ui_alto() + 10, i;
	ui_control *f = q->foco >= 0 && q->foco < q->n ? &q->c[q->foco] : NULL;

	if (s->clase == SO_MENU && s->etiqueta == UI_ETIQUETA_MENU) {
		if (q->menu_de >= 0 && q->menu_de < q->n && s->opcion >= 0)
			q->c[q->menu_de].elegida = s->opcion;
		q->menu_de = -1;
		return 1;
	}
	if (s->clase == SO_TECLA) {
		u32 t = s->tecla;

		if (t == SO_ESC) {
			cierra(q, -1);
			return 1;
		}
		if (t == SO_TAB || t == SO_T_MAYUS_TAB) {
			pasa_foco(q, t == SO_T_MAYUS_TAB || (s->mods & SO_CON_MAYUS));
			return 1;
		}
		if (t == SO_INTRO) {
			if (f && f->tipo == UC_BOTON && !f->apagado) {
				cierra(q, f->id);
				return 1;
			}
			for (i = 0; i < q->n; i++)
				if (q->c[i].tipo == UC_BOTON && q->c[i].principal && !q->c[i].apagado) {
					cierra(q, q->c[i].id);
					return 1;
				}
			return 0;
		}
		if (!f)
			return 0;
		switch (f->tipo) {
		case UC_CAMPO:
			return tecla_campo(f, s);
		case UC_NUMERO:
			if (t == SO_T_ARRIBA || t == SO_T_ABAJO) {
				ui_pon_numero(f, ui_numero(f) + (t == SO_T_ARRIBA ? f->paso : -f->paso));
				return 1;
			}
			return tecla_campo(f, s);
		case UC_CASILLA:
			if (t == ' ') {
				f->marcado = !f->marcado;
				return 1;
			}
			break;
		case UC_OPCION:
			if (t == ' ') {
				marca_opcion(q, f);
				return 1;
			}
			if (t == SO_T_ABAJO || t == SO_T_ARRIBA || t == SO_T_DERECHA || t == SO_T_IZQUIERDA) {
				/* la siguiente del grupo */
				int k = q->foco, j;

				for (j = 0; j < q->n; j++) {
					k = (t == SO_T_ABAJO || t == SO_T_DERECHA) ? (k + 1) % q->n : (k + q->n - 1) % q->n;
					if (q->c[k].tipo == UC_OPCION && q->c[k].grupo == f->grupo && !q->c[k].apagado) {
						q->foco = k;
						marca_opcion(q, &q->c[k]);
						break;
					}
				}
				return 1;
			}
			break;
		case UC_BOTON:
			if (t == ' ') {
				cierra(q, f->id);
				return 1;
			}
			break;
		case UC_LISTA:
		case UC_DESPLEGABLE: {
			int n = n_opciones(f->opciones);

			if (t == SO_T_ABAJO && f->elegida + 1 < n)
				f->elegida++;
			else if (t == SO_T_ARRIBA && f->elegida > 0)
				f->elegida--;
			else if (t == SO_T_INICIO)
				f->elegida = 0;
			else if (t == SO_T_FIN)
				f->elegida = n - 1;
			else if (t == SO_T_AV_PAG)
				f->elegida = f->elegida + 8 < n ? f->elegida + 8 : n - 1;
			else if (t == SO_T_RE_PAG)
				f->elegida = f->elegida > 8 ? f->elegida - 8 : 0;
			else if (t == ' ' && f->tipo == UC_DESPLEGABLE) {
				q->menu_de = q->foco;
				so_menu(v, q->x + f->x, q->y + th + f->y + ui_alto_control(), f->opciones, 0, UI_ETIQUETA_MENU);
				return 0;
			} else if (t >= 0x20 && t < SO_T_BASE) {
				/* la primera que empieza por esa letra (detrás de la elegida) */
				int k;

				for (k = 1; k <= n; k++) {
					char o[200];
					u32 c;
					int j = (f->elegida + k) % n;

					opcion(f->opciones, j, o, sizeof o);
					utf8_lee((u8 *)o, largo(o), &c);
					if (minuscula(sin_tilde(c)) == minuscula(sin_tilde(t))) {
						f->elegida = j;
						break;
					}
				}
			} else
				return 0;
			return 1;
		}
		}
		return 0;
	}
	if (s->clase == SO_BAJA || s->clase == SO_SUBE || s->clase == SO_MUEVE || s->clase == SO_RUEDA) {
		int x = s->x - q->x, y = s->y - q->y - th;

		for (i = 0; i < q->n; i++) {
			ui_control *c = &q->c[i];
			int h = c->al ? c->al : ui_alto_control(), dentro = x >= c->x && x < c->x + c->an && y >= c->y && y < c->y + h;

			if (s->clase == SO_RUEDA) {
				if (dentro && c->tipo == UC_LISTA) {
					int n = n_opciones(c->opciones), vis = (c->al - 4) / (lz_ui_alto() + 2);

					c->primera -= s->pasos * 3;
					if (c->primera > n - vis)
						c->primera = n - vis;
					if (c->primera < 0)
						c->primera = 0;
					/* (la elegida, dentro de lo que se ve) */
					if (c->elegida < c->primera)
						c->elegida = c->primera;
					if (c->elegida >= c->primera + vis)
						c->elegida = c->primera + vis - 1;
					return 1;
				}
				continue;
			}
			if (s->clase == SO_MUEVE) {
				if (q->arrastra == i && (c->tipo == UC_CAMPO || c->tipo == UC_NUMERO) && (s->botones & SO_BOTON_IZQ)) {
					/* elegir con el ratón */
					int k, n = (int)utf8_cuenta(c->valor, largo(c->valor)), mejor = 0;
					char pre[260];

					for (k = 0; k <= n; k++) {
						copia(pre, c->valor, byte_de(c->valor, k) + 1);
						if (lz_ui_ancho(pre) + 6 <= x - c->x)
							mejor = k;
					}
					c->cursor = mejor;
					return 1;
				}
				continue;
			}
			if (s->clase == SO_SUBE) {
				q->arrastra = -1;
				if (q->pulsado == i) {
					q->pulsado = -1;
					if (dentro && c->tipo == UC_BOTON && !c->apagado)
						cierra(q, c->id);
					return 1;
				}
				continue;
			}
			/* bajar */
			if (!dentro || c->apagado || !recibe_foco(c))
				continue;
			q->foco = i;
			switch (c->tipo) {
			case UC_BOTON:
				q->pulsado = i;
				break;
			case UC_CASILLA:
				c->marcado = !c->marcado;
				break;
			case UC_OPCION:
				marca_opcion(q, c);
				break;
			case UC_CAMPO:
			case UC_NUMERO: {
				int an = c->tipo == UC_NUMERO ? c->an - 18 : c->an;

				if (c->tipo == UC_NUMERO && x >= c->x + an) {
					ui_pon_numero(c, ui_numero(c) + (y - c->y < h / 2 ? c->paso : -c->paso));
					break;
				}
				{
					int k, n = (int)utf8_cuenta(c->valor, largo(c->valor)), mejor = 0;
					char pre[260];

					for (k = 0; k <= n; k++) {
						copia(pre, c->valor, byte_de(c->valor, k) + 1);
						if (lz_ui_ancho(pre) + 6 <= x - c->x + 3)
							mejor = k;
					}
					c->cursor = c->ancla = mejor;
					if (s->veces >= 2) {
						c->ancla = 0;
						c->cursor = n;
					}
					q->arrastra = i;
				}
				break;
			}
			case UC_LISTA: {
				int fila = lz_ui_alto() + 2, k = c->primera + (y - c->y - 2) / fila;

				if (k >= 0 && k < n_opciones(c->opciones))
					c->elegida = k;
				/* dos clics: como Intro */
				if (s->veces >= 2)
					for (k = 0; k < q->n; k++)
						if (q->c[k].tipo == UC_BOTON && q->c[k].principal && !q->c[k].apagado)
							cierra(q, q->c[k].id);
				break;
			}
			case UC_DESPLEGABLE:
				q->menu_de = i;
				so_menu(v, q->x + c->x, q->y + th + c->y + h, c->opciones, 0, UI_ETIQUETA_MENU);
				break;
			case UC_COLOR:
				q->menu_de = -1;
				break;
			}
			return 1;
		}
		return 0;
	}
	return 0;
}
