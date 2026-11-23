/* imagen.c - la oficina de 32 bits: las imágenes (oficina/comun/imagen.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <string.h>
#include <stdlib.h>
#include "imagen.h"

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(n) so_pide(n)
#define STBI_REALLOC(p, n) so_repide(p, n)
#define STBI_FREE(p) so_suelta(p)
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int imagen_tipo(const u8 *d, u32 n)
{
	if (n >= 8 && !memcmp(d, "\x89PNG\r\n\x1a\n", 8))
		return IMAGEN_PNG;
	if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)
		return IMAGEN_JPG;
	if (n >= 2 && d[0] == 'B' && d[1] == 'M')
		return IMAGEN_BMP;
	if (n >= 6 && (!memcmp(d, "GIF87a", 6) || !memcmp(d, "GIF89a", 6)))
		return IMAGEN_GIF;
	return IMAGEN_NADA;
}

int imagen_mide(const u8 *d, u32 n, int *an, int *al)
{
	int c;

	if (!imagen_tipo(d, n) || n > 0x7FFFFFFF)
		return 0;
	if (!stbi_info_from_memory(d, (int)n, an, al, &c))
		return 0;
	return *an > 0 && *al > 0;
}

/* (lo de stb_image a 0xAARRGGBB) */
static u32 *de_stb(const u8 *d, u32 n, int *an, int *al)
{
	int a, b, c, i;
	u8 *px;
	u32 *r;

	px = stbi_load_from_memory(d, (int)n, &a, &b, &c, 4);
	if (!px)
		return NULL;
	r = so_pide((size_t)a * b * 4);
	if (r)
		for (i = 0; i < a * b; i++)
			r[i] = (u32)px[4 * i + 3] << 24 | (u32)px[4 * i] << 16 | (u32)px[4 * i + 1] << 8 | px[4 * i + 2];
	stbi_image_free(px);
	*an = a;
	*al = b;
	return r;
}

u32 *imagen_lee(const u8 *d, u32 n, int *an, int *al)
{
	int a, b;

	if (!imagen_mide(d, n, &a, &b) || (u64)a * b > IMAGEN_MAX_PUNTOS)
		return NULL;
	return de_stb(d, n, an, al);
}

int imagen_mide_tga(const u8 *d, u32 n, int *an, int *al)
{
	int c;

	/* (TGA no tiene firma: solo si lo dice la extensión; stb mira la cabecera) */
	if (n < 18 || n > 0x7FFFFFFF)
		return 0;
	if (!stbi_info_from_memory(d, (int)n, an, al, &c))
		return 0;
	return *an > 0 && *al > 0;
}

u32 *imagen_lee_tga(const u8 *d, u32 n, int *an, int *al, u32 max_puntos)
{
	int a, b;

	if (imagen_tipo(d, n) || !imagen_mide_tga(d, n, &a, &b) || (u64)a * b > max_puntos)
		return NULL;
	return de_stb(d, n, an, al);
}

u32 *imagen_lee_max(const u8 *d, u32 n, int *an, int *al, u32 max_puntos)
{
	int a, b;

	if (!imagen_mide(d, n, &a, &b) || (u64)a * b > max_puntos)
		return NULL;
	return de_stb(d, n, an, al);
}

int imagen_jpeg_componentes(const u8 *d, u32 n)
{
	u32 i = 2;

	if (imagen_tipo(d, n) != IMAGEN_JPG)
		return 0;
	while (i + 4 <= n) {
		u32 m, l;

		if (d[i] != 0xFF) {
			i++;
			continue;
		}
		m = d[i + 1];
		if (m == 0xFF) {
			i++;
			continue;
		}
		if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7)) {
			i += 2;
			continue;
		}
		l = (u32)d[i + 2] << 8 | d[i + 3];
		if ((m >= 0xC0 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC)
			return i + 9 < n ? d[i + 9] : 0;
		i += 2 + l;
	}
	return 0;
}
