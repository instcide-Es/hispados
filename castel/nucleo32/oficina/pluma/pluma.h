/* pluma.h - Pluma 2: lo que se dicen sus partes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_H
#define PLUMA_H
#include "so.h"

#define PLUMA_VERSION "2.0"

const char *pluma_arg(const char *s, char *d, u32 cabe);
/* la ventana (ventana.c), con el documento que se abre (o NULL) */
int pluma_ventana(const char *archivo);
/* lo que tarda escribir en un documento de tantas páginas (ventana.c) */
int pluma_mide(int paginas, int letras);

#endif
