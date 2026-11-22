/* imagen.h - la oficina de 32 bits: las imágenes (PNG, JPG, BMP, GIF) con
 * stb_image (terceros/stb, dominio público o MIT), con sus topes: llegan de
 * documentos de fuera.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_IMAGEN_H
#define OFI_IMAGEN_H
#include "so.h"

enum { IMAGEN_NADA, IMAGEN_PNG, IMAGEN_JPG, IMAGEN_BMP, IMAGEN_GIF };
#define IMAGEN_MAX_PUNTOS (8u * 1024 * 1024)

/* qué es (por sus primeros octetos) */
int imagen_tipo(const u8 *d, u32 n);
/* lo que mide (sin descomprimirla); 0 si no se sabe leer */
int imagen_mide(const u8 *d, u32 n, int *an, int *al);
/* descomprimida: an x al puntos 0xAARRGGBB (de so_pide), o NULL */
u32 *imagen_lee(const u8 *d, u32 n, int *an, int *al);
/* (JPEG) los componentes de color: 1 gris, 3 RGB, 4 CMYK; 0 si no lo es */
int imagen_jpeg_componentes(const u8 *d, u32 n);

#endif
