/* maqueta.h - Pluma 2: la maquetación. Parte cada párrafo en líneas (por
 * lo que mide cada letra en su TrueType, en twips: lo mismo en la pantalla
 * a cualquier zoom que en el PDF) y reparte los párrafos en páginas. Es
 * incremental: un párrafo solo se rehace si ha cambiado (o si cambia su
 * ancho), y repartir en páginas es solo sumar lo alto de cada uno; así un
 * documento de cientos de páginas va igual de ligero al escribir.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_MAQUETA_H
#define PLUMA_MAQUETA_H
#include "doc.h"

#define TAB_DEFECTO 709			/* cada 1,25 cm */
#define HUECO_FLOTA 180			/* entre una imagen que flota y el texto */

/* lo que se sabe de cada formato de letra (para medir y pintar) */
typedef struct {
	int cara, falsas;		/* (letras.h) */
	i32 tam;			/* el tamaño de verdad (medios puntos; menor en super/subíndice) */
	i32 asc, desc;			/* en twips (desc, positivo) */
	i32 sube;			/* lo que sube (superíndice) o baja (negativo) la línea de base */
	int mayus;			/* versalitas: se escribe en mayúsculas, más pequeñas */
} info_fmt;
const info_fmt *maq_info(documento *d, u16 f);

/* lo que avanza la letra c (con el formato f), en twips; «x» (desde el
 * borde del texto) para las tabulaciones */
i32 maq_avance(documento *d, const parrafo *p, u32 o, i32 x);
/* el texto de la viñeta o del número del párrafo i («•», «3.») */
void maq_marca_lista(documento *d, u32 i, u32 *t, int cabe);

/* maqueta un párrafo para ese ancho (si no lo estaba ya) */
void maq_parrafo(documento *d, parrafo *p, i32 ancho);
/* el ancho para el que se maqueta el párrafo i (el del texto, o el de su celda) */
i32 maq_ancho_de(documento *d, u32 i);
/* todo: los párrafos que han cambiado y las páginas */
void maq_documento(documento *d);
/* lo alto de una fila de tabla (desde el párrafo i, el primero de la fila);
 * en *fin, el párrafo de después de la fila */
i32 maq_fila(documento *d, u32 i, u32 *fin);
/* dónde empieza la columna c de la tabla t (desde el borde del texto) y lo que mide la celda */
i32 maq_x_col(const documento *d, u32 t, u32 c);
i32 maq_ancho_celda(const documento *d, const parrafo *p);

/* la x (desde el borde del texto) de la letra o de la línea l del párrafo p */
i32 maq_x_de(documento *d, const parrafo *p, const linea *l, u32 o);
/* la letra de la línea l más cerca de x */
u32 maq_o_de(documento *d, const parrafo *p, const linea *l, i32 x);
/* la línea del párrafo que lleva la letra o (con «al_final»: si o es el
 * final de una línea partida, la de antes) */
u32 maq_linea_de(const parrafo *p, u32 o, int al_final);

/* dónde está cada cosa en la página: el trozo y la página del párrafo i
 * (de la línea l); -1 si no está */
int maq_donde(const documento *d, u32 i, u32 l, u32 *pagina, u32 *trz);
/* la y (desde arriba de la zona del texto de su página) del párrafo i en el trozo */
i32 maq_y_parrafo(documento *d, const trozo *t, u32 i);

#endif
