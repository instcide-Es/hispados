/* util.h - el Reproductor: lo pequeño que usan todos (textos, números de
 * los archivos, UTF-8), portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_UTIL_H
#define REPRO_UTIL_H
#include <stdarg.h>
#include "so.h"

void copia(char *d, int cabe, const char *o);
void anade(char *d, int cabe, const char *o);
/* como snprintf, con %d %u %x %X %s %c %% (y el ancho, con 0 delante, y
 * %ld/%lu para 64 bits: %lld) */
int fmt(char *d, int cabe, const char *f, ...);
int vfmt(char *d, int cabe, const char *f, va_list va);
void reg(const char *f, ...);		/* al registro: «reproductor: ...» */

static inline u32 le16(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8; }
static inline u32 le32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static inline u32 be16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static inline u32 be24(const u8 *p) { return (u32)p[0] << 16 | (u32)p[1] << 8 | p[2]; }
static inline u32 be32(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

int mayus(int c);
int igual_sin_caja(const char *a, const char *b);
int empieza_sin_caja(const char *t, const char *p);
const char *busca_sin_caja(const char *t, const char *p);
/* UTF-8: la letra siguiente (y avanza); el ? si está mal */
u32 utf8_sig(const char **p);
int utf8_pon(u32 u, char *d);		/* los octetos (1 a 4) */
/* de Latin-1 o de UTF-16 (le: 1 si es little endian) a UTF-8 */
void latin1_a_utf8(const u8 *e, int n, char *d, int cabe);
void utf16_a_utf8(const u8 *e, int n, int le, char *d, int cabe);
int es_utf8(const u8 *e, int n);
/* sin tildes y en minúsculas (para buscar y ordenar) */
u32 llana(u32 u);
int compara_llano(const char *a, const char *b);
const char *contiene_llano(const char *t, const char *p);
/* «03:25», o «1:02:03» con horas */
void a_tiempo(u32 segundos, char *d, int cabe);
int a_entero(const char *t);
/* un número con coma: «-6,5» (en décimas) */
int decimas_de(const char *t);
/* la extensión y el nombre sin carpeta */
const char *base_de(const char *ruta);
void carpeta_de(const char *ruta, char *d, int cabe);
u32 azar(void);
void siembra(u32 s);
#endif
