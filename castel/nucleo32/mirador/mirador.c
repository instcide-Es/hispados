/* mirador.c - MIRADOR.BIN, el Mirador de Hispa-Pentium: el visor de
 * imágenes (docs/PENTIUM.md, «El Mirador»), como el Mirador de Hispa-386
 * (programas/mirador*) pero un programa del anillo 3 en C, con stb_image
 * (terceros/stb, dominio público o MIT) para leer BMP, PNG, JPG, GIF (con
 * su animación), TGA, PSD y PNM, y un lector propio de ICO (y CUR):
 *
 *   MIRADOR [RUTA]
 *
 *   - ajustada a la ventana (sin agrandar las pequeñas) o al 100 %; la
 *     rueda acerca y aleja donde está la flecha; + y -; arrastrar mueve;
 *   - girar (R, y Mayús+R al otro lado); las demás imágenes de la carpeta
 *     con las flechas (Inicio y Fin: la primera y la última);
 *   - pase de diapositivas (F5: cada 4 segundos, en la ventana o a
 *     pantalla completa), pantalla completa (Intro, F o dos clics; Esc vuelve);
 *   - «Usar como fondo de escritorio»: la imagen, a la medida de la
 *     pantalla (llenándola: lo que sobra, cortado), al núcleo (la llamada
 *     172), que la guarda y la pone; «Quitar el fondo...» vuelve al de siempre.
 *
 * Al alejar se lee de la imagen a la mitad, a la cuarta parte... (hechas
 * con la media de cada cuatro puntos) y con el bilineal; al acercar, los
 * puntos tal cual (como los visores de siempre). Lo transparente, sobre
 * cuadros grises. Lo que pasa, por el puerto serie («mirador: ...»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pentium.h"
#include "ui.h"

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAX_ARCHIVO (64u * 1024 * 1024)
#define MAX_PUNTOS (40u * 1024 * 1024)	/* lo más que se guarda de una imagen (de todos sus cuadros): 160 MB no */
#define ALTO_BARRA 40
#define ALTO_ESTADO 26
#define FONDO_VISOR 0x2A2D31

static void serie(const char *f, ...)
{
	char t[400];
	va_list a;

	va_start(a, f);
	vsnprintf(t, sizeof(t), f, a);
	va_end(a);
	ui_serie(t);
}

/* ------------------------------------------------------------ la imagen */
typedef struct { int w, h; uint32_t *px; } lienzo;	/* 0xAARRGGBB */

static int iw, ih;			/* lo que mide (ya girada) */
static uint32_t **cuadros;		/* uno, o los de un GIF animado */
static int *esperas;			/* (lo que dura cada cuadro, en milésimas) */
static int n_cuadros, cuadro;
static lienzo mip[14];			/* el cuadro de ahora a la mitad, a la cuarta parte... */
static int n_mip;
static char ruta[300], nombre[100], clase[8], error_img[120];
static int hay_imagen;
static uint32_t v_normal;		/* la ventana de siempre (la otra es la de la pantalla completa) */

static void suelta_mip(void)
{
	int i;

	for (i = 1; i < n_mip; i++)
		free(mip[i].px);
	n_mip = 0;
}

static void suelta_imagen(void)
{
	int i;

	suelta_mip();
	for (i = 0; i < n_cuadros; i++)
		free(cuadros[i]);
	free(cuadros);
	free(esperas);
	cuadros = NULL;
	esperas = NULL;
	n_cuadros = cuadro = 0;
	hay_imagen = 0;
}

/* de los RGBA de stb a 0xAARRGGBB, en su sitio */
static void a_puntos(uint8_t *p, uint32_t n)
{
	uint32_t i, *q = (uint32_t *)p;

	for (i = 0; i < n; i++, p += 4)
		q[i] = (uint32_t)p[3] << 24 | (uint32_t)p[0] << 16 | (uint32_t)p[1] << 8 | p[2];
}

/* las mitades del cuadro de ahora (para alejar sin que salga «picado») */
static void haz_mip(void)
{
	int k, x, y;

	if (n_mip)
		return;
	mip[0].w = iw;
	mip[0].h = ih;
	mip[0].px = cuadros[cuadro];
	n_mip = 1;
	if (n_cuadros > 1)
		return;		/* (en los animados, no: cambian a cada momento) */
	for (k = 1; k < 14; k++) {
		lienzo *a = &mip[k - 1], *b = &mip[k];

		if (a->w < 2 || a->h < 2)
			break;
		b->w = a->w / 2;
		b->h = a->h / 2;
		b->px = malloc((size_t)b->w * b->h * 4);
		if (!b->px)
			break;
		for (y = 0; y < b->h; y++)
			for (x = 0; x < b->w; x++) {
				uint32_t p0 = a->px[(2 * y) * a->w + 2 * x], p1 = a->px[(2 * y) * a->w + 2 * x + 1];
				uint32_t p2 = a->px[(2 * y + 1) * a->w + 2 * x], p3 = a->px[(2 * y + 1) * a->w + 2 * x + 1];
				uint32_t r = 0, c;
				int s;

				for (s = 0; s < 32; s += 8) {
					c = ((p0 >> s & 255) + (p1 >> s & 255) + (p2 >> s & 255) + (p3 >> s & 255) + 2) >> 2;
					r |= c << s;
				}
				b->px[y * b->w + x] = r;
			}
		n_mip++;
	}
}

/* ---- ICO y CUR: el más grande (y de más colores) de los que lleva ---- */
static uint8_t *lee_ico(const uint8_t *d, uint32_t n, int *w, int *h)
{
	uint32_t k, cuantos, mejor = 0xFFFFFFFFu, mejor_area = 0, mejor_bpp = 0, off, tam;
	const uint8_t *e;
	uint8_t *img;

	if (n < 6 || d[0] || d[1] || (d[2] != 1 && d[2] != 2) || d[3])
		return NULL;
	cuantos = (uint32_t)(d[4] | d[5] << 8);
	if (cuantos == 0 || 6 + cuantos * 16 > n)
		return NULL;
	for (k = 0; k < cuantos; k++) {
		uint32_t a, bpp;

		e = d + 6 + k * 16;
		a = (uint32_t)(e[0] ? e[0] : 256) * (e[1] ? e[1] : 256);
		bpp = (uint32_t)(e[6] | e[7] << 8);
		if (a > mejor_area || (a == mejor_area && bpp > mejor_bpp)) {
			mejor = k;
			mejor_area = a;
			mejor_bpp = bpp;
		}
	}
	e = d + 6 + mejor * 16;
	tam = (uint32_t)(e[8] | e[9] << 8 | e[10] << 16 | (uint32_t)e[11] << 24);
	off = (uint32_t)(e[12] | e[13] << 8 | e[14] << 16 | (uint32_t)e[15] << 24);
	if (off >= n || tam > n - off || tam < 40)
		return NULL;
	d += off;
	n = tam;
	/* un PNG dentro (los de 256 x 256) */
	if (n >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') {
		int c;

		return stbi_load_from_memory(d, (int)n, w, h, &c, 4);
	}
	/* un BMP sin su cabecera de archivo, de doble alto (la imagen y su máscara) */
	{
		int32_t bw = (int32_t)(d[4] | d[5] << 8 | d[6] << 16 | (uint32_t)d[7] << 24);
		int32_t bh = (int32_t)(d[8] | d[9] << 8 | d[10] << 16 | (uint32_t)d[11] << 24) / 2;
		uint32_t cab = (uint32_t)(d[0] | d[1] << 8), bpp = (uint32_t)(d[14] | d[15] << 8), usados, paleta, fila, xor_n, mfila;
		uint8_t *bmp;
		int c, x, y;

		if (cab < 40 || cab > n || bw <= 0 || bh <= 0 || bw > 1024 || bh > 1024 || (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32))
			return NULL;
		usados = (uint32_t)(d[32] | d[33] << 8 | d[34] << 16 | (uint32_t)d[35] << 24);
		paleta = bpp <= 8 ? 4 * (usados && usados <= (1u << bpp) ? usados : 1u << bpp) : 0;
		fila = (((uint32_t)bw * bpp + 31) / 32) * 4;
		xor_n = fila * (uint32_t)bh;
		mfila = (((uint32_t)bw + 31) / 32) * 4;
		if (cab + paleta + xor_n > n)
			return NULL;
		bmp = malloc(14 + n);
		if (!bmp)
			return NULL;
		memcpy(bmp + 14, d, n);
		bmp[0] = 'B';
		bmp[1] = 'M';
		k = 14 + n;
		memcpy(bmp + 2, &k, 4);
		memset(bmp + 6, 0, 4);
		k = 14 + cab + paleta;
		memcpy(bmp + 10, &k, 4);
		memcpy(bmp + 14 + 8, &bh, 4);	/* (el alto de la imagen sola) */
		memset(bmp + 14 + 20, 0, 4);	/* (sin tamaño de la imagen: lo calcula) */
		img = stbi_load_from_memory(bmp, (int)(14 + n), w, h, &c, 4);
		free(bmp);
		if (!img)
			return NULL;
		/* la máscara (de abajo arriba): un 1 es transparente */
		if (bpp < 32 && cab + paleta + xor_n + mfila * (uint32_t)bh <= n) {
			const uint8_t *m = d + cab + paleta + xor_n;

			for (y = 0; y < bh; y++)
				for (x = 0; x < bw; x++)
					if (m[(uint32_t)(bh - 1 - y) * mfila + (uint32_t)x / 8] & (0x80 >> (x & 7)))
						img[((size_t)y * (size_t)bw + (size_t)x) * 4 + 3] = 0;
		}
		return img;
	}
}

static int extension(const char *n, char *e)
{
	const char *p = strrchr(n, '.'), *b = strrchr(n, '\\');
	int i;

	e[0] = 0;
	if (!p || (b && p < b))
		return 0;
	for (i = 0; i < 4 && p[1 + i]; i++)
		e[i] = (char)(p[1 + i] >= 'a' && p[1 + i] <= 'z' ? p[1 + i] - 32 : p[1 + i]);
	e[i] = 0;
	return 1;
}

static int es_imagen(const char *n)
{
	static const char *const clases[] = { "BMP", "PNG", "JPG", "JPE", "JPEG", "GIF", "ICO", "CUR", "TGA", "PSD", "PPM", "PGM", "PNM", NULL };
	char e[8];
	int i;

	if (!extension(n, e))
		return 0;
	for (i = 0; clases[i]; i++)
		if (!strcmp(e, clases[i]))
			return 1;
	return 0;
}

static int carga(const char *r)
{
	uint32_t m, largo, i, n;
	uint8_t *datos, *img = NULL;
	int w = 0, h = 0, c, z = 0, *retrasos = NULL;
	const char *b;

	suelta_imagen();
	error_img[0] = 0;
	snprintf(ruta, sizeof(ruta), "%s", r);
	b = strrchr(ruta, '\\');
	snprintf(nombre, sizeof(nombre), "%s", b ? b + 1 : ruta);
	extension(ruta, clase);
	m = sc(SC_ABRIR, (uint32_t)ruta, PARA_LEER, 0);
	if (ES_ERROR(m)) {
		snprintf(error_img, sizeof(error_img), "No se puede abrir %s.", nombre);
		return -1;
	}
	largo = sc(SC_LARGO, m, 0, 0);
	if (ES_ERROR(largo) || largo == 0 || largo > MAX_ARCHIVO) {
		sc(SC_CERRAR, m, 0, 0);
		snprintf(error_img, sizeof(error_img), "%s est\xA0 vac\xA1o o es demasiado grande.", nombre);
		return -1;
	}
	datos = malloc(largo);
	if (!datos) {
		sc(SC_CERRAR, m, 0, 0);
		snprintf(error_img, sizeof(error_img), "No hay memoria para leer %s.", nombre);
		return -1;
	}
	for (i = 0; i < largo; i += n) {
		n = sc(SC_LEER, m, (uint32_t)(datos + i), largo - i);
		if (ES_ERROR(n) || n == 0)
			break;
	}
	sc(SC_CERRAR, m, 0, 0);
	if (i < largo) {
		free(datos);
		snprintf(error_img, sizeof(error_img), "No se puede leer %s.", nombre);
		return -1;
	}
	if (!strcmp(clase, "ICO") || !strcmp(clase, "CUR"))
		img = lee_ico(datos, largo, &w, &h), z = 1;
	else if (largo > 6 && !memcmp(datos, "GIF", 3)) {
		img = stbi_load_gif_from_memory(datos, (int)largo, &retrasos, &w, &h, &z, &c, 4);
		if (img && (uint64_t)w * h * z > MAX_PUNTOS) {
			stbi_image_free(img);
			free(retrasos);
			retrasos = NULL;
			img = NULL;
		}
		if (!img)
			img = stbi_load_from_memory(datos, (int)largo, &w, &h, &c, 4), z = 1;
	} else
		img = stbi_load_from_memory(datos, (int)largo, &w, &h, &c, 4), z = 1;
	free(datos);
	if (!img || w <= 0 || h <= 0 || z <= 0) {
		free(retrasos);
		snprintf(error_img, sizeof(error_img), "%s no es una imagen que se sepa leer (o est\xA0 rota).", nombre);
		serie("mirador: no se lee %s (%s)\n", ruta, stbi_failure_reason() ? stbi_failure_reason() : "?");
		return -1;
	}
	if ((uint64_t)w * h > MAX_PUNTOS) {
		stbi_image_free(img);
		free(retrasos);
		snprintf(error_img, sizeof(error_img), "%s es demasiado grande (%d x %d).", nombre, w, h);
		return -1;
	}
	iw = w;
	ih = h;
	cuadros = calloc((size_t)z, sizeof(uint32_t *));
	esperas = calloc((size_t)z, sizeof(int));
	if (!cuadros || !esperas) {
		stbi_image_free(img);
		free(retrasos);
		return -1;
	}
	if (z == 1) {
		a_puntos(img, (uint32_t)(w * h));
		cuadros[0] = (uint32_t *)img;
		n_cuadros = 1;
	} else {
		/* cada cuadro, aparte (para girarlos y soltarlos uno a uno) */
		for (i = 0; i < (uint32_t)z; i++) {
			cuadros[i] = malloc((size_t)w * h * 4);
			if (!cuadros[i])
				break;
			memcpy(cuadros[i], img + (size_t)i * w * h * 4, (size_t)w * h * 4);
			a_puntos((uint8_t *)cuadros[i], (uint32_t)(w * h));
			esperas[i] = retrasos && retrasos[i] > 10 ? retrasos[i] : 100;
		}
		n_cuadros = (int)i;
		stbi_image_free(img);
		if (n_cuadros == 0)
			return -1;
	}
	free(retrasos);
	hay_imagen = 1;
	serie("mirador: abierta %s (%d x %d, %d cuadros)\n", ruta, iw, ih, n_cuadros);
	return 0;
}

/* gira todos los cuadros un cuarto de vuelta (a la derecha, o a la izquierda) */
static void gira(int derecha)
{
	int i, x, y;

	if (!hay_imagen)
		return;
	suelta_mip();
	for (i = 0; i < n_cuadros; i++) {
		uint32_t *a = cuadros[i], *b = malloc((size_t)iw * ih * 4);

		if (!b)
			return;
		for (y = 0; y < ih; y++)
			for (x = 0; x < iw; x++) {
				if (derecha)
					b[x * ih + (ih - 1 - y)] = a[y * iw + x];
				else
					b[(iw - 1 - x) * ih + y] = a[y * iw + x];
			}
		free(a);
		cuadros[i] = b;
	}
	x = iw;
	iw = ih;
	ih = x;
	serie("mirador: girada a la %s (%d x %d)\n", derecha ? "derecha" : "izquierda", iw, ih);
}

/* ------------------------------------------------------------- pintar */
static int zoom = 1000, ajustar = 1;	/* zoom, en milésimas */
static int centro_x, centro_y;		/* el punto de la imagen en el centro (si no cabe) */
static uint32_t *lienzo_px;
static int lienzo_n;

static uint32_t mezcla(uint32_t p, uint32_t fondo)
{
	uint32_t a = p >> 24, r, g, b;

	if (a == 255)
		return p & 0xFFFFFF;
	if (a == 0)
		return fondo;
	r = (((p >> 16) & 255) * a + ((fondo >> 16) & 255) * (255 - a)) / 255;
	g = (((p >> 8) & 255) * a + ((fondo >> 8) & 255) * (255 - a)) / 255;
	b = ((p & 255) * a + (fondo & 255) * (255 - a)) / 255;
	return r << 16 | g << 8 | b;
}

static int zoom_ajustado(int cw, int ch)
{
	int64_t a = (int64_t)cw * 1000 / iw, b = (int64_t)ch * 1000 / ih;
	int z = (int)(a < b ? a : b);

	if (z > 1000)
		z = 1000;
	return z < 1 ? 1 : z;
}

/* dónde cae la esquina de la imagen (ox, oy) en un lienzo de cw x ch, con
 * ese zoom: centrada si cabe; si no, con «centro» en el centro (sin huecos) */
static void origen(int cw, int ch, int z, int *ox, int *oy)
{
	int64_t dw = (int64_t)iw * z / 1000, dh = (int64_t)ih * z / 1000, mx, my;

	if (dw <= cw) {
		*ox = (int)((cw - dw) / 2);
		centro_x = iw / 2;
	} else {
		mx = (int64_t)cw * 500 / z;
		if (centro_x < mx)
			centro_x = (int)mx;
		if (centro_x > iw - mx)
			centro_x = (int)(iw - mx);
		*ox = (int)(cw / 2 - (int64_t)centro_x * z / 1000);
	}
	if (dh <= ch) {
		*oy = (int)((ch - dh) / 2);
		centro_y = ih / 2;
	} else {
		my = (int64_t)ch * 500 / z;
		if (centro_y < my)
			centro_y = (int)my;
		if (centro_y > ih - my)
			centro_y = (int)(ih - my);
		*oy = (int)(ch / 2 - (int64_t)centro_y * z / 1000);
	}
}

/* la imagen en «dst» (cw x ch) con ese zoom y esa esquina; «cuadros» de
 * fondo para lo transparente (o, con «solido», ese color) */
static void pinta_en(uint32_t *dst, int cw, int ch, int z, int ox, int oy, int solido, uint32_t color)
{
	int x, y, L = 0;
	uint32_t paso, fondo;
	lienzo *m;
	int x0, x1, y0, y1;

	for (y = 0; y < ch * cw; y++)
		dst[y] = FONDO_VISOR;
	if (!hay_imagen)
		return;
	haz_mip();
	/* de la mitad que haga falta (la más pequeña que no se quede corta) */
	if (z < 1000)
		while (L + 1 < n_mip && ((int64_t)iw * z / 1000) <= (iw >> (L + 1)))
			L++;
	m = &mip[L];
	/* el paso, en 16.16, en los puntos de esa mitad */
	paso = (uint32_t)(((uint64_t)1000 << 16) / (uint32_t)z) >> L;
	x0 = ox > 0 ? ox : 0;
	y0 = oy > 0 ? oy : 0;
	x1 = ox + (int)((int64_t)iw * z / 1000);
	y1 = oy + (int)((int64_t)ih * z / 1000);
	if (x1 > cw)
		x1 = cw;
	if (y1 > ch)
		y1 = ch;
	for (y = y0; y < y1; y++) {
		uint32_t sy = (uint32_t)(y - oy) * paso + (L || z < 1000 ? paso / 2 : 0);
		int iy = (int)(sy >> 16);
		uint32_t fy = (sy >> 8) & 255;
		uint32_t *fila = dst + (size_t)y * cw;

		if (iy >= m->h)
			iy = m->h - 1;
		for (x = x0; x < x1; x++) {
			uint32_t sx = (uint32_t)(x - ox) * paso + (L || z < 1000 ? paso / 2 : 0), p;
			int ix = (int)(sx >> 16);

			if (ix >= m->w)
				ix = m->w - 1;
			if (z >= 1000) {
				p = m->px[iy * m->w + ix];
			} else {
				/* bilineal (con su alfa) */
				uint32_t fx = (sx >> 8) & 255, a, b, c, d, r = 0;
				int ix2 = ix + 1 < m->w ? ix + 1 : ix, iy2 = iy + 1 < m->h ? iy + 1 : iy, s;

				a = m->px[iy * m->w + ix];
				b = m->px[iy * m->w + ix2];
				c = m->px[iy2 * m->w + ix];
				d = m->px[iy2 * m->w + ix2];
				for (s = 0; s < 32; s += 8) {
					uint32_t t1 = ((a >> s & 255) * (256 - fx) + (b >> s & 255) * fx) >> 8;
					uint32_t t2 = ((c >> s & 255) * (256 - fx) + (d >> s & 255) * fx) >> 8;

					r |= ((t1 * (256 - fy) + t2 * fy) >> 8) << s;
				}
				p = r;
			}
			fondo = solido ? color : (((x >> 3) ^ (y >> 3)) & 1) ? 0xC8C8C8 : 0xF0F0F0;
			fila[x] = mezcla(p, fondo);
		}
	}
}

/* ---- las imágenes de la carpeta (para las flechas) ---- */
static char (*hermanas)[13];
static int n_hermanas, esta;

static void mira_carpeta(void)
{
	char carpeta[300], *b;
	struct entrada e;
	uint32_t m, r;
	int k, j;

	free(hermanas);
	hermanas = NULL;
	n_hermanas = 0;
	esta = -1;
	snprintf(carpeta, sizeof(carpeta), "%s", ruta);
	b = strrchr(carpeta, '\\');
	if (b)
		b[b == carpeta || b[-1] == ':' ? 1 : 0] = 0;
	else
		strcpy(carpeta, ".");
	m = sc(SC_ABRIR, (uint32_t)carpeta, PARA_LEER, 0);
	if (ES_ERROR(m))
		return;
	for (;;) {
		r = sc(SC_ENTRADA, m, (uint32_t)&e, 0);
		if (ES_ERROR(r) || r == 0)
			break;
		e.nombre[12] = 0;
		if ((e.atributos & 0x18) || !es_imagen(e.nombre))
			continue;
		if (n_hermanas % 64 == 0) {
			char (*q)[13] = realloc(hermanas, (size_t)(n_hermanas + 64) * 13);

			if (!q)
				break;
			hermanas = q;
		}
		strcpy(hermanas[n_hermanas++], e.nombre);
	}
	sc(SC_CERRAR, m, 0, 0);
	/* por el nombre */
	for (k = 1; k < n_hermanas; k++) {
		char t[13];

		strcpy(t, hermanas[k]);
		for (j = k; j > 0 && strcmp(t, hermanas[j - 1]) < 0; j--)
			strcpy(hermanas[j], hermanas[j - 1]);
		strcpy(hermanas[j], t);
	}
	for (k = 0; k < n_hermanas; k++)
		if (!strcmp(hermanas[k], nombre))
			esta = k;
}

static void abre(const char *r)
{
	char t[160];

	carga(r);
	zoom = 1000;
	ajustar = 1;
	centro_x = iw / 2;
	centro_y = ih / 2;
	snprintf(t, sizeof(t), "%s - Mirador", nombre);
	sc(SC_TITULO, v_normal ? v_normal : ui_v, (uint32_t)t, 0);
}

static void otra(int paso)
{
	char r[300], *b;
	int k;

	if (n_hermanas == 0)
		return;
	if (paso == -1000)
		k = 0;
	else if (paso == 1000)
		k = n_hermanas - 1;
	else
		k = ((esta < 0 ? 0 : esta) + paso + n_hermanas) % n_hermanas;
	snprintf(r, sizeof(r), "%s", ruta);
	b = strrchr(r, '\\');
	if (b)
		b[1] = 0;
	else
		r[0] = 0;
	strcat(r, hermanas[k]);
	esta = k;
	abre(r);
}

/* ---- la ventana ---- */
#define B_ANTES 1
#define B_DESPUES 2
#define B_MENOS 3
#define B_MAS 4
#define B_AJUSTAR 5
#define B_UNO 6
#define B_GIRAR 7
#define B_DIAPOS 8
#define B_COMPLETA 9
#define B_OTRAS 10
#define M_MAS 1

static int diapos, completa;
static uint32_t v_completa;
static int an_normal, al_normal;
static uint32_t ultimo_cambio, ultimo_cuadro;
static char aviso[200];

static void lienzo_de(int cw, int ch)
{
	if (lienzo_n < cw * ch) {
		free(lienzo_px);
		lienzo_px = malloc((size_t)cw * ch * 4);
		lienzo_n = lienzo_px ? cw * ch : 0;
	}
}

static void pinta(void)
{
	int cw, ch, ox, oy, x, z;
	char t[300];

	if (completa) {
		cw = ui_an;
		ch = ui_al;
	} else {
		cw = ui_an;
		ch = ui_al - ALTO_BARRA - ALTO_ESTADO;
	}
	if (cw < 1 || ch < 1)
		return;
	lienzo_de(cw, ch);
	if (!lienzo_px)
		return;
	z = hay_imagen ? (ajustar ? zoom_ajustado(cw, ch) : zoom) : 1000;
	if (ajustar)
		zoom = z;
	if (hay_imagen) {
		origen(cw, ch, z, &ox, &oy);
		pinta_en(lienzo_px, cw, ch, z, ox, oy, 0, 0);
	} else
		pinta_en(lienzo_px, cw, ch, z, 0, 0, 0, 0);
	ui_olvida_zonas();
	ui_mapa(0, completa ? 0 : ALTO_BARRA, cw, ch, lienzo_px);
	if (!hay_imagen)
		ui_texto(20, (completa ? 0 : ALTO_BARRA) + 20, error_img[0] ? error_img : "Abra una imagen con dos clics en Mi equipo, o con M\xA0s... > Abrir.",
			 (cw - 40) / ANCHO_LETRA, 0xE5E7EB);
	if (completa) {
		if (aviso[0])
			ui_texto(12, ch - 24, aviso, (cw - 24) / ANCHO_LETRA, 0xE5E7EB);
		return;
	}
	ui_rect(0, 0, ui_an, ALTO_BARRA, C_BARRA);
	ui_rect(0, ALTO_BARRA - 1, ui_an, 1, C_GRIS);
	x = 8;
	x += ui_boton(B_ANTES, x, 6, 32, "<", 0, n_hermanas < 2) + 4;
	x += ui_boton(B_DESPUES, x, 6, 32, ">", 0, n_hermanas < 2) + 10;
	x += ui_boton(B_MENOS, x, 6, 32, "-", 0, !hay_imagen) + 4;
	x += ui_boton(B_MAS, x, 6, 32, "+", 0, !hay_imagen) + 4;
	x += ui_boton(B_AJUSTAR, x, 6, 0, "Ajustar", ajustar && hay_imagen, !hay_imagen) + 4;
	x += ui_boton(B_UNO, x, 6, 0, "1:1", !ajustar && zoom == 1000 && hay_imagen, !hay_imagen) + 10;
	x += ui_boton(B_GIRAR, x, 6, 0, "Girar", 0, !hay_imagen) + 10;
	x += ui_boton(B_DIAPOS, x, 6, 0, "Diapositivas", diapos, n_hermanas < 1) + 4;
	x += ui_boton(B_COMPLETA, x, 6, 0, "Completa", 0, !hay_imagen) + 4;
	if (x + ui_ancho_boton("M\xA0s...") < ui_an)
		ui_boton(B_OTRAS, x, 6, 0, "M\xA0s...", 0, 0);
	ui_rect(0, ui_al - ALTO_ESTADO, ui_an, ALTO_ESTADO, C_BARRA);
	ui_rect(0, ui_al - ALTO_ESTADO, ui_an, 1, C_GRIS);
	if (aviso[0])
		snprintf(t, sizeof(t), "%s", aviso);
	else if (hay_imagen)
		snprintf(t, sizeof(t), "%s   %d x %d   %d %%   %d de %d   %s%s%s", nombre, iw, ih, (zoom + 5) / 10, esta + 1, n_hermanas, clase,
			 n_cuadros > 1 ? "   animada: " : "", "");
	else
		snprintf(t, sizeof(t), "Mirador: el visor de im\xA0genes de Hispa-Pentium.");
	if (!aviso[0] && n_cuadros > 1) {
		char u[24];

		snprintf(u, sizeof(u), "%d cuadros", n_cuadros);
		strcat(t, u);
	}
	if (diapos && !aviso[0])
		strcat(t, "   (diapositivas: F5 para)");
	ui_texto(8, ui_al - ALTO_ESTADO + 5, t, (ui_an - 16) / ANCHO_LETRA, C_SUAVE);
}

static void acerca(int factor, int mx, int my)
{
	int cw = ui_an, ch = completa ? ui_al : ui_al - ALTO_BARRA - ALTO_ESTADO, ox, oy, nz, ix, iy;

	if (!hay_imagen)
		return;
	if (ajustar)
		zoom = zoom_ajustado(cw, ch);
	origen(cw, ch, zoom, &ox, &oy);
	if (mx < 0) {
		mx = cw / 2;
		my = ch / 2;
	}
	/* el punto de la imagen bajo la flecha, que siga bajo la flecha */
	ix = (int)((int64_t)(mx - ox) * 1000 / zoom);
	iy = (int)((int64_t)(my - oy) * 1000 / zoom);
	nz = factor > 0 ? zoom * 5 / 4 : zoom * 4 / 5;
	if (factor > 0 && nz == zoom)
		nz++;
	if (nz < 10)
		nz = 10;
	if (nz > 32000)
		nz = 32000;
	ajustar = 0;
	zoom = nz;
	centro_x = ix - (int)((int64_t)(mx - cw / 2) * 1000 / zoom);
	centro_y = iy - (int)((int64_t)(my - ch / 2) * 1000 / zoom);
}

/* «Usar como fondo de escritorio»: la imagen llenando la pantalla, al núcleo */
static void al_tapiz(int poner)
{
	struct { uint32_t ancho, alto; uint32_t puntos; } it;
	uint32_t W = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), H = sc(SC_DATO, DATO_ALTO_PANTALLA, 0, 0), r;
	uint32_t *p;
	int z, ox, oy, i;
	int64_t a, b;

	if (!poner) {
		r = sc(SC_TAPIZ, 0, 0, 0);
		snprintf(aviso, sizeof(aviso), ES_ERROR(r) ? "No se ha podido quitar el fondo." : "Vuelve el fondo de siempre.");
		return;
	}
	if (!hay_imagen || W == 0 || H == 0 || ES_ERROR(W) || W > 4096 || H > 4096)
		return;
	p = malloc((size_t)W * H * 4);
	if (!p) {
		snprintf(aviso, sizeof(aviso), "No hay memoria para hacer el fondo.");
		return;
	}
	/* que la llene (lo que sobre, cortado por igual a los dos lados) */
	a = (int64_t)W * 1000 / iw;
	b = (int64_t)H * 1000 / ih;
	z = (int)(a > b ? a : b);
	if ((int64_t)iw * z / 1000 < W || (int64_t)ih * z / 1000 < H)
		z++;
	ox = (int)(((int64_t)W - (int64_t)iw * z / 1000) / 2);
	oy = (int)(((int64_t)H - (int64_t)ih * z / 1000) / 2);
	pinta_en(p, (int)W, (int)H, z, ox, oy, 1, 0);
	for (i = 0; i < (int)(W * H); i++)
		p[i] &= 0xFFFFFF;
	it.ancho = W;
	it.alto = H;
	it.puntos = (uint32_t)p;
	r = sc(SC_TAPIZ, 1, (uint32_t)&it, 0);
	free(p);
	if (ES_ERROR(r))
		snprintf(aviso, sizeof(aviso), "No se ha podido poner de fondo (\xA8" "hay escritorio? \xA8se puede escribir en el disco del sistema?).");
	else
		snprintf(aviso, sizeof(aviso), "Puesta de fondo de escritorio (%u x %u).", W, H);
	serie("mirador: de fondo de escritorio, %u x %u: %s\n", W, H, ES_ERROR(r) ? "no" : "bien");
}

static void pantalla_completa(int si)
{
	struct pide_ventana pv;
	uint32_t W = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), H = sc(SC_DATO, DATO_ALTO_PANTALLA, 0, 0), v;

	if (si == completa)
		return;
	if (si) {
		if (!hay_imagen || ES_ERROR(W) || !W)
			return;
		pv.nombre = "Mirador";
		pv.x = 0;
		pv.y = 0;
		pv.ancho = (int32_t)W;
		pv.alto = (int32_t)H;
		pv.marcas = V_COMPLETA | V_ENCIMA | V_SIN_MARCO;
		v = sc(SC_VENTANAS_W32, 0, 0, (uint32_t)&pv);
		if (ES_ERROR(v) || !v)
			return;
		sc(SC_VENTANAS_W32, 3, v, 0);	/* delante, y la activa */
		v_completa = v;
		an_normal = ui_an;
		al_normal = ui_al;
		ui_v = v;
		ui_an = (int)W;
		ui_al = (int)H;
		completa = 1;
		serie("mirador: pantalla completa, %u x %u\n", W, H);
	} else {
		sc(SC_CERRAR_VENTANA, v_completa, 0, 0);
		v_completa = 0;
		ui_v = v_normal;
		ui_an = an_normal;
		ui_al = al_normal;
		completa = 0;
		serie("mirador: fuera de la pantalla completa\n");
	}
}

static void propiedades(void)
{
	char t[400];

	if (!hay_imagen)
		return;
	snprintf(t, sizeof(t), "%s\n%d x %d puntos (%s)%s\n%s", ruta, iw, ih, clase, n_cuadros > 1 ? ", animada" : "",
		 n_hermanas > 1 ? "Con las flechas, las dem\xA0s im\xA0genes de la carpeta." : "");
	ui_aviso("Propiedades", t);
}

static void abrir_otra(void)
{
	static char r[300];

	if (!ui_pide_texto("Abrir", "La ruta de la imagen (por ejemplo, C:\\FOTOS\\PLAYA.JPG):", r, sizeof(r), 0) || !r[0])
		return;
	abre(r);
	mira_carpeta();
}

static void menu_mas(int x, int y)
{
	ui_menu(x, y, "Girar a la izquierda|Girar a la derecha|-|Usar como fondo de escritorio|Quitar el fondo de escritorio|-|Abrir...|Propiedades",
		hay_imagen ? 0 : (1 | 2 | 8 | 128), M_MAS);
}

static void elegido(uint32_t op)
{
	switch (op) {
	case 0: gira(0); break;
	case 1: gira(1); break;
	case 3: al_tapiz(1); break;
	case 4: al_tapiz(0); break;
	case 6: abrir_otra(); break;
	case 7: propiedades(); break;
	}
}

static void diapositivas(int si)
{
	diapos = si && n_hermanas > 0;
	ultimo_cambio = ui_tics();
	serie("mirador: diapositivas %s\n", diapos ? "en marcha" : "paradas");
}

static void boton(int id)
{
	aviso[0] = 0;
	switch (id) {
	case B_ANTES: otra(-1); break;
	case B_DESPUES: otra(1); break;
	case B_MENOS: acerca(-1, -1, -1); break;
	case B_MAS: acerca(1, -1, -1); break;
	case B_AJUSTAR: ajustar = 1; break;
	case B_UNO: ajustar = 0; zoom = 1000; break;
	case B_GIRAR: gira(1); break;
	case B_DIAPOS: diapositivas(!diapos); break;
	case B_COMPLETA: pantalla_completa(1); break;
	case B_OTRAS: menu_mas(ui_an - 300, ALTO_BARRA); break;
	}
}

static int tecla(uint32_t k, uint32_t b)
{
	aviso[0] = 0;
	switch (k) {
	case T_IZQUIERDA: case T_RE_PAG: case T_BORRAR: otra(-1); break;
	case T_DERECHA: case T_AV_PAG: case ' ': otra(1); break;
	case T_INICIO: otra(-1000); break;
	case T_FIN: otra(1000); break;
	case T_ARRIBA: centro_y -= 40 * 1000 / (zoom ? zoom : 1000); break;
	case T_ABAJO: centro_y += 40 * 1000 / (zoom ? zoom : 1000); break;
	case '+': case '=': acerca(1, -1, -1); break;
	case '-': acerca(-1, -1, -1); break;
	case '0': case 'a': case 'A': ajustar = 1; break;
	case '1': ajustar = 0; zoom = 1000; break;
	case 'r': gira(!(b & 1)); break;
	case 'R': gira(0); break;
	case 'l': case 'L': gira(0); break;
	case T_F5: diapositivas(!diapos); break;
	case T_F11: case 'f': case 'F': case 10: case 13: pantalla_completa(!completa); break;
	case T_ESC:
		if (completa)
			pantalla_completa(0);
		else if (diapos)
			diapositivas(0);
		break;
	default:
		return 0;
	}
	return 1;
}

int principal(void)
{
	static char args[400];
	struct suceso s;
	uint32_t n, alto = sc(SC_DATO, DATO_ALTO_LIBRE, 0, 0), ancho = sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0);
	int an = 740, al = 500, px = 0, py = 0, arrastra = 0, cx0 = 0, cy0 = 0;
	char *argv[4];

	n = sc(SC_ARGUMENTOS, (uint32_t)args, sizeof(args) - 1, 0);
	if (ES_ERROR(n))
		n = 0;
	args[n < sizeof(args) ? n : sizeof(args) - 1] = 0;
	if (ancho && !ES_ERROR(ancho) && ancho < 780)
		an = (int)ancho - 40;
	if (alto && !ES_ERROR(alto) && alto < 540)
		al = (int)alto - 40;
	if (ui_ventana("Mirador", an, al)) {
		static const char m[] = "mirador: no hay escritorio (o no hay memoria para la ventana)\r\n";

		sc(SC_ESCRIBIR, (uint32_t)m, sizeof(m) - 1, 0);
		return 1;
	}
	v_normal = ui_v;
	sc(SC_COLOCAR, ui_v, 0xFFFFFFFFu, 0);
	ui_al_repintar(pinta);
	if (trocea(args, argv, 4) > 0) {
		/* la ruta entera (el explorador la da sin la letra, con la unidad ya puesta) */
		char r[300];
		uint32_t u = sc(SC_UNIDAD, 0, 0, 0);

		if (argv[0][0] == '\\')
			snprintf(r, sizeof(r), "%c:%s", (char)(u >= 'A' && u <= 'Z' ? u : 'C'), argv[0]);
		else
			snprintf(r, sizeof(r), "%s", argv[0]);
		abre(r);
		mira_carpeta();
	}
	for (;;) {
		int hay, espera = diapos || n_cuadros > 1;

		pinta();
		for (;;) {
			uint32_t t = ui_tics(), hz = ui_hz();

			hay = ui_suceso(&s, !espera);
			if (hay)
				break;
			if (!espera)
				return 0;
			if (diapos && t - ultimo_cambio >= 4 * hz) {
				ultimo_cambio = t;
				otra(1);
				break;
			}
			if (n_cuadros > 1 && (t - ultimo_cuadro) * 1000 >= (uint32_t)esperas[cuadro] * hz) {
				ultimo_cuadro = t;
				cuadro = (cuadro + 1) % n_cuadros;
				suelta_mip();
				break;
			}
			sc(SC_DORMIR, 1, 0, 0);
		}
		if (!hay)
			continue;
		switch (s.clase) {
		case EV_CERRAR:
			if (completa)
				pantalla_completa(0);
			else
				return 0;
			break;
		case EV_TAMANO:
			ui_an = (int)s.a;
			ui_al = (int)s.b;
			break;
		case EV_TECLA:
			tecla(s.a, s.b);
			break;
		case EV_RUEDA:
			aviso[0] = 0;
			acerca((int8_t)(s.c & 0xFF) > 0 ? 1 : -1, (int)(int32_t)s.a, (int)(int32_t)s.b - (completa ? 0 : ALTO_BARRA));
			break;
		case EV_RATON_BAJA: {
			int x = (int)(int32_t)s.a, y = (int)(int32_t)s.b, id = completa ? 0 : ui_en_zona(x, y);

			if (id) {
				boton(id);
				break;
			}
			if (s.c & BOTON_DERECHO) {
				menu_mas(x, y);
				break;
			}
			if ((s.c >> 16) >= 2 && hay_imagen) {
				pantalla_completa(!completa);
				break;
			}
			arrastra = 1;
			px = x;
			py = y;
			cx0 = centro_x;
			cy0 = centro_y;
			continue;
		}
		case EV_RATON_MUEVE:
			if (!arrastra || !(s.c & BOTON_IZQUIERDO) || ajustar)
				continue;
			centro_x = cx0 - (int)((int64_t)((int32_t)s.a - px) * 1000 / zoom);
			centro_y = cy0 - (int)((int64_t)((int32_t)s.b - py) * 1000 / zoom);
			break;
		case EV_RATON_SUBE:
			arrastra = 0;
			continue;
		case EV_MENU:
			if (s.b == M_MAS && s.a != SIN_ELEGIR)
				elegido(s.a);
			break;
		default:
			continue;
		}
	}
}
