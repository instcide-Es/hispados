/* interno.h - Correo: lo que comparten por dentro juegos.c, mime.c,
 * construye.c, html.c y vcard.c (las letras sin ctype.h, los textos que
 * crecen, las copias). No es interfaz de nadie más. Lo de Texto está en
 * juegos.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_INTERNO_H
#define CORREO_INTERNO_H
#include <string.h>
#include "so.h"

static inline int es_espacio(int c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}
static inline int es_blanco(int c)
{
	return c == ' ' || c == '\t';
}
static inline int es_cifra(int c)
{
	return c >= '0' && c <= '9';
}
static inline int es_letra(int c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static inline int es_alnum(int c)
{
	return es_letra(c) || es_cifra(c);
}
static inline int minuscula(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}
static inline int mayuscula(int c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}
static inline int valor_hex(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* un texto que crece (al doble); «tope»: lo más que se deja (0: 64 MB). Si
 * no hay memoria o se pasa, «malo» = 1 y lo de después no se pone */
typedef struct {
	char *d;
	u32 n, cabe, tope;
	int malo;	/* 1: sin memoria; 2: pasado del tope */
} Texto;
void tx_ini(Texto *t, u32 tope);
int tx_pon(Texto *t, const void *d, u32 n);
int tx_c(Texto *t, int c);
int tx_s(Texto *t, const char *s);
int tx_letra(Texto *t, u32 letra);
int tx_fmt(Texto *t, const char *f, ...);
/* con su 0 detrás; NULL si no había memoria (y lo suelta) */
char *tx_fin(Texto *t, u32 *largo);
void tx_suelta(Texto *t);

/* iguales sin mayúsculas (ASCII) */
int cr_igual(const char *a, const char *b);
/* a[0..na) igual a b (que acaba en 0), sin mayúsculas */
int cr_igual_n(const char *a, u32 na, const char *b);
/* ¿empieza a[0..na) por b? (sin mayúsculas) */
int cr_empieza(const char *a, u32 na, const char *b);
/* una copia (con un 0 detrás) en memoria nueva */
char *cr_copia(const char *s, u32 n);
/* copiar a un arreglo fijo sin partir una letra de UTF-8; acaba en 0 */
void cr_copia_en(char *d, int cabe, const char *s, u32 n);
/* buscar «b» (de nb) en a[0..na): la posición o -1 (sin pasarse) */
i32 cr_busca(const u8 *a, u32 na, const char *b, u32 nb);
/* lo mismo sin mayúsculas */
i32 cr_busca_sin(const u8 *a, u32 na, const char *b, u32 nb);
/* un número decimal (sin signo, saturado); avanza *i */
u32 cr_numero(const char *s, u32 n, u32 *i, int *hay);

#endif
