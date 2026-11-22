/* colores.h - Pintura 2: los colores con su nombre (Carlos es daltónico:
 * el color elegido se dice siempre por su nombre, y el cuentagotas dice el
 * nombre más parecido y el código), la paleta de 28, el código #RRGGBB y
 * el paso a HSV y vuelta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PIN_COLORES_H
#define PIN_COLORES_H
#include "so.h"

#define PALETA_N 28
extern u32 paleta[PALETA_N];			/* 0xAARRGGBB (se pueden cambiar) */
const char *paleta_nombre(int i);		/* el de la de fábrica, o el más parecido */
void paleta_de_fabrica(void);

/* el nombre del color más parecido (de los de la paleta y unos cien más);
 * *exacto, si es ese mismo. «Transparente» si su alfa es 0 */
const char *color_nombre(u32 c, int *exacto);
/* «Rojo (#E02020)», «Azul marino, casi (#101070)», «Rojo translúcido al 50 % (#E02020)» */
void color_describe(u32 c, char *d, u32 cabe);
/* #RRGGBB (o #RRGGBBAA si no es opaco) */
void color_codigo(u32 c, char *d, u32 cabe);
/* de «#RRGGBB», «RRGGBB», «#RGB» o «#RRGGBBAA» (1 si vale) */
int color_de_codigo(const char *s, u32 *c);
/* HSV: h 0-359, s y v 0-100 */
void color_a_hsv(u32 c, int *h, int *s, int *v);
u32 color_de_hsv(int h, int s, int v);

#endif
