/* archivo.h - Pluma 2: abrir y guardar. El formato se conoce por lo que
 * lleva dentro (no por la extensión: el núcleo de Hispa-Pentium aún crea
 * solo nombres 8+3, y un .DOCX nuevo puede acabar en .DOC):
 *   .PLU   el de Pluma de Hispa-386 (lo que entiende aquel, delante; y
 *          detrás, todo lo de Pluma 2, que aquel no mira: no se pierde nada)
 *   .ODT   OpenDocument (LibreOffice)        .DOCX  Office Open XML (Word)
 *   .RTF   Rich Text Format                  .TXT   texto (437 o UTF-8)
 *   .PDF   (solo exportar) con las letras TrueType dentro
 * Cada lector trabaja con el archivo ya en la memoria y no se fía de él
 * (topes y comprobaciones: castel/pruebas.sh, parte 41, los estropea).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_ARCHIVO_H
#define PLUMA_ARCHIVO_H
#include "doc.h"
#include "texto.h"

enum { FMT_NADA, FMT_PLU, FMT_ODT, FMT_DOCX, FMT_RTF, FMT_TXT, FMT_PDF };
#define ARCH_MAX (64u * 1024 * 1024)	/* lo más que se lee de un archivo */

/* el formato por la extensión (FMT_NADA si no se sabe) y por lo de dentro */
int arch_formato_ext(const char *ruta);
int arch_formato_datos(const u8 *d, u32 n);
const char *arch_nombre_formato(int f);	/* «Documento de Word (.DOCX)» */
const char *arch_extension(int f);	/* «DOCX» */

/* abre el documento (NULL si no se puede; en «error», por qué) */
documento *arch_abre(const char *ruta, int *formato, char *error, u32 cabe);
/* lo guarda en la ruta (escribe a un temporal y luego cambia el nombre:
 * si algo falla, el de antes se queda). 0 si va bien. En «ruta_real», el
 * nombre con que ha quedado (si el sistema no deja nombres largos, 8+3) */
int arch_guarda(documento *d, const char *ruta, int formato, char *ruta_real, u32 cabe_ruta, char *error, u32 cabe);

/* los de cada formato, en la memoria */
documento *plu_lee(const u8 *d, u32 n, char *error, u32 cabe);
int plu_escribe(documento *d, bufer *b);
documento *txt_lee(const u8 *d, u32 n, char *error, u32 cabe);
int txt_escribe(documento *d, bufer *b, int utf8);
documento *odt_lee(const u8 *d, u32 n, char *error, u32 cabe);
int odt_escribe(documento *d, bufer *b);
documento *docx_lee(const u8 *d, u32 n, char *error, u32 cabe);
int docx_escribe(documento *d, bufer *b);
documento *rtf_lee(const u8 *d, u32 n, char *error, u32 cabe);
int rtf_escribe(documento *d, bufer *b);
/* el PDF (pluma/f_pdf.c, con oficina/comun/pdfsal.c) */
int pdf_exporta(documento *d, bufer *b);

/* (para los lectores) el párrafo nuevo del final con su formato, y añadir
 * letras al último */
parrafo *arch_parrafo(documento *d);
void arch_letras(documento *d, const u32 *t, u32 n, u16 f);

#endif
