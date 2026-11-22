/* texto.h - la oficina de 32 bits: los textos (UTF-8, UTF-32 y la página
 * 437), un printf pequeño y el búfer que crece (lo usan los que escriben
 * archivos: XML, ZIP, PDF, RTF...). C99 portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_TEXTO_H
#define OFI_TEXTO_H
#include <stdarg.h>
#include "so.h"

/* ---- UTF-8 ---- */
/* la letra siguiente de s (n octetos); da cuántos lleva (1 si está mal: U+FFFD) */
int utf8_lee(const u8 *s, u32 n, u32 *c);
/* la letra c en UTF-8 (hasta 4 octetos); da cuántos */
int utf8_pon(u32 c, char *d);
/* cuántas letras hay */
u32 utf8_cuenta(const char *s, u32 n);

/* ---- la página 437 (la de Hispa-Pentium) ---- */
u32 de437(u8 c);
u8 a437(u32 c);		/* lo que no está: '?' (las tildes que faltan, sin tilde) */
/* de UTF-8 a 437 y al revés (d acaba en 0; da el largo) */
u32 utf8_a_437(const char *s, char *d, u32 cabe);
u32 de437_a_utf8(const u8 *s, u32 n, char *d, u32 cabe);

/* ---- letras ---- */
u32 minuscula(u32 c);
u32 mayuscula(u32 c);
u32 sin_tilde(u32 c);		/* á -> a, Ñ -> N... (para buscar) */
int es_letra(u32 c);		/* letras (también con tilde) y cifras */
int es_espacio(u32 c);

/* ---- cadenas de C ---- */
u32 largo(const char *s);
void copia(char *d, const char *s, u32 cabe);	/* como strlcpy */
void anade(char *d, const char *s, u32 cabe);	/* como strlcat */
int iguales(const char *a, const char *b);
int iguales_sin_caja(const char *a, const char *b);	/* (ASCII) */
int empieza(const char *s, const char *p);
const char *busca(const char *s, const char *p);
char *duplica(const char *s);
char *duplica_n(const char *s, u32 n);
/* la extensión del nombre (sin el punto, en mayúsculas, hasta 7 letras) */
void extension(const char *ruta, char *ext);
/* el nombre sin la carpeta */
const char *sin_carpeta(const char *ruta);
long numero(const char *s, const char **fin);	/* entero con signo */
double numero_real(const char *s, const char **fin);

/* ---- printf pequeño: %d %u %x %X %s %c %% (con ancho, '-' y '0'), %f
 * (con .n) y %S (un texto UTF-32 acabado en 0) ---- */
int fmt(char *d, u32 cabe, const char *f, ...);
int vfmt(char *d, u32 cabe, const char *f, va_list a);
void dice(const char *f, ...);		/* so_dice con formato (y salto de línea) */

/* ---- el búfer que crece ---- */
typedef struct {
	u8 *d;
	u32 n, cabe;
	int mal;		/* se acabó la memoria: lo de después no se guarda */
} bufer;
void buf_pon(bufer *b, const void *d, u32 n);
void buf_txt(bufer *b, const char *s);
void buf_c(bufer *b, u8 c);
void buf_fmt(bufer *b, const char *f, ...);
void buf_xml(bufer *b, const char *s, u32 n);	/* con &amp; &lt; &gt; &quot; */
void buf_u32(bufer *b, u32 c);		/* una letra, en UTF-8 */
void buf_le16(bufer *b, u32 v);
void buf_le32(bufer *b, u32 v);
void buf_suelta(bufer *b);

/* ---- textos UTF-32 (el documento) ---- */
u32 u32_largo(const u32 *s);
/* de UTF-8 a UTF-32 (de so_pide; acaba en 0); *n, cuántas */
u32 *u32_de_utf8(const char *s, u32 bytes, u32 *n);
/* de UTF-32 a UTF-8 (de so_pide, acabado en 0) */
char *utf8_de_u32(const u32 *s, u32 n);

#endif
