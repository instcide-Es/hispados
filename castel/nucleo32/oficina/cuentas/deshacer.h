/* deshacer.h - Cuentas 2: deshacer y rehacer sin límite. Cada paso guarda,
 * antes de cambiar nada, lo que había: regiones de celdas (todas las de un
 * rango, también las que no existen: al deshacer, lo que haya en él se
 * cambia por lo guardado), lo de una hoja (anchos, combinadas, comentarios,
 * gráficos, condicionales...), sus altos y filas ocultas, y lo del libro
 * (las hojas y los nombres). Deshacer cambia lo de ahora por lo guardado
 * (y lo guarda: así rehacer es lo mismo al revés).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_DESHACER_H
#define CU_DESHACER_H
#include "libro.h"

void dh_empieza(libro *l, const char *nombre);	/* un paso (los de dentro, uno solo) */
void dh_acaba(libro *l);
void dh_region(libro *l, hoja *h, rango r);	/* lo de esas celdas, antes de cambiarlas */
void dh_celda(libro *l, hoja *h, u32 f, u32 c);
void dh_meta(libro *l, hoja *h);		/* lo de la hoja (una vez por paso) */
void dh_altos(libro *l, hoja *h);		/* los altos, las ocultas y los estilos de las filas */
void dh_libro(libro *l);			/* las hojas y los nombres */
void dh_vista(libro *l, hoja *h);		/* (dónde estaba el cursor: se vuelve allí) */
int dh_deshace(libro *l);	/* 0 si no había nada */
int dh_rehace(libro *l);
int dh_hay(const libro *l, int rehacer);
const char *dh_nombre(const libro *l, int rehacer);	/* «Deshacer Escribir en B7» */
void dh_suelta(libro *l);	/* (al soltar el libro; antes que lib_suelta) */
u32 dh_memoria(const libro *l);
void dh_olvida(libro *l);	/* sin pasos (al abrir otro) */

#endif
