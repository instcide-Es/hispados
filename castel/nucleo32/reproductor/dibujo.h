/* dibujo.h - el Reproductor: dibujar en un lienzo de 32 bits (0x00RRGGBB):
 * rectángulos, redondeados, degradados, líneas, triángulos, círculos, las
 * letras DejaVu ya hechas puntos (letras.c, de haz_letras.c) mezcladas con lo
 * de debajo, e imágenes. Todo se recorta a un rectángulo. Portable: lo mismo
 * pinta en Hispa-Pentium y en el anfitrión (las capturas de las pruebas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_DIBUJO_H
#define REPRO_DIBUJO_H
#include "so.h"

typedef struct { u16 cp; u8 an, al; signed char dx, dy; u8 avance; u32 off; } letra;
typedef struct { int alto, linea, n; const letra *l; const u8 *p; } fuente;
extern const fuente f_normal12, f_normal14, f_negrita14, f_negrita20, f_negrita12;

typedef struct { int x, y, an, al; } rect;

void dib_lienzo(lienzo *l);		/* en cuál se pinta */
void dib_recorte(int x, int y, int an, int al);	/* lo que se puede tocar (0, 0, 0, 0: todo) */
void dib_rect(int x, int y, int an, int al, u32 c);
void dib_borde(int x, int y, int an, int al, u32 c);
void dib_redondo(int x, int y, int an, int al, int r, u32 c);
void dib_redondo_borde(int x, int y, int an, int al, int r, u32 c);
void dib_degradado(int x, int y, int an, int al, u32 arriba, u32 abajo);
void dib_redondo_degradado(int x, int y, int an, int al, int r, u32 arriba, u32 abajo);
void dib_linea(int x0, int y0, int x1, int y1, u32 c);
void dib_triangulo(int x0, int y0, int x1, int y1, int x2, int y2, u32 c);
void dib_circulo(int cx, int cy, int r, u32 c);
void dib_mezcla(int x, int y, u32 c, int alfa);	/* un punto, con alfa (0 a 255) */
/* las letras (UTF-8): dónde acaba */
int dib_texto(const fuente *f, int x, int y, const char *t, u32 c);
int dib_ancho(const fuente *f, const char *t);
/* el texto, recortado con «…» para que quepa en «an» puntos */
int dib_texto_corto(const fuente *f, int x, int y, int an, const char *t, u32 c);
void dib_texto_centrado(const fuente *f, int x, int y, int an, const char *t, u32 c);
/* una imagen (0xAARRGGBB, de «ia» x «il»), puesta en el rectángulo */
void dib_imagen(int x, int y, int an, int al, const u32 *img, int ia, int il);
u32 dib_mezcla_color(u32 a, u32 b, int t);	/* de a a b, t de 0 a 256 */
#endif
