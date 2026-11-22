/* calculo.h - Cuentas 2: calcular las fórmulas. Cada fórmula apunta de qué
 * celdas y rangos depende; al cambiar una celda se marcan las que dependen
 * de ella (y las que dependen de esas...) y solo esas se calculan, en el
 * orden de sus dependencias (de Kahn: primero lo que no depende de nada
 * pendiente). Lo que queda (un círculo: A1 pide B1 y B1 pide A1) da
 * #¡CIRC! y se apunta en libro->circular. Las volátiles (HOY, AHORA,
 * ALEATORIO, INDIRECTO, DESREF) se calculan cada vez.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_CALCULO_H
#define CU_CALCULO_H
#include "libro.h"
#include "formula.h"

int calc_inicia(libro *l);			/* 0 si va bien */
void calc_suelta(libro *l);
void calc_suelta_hoja(hoja *h);
/* (libro.c) la celda tiene fórmula nueva / se le va a quitar / cambió su valor */
void calc_mete_formula(libro *l, hoja *h, celda *x);
void calc_quita_formula(libro *l, hoja *h, celda *x);
void calc_cambia(libro *l, hoja *h, u32 f, u32 c);
/* hay que rehacer todas las dependencias y calcularlo todo (tras meter o
 * quitar filas, ordenar, abrir un archivo...); se hace en calc_recalcula */
void calc_rehaz(libro *l);
/* la hoja sale del libro (se quita, o se deshace su alta): sus fórmulas olvidan lo apuntado */
void calc_hoja_fuera(libro *l, hoja *h);
/* calcula lo pendiente; da cuántas fórmulas ha calculado */
u32 calc_recalcula(libro *l);
/* todo otra vez, ya */
u32 calc_todo(libro *l);
/* el valor de una fórmula suelta, como si estuviera en (f, c) de h (la del
 * formato condicional, un nombre...): en *res (hay que soltarlo) */
void calc_evalua(libro *l, hoja *h, u32 f, u32 c, const formula *fm, valor *res);
/* lo que tardó el último cálculo (en tics) y cuántas */
extern u32 calc_ultimas, calc_tics;

#endif
