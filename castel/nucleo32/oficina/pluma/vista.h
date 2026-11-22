/* vista.h - Pluma 2: lo que se ve del documento: las páginas una debajo
 * de otra (vista de página) a cualquier zoom, el cursor, lo elegido, las
 * faltas de ortografía (con un trazo ondulado), las marcas de párrafo (¶) y
 * la vista previa de impresión (varias páginas a la vez). Y lo contrario:
 * qué letra hay en un punto de la pantalla.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_VISTA_H
#define PLUMA_VISTA_H
#include "doc.h"
#include "editar.h"
#include "lienzo.h"

#define VISTA_HUECO 16		/* entre las páginas (puntos de pantalla) */

typedef struct {
	documento *d;
	editor *e;
	int x, y, an, al;	/* la zona en la ventana */
	int zoom;		/* en tantos por ciento */
	i32 dy, dx;		/* lo desplazado (puntos de pantalla) */
	int marcas;		/* se ven los ¶ y los espacios */
	int previa;		/* la vista previa: varias páginas, sin cursor */
	int sigue;		/* hay que llevar lo que se ve hasta el cursor */
	i32 x_deseada;		/* (arriba y abajo) la x a la que se quiere volver */
	int foco;		/* la ventana es la activa (el cursor se ve más) */
	/* las faltas de ortografía: quien sabe hacerlo (orto) */
	void (*faltas)(documento *d, parrafo *p);
	int con_faltas;
} vista;

void vista_inicia(vista *v, documento *d, editor *e);
/* de twips a puntos de pantalla y al revés */
int vista_px(const vista *v, i32 tw);
i32 vista_tw(const vista *v, int px);
/* lo que mide una página en la pantalla, y dónde está la página k (en la vista) */
int vista_alto_pag(const vista *v);
int vista_ancho_pag(const vista *v);
void vista_sitio_pag(const vista *v, u32 k, int *x, int *y);
/* lo alto y lo ancho de todo (para las barras de desplazamiento) */
i32 vista_alto_total(const vista *v);
i32 vista_ancho_total(const vista *v);
/* pinta la zona (maqueta lo que haga falta antes) */
void vista_pinta(vista *v, lienzo *l);
/* lleva lo que se ve hasta el cursor; 1 si se ha movido */
int vista_al_cursor(vista *v);
/* la posición del documento en (x, y) de la ventana; 0 si no hay */
int vista_pos_en(vista *v, int x, int y, pos *r);
/* dónde está el cursor en la pantalla (para el menú y para arriba/abajo) */
int vista_cursor_xy(vista *v, pos c, int *x, int *y, int *alto);
/* arriba y abajo una línea, y una pantalla */
pos vista_arriba(vista *v, pos c, int lineas);
pos vista_inicio_linea(vista *v, pos c);
pos vista_fin_linea(vista *v, pos c);
/* la página del cursor (desde 0) */
u32 vista_pagina_cursor(vista *v);
void vista_zoom(vista *v, int zoom);	/* (mantiene a la vista el cursor) */
int vista_zoom_ancho(vista *v);	/* el zoom con que cabe el ancho de la página */

#endif
