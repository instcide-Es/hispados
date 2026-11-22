/* prueba_pdf.c - Pluma 2: las pruebas de exportar a PDF (pruebas/pdf.sh).
 * Hace con la API de editar documentos de muestra (estilos, listas, una
 * tabla con celdas unidas, imágenes PNG con transparencia y JPG, en la
 * línea y flotando, encabezado y pie con el número de página, las tres
 * DejaVu en normal, negrita y cursiva hecha a mano) y uno de 100 páginas, y
 * los exporta a PDF (y a .PLU) en la carpeta que se le da.
 *   prueba_pdf CARPETA
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "archivo.h"
#include "editar.h"
#include "maqueta.h"
#include "letras.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT(x) ((void)0)
#include "stb_image_write.h"

static void escribe(editor *e, const char *s)
{
	u32 n, *t = u32_de_utf8(s, largo(s), &n);

	if (t) {
		ed_escribe(e, t, n);
		so_suelta(t);
	}
}

static void marca(editor *e, u32 m, int v)
{
	fmt_letra f = ed_formato_actual(e);

	if (m & CF_NEGRITA) f.negrita = (u8)v;
	if (m & CF_CURSIVA) f.cursiva = (u8)v;
	if (m & CF_SUBRAYADO) f.subrayado = (u8)v;
	if (m & CF_TACHADO) f.tachado = (u8)v;
	if (m & CF_VERT) f.vert = (u8)v;
	ed_formato(e, m, &f);
}

static void familia(editor *e, const char *fam, u32 color, u32 resalte)
{
	fmt_letra f = ed_formato_actual(e);

	copia(f.familia, fam, sizeof f.familia);
	f.color = color;
	f.resalte = resalte;
	ed_formato(e, CF_FAMILIA | CF_COLOR | CF_RESALTE, &f);
}

static void a_memoria(void *c, void *d, int n)
{
	buf_pon(c, d, (u32)n);
}

/* una imagen de an x al: PNG con alfa o JPG */
static u8 *haz_imagen(int jpg, int an, int al, u32 *n)
{
	u8 *px = malloc((size_t)an * al * 4);
	bufer b = { 0 };
	int x, y;
	u8 *r;

	for (y = 0; y < al; y++)
		for (x = 0; x < an; x++) {
			u8 *p = px + 4 * (y * an + x);
			int dx = x - an / 2, dy = y - al / 2;

			p[0] = (u8)(x * 255 / an);
			p[1] = (u8)(y * 255 / al);
			p[2] = 160;
			p[3] = dx * dx + dy * dy < an * an / 5 ? 255 : 0;	/* (un círculo, y fuera transparente) */
		}
	if (jpg)
		stbi_write_jpg_to_func(a_memoria, &b, an, al, 4, px, 85);
	else
		stbi_write_png_to_func(a_memoria, &b, an, al, 4, px, an * 4);
	free(px);
	r = so_pide(b.n);
	memcpy(r, b.d, b.n);
	*n = b.n;
	buf_suelta(&b);
	return r;
}

static void campo_en(documento *d, parrafo *p, const char *antes, int estilo)
{
	u16 f = doc_fmt_estilo(d, estilo);
	u32 n, *t = u32_de_utf8(antes, largo(antes), &n), c;

	par_inserta(p, p->n, t, NULL, n);
	so_suelta(t);
	c = CAMPO_PAGINA;
	par_inserta_uno(p, p->n, c, f);
	t = u32_de_utf8(" de ", 4, &n);
	par_inserta(p, p->n, t, NULL, n);
	so_suelta(t);
	par_inserta_uno(p, p->n, CAMPO_PAGINAS, f);
	for (n = 0; n < p->n; n++)
		p->f[n] = f;
	par_cambia(d, p);
}

static pos ps(u32 p, u32 o)
{
	pos r;

	r.p = p;
	r.o = o;
	return r;
}

static documento *variado(void)
{
	documento *d = doc_nuevo();
	editor e;
	u32 n, t0, k;
	u8 *im;

	ed_inicia(&e, d);
	copia(d->titulo, "Informe de prueba de Pluma 2", sizeof d->titulo);
	copia(d->autor, "Carlos Sanz", sizeof d->autor);
	ed_estilo(&e, EST_TITULO);
	escribe(&e, "Informe de prueba");
	ed_intro(&e);
	ed_estilo(&e, EST_T1);
	escribe(&e, "1. Introducción");
	ed_intro(&e);
	escribe(&e, "Texto normal con tildes: áéíóú, la eñe (ñ, Ñ), la diéresis de pingüino, ¿preguntas? y ¡exclamaciones! ");
	marca(&e, CF_NEGRITA, 1);
	escribe(&e, "Esto va en negrita, ");
	marca(&e, CF_NEGRITA, 0);
	marca(&e, CF_CURSIVA, 1);
	escribe(&e, "esto en cursiva, ");
	marca(&e, CF_NEGRITA, 1);
	escribe(&e, "esto en las dos, ");
	marca(&e, CF_NEGRITA | CF_CURSIVA, 0);
	marca(&e, CF_SUBRAYADO, 1);
	escribe(&e, "subrayado");
	marca(&e, CF_SUBRAYADO, 0);
	escribe(&e, " y ");
	marca(&e, CF_TACHADO, 1);
	escribe(&e, "tachado");
	marca(&e, CF_TACHADO, 0);
	escribe(&e, ". Un número con índice: E = mc");
	marca(&e, CF_VERT, 1);
	escribe(&e, "2");
	marca(&e, CF_VERT, 0);
	escribe(&e, ". ");
	familia(&e, FAM_SANS, 0xC00000, SIN_COLOR);
	escribe(&e, "Letra sin remates en rojo, ");
	familia(&e, FAM_MONO, SIN_COLOR, 0xFFFF00);
	escribe(&e, "y de máquina resaltada");
	familia(&e, FAM_SERIF, SIN_COLOR, SIN_COLOR);
	escribe(&e, ". Fin del primer párrafo, que es largo para que se parta en varias líneas y se vea el justificado.");
	{
		fmt_parrafo v = d->par[e.cur.p].p;

		v.alin = AL_JUST;
		ed_formato_parrafo(&e, CP_ALIN, &v);
	}
	ed_intro(&e);
	ed_estilo(&e, EST_T2);
	escribe(&e, "1.1 Listas");
	ed_intro(&e);
	ed_lista(&e, LISTA_VINETAS);
	escribe(&e, "Primera viñeta");
	ed_intro(&e);
	escribe(&e, "Segunda viñeta");
	ed_intro(&e);
	ed_intro(&e);
	ed_lista(&e, LISTA_NUMEROS);
	escribe(&e, "Uno");
	ed_intro(&e);
	escribe(&e, "Dos");
	ed_intro(&e);
	escribe(&e, "Tres");
	ed_intro(&e);
	ed_intro(&e);
	ed_estilo(&e, EST_T1);
	escribe(&e, "2. Tablas");
	ed_intro(&e);
	escribe(&e, "La tabla de abajo tiene celdas unidas:");
	ed_intro(&e);
	ed_tabla(&e, 3, 3);
	t0 = e.cur.p;
	escribe(&e, "Cabecera unida");
	for (k = 1; k < 9; k++) {
		static const char *const c[] = { "", "(tapada)", "Col 3", "Año", "Señal", "Cigüeña", "1.234,56", "7", "Último" };

		ed_celda_siguiente(&e, 0);
		if (k != 1)
			escribe(&e, c[k]);
	}
	ed_elige(&e, ps(t0, 0), ps(t0 + 1, 0));
	ed_une_celdas(&e);
	ed_cursor(&e, ps(t0 + 9, 0), 0);
	ed_estilo(&e, EST_T1);
	escribe(&e, "3. Imágenes");
	ed_intro(&e);
	escribe(&e, "Una imagen PNG con transparencia en la línea: ");
	im = haz_imagen(0, 120, 90, &n);
	ed_imagen(&e, im, n);
	escribe(&e, " y sigue el texto.");
	ed_intro(&e);
	im = haz_imagen(1, 160, 120, &n);
	ed_imagen(&e, im, n);
	ed_elige(&e, ps(e.cur.p, 0), ps(e.cur.p, 1));
	ed_ajuste_imagen(&e, AJUSTE_IZQ);
	ed_cursor(&e, ps(e.cur.p, 1), 0);
	escribe(&e, "Una foto JPG que flota a la izquierda y el texto alrededor. Este párrafo es un poco largo para que el texto se vea a su lado, línea tras línea, hasta pasar la foto entera; y luego sigue debajo como siempre.");
	ed_intro(&e);
	d->hay_enc = d->hay_pie = 1;
	campo_en(d, &d->enc, "Pluma 2 — página ", EST_ENC);
	campo_en(d, &d->pie, "Página ", EST_PIE);
	ed_suelta(&e);
	return d;
}

static documento *cien(int paginas)
{
	documento *d = doc_nuevo();
	editor e;
	int i = 0;

	ed_inicia(&e, d);
	d->hay_pie = 1;
	campo_en(d, &d->pie, "Página ", EST_PIE);
	for (;;) {
		char t[700];

		if (i % 12 == 0) {
			ed_estilo(&e, EST_T1);
			fmt(t, sizeof t, "Capítulo %d", i / 12 + 1);
			escribe(&e, t);
			ed_intro(&e);
		}
		fmt(t, sizeof t, "Párrafo %d. En un lugar de la Mancha, de cuyo nombre no quiero acordarme, no ha mucho tiempo que vivía un hidalgo de los de lanza en astillero, adarga antigua, rocín flaco y galgo corredor. Una olla de algo más vaca que carnero, salpicón las más noches, duelos y quebrantos los sábados.", i + 1);
		escribe(&e, t);
		ed_intro(&e);
		i++;
		if (i % 20 == 0) {
			maq_documento(d);
			if ((int)d->n_pag > paginas)
				break;
		}
	}
	ed_suelta(&e);
	return d;
}

static int exporta(documento *d, const char *carpeta, const char *nombre)
{
	char r[600], real[600], e[300];
	u32 t0 = so_tics();
	int f;

	fmt(r, sizeof r, "%s/%s.pdf", carpeta, nombre);
	if (arch_guarda(d, r, FMT_PDF, real, sizeof real, e, sizeof e)) {
		dice("prueba_pdf: %s: %s", nombre, e);
		return 1;
	}
	dice("prueba_pdf: %s.pdf: %u páginas en %u ms", nombre, d->n_pag, so_tics() - t0);
	fmt(r, sizeof r, "%s/%s.plu", carpeta, nombre);
	if (arch_guarda(d, r, FMT_PLU, real, sizeof real, e, sizeof e)) {
		dice("prueba_pdf: %s: %s", nombre, e);
		return 1;
	}
	/* y de vuelta: el .PLU, abierto y exportado otra vez */
	doc_suelta(d);
	d = arch_abre(r, &f, e, sizeof e);
	if (!d) {
		dice("prueba_pdf: %s", e);
		return 1;
	}
	fmt(r, sizeof r, "%s/%s-plu.pdf", carpeta, nombre);
	if (arch_guarda(d, r, FMT_PDF, real, sizeof real, e, sizeof e)) {
		dice("prueba_pdf: %s", e);
		doc_suelta(d);
		return 1;
	}
	doc_suelta(d);
	return 0;
}

int ofi_principal(const char *args)
{
	char c[600];
	int r = 0;

	letras_inicia();
	copia(c, args && args[0] ? args : ".", sizeof c);
	r |= exporta(variado(), c, "variado");
	r |= exporta(cien(100), c, "cien");
	return r;
}
