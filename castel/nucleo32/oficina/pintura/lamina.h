/* lamina.h - Pintura 2: la imagen que se pinta (la «lámina»): puntos de
 * color verdadero de 32 bits con su transparencia, 0xAARRGGBB (AA = 255
 * opaco, 0 transparente; sin premultiplicar), de cualquier tamaño hasta
 * 4096 x 4096 si hay memoria. Y la mezcla de un color sobre otro.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PIN_LAMINA_H
#define PIN_LAMINA_H
#include "so.h"

#define LAM_MAX_LADO 4096

typedef struct {
	u32 *p;			/* an x al, fila a fila (el paso es an) */
	int an, al;
} lamina;

/* una lámina nueva llena de «fondo» (0 si no hay memoria) */
int lam_nueva(lamina *l, int an, int al, u32 fondo);
void lam_suelta(lamina *l);
/* el trozo (x, y, an, al) de o, en una lámina nueva (lo de fuera, transparente) */
int lam_trozo(lamina *d, const lamina *o, int x, int y, int an, int al);
/* lo que gasta una de an x al (en octetos; u64: puede no caber en u32) */
u64 lam_octetos(int an, int al);
/* ¿tiene algo que no sea opaco? */
int lam_transparente(const u32 *p, u32 n);

/* el color «tinta» (con su alfa) por encima de «fondo», con la cobertura a
 * (0-255: lo suavizado de los bordes) */
u32 lam_sobre(u32 fondo, u32 tinta, int a);
/* «fondo» borrado (hacia lo transparente) en a */
u32 lam_borra(u32 fondo, int a);
/* lo que se ve de c sobre un fondo opaco f (para la pantalla; 0x00RRGGBB) */
u32 lam_sobre_opaco(u32 f, u32 c);

#define ALFA(c) ((c) >> 24)
#define OPACO(c) ((c) | 0xFF000000u)

#endif
