/* imagenes.c - Correo: las imágenes de los mensajes (las de dentro, cid:, y
 * las de fuera solo si el usuario pulsa «Mostrar imágenes»), con stb_image
 * (terceros/stb, dominio público o MIT: PNG, JPEG, GIF, BMP), a lo ancho que
 * haya como mucho (la media de los puntos que caen en cada uno). Con topes:
 * 4000 x 4000 puntos, 16 MB de imagen. C portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include "so.h"

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 4000
#define STBI_MALLOC(n) so_pide(n)
#define STBI_REALLOC(p, n) so_repide(p, n)
#define STBI_FREE(p) so_suelta(p)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* los puntos (0x00RRGGBB, sobre blanco si son transparentes), a lo sumo
 * max_an de ancho y max_al de alto; NULL si no se puede */
u32 *img_lee(const u8 *d, u32 n, int max_an, int max_al, int *an, int *al)
{
	int w, h, c, sw, sh, x, y;
	u8 *p;
	u32 *r;

	*an = *al = 0;
	if (n < 8 || n > (32u << 20) || !stbi_info_from_memory(d, (int)n, &w, &h, &c) || w <= 0 || h <= 0 ||
	    (u64)w * (u64)h > (4u << 20))
		return NULL;
	p = stbi_load_from_memory(d, (int)n, &w, &h, &c, 4);
	if (!p)
		return NULL;
	sw = w;
	sh = h;
	if (max_an > 0 && sw > max_an) {
		sh = (int)((i64)sh * max_an / sw);
		sw = max_an;
	}
	if (max_al > 0 && sh > max_al) {
		sw = (int)((i64)sw * max_al / sh);
		sh = max_al;
	}
	if (sw < 1)
		sw = 1;
	if (sh < 1)
		sh = 1;
	r = so_pide((size_t)sw * (size_t)sh * 4);
	if (!r) {
		stbi_image_free(p);
		return NULL;
	}
	for (y = 0; y < sh; y++) {
		int y0 = (int)((i64)y * h / sh), y1 = (int)((i64)(y + 1) * h / sh);

		if (y1 <= y0)
			y1 = y0 + 1;
		for (x = 0; x < sw; x++) {
			int x0 = (int)((i64)x * w / sw), x1 = (int)((i64)(x + 1) * w / sw), i, j;
			u32 sr = 0, sg = 0, sb = 0, k = 0;

			if (x1 <= x0)
				x1 = x0 + 1;
			/* (como mucho 4 x 4 muestras: rápido en el Atom) */
			for (j = y0; j < y1; j += (y1 - y0 + 3) / 4)
				for (i = x0; i < x1; i += (x1 - x0 + 3) / 4) {
					const u8 *q = p + ((size_t)j * (size_t)w + (size_t)i) * 4;
					u32 a = q[3];

					sr += (q[0] * a + 255 * (255 - a)) / 255;
					sg += (q[1] * a + 255 * (255 - a)) / 255;
					sb += (q[2] * a + 255 * (255 - a)) / 255;
					k++;
				}
			r[y * sw + x] = (sr / k) << 16 | (sg / k) << 8 | (sb / k);
		}
	}
	stbi_image_free(p);
	*an = sw;
	*al = sh;
	return r;
}
