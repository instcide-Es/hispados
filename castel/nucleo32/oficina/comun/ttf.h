/* ttf.h - la oficina de 32 bits: las TrueType, leídas sin fiarse de ellas
 * (las pone el usuario en su carpeta: stb_truetype no mira los límites).
 * La comprobación es la de Hispa-PDF (castel/nucleo32/pdf/fuente.c:
 * tt_valida, glifo_bien y cmap_busca); aquí también el nombre de la
 * familia y el subconjunto para meterla en un PDF.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_TTF_H
#define OFI_TTF_H
#include "so.h"

typedef struct {
	const u8 *d;
	u32 n;
	u32 n_glifos, loca, glyf, glyf_n, loca_larga;
	u32 cmap_uni;		/* la subtabla de Unicode (3,1 / 3,10 / 0,x) */
	u32 em;			/* unidades por em */
	int asc, desc, salto;	/* de hhea (desc, negativo) */
	int x0, y0, x1, y1;	/* la caja de head */
	int negrita, cursiva;	/* de OS/2 o head.macStyle */
	int cap;		/* el alto de las mayúsculas (aprox.) */
	u8 *ok;			/* por glifo: 0 sin mirar, 1 bien, 2 mal */
	char familia[64], estilo[32], postscript[64];
} ttf;

/* comprueba la letra (d, n): 1 si se puede usar (y rellena t) */
int ttf_abre(ttf *t, const u8 *d, u32 n);
void ttf_cierra(ttf *t);
/* el glifo de la letra Unicode c (0: no la tiene) */
u32 ttf_glifo(const ttf *t, u32 c);
/* ¿se puede dibujar el glifo g? (sus puntos y compuestos, dentro de glyf) */
int ttf_glifo_vale(ttf *t, u32 g);
/* lo que avanza el glifo (unidades de la letra) */
int ttf_avance(const ttf *t, u32 g);
/* el subconjunto para un PDF: la misma letra con los glifos que no van
 * vacíos (los números de glifo no cambian) y sin las tablas que el PDF no
 * usa. «usados»: un bit por glifo. De so_pide, o NULL */
u8 *ttf_subconjunto(const ttf *t, const u8 *usados, u32 *n);

#endif
