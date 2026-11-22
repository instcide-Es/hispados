/* lienzo.h - la oficina de 32 bits: dibujar en un lienzo de puntos
 * 0x00RRGGBB (rectángulos, rayas, rayas suavizadas para los iconos, letras
 * con sus grises, imágenes a escala, el subrayado ondulado del corrector).
 * Todo se recorta a la zona de recorte del lienzo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_LIENZO_H
#define OFI_LIENZO_H
#include "so.h"

typedef struct {
	u32 *p;
	int an, al, paso;
	int rx0, ry0, rx1, ry1;		/* el recorte: [rx0, rx1) x [ry0, ry1) */
} lienzo;

int lz_nuevo(lienzo *l, int an, int al);	/* (de so_pide; 0 si no hay memoria) */
void lz_suelta(lienzo *l);
void lz_recorte(lienzo *l, int x, int y, int an, int al);	/* (dentro del lienzo) */
void lz_sin_recorte(lienzo *l);

u32 lz_mezcla_color(u32 fondo, u32 tinta, int a);	/* a: 0-255 */
void lz_punto(lienzo *l, int x, int y, u32 c);
void lz_punto_a(lienzo *l, int x, int y, u32 c, int a);
void lz_rect(lienzo *l, int x, int y, int an, int al, u32 c);
void lz_rect_a(lienzo *l, int x, int y, int an, int al, u32 c, int a);	/* translúcido */
void lz_marco(lienzo *l, int x, int y, int an, int al, u32 c);
void lz_marco_grueso(lienzo *l, int x, int y, int an, int al, int g, u32 c);
void lz_redondeado(lienzo *l, int x, int y, int an, int al, int r, u32 c);
void lz_marco_redondeado(lienzo *l, int x, int y, int an, int al, int r, u32 c);
void lz_degradado(lienzo *l, int x, int y, int an, int al, u32 arriba, u32 abajo);
void lz_raya_h(lienzo *l, int x, int y, int an, u32 c);
void lz_raya_v(lienzo *l, int x, int y, int al, u32 c);
void lz_punteada_h(lienzo *l, int x, int y, int an, u32 c);
void lz_punteada_v(lienzo *l, int x, int y, int al, u32 c);
/* una raya suavizada, de grosor g (en dieciseisavos de punto: 16 = 1 punto),
 * con los extremos en dieciseisavos */
void lz_linea(lienzo *l, int x0, int y0, int x1, int y1, int g, u32 c);
/* un polígono relleno y suavizado (puntos en dieciseisavos) */
void lz_poligono(lienzo *l, const int *xy, int n, u32 c);
void lz_circulo(lienzo *l, int cx, int cy, int r, u32 c);	/* relleno, suavizado (en dieciseisavos) */
/* el subrayado ondulado (las faltas de ortografía) */
void lz_ondulada(lienzo *l, int x, int y, int an, u32 c);
/* una imagen (an x al, 0x00RRGGBB; con alfa en el octeto alto si «alfa») a
 * escala en (x, y, dan x dal) */
void lz_imagen(lienzo *l, const u32 *px, int an, int al, int x, int y, int dan, int dal, int alfa);
void lz_copia(lienzo *d, int x, int y, const lienzo *o, int ox, int oy, int an, int al);

/* ---- las letras (oficina/comun/letras.h) ---- */
/* el texto (UTF-8) con su línea de base en y; da lo que avanza. tam: en
 * cuartos de punto de pantalla */
int lz_texto(lienzo *l, int cara, int tam, int falsas, int x, int y, const char *t, u32 c);
int lz_ancho(int cara, int tam, int falsas, const char *t);
/* una letra (de la cara, con su respaldo) en x (en cuartos de punto); da lo que avanza (en cuartos) */
int lz_letra(lienzo *l, int cara, int tam, int falsas, int x4, int y, u32 letra, u32 c);
/* lo de la interfaz: la letra de los menús y los botones (DejaVu Sans) */
int lz_ui(lienzo *l, int x, int y, const char *t, u32 c);		/* normal, 13 puntos */
int lz_ui_negrita(lienzo *l, int x, int y, const char *t, u32 c);
int lz_ui_ancho(const char *t);
int lz_ui_ancho_negrita(const char *t);
int lz_ui_alto(void);		/* el renglón */
int lz_ui_base(void);		/* de arriba a la línea de base */
void lz_ui_tamano(int puntos);	/* (13 si no se dice; Ctrl+rueda en la interfaz) */

#endif
