/* =============================================================================
 *  woff.h - las letras de la web (@font-face) para NAVEGA2.EXE: WOFF 1.0 y
 *  WOFF2 pasados a sfnt (TTF/OTF), que es lo que entiende stb_truetype.
 *  docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#ifndef SURF_WOFF_H
#define SURF_WOFF_H
#include <stddef.h>

/* las letras de la web a sfnt (TTF/OTF) que entiende stb_truetype: acepta un
   sfnt tal cual (devuelve una copia), WOFF 1.0 y WOFF2 (sin colecciones).
   Lo devuelto es de malloc (se suelta con free) y su tamaño va en *sale.
   NULL si no se puede (archivo roto, truncado, demasiado grande o sin memoria). */
unsigned char *woff_a_sfnt(const unsigned char *d, size_t n, size_t *sale);

/* la carpeta donde está BROTLI.DIC (el diccionario de Brotli, que hace falta
   para los WOFF2 y no va dentro del EXE): ruta de DOS, p. ej.
   "C:\\HISPADOS\\INTERNET\\RECURSOS". Se copia; NULL o "" = la carpeta actual.
   El diccionario se lee solo mientras se descomprime un WOFF2 y se suelta luego. */
void woff_carpeta(const char *dir);

#endif
