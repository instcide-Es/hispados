/* imagen.c - Hispa-PDF: las imágenes. Hacer un PDF con fotos (las JPEG van
 * tal cual; las demás, sin perder nada, con Flate y su transparencia
 * aparte), hacer más pequeñas las JPEG de un PDF al comprimirlo y escribir
 * las páginas pintadas como PNG o JPG. Con stb_image y stb_image_write
 * (terceros/stb, dominio público o MIT) y el Flate de miniz.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "hpdf.h"
#include "miniz_cfg.h"

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(n) so_pide(n)
#define STBI_REALLOC(p, n) so_repide(p, n)
#define STBI_FREE(p) so_suelta(p)
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static unsigned char *comprime_stbiw(unsigned char *d, int n, int *sal, int calidad)
{
	u32 k;
	u8 *r;

	(void)calidad;
	r = flate_cod(d, (u32)n, &k, 7);
	*sal = (int)k;
	return r;
}
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(n) so_pide(n)
#define STBIW_REALLOC(p, n) so_repide(p, n)
#define STBIW_FREE(p) so_suelta(p)
#define STBIW_ZLIB_COMPRESS comprime_stbiw
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

/* lo que escribe stb_image_write, a un búfer */
typedef struct {
	u8 *d;
	u32 n, cap;
} pmem;

static void a_mem(void *u, void *d, int n)
{
	pmem *m = u;

	if (m->n + (u32)n > m->cap) {
		u32 c = m->cap ? m->cap * 2 : 65536;

		while (c < m->n + (u32)n)
			c *= 2;
		m->d = repide(m->d, c);
		m->cap = c;
	}
	memcpy(m->d + m->n, d, (size_t)n);
	m->n += (u32)n;
}

/* la imagen en pequeño (la media de lo que cae en cada punto) */
static u8 *achica(const u8 *img, int w, int h, int comp, int an, int al)
{
	u8 *r = pide((size_t)an * al * comp);
	int x, y, c;

	for (y = 0; y < al; y++) {
		int y0 = (int)((i64)y * h / al), y1 = (int)((i64)(y + 1) * h / al);

		if (y1 <= y0)
			y1 = y0 + 1;
		for (x = 0; x < an; x++) {
			int x0 = (int)((i64)x * w / an), x1 = (int)((i64)(x + 1) * w / an), yy, xx;
			u32 s[4] = {0, 0, 0, 0}, cuenta = 0;

			if (x1 <= x0)
				x1 = x0 + 1;
			for (yy = y0; yy < y1; yy++)
				for (xx = x0; xx < x1; xx++) {
					const u8 *p = img + ((size_t)yy * w + xx) * comp;

					for (c = 0; c < comp; c++)
						s[c] += p[c];
					cuenta++;
				}
			for (c = 0; c < comp; c++)
				r[((size_t)y * an + x) * comp + c] = (u8)(s[c] / cuenta);
		}
	}
	return r;
}

u8 *jpeg_menor(const u8 *d, u32 n, int calidad, int lado, int *an, int *al, u32 *sal)
{
	int w, h, comp, cw;
	u8 *img, *peq = NULL;
	pmem m;

	if (!stbi_info_from_memory(d, (int)n, &w, &h, &comp))
		return NULL;
	if (comp != 1 && comp != 3)
		return NULL;
	img = stbi_load_from_memory(d, (int)n, &w, &h, &cw, comp);
	if (!img)
		return NULL;
	*an = w;
	*al = h;
	if (lado > 0 && (w > lado || h > lado)) {
		if (w >= h) {
			*an = lado;
			*al = (int)((i64)h * lado / w);
		} else {
			*al = lado;
			*an = (int)((i64)w * lado / h);
		}
		if (*an < 1)
			*an = 1;
		if (*al < 1)
			*al = 1;
		peq = achica(img, w, h, comp, *an, *al);
		stbi_image_free(img);
		img = peq;
	}
	memset(&m, 0, sizeof m);
	if (!stbi_write_jpg_to_func(a_mem, &m, *an, *al, comp, img, calidad)) {
		if (peq)
			so_suelta(peq);
		else
			stbi_image_free(img);
		so_suelta(m.d);
		return NULL;
	}
	if (peq)
		so_suelta(peq);
	else
		stbi_image_free(img);
	*sal = m.n;
	return m.d;
}

/* ---- la página pintada, a PNG o JPG ---- */
u8 *imagen_a_archivo(pimagen *im, int jpg, int calidad, u32 *n)
{
	u8 *rgb = pide((size_t)im->an * im->al * 3);
	size_t i, t = (size_t)im->an * im->al;
	pmem m;
	int bien;

	for (i = 0; i < t; i++) {
		u32 c = im->px[i];

		rgb[3 * i] = (u8)(c >> 16);
		rgb[3 * i + 1] = (u8)(c >> 8);
		rgb[3 * i + 2] = (u8)c;
	}
	memset(&m, 0, sizeof m);
	if (jpg)
		bien = stbi_write_jpg_to_func(a_mem, &m, im->an, im->al, 3, rgb, calidad);
	else
		bien = stbi_write_png_to_func(a_mem, &m, im->an, im->al, 3, rgb, im->an * 3);
	so_suelta(rgb);
	if (!bien) {
		so_suelta(m.d);
		return NULL;
	}
	*n = m.n;
	return m.d;
}

/* ---- leer una imagen cualquiera (para pintar: los JPEG de los PDF) ---- */
u8 *decodifica_imagen(const u8 *d, u32 n, int *w, int *h, int *comp, int quiero)
{
	return stbi_load_from_memory(d, (int)n, w, h, comp, quiero);
}

void suelta_stbi(void *p)
{
	stbi_image_free(p);
}

/* ---- un PDF con imágenes ---- */

/* añade la página de una imagen a «s»; da su número o 0 (y por qué) */
u32 pagina_de_imagen(pnuevo *s, const u8 *datos, u32 n, ppapel *papel, const char **error)
{
	pdoc *d = s->d;
	int w, h, comp;
	pobj *img, *dic, *pg, *res, *xo, *mb, *cont;
	u32 num_img, num_cont, num_pg;
	double pw, ph, esc, x, y, dw, dh;
	char t[200];
	int tn;

	if (!stbi_info_from_memory(datos, (int)n, &w, &h, &comp)) {
		*error = "no es una imagen que se conozca (JPG, PNG, BMP, GIF, TGA, PSD, PNM)";
		return 0;
	}
	img = o_nuevo(d, O_FLUJO);
	dic = o_dicc(d);
	img->u.f.dic = dic;
	img->propio = 1;
	o_pon(d, dic, "Type", o_nombre(d, "XObject"));
	o_pon(d, dic, "Subtype", o_nombre(d, "Image"));
	o_pon(d, dic, "Width", o_ent(d, w));
	o_pon(d, dic, "Height", o_ent(d, h));
	o_pon(d, dic, "BitsPerComponent", o_ent(d, 8));
	if (n > 3 && datos[0] == 0xFF && datos[1] == 0xD8 && (comp == 1 || comp == 3)) {
		/* JPEG: tal cual */
		u8 *copia = ar_pide(d, n);

		memcpy(copia, datos, n);
		img->u.f.datos = copia;
		img->u.f.largo = n;
		o_pon(d, dic, "ColorSpace", o_nombre(d, comp == 1 ? "DeviceGray" : "DeviceRGB"));
		o_pon(d, dic, "Filter", o_nombre(d, "DCTDecode"));
	} else {
		int cw, gris = 1, alfa = 0;
		u8 *px = stbi_load_from_memory(datos, (int)n, &w, &h, &cw, 4), *plano, *z;
		size_t i, t2 = (size_t)w * h;
		u32 k;

		if (!px) {
			*error = "la imagen está estropeada";
			return 0;
		}
		for (i = 0; i < t2; i++) {
			u8 *p = px + 4 * i;

			if (p[0] != p[1] || p[1] != p[2])
				gris = 0;
			if (p[3] != 255)
				alfa = 1;
		}
		plano = pide(t2 * (gris ? 1 : 3) + 1);
		for (i = 0; i < t2; i++) {
			if (gris)
				plano[i] = px[4 * i];
			else
				memcpy(plano + 3 * i, px + 4 * i, 3);
		}
		z = flate_cod(plano, (u32)(t2 * (gris ? 1 : 3)), &k, 6);
		so_suelta(plano);
		img->u.f.datos = ar_pide(d, k);
		memcpy((u8 *)img->u.f.datos, z, k);
		img->u.f.largo = k;
		so_suelta(z);
		o_pon(d, dic, "ColorSpace", o_nombre(d, gris ? "DeviceGray" : "DeviceRGB"));
		o_pon(d, dic, "Filter", o_nombre(d, "FlateDecode"));
		if (alfa) {
			pobj *m = o_nuevo(d, O_FLUJO), *md = o_dicc(d);
			u8 *a = pide(t2 + 1);

			for (i = 0; i < t2; i++)
				a[i] = px[4 * i + 3];
			z = flate_cod(a, (u32)t2, &k, 6);
			so_suelta(a);
			m->u.f.dic = md;
			m->propio = 1;
			m->u.f.datos = ar_pide(d, k);
			memcpy((u8 *)m->u.f.datos, z, k);
			m->u.f.largo = k;
			so_suelta(z);
			o_pon(d, md, "Type", o_nombre(d, "XObject"));
			o_pon(d, md, "Subtype", o_nombre(d, "Image"));
			o_pon(d, md, "Width", o_ent(d, w));
			o_pon(d, md, "Height", o_ent(d, h));
			o_pon(d, md, "BitsPerComponent", o_ent(d, 8));
			o_pon(d, md, "ColorSpace", o_nombre(d, "DeviceGray"));
			o_pon(d, md, "Filter", o_nombre(d, "FlateDecode"));
			o_pon(d, dic, "SMask", o_ref(d, nuevo_obj(s, m, NULL), 0));
		}
		stbi_image_free(px);
	}
	num_img = nuevo_obj(s, img, NULL);
	/* la página y dónde va la imagen */
	dw = w * 72.0 / 96;
	dh = h * 72.0 / 96;
	if (papel->an <= 0) {
		pw = dw + 2 * papel->margen;
		ph = dh + 2 * papel->margen;
	} else {
		pw = papel->an;
		ph = papel->al;
		if ((w > h) != (pw > ph)) {
			double tt = pw;
			pw = ph;
			ph = tt;
		}
	}
	esc = (pw - 2 * papel->margen) / dw;
	if ((ph - 2 * papel->margen) / dh < esc)
		esc = (ph - 2 * papel->margen) / dh;
	if (esc <= 0)
		esc = 1;
	dw *= esc;
	dh *= esc;
	x = (pw - dw) / 2;
	y = (ph - dh) / 2;
	tn = fmt(t, sizeof t, "q %.3f 0 0 %.3f %.3f %.3f cm /Im0 Do Q\n", dw, dh, x, y);
	cont = o_nuevo(d, O_FLUJO);
	cont->u.f.dic = o_dicc(d);
	cont->propio = 1;
	cont->u.f.datos = ar_pide(d, (u32)tn);
	memcpy((u8 *)cont->u.f.datos, t, (size_t)tn);
	cont->u.f.largo = (u32)tn;
	num_cont = nuevo_obj(s, cont, NULL);
	pg = o_dicc(d);
	res = o_dicc(d);
	xo = o_dicc(d);
	mb = o_arreglo(d);
	o_mete(d, mb, o_ent(d, 0));
	o_mete(d, mb, o_ent(d, 0));
	o_mete(d, mb, o_real(d, (double)(i32)(pw * 100 + 0.5) / 100));
	o_mete(d, mb, o_real(d, (double)(i32)(ph * 100 + 0.5) / 100));
	o_pon(d, xo, "Im0", o_ref(d, num_img, 0));
	o_pon(d, res, "XObject", xo);
	o_pon(d, pg, "Type", o_nombre(d, "Page"));
	o_pon(d, pg, "MediaBox", mb);
	o_pon(d, pg, "Resources", res);
	o_pon(d, pg, "Contents", o_ref(d, num_cont, 0));
	num_pg = nuevo_obj(s, pg, NULL);
	return num_pg;
}
