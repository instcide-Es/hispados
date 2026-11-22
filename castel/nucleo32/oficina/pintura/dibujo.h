/* dibujo.h - Pintura 2: lo que pinta en la lámina. Las figuras (polígonos,
 * elipses, rectángulos, rayas y curvas) se rasterizan con su cobertura
 * exacta (el área de cada punto que tapan: bordes suavizados sin
 * supermuestreo) en una «máscara», que luego se mezcla con la tinta. Las
 * pinceladas (lápiz, pincel, aerógrafo, borrador) van trozo a trozo con la
 * cobertura de la historia (historia.h), para que lo translúcido no se
 * oscurezca donde el trazo pasa dos veces. Todo llama antes a his_toca().
 * Las coordenadas, en puntos de la lámina (el punto (x, y) es el cuadrado
 * [x, x+1) x [y, y+1); su centro, x + 0,5).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PIN_DIBUJO_H
#define PIN_DIBUJO_H
#include "lamina.h"

/* cómo va la tinta */
enum {
	TINTA_SOBRE,		/* el color (con su alfa) por encima */
	TINTA_PONE,		/* el color tal cual (también su alfa): lápiz, relleno */
	TINTA_BORRA		/* hacia lo transparente (el borrador) */
};

/* las formas del pincel */
enum { FORMA_REDONDA, FORMA_CUADRADA, FORMA_BARRA, FORMA_CONTRABARRA, FORMA_N };

typedef struct {
	int x0, y0, an, al;	/* dónde, en la lámina */
	u8 *a;			/* an x al coberturas (0-255) */
} mascara;

/* una máscara para [x0, x1) x [y0, y1), recortada a la lámina (0 si queda
 * vacía o no hay memoria: m->a es NULL) */
int masc_nueva(mascara *m, const lamina *l, int x0, int y0, int x1, int y1);
void masc_suelta(mascara *m);
/* polígonos (varios contornos, cerrados: los agujeros, al revés), con la
 * regla de «distinto de cero»; xy, los puntos de todos seguidos; cuantos[i],
 * los del contorno i. aa: suavizado */
void masc_poligono(mascara *m, const float *xy, const int *cuantos, int contornos, int aa);
/* una raya de grosor g con las puntas redondas (y las uniones, si se juntan varias) */
void masc_raya(mascara *m, float x0, float y0, float x1, float y1, float g, int aa);
/* la máscara, mezclada en la lámina con la tinta; zona: lo que cambia (se suma) */
void masc_aplica(lamina *l, const mascara *m, u32 color, int modo, int zona[4]);

/* ---- las figuras (con su caja de (x0, y0) a (x1, y1), los dos incluidos) ---- */
enum { RELLENO_NO, RELLENO_SOLO, RELLENO_Y_CONTORNO };
/* el rectángulo, el redondeado y la elipse: el contorno de grosor g por
 * dentro de la caja, en «borde»; el relleno, en «dentro» */
void fig_rectangulo(lamina *l, int x0, int y0, int x1, int y1, int redondo, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4]);
void fig_elipse(lamina *l, int x0, int y0, int x1, int y1, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4]);
/* la raya (de centro a centro de los puntos) */
void fig_raya(lamina *l, int x0, int y0, int x1, int y1, float g, u32 c, int aa, int zona[4]);
/* la curva de Bézier de p0 a p3 con p1 y p2 (pt: 8 enteros) */
void fig_curva(lamina *l, const int *pt, float g, u32 c, int aa, int zona[4]);
/* el polígono (n puntos, x y y seguidos): cerrado, o abierto (mientras se hace) */
void fig_poligono(lamina *l, const int *pt, int n, int cerrado, float g, int relleno, u32 borde, u32 dentro, int aa, int zona[4]);

/* ---- las pinceladas (dentro de un paso de la historia) ---- */
typedef struct {
	u32 color;
	int modo;		/* TINTA_* */
	int forma;		/* FORMA_* */
	float grosor;		/* en puntos */
	int aa;
} pincel;
/* un trozo de la pincelada, de (x0, y0) a (x1, y1) (los centros) */
void trazo(lamina *l, const pincel *p, float x0, float y0, float x1, float y1, int zona[4]);
/* el lápiz: un punto, sin suavizar (de (x0, y0) a (x1, y1), los dos incluidos) */
void trazo_lapiz(lamina *l, u32 color, int x0, int y0, int x1, int y1, int zona[4]);
/* el aerógrafo: «n» gotas al azar en el círculo de radio r alrededor de (x, y) */
void trazo_aerografo(lamina *l, u32 color, int x, int y, int r, int n, u32 *semilla, int zona[4]);

/* ---- el relleno: lo del mismo color (con la tolerancia, 0-100) que (x, y),
 * pegado a él, pasa a ser «color». Da los puntos cambiados (0: nada) ---- */
u32 rellena(lamina *l, int x, int y, u32 color, int tolerancia, int zona[4]);

/* ---- el texto: las letras de la cara (letras.h) en una lámina (sin
 * historia: la caja de texto), con la línea de base en y. tam: en puntos.
 * Da lo que avanza ---- */
int texto_en_lamina(lamina *l, int cara, int tam, int falsas, int x, int y, const u32 *t, u32 n, u32 color);
int texto_ancho(int cara, int tam, int falsas, const u32 *t, u32 n);
/* lo que sube y lo que baja la letra, y el renglón (en puntos) */
void texto_medidas(int cara, int tam, int *sube, int *baja, int *renglon);

/* una lámina encima de otra (en x, y): la «o» por encima (con su alfa) de la
 * «d»; con «sin», los puntos de ese color no van (la selección transparente) */
void lamina_encima(lamina *d, const lamina *o, int x, int y, int transparente, u32 sin, int zona[4]);

/* la zona (x0, y0, x1, y1) que suma otra */
void zona_suma(int zona[4], int x0, int y0, int x1, int y1);

#endif
