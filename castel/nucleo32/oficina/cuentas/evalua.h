/* evalua.h - Cuentas 2: lo que comparten el evaluador (calculo.c) y las
 * funciones (funciones.c): los valores a medio calcular y cómo se piden.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_EVALUA_H
#define CU_EVALUA_H
#include "calculo.h"

/* un valor a medio calcular (V_VACIO, V_NUM, V_TXT, V_LOG, V_ERR o V_REF) */
typedef struct {
	u8 t, e;
	u16 h;			/* (V_REF) el id de la hoja */
	double n;
	const char *s;		/* (V_TXT) en la memoria del cálculo; sn octetos */
	u32 sn;
	rango r;		/* (V_REF) */
} ev;

typedef struct ev_trozo ev_trozo;
typedef struct {
	libro *l;
	hoja *h;		/* la de la fórmula */
	u32 f, c;		/* su celda */
	ev_trozo *mem;		/* la memoria de los textos */
	int hondo;		/* (fórmulas que piden otras sin calcular) */
} ctx;

/* memoria para los textos (dura lo que el cálculo de la celda) */
char *ev_pide(ctx *x, u32 n);
void ev_txt(ctx *x, ev *r, const char *s, u32 n);	/* (se copia) */
void ev_num(ev *r, double n);
void ev_log(ev *r, int v);
void ev_err(ev *r, int e);
/* el valor de una celda de otra hoja (o de esta); la calcula si está pendiente */
const valor *ev_celda(ctx *x, u16 h, u32 f, u32 c);
/* de un rango (V_REF) a su valor (la celda, o la intersección con la fila
 * o la columna de la fórmula); lo demás, tal cual */
void ev_valor(ctx *x, const ev *a, ev *r);
/* a número (los textos que lo parecen valen; el vacío es 0); el error, si no se puede */
int ev_a_num(ctx *x, const ev *a, double *n);
/* a texto (los números, como en General); en *s y *n */
int ev_a_txt(ctx *x, const ev *a, const char **s, u32 *n);
int ev_a_log(ctx *x, const ev *a, int *v);
/* las celdas de un rango, una a una (las que tienen algo, si es muy grande:
 * entonces no vienen por orden); la función da 1 para parar */
typedef int (*ev_cada)(void *u, const valor *v, u32 f, u32 c);
void ev_recorre(ctx *x, const ev *ref, int todas, ev_cada cb, void *u);
/* comparar dos valores como las hojas de cálculo (números < textos < lógicos;
 * los textos sin mirar mayúsculas): <0, 0, >0 */
int ev_compara(ctx *x, const ev *a, const ev *b);
/* (funciones.c) */
void fn_llama(ctx *x, int id, ev *a, int n, ev *r);
/* comparar textos sin mirar mayúsculas ni tildes (con ? y * si «comodin») */
int ev_texto_igual(const char *a, u32 na, const char *b, u32 nb, int comodin);
int ev_texto_cmp(const char *a, u32 na, const char *b, u32 nb);

#endif
