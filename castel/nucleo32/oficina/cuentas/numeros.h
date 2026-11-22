/* numeros.h - Cuentas 2: los números como texto. Leer lo que se teclea
 * (1.234,5; 12 %; 3,50 €; 22/11/2026; 14:30), escribirlos con su formato
 * (los códigos de las hojas de cálculo de siempre: «#,##0.00 €», «0%»,
 * «dd/mm/yyyy», «[Red]-0.0», con secciones «;»: se guardan como en los
 * .XLSX, en inglés, y se enseñan con la coma y el punto de quien usa
 * Cuentas), las fechas (los días desde el 30/12/1899, como todas) y sus
 * nombres en español; y los números de C para los archivos (que vuelven
 * exactos: num_a_texto_c y num_de_texto_c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_NUMEROS_H
#define CU_NUMEROS_H
#include "libro.h"

/* ---- los de C (para los archivos): «1.5», «-2.5E-07»; el más corto que vuelve igual ---- */
void num_a_texto_c(double x, char *d, u32 cabe);
double num_de_texto_c(const char *s);
/* (lee un número de C al principio de s; 0 si no hay; en *fin, detrás) */
int num_lee_c(const char *s, double *x, const char **fin);

/* ---- lo que se teclea ---- */
enum { NE_NADA, NE_PORCENTAJE, NE_MONEDA, NE_FECHA, NE_HORA, NE_HORA_SEG, NE_FECHA_HORA, NE_CIENTIFICO, NE_MILES, NE_MILES_DEC };
/* ¿es un número? (1); en *tipo, el formato que trae (NE_*) */
int num_lee_entrada(const char *s, const cu_local *loc, const char *moneda, double *x, int *tipo);
/* el código del formato que trae lo tecleado (NE_*) */
const char *num_fmt_de_entrada(int tipo, const char *s, const cu_local *loc, const char *moneda);

/* ---- escribir con formato ---- */
/* el número x con el código (en *color, el de [Rojo] o CU_AUTO) */
void num_formatea(double x, const char *codigo, const cu_local *loc, char *d, u32 cabe, u32 *color);
/* un texto con el código (la sección de texto: «"Sr. "@») */
void num_formatea_texto(const char *t, const char *codigo, char *d, u32 cabe);
/* General, con «max» letras como mucho (0: 11) */
void num_general(double x, const cu_local *loc, int max, char *d, u32 cabe);
/* como se corrige (la fecha como fecha, el % como %; si no, todas las cifras) */
void num_edicion(double x, const char *codigo, const cu_local *loc, char *d, u32 cabe);
/* lo de TEXTO(x; "0,00"): el código escrito a lo español */
void num_texto_funcion(double x, const char *codigo_local, const cu_local *loc, char *d, u32 cabe);
/* x con d decimales (redondeado como REDONDEAR: la mitad, lejos de cero) */
double num_redondea(double x, int d);

/* ---- los códigos ---- */
enum { CAT_GENERAL, CAT_NUMERO, CAT_MONEDA, CAT_PORCENTAJE, CAT_FECHA, CAT_HORA, CAT_CIENTIFICA, CAT_TEXTO, CAT_PERSONAL };
int fmt_categoria(const char *codigo);
int fmt_es_fecha(const char *codigo);
int fmt_decimales(const char *codigo);	/* los de la primera sección */
/* el código para quien lo lee (#.##0,00; dd/mm/aaaa) y de vuelta */
void fmt_a_local(const char *codigo, const cu_local *loc, char *d, u32 cabe);
void fmt_de_local(const char *t, const cu_local *loc, char *d, u32 cabe);
/* uno con tantos decimales más (o menos) que «codigo» (los botones) */
void fmt_cambia_decimales(const char *codigo, int mas, char *d, u32 cabe);

/* ---- las fechas: los días desde el 30/12/1899 (y la hora, la parte de detrás de la coma) ---- */
double fecha_serie(int ano, int mes, int dia);	/* (los meses y días de más se pasan al siguiente) */
void serie_fecha(double s, int *ano, int *mes, int *dia);
void serie_hora(double s, int *h, int *mi, int *se);
int dia_semana(double s);		/* 0 domingo ... 6 sábado */
int dias_mes(int ano, int mes);
extern const char *const MESES[12];
extern const char *const MESES_CORTOS[12];
extern const char *const DIAS[7];
extern const char *const DIAS_CORTOS[7];

#endif
