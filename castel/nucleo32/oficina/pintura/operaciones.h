/* operaciones.h - Pintura 2: lo del menú Imagen y del menú Colores sobre
 * una lámina (la imagen entera o lo seleccionado): cambiar el tamaño
 * (reduciendo con la media del área que cae en cada punto, ampliando con
 * el bilineal; con la transparencia bien llevada), inclinar, girar 90, 180
 * o lo que se diga, voltear, recortar, el tamaño del lienzo, invertir los
 * colores, la escala de grises y el brillo y el contraste. Lo que cambia
 * el tamaño hace una lámina nueva en «d» (0 si no hay memoria).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PIN_OPERACIONES_H
#define PIN_OPERACIONES_H
#include "lamina.h"

int op_escala(const lamina *o, lamina *d, int an, int al);
/* inclinar: grados a lo ancho (-89..89) y a lo alto; lo que queda al
 * descubierto, de «fondo» */
int op_inclina(const lamina *o, lamina *d, int gh, int gv, u32 fondo);
int op_gira90(const lamina *o, lamina *d, int veces);	/* 1 a la derecha, 2 media vuelta, 3 a la izquierda */
int op_gira(const lamina *o, lamina *d, double grados, u32 fondo);	/* (a la derecha; el lienzo crece) */
int op_lienzo(const lamina *o, lamina *d, int an, int al, u32 fondo);	/* (lo de antes, arriba a la izquierda) */
/* en el sitio */
void op_voltea(lamina *l, int horizontal);
void op_invierte(lamina *l);
void op_grises(lamina *l);
void op_brillo(lamina *l, int brillo, int contraste);	/* (-100..100) */

double op_seno(double x);
double op_coseno(double x);

#endif
