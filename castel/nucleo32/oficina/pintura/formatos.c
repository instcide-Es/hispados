/* formatos.c - Pintura 2: leer y escribir las imágenes (pintura/formatos.h).
 * Leer PNG, JPG, BMP, GIF y TGA con stb_image (oficina/comun/imagen.c);
 * ICO, CUR y PCX, con lectores propios (y los BMP con RLE8 o RLE4, que
 * stb_image no lee, se descomprimen aquí antes). Escribir PNG y JPG con
 * stb_image_write (terceros/stb, dominio público o MIT; el Flate de los PNG,
 * con miniz, MIT); BMP, GIF (con su LZW) y PCX, aquí. Lo que llega de fuera
 * no es de fiar: todos los campos se miran y todo va con topes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "formatos.h"
#include "imagen.h"
#include "miniz_cfg.h"

/* ---- stb_image_write (PNG y JPG), con el compresor de miniz ---- */
static unsigned char *comprime_png(unsigned char *d, int n, int *sal, int nivel)
{
	size_t k = 0;
	void *z;
	u8 *r;
	int fl = (int)tdefl_create_comp_flags_from_zip_params(nivel < 1 ? 6 : nivel > 9 ? 9 : nivel, 15, 0);

	*sal = 0;
	if (n < 0)
		return NULL;
	z = tdefl_compress_mem_to_heap(d, (size_t)n, &k, fl);
	if (!z)
		return NULL;
	/* (miniz pide con malloc; stb_image_write suelta con so_suelta) */
	r = k < 0x7FFFFFFF ? so_pide(k ? k : 1) : NULL;
	if (r) {
		memcpy(r, z, k);
		*sal = (int)k;
	}
	mz_free(z);
	return r;
}

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(n) so_pide(n)
#define STBIW_REALLOC(p, n) so_repide(p, n)
#define STBIW_FREE(p) so_suelta(p)
#define STBIW_MEMMOVE memmove
#define STBIW_ZLIB_COMPRESS comprime_png
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "stb_image_write.h"
#pragma GCC diagnostic pop

/* ---- lo de siempre ---- */
static void pon_err(char *err, u32 cabe, const char *f, ...)
{
	va_list a;

	if (!err || !cabe)
		return;
	va_start(a, f);
	vfmt(err, cabe, f, a);
	va_end(a);
}

static u32 le16(const u8 *p)
{
	return (u32)p[0] | (u32)p[1] << 8;
}

static u32 le32(const u8 *p)
{
	return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}

static void pon16(u8 *p, u32 v)
{
	p[0] = (u8)v;
	p[1] = (u8)(v >> 8);
}

static void pon32(u8 *p, u32 v)
{
	p[0] = (u8)v;
	p[1] = (u8)(v >> 8);
	p[2] = (u8)(v >> 16);
	p[3] = (u8)(v >> 24);
}

/* los puntos de una imagen de a x b (0 si no vale o pasa del tope) */
static u32 puntos(int a, int b)
{
	if (a <= 0 || b <= 0 || a > FI_MAX_LADO || b > FI_MAX_LADO || (u64)a * (u64)b > FI_MAX_PUNTOS)
		return 0;
	return (u32)a * (u32)b;
}

static const char *const nombres[] = { "", "PNG", "JPG", "BMP", "GIF", "PCX", "ICO", "TGA" };
static const char *const extensiones[] = { "", "png", "jpg", "bmp", "gif", "pcx", "ico", "tga" };

const char *fi_nombre(int f)
{
	return f > FI_NADA && f <= FI_TGA ? nombres[f] : "?";
}

const char *fi_extension(int f)
{
	return f > FI_NADA && f <= FI_TGA ? extensiones[f] : "";
}

int fi_se_escribe(int f)
{
	return f == FI_PNG || f == FI_JPG || f == FI_BMP || f == FI_GIF || f == FI_PCX;
}

int fi_por_extension(const char *ruta)
{
	char e[8];

	if (!ruta)
		return FI_NADA;
	extension(ruta, e);
	if (iguales(e, "PNG"))
		return FI_PNG;
	if (iguales(e, "JPG") || iguales(e, "JPEG") || iguales(e, "JPE"))
		return FI_JPG;
	if (iguales(e, "BMP"))
		return FI_BMP;
	if (iguales(e, "GIF"))
		return FI_GIF;
	if (iguales(e, "PCX"))
		return FI_PCX;
	if (iguales(e, "ICO") || iguales(e, "CUR"))
		return FI_ICO;
	if (iguales(e, "TGA"))
		return FI_TGA;
	return FI_NADA;
}

static int es_pcx(const u8 *d, u32 n)
{
	return n >= 128 && d[0] == 0x0A && d[1] <= 5 && d[2] <= 1 && (d[3] == 1 || d[3] == 2 || d[3] == 4 || d[3] == 8);
}

static int es_ico(const u8 *d, u32 n)
{
	u32 k;

	if (n < 22 || d[0] || d[1] || (d[2] != 1 && d[2] != 2) || d[3])
		return 0;
	k = le16(d + 4);
	if (!k || 6 + 16 * k > n)
		return 0;
	/* (la primera, que empiece detrás del directorio y dentro del archivo) */
	return le32(d + 18) >= 6 + 16 * k && le32(d + 18) < n;
}

int fi_por_contenido(const u8 *d, u32 n)
{
	if (!d)
		return FI_NADA;
	switch (imagen_tipo(d, n)) {
	case IMAGEN_PNG:
		return FI_PNG;
	case IMAGEN_JPG:
		return FI_JPG;
	case IMAGEN_BMP:
		return FI_BMP;
	case IMAGEN_GIF:
		return FI_GIF;
	}
	if (es_pcx(d, n))
		return FI_PCX;
	if (es_ico(d, n))
		return FI_ICO;
	return FI_NADA;
}

int fi_hay_transparencia(const u32 *px, int an, int al)
{
	u32 i, np = puntos(an, al);

	for (i = 0; i < np; i++)
		if (px[i] >> 24 != 255)
			return 1;
	return 0;
}

/* ================================================================
 * Leer
 * ================================================================ */

/* lo que dice la cabecera que mide (para decir «mide más» aunque stb_image no
 * lo quiera leer: pasa de 16384); 0 si no se sabe */
static int lado_declarado(int f, const u8 *d, u32 n, u32 *a, u32 *b)
{
	u32 i;

	switch (f) {
	case FI_PNG:
		if (n < 24)
			return 0;
		*a = (u32)d[16] << 24 | (u32)d[17] << 16 | (u32)d[18] << 8 | d[19];
		*b = (u32)d[20] << 24 | (u32)d[21] << 16 | (u32)d[22] << 8 | d[23];
		return 1;
	case FI_GIF:
		if (n < 10)
			return 0;
		*a = le16(d + 6);
		*b = le16(d + 8);
		return 1;
	case FI_BMP:
		if (n < 26)
			return 0;
		if (le32(d + 14) == 12) {
			*a = le16(d + 18);
			*b = le16(d + 20);
		} else {
			*a = le32(d + 18);
			*b = le32(d + 22);
			if (*b & 0x80000000u)
				*b = 0u - *b;
		}
		return 1;
	case FI_JPG:
		for (i = 2; i + 9 <= n;) {
			u32 m;

			if (d[i] != 0xFF) {
				i++;
				continue;
			}
			m = d[i + 1];
			if (m == 0xFF || m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) {
				i += m == 0xFF ? 1 : 2;
				continue;
			}
			if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
				*b = (u32)d[i + 5] << 8 | d[i + 6];
				*a = (u32)d[i + 7] << 8 | d[i + 8];
				return 1;
			}
			i += 2 + ((u32)d[i + 2] << 8 | d[i + 3]);
		}
		return 0;
	case FI_TGA:
		if (n < 18)
			return 0;
		*a = le16(d + 12);
		*b = le16(d + 14);
		return 1;
	}
	return 0;
}

static int demasiado(u32 a, u32 b)
{
	return a > FI_MAX_LADO || b > FI_MAX_LADO || (u64)a * b > FI_MAX_PUNTOS;
}

/* (BMP) los de RLE8 y RLE4, que stb_image no lee: a uno sin comprimir (de
 * so_pide, en *m su largo), con la misma cabecera y paleta; NULL si no vale.
 * Lo que no se pinta (los saltos), con el color 0 */
static u8 *bmp_sin_rle(const u8 *d, u32 n, u32 *m, int *enorme)
{
	u32 hsz, comp, bpp, off, fila, x = 0, y = 0, p, todo;
	i32 w, h;
	u8 *r, *o;

	if (n < 54)
		return NULL;
	hsz = le32(d + 14);
	w = (i32)le32(d + 18);
	h = (i32)le32(d + 22);
	bpp = le16(d + 28);
	comp = le32(d + 30);
	off = le32(d + 10);
	if ((hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124) || w <= 0 || h <= 0 ||
	    !((comp == 1 && bpp == 8) || (comp == 2 && bpp == 4)) || off < 14 + hsz || off > n || off - 14 - hsz > 1024)
		return NULL;
	if (!puntos(w, h)) {
		*enorme = 1;
		return NULL;
	}
	fila = ((u32)w * bpp + 31) / 32 * 4;
	todo = off + fila * (u32)h;
	r = so_pide(todo);
	if (!r)
		return NULL;
	memcpy(r, d, off);
	memset(r + off, 0, todo - off);
	pon32(r + 2, todo);
	pon32(r + 30, 0);
	pon32(r + 34, 0);
	o = r + off;
#define BMP_PON(v) do { \
		if (x < (u32)w && y < (u32)h) { \
			if (bpp == 8) \
				o[y * fila + x] = (u8)(v); \
			else \
				o[y * fila + x / 2] |= (u8)(((v) & 15) << (x & 1 ? 0 : 4)); \
		} \
		x++; \
	} while (0)
	for (p = off; p + 1 < n && y < (u32)h;) {
		u32 c = d[p], v = d[p + 1], i;

		p += 2;
		if (c) {
			for (i = 0; i < c && x < (u32)w; i++)
				BMP_PON(bpp == 8 ? v : i & 1 ? v & 15 : v >> 4);
			continue;
		}
		if (v == 0) {
			x = 0;
			y++;
		} else if (v == 1) {
			break;
		} else if (v == 2) {
			if (p + 1 >= n)
				break;
			x += d[p];
			y += d[p + 1];
			p += 2;
		} else {
			u32 oct = bpp == 8 ? v : (v + 1) / 2;

			for (i = 0; i < v && p + (bpp == 8 ? i : i / 2) < n; i++)
				BMP_PON(bpp == 8 ? d[p + i] : i & 1 ? d[p + i / 2] & 15 : d[p + i / 2] >> 4);
			p += oct + (oct & 1);
		}
	}
#undef BMP_PON
	*m = todo;
	return r;
}

/* PNG, JPG, BMP, GIF y TGA: con stb_image (comun/imagen.c) */
static u32 *lee_stb(int f, const u8 *d, u32 n, int *an, int *al, char *err, u32 cabe)
{
	int a = 0, b = 0, ok;
	u32 da, db, *r;

	if (f == FI_BMP && n >= 34 && (le32(d + 30) == 1 || le32(d + 30) == 2)) {
		int enorme = 0;
		u32 m = 0;
		u8 *t = bmp_sin_rle(d, n, &m, &enorme);

		if (!t) {
			if (enorme)
				pon_err(err, cabe, "la imagen mide más de %d x %d", FI_MAX_LADO, FI_MAX_LADO);
			else
				pon_err(err, cabe, "el BMP comprimido (RLE) está dañado");
			return NULL;
		}
		r = lee_stb(f, t, m, an, al, err, cabe);
		so_suelta(t);
		return r;
	}
	ok = f == FI_TGA ? imagen_mide_tga(d, n, &a, &b) : imagen_mide(d, n, &a, &b);
	if (!ok) {
		if (lado_declarado(f, d, n, &da, &db) && demasiado(da, db))
			pon_err(err, cabe, "la imagen mide %u x %u: más de %d x %d", da, db, FI_MAX_LADO, FI_MAX_LADO);
		else
			pon_err(err, cabe, "el archivo %s está dañado o es de un tipo que Pintura no sabe leer", fi_nombre(f));
		return NULL;
	}
	if (!puntos(a, b)) {
		pon_err(err, cabe, "la imagen mide %d x %d: más de %d x %d", a, b, FI_MAX_LADO, FI_MAX_LADO);
		return NULL;
	}
	r = f == FI_TGA ? imagen_lee_tga(d, n, an, al, FI_MAX_PUNTOS) : imagen_lee_max(d, n, an, al, FI_MAX_PUNTOS);
	if (!r) {
		pon_err(err, cabe, "no se puede leer el %s: está dañado o no hay memoria", fi_nombre(f));
		return NULL;
	}
	if (!puntos(*an, *al)) {
		so_suelta(r);
		pon_err(err, cabe, "el archivo %s está dañado", fi_nombre(f));
		return NULL;
	}
	return r;
}

/* ---- ICO y CUR: la imagen más grande (y de más colores) que llevan ---- */

/* una de las imágenes del icono (d, n: lo suyo). 0: no se sabe; -1: es enorme */
static u32 *ico_una(const u8 *d, u32 n, int *an, int *al, int *enorme)
{
	u32 cab, bpp, comp, usados, paleta, mascaras, x, y, todo_opaco, np;
	i32 bw, bh2, bh;
	u64 fila, xor_n, mfila, hasta, i;
	u8 *bmp;
	u32 *img;
	int a, b;

	/* un PNG dentro (los de 256 x 256) */
	if (n >= 8 && imagen_tipo(d, n) == IMAGEN_PNG) {
		u32 da, db;

		if (!imagen_mide(d, n, &a, &b)) {
			if (lado_declarado(FI_PNG, d, n, &da, &db) && demasiado(da, db))
				*enorme = 1;
			return NULL;
		}
		if (!puntos(a, b)) {
			*enorme = 1;
			return NULL;
		}
		img = imagen_lee_max(d, n, an, al, FI_MAX_PUNTOS);
		if (img && !puntos(*an, *al)) {
			so_suelta(img);
			img = NULL;
		}
		return img;
	}
	/* un BMP sin su cabecera de archivo, de doble alto (la imagen y su máscara AND) */
	if (n < 40)
		return NULL;
	cab = le32(d);
	if (cab != 40 && cab != 56 && cab != 108 && cab != 124)
		return NULL;
	if (n < cab)
		return NULL;
	bw = (i32)le32(d + 4);
	bh2 = (i32)le32(d + 8);
	bpp = le16(d + 14);
	comp = le32(d + 16);
	if (bw <= 0 || bh2 <= 1 || le16(d + 12) != 1)
		return NULL;
	bh = bh2 / 2;
	if (!puntos(bw, bh)) {
		*enorme = 1;
		return NULL;
	}
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
		return NULL;
	if (comp != 0 && !(comp == 3 && (bpp == 16 || bpp == 32)))
		return NULL;
	usados = le32(d + 32);
	paleta = bpp <= 8 ? 4 * (usados && usados <= (1u << bpp) ? usados : 1u << bpp) : 0;
	mascaras = comp == 3 && cab == 40 ? 12 : 0;
	fila = ((u64)bw * bpp + 31) / 32 * 4;
	xor_n = fila * (u64)bh;
	mfila = ((u64)bw + 31) / 32 * 4;
	hasta = (u64)cab + mascaras + paleta + xor_n;
	if (hasta > n)
		return NULL;
	/* el BMP en la memoria, con su cabecera de archivo y el alto de la imagen sola */
	bmp = so_pide((size_t)hasta + 14);
	if (!bmp)
		return NULL;
	memcpy(bmp + 14, d, (size_t)hasta);
	bmp[0] = 'B';
	bmp[1] = 'M';
	pon32(bmp + 2, (u32)hasta + 14);
	pon32(bmp + 6, 0);
	pon32(bmp + 10, 14 + cab + mascaras + paleta);
	pon32(bmp + 14 + 8, (u32)bh);
	pon32(bmp + 14 + 20, 0);	/* (sin el tamaño de la imagen: lo calcula) */
	img = imagen_lee_max(bmp, (u32)hasta + 14, an, al, FI_MAX_PUNTOS);
	so_suelta(bmp);
	if (!img)
		return NULL;
	if (*an != bw || *al != bh) {
		so_suelta(img);
		return NULL;
	}
	/* la máscara (de abajo arriba): un 1 es transparente. En los de 32 bits
	 * manda el alfa, salvo que no lo traigan (todo opaco) */
	np = (u32)bw * (u32)bh;
	todo_opaco = 1;
	if (bpp == 32)
		for (i = 0; i < np && todo_opaco; i++)
			todo_opaco = img[i] >> 24 == 255;
	if (todo_opaco && hasta + mfila * (u64)bh <= n) {
		const u8 *m = d + hasta;

		for (y = 0; y < (u32)bh; y++)
			for (x = 0; x < (u32)bw; x++)
				if (m[(u64)((u32)bh - 1 - y) * mfila + x / 8] & (0x80 >> (x & 7)))
					img[(u64)y * (u32)bw + x] &= 0x00FFFFFFu;
	}
	return img;
}

static u32 *lee_ico(const u8 *d, u32 n, int *an, int *al, char *err, u32 cabe)
{
	u32 k, cuantos, mejor = 0, mejor_area = 0, mejor_bpp = 0, intentos;
	int enorme = 0;
	u32 *img;

	if (!es_ico(d, n)) {
		pon_err(err, cabe, "no es un icono (ICO o CUR) bien hecho");
		return NULL;
	}
	cuantos = le16(d + 4);
	for (k = 0; k < cuantos; k++) {
		const u8 *e = d + 6 + k * 16;
		u32 a = (u32)(e[0] ? e[0] : 256) * (e[1] ? e[1] : 256), bpp = le16(e + 6);

		if (a > mejor_area || (a == mejor_area && bpp > mejor_bpp)) {
			mejor = k;
			mejor_area = a;
			mejor_bpp = bpp;
		}
	}
	/* la mejor; si no se puede, las otras (como mucho 64) */
	for (intentos = 0; intentos <= cuantos && intentos < 65; intentos++) {
		u32 j = intentos == 0 ? mejor : intentos - 1, off, tam;
		const u8 *e;

		if (intentos && j == mejor)
			continue;
		if (j >= cuantos)
			break;
		e = d + 6 + j * 16;
		tam = le32(e + 8);
		off = le32(e + 12);
		if (off >= n)
			continue;
		if (tam > n - off)
			tam = n - off;		/* (no se fía: lo que haya) */
		img = ico_una(d + off, tam, an, al, &enorme);
		if (img)
			return img;
	}
	if (enorme)
		pon_err(err, cabe, "la imagen del icono mide más de %d x %d", FI_MAX_LADO, FI_MAX_LADO);
	else
		pon_err(err, cabe, "el icono está dañado o no lleva ninguna imagen que Pintura sepa leer");
	return NULL;
}

/* ---- PCX (PC Paintbrush) ----
 * Un plano de 8 bits (256 colores con la paleta del final, o grises sin
 * ella), tres planos de 8 bits (color verdadero), cuatro (con alfa), un plano
 * de un bit (blanco y negro), de dos a cuatro planos de un bit y un plano de
 * 2 o 4 bits (16 colores, con la paleta de la cabecera). Con RLE o sin
 * comprimir; las tiradas pueden pasar de una fila a otra; si se acaba el
 * archivo, lo que falta va a 0. */
static u32 *lee_pcx(const u8 *d, u32 n, int *an, int *al, char *err, u32 cabe)
{
	u32 bits, planos, bpl, x0, y0, x1, y1, w, h, linea, fin, i, p, rep, val, x, y, np, mask;
	u32 pal[256];
	u8 *fila;
	u32 *px;
	int rle;

	if (!es_pcx(d, n)) {
		pon_err(err, cabe, "no es un PCX bien hecho");
		return NULL;
	}
	rle = d[2];
	bits = d[3];
	planos = d[65];
	bpl = le16(d + 66);
	x0 = le16(d + 4);
	y0 = le16(d + 6);
	x1 = le16(d + 8);
	y1 = le16(d + 10);
	if (x1 < x0 || y1 < y0) {
		pon_err(err, cabe, "el PCX está dañado (sus medidas no valen)");
		return NULL;
	}
	w = x1 - x0 + 1;
	h = y1 - y0 + 1;
	np = puntos((int)w, (int)h);
	if (!np) {
		pon_err(err, cabe, "la imagen mide %u x %u: más de %d x %d", w, h, FI_MAX_LADO, FI_MAX_LADO);
		return NULL;
	}
	if (!((bits == 8 && (planos == 1 || planos == 3 || planos == 4)) || (bits == 1 && planos >= 1 && planos <= 4) ||
	      ((bits == 2 || bits == 4) && planos == 1))) {
		pon_err(err, cabe, "Pintura no sabe leer los PCX de %u bits y %u planos", bits, planos);
		return NULL;
	}
	if (bpl == 0 || (u64)bpl * 8 < (u64)w * bits) {
		pon_err(err, cabe, "el PCX está dañado (los octetos por fila no llegan)");
		return NULL;
	}
	linea = bpl * planos;
	fin = n;
	/* los colores */
	memset(pal, 0, sizeof pal);
	if (bits == 8 && planos == 1) {
		if (n >= 128 + 769 && d[n - 769] == 0x0C) {
			for (i = 0; i < 256; i++)
				pal[i] = 0xFF000000u | (u32)d[n - 768 + 3 * i] << 16 | (u32)d[n - 767 + 3 * i] << 8 | d[n - 766 + 3 * i];
			fin = n - 769;
		} else {
			for (i = 0; i < 256; i++)
				pal[i] = 0xFF000000u | i << 16 | i << 8 | i;
		}
	} else if (bits == 1 && planos == 1 && !memcmp(d + 16, d + 19, 3)) {
		/* (blanco y negro; si la cabecera trae dos colores, esos: como GIMP) */
		pal[0] = 0xFF000000u;
		pal[1] = 0xFFFFFFFFu;
	} else if (bits < 8) {
		for (i = 0; i < 16; i++)
			pal[i] = 0xFF000000u | (u32)d[16 + 3 * i] << 16 | (u32)d[17 + 3 * i] << 8 | d[18 + 3 * i];
	}
	px = so_pide((size_t)np * 4);
	fila = so_pide(linea);
	if (!px || !fila) {
		so_suelta(px);
		so_suelta(fila);
		pon_err(err, cabe, "no hay memoria para una imagen de %u x %u", w, h);
		return NULL;
	}
	p = 128;
	rep = 0;
	val = 0;
	mask = (1u << bits) - 1;
	for (y = 0; y < h; y++) {
		u32 *o = px + (u64)y * w;

		/* los planos de la fila, descomprimidos */
		for (i = 0; i < linea;) {
			u32 c;

			if (rep) {
				fila[i++] = (u8)val;
				rep--;
				continue;
			}
			if (p >= fin) {
				fila[i++] = 0;
				continue;
			}
			c = d[p++];
			if (rle && (c & 0xC0) == 0xC0) {
				rep = c & 0x3F;
				val = p < fin ? d[p++] : 0;
				continue;
			}
			fila[i++] = (u8)c;
		}
		/* a la imagen */
		if (bits == 8 && planos == 1) {
			for (x = 0; x < w; x++)
				o[x] = pal[fila[x]];
		} else if (bits == 8) {
			for (x = 0; x < w; x++)
				o[x] = (planos == 4 ? (u32)fila[3 * bpl + x] << 24 : 0xFF000000u) |
				       (u32)fila[x] << 16 | (u32)fila[bpl + x] << 8 | fila[2 * bpl + x];
		} else if (bits == 1) {
			for (x = 0; x < w; x++) {
				u32 k, ind = 0;

				for (k = 0; k < planos; k++)
					ind |= (u32)(fila[k * bpl + x / 8] >> (7 - (x & 7)) & 1) << k;
				o[x] = pal[ind];
			}
		} else {
			for (x = 0; x < w; x++) {
				u32 b = x * bits;

				o[x] = pal[fila[b >> 3] >> (8 - (b & 7) - bits) & mask];
			}
		}
	}
	so_suelta(fila);
	*an = (int)w;
	*al = (int)h;
	return px;
}

static u32 *lee_como(int f, const u8 *d, u32 n, int *an, int *al, char *err, u32 cabe)
{
	switch (f) {
	case FI_PNG:
	case FI_JPG:
	case FI_BMP:
	case FI_GIF:
	case FI_TGA:
		return lee_stb(f, d, n, an, al, err, cabe);
	case FI_PCX:
		return lee_pcx(d, n, an, al, err, cabe);
	case FI_ICO:
		return lee_ico(d, n, an, al, err, cabe);
	}
	return NULL;
}

u32 *fi_lee(const u8 *d, u32 n, int pista, int *an, int *al, char *err, u32 cabe)
{
	int f, a = 0, b = 0;
	u32 *r;
	char otro[8];

	if (err && cabe)
		err[0] = 0;
	*an = *al = 0;
	if (!d || !n) {
		pon_err(err, cabe, "el archivo está vacío");
		return NULL;
	}
	f = fi_por_contenido(d, n);
	if (f) {
		r = lee_como(f, d, n, &a, &b, err, cabe);
		if (r) {
			*an = a;
			*al = b;
			return r;
		}
		/* (un TGA puede empezar como un CUR: «00 00 02 00...») */
		if (pista && pista != f && (f == FI_ICO || f == FI_PCX)) {
			r = lee_como(pista, d, n, &a, &b, otro, sizeof otro);
			if (r) {
				*an = a;
				*al = b;
				if (err && cabe)
					err[0] = 0;
				return r;
			}
		}
		return NULL;
	}
	if (pista > FI_NADA && pista <= FI_TGA && pista != FI_PNG && pista != FI_JPG && pista != FI_BMP && pista != FI_GIF) {
		r = lee_como(pista, d, n, &a, &b, err, cabe);
		if (r) {
			*an = a;
			*al = b;
		}
		return r;
	}
	pon_err(err, cabe, "no es una imagen que Pintura sepa leer");
	return NULL;
}

/* ================================================================
 * Escribir
 * ================================================================ */

/* lo de stb_image_write, al búfer */
static void a_bufer(void *c, void *d, int n)
{
	if (n > 0)
		buf_pon(c, d, (u32)n);
}

/* sobre blanco */
static u32 sobre_blanco(u32 c)
{
	u32 a = c >> 24, r, g, b;

	if (a == 255)
		return c & 0xFFFFFFu;
	r = ((c >> 16 & 255) * a + 255 * (255 - a) + 127) / 255;
	g = ((c >> 8 & 255) * a + 255 * (255 - a) + 127) / 255;
	b = ((c & 255) * a + 255 * (255 - a) + 127) / 255;
	return r << 16 | g << 8 | b;
}

static int escribe_png(bufer *b, const u32 *px, int an, int al, u32 np)
{
	int alfa = fi_hay_transparencia(px, an, al), comp = alfa ? 4 : 3, ok;
	u8 *t = so_pide((size_t)np * comp), *q;
	u32 i;

	if (!t)
		return -1;
	for (i = 0, q = t; i < np; i++) {
		*q++ = (u8)(px[i] >> 16);
		*q++ = (u8)(px[i] >> 8);
		*q++ = (u8)px[i];
		if (alfa)
			*q++ = (u8)(px[i] >> 24);
	}
	ok = stbi_write_png_to_func(a_bufer, b, an, al, comp, t, an * comp);
	so_suelta(t);
	return ok ? 0 : -1;
}

static int escribe_jpg(bufer *b, const u32 *px, int an, int al, u32 np, int calidad)
{
	u8 *t = so_pide((size_t)np * 3), *q;
	u32 i;
	int ok;

	if (!t)
		return -1;
	for (i = 0, q = t; i < np; i++) {
		u32 c = sobre_blanco(px[i]);

		*q++ = (u8)(c >> 16);
		*q++ = (u8)(c >> 8);
		*q++ = (u8)c;
	}
	ok = stbi_write_jpg_to_func(a_bufer, b, an, al, 3, t, calidad < 1 ? 1 : calidad > 100 ? 100 : calidad);
	so_suelta(t);
	return ok ? 0 : -1;
}

/* BMP: 24 bits (BITMAPINFOHEADER), o 32 con alfa (BITMAPV5HEADER, BI_BITFIELDS) */
static int escribe_bmp(bufer *b, const u32 *px, int an, int al)
{
	int alfa = fi_hay_transparencia(px, an, al);
	u32 cab = alfa ? 124 : 40, fila = alfa ? (u32)an * 4 : ((u32)an * 3 + 3) & ~3u, x, y;
	u32 tam = fila * (u32)al;
	u8 h[14 + 124], *f;

	memset(h, 0, sizeof h);
	h[0] = 'B';
	h[1] = 'M';
	pon32(h + 2, 14 + cab + tam);
	pon32(h + 10, 14 + cab);
	pon32(h + 14, cab);
	pon32(h + 18, (u32)an);
	pon32(h + 22, (u32)al);		/* (de abajo arriba) */
	pon16(h + 26, 1);
	pon16(h + 28, alfa ? 32 : 24);
	pon32(h + 30, alfa ? 3 : 0);
	pon32(h + 34, tam);
	pon32(h + 38, 2835);		/* 72 puntos por pulgada */
	pon32(h + 42, 2835);
	if (alfa) {
		pon32(h + 54, 0x00FF0000u);
		pon32(h + 58, 0x0000FF00u);
		pon32(h + 62, 0x000000FFu);
		pon32(h + 66, 0xFF000000u);
		pon32(h + 70, 0x73524742u);	/* «sRGB» */
		pon32(h + 14 + 108, 4);		/* LCS_GM_IMAGES */
	}
	buf_pon(b, h, 14 + cab);
	f = so_pide(fila);
	if (!f)
		return -1;
	memset(f, 0, fila);
	for (y = (u32)al; y-- > 0;) {
		const u32 *s = px + (u64)y * (u32)an;
		u8 *q = f;

		for (x = 0; x < (u32)an; x++) {
			*q++ = (u8)s[x];
			*q++ = (u8)(s[x] >> 8);
			*q++ = (u8)(s[x] >> 16);
			if (alfa)
				*q++ = (u8)(s[x] >> 24);
		}
		buf_pon(b, f, fila);
	}
	so_suelta(f);
	return 0;
}

/* ---- los colores que hay (hasta 256): una tabla de dispersión pequeña ---- */
#define TC_TAM 1024
typedef struct {
	u32 clave[TC_TAM];	/* el color (0x00RRGGBB), o 0xFFFFFFFF si está libre */
	u16 ind[TC_TAM];
	u32 color[256];
	u32 n;
} tcolores;

static void tc_vacia(tcolores *t)
{
	memset(t->clave, 0xFF, sizeof t->clave);
	t->n = 0;
}

/* su índice; si no está, lo añade (-1 si ya hay 256) */
static int tc_pon(tcolores *t, u32 c)
{
	u32 h = (c * 2654435761u) >> 22;

	for (;; h = (h + 1) & (TC_TAM - 1)) {
		if (t->clave[h] == c)
			return t->ind[h];
		if (t->clave[h] == 0xFFFFFFFFu) {
			if (t->n >= 256)
				return -1;
			t->clave[h] = c;
			t->ind[h] = (u16)t->n;
			t->color[t->n] = c;
			return (int)t->n++;
		}
	}
}

/* ---- PCX ---- */
static void pcx_rle(bufer *b, const u8 *s, u32 n)
{
	u32 i = 0, k;

	while (i < n) {
		u8 v = s[i];

		for (k = 1; i + k < n && s[i + k] == v && k < 63; k++)
			;
		if (k > 1 || (v & 0xC0) == 0xC0)
			buf_c(b, (u8)(0xC0 | k));
		buf_c(b, v);
		i += k;
	}
}

static int escribe_pcx(bufer *b, const u32 *px, int an, int al, u32 np)
{
	u32 bpl = ((u32)an + 1) & ~1u, i, x, y, k;
	u8 h[128], *f;
	tcolores *t = so_pide(sizeof *t);
	int ocho = 1;

	if (!t)
		return -1;
	tc_vacia(t);
	for (i = 0; i < np && ocho; i++)
		ocho = tc_pon(t, sobre_blanco(px[i])) >= 0;
	memset(h, 0, sizeof h);
	h[0] = 0x0A;
	h[1] = 5;
	h[2] = 1;
	h[3] = 8;
	pon16(h + 8, (u32)an - 1);
	pon16(h + 10, (u32)al - 1);
	pon16(h + 12, 72);
	pon16(h + 14, 72);
	h[65] = ocho ? 1 : 3;
	pon16(h + 66, bpl);
	pon16(h + 68, 1);
	buf_pon(b, h, 128);
	f = so_pide((size_t)bpl * 3);
	if (!f) {
		so_suelta(t);
		return -1;
	}
	memset(f, 0, (size_t)bpl * 3);
	for (y = 0; y < (u32)al; y++) {
		const u32 *s = px + (u64)y * (u32)an;

		for (x = 0; x < (u32)an; x++) {
			u32 c = sobre_blanco(s[x]);

			if (ocho) {
				f[x] = (u8)tc_pon(t, c);
			} else {
				f[x] = (u8)(c >> 16);
				f[bpl + x] = (u8)(c >> 8);
				f[2 * bpl + x] = (u8)c;
			}
		}
		for (k = 0; k < (ocho ? 1u : 3u); k++)
			pcx_rle(b, f + k * bpl, bpl);
	}
	so_suelta(f);
	if (ocho) {
		u8 p[769];

		memset(p, 0, sizeof p);
		p[0] = 0x0C;
		for (i = 0; i < t->n; i++) {
			p[1 + 3 * i] = (u8)(t->color[i] >> 16);
			p[2 + 3 * i] = (u8)(t->color[i] >> 8);
			p[3 + 3 * i] = (u8)t->color[i];
		}
		buf_pon(b, p, sizeof p);
	}
	so_suelta(t);
	return 0;
}

/* ---- GIF: los colores (exactos, o por corte de la mediana) ---- */

/* corte de la mediana sobre un histograma de 5 bits por color (32768 cajitas) */
#define MC_B 5
#define MC_N (1u << (3 * MC_B))
#define MC_I(r, g, b) ((u32)(r) << (2 * MC_B) | (u32)(g) << MC_B | (u32)(b))
typedef struct {
	u32 n, s[3];		/* cuántos y la suma de cada color (de 8 bits) */
} mc_cajita;
typedef struct {
	u8 lo[3], hi[3];
	u32 n;
} mc_caja;

/* lo que hay en la caja: ajustar sus bordes a lo ocupado y contar */
static void mc_ajusta(const mc_cajita *h, mc_caja *c)
{
	u32 lo[3] = { 31, 31, 31 }, hi[3] = { 0, 0, 0 }, n = 0, r, g, b;

	for (r = c->lo[0]; r <= c->hi[0]; r++)
		for (g = c->lo[1]; g <= c->hi[1]; g++)
			for (b = c->lo[2]; b <= c->hi[2]; b++) {
				u32 k = h[MC_I(r, g, b)].n;

				if (!k)
					continue;
				n += k;
				if (r < lo[0]) lo[0] = r;
				if (r > hi[0]) hi[0] = r;
				if (g < lo[1]) lo[1] = g;
				if (g > hi[1]) hi[1] = g;
				if (b < lo[2]) lo[2] = b;
				if (b > hi[2]) hi[2] = b;
			}
	c->n = n;
	if (n)
		for (r = 0; r < 3; r++) {
			c->lo[r] = (u8)lo[r];
			c->hi[r] = (u8)hi[r];
		}
}

/* la paleta de hasta «max» colores para los puntos opacos (alfa >= 128) */
static u32 mediana(const u32 *px, u32 np, u32 *pal, u32 max)
{
	mc_cajita *h = so_pide(MC_N * sizeof *h);
	mc_caja *cj;
	u32 i, nc = 1, k;

	if (!h)
		return 0;
	cj = so_pide(256 * sizeof *cj);
	if (!cj) {
		so_suelta(h);
		return 0;
	}
	memset(h, 0, MC_N * sizeof *h);
	for (i = 0; i < np; i++) {
		u32 c = px[i];
		mc_cajita *e;

		if (c >> 24 < 128)
			continue;
		e = &h[MC_I(c >> 19 & 31, c >> 11 & 31, c >> 3 & 31)];
		e->n++;
		e->s[0] += c >> 16 & 255;
		e->s[1] += c >> 8 & 255;
		e->s[2] += c & 255;
	}
	cj[0].lo[0] = cj[0].lo[1] = cj[0].lo[2] = 0;
	cj[0].hi[0] = cj[0].hi[1] = cj[0].hi[2] = 31;
	mc_ajusta(h, &cj[0]);
	if (!cj[0].n)
		nc = 0;
	while (nc && nc < max) {
		u32 mejor = 0, eje = 0, lado, mitad, suma, corte, r, g, b;
		u64 prio = 0;
		mc_caja *c, *o;

		/* la caja con más puntos por lo que mide (la de más error) */
		for (k = 0; k < nc; k++) {
			u32 l = 0, e;
			u64 p;

			for (e = 0; e < 3; e++)
				if ((u32)(cj[k].hi[e] - cj[k].lo[e]) > l)
					l = (u32)(cj[k].hi[e] - cj[k].lo[e]);
			if (!l)
				continue;
			p = (u64)cj[k].n * (l + 1);
			if (p > prio) {
				prio = p;
				mejor = k;
			}
		}
		if (!prio)
			break;
		c = &cj[mejor];
		/* el eje más largo (el verde pesa más) */
		lado = 0;
		for (k = 0; k < 3; k++) {
			u32 l = (u32)(c->hi[k] - c->lo[k]) * (k == 1 ? 4 : k == 0 ? 3 : 2);

			if (l > lado) {
				lado = l;
				eje = k;
			}
		}
		/* la mediana en ese eje */
		mitad = c->n / 2;
		suma = 0;
		corte = c->lo[eje];
		for (k = c->lo[eje]; k < c->hi[eje]; k++) {
			u32 lo0 = c->lo[0], hi0 = c->hi[0], lo1 = c->lo[1], hi1 = c->hi[1], lo2 = c->lo[2], hi2 = c->hi[2];

			if (eje == 0) lo0 = hi0 = k;
			else if (eje == 1) lo1 = hi1 = k;
			else lo2 = hi2 = k;
			for (r = lo0; r <= hi0; r++)
				for (g = lo1; g <= hi1; g++)
					for (b = lo2; b <= hi2; b++)
						suma += h[MC_I(r, g, b)].n;
			corte = k;
			if (suma >= mitad)
				break;
		}
		o = &cj[nc++];
		*o = *c;
		c->hi[eje] = (u8)corte;
		o->lo[eje] = (u8)(corte + 1);
		mc_ajusta(h, c);
		mc_ajusta(h, o);
		if (!o->n)
			nc--;
		else if (!c->n) {
			*c = *o;
			nc--;
		}
	}
	/* el color de cada caja: la media de lo que lleva */
	for (k = 0; k < nc; k++) {
		u64 s[3] = { 0, 0, 0 }, n = 0;
		u32 r, g, b;

		for (r = cj[k].lo[0]; r <= cj[k].hi[0]; r++)
			for (g = cj[k].lo[1]; g <= cj[k].hi[1]; g++)
				for (b = cj[k].lo[2]; b <= cj[k].hi[2]; b++) {
					const mc_cajita *e = &h[MC_I(r, g, b)];

					n += e->n;
					s[0] += e->s[0];
					s[1] += e->s[1];
					s[2] += e->s[2];
				}
		if (!n)
			n = 1;
		pal[k] = (u32)((s[0] + n / 2) / n) << 16 | (u32)((s[1] + n / 2) / n) << 8 | (u32)((s[2] + n / 2) / n);
	}
	so_suelta(cj);
	so_suelta(h);
	return nc;
}

/* el más parecido de la paleta (con una memoria de 6 bits por color) */
static u32 cercano(const u32 *pal, u32 n, u16 *memo, int r, int g, int b)
{
	u32 k = (u32)(r >> 2) << 12 | (u32)(g >> 2) << 6 | (u32)(b >> 2), i, mejor = 0, md = 0xFFFFFFFFu;

	if (memo[k] != 0xFFFF)
		return memo[k];
	for (i = 0; i < n; i++) {
		int dr = (int)(pal[i] >> 16 & 255) - r, dg = (int)(pal[i] >> 8 & 255) - g, db = (int)(pal[i] & 255) - b;
		u32 dd = (u32)(3 * dr * dr + 4 * dg * dg + 2 * db * db);

		if (dd < md) {
			md = dd;
			mejor = i;
		}
	}
	memo[k] = (u16)mejor;
	return mejor;
}

/* reducir a la paleta con Floyd-Steinberg: los índices en «ind» */
static int difumina(const u32 *px, int an, int al, const u32 *pal, u32 n, u32 transp, u8 *ind)
{
	int *e0, *e1, *t, x, y, k;
	u16 *memo = so_pide(sizeof(u16) << 18);
	u32 w = (u32)an;

	e0 = so_pide(((size_t)w + 2) * 3 * sizeof(int));
	e1 = so_pide(((size_t)w + 2) * 3 * sizeof(int));
	if (!memo || !e0 || !e1) {
		so_suelta(memo);
		so_suelta(e0);
		so_suelta(e1);
		return -1;
	}
	memset(memo, 0xFF, sizeof(u16) << 18);
	memset(e0, 0, ((size_t)w + 2) * 3 * sizeof(int));
	for (y = 0; y < al; y++) {
		const u32 *s = px + (u64)y * w;
		u8 *o = ind + (u64)y * w;

		memset(e1, 0, ((size_t)w + 2) * 3 * sizeof(int));
		for (x = 0; x < an; x++) {
			int c[3], *ex = e0 + 3 * (x + 1);
			u32 i;

			if (s[x] >> 24 < 128) {
				o[x] = (u8)transp;
				continue;
			}
			for (k = 0; k < 3; k++) {
				int v = (int)(s[x] >> (16 - 8 * k) & 255) + (ex[k] + (ex[k] < 0 ? -8 : 8)) / 16;

				c[k] = v < 0 ? 0 : v > 255 ? 255 : v;
			}
			i = cercano(pal, n, memo, c[0], c[1], c[2]);
			o[x] = (u8)i;
			for (k = 0; k < 3; k++) {
				int err = c[k] - (int)(pal[i] >> (16 - 8 * k) & 255);

				ex[3 + k] += err * 7;
				e1[3 * x + k] += err * 3;
				e1[3 * (x + 1) + k] += err * 5;
				e1[3 * (x + 2) + k] += err;
			}
		}
		t = e0;
		e0 = e1;
		e1 = t;
	}
	so_suelta(memo);
	so_suelta(e0);
	so_suelta(e1);
	return 0;
}

/* ---- GIF: el LZW (códigos de 12 bits como mucho, con su reinicio) ---- */
#define LZ_TAM 8192
typedef struct {
	bufer *b;
	u32 acc, nacc;
	u8 blq[256];
	u32 nb;
	u32 clave[LZ_TAM];
	u16 cod[LZ_TAM];
} lzw;

static void lz_octeto(lzw *z, u8 v)
{
	z->blq[1 + z->nb++] = v;
	if (z->nb == 255) {
		z->blq[0] = 255;
		buf_pon(z->b, z->blq, 256);
		z->nb = 0;
	}
}

static void lz_pon(lzw *z, u32 c, u32 bits)
{
	z->acc |= c << z->nacc;
	z->nacc += bits;
	while (z->nacc >= 8) {
		lz_octeto(z, (u8)z->acc);
		z->acc >>= 8;
		z->nacc -= 8;
	}
}

static void lz_codifica(bufer *b, const u8 *ind, u32 np, u32 minimo)
{
	lzw *z = so_pide(sizeof *z);
	u32 borra = 1u << minimo, sig, bits, pre, i;

	if (!z) {
		b->mal = 1;
		return;
	}
	z->b = b;
	z->acc = z->nacc = z->nb = 0;
	buf_c(b, (u8)minimo);
	memset(z->clave, 0xFF, sizeof z->clave);
	bits = minimo + 1;
	sig = borra + 2;
	lz_pon(z, borra, bits);
	pre = ind[0];
	for (i = 1; i < np; i++) {
		u32 c = ind[i], k = pre << 8 | c, h = (k * 2654435761u) >> 19;

		for (;; h = (h + 1) & (LZ_TAM - 1))
			if (z->clave[h] == k || z->clave[h] == 0xFFFFFFFFu)
				break;
		if (z->clave[h] == k) {
			pre = z->cod[h];
			continue;
		}
		lz_pon(z, pre, bits);
		if (sig < 4096) {
			z->clave[h] = k;
			z->cod[h] = (u16)sig++;
			if (sig > (1u << bits) && bits < 12)
				bits++;
		} else {
			/* la tabla, llena: se empieza otra */
			lz_pon(z, borra, bits);
			memset(z->clave, 0xFF, sizeof z->clave);
			bits = minimo + 1;
			sig = borra + 2;
		}
		pre = c;
	}
	lz_pon(z, pre, bits);
	lz_pon(z, borra + 1, bits);
	if (z->nacc)
		lz_octeto(z, (u8)z->acc);
	if (z->nb) {
		z->blq[0] = (u8)z->nb;
		buf_pon(b, z->blq, z->nb + 1);
	}
	buf_c(b, 0);
	so_suelta(z);
}

static int escribe_gif(bufer *b, const u32 *px, int an, int al, u32 np)
{
	tcolores *t = so_pide(sizeof *t);
	u8 *ind = so_pide(np), h[13];
	u32 pal[256], n = 0, i, hay_tr = 0, tr = 0, tam, sz;
	int exacto = 1;

	if (!t || !ind) {
		so_suelta(t);
		so_suelta(ind);
		return -1;
	}
	tc_vacia(t);
	for (i = 0; i < np; i++)
		if (px[i] >> 24 < 128)
			hay_tr = 1;
	/* ¿caben en 256 (con el transparente)? */
	for (i = 0; i < np && exacto; i++) {
		int k;

		if (px[i] >> 24 < 128)
			continue;
		k = tc_pon(t, px[i] & 0xFFFFFFu);
		if (k < 0 || t->n + hay_tr > 256)
			exacto = 0;
	}
	if (exacto) {
		n = t->n;
		memcpy(pal, t->color, n * 4);
		tr = n;
		for (i = 0; i < np; i++)
			ind[i] = (u8)(px[i] >> 24 < 128 ? tr : (u32)tc_pon(t, px[i] & 0xFFFFFFu));
	} else {
		n = mediana(px, np, pal, 255);
		tr = n;
		if (!n || difumina(px, an, al, pal, n, tr, ind) < 0) {
			so_suelta(t);
			so_suelta(ind);
			return -1;
		}
	}
	so_suelta(t);
	tam = n + hay_tr;
	for (sz = 1; (1u << sz) < tam; sz++)
		;
	memcpy(h, "GIF89a", 6);
	pon16(h + 6, (u32)an);
	pon16(h + 8, (u32)al);
	h[10] = (u8)(0x80 | (sz - 1) << 4 | (sz - 1));
	h[11] = 0;
	h[12] = 0;
	buf_pon(b, h, 13);
	for (i = 0; i < (1u << sz); i++) {
		u32 c = i < n ? pal[i] : 0;

		buf_c(b, (u8)(c >> 16));
		buf_c(b, (u8)(c >> 8));
		buf_c(b, (u8)c);
	}
	if (hay_tr) {
		u8 g[8] = { 0x21, 0xF9, 4, 1, 0, 0, (u8)tr, 0 };

		buf_pon(b, g, 8);
	}
	{
		u8 d[10] = { 0x2C, 0, 0, 0, 0, (u8)an, (u8)(an >> 8), (u8)al, (u8)(al >> 8), 0 };

		buf_pon(b, d, 10);
	}
	lz_codifica(b, ind, np, sz < 2 ? 2 : sz);
	buf_c(b, 0x3B);
	so_suelta(ind);
	return 0;
}

int fi_escribe(bufer *b, const u32 *px, int an, int al, int f, int calidad, char *err, u32 cabe)
{
	u32 np = puntos(an, al), antes;
	int r;

	if (err && cabe)
		err[0] = 0;
	if (!b || !px || !np) {
		pon_err(err, cabe, "la imagen no vale (mide %d x %d; como mucho, %d x %d)", an, al, FI_MAX_LADO, FI_MAX_LADO);
		return -1;
	}
	if (!fi_se_escribe(f)) {
		pon_err(err, cabe, "Pintura no sabe guardar en %s", fi_nombre(f));
		return -1;
	}
	antes = b->n;
	switch (f) {
	case FI_PNG:
		r = escribe_png(b, px, an, al, np);
		break;
	case FI_JPG:
		r = escribe_jpg(b, px, an, al, np, calidad);
		break;
	case FI_BMP:
		r = escribe_bmp(b, px, an, al);
		break;
	case FI_GIF:
		r = escribe_gif(b, px, an, al, np);
		break;
	default:
		r = escribe_pcx(b, px, an, al, np);
		break;
	}
	if (r < 0 || b->mal) {
		if (!b->mal)
			b->n = antes;
		pon_err(err, cabe, "no hay memoria para guardar la imagen en %s", fi_nombre(f));
		return -1;
	}
	return 0;
}
