/* f_pdf.h - Cuentas 2: repartir una hoja en páginas (para exportar a PDF
 * y para la vista preliminar) y pintar una página en cualquier pintor.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_F_PDF_H
#define CU_F_PDF_H
#include "libro.h"
#include "pintor.h"

typedef struct {
	u32 f0, f1, c0, c1;	/* lo que va en la página */
} pdf_trozo;

typedef struct {
	double an, al;		/* el papel, en puntos (1/72") */
	double mx, my;		/* dónde empieza lo impreso (los márgenes, y lo centrado) */
	double esc;		/* puntos del papel por punto de pantalla a 100% (0,75 * escala) */
	u32 n;			/* cuántas páginas */
	pdf_trozo *t;		/* (de so_pide) */
	rango zona;		/* lo que se imprime */
} pdf_plan;

/* reparte la hoja (el área de impresión o lo usado, con los gráficos) en
 * páginas: primero hacia abajo y luego a la derecha, con los saltos puestos
 * a mano; 0 si va bien */
int pdf_plan_hoja(libro *l, hoja *h, pdf_plan *pl);
void pdf_plan_suelta(pdf_plan *pl);
/* cuántas páginas saldrían */
int pdf_paginas(libro *l, hoja *h);
/* pinta la página «pag» (desde 0; de «total» en el libro, desde «primera»)
 * con su papel en (x, y) del pintor y k unidades del pintor por punto del
 * papel (el PDF: 0, 0, 1). «archivo»: lo de &F */
void pdf_pinta_pagina(pintor *p, libro *l, hoja *h, const pdf_plan *pl, u32 pag, double x, double y, double k,
		      const char *archivo, u32 primera, u32 total);

#endif
