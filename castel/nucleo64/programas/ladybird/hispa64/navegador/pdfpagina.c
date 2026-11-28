/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * hispa64/navegador/pdfpagina.c - Hispa-64, etapa 11 a (docs/64/HISPA-64.md,
 * sección 27): la página web a PDF, para «Imprimir…» de NAVEGADOR. Ladybird
 * pinta la página entera (la captura de todo el documento, en PNG: lo que
 * hace el Hub con view_take_and_save_screenshot); aquí se lee (stb_image,
 * solo PNG) y se trocea en hojas A4 con un margen de 1 cm, cada trozo una
 * imagen RGB comprimida con Flate (miniz) que ocupa el ancho de la hoja, como
 * hacen los navegadores con «imprimir a PDF» cuando no maquetan otra vez.
 * Las hojas, de arriba abajo; la última, solo con lo que quede. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS (1 << 17)
#include "stb_image.h"
#include "miniz_cfg.h"
#include "pdfpagina.h"

#define HOJA_AN 595.28
#define HOJA_AL 841.89
#define MARGEN 28.35		/* 1 cm */
#define MAX_ALTO 60000		/* (lo más que se pinta de una página muy larga: unas 30 hojas) */

static void pon_texto_pdf(FILE *f, const char *t)
{
	/* (una cadena de PDF en UTF-16BE con su marca: el título con acentos) */
	const unsigned char *s = (const unsigned char *)t;

	fputs("<FEFF", f);
	while (*s) {
		uint32_t c = *s++;

		if (c >= 0xC0 && c < 0xE0 && (*s & 0xC0) == 0x80)
			c = ((c & 0x1F) << 6) | (*s++ & 0x3F);
		else if (c >= 0xE0 && c < 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80) {
			c = ((c & 0x0F) << 12) | ((uint32_t)(s[0] & 0x3F) << 6) | (s[1] & 0x3F);
			s += 2;
		} else if (c >= 0xF0 && (s[0] & 0xC0) == 0x80 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
			c = ((c & 0x07) << 18) | ((uint32_t)(s[0] & 0x3F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
			s += 3;
		} else if (c >= 0x80)
			c = 0xFFFD;
		if (c >= 0x10000) {
			c -= 0x10000;
			fprintf(f, "%04X%04X", 0xD800 + (unsigned)(c >> 10), 0xDC00 + (unsigned)(c & 0x3FF));
		} else
			fprintf(f, "%04X", (unsigned)c);
	}
	fputs(">", f);
}

int nv_png_a_pdf(const char *png, const char *pdf, const char *titulo, int *hojas, char *error, int cabe)
{
	int an, al, n, h, i;
	unsigned char *px;
	FILE *f;
	long *pos;
	int nobj, k;
	double esc, alto_px;
	char tmp[1100];

	*hojas = 0;
	px = stbi_load(png, &an, &al, &n, 3);
	if (!px) {
		snprintf(error, (size_t)cabe, "no se puede leer lo pintado (%s)", stbi_failure_reason());
		return -1;
	}
	if (al > MAX_ALTO)
		al = MAX_ALTO;
	esc = (HOJA_AN - 2 * MARGEN) / an;			/* puntos de la hoja por punto de la pantalla */
	alto_px = (HOJA_AL - 2 * MARGEN) / esc;
	h = (int)((al + alto_px - 1) / alto_px);
	if (h < 1)
		h = 1;
	snprintf(tmp, sizeof tmp, "%s.nuevo", pdf);
	f = fopen(tmp, "wb");
	if (!f) {
		snprintf(error, (size_t)cabe, "no se puede escribir en %s", pdf);
		stbi_image_free(px);
		return -1;
	}
	/* los objetos: 1 el catálogo, 2 las páginas, 3 los datos; y de cada hoja,
	 * la página, su contenido y su imagen */
	nobj = 3 + 3 * h;
	pos = calloc((size_t)nobj + 1, sizeof *pos);
	fputs("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n", f);
	pos[1] = ftell(f);
	fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", f);
	pos[2] = ftell(f);
	fprintf(f, "2 0 obj\n<< /Type /Pages /Count %d /Kids [", h);
	for (i = 0; i < h; i++)
		fprintf(f, " %d 0 R", 4 + 3 * i);
	fputs(" ] >>\nendobj\n", f);
	pos[3] = ftell(f);
	fputs("3 0 obj\n<< /Title ", f);
	pon_texto_pdf(f, titulo && titulo[0] ? titulo : "Página web");
	fputs(" /Producer (NAVEGADOR de Hispa-64) /Creator (Ladybird) >>\nendobj\n", f);
	for (i = 0; i < h; i++) {
		int y0 = (int)(i * alto_px), y1 = (int)((i + 1) * alto_px);
		int ah;
		double alto_pt;
		size_t zn = 0;
		void *z;
		char c[256];
		int cn;

		if (y1 > al)
			y1 = al;
		ah = y1 - y0;
		if (ah < 1)
			ah = 1;
		alto_pt = ah * esc;
		k = 4 + 3 * i;
		pos[k] = ftell(f);
		fprintf(f, "%d 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f] /Contents %d 0 R /Resources << /XObject << /Im0 %d 0 R >> >> >>\nendobj\n",
			k, HOJA_AN, HOJA_AL, k + 1, k + 2);
		cn = snprintf(c, sizeof c, "q %.4f 0 0 %.4f %.4f %.4f cm /Im0 Do Q\n", an * esc, alto_pt, MARGEN, HOJA_AL - MARGEN - alto_pt);
		pos[k + 1] = ftell(f);
		fprintf(f, "%d 0 obj\n<< /Length %d >>\nstream\n%s\nendstream\nendobj\n", k + 1, cn, c);
		z = tdefl_compress_mem_to_heap(px + (size_t)y0 * an * 3, (size_t)ah * an * 3, &zn, TDEFL_WRITE_ZLIB_HEADER | 128);
		if (!z) {
			fclose(f);
			remove(tmp);
			free(pos);
			stbi_image_free(px);
			snprintf(error, (size_t)cabe, "no hay memoria para comprimir la hoja %d", i + 1);
			return -1;
		}
		pos[k + 2] = ftell(f);
		fprintf(f, "%d 0 obj\n<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /Length %zu >>\nstream\n",
			k + 2, an, ah, zn);
		fwrite(z, 1, zn, f);
		fputs("\nendstream\nendobj\n", f);
		mz_free(z);
	}
	{
		long x = ftell(f);

		fprintf(f, "xref\n0 %d\n0000000000 65535 f \n", nobj + 1);
		for (i = 1; i <= nobj; i++)
			fprintf(f, "%010ld 00000 n \n", pos[i]);
		fprintf(f, "trailer\n<< /Size %d /Root 1 0 R /Info 3 0 R >>\nstartxref\n%ld\n%%%%EOF\n", nobj + 1, x);
	}
	free(pos);
	stbi_image_free(px);
	if (fclose(f) || rename(tmp, pdf)) {
		remove(tmp);
		snprintf(error, (size_t)cabe, "no se ha podido escribir %s", pdf);
		return -1;
	}
	*hojas = h;
	return 0;
}
