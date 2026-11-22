/* pintor.h - Cuentas 2: pintar lo mismo en la pantalla (un lienzo de
 * oficina/comun) y en un PDF (oficina/comun/pdfsal): las celdas, sus
 * bordes y tramas, los textos y los gráficos se pintan una vez, con estas
 * funciones, y salen iguales en los dos. Las medidas, en las unidades del
 * pintor (puntos de pantalla en el lienzo; puntos de 1/72" en el PDF),
 * desde arriba a la izquierda; los colores, 0xRRGGBB.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_PINTOR_H
#define CU_PINTOR_H
#include "so.h"
#include "lienzo.h"
#include "pdfsal.h"

typedef struct pintor pintor;
struct pintor {
	void *u;		/* (el lienzo o el pdfsal) */
	int pdf;		/* 1: es un PDF */
	double rx0, ry0, rx1, ry1;	/* el recorte de ahora */
	double pila_r[16][4];	/* (los recortes guardados) */
	int n_pila;
	/* lo de cada uno */
	void (*rect)(pintor *p, double x, double y, double an, double al, u32 c);
	void (*raya)(pintor *p, double x0, double y0, double x1, double y1, double g, u32 c);
	void (*poligono)(pintor *p, const double *xy, int n, u32 c);	/* relleno (n <= 64 puntos) */
	/* el texto UTF-8 (n octetos) con la línea de base en y; tam: lo que mide la letra (el em) */
	void (*texto)(pintor *p, int cara, int falsas, double tam, double x, double y, const char *t, u32 n, u32 c);
	void (*recorta)(pintor *p);	/* (aplica rx0..ry1) */
};

/* el pintor de un lienzo y el de un PDF (con su página ya empezada) */
void pt_de_lienzo(pintor *p, lienzo *l);
void pt_de_pdf(pintor *p, pdfsal *s, double an_pag, double al_pag);
/* el recorte: guardar el de ahora y cortar más (la intersección), y volver al de antes */
void pt_recorte(pintor *p, double x, double y, double an, double al);
void pt_vuelve(pintor *p);
/* lo que mide un texto (lo mismo en los dos: las milésimas de em de letras.h) */
double pt_ancho(int cara, int falsas, double tam, const char *t, u32 n);
/* un rectángulo con su trama (TR_* de libro.h): el fondo y las rayas o los puntos */
void pt_trama(pintor *p, double x, double y, double an, double al, int trama, u32 fondo, u32 color);
/* lo mismo dentro de un polígono convexo (los sectores) */
void pt_trama_pol(pintor *p, const double *xy, int n, int trama, u32 fondo, u32 color);
/* los marcadores de las series (MK_*), en (cx, cy), de radio r: se distinguen por la forma */
enum { MK_NADA, MK_CUADRADO, MK_CIRCULO, MK_TRIANGULO, MK_ROMBO, MK_ASPA, MK_CRUZ, MK_ESTRELLA, MK_TRIANGULO_ABAJO, MK_N };
void pt_marcador(pintor *p, int tipo, double cx, double cy, double r, u32 c);
/* una raya de puntos o de guiones (los bordes y las rayas de las series de líneas) */
void pt_raya_trazos(pintor *p, double x0, double y0, double x1, double y1, double g, u32 c, int estilo);

#endif
