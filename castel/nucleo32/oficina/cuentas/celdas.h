/* celdas.h - Cuentas 2: pintar un trozo de hoja (los fondos con su trama,
 * la cuadrícula, los textos con su letra, su alineación y lo que se sale a
 * las de al lado, las combinadas, los bordes, el formato condicional con
 * su símbolo y la marca de los comentarios) en un pintor (pintor.h): la
 * vista de la ventana, la vista preliminar y el PDF lo usan igual.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_CELDAS_H
#define CU_CELDAS_H
#include "libro.h"
#include "pintor.h"

/* lo que se pinta: las filas f0..f1 y las columnas c0..c1 de h, con la
 * esquina de (f0, c0) en (x, y) del pintor; «esc» son las unidades del
 * pintor por punto a 100% (1 en la pantalla a 100%, 0,75 en el PDF...) */
typedef struct {
	libro *l;
	hoja *h;
	u32 f0, f1, c0, c1;
	double x, y, esc;
	int rejilla;		/* las líneas de división (las de la hoja, en gris) */
	int comentarios;	/* la marca de los que tienen comentario (en la pantalla) */
	int sin_fondo;		/* (no pintar el blanco de debajo) */
	u32 color_rejilla;
} trozo_hoja;
void cel_pinta(pintor *p, const trozo_hoja *t);

/* lo que mide (en puntos a 100%) lo que se ve en la celda: para autoajustar */
u32 cel_ancho_texto(libro *l, hoja *h, const celda *x);
/* la letra de un estilo (cara de letras.h y lo que hay que hacer a mano) */
int cel_cara(const estilo *e, int *falsas);
/* el formato condicional que manda en (f, c) (o NULL) */
const condicional *cel_condicional(libro *l, hoja *h, u32 f, u32 c);

#endif
