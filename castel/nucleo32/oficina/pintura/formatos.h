/* formatos.h - Pintura 2: leer y escribir las imágenes (PNG, JPG, BMP, GIF, PCX, ICO/CUR y TGA).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PINTURA_FORMATOS_H
#define PINTURA_FORMATOS_H
#include "so.h"
#include "texto.h"

enum { FI_NADA, FI_PNG, FI_JPG, FI_BMP, FI_GIF, FI_PCX, FI_ICO, FI_TGA };
#define FI_MAX_LADO 4096
#define FI_MAX_PUNTOS (4096u * 4096u)

/* por la extensión (PNG, JPG/JPEG/JPE, BMP, GIF, PCX, ICO/CUR, TGA) */
int fi_por_extension(const char *ruta);
/* por los primeros octetos (TGA no se puede: FI_NADA) */
int fi_por_contenido(const u8 *d, u32 n);
/* «PNG», «JPG»... */
const char *fi_nombre(int f);
/* «png», «jpg»... (para añadir al nombre) */
const char *fi_extension(int f);
/* PNG, JPG, BMP, GIF y PCX */
int fi_se_escribe(int f);

/* leer: an x al puntos 0xAARRGGBB (de so_pide), o NULL con el porqué en err
 * (en español: «no es una imagen que Pintura sepa leer», «mide más de 4096 x
 * 4096»...). «pista»: lo que dice la extensión (para TGA, que no tiene
 * firma); se mira primero el contenido. Los GIF, el primer cuadro; los ICO y
 * CUR, la imagen más grande (y de más colores) que llevan. */
u32 *fi_lee(const u8 *d, u32 n, int pista, int *an, int *al, char *err, u32 cabe);

/* escribir la imagen (0xAARRGGBB) en b con el formato f. calidad: la de JPG
 * (1-100). 0 si va bien; -1 con el porqué en err.
 *  PNG: sin pérdida (con alfa solo si hay algo transparente);
 *  JPG: lo transparente, sobre blanco;
 *  BMP: 24 bits, o 32 con su alfa (BITMAPV5HEADER) si hay transparencia;
 *  GIF: GIF89a; con 256 colores o menos (contando el transparente, alfa <
 *       128), la paleta exacta; con más, 255 por corte de la mediana con
 *       difuminado de Floyd-Steinberg (y el transparente);
 *  PCX: versión 5, RLE; sobre blanco; con 256 colores o menos, un plano de
 *       8 bits con la paleta del final; si no, tres planos de 8 bits. */
int fi_escribe(bufer *b, const u32 *px, int an, int al, int f, int calidad, char *err, u32 cabe);

/* ¿hay algún punto que no sea opaco del todo? */
int fi_hay_transparencia(const u32 *px, int an, int al);

#endif
