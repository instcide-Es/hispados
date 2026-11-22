/* pinta_pag.h - Pluma 2: el recorrido de una página ya maquetada, igual
 * para la pantalla (vista.c) que para el PDF (f_pdf.c): así lo que se ve es
 * lo que se exporta (WYSIWYG). Da al «pintor» cada trozo de texto con el
 * mismo formato y la x de cada letra, los rectángulos (resaltado, fondos de
 * celda), las rayas (subrayado, tachado, bordes de tabla) y las imágenes;
 * todo en twips desde la esquina de arriba a la izquierda de la página.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_PINTA_PAG_H
#define PLUMA_PINTA_PAG_H
#include "doc.h"
#include "maqueta.h"

typedef struct {
	void *u;
	/* n letras (t) con el mismo formato (f, in) y su línea de base en y; xs,
	 * la x de cada una; «par» y «o0»: el párrafo y la primera letra (para lo
	 * elegido en la pantalla; par = 0xFFFFFFFF en el encabezado y el pie) */
	void (*texto)(void *u, const u32 *t, u32 n, const i32 *xs, i32 y, const fmt_letra *f, const info_fmt *in, u32 par, u32 o0);
	void (*rect)(void *u, i32 x, i32 y, i32 an, i32 al, u32 color);
	/* una raya (horizontal o vertical) de grosor g */
	void (*raya)(void *u, i32 x0, i32 y0, i32 x1, i32 y1, i32 g, u32 color);
	void (*imagen)(void *u, documento *d, int img, i32 x, i32 y, i32 an, i32 al);
	/* (si no es NULL) el sitio de cada línea: para la pantalla (el cursor) */
	void (*linea)(void *u, u32 par, u32 lin, i32 x0, i32 y, i32 x1, i32 alto);
	/* (si no es NULL) ¿hace falta esta zona? (y, alto en twips): para no
	 * recorrer lo que no se ve */
	int (*visible)(void *u, i32 y, i32 al);
} pintor;

void pinta_pagina(documento *d, u32 pag, const pintor *p);
/* el texto de un campo en una página («7», «12») */
void pinta_campo(const documento *d, u32 campo, u32 pag, u32 *t, int cabe);

#endif
