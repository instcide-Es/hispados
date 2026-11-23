/* pdfsal.c - la oficina de 32 bits: escribir un PDF (oficina/comun/pdfsal.h).
 *
 * Cómo va: lo escrito va a un búfer, objeto a objeto, apuntando dónde
 * empieza cada uno (para la tabla xref del final). Cada página, al
 * acabarse, deja su flujo de contenido (comprimido) y su objeto /Page. Las
 * letras y los recursos (uno para todas las páginas) se escriben al final,
 * cuando ya se sabe qué glifos se han usado. Los objetos fijos: 1 el
 * catálogo, 2 el árbol de páginas, 3 los recursos, 4 /Info.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "pdfsal.h"
#include "letras.h"
#include "ttf.h"
#include "imagen.h"
/* (de miniz solo tdefl_compress_mem_to_heap y mz_free: sin stdio ni la hora,
 * como la copia de miniz que lleva el programa) */
#ifndef MINIZ_NO_STDIO
#define MINIZ_NO_STDIO
#endif
#ifndef MINIZ_NO_TIME
#define MINIZ_NO_TIME
#endif
#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "miniz.h"

#define OBJ_CATALOGO 1
#define OBJ_PAGINAS 2
#define OBJ_RECURSOS 3
#define OBJ_INFO 4
#define INCLINA 0.21		/* la cursiva hecha a mano (tan 12°), como letras.c */

typedef struct {
	u32 obj;		/* el /Type0 (0: no se ha usado) */
	u8 *usados;		/* un bit por glifo */
	u32 *uni;		/* el Unicode de cada glifo (para ToUnicode) */
	u32 n_glifos;
} pcara;

typedef struct {
	const u8 *datos;
	u32 n, obj;
} pimg;

typedef struct {
	char *texto;
	int nivel, pag;
	double y;
} pmarca;

struct pdfsal {
	bufer o;		/* el PDF */
	u32 *off;		/* dónde empieza cada objeto (por su número) */
	u32 n_obj, cabe_obj;
	bufer c;		/* el contenido de la página de ahora */
	int en_pagina;
	double an, al;		/* lo que mide la página de ahora */
	u32 *pag_obj;		/* el objeto de cada página */
	double *pag_al;
	u32 n_pag, cabe_pag;
	pcara cara[LETRAS_MAX];
	pimg *img;
	u32 n_img, cabe_img;
	pmarca *mar;
	u32 n_mar, cabe_mar;
	char titulo[200], autor[200], asunto[200];
	char productor[80];		/* (0.8.8) el programa que lo hace: pdf_productor */
	int mal;
};

/* ---- lo pequeño ---- */
static void *crece(void *p, u32 *cabe, u32 falta, u32 tam, int *mal)
{
	u32 c;
	void *n;

	if (falta <= *cabe)
		return p;
	c = *cabe ? *cabe * 2 : 16;
	while (c < falta)
		c *= 2;
	n = so_repide(p, (size_t)c * tam);
	if (!n) {
		*mal = 1;
		return NULL;
	}
	*cabe = c;
	return n;
}

/* un número con hasta 3 decimales, sin ceros de más */
static void num(bufer *b, double v)
{
	char t[32];
	int k = 0, neg = v < 0;
	u32 e, f;

	if (neg)
		v = -v;
	if (v > 4e6)
		v = 4e6;
	v = v * 1000 + 0.5;
	e = (u32)v / 1000;
	f = (u32)v % 1000;
	if (neg && (e || f))
		t[k++] = '-';
	k += fmt(t + k, sizeof t - (u32)k, "%u", e);
	if (f) {
		t[k++] = '.';
		t[k++] = (char)('0' + f / 100);
		if (f % 100) {
			t[k++] = (char)('0' + f / 10 % 10);
			if (f % 10)
				t[k++] = (char)('0' + f % 10);
		}
	}
	t[k++] = ' ';
	buf_pon(b, t, (u32)k);
}

static void color(bufer *b, u32 c, const char *op)
{
	num(b, (c >> 16 & 255) / 255.0);
	num(b, (c >> 8 & 255) / 255.0);
	num(b, (c & 255) / 255.0);
	buf_txt(b, op);
	buf_c(b, ' ');
}

/* un texto como cadena del PDF: UTF-16BE con su marca, en hexadecimal */
static void cadena(bufer *b, const char *s)
{
	u32 n = largo(s), i = 0, c;

	buf_txt(b, "<FEFF");
	while (i < n) {
		i += (u32)utf8_lee((const u8 *)s + i, n - i, &c);
		if (c >= 0x10000) {
			c -= 0x10000;
			buf_fmt(b, "%04X%04X", 0xD800 + (c >> 10), 0xDC00 + (c & 0x3FF));
		} else
			buf_fmt(b, "%04X", c);
	}
	buf_c(b, '>');
}

static u32 obj_nuevo(pdfsal *s)
{
	u32 *n = crece(s->off, &s->cabe_obj, s->n_obj + 1, 4, &s->mal);

	if (!n)
		return 0;
	s->off = n;
	s->off[s->n_obj] = 0;
	return s->n_obj++;
}

static void obj_empieza(pdfsal *s, u32 n)
{
	if (n && n < s->n_obj)
		s->off[n] = s->o.n;
	buf_fmt(&s->o, "%u 0 obj\n", n);
}

static void obj_acaba(pdfsal *s)
{
	buf_txt(&s->o, "\nendobj\n");
}

/* un flujo (comprimido si «flate» y si sale más pequeño); «dic», lo de más de su diccionario */
static void flujo(pdfsal *s, u32 n, const char *dic, const u8 *d, u32 len, int flate)
{
	size_t m = 0;
	void *z = NULL;

	if (flate && len > 64)
		z = tdefl_compress_mem_to_heap(d, len, &m, TDEFL_WRITE_ZLIB_HEADER | 128);
	if (!z && flate && len > 64)
		s->mal = 1;
	obj_empieza(s, n);
	if (z && m < len) {
		buf_fmt(&s->o, "<< %s /Filter /FlateDecode /Length %u >>\nstream\n", dic, (u32)m);
		buf_pon(&s->o, z, (u32)m);
	} else {
		buf_fmt(&s->o, "<< %s /Length %u >>\nstream\n", dic, len);
		buf_pon(&s->o, d, len);
	}
	buf_txt(&s->o, "\nendstream");
	obj_acaba(s);
	if (z)
		mz_free(z);
}

/* ---- el documento ---- */
pdfsal *pdf_nuevo(void)
{
	pdfsal *s = so_pide(sizeof(pdfsal));
	int i;

	if (!s)
		return NULL;
	memset(s, 0, sizeof *s);
	for (i = 0; i <= OBJ_INFO; i++)
		obj_nuevo(s);
	buf_txt(&s->o, "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
	return s;
}

/* (0.8.8) el programa que hace el PDF (/Producer: «Cuentas 2.0 (Hispa-Pentium)») */
void pdf_productor(pdfsal *s, const char *p)
{
	u32 i;

	for (i = 0; p && p[i] && i + 1 < sizeof s->productor; i++)
		s->productor[i] = p[i];
	s->productor[i] = 0;
}

void pdf_datos(pdfsal *s, const char *titulo, const char *autor, const char *asunto)
{
	copia(s->titulo, titulo ? titulo : "", sizeof s->titulo);
	copia(s->autor, autor ? autor : "", sizeof s->autor);
	copia(s->asunto, asunto ? asunto : "", sizeof s->asunto);
}

static void acaba_pagina(pdfsal *s)
{
	u32 c, p;

	if (!s->en_pagina)
		return;
	s->en_pagina = 0;
	p = s->pag_obj[s->n_pag - 1];
	c = obj_nuevo(s);
	if (!c)
		return;
	flujo(s, c, "", s->c.d ? s->c.d : (const u8 *)"", s->c.n, 1);
	if (s->c.mal)
		s->mal = 1;
	buf_suelta(&s->c);
	obj_empieza(s, p);
	buf_txt(&s->o, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
	num(&s->o, s->an);
	num(&s->o, s->al);
	buf_fmt(&s->o, "] /Resources 3 0 R /Contents %u 0 R >>", c);
	obj_acaba(s);
}

int pdf_pagina(pdfsal *s, double ancho, double alto)
{
	u32 *n;
	double *h;
	u32 cabe = s->cabe_pag;

	acaba_pagina(s);
	if (ancho < 36)
		ancho = 36;
	if (alto < 36)
		alto = 36;
	n = crece(s->pag_obj, &cabe, s->n_pag + 1, 4, &s->mal);
	if (!n)
		return -1;
	s->pag_obj = n;
	cabe = s->cabe_pag;
	h = crece(s->pag_al, &cabe, s->n_pag + 1, sizeof(double), &s->mal);
	if (!h)
		return -1;
	s->pag_al = h;
	s->cabe_pag = cabe;
	s->pag_obj[s->n_pag] = obj_nuevo(s);
	s->pag_al[s->n_pag] = alto;
	s->an = ancho;
	s->al = alto;
	s->en_pagina = 1;
	return (int)s->n_pag++;
}

/* ---- el texto ---- */
static pcara *usa_cara(pdfsal *s, int k)
{
	pcara *c = &s->cara[k];

	if (!c->obj) {
		ttf *t = letra_ttf(k);

		c->n_glifos = t->n_glifos;
		c->usados = so_pide(c->n_glifos / 8 + 1);
		c->uni = so_pide(c->n_glifos * 4 + 4);
		if (!c->usados || !c->uni) {
			so_suelta(c->usados);
			so_suelta(c->uni);
			c->usados = NULL;
			c->uni = NULL;
			s->mal = 1;
			return NULL;
		}
		memset(c->usados, 0, c->n_glifos / 8 + 1);
		memset(c->uni, 0, c->n_glifos * 4 + 4);
		c->obj = obj_nuevo(s);
	}
	return c;
}

/* lo que avanza el glifo en milésimas (lo que dice /W) */
static int ancho_w(int cara, u32 g)
{
	return (int)(((i64)letra_avance(cara, g) * 1000 + letra_em(cara) / 2) / letra_em(cara));
}

void pdf_texto(pdfsal *s, int cara, int falsas, double tam, u32 col, const u32 *t, u32 n, const double *xs, double y)
{
	u32 i = 0;
	bufer *b = &s->c;

	if (!s->en_pagina || !n || tam <= 0 || cara < 0 || cara >= LETRAS_MAX)
		return;
	if (falsas & FALSA_NEGRITA) {
		num(b, tam * 0.03);
		buf_txt(b, "w ");
		color(b, col, "RG");
	}
	while (i < n) {
		int real, r2;
		u32 g = letra_glifo(cara, t[i], &real), j;
		pcara *pc = usa_cara(s, real);
		double pluma;

		if (!pc)
			return;
		buf_txt(b, "BT ");
		color(b, col, "rg");
		buf_fmt(b, "/F%d ", real);
		num(b, tam);
		buf_txt(b, (falsas & FALSA_NEGRITA) ? "Tf 2 Tr 1 0 " : "Tf 0 Tr 1 0 ");
		num(b, (falsas & FALSA_CURSIVA) ? INCLINA : 0);
		buf_txt(b, "1 ");
		num(b, xs[i]);
		num(b, s->al - y);
		buf_txt(b, "Tm [<");
		pluma = xs[i];
		for (j = i; j < n; j++) {
			int w;

			if (j > i) {
				g = letra_glifo(cara, t[j], &r2);
				if (r2 != real)
					break;
			}
			if (g < pc->n_glifos) {
				pc->usados[g >> 3] |= (u8)(1 << (g & 7));
				if (!pc->uni[g])
					pc->uni[g] = t[j];
			}
			buf_fmt(b, "%04X", g & 0xFFFF);
			w = ancho_w(real, g);
			/* la corrección para que la letra de después caiga en su x */
			if (j + 1 < n) {
				double sig = pluma + w * tam / 1000;
				i32 k = (i32)((sig - xs[j + 1]) * 1000 / tam + (sig >= xs[j + 1] ? 0.5 : -0.5));

				if (k) {
					buf_fmt(b, "> %d <", k);
				}
				pluma = sig - k * tam / 1000;
			}
		}
		buf_txt(b, ">] TJ ET\n");
		i = j;
	}
}

void pdf_rect(pdfsal *s, double x, double y, double an, double al, u32 col)
{
	if (!s->en_pagina || an <= 0 || al <= 0)
		return;
	color(&s->c, col, "rg");
	num(&s->c, x);
	num(&s->c, s->al - y - al);
	num(&s->c, an);
	num(&s->c, al);
	buf_txt(&s->c, "re f\n");
}

/* (CUENTAS32) polígonos rellenos y recortes (Cuentas 2: sectores, marcadores, tramas) */
static void camino(pdfsal *s, const double *xy, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		num(&s->c, xy[2 * i]);
		num(&s->c, s->al - xy[2 * i + 1]);
		buf_txt(&s->c, i ? "l " : "m ");
	}
	buf_txt(&s->c, "h ");
}

void pdf_poligono(pdfsal *s, const double *xy, int n, u32 col)
{
	if (!s->en_pagina || n < 3)
		return;
	color(&s->c, col, "rg");
	camino(s, xy, n);
	buf_txt(&s->c, "f\n");
}

void pdf_guarda(pdfsal *s)
{
	if (s->en_pagina)
		buf_txt(&s->c, "q\n");
}

void pdf_restaura(pdfsal *s)
{
	if (s->en_pagina)
		buf_txt(&s->c, "Q\n");
}

void pdf_recorta_rect(pdfsal *s, double x, double y, double an, double al)
{
	if (!s->en_pagina)
		return;
	if (an < 0)
		an = 0;
	if (al < 0)
		al = 0;
	num(&s->c, x);
	num(&s->c, s->al - y - al);
	num(&s->c, an);
	num(&s->c, al);
	buf_txt(&s->c, "re W n\n");
}

void pdf_recorta_poligono(pdfsal *s, const double *xy, int n)
{
	if (!s->en_pagina || n < 3)
		return;
	camino(s, xy, n);
	buf_txt(&s->c, "W n\n");
}

void pdf_raya(pdfsal *s, double x0, double y0, double x1, double y1, double g, u32 col)
{
	if (!s->en_pagina)
		return;
	color(&s->c, col, "RG");
	num(&s->c, g > 0 ? g : 0.5);
	buf_txt(&s->c, "w 0 J ");
	num(&s->c, x0);
	num(&s->c, s->al - y0);
	buf_txt(&s->c, "m ");
	num(&s->c, x1);
	num(&s->c, s->al - y1);
	buf_txt(&s->c, "l S\n");
}

/* ---- las imágenes ---- */
static int jpeg_adobe(const u8 *d, u32 n)
{
	u32 i;

	for (i = 2; i + 11 < n && i < 65536; i++)
		if (d[i] == 0xFF && d[i + 1] == 0xEE && !memcmp(d + i + 4, "Adobe", 5))
			return 1;
	return 0;
}

int pdf_imagen(pdfsal *s, const u8 *datos, u32 n)
{
	u32 k, obj;
	int an, al, tipo = imagen_tipo(datos, n);
	pimg *v;

	for (k = 0; k < s->n_img; k++)
		if (s->img[k].datos == datos && s->img[k].n == n)
			return (int)k;
	if (!tipo || !imagen_mide(datos, n, &an, &al))
		return -1;
	v = crece(s->img, &s->cabe_img, s->n_img + 1, sizeof(pimg), &s->mal);
	if (!v)
		return -1;
	s->img = v;
	obj = obj_nuevo(s);
	if (!obj)
		return -1;
	if (tipo == IMAGEN_JPG) {
		int c = imagen_jpeg_componentes(datos, n);
		char dic[200];

		if (c != 1 && c != 3 && c != 4)
			goto descomprime;
		fmt(dic, sizeof dic, "/Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 8 /ColorSpace /%s /Filter /DCTDecode%s",
		    an, al, c == 1 ? "DeviceGray" : c == 3 ? "DeviceRGB" : "DeviceCMYK",
		    c == 4 && jpeg_adobe(datos, n) ? " /Decode [1 0 1 0 1 0 1 0]" : "");
		flujo(s, obj, dic, datos, n, 0);
	} else {
		u32 *px, i, mascara = 0;
		u8 *rgb, *alfa;
		char dic[200];

descomprime:
		px = imagen_lee(datos, n, &an, &al);
		if (!px) {
			/* (la reservada: un punto blanco, para que el PDF esté entero) */
			static const u8 blanco[3] = { 255, 255, 255 };

			flujo(s, obj, "/Type /XObject /Subtype /Image /Width 1 /Height 1 /BitsPerComponent 8 /ColorSpace /DeviceRGB", blanco, 3, 0);
			goto apunta;
		}
		rgb = so_pide((size_t)an * al * 3);
		alfa = so_pide((size_t)an * al);
		if (!rgb || !alfa) {
			so_suelta(px);
			so_suelta(rgb);
			so_suelta(alfa);
			s->mal = 1;
			return -1;
		}
		for (i = 0; i < (u32)(an * al); i++) {
			rgb[3 * i] = (u8)(px[i] >> 16);
			rgb[3 * i + 1] = (u8)(px[i] >> 8);
			rgb[3 * i + 2] = (u8)px[i];
			alfa[i] = (u8)(px[i] >> 24);
			if (alfa[i] != 255)
				mascara = 1;
		}
		so_suelta(px);
		if (mascara) {
			u32 m = obj_nuevo(s);

			fmt(dic, sizeof dic, "/Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 8 /ColorSpace /DeviceGray", an, al);
			flujo(s, m, dic, alfa, (u32)(an * al), 1);
			fmt(dic, sizeof dic, "/Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 8 /ColorSpace /DeviceRGB /SMask %u 0 R", an, al, m);
		} else
			fmt(dic, sizeof dic, "/Type /XObject /Subtype /Image /Width %d /Height %d /BitsPerComponent 8 /ColorSpace /DeviceRGB", an, al);
		flujo(s, obj, dic, rgb, (u32)(an * al * 3), 1);
		so_suelta(rgb);
		so_suelta(alfa);
	}
apunta:
	s->img[s->n_img].datos = datos;
	s->img[s->n_img].n = n;
	s->img[s->n_img].obj = obj;
	return (int)s->n_img++;
}

void pdf_pon_imagen(pdfsal *s, int k, double x, double y, double an, double al)
{
	if (!s->en_pagina || k < 0 || (u32)k >= s->n_img || an <= 0 || al <= 0)
		return;
	buf_txt(&s->c, "q ");
	num(&s->c, an);
	buf_txt(&s->c, "0 0 ");
	num(&s->c, al);
	num(&s->c, x);
	num(&s->c, s->al - y - al);
	buf_fmt(&s->c, "cm /Im%d Do Q\n", k);
}

/* ---- los marcadores ---- */
void pdf_marcador(pdfsal *s, const char *texto, int nivel, int pag, double y)
{
	pmarca *m = crece(s->mar, &s->cabe_mar, s->n_mar + 1, sizeof(pmarca), &s->mal);

	if (!m)
		return;
	s->mar = m;
	m = &s->mar[s->n_mar];
	m->texto = duplica(texto);
	if (!m->texto) {
		s->mal = 1;
		return;
	}
	m->nivel = nivel < 1 ? 1 : nivel;
	m->pag = pag;
	m->y = y;
	s->n_mar++;
}

static void escribe_marcadores(pdfsal *s, u32 raiz)
{
	u32 n = s->n_mar, i, j, *obj = so_pide((n + 1) * 4);
	int *padre = so_pide((n + 1) * sizeof(int));

	if (!obj || !padre) {
		so_suelta(obj);
		so_suelta(padre);
		s->mal = 1;
		return;
	}
	for (i = 0; i < n; i++) {
		obj[i] = obj_nuevo(s);
		padre[i] = -1;
		for (j = i; j-- > 0;)
			if (s->mar[j].nivel < s->mar[i].nivel) {
				padre[i] = (int)j;
				break;
			}
	}
	for (i = 0; i < n; i++) {
		int prev = -1, sig = -1, primero = -1, ultimo = -1, cuantos = 0, pg = s->mar[i].pag;

		for (j = i; j-- > 0;)
			if (padre[j] == padre[i]) {
				prev = (int)j;
				break;
			}
		for (j = i + 1; j < n; j++)
			if (padre[j] == padre[i]) {
				sig = (int)j;
				break;
			}
		for (j = i + 1; j < n; j++) {
			if (padre[j] == (int)i) {
				if (primero < 0)
					primero = (int)j;
				ultimo = (int)j;
			}
			/* (los que van debajo: hasta uno de nivel igual o menor) */
			if (s->mar[j].nivel <= s->mar[i].nivel)
				break;
			cuantos++;
		}
		if (pg < 0 || (u32)pg >= s->n_pag)
			pg = 0;
		obj_empieza(s, obj[i]);
		buf_txt(&s->o, "<< /Title ");
		cadena(&s->o, s->mar[i].texto);
		buf_fmt(&s->o, " /Parent %u 0 R", padre[i] < 0 ? raiz : obj[padre[i]]);
		if (prev >= 0)
			buf_fmt(&s->o, " /Prev %u 0 R", obj[prev]);
		if (sig >= 0)
			buf_fmt(&s->o, " /Next %u 0 R", obj[sig]);
		if (primero >= 0)
			buf_fmt(&s->o, " /First %u 0 R /Last %u 0 R /Count %d", obj[primero], obj[ultimo], cuantos);
		buf_fmt(&s->o, " /Dest [%u 0 R /XYZ 0 ", s->pag_obj[pg]);
		num(&s->o, s->pag_al[pg] - s->mar[i].y);
		buf_txt(&s->o, "null] >>");
		obj_acaba(s);
	}
	{
		int primero = -1, ultimo = -1;

		for (i = 0; i < n; i++)
			if (padre[i] < 0) {
				if (primero < 0)
					primero = (int)i;
				ultimo = (int)i;
			}
		obj_empieza(s, raiz);
		buf_fmt(&s->o, "<< /Type /Outlines /First %u 0 R /Last %u 0 R /Count %u >>", obj[primero], obj[ultimo], n);
		obj_acaba(s);
	}
	so_suelta(obj);
	so_suelta(padre);
}

/* ---- las letras, al final ---- */
static void escribe_cara(pdfsal *s, int k)
{
	pcara *c = &s->cara[k];
	ttf *t = letra_ttf(k);
	u32 cid = obj_nuevo(s), desc = obj_nuevo(s), ff = obj_nuevo(s), tu = obj_nuevo(s), g, n, em = t->em;
	u8 *sub;
	char nombre[80], tag[8];
	bufer b = { 0 };

	/* el nombre: la etiqueta del subconjunto (seis mayúsculas) y el de la letra */
	for (g = 0; g < 6; g++)
		tag[g] = (char)('A' + ((u32)k * 7 + g * 3 + t->n_glifos) % 26);
	tag[6] = 0;
	fmt(nombre, sizeof nombre, "%s+%s", tag, t->postscript[0] ? t->postscript : "Letra");
	/* el /Type0 */
	obj_empieza(s, c->obj);
	buf_fmt(&s->o, "<< /Type /Font /Subtype /Type0 /BaseFont /%s /Encoding /Identity-H /DescendantFonts [%u 0 R] /ToUnicode %u 0 R >>",
		nombre, cid, tu);
	obj_acaba(s);
	/* la CIDFontType2, con los anchos de los glifos usados */
	obj_empieza(s, cid);
	buf_fmt(&s->o, "<< /Type /Font /Subtype /CIDFontType2 /BaseFont /%s /CIDSystemInfo << /Registry (Adobe) /Ordering (Identity) /Supplement 0 >> "
		"/FontDescriptor %u 0 R /CIDToGIDMap /Identity /DW 1000 /W [", nombre, desc);
	for (g = 0; g < c->n_glifos; g++) {
		u32 h;

		if (!(c->usados[g >> 3] >> (g & 7) & 1))
			continue;
		buf_fmt(&s->o, "%u [", g);
		for (h = g; h < c->n_glifos && (c->usados[h >> 3] >> (h & 7) & 1); h++)
			buf_fmt(&s->o, "%d ", ancho_w(k, h));
		buf_txt(&s->o, "] ");
		g = h;
	}
	buf_txt(&s->o, "] >>");
	obj_acaba(s);
	/* el descriptor */
	obj_empieza(s, desc);
	buf_fmt(&s->o, "<< /Type /FontDescriptor /FontName /%s /Flags %d /FontBBox [%d %d %d %d] /ItalicAngle 0 /Ascent %d /Descent %d "
		"/CapHeight %d /StemV %d /FontFile2 %u 0 R >>", nombre, 4 | (t->cursiva ? 64 : 0),
		(int)((i64)t->x0 * 1000 / em), (int)((i64)t->y0 * 1000 / em), (int)((i64)t->x1 * 1000 / em), (int)((i64)t->y1 * 1000 / em),
		(int)((i64)t->asc * 1000 / em), (int)((i64)t->desc * 1000 / em), (int)((i64)t->cap * 1000 / em), t->negrita ? 140 : 80, ff);
	obj_acaba(s);
	/* la letra (solo lo usado) */
	sub = ttf_subconjunto(t, c->usados, &n);
	if (sub) {
		char dic[40];

		fmt(dic, sizeof dic, "/Length1 %u", n);
		flujo(s, ff, dic, sub, n, 1);
		so_suelta(sub);
	} else {
		char dic[40];

		s->mal = 1;
		fmt(dic, sizeof dic, "/Length1 %u", t->n);
		flujo(s, ff, dic, t->d, t->n, 1);
	}
	/* ToUnicode */
	buf_txt(&b, "/CIDInit /ProcSet findresource begin\n12 dict begin\nbegincmap\n"
		"/CIDSystemInfo << /Registry (Adobe) /Ordering (UCS) /Supplement 0 >> def\n"
		"/CMapName /Adobe-Identity-UCS def\n/CMapType 2 def\n1 begincodespacerange\n<0000> <FFFF>\nendcodespacerange\n");
	{
		u32 lote = 0, total = 0;

		for (g = 0; g < c->n_glifos; g++)
			if (c->uni[g])
				total++;
		for (g = 0; g < c->n_glifos; g++) {
			u32 u = c->uni[g];

			if (!u)
				continue;
			if (lote == 0)
				buf_fmt(&b, "%u beginbfchar\n", total > 100 ? 100 : total);
			if (u >= 0x10000)
				buf_fmt(&b, "<%04X> <%04X%04X>\n", g, 0xD800 + ((u - 0x10000) >> 10), 0xDC00 + ((u - 0x10000) & 0x3FF));
			else
				buf_fmt(&b, "<%04X> <%04X>\n", g, u);
			total--;
			if (++lote == 100) {
				buf_txt(&b, "endbfchar\n");
				lote = 0;
			}
		}
		if (lote)
			buf_txt(&b, "endbfchar\n");
	}
	buf_txt(&b, "endcmap\nCMapName currentdict /CMap defineresource pop\nend\nend\n");
	if (b.mal)
		s->mal = 1;
	flujo(s, tu, "", b.d, b.n, 1);
	buf_suelta(&b);
}

int pdf_acaba(pdfsal *s, bufer *b)
{
	u32 i, xref, outlines = 0;
	int k;

	if (!s->n_pag)
		pdf_pagina(s, 595.276, 841.89);
	acaba_pagina(s);
	for (k = 0; k < LETRAS_MAX; k++)
		if (s->cara[k].obj)
			escribe_cara(s, k);
	/* los recursos (los mismos para todas las páginas) */
	obj_empieza(s, OBJ_RECURSOS);
	buf_txt(&s->o, "<< /ProcSet [/PDF /Text /ImageB /ImageC] /Font <<");
	for (k = 0; k < LETRAS_MAX; k++)
		if (s->cara[k].obj)
			buf_fmt(&s->o, " /F%d %u 0 R", k, s->cara[k].obj);
	buf_txt(&s->o, " >> /XObject <<");
	for (i = 0; i < s->n_img; i++)
		buf_fmt(&s->o, " /Im%u %u 0 R", i, s->img[i].obj);
	buf_txt(&s->o, " >> >>");
	obj_acaba(s);
	/* las páginas */
	obj_empieza(s, OBJ_PAGINAS);
	buf_txt(&s->o, "<< /Type /Pages /Kids [");
	for (i = 0; i < s->n_pag; i++)
		buf_fmt(&s->o, "%u 0 R ", s->pag_obj[i]);
	buf_fmt(&s->o, "] /Count %u >>", s->n_pag);
	obj_acaba(s);
	if (s->n_mar) {
		outlines = obj_nuevo(s);
		escribe_marcadores(s, outlines);
	}
	obj_empieza(s, OBJ_CATALOGO);
	buf_txt(&s->o, "<< /Type /Catalog /Pages 2 0 R");
	if (outlines)
		buf_fmt(&s->o, " /Outlines %u 0 R /PageMode /UseOutlines", outlines);
	buf_txt(&s->o, " /Lang (es-ES) >>");
	obj_acaba(s);
	{
		int a, me, d, h, mi, se;

		so_ahora(&a, &me, &d, &h, &mi, &se);
		obj_empieza(s, OBJ_INFO);
		buf_txt(&s->o, "<< /Producer ");
		cadena(&s->o, s->productor[0] ? s->productor : "Hispa-Pentium");	/* (0.8.8: antes, siempre «Pluma 2.0») */
		if (s->titulo[0]) {
			buf_txt(&s->o, " /Title ");
			cadena(&s->o, s->titulo);
		}
		if (s->autor[0]) {
			buf_txt(&s->o, " /Author ");
			cadena(&s->o, s->autor);
		}
		if (s->asunto[0]) {
			buf_txt(&s->o, " /Subject ");
			cadena(&s->o, s->asunto);
		}
		buf_fmt(&s->o, " /CreationDate (D:%04d%02d%02d%02d%02d%02d) >>", a, me, d, h, mi, se);
		obj_acaba(s);
	}
	/* la tabla xref */
	xref = s->o.n;
	buf_fmt(&s->o, "xref\n0 %u\n0000000000 65535 f \n", s->n_obj);
	for (i = 1; i < s->n_obj; i++)
		buf_fmt(&s->o, "%010u 00000 n \n", s->off[i]);
	buf_fmt(&s->o, "trailer\n<< /Size %u /Root 1 0 R /Info 4 0 R >>\nstartxref\n%u\n%%EOF\n", s->n_obj, xref);
	for (i = 1; i < s->n_obj; i++)
		if (!s->off[i])
			s->mal = 1;	/* (un objeto apartado y sin escribir: no debería) */
	if (s->o.mal || s->mal)
		return -1;
	buf_pon(b, s->o.d, s->o.n);
	return b->mal ? -1 : 0;
}

void pdf_suelta(pdfsal *s)
{
	u32 i;
	int k;

	if (!s)
		return;
	buf_suelta(&s->o);
	buf_suelta(&s->c);
	so_suelta(s->off);
	so_suelta(s->pag_obj);
	so_suelta(s->pag_al);
	for (k = 0; k < LETRAS_MAX; k++) {
		so_suelta(s->cara[k].usados);
		so_suelta(s->cara[k].uni);
	}
	so_suelta(s->img);
	for (i = 0; i < s->n_mar; i++)
		so_suelta(s->mar[i].texto);
	so_suelta(s->mar);
	so_suelta(s);
}
