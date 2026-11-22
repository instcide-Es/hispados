/* imagen.h - el Reproductor: la carátula, a la medida (imagen.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_IMAGEN_H
#define REPRO_IMAGEN_H
#include "so.h"
/* la imagen (JPEG, PNG, GIF, BMP) a un cuadrado de «lado» x «lado», 0xAARRGGBB
 * (de so_pide); NULL si no se puede */
u32 *imagen_a_medida(const u8 *d, u32 n, int lado);
#endif
