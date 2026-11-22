/* grafico.h - Cuentas 2: los gráficos (columnas, barras, líneas, sectores
 * y dispersión) de los datos de la hoja, con su título, ejes, rayas guía y
 * leyenda. Cada serie se distingue por su trama (columnas, barras,
 * sectores), su marcador y su raya (líneas, dispersión) además de por su
 * color: se leen igual en gris o sin ver los colores.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_GRAFICO_H
#define CU_GRAFICO_H
#include "libro.h"
#include "pintor.h"

/* pinta g (sus datos, de su hoja del libro l) en el rectángulo (x, y, an, al)
 * del pintor; esc: unidades del pintor por punto a 100% */
void gr_pinta(pintor *p, libro *l, const grafico *g, double x, double y, double an, double al, double esc);
const char *gr_nombre_tipo(int tipo);	/* «Columnas»... */
/* un gráfico nuevo para los datos r de h (el tipo; lo demás, lo de siempre) */
void gr_nuevo(grafico *g, hoja *h, rango r, int tipo);
/* lo de cada serie (para la leyenda y los cuadros): su trama, su marcador, su color */
int gr_trama_serie(int i);
int gr_marcador_serie(int i);
u32 gr_color_serie(int i);

#endif
