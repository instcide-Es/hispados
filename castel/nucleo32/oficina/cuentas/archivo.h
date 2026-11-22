/* archivo.h - Cuentas 2: los formatos de los libros. Se conocen por lo de
 * dentro, no por la extensión (el núcleo aún guarda 8+3: «libro.xlsx» queda
 * LIBRO.XLS y se vuelve a abrir como XLSX):
 *   .HOJ   el de Cuentas de Hispa-386 (texto, una línea por cosa: f_hoj.c).
 *          Delante va todo lo que entiende Cuentas de 386 (que salta lo que no
 *          conoce); detrás, en líneas «2x», todo lo de Cuentas 2: no se pierde nada
 *   .XLSX  el de Excel (OOXML: f_xlsx.c)          .ODS  el de LibreOffice (f_ods.c)
 *   .CSV   texto con separador (f_csv.c)          .PDF  exportar (f_pdf.c)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_ARCHIVO_H
#define CU_ARCHIVO_H
#include "libro.h"

enum { FMT_NADA, FMT_HOJ, FMT_XLSX, FMT_ODS, FMT_CSV, FMT_PDF, FMT_XLS };

#define ARCH_MAX (64u * 1024 * 1024)	/* lo más que se lee */

/* abre un libro (calculado); NULL si no se puede (en err, por qué). En *formato, el que era */
libro *arch_abre(const char *ruta, int *formato, char *err, u32 cabe);
/* guarda (o exporta, con FMT_PDF); en «real», el nombre con que queda (8+3) */
int arch_guarda(libro *l, const char *ruta, int formato, char *real, u32 cabe_real, char *err, u32 cabe);
int arch_formato_ext(const char *ruta);		/* por la extensión (0 si no se sabe) */
int arch_formato_de(const u8 *d, u32 n);	/* por lo de dentro */
const char *arch_extension(int f);		/* «HOJ», «XLSX»... */
const char *arch_nombre_formato(int f);		/* «Libro de Excel (.XLSX)» */

/* ---- CSV: el separador, la codificación y la hoja ---- */
enum { CSV_AUTO, CSV_UTF8, CSV_1252, CSV_437 };
typedef struct {
	char sep;		/* ';' ',' '\t' (0: el que más aparezca, al leer; ';' al escribir) */
	int cod;		/* CSV_* (CSV_AUTO: UTF-8 si lo es; si no, 1252) */
	int bom;		/* (al escribir en UTF-8) con la marca delante */
	int formulas;		/* (al escribir) las fórmulas con su «=» en vez de su valor */
	u32 hoja;		/* (al escribir) la hoja (la activa) */
} csv_opc;
extern csv_opc arch_csv;	/* lo que se usa al abrir y guardar .CSV (el cuadro lo cambia) */

/* ---- cada formato: de la memoria a un libro (NULL y err) y de un libro a un búfer (0 si va bien) ---- */
libro *hoj_lee(const u8 *d, u32 n, char *err, u32 cabe);
int hoj_escribe(libro *l, bufer *b, char *err, u32 cabe);
libro *csv_lee(const u8 *d, u32 n, const csv_opc *o, char *err, u32 cabe);
int csv_escribe(libro *l, const csv_opc *o, bufer *b, char *err, u32 cabe);
libro *xlsx_lee(const u8 *d, u32 n, char *err, u32 cabe);
int xlsx_escribe(libro *l, bufer *b, char *err, u32 cabe);
libro *ods_lee(const u8 *d, u32 n, char *err, u32 cabe);
int ods_escribe(libro *l, bufer *b, char *err, u32 cabe);
/* exportar a PDF (las hojas que se digan: -1 todas; la activa de ordinario) */
int pdf_exporta(libro *l, int hoja, const char *titulo, bufer *b, char *err, u32 cabe);

/* (para los formatos) los colores de la paleta de Hispa Ventanas (Cuentas de 386) */
extern const u32 PALETA386[16];

#endif
