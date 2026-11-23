/* dibujo.c - Pintura 2: lo que pinta en la lámina (dibujo.h). El polígono
 * suavizado es el de «acumular el área con signo» (como font-rs o
 * stb_truetype 2): cada lado suma a cada punto el área que deja a su
 * derecha, y una suma de izquierda a derecha da la cobertura exacta; se
 * hace a franjas de 32 filas, para no pedir memoria de más.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <math.h>
#include "dibujo.h"
#include "historia.h"
#include "letras.h"
#include "texto.h"

#define FRANJA 32

static float absf(float x)
{
	return x < 0 ? -x : x;
}

static int suelo(float x)
{
	int i = (int)x;

	return (float)i > x ? i - 1 : i;
}

static int techo(float x)
{
	int i = (int)x;

	return (float)i < x ? i + 1 : i;
}

void zona_suma(int zona[4], int x0, int y0, int x1, int y1)
{
	if (!zona || x1 <= x0 || y1 <= y0)
		return;
	if (zona[2] <= zona[0] || zona[3] <= zona[1]) {
		zona[0] = x0;
		zona[1] = y0;
		zona[2] = x1;
		zona[3] = y1;
		return;
	}
	if (x0 < zona[0])
		zona[0] = x0;
	if (y0 < zona[1])
		zona[1] = y0;
	if (x1 > zona[2])
		zona[2] = x1;
	if (y1 > zona[3])
		zona[3] = y1;
}

/* ===================================================================== */
/*  Las máscaras                                                         */
/* ===================================================================== */
int masc_nueva(mascara *m, const lamina *l, int x0, int y0, int x1, int y1)
{
	memset(m, 0, sizeof *m);
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > l->an)
		x1 = l->an;
	if (y1 > l->al)
		y1 = l->al;
	if (x1 <= x0 || y1 <= y0)
		return 0;
	m->a = so_pide((size_t)(x1 - x0) * (size_t)(y1 - y0));
	if (!m->a)
		return 0;
	memset(m->a, 0, (size_t)(x1 - x0) * (size_t)(y1 - y0));
	m->x0 = x0;
	m->y0 = y0;
	m->an = x1 - x0;
	m->al = y1 - y0;
	return 1;
}

void masc_suelta(mascara *m)
{
	so_suelta(m->a);
	m->a = NULL;
}

/* un lado del polígono (en la franja: x desde 0, y desde 0 hasta h) */
static void acumula(float *a, int w, int h, float x0, float y0, float x1, float y1)
{
	float dir, dxdy, x;
	int y, ys, ye, ancho = w + 3;
	float lim = (float)w + 1;

	if (y0 == y1)
		return;
	if (y0 < y1)
		dir = 1;
	else {
		float t;

		dir = -1;
		t = x0, x0 = x1, x1 = t;
		t = y0, y0 = y1, y1 = t;
	}
	if (y1 <= 0 || y0 >= (float)h)
		return;
	dxdy = (x1 - x0) / (y1 - y0);
	x = x0;
	if (y0 < 0) {
		x -= y0 * dxdy;
		ys = 0;
	} else
		ys = suelo(y0);
	ye = y1 > (float)h ? h : techo(y1);
	for (y = ys; y < ye; y++) {
		float lo = (float)y > y0 ? (float)y : y0, hi = (float)(y + 1) < y1 ? (float)(y + 1) : y1;
		float dy = hi - lo, xn = x + dxdy * dy, d = dy * dir;
		float xa = x < xn ? x : xn, xb = x < xn ? xn : x;
		float *f = a + y * ancho;
		int x0i, x1i;

		x = xn;
		if (dy <= 0)
			continue;
		xa = xa < 0 ? 0 : xa > lim ? lim : xa;
		xb = xb < 0 ? 0 : xb > lim ? lim : xb;
		x0i = suelo(xa);
		x1i = techo(xb);
		if (x1i <= x0i + 1) {
			float xmf = 0.5f * (xa + xb) - (float)x0i;

			f[x0i] += d - d * xmf;
			f[x0i + 1] += d * xmf;
		} else {
			float s = 1.0f / (xb - xa), x0f = xa - (float)x0i, a0 = 0.5f * s * (1 - x0f) * (1 - x0f);
			float x1f = xb - (float)x1i + 1, am = 0.5f * s * x1f * x1f;
			int k;

			f[x0i] += d * a0;
			if (x1i == x0i + 2)
				f[x0i + 1] += d * (1 - a0 - am);
			else {
				float a1 = s * (1.5f - x0f), a2;

				f[x0i + 1] += d * (a1 - a0);
				for (k = x0i + 2; k < x1i - 1; k++)
					f[k] += d * s;
				a2 = a1 + (float)(x1i - x0i - 3) * s;
				f[x1i - 1] += d * (1 - a2 - am);
			}
			f[x1i] += d * am;
		}
	}
}

void masc_poligono(mascara *m, const float *xy, const int *cuantos, int contornos, int aa)
{
	int w = m->an, ancho = w + 3, fy, c, i;
	float *a;

	if (!m->a)
		return;
	a = so_pide((size_t)ancho * FRANJA * sizeof(float));
	if (!a)
		return;
	for (fy = 0; fy < m->al; fy += FRANJA) {
		int h = m->al - fy < FRANJA ? m->al - fy : FRANJA, k = 0, r;
		float oy = (float)(m->y0 + fy), ox = (float)m->x0;

		memset(a, 0, (size_t)ancho * (size_t)h * sizeof(float));
		for (c = 0; c < contornos; c++) {
			int n = cuantos[c];

			for (i = 0; i < n; i++) {
				const float *p = xy + 2 * (k + i), *q = xy + 2 * (k + (i + 1) % n);

				acumula(a, w, h, p[0] - ox, p[1] - oy, q[0] - ox, q[1] - oy);
			}
			k += n;
		}
		for (r = 0; r < h; r++) {
			float s = 0;
			const float *f = a + r * ancho;
			u8 *d = m->a + (size_t)(fy + r) * (size_t)w;
			int x;

			for (x = 0; x < w; x++) {
				int v;

				s += f[x];
				if (aa) {
					float t = absf(s);

					v = t >= 1 ? 255 : (int)(t * 255 + 0.5f);
				} else
					v = absf(s) >= 0.5f ? 255 : 0;
				if (v > d[x])
					d[x] = (u8)v;
			}
		}
	}
	so_suelta(a);
}

/* la cobertura del punto de centro (px, py) por la raya de radio r (con
 * las puntas redondas) */
static int cobertura_raya(float px, float py, float x0, float y0, float dx, float dy, float l2, float r, int aa)
{
	float t = l2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0, qx, qy, d2, ri, ro;

	if (t < 0)
		t = 0;
	else if (t > 1)
		t = 1;
	qx = px - (x0 + t * dx);
	qy = py - (y0 + t * dy);
	d2 = qx * qx + qy * qy;
	if (!aa)
		return d2 <= r * r + 0.0001f ? 255 : 0;
	ro = r + 0.5f;
	if (d2 >= ro * ro)
		return 0;
	ri = r - 0.5f;
	if (ri > 0 && d2 <= ri * ri)
		return 255;
	{
		float d = (float)so_raiz(d2), v = ro - d;

		return v >= 1 ? 255 : (int)(v * 255 + 0.5f);
	}
}

void masc_raya(mascara *m, float x0, float y0, float x1, float y1, float g, int aa)
{
	float r = g < 1 ? 0.5f : g / 2, dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
	int bx0 = suelo((x0 < x1 ? x0 : x1) - r - 1), by0 = suelo((y0 < y1 ? y0 : y1) - r - 1);
	int bx1 = techo((x0 > x1 ? x0 : x1) + r + 1), by1 = techo((y0 > y1 ? y0 : y1) + r + 1);
	int x, y;

	if (!m->a)
		return;
	if (bx0 < m->x0)
		bx0 = m->x0;
	if (by0 < m->y0)
		by0 = m->y0;
	if (bx1 > m->x0 + m->an)
		bx1 = m->x0 + m->an;
	if (by1 > m->y0 + m->al)
		by1 = m->y0 + m->al;
	for (y = by0; y < by1; y++) {
		u8 *d = m->a + (size_t)(y - m->y0) * (size_t)m->an - m->x0;

		for (x = bx0; x < bx1; x++) {
			int v = cobertura_raya((float)x + 0.5f, (float)y + 0.5f, x0, y0, dx, dy, l2, r, aa);

			if (v > d[x])
				d[x] = (u8)v;
		}
	}
}

/* (lo de la tinta con su cobertura, sobre el punto de antes) */
static u32 entinta(u32 antes, u32 color, int modo, int a)
{
	switch (modo) {
	case TINTA_BORRA:
		return lam_borra(antes, a);
	case TINTA_PONE:
		if (a >= 255)
			return color;
		{
			u32 r = 0;
			int k;

			for (k = 0; k < 32; k += 8) {
				int x = (int)((antes >> k) & 255), y = (int)((color >> k) & 255);

				r |= (u32)(x + ((y - x) * a + (y > x ? 127 : -127)) / 255) << k;
			}
			return r;
		}
	}
	return lam_sobre(antes, color, a);
}

void masc_aplica(lamina *l, const mascara *m, u32 color, int modo, int zona[4])
{
	int x, y;

	if (!m->a)
		return;
	his_toca(m->x0, m->y0, m->x0 + m->an, m->y0 + m->al);
	for (y = 0; y < m->al; y++) {
		const u8 *a = m->a + (size_t)y * (size_t)m->an;
		u32 *p = l->p + (size_t)(m->y0 + y) * (size_t)l->an + m->x0;

		for (x = 0; x < m->an; x++)
			if (a[x])
				p[x] = entinta(p[x], color, modo, a[x]);
	}
	zona_suma(zona, m->x0, m->y0, m->x0 + m->an, m->y0 + m->al);
}

/* ===================================================================== */
/*  Las figuras                                                          */
/* ===================================================================== */
static void ordena(int *a, int *b)
{
	if (*a > *b) {
		int t = *a;

		*a = *b;
		*b = t;
	}
}

/* los puntos de una caja redondeada (o una elipse con redondo < 0) de
 * [x0, x1] x [y0, y1]; al revés con «reves». Da cuántos (en p, hasta max) */
static int contorno_caja(float *p, int max, float x0, float y0, float x1, float y1, float redondo, int reves)
{
	int n = 0, i, k;
	float w = x1 - x0, h = y1 - y0;

	if (w <= 0 || h <= 0)
		return 0;
	if (redondo < 0) {
		/* la elipse */
		float a = w / 2, b = h / 2, cx = x0 + a, cy = y0 + b, per = 3.2f * (a + b);
		int m = (int)(per / 1.5f);

		if (m < 24)
			m = 24;
		if (m > max / 2)
			m = max / 2;
		for (i = 0; i < m; i++) {
			int j = reves ? m - 1 - i : i;
			double t = 6.283185307179586 * j / m;

			p[2 * n] = cx + a * (float)cos(t);
			p[2 * n + 1] = cy + b * (float)cos(t - 1.5707963267948966);
			n++;
		}
		return n;
	}
	if (redondo > w / 2)
		redondo = w / 2;
	if (redondo > h / 2)
		redondo = h / 2;
	if (redondo < 0.5f) {
		float q[8] = { x0, y0, x1, y0, x1, y1, x0, y1 };

		for (i = 0; i < 4; i++) {
			int j = reves ? 3 - i : i;

			p[2 * i] = q[2 * j];
			p[2 * i + 1] = q[2 * j + 1];
		}
		return 4;
	}
	{
		/* las cuatro esquinas, de arriba a la derecha... */
		float cx[4] = { x1 - redondo, x1 - redondo, x0 + redondo, x0 + redondo };
		float cy[4] = { y0 + redondo, y1 - redondo, y1 - redondo, y0 + redondo };
		int m = (int)(redondo * 1.2f) + 4, tot;

		if (m > 64)
			m = 64;
		if (m * 4 > max / 2)
			m = max / 8;
		tot = 4 * (m + 1);
		for (i = 0; i < tot; i++) {
			int j = reves ? tot - 1 - i : i, e = j / (m + 1), s = j % (m + 1);
			double t = -1.5707963267948966 + 1.5707963267948966 * (e + (double)s / m);

			k = e;
			p[2 * n] = cx[k] + redondo * (float)cos(t);
			p[2 * n + 1] = cy[k] + redondo * (float)cos(t - 1.5707963267948966);
			n++;
		}
		return n;
	}
}

#define MAXP 4200

static void figura_caja(lamina *l, int x0, int y0, int x1, int y1, float redondo, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4])
{
	static float p[2 * MAXP];
	int cu[2];
	float fx0, fy0, fx1, fy1, ri;
	mascara m;

	ordena(&x0, &x1);
	ordena(&y0, &y1);
	fx0 = (float)x0;
	fy0 = (float)y0;
	fx1 = (float)x1 + 1;
	fy1 = (float)y1 + 1;
	if (g < 1)
		g = 1;
	if (relleno != RELLENO_NO) {
		float e = relleno == RELLENO_Y_CONTORNO ? g / 2 : 0;

		if (masc_nueva(&m, l, x0, y0, x1 + 1, y1 + 1)) {
			cu[0] = contorno_caja(p, MAXP, fx0 + e, fy0 + e, fx1 - e, fy1 - e, redondo < 0 ? -1 : (redondo - e > 0 ? redondo - e : 0), 0);
			if (cu[0] > 2) {
				masc_poligono(&m, p, cu, 1, aa);
				masc_aplica(l, &m, relleno == RELLENO_SOLO ? borde : dentro, TINTA_SOBRE, zona);
			}
			masc_suelta(&m);
		}
		if (relleno == RELLENO_SOLO)
			return;
	}
	if (!masc_nueva(&m, l, x0, y0, x1 + 1, y1 + 1))
		return;
	cu[0] = contorno_caja(p, MAXP, fx0, fy0, fx1, fy1, redondo, 0);
	ri = redondo < 0 ? -1 : (redondo - g > 0 ? redondo - g : 0);
	cu[1] = (fx1 - fx0 > 2 * g && fy1 - fy0 > 2 * g) ? contorno_caja(p + 2 * cu[0], MAXP - cu[0], fx0 + g, fy0 + g, fx1 - g, fy1 - g, ri, 1) : 0;
	if (cu[0] > 2) {
		masc_poligono(&m, p, cu, cu[1] > 2 ? 2 : 1, aa);
		masc_aplica(l, &m, borde, TINTA_SOBRE, zona);
	}
	masc_suelta(&m);
}

void fig_rectangulo(lamina *l, int x0, int y0, int x1, int y1, int redondo, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4])
{
	float r = 0;

	if (redondo) {
		int w = x1 > x0 ? x1 - x0 + 1 : x0 - x1 + 1, h = y1 > y0 ? y1 - y0 + 1 : y0 - y1 + 1, mn = w < h ? w : h;

		r = (float)mn / 5;
		if (r < 3)
			r = 3;
		if (r < g * 1.5f)
			r = g * 1.5f;
		if (r > 48)
			r = 48;
	}
	figura_caja(l, x0, y0, x1, y1, r, g, relleno, borde, dentro, aa, zona);
}

void fig_elipse(lamina *l, int x0, int y0, int x1, int y1, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4])
{
	figura_caja(l, x0, y0, x1, y1, -1, g, relleno, borde, dentro, aa, zona);
}

void fig_raya(lamina *l, int x0, int y0, int x1, int y1, float g, u32 c, int aa, int zona[4])
{
	mascara m;
	int r = (int)(g / 2) + 2;

	if (!masc_nueva(&m, l, (x0 < x1 ? x0 : x1) - r, (y0 < y1 ? y0 : y1) - r, (x0 > x1 ? x0 : x1) + r + 1, (y0 > y1 ? y0 : y1) + r + 1))
		return;
	masc_raya(&m, (float)x0 + 0.5f, (float)y0 + 0.5f, (float)x1 + 0.5f, (float)y1 + 0.5f, g, aa);
	masc_aplica(l, &m, c, TINTA_SOBRE, zona);
	masc_suelta(&m);
}

void fig_curva(lamina *l, const int *pt, float g, u32 c, int aa, int zona[4])
{
	mascara m;
	int i, n, r = (int)(g / 2) + 2, mx0 = pt[0], my0 = pt[1], mx1 = pt[0], my1 = pt[1];
	float px, py, lar = 0;

	for (i = 1; i < 4; i++) {
		if (pt[2 * i] < mx0)
			mx0 = pt[2 * i];
		if (pt[2 * i] > mx1)
			mx1 = pt[2 * i];
		if (pt[2 * i + 1] < my0)
			my0 = pt[2 * i + 1];
		if (pt[2 * i + 1] > my1)
			my1 = pt[2 * i + 1];
		lar += absf((float)(pt[2 * i] - pt[2 * i - 2])) + absf((float)(pt[2 * i + 1] - pt[2 * i - 1]));
	}
	if (!masc_nueva(&m, l, mx0 - r, my0 - r, mx1 + r + 1, my1 + r + 1))
		return;
	n = (int)(lar / 4);
	if (n < 8)
		n = 8;
	if (n > 400)
		n = 400;
	px = (float)pt[0] + 0.5f;
	py = (float)pt[1] + 0.5f;
	for (i = 1; i <= n; i++) {
		float t = (float)i / n, u = 1 - t;
		float b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
		float x = b0 * pt[0] + b1 * pt[2] + b2 * pt[4] + b3 * pt[6] + 0.5f;
		float y = b0 * pt[1] + b1 * pt[3] + b2 * pt[5] + b3 * pt[7] + 0.5f;

		masc_raya(&m, px, py, x, y, g, aa);
		px = x;
		py = y;
	}
	masc_aplica(l, &m, c, TINTA_SOBRE, zona);
	masc_suelta(&m);
}

void fig_poligono(lamina *l, const int *pt, int n, int cerrado, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4])
{
	mascara m;
	int i, r = (int)(g / 2) + 2, mx0, my0, mx1, my1;

	if (n < 1)
		return;
	mx0 = mx1 = pt[0];
	my0 = my1 = pt[1];
	for (i = 1; i < n; i++) {
		if (pt[2 * i] < mx0)
			mx0 = pt[2 * i];
		if (pt[2 * i] > mx1)
			mx1 = pt[2 * i];
		if (pt[2 * i + 1] < my0)
			my0 = pt[2 * i + 1];
		if (pt[2 * i + 1] > my1)
			my1 = pt[2 * i + 1];
	}
	if (cerrado && n >= 3 && relleno != RELLENO_NO) {
		float *f = so_pide((size_t)n * 2 * sizeof(float));
		int cu = n;

		if (f && masc_nueva(&m, l, mx0, my0, mx1 + 1, my1 + 1)) {
			for (i = 0; i < 2 * n; i++)
				f[i] = (float)pt[i] + 0.5f;
			masc_poligono(&m, f, &cu, 1, aa);
			masc_aplica(l, &m, relleno == RELLENO_SOLO ? borde : dentro, TINTA_SOBRE, zona);
			masc_suelta(&m);
		}
		so_suelta(f);
		if (relleno == RELLENO_SOLO)
			return;
	}
	if (!masc_nueva(&m, l, mx0 - r, my0 - r, mx1 + r + 1, my1 + r + 1))
		return;
	for (i = 0; i + 1 < n + (cerrado && n >= 3 ? 1 : 0); i++) {
		const int *p = pt + 2 * i, *q = pt + 2 * ((i + 1) % n);

		masc_raya(&m, (float)p[0] + 0.5f, (float)p[1] + 0.5f, (float)q[0] + 0.5f, (float)q[1] + 0.5f, g, aa);
	}
	if (n == 1)
		masc_raya(&m, (float)pt[0] + 0.5f, (float)pt[1] + 0.5f, (float)pt[0] + 0.5f, (float)pt[1] + 0.5f, g, aa);
	masc_aplica(l, &m, borde, TINTA_SOBRE, zona);
	masc_suelta(&m);
}

/* ===================================================================== */
/*  Las pinceladas                                                       */
/* ===================================================================== */
/* el punto (x, y) con la cobertura a, en la pincelada (lo más tapado gana) */
static void punto_trazo(lamina *l, int x, int y, int a, u32 color, int modo)
{
	u8 *c = his_cob(x, y);
	u32 *p = l->p + (size_t)y * (size_t)l->an + x;

	if (!c) {
		*p = entinta(*p, color, modo, a);
		return;
	}
	if (a <= *c)
		return;
	*c = (u8)a;
	*p = entinta(his_viejo(x, y), color, modo, a);
}

/* la envolvente convexa (de n <= 16 puntos), en el sentido de las agujas */
static int envolvente(float *p, int n, float *d)
{
	int i, j, k = 0;
	float t[32];

	/* (ordenar por x, y; con n pequeño, a mano) */
	memcpy(t, p, (size_t)n * 2 * sizeof(float));
	for (i = 1; i < n; i++)
		for (j = i; j > 0 && (t[2 * j - 2] > t[2 * j] || (t[2 * j - 2] == t[2 * j] && t[2 * j - 1] > t[2 * j + 1])); j--) {
			float a = t[2 * j], b = t[2 * j + 1];

			t[2 * j] = t[2 * j - 2];
			t[2 * j + 1] = t[2 * j - 1];
			t[2 * j - 2] = a;
			t[2 * j - 1] = b;
		}
#define GIRO(o, a, b) ((d[2 * (a)] - d[2 * (o)]) * (t[2 * (b) + 1] - d[2 * (o) + 1]) - (d[2 * (a) + 1] - d[2 * (o) + 1]) * (t[2 * (b)] - d[2 * (o)]))
	for (i = 0; i < n; i++) {
		while (k >= 2 && GIRO(k - 2, k - 1, i) <= 0)
			k--;
		d[2 * k] = t[2 * i];
		d[2 * k + 1] = t[2 * i + 1];
		k++;
	}
	for (i = n - 2, j = k + 1; i >= 0; i--) {
		while (k >= j && GIRO(k - 2, k - 1, i) <= 0)
			k--;
		d[2 * k] = t[2 * i];
		d[2 * k + 1] = t[2 * i + 1];
		k++;
	}
#undef GIRO
	return k > 1 ? k - 1 : k;
}

void trazo(lamina *l, const pincel *p, float x0, float y0, float x1, float y1, int zona[4])
{
	float r = p->grosor < 1 ? 0.5f : p->grosor / 2;
	int bx0 = suelo((x0 < x1 ? x0 : x1) - r - 1), by0 = suelo((y0 < y1 ? y0 : y1) - r - 1);
	int bx1 = techo((x0 > x1 ? x0 : x1) + r + 1), by1 = techo((y0 > y1 ? y0 : y1) + r + 1);
	int x, y;

	if (bx0 < 0)
		bx0 = 0;
	if (by0 < 0)
		by0 = 0;
	if (bx1 > l->an)
		bx1 = l->an;
	if (by1 > l->al)
		by1 = l->al;
	if (bx1 <= bx0 || by1 <= by0)
		return;
	his_toca(bx0, by0, bx1, by1);
	if (p->forma == FORMA_REDONDA) {
		float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;

		for (y = by0; y < by1; y++)
			for (x = bx0; x < bx1; x++) {
				int v = cobertura_raya((float)x + 0.5f, (float)y + 0.5f, x0, y0, dx, dy, l2, r, p->aa);

				if (v)
					punto_trazo(l, x, y, v, p->color, p->modo);
			}
	} else {
		/* la forma, de un extremo al otro: su envolvente */
		float f[16], e[34], q = r;
		int n = 0, k, i, cu;
		mascara m;
		static const float cuadrada[8] = { -1, -1, 1, -1, 1, 1, -1, 1 };

		if (p->forma == FORMA_CUADRADA)
			for (i = 0; i < 4; i++) {
				f[2 * n] = cuadrada[2 * i] * q;
				f[2 * n + 1] = cuadrada[2 * i + 1] * q;
				n++;
			}
		else {
			/* una barra fina inclinada («/» o «\») */
			float s = p->forma == FORMA_BARRA ? -1 : 1, w = 0.6f;

			f[0] = -q - w;
			f[1] = s * q;
			f[2] = -q + w;
			f[3] = s * q;
			f[4] = q + w;
			f[5] = -s * q;
			f[6] = q - w;
			f[7] = -s * q;
			n = 4;
		}
		{
			float t[32];

			for (i = 0; i < n; i++) {
				t[2 * i] = x0 + f[2 * i];
				t[2 * i + 1] = y0 + f[2 * i + 1];
				t[2 * (n + i)] = x1 + f[2 * i];
				t[2 * (n + i) + 1] = y1 + f[2 * i + 1];
			}
			k = envolvente(t, 2 * n, e);
		}
		if (k < 3 || !masc_nueva(&m, l, bx0, by0, bx1, by1))
			return;
		cu = k;
		masc_poligono(&m, e, &cu, 1, p->aa);
		for (y = 0; y < m.al; y++)
			for (x = 0; x < m.an; x++) {
				int v = m.a[(size_t)y * (size_t)m.an + x];

				if (v)
					punto_trazo(l, m.x0 + x, m.y0 + y, v, p->color, p->modo);
			}
		masc_suelta(&m);
	}
	zona_suma(zona, bx0, by0, bx1, by1);
}

void trazo_lapiz(lamina *l, u32 color, int x0, int y0, int x1, int y1, int zona[4])
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
	int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, e = dx + dy;

	his_toca(x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, (x0 > x1 ? x0 : x1) + 1, (y0 > y1 ? y0 : y1) + 1);
	for (;;) {
		int e2;

		if (x0 >= 0 && y0 >= 0 && x0 < l->an && y0 < l->al) {
			punto_trazo(l, x0, y0, 255, color, TINTA_PONE);
			zona_suma(zona, x0, y0, x0 + 1, y0 + 1);
		}
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

static u32 azar(u32 *s)
{
	*s = *s * 1103515245u + 12345u;
	return (*s >> 16) & 0x7FFF;
}

void trazo_aerografo(lamina *l, u32 color, int x, int y, int r, int n, u32 *semilla, int zona[4])
{
	int i;

	if (r < 1)
		r = 1;
	his_toca(x - r, y - r, x + r + 1, y + r + 1);
	for (i = 0; i < n; i++) {
		int dx, dy, k = 0;

		do {
			dx = (int)(azar(semilla) % (u32)(2 * r + 1)) - r;
			dy = (int)(azar(semilla) % (u32)(2 * r + 1)) - r;
		} while (dx * dx + dy * dy > r * r && ++k < 8);
		if (x + dx < 0 || y + dy < 0 || x + dx >= l->an || y + dy >= l->al)
			continue;
		punto_trazo(l, x + dx, y + dy, 255, color, TINTA_SOBRE);
	}
	zona_suma(zona, x - r < 0 ? 0 : x - r, y - r < 0 ? 0 : y - r, x + r + 1 > l->an ? l->an : x + r + 1, y + r + 1 > l->al ? l->al : y + r + 1);
}

/* ===================================================================== */
/*  El relleno                                                           */
/* ===================================================================== */
static int parecido(u32 a, u32 b, int tol)
{
	int k;

	if (ALFA(a) == 0 && ALFA(b) == 0)
		return 1;
	for (k = 0; k < 32; k += 8) {
		int d = (int)((a >> k) & 255) - (int)((b >> k) & 255);

		if (d > tol || d < -tol)
			return 0;
	}
	return 1;
}

u32 rellena(lamina *l, int x, int y, u32 color, int tolerancia, int zona[4])
{
	u32 n = (u32)l->an * (u32)l->al, *pila, cabe = 4096, np = 0, hechos = 0;
	u8 *vis;
	u32 semilla;
	int tol = tolerancia * 255 / 100, mx0 = x, my0 = y, mx1 = x, my1 = y, yy;

	if (x < 0 || y < 0 || x >= l->an || y >= l->al)
		return 0;
	semilla = l->p[(size_t)y * l->an + x];
	if (semilla == color && tol == 0)
		return 0;
	vis = so_pide(n / 8 + 1);
	pila = so_pide(cabe * 4);
	if (!vis || !pila) {
		so_suelta(vis);
		so_suelta(pila);
		return 0;
	}
	memset(vis, 0, n / 8 + 1);
#define VISTO(i) (vis[(i) >> 3] & (1 << ((i) & 7)))
#define MARCA(i) (vis[(i) >> 3] |= (u8)(1 << ((i) & 7)))
	pila[np++] = (u32)y * (u32)l->an + (u32)x;
	/* primero, lo que se rellena (sin tocar nada: para guardar su zona) */
	while (np) {
		u32 i = pila[--np];
		int px = (int)(i % (u32)l->an), py = (int)(i / (u32)l->an), a, b, k;

		if (VISTO(i))
			continue;
		a = px;
		while (a > 0 && !VISTO(i - (u32)(px - a + 1)) && parecido(l->p[i - (u32)(px - a + 1)], semilla, tol))
			a--;
		b = px;
		while (b + 1 < l->an && !VISTO(i + (u32)(b + 1 - px)) && parecido(l->p[i + (u32)(b + 1 - px)], semilla, tol))
			b++;
		if (!parecido(l->p[i], semilla, tol))
			continue;
		for (k = a; k <= b; k++)
			MARCA((u32)py * (u32)l->an + (u32)k);
		hechos += (u32)(b - a + 1);
		if (a < mx0)
			mx0 = a;
		if (b > mx1)
			mx1 = b;
		if (py < my0)
			my0 = py;
		if (py > my1)
			my1 = py;
		/* las filas de arriba y de abajo: un trozo por cada tramo */
		for (yy = py - 1; yy <= py + 1; yy += 2) {
			int dentro = 0;

			if (yy < 0 || yy >= l->al)
				continue;
			for (k = a; k <= b; k++) {
				u32 j = (u32)yy * (u32)l->an + (u32)k;
				int vale = !VISTO(j) && parecido(l->p[j], semilla, tol);

				if (vale && !dentro) {
					if (np >= cabe) {
						u32 *q = so_repide(pila, cabe * 8);

						if (!q)
							goto basta;
						pila = q;
						cabe *= 2;
					}
					pila[np++] = j;
				}
				dentro = vale;
			}
		}
	}
basta:
	so_suelta(pila);
	his_toca(mx0, my0, mx1 + 1, my1 + 1);
	for (yy = my0; yy <= my1; yy++) {
		int k;

		for (k = mx0; k <= mx1; k++) {
			u32 j = (u32)yy * (u32)l->an + (u32)k;

			if (VISTO(j))
				l->p[j] = color;
		}
	}
#undef VISTO
#undef MARCA
	so_suelta(vis);
	zona_suma(zona, mx0, my0, mx1 + 1, my1 + 1);
	return hechos;
}

/* ===================================================================== */
/*  El texto                                                             */
/* ===================================================================== */
static int letra_en(lamina *l, int cara, int tam4, int falsas, int x4, int y, u32 letra, u32 color)
{
	int real;
	u32 g = letra_glifo(cara, letra, &real);
	int adv = letra_avance(real, g), px = x4 >> 2, sub = ((x4 & 3) * 3) >> 2;

	if (l && letra > ' ') {
		const glifo_px *gp = letra_dibujo(real, g, tam4, sub, falsas);

		if (gp && gp->gris) {
			int f, c;

			for (f = 0; f < gp->al; f++) {
				int yy = y + gp->y0 + f;

				if (yy < 0 || yy >= l->al)
					continue;
				for (c = 0; c < gp->an; c++) {
					int xx = px + gp->x0 + c, a = gp->gris[f * gp->an + c];

					if (a && xx >= 0 && xx < l->an) {
						u32 *p = l->p + (size_t)yy * (size_t)l->an + xx;

						*p = lam_sobre(*p, color, a);
					}
				}
			}
		}
	}
	return (int)(((i64)adv * tam4 + letra_em(real) / 2) / letra_em(real)) + ((falsas & FALSA_NEGRITA) ? 2 : 0);
}

int texto_en_lamina(lamina *l, int cara, int tam, int falsas, int x, int y, const u32 *t, u32 n, u32 color)
{
	int x4 = x * 4;
	u32 i;

	for (i = 0; i < n; i++)
		x4 += letra_en(l, cara, tam * 4, falsas, x4, y, t[i], color);
	return (x4 + 3) / 4 - x;
}

int texto_ancho(int cara, int tam, int falsas, const u32 *t, u32 n)
{
	return texto_en_lamina(NULL, cara, tam, falsas, 0, 0, t, n, 0);
}

void texto_medidas(int cara, int tam, int *sube, int *baja, int *renglon)
{
	ttf *f = letra_ttf(cara);
	int em = f && f->em ? (int)f->em : 2048;
	int asc = f ? f->asc : em * 4 / 5, desc = f ? f->desc : -em / 5, salto = f ? f->salto : 0;

	*sube = (asc * tam + em - 1) / em;
	*baja = (-desc * tam + em - 1) / em;
	*renglon = ((asc - desc + salto) * tam + em / 2) / em;
	if (*renglon < *sube + *baja)
		*renglon = *sube + *baja;
}

void lamina_encima(lamina *d, const lamina *o, int x, int y, int transparente, u32 sin, int zona[4])
{
	int f, c, x0 = x < 0 ? -x : 0, y0 = y < 0 ? -y : 0;
	int x1 = x + o->an > d->an ? d->an - x : o->an, y1 = y + o->al > d->al ? d->al - y : o->al;

	if (x1 <= x0 || y1 <= y0)
		return;
	his_toca(x + x0, y + y0, x + x1, y + y1);
	for (f = y0; f < y1; f++) {
		const u32 *s = o->p + (size_t)f * (size_t)o->an;
		u32 *p = d->p + (size_t)(y + f) * (size_t)d->an + x;

		for (c = x0; c < x1; c++) {
			u32 v = s[c];

			if (transparente && (v & 0xFFFFFF) == (sin & 0xFFFFFF) && ALFA(v) == 255)
				continue;
			p[c] = lam_sobre(p[c], v, 255);
		}
	}
	zona_suma(zona, x + x0, y + y0, x + x1, y + y1);
}
