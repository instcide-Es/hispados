/* imagen.c - el Reproductor: la carátula. La imagen que viene en las
 * etiquetas (JPEG, PNG, GIF o BMP) con stb_image (terceros/stb, dominio
 * público o MIT), hecha a la medida del recuadro (la media de los puntos que
 * caen en cada uno: sin escalones). Portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "imagen.h"

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 8192
#define STBI_MALLOC(n) so_pide(n)
#define STBI_REALLOC(p, n) so_repide(p, n)
#define STBI_FREE(p) so_suelta(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

u32 *imagen_a_medida(const u8 *d, u32 n, int lado)
{
	int w, h, c, x, y;
	u8 *p;
	u32 *s;

	if (!d || n < 8 || lado <= 0)
		return NULL;
	p = stbi_load_from_memory(d, (int)n, &w, &h, &c, 4);
	if (!p)
		return NULL;
	s = so_pide((size_t)lado * lado * 4);
	if (!s) {
		stbi_image_free(p);
		return NULL;
	}
	/* (cuadrada: la parte de en medio si no lo es) */
	{
		int q = w < h ? w : h, ox = (w - q) / 2, oy = (h - q) / 2;

		for (y = 0; y < lado; y++) {
			int y0 = oy + y * q / lado, y1 = oy + (y + 1) * q / lado;

			if (y1 <= y0)
				y1 = y0 + 1;
			for (x = 0; x < lado; x++) {
				int x0 = ox + x * q / lado, x1 = ox + (x + 1) * q / lado, i, j;
				u32 r = 0, g = 0, b = 0, a = 0, k = 0;

				if (x1 <= x0)
					x1 = x0 + 1;
				/* (como mucho 8 x 8 muestras) */
				for (j = y0; j < y1; j += (y1 - y0 + 7) / 8)
					for (i = x0; i < x1; i += (x1 - x0 + 7) / 8) {
						const u8 *q4 = p + ((size_t)(j < h ? j : h - 1) * w + (i < w ? i : w - 1)) * 4;

						r += q4[0];
						g += q4[1];
						b += q4[2];
						a += q4[3];
						k++;
					}
				s[y * lado + x] = (a / k) << 24 | (r / k) << 16 | (g / k) << 8 | (b / k);
			}
		}
	}
	stbi_image_free(p);
	return s;
}
