/* operar.h - Cuentas 2: lo que se hace con el libro desde la ventana (y
 * desde los guiones de prueba), cada cosa un paso que se puede deshacer
 * (deshacer.h) y que deja el libro recalculado.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_OPERAR_H
#define CU_OPERAR_H
#include "libro.h"

/* escribir lo tecleado en una celda (o en todas las de r: Ctrl+Intro) */
int op_entrada(libro *l, hoja *h, u32 f, u32 c, const char *t, char *err, u32 cabe);
int op_entrada_rango(libro *l, hoja *h, rango r, const char *t, char *err, u32 cabe);
/* borrar: */
enum { BR_CONTENIDO = 1, BR_FORMATO = 2, BR_COMENTARIOS = 4, BR_TODO = 7 };
void op_borra(libro *l, hoja *h, rango r, int que);
/* cambiar el estilo de todo lo de r (las columnas o filas enteras, también
 * las celdas que vengan): la función cambia el estilo que se le da */
typedef void (*op_cambia_estilo)(libro *l, estilo *e, const void *u);
void op_formato(libro *l, hoja *h, rango r, op_cambia_estilo cambia, const void *u, const char *nombre);
/* anchos y altos (0: el de siempre; ocultas con op_oculta) */
void op_ancho(libro *l, hoja *h, u32 c0, u32 c1, u32 an);
void op_alto(libro *l, hoja *h, u32 f0, u32 f1, u32 al);
void op_oculta(libro *l, hoja *h, int eje, u32 de, u32 a, int ocultar);
/* el ancho que necesita lo escrito en las columnas (lo mide quien pinta) */
typedef u32 (*op_mide)(libro *l, hoja *h, const celda *x);
void op_autoancho(libro *l, hoja *h, u32 c0, u32 c1, op_mide mide);
/* meter o quitar filas (eje 0) o columnas (eje 1) */
int op_inserta(libro *l, hoja *h, int eje, u32 desde, u32 n);
int op_elimina(libro *l, hoja *h, int eje, u32 desde, u32 n);
/* copiar y pegar (el portapapeles de Cuentas; el texto, para el del sistema) */
enum { PG_TODO, PG_FORMULAS, PG_VALORES, PG_FORMATOS, PG_COMENTARIOS, PG_SIN_BORDES };
char *op_copia(libro *l, hoja *h, rango r, int cortar, u32 *n);	/* (el texto con tabuladores; de so_pide) */
int op_hay_recorte(void);
int op_recorte_cortado(rango *r, u16 *hoja);
void op_recorte_olvida(void);
/* pega en (f, c); da 0 si va bien (si no, en err por qué); en *pegado, lo que ocupa */
int op_pega(libro *l, hoja *h, u32 f, u32 c, int modo, int transponer, rango *pegado, char *err, u32 cabe);
void op_pega_texto(libro *l, hoja *h, u32 f, u32 c, const char *t, u32 n, rango *pegado);
/* arrastrar lo elegido a otro sitio (como cortar y pegar) */
int op_mueve(libro *l, hoja *h, rango de, u32 f, u32 c, char *err, u32 cabe);
/* rellenar (el controlador): de «fuente» hasta «destino» (que la tiene dentro) */
void op_rellena(libro *l, hoja *h, rango fuente, rango destino);
/* Edición > Rellenar > Series: en r, desde la primera de cada fila o columna */
enum { SE_LINEAL, SE_GEOMETRICA, SE_DIAS, SE_LABORABLES, SE_MESES, SE_ANOS };
void op_series(libro *l, hoja *h, rango r, int en_columnas, int tipo, double paso, int con_limite, double limite);
/* ordenar las filas de r por hasta tres columnas (-1: ninguna) */
void op_ordena(libro *l, hoja *h, rango r, const int col[3], const int desc[3], int con_titulos);
/* el autofiltro: ponerlo en r (o quitarlo), y lo que pide una columna */
void op_autofiltro(libro *l, hoja *h, const rango *r);
void op_filtra(libro *l, hoja *h, const filtro_col *fc);
void op_filtro_aplica(libro *l, hoja *h);	/* (otra vez, sin paso) */
/* combinar */
void op_combina(libro *l, hoja *h, rango r, int centrar);
void op_descombina(libro *l, hoja *h, rango r);
void op_comentario(libro *l, hoja *h, u32 f, u32 c, const char *t);
void op_condicional(libro *l, hoja *h, const condicional *c);	/* (NULL en r: quitar las que pisan) */
void op_quita_condicional(libro *l, hoja *h, rango r);
int op_grafico(libro *l, hoja *h, int i, const grafico *g);	/* i < 0: uno nuevo; g NULL: quitarlo. Da su número */
int op_nombre(libro *l, hoja *h, const char *nombre, const char *ref, char *err, u32 cabe);	/* (ref NULL: quitarlo) */
void op_inmoviliza(libro *l, hoja *h, u32 f, u32 c);
void op_area(libro *l, hoja *h, const rango *r);
void op_salto(libro *l, hoja *h, int eje, u32 pos, int poner);
void op_pagina(libro *l, hoja *h, const pagina *p);
/* las hojas */
hoja *op_hoja_nueva(libro *l, int pos);
int op_hoja_quita(libro *l, u32 i);
int op_hoja_nombre(libro *l, u32 i, const char *n, char *err, u32 cabe);
void op_hoja_mueve(libro *l, u32 de, u32 a);
hoja *op_hoja_copia(libro *l, u32 i, int pos);
/* buscar (desde la de después de (f, c), por filas o por columnas): 1 si la hay */
enum { BU_FORMULAS = 1, BU_ENTERA = 2, BU_CAJA = 4, BU_COLUMNAS = 8, BU_ATRAS = 16 };
int op_busca(libro *l, hoja *h, const char *que, int opc, u32 *f, u32 *c);
int op_reemplaza(libro *l, hoja *h, const char *que, const char *por, int opc, int todas, u32 f, u32 c);
/* las celdas de los textos (para pegar y para los formatos): «A1» -> f, c */
void op_suma_rango(libro *l, hoja *h, rango r, double *suma, u32 *numeros, u32 *llenas);

#endif
