/* dibujo.c - (JUEGOS) dibujar en un lienzo de 32 bits en la memoria del
 * programa: rectángulos, degradados, esquinas redondas, líneas, círculos y
 * polígonos (estos dos, con el borde suavizado), copiar, mezclar y la letra
 * de 8x16 de Hispa Ventanas a cualquier escala entera. Todo se recorta a los
 * bordes del lienzo. Va igual en Hispa-Pentium y en el anfitrión (las
 * pruebas): no llama al sistema.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "juego.h"
#include "letra.h"

void lz_nuevo(lienzo *l, int an, int al)
{
	if (an < 1)
		an = 1;
	if (al < 1)
		al = 1;
	l->p = (u32 *)malloc((size_t)an * (size_t)al * 4);
	l->an = l->p ? an : 0;
	l->al = l->p ? al : 0;
	l->paso = l->an;
}

void lz_libre(lienzo *l)
{
	free(l->p);
	l->p = 0;
	l->an = l->al = l->paso = 0;
}

void lz_trozo(lienzo *t, const lienzo *de, int x, int y, int an, int al)
{
	if (x < 0) { an += x; x = 0; }
	if (y < 0) { al += y; y = 0; }
	if (x + an > de->an) an = de->an - x;
	if (y + al > de->al) al = de->al - y;
	if (an < 0) an = 0;
	if (al < 0) al = 0;
	t->p = de->p + (size_t)y * (size_t)de->paso + (size_t)x;
	t->an = an;
	t->al = al;
	t->paso = de->paso;
}

/* recorta [x, x+an) x [y, y+al) al lienzo: 0 si no queda nada */
static int recorta(const lienzo *l, int *x, int *y, int *an, int *al)
{
	if (*x < 0) { *an += *x; *x = 0; }
	if (*y < 0) { *al += *y; *y = 0; }
	if (*x + *an > l->an) *an = l->an - *x;
	if (*y + *al > l->al) *al = l->al - *y;
	return *an > 0 && *al > 0;
}

static inline void llena(u32 *d, u32 c, int n)
{
	size_t k = (size_t)n;	/* (del tamaño de un puntero: también en el anfitrión de 64 bits) */

	__asm__ volatile("rep stosl" : "+D"(d), "+c"(k) : "a"(c) : "memory");
}

void lz_rect(lienzo *l, int x, int y, int an, int al, u32 c)
{
	int f;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++)
		llena(l->p + (size_t)(y + f) * (size_t)l->paso + (size_t)x, c, an);
}

void lz_marco(lienzo *l, int x, int y, int an, int al, u32 c)
{
	lz_rect(l, x, y, an, 1, c);
	lz_rect(l, x, y + al - 1, an, 1, c);
	lz_rect(l, x, y, 1, al, c);
	lz_rect(l, x + an - 1, y, 1, al, c);
}

u32 mezcla(u32 a, u32 b, int alfa)
{
	u32 rb, g;

	if (alfa <= 0)
		return a;
	if (alfa >= 256)
		return b;
	rb = ((a & 0xFF00FF) * (u32)(256 - alfa) + (b & 0xFF00FF) * (u32)alfa) >> 8;
	g = ((a & 0x00FF00) * (u32)(256 - alfa) + (b & 0x00FF00) * (u32)alfa) >> 8;
	return (rb & 0xFF00FF) | (g & 0x00FF00);
}

u32 aclara(u32 c, int n)
{
	if (n >= 0)
		return mezcla(c, 0xFFFFFF, n);
	return mezcla(c, 0x000000, -n);
}

void lz_degradado(lienzo *l, int x, int y, int an, int al, u32 arriba, u32 abajo)
{
	int f, y0 = y, al0 = al;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++) {
		int k = al0 > 1 ? ((y + f - y0) * 256) / (al0 - 1) : 0;

		llena(l->p + (size_t)(y + f) * (size_t)l->paso + (size_t)x, mezcla(arriba, abajo, k), an);
	}
}

void lz_degradado_h(lienzo *l, int x, int y, int an, int al, u32 izq, u32 der)
{
	int f, c, x0 = x, an0 = an;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++) {
		u32 *p = l->p + (size_t)(y + f) * (size_t)l->paso + (size_t)x;

		for (c = 0; c < an; c++)
			p[c] = mezcla(izq, der, an0 > 1 ? ((x + c - x0) * 256) / (an0 - 1) : 0);
	}
}

void lz_mezcla(lienzo *l, int x, int y, int an, int al, u32 c, int alfa)
{
	int f, k;

	if (!recorta(l, &x, &y, &an, &al))
		return;
	for (f = 0; f < al; f++) {
		u32 *p = l->p + (size_t)(y + f) * (size_t)l->paso + (size_t)x;

		for (k = 0; k < an; k++)
			p[k] = mezcla(p[k], c, alfa);
	}
}

/* un punto, mezclado (alfa 0-256) */
static inline void punto(lienzo *l, int x, int y, u32 c, int alfa)
{
	u32 *p;

	if ((unsigned)x >= (unsigned)l->an || (unsigned)y >= (unsigned)l->al || alfa <= 0)
		return;
	p = l->p + (size_t)y * (size_t)l->paso + (size_t)x;
	*p = alfa >= 256 ? c : mezcla(*p, c, alfa);
}

void lz_redondo(lienzo *l, int x, int y, int an, int al, int r, u32 c)
{
	int f;

	if (r * 2 > an)
		r = an / 2;
	if (r * 2 > al)
		r = al / 2;
	if (r < 1) {
		lz_rect(l, x, y, an, al, c);
		return;
	}
	lz_rect(l, x, y + r, an, al - 2 * r, c);
	for (f = 0; f < r; f++) {
		/* lo que entra la fila f de la esquina (con un punto suavizado) */
		float dy = (float)r - (float)f - 0.5f;
		float dx = m_raiz((float)(r * r) - dy * dy);
		int e = r - (int)dx;
		int a = (int)((dx - (float)(int)dx) * 256.0f);

		lz_rect(l, x + e, y + f, an - 2 * e, 1, c);
		lz_rect(l, x + e, y + al - 1 - f, an - 2 * e, 1, c);
		if (e > 0) {
			punto(l, x + e - 1, y + f, c, a);
			punto(l, x + an - e, y + f, c, a);
			punto(l, x + e - 1, y + al - 1 - f, c, a);
			punto(l, x + an - e, y + al - 1 - f, c, a);
		}
	}
}

void lz_linea(lienzo *l, int x1, int y1, int x2, int y2, u32 c)
{
	int dx = abs(x2 - x1), dy = -abs(y2 - y1), sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, e = dx + dy, n = 0;

	for (;;) {
		punto(l, x1, y1, c, 256);
		if ((x1 == x2 && y1 == y2) || ++n > 8192)
			break;
		if (2 * e >= dy) {
			e += dy;
			x1 += sx;
		}
		if (2 * e <= dx) {
			e += dx;
			y1 += sy;
		}
	}
}

void lz_linea_gruesa(lienzo *l, int x1, int y1, int x2, int y2, int grueso, u32 c)
{
	float dx = (float)(x2 - x1), dy = (float)(y2 - y1), n = m_raiz(dx * dx + dy * dy), px, py, g = (float)grueso / 2;
	float q[8];

	if (n < 0.01f) {
		lz_circulo_suave(l, (float)x1, (float)y1, g, c);
		return;
	}
	px = -dy / n * g;
	py = dx / n * g;
	q[0] = x1 + px; q[1] = y1 + py;
	q[2] = x2 + px; q[3] = y2 + py;
	q[4] = x2 - px; q[5] = y2 - py;
	q[6] = x1 - px; q[7] = y1 - py;
	lz_poligono(l, q, 4, c);
}

void lz_circulo(lienzo *l, int cx, int cy, int r, u32 c)
{
	int f;

	for (f = -r; f <= r; f++) {
		int d = (int)m_raiz((float)(r * r - f * f) + 0.25f);

		lz_rect(l, cx - d, cy + f, 2 * d + 1, 1, c);
	}
}

void lz_circulo_suave(lienzo *l, float cx, float cy, float r, u32 c)
{
	int x0 = m_suelo(cx - r - 1), x1 = m_suelo(cx + r + 1), y0 = m_suelo(cy - r - 1), y1 = m_suelo(cy + r + 1), x, y;
	float r2i = (r - 0.5f) * (r - 0.5f), r2o = (r + 0.5f) * (r + 0.5f);

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 >= l->an) x1 = l->an - 1;
	if (y1 >= l->al) y1 = l->al - 1;
	for (y = y0; y <= y1; y++) {
		float dy = (float)y + 0.5f - cy;
		u32 *p = l->p + (size_t)y * (size_t)l->paso;

		for (x = x0; x <= x1; x++) {
			float dx = (float)x + 0.5f - cx, d2 = dx * dx + dy * dy;

			if (d2 <= r2i)
				p[x] = c;
			else if (d2 < r2o) {
				float d = m_raiz(d2);
				int a = (int)((r + 0.5f - d) * 256.0f);

				p[x] = mezcla(p[x], c, a);
			}
		}
	}
}

void lz_anillo(lienzo *l, int cx, int cy, int r, int grueso, u32 c)
{
	int f, ri = r - grueso;

	/* por filas: lo de dentro del grande y fuera del pequeño (dos tramos) */
	for (f = -r; f <= r; f++) {
		int d = (int)m_raiz((float)(r * r - f * f)), di = f * f < ri * ri ? (int)m_raiz((float)(ri * ri - f * f)) : -1;

		if (di < 0)
			lz_rect(l, cx - d, cy + f, 2 * d + 1, 1, c);
		else {
			lz_rect(l, cx - d, cy + f, d - di, 1, c);
			lz_rect(l, cx + di + 1, cy + f, d - di, 1, c);
		}
	}
}

void lz_elipse(lienzo *l, int cx, int cy, int rx, int ry, u32 c)
{
	int f;

	if (ry < 1 || rx < 1)
		return;
	for (f = -ry; f <= ry; f++) {
		float k = 1.0f - ((float)f * (float)f) / ((float)ry * (float)ry);
		int d = (int)((float)rx * m_raiz(k) + 0.5f);

		lz_rect(l, cx - d, cy + f, 2 * d + 1, 1, c);
	}
}

/* ---- los polígonos: cuatro renglones por fila, con lo que tapa cada punto
 * de los bordes (el suavizado), por la regla par-impar ---- */
#define MAX_CORTES 64
static u16 cubre[4096];

static void poligono(lienzo *l, const float *xy, int n, u32 c, int alfa)
{
	float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
	int i, y, y0, y1, xa, xb, s;

	if (n < 3)
		return;
	for (i = 0; i < n; i++) {
		float px = xy[2 * i], py = xy[2 * i + 1];

		if (py < ymin) ymin = py;
		if (py > ymax) ymax = py;
		if (px < xmin) xmin = px;
		if (px > xmax) xmax = px;
	}
	y0 = m_suelo(ymin);
	y1 = m_suelo(ymax);
	if (y0 < 0) y0 = 0;
	if (y1 >= l->al) y1 = l->al - 1;
	xa = m_suelo(xmin);
	xb = m_suelo(xmax) + 1;
	if (xa < 0) xa = 0;
	if (xb >= l->an) xb = l->an - 1;
	if (xb - xa + 1 > 4096 || xa > xb)
		return;
	for (y = y0; y <= y1; y++) {
		int hay = 0, k;

		memset(cubre + xa, 0, (size_t)(xb - xa + 1) * 2);
		for (s = 0; s < 4; s++) {
			float ys = (float)y + ((float)s + 0.5f) * 0.25f, cortes[MAX_CORTES];
			int nc = 0, a, b;

			for (i = 0; i < n; i++) {
				float ax = xy[2 * i], ay = xy[2 * i + 1];
				float bx = xy[2 * ((i + 1) % n)], by = xy[2 * ((i + 1) % n) + 1];

				if ((ay <= ys && by > ys) || (by <= ys && ay > ys)) {
					if (nc < MAX_CORTES)
						cortes[nc++] = ax + (ys - ay) * (bx - ax) / (by - ay);
				}
			}
			/* ordenar (pocos) */
			for (a = 1; a < nc; a++) {
				float v = cortes[a];

				for (b = a - 1; b >= 0 && cortes[b] > v; b--)
					cortes[b + 1] = cortes[b];
				cortes[b + 1] = v;
			}
			for (a = 0; a + 1 < nc; a += 2) {
				float fa = cortes[a], fb = cortes[a + 1];
				int pa, pb, x;

				if (fa < (float)xa) fa = (float)xa;
				if (fb > (float)xb + 1) fb = (float)xb + 1;
				if (fb <= fa)
					continue;
				pa = m_suelo(fa);
				pb = m_suelo(fb);
				hay = 1;
				if (pa == pb) {
					cubre[pa] += (u16)((fb - fa) * 64.0f);
					continue;
				}
				cubre[pa] += (u16)(((float)(pa + 1) - fa) * 64.0f);
				for (x = pa + 1; x < pb; x++)
					cubre[x] += 64;
				if (pb <= xb)
					cubre[pb] += (u16)((fb - (float)pb) * 64.0f);
			}
		}
		if (!hay)
			continue;
		{
			u32 *p = l->p + (size_t)y * (size_t)l->paso;

			for (k = xa; k <= xb; k++) {
				int v = cubre[k];

				if (!v)
					continue;
				if (v > 256)
					v = 256;
				v = (v * alfa) >> 8;
				p[k] = v >= 256 ? c : mezcla(p[k], c, v);
			}
		}
	}
}

void lz_poligono(lienzo *l, const float *xy, int n, u32 c) { poligono(l, xy, n, c, 256); }
void lz_poligono_mezcla(lienzo *l, const float *xy, int n, u32 c, int alfa) { poligono(l, xy, n, c, alfa); }

void lz_copia(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al)
{
	int f;

	/* recortar a los dos */
	if (ox < 0) { x -= ox; an += ox; ox = 0; }
	if (oy < 0) { y -= oy; al += oy; oy = 0; }
	if (ox + an > o->an) an = o->an - ox;
	if (oy + al > o->al) al = o->al - oy;
	if (x < 0) { ox -= x; an += x; x = 0; }
	if (y < 0) { oy -= y; al += y; y = 0; }
	if (x + an > d->an) an = d->an - x;
	if (y + al > d->al) al = d->al - y;
	if (an <= 0 || al <= 0)
		return;
	for (f = 0; f < al; f++)
		memcpy(d->p + (size_t)(y + f) * (size_t)d->paso + (size_t)x, o->p + (size_t)(oy + f) * (size_t)o->paso + (size_t)ox, (size_t)an * 4);
}

void lz_copia_clave(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al, u32 clave)
{
	int f, k;

	if (ox < 0) { x -= ox; an += ox; ox = 0; }
	if (oy < 0) { y -= oy; al += oy; oy = 0; }
	if (ox + an > o->an) an = o->an - ox;
	if (oy + al > o->al) al = o->al - oy;
	if (x < 0) { ox -= x; an += x; x = 0; }
	if (y < 0) { oy -= y; al += y; y = 0; }
	if (x + an > d->an) an = d->an - x;
	if (y + al > d->al) al = d->al - y;
	for (f = 0; f < al; f++) {
		u32 *p = d->p + (size_t)(y + f) * (size_t)d->paso + (size_t)x;
		const u32 *q = o->p + (size_t)(oy + f) * (size_t)o->paso + (size_t)ox;

		for (k = 0; k < an; k++)
			if (q[k] != clave)
				p[k] = q[k];
	}
}

void lz_dibujo(lienzo *l, int x, int y, const char *d, int an, int al, int escala, const u32 *tabla)
{
	int f, k;

	for (f = 0; f < al; f++)
		for (k = 0; k < an; k++) {
			unsigned char ch = (unsigned char)d[f * an + k];
			u32 c;

			if (ch == '.' || ch >= 128)
				continue;
			c = tabla[ch];
			if (c == 0xFFFFFFFFu)
				continue;
			lz_rect(l, x + k * escala, y + f * escala, escala, escala, c);
		}
}

/* ---- la letra ---- */
int a437(const char *t, char *d, int cabe)
{
	int n = 0;
	const unsigned char *s = (const unsigned char *)t;

	while (*s && n < cabe - 1) {
		u32 u = *s++;

		if (u >= 0x80) {
			int mas = 0, i;

			if ((u & 0xE0) == 0xC0) { u &= 0x1F; mas = 1; }
			else if ((u & 0xF0) == 0xE0) { u &= 0x0F; mas = 2; }
			else if ((u & 0xF8) == 0xF0) { u &= 0x07; mas = 3; }
			else u = '?';
			while (mas-- > 0 && (*s & 0xC0) == 0x80)
				u = (u << 6) | (*s++ & 0x3F);
			if (u == 0xD7)
				u = 'x';	/* (×) */
			for (i = 128; i < 256; i++)
				if (uni437[i] == u)
					break;
			u = i < 256 ? (u32)i : '?';
		}
		d[n++] = (char)u;
	}
	d[n] = 0;
	return n;
}

static void letra(lienzo *l, int x, int y, unsigned char ch, u32 c, int e)
{
	const unsigned char *g = letra8x16 + ch * 16;
	int f, b;

	if (x >= l->an || y >= l->al || x + 8 * e <= 0 || y + 16 * e <= 0)
		return;
	for (f = 0; f < 16; f++) {
		unsigned v = g[f];

		if (!v)
			continue;
		for (b = 0; b < 8; b++)
			if (v & (0x80u >> b)) {
				if (e == 1)
					punto(l, x + b, y + f, c, 256);
				else
					lz_rect(l, x + b * e, y + f * e, e, e, c);
			}
	}
}

int lz_texto(lienzo *l, int x, int y, const char *t, u32 c, int e)
{
	char b[512];
	int n = a437(t, b, sizeof b), i;

	for (i = 0; i < n; i++)
		letra(l, x + i * 8 * e, y, (unsigned char)b[i], c, e);
	return n * 8 * e;
}

int lz_negrita(lienzo *l, int x, int y, const char *t, u32 c, int e)
{
	int a = lz_texto(l, x, y, t, c, e);

	lz_texto(l, x + (e > 1 ? e / 2 : 1), y, t, c, e);
	return a + 1;
}

int lz_texto_sombra(lienzo *l, int x, int y, const char *t, u32 c, u32 sombra, int e)
{
	lz_texto(l, x + e, y + e, t, sombra, e);
	return lz_texto(l, x, y, t, c, e);
}

int ancho_texto(const char *t, int e)
{
	char b[512];

	return a437(t, b, sizeof b) * 8 * e;
}

int lz_texto_centro(lienzo *l, int cx, int y, const char *t, u32 c, int e, int negrita)
{
	int a = ancho_texto(t, e);

	if (negrita)
		lz_negrita(l, cx - a / 2, y, t, c, e);
	else
		lz_texto(l, cx - a / 2, y, t, c, e);
	return a;
}

int lz_parrafo(lienzo *l, int x, int y, int max, const char *t, u32 c, int e, int interlinea)
{
	char b[2048], r[256];
	int n = a437(t, b, sizeof b), i = 0, renglones = 0, cabe = max / (8 * e);

	if (cabe < 1)
		cabe = 1;
	if (cabe > 255)
		cabe = 255;
	while (i < n) {
		int k = 0, ult = -1;

		while (i + k < n && b[i + k] != '\n' && k < cabe) {
			if (b[i + k] == ' ')
				ult = k;
			k++;
		}
		if (i + k < n && b[i + k] != '\n' && ult > 0)
			k = ult;
		memcpy(r, b + i, (size_t)k);
		r[k] = 0;
		if (l) {
			int j;

			for (j = 0; j < k; j++)
				letra(l, x + j * 8 * e, y + renglones * interlinea, (unsigned char)r[j], c, e);
		}
		renglones++;
		i += k;
		if (i < n && (b[i] == ' ' || b[i] == '\n'))
			i++;
	}
	return renglones;
}

/* las 16 filas de una letra (de la página 437), para quien la pinta a su manera (el marcador del pinball) */
const unsigned char *jg_letra(int c) { return letra8x16 + (c & 255) * 16; }
