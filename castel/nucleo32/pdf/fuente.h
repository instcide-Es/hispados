/* fuente.h - Hispa-PDF: las letras de los PDF (lo que comparten el texto y
 * el pintado): qué letra es cada código, lo que mide y su dibujo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_FUENTE_H
#define HPDF_FUENTE_H
#include "hpdf.h"

typedef struct {
	u32 desde, hasta;	/* los códigos */
	u32 uni;		/* el primero: uni, uni+1...; o, con n > 1, una cadena */
	u32 *cadena;
	u8 n, bytes;
} ptramo;

typedef struct pfuente {
	pobj *dic;
	pdoc *doc;
	int compuesta;		/* Type0: códigos de 2 octetos (Identity y las de CJK) */
	int tipo3;
	u32 uni[256];		/* (simple) código -> Unicode */
	ptramo *tu;		/* ToUnicode */
	u32 n_tu;
	int bytes_tu[5];	/* (los largos de código del ToUnicode, de las compuestas) */
	double ancho[256];	/* (simple) en milésimas */
	double ancho_def;
	struct { u32 desde, hasta; double w; } *anchos_cid;
	u32 n_anchos_cid;
	/* el dibujo */
	void *tt;		/* stbtt_fontinfo, o NULL */
	int propia;		/* la letra viene en el PDF */
	int simbolica;
	u32 cmap30, cmap31, cmap10;
	/* (la comprobación de la TrueType del PDF: stb_truetype no mira los límites) */
	const u8 *tt_d;
	u32 tt_n, n_glifos, loca, glyf, glyf_n, loca_larga;
	u8 *glifo_ok;		/* 0 sin mirar, 1 bien, 2 mal */
	u16 *cid_gid;
	u32 n_cid_gid;
	double em;		/* 1/unidades por em */
	double fm[6];		/* (Type3) FontMatrix */
	pobj *procs, *recursos3;	/* (Type3) CharProcs y Resources */
	char *nombres[256];	/* (Type3 y las simples) el nombre de cada código */
	int respaldo;		/* cuál de las DejaVu, si no viene la suya */
} pfuente;

pfuente *fuente_de(pdoc *d, pobj *dic);
/* el siguiente código de la cadena s (da cuántos octetos lleva) */
int fuente_codigo(pfuente *f, const u8 *s, u32 n, u32 *codigo);
/* lo que es en Unicode (hasta max letras en u) */
int fuente_unicode(pfuente *f, u32 codigo, u32 *u, int max);
double fuente_ancho(pfuente *f, u32 codigo);	/* en milésimas del tamaño */
/* el dibujo de la letra: llama a la función con las rayas (en unidades
 * del tamaño: 1 = el tamaño de la letra); da 0 si no hay dibujo */
typedef struct {
	void *u;
	void (*mueve)(void *u, double x, double y);
	void (*linea)(void *u, double x, double y);
	void (*curva)(void *u, double x1, double y1, double x2, double y2, double x3, double y3);
	void (*cierra)(void *u);
} ptrazo;
int fuente_dibujo(pfuente *f, u32 codigo, ptrazo *t, double *escala_h);
void fuentes_suelta(pdoc *d);
#endif
