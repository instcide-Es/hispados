/* f_xmlcomun.h - Pluma 2: lo que comparten los formatos de XML (f_odt.c y
 * f_docx.c): los formatos parciales (lo que dice un estilo, con qué
 * campos dice), los nombres de los estilos de serie, las medidas, los
 * colores, repasar las tablas leídas y escribir texto en XML.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_F_XMLCOMUN_H
#define PLUMA_F_XMLCOMUN_H
#include "doc.h"
#include "texto.h"

enum { FL_FAM = 1, FL_TAM = 2, FL_NEG = 4, FL_CUR = 8, FL_SUB = 16, FL_TACH = 32, FL_COLOR = 64, FL_RES = 128, FL_VERT = 256, FL_VERS = 512 };
enum { FP_ALIN = 1, FP_SI = 2, FP_SD = 4, FP_SP = 8, FP_ANT = 16, FP_DES = 32, FP_INTER = 64, FP_TABS = 128, FP_SALTO = 256,
       FP_JUNTOS = 512, FP_FONDO = 1024, FP_NIVTIT = 2048 };

typedef struct {
	fmt_letra l;
	u32 ml;
	fmt_parrafo p;
	u32 mp;
} fx_props;

/* lo de s encima de d */
void fx_junta(fx_props *d, const fx_props *s);
void fx_aplica(const fx_props *s, fmt_letra *l, fmt_parrafo *p);
/* el estilo de serie (EST_*) de un nombre de LibreOffice, de Word o de Pluma; -1 */
int fx_estilo_serie(const char *nombre);
/* el nombre de un estilo de serie para LibreOffice (interno y visible) y para Word */
const char *fx_nombre_odt(int est);
const char *fx_nombre_word(int est);
/* una medida de ODF («2.5cm», «12pt», «0.5in», «3mm», «1pc», «10px») en twips */
i32 fx_medida(const char *s);
/* «#RRGGBB» o «RRGGBB»; SIN_COLOR si no lo es (auto, transparent) */
u32 fx_color(const char *s);
/* una medida para escribir («2.500cm») */
void fx_cm(char *t, u32 cabe, i32 tw);
/* deja las tablas leídas como las quiere doc.h (o las quita si no casan) */
void fx_repasa_tablas(documento *d);
/* una letra del documento en XML (lo que XML no deja, fuera) */
void fx_pon_letra(bufer *b, u32 c);
/* el nombre de ODF sin los «_20_» («Heading_20_1» -> «Heading 1») */
void fx_nombre_odf(const char *s, char *d, u32 cabe);
/* añade letras a un párrafo concreto (como arch_letras) */
void fx_letras(documento *d, parrafo *p, const u32 *t, u32 n, u16 f);
/* ¿es «true», «1», «on»? (y lo de Word: sin w:val es cierto) */
int fx_cierto(const char *v, int sin_valor);

#endif
