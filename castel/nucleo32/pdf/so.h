/* so.h - Hispa-PDF: lo que el programa pide al sistema. Es lo único que
 * cambia de un sistema a otro: so_pentium.c (Hispa-Pentium, el anillo 3 con
 * la int 0x80) y so_anfitrion.c (Linux, para las pruebas; y lo mismo valdrá
 * para Hispa-64). Todo lo demás de castel/nucleo32/pdf es C99 portable, sin
 * nada del sistema.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HPDF_SO_H
#define HPDF_SO_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

/* la memoria (malloc, realloc y free de siempre; NULL si no hay) */
void *so_pide(size_t n);
void *so_repide(void *p, size_t n);
void so_suelta(void *p);

/* un archivo entero a la memoria (de so_pide); NULL si no se puede o si
 * mide más de «max». En *error, por qué (texto en español) */
u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max, const char **error);

/* escribir un archivo a trozos: abrirlo (creándolo o vaciándolo), los
 * trozos y cerrarlo. 0 si va bien */
void *so_crea(const char *ruta, const char **error);
int so_escribe(void *f, const void *d, u32 n);
int so_cierra(void *f);
int so_borra(const char *ruta);

/* lo que dice el programa (texto UTF-8; el sistema lo pasa a su página) */
void so_dice(const char *t);
/* la página de códigos del sistema para los textos: 437 o 0 (UTF-8) */
int so_pagina(void);

/* los argumentos (texto, en la página del sistema) */
int so_argumentos(char *d, int cabe);

/* los tics (para medir) y su frecuencia; el azar (para el cifrado) */
u32 so_tics(void);
u32 so_hz(void);
void so_azar(void *d, u32 n);

/* el salto de los errores sin vuelta (sin memoria, un límite pasado): como
 * setjmp/longjmp */
#if defined(HPDF_PENTIUM)
typedef u32 so_salto[8];
int so_marca(so_salto s) __attribute__((returns_twice));
void so_salta(so_salto s, int v) __attribute__((noreturn));
#else
#include <setjmp.h>
typedef jmp_buf so_salto;
#define so_marca(s) setjmp(s)
#define so_salta(s, v) longjmp(s, v)
#endif

/* la coma flotante (la libm, o lo del x87 en Pentium) */
double so_raiz(double x);
double so_suelo(double x);
double so_techo(double x);
double so_abs(double x);
double so_pot(double x, double y);
double so_resto(double x, double y);
double so_cos(double x);
double so_sen(double x);
double so_acos(double x);
double so_atan2(double y, double x);
double so_ln(double x);

#endif
