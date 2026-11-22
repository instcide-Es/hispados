/* lienzo.h - Correo: (PARPADEO) el lienzo propio de la ventana (lienzo.c):
 * se pinta aquí y se vuelca de una vez (ui_vuelca), con las filas cambiadas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_LIENZO_H
#define CORREO_LIENZO_H
#include "so.h"

typedef struct {
	u32 *p;			/* los puntos, 0x00RRGGBB, fila a fila (an por fila) */
	int an, al;
	int sucio, sy0, sy1;	/* las filas que hay que volcar: [sy0, sy1) */
	int callado;		/* lo que se pinta no ensucia (lo de debajo de un cuadro, ya en la pantalla) */
} Lienzo;

int lz_crea(Lienzo *l, int an, int al);
void lz_suelta(Lienzo *l);
void lz_ensucia(Lienzo *l, int y, int al);
void lz_rect(Lienzo *l, int x, int y, int an, int al, u32 c);
void lz_marco(Lienzo *l, int x, int y, int an, int al, u32 c);
void lz_redondo(Lienzo *l, int x, int y, int an, int al, int radio, u32 arriba, u32 abajo);
void lz_degradado(Lienzo *l, int x, int y, int an, int al, u32 arriba, u32 abajo);
void lz_linea(Lienzo *l, int x1, int y1, int x2, int y2, u32 c);
void lz_mapa(Lienzo *l, int x, int y, int an, int al, const u32 *p);
void lz_texto(Lienzo *l, int x, int y, const u8 *t, int n, u32 c, int negrita);
#endif
