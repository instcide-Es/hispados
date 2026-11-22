/* pinta.h - Hispa-PDF: lo de dentro del pintor (pinta.c), que también usa
 * texto.c para recorrer el contenido de las páginas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_PINTA_H
#define HPDF_PINTA_H
#include "fuente.h"

#define MAX_Q_PILA 64

typedef struct { double a, b, c, d, e, f; } pmat;

typedef struct {
	double *x, *y;
	u8 *ini;		/* 1: empieza un trozo */
	u32 n, cap;
	u32 n_sub;
	int abierto;
} pcamino;

typedef struct {
	int x0, y0, x1, y1;	/* (x1, y1 fuera) */
} prect;

/* el recorte: un rectángulo y, si no es solo eso, una máscara */
typedef struct precorte {
	prect r;
	u8 *m;			/* an x al, o NULL */
	int usos;
} precorte;

/* de qué color se pinta: liso o un degradado */
typedef struct {
	int clase;		/* 0 liso; 1 degradado */
	u32 color;
	double alfa;
	pobj *sombra;		/* (degradado) el diccionario del sombreado */
	pmat inv;		/* de la imagen a su espacio */
} ppintura;

typedef struct {
	pmat ctm;
	pobj *cs_relleno, *cs_trazo;	/* los espacios de color */
	double c_relleno[8], c_trazo[8];
	int n_relleno, n_trazo;
	u32 relleno, trazo;
	pobj *pat_relleno, *pat_trazo;	/* (con /Pattern) */
	double alfa_relleno, alfa_trazo;
	double grosor, inglete;
	int tapa, junta;
	double guion[16];
	int n_guion;
	double fase;
	precorte *rec;
	/* el texto */
	pfuente *fuente;
	double tam, carac, palabra, esc_h, interl, subida;
	int modo;
	pobj *dic_fuente;
} pgestado;

/* lo que se hace con cada letra (texto.c) */
typedef void (*pletra_fn)(void *u, pfuente *f, u32 codigo, const u32 *uni, int n_uni, double x, double y,
			  double ancho, double tam, double ex, double ey);

typedef struct ppintor {
	pdoc *d;
	int an, al;
	u32 *px;		/* NULL: solo se recorre (el texto) */
	pgestado g[MAX_Q_PILA];
	int ng;
	prect rec;		/* (el del recorte de ahora, para cubre) */
	pcamino cam;
	pcamino camt;		/* (las letras) */
	pcamino trazo;		/* (lo que se traza, en polígonos) */
	pdoc *tmp;		/* la memoria de los operandos */
	double cx, cy;		/* el punto de ahora (en el espacio del usuario) */
	double ix, iy;		/* el primero del trozo */
	int recorte_pendiente;	/* W o W* (1 y 2) */
	pmat tm, tlm;		/* las matrices del texto */
	u32 ordenes;
	int formas;
	pletra_fn letra;
	void *u;
	pmat base;		/* (de la página a la imagen) */
	pmat tm_letra;		/* (la de la letra que se dibuja) */
	double lx, ly;
	int ng_base;		/* (lo de antes de un formulario no se saca) */
	pobj *recursos;		/* los de ahora */
	pobj *cs_dev[4];	/* DeviceGray, DeviceRGB, DeviceCMYK y Pattern */
	int en_tipo3;
} ppintor;

#endif
