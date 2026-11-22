/* pdfsal.h - la oficina de 32 bits: escribir un PDF (lo usan Pluma 2 para
 * «Exportar a PDF» e «Imprimir», y lo usará Cuentas 2). Es el lado de
 * escribir de Hispa-PDF (castel/nucleo32/pdf, que lee, une, separa...):
 * páginas de cualquier tamaño, texto con la x exacta de cada letra, las
 * letras TrueType de oficina/comun/letras.h incrustadas (solo los glifos que
 * se usan: ttf_subconjunto), con su ToUnicode (el texto se copia y se
 * busca), negrita y cursiva hechas a mano como en la pantalla, rectángulos,
 * rayas, imágenes (JPEG tal cual; PNG, BMP y GIF con Flate y su
 * transparencia), marcadores y los datos del documento. Los flujos van
 * comprimidos con Flate (miniz: tdefl_compress_mem_to_heap y mz_free).
 *
 * Las medidas, en puntos (1/72 de pulgada) y desde la esquina de ARRIBA a
 * la izquierda de la página (el PDF las cuenta desde abajo: se da la vuelta
 * aquí). Los colores, 0xRRGGBB.
 *
 *   pdfsal *s = pdf_nuevo();
 *   pdf_datos(s, "Carta", "Carlos", NULL);
 *   pdf_pagina(s, 595.3, 841.9);                    (A4)
 *   pdf_texto(s, cara, 0, 12, 0x000000, letras, n, xs, 100);
 *   pdf_rect(s, 70, 120, 200, 1, 0x808080);
 *   pdf_marcador(s, "Capítulo 1", 1, 0, 90);
 *   bufer b = { 0 }; pdf_acaba(s, &b); ... buf_suelta(&b);
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_PDFSAL_H
#define OFI_PDFSAL_H
#include "so.h"
#include "texto.h"

typedef struct pdfsal pdfsal;

pdfsal *pdf_nuevo(void);
/* el título, el autor y el asunto (UTF-8; NULL o "" si no hay) */
void pdf_datos(pdfsal *s, const char *titulo, const char *autor, const char *asunto);
/* una página nueva (la de antes queda acabada); da su número (desde 0) */
int pdf_pagina(pdfsal *s, double ancho, double alto);
/* n letras (Unicode) de la cara de letras.h (con su respaldo, letra_glifo),
 * de tam puntos, con la línea de base en y; xs[i], la x de cada una.
 * falsas: FALSA_NEGRITA, FALSA_CURSIVA (letras.h) */
void pdf_texto(pdfsal *s, int cara, int falsas, double tam, u32 color, const u32 *t, u32 n, const double *xs, double y);
void pdf_rect(pdfsal *s, double x, double y, double an, double al, u32 color);
/* una raya de x0,y0 a x1,y1, de grosor g */
void pdf_raya(pdfsal *s, double x0, double y0, double x1, double y1, double g, u32 color);
/* una imagen (el archivo entero: PNG, JPG, BMP, GIF). La misma «datos» dos
 * veces es la misma imagen (va una vez). Da su número, o -1 si no se lee */
int pdf_imagen(pdfsal *s, const u8 *datos, u32 n);
void pdf_pon_imagen(pdfsal *s, int img, double x, double y, double an, double al);
/* un marcador (nivel 1, 2, 3...) que lleva a la página «pag», a la altura y */
void pdf_marcador(pdfsal *s, const char *texto, int nivel, int pag, double y);
/* el PDF entero, a b (0 si va bien; -1 sin memoria) */
int pdf_acaba(pdfsal *s, bufer *b);
void pdf_suelta(pdfsal *s);

#endif
