/* Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * nucleo64/programas/cxx/mtexto.c - Hispa-64, etapa 10 (docs/64/HISPA-64.md,
 * sección 16): MTEXTO, la letra de los navegadores: HarfBuzz (las formas:
 * GSUB y GPOS, las ligaduras y el kerning) sobre FreeType (la letra
 * dibujada con suavizado), con DejaVu Sans entera (/DEJAVU.TTF). Coloca un texto con
 * acentos, eñes, una ligadura («fi») y pares con kerning, lo dibuja en un
 * lienzo de 8 bits y dice cuántos glifos, sus avances y una suma del
 * lienzo (FNV-1a): en Linux y en Hispa-64 tiene que salir lo mismo.
 * Uso: MTEXTO [FUENTE]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

#define ANCHO 640
#define ALTO 64
static unsigned char lienzo[ALTO][ANCHO];
static int bien_, mal_;
#define MIRA(c, d) do { if (c) bien_++; else { mal_++; printf("  MAL: %s (línea %d)\n", (d), __LINE__); fflush(stdout); } } while (0)

/* coloca t con HarfBuzz: cuántos glifos (y el avance total, en 1/64 de
 * punto); los glifos quedan en el búfer «b» (lo suelta el siguiente) */
static hb_buffer_t *b;
static unsigned coloca(hb_font_t *f, const char *t, const char *rasgos, long *avance, hb_glyph_info_t **inf, hb_glyph_position_t **pos)
{
	if (b) hb_buffer_destroy(b);
	b = hb_buffer_create();
	hb_feature_t r[4];
	unsigned n, i, nr = 0;
	hb_buffer_add_utf8(b, t, -1, 0, -1);
	hb_buffer_guess_segment_properties(b);
	if (rasgos && hb_feature_from_string(rasgos, -1, &r[0])) nr = 1;
	hb_shape(f, b, nr ? r : 0, nr);
	n = hb_buffer_get_length(b);
	*inf = hb_buffer_get_glyph_infos(b, 0);
	*pos = hb_buffer_get_glyph_positions(b, 0);
	*avance = 0;
	for (i = 0; i < n; i++) *avance += (*pos)[i].x_advance;
	return n;
}

int main(int argc, char **argv)
{
	const char *ruta = argc > 1 ? argv[1] : "/DEJAVU.TTF";
	const char *texto = "Hispa-64: ¡Ñandú! fino AV To áéíóú";
	FT_Library lib;
	FT_Face cara;
	hb_font_t *f;
	hb_glyph_info_t *inf;
	hb_glyph_position_t *pos;
	long av, av2, av3;
	unsigned n, n2, n3, i, k;
	int x, y, px, py;
	uint32_t suma = 2166136261u;
	long tinta = 0;

	printf("MTEXTO: HarfBuzz %s sobre FreeType, con %s\n", hb_version_string(), ruta);
	fflush(stdout);
	MIRA(FT_Init_FreeType(&lib) == 0, "FT_Init_FreeType");
	if (FT_New_Face(lib, ruta, 0, &cara)) {
		printf("  MAL: no se abre %s\nMTEXTO: 1 mal\n", ruta);
		return 1;
	}
	{
		FT_Int ma, me, pa;
		FT_Library_Version(lib, &ma, &me, &pa);
		printf("  FreeType %d.%d.%d: «%s %s», %ld glifos, %d unidades por em\n", ma, me, pa, cara->family_name, cara->style_name, cara->num_glyphs, cara->units_per_EM);
	}
	MIRA(cara->num_glyphs > 1000 && !strcmp(cara->family_name, "DejaVu Sans"), "la fuente: DejaVu Sans");
	MIRA(FT_Set_Pixel_Sizes(cara, 0, 32) == 0, "FT_Set_Pixel_Sizes(32)");
	f = hb_ft_font_create_referenced(cara);
	/* las formas: con ligaduras y kerning, y sin ellas */
	n = coloca(f, texto, 0, &av, &inf, &pos);
	n2 = coloca(f, "fino", "-liga", &av2, &inf, &pos);
	n3 = coloca(f, "fino", 0, &av3, &inf, &pos);
	MIRA(n3 == n2 - 1, "la ligadura «fi» (GSUB): un glifo menos");
	{
		long a1, a2;
		coloca(f, "AV", "-kern", &a1, &inf, &pos);
		coloca(f, "AV", 0, &a2, &inf, &pos);
		MIRA(a2 < a1, "el kerning de «AV» (GPOS): más juntas");
		printf("  formas: %u glifos para %zu letras; «fino», %u glifos (sin la ligadura, %u); «AV», %ld y %ld (1/64 de punto, con y sin kerning)\n", n, strlen(texto), n3, n2, a2, a1);
	}
	/* dibujarlo: cada glifo con FreeType, donde dice HarfBuzz */
	n = coloca(f, texto, 0, &av, &inf, &pos);
	px = 4 * 64;
	py = 44 * 64;
	for (i = 0; i < n; i++) {
		FT_GlyphSlot g;
		if (FT_Load_Glyph(cara, inf[i].codepoint, FT_LOAD_DEFAULT) || FT_Render_Glyph(cara->glyph, FT_RENDER_MODE_NORMAL)) { mal_++; continue; }
		g = cara->glyph;
		for (y = 0; y < (int)g->bitmap.rows; y++)
			for (x = 0; x < (int)g->bitmap.width; x++) {
				int lx = (px + pos[i].x_offset) / 64 + g->bitmap_left + x;
				int ly = (py - pos[i].y_offset) / 64 - g->bitmap_top + y;
				unsigned v = g->bitmap.buffer[y * g->bitmap.pitch + x];
				if (lx >= 0 && lx < ANCHO && ly >= 0 && ly < ALTO && v > lienzo[ly][lx]) lienzo[ly][lx] = v;
			}
		px += pos[i].x_advance;
		py -= pos[i].y_advance;
	}
	for (y = 0; y < ALTO; y++)
		for (x = 0; x < ANCHO; x++) {
			suma = (suma ^ lienzo[y][x]) * 16777619u;
			tinta += lienzo[y][x];
		}
	k = 0;
	for (y = 0; y < ALTO; y++)
		for (x = 0; x < ANCHO; x++)
			if (lienzo[y][x] > 0 && lienzo[y][x] < 255) k++;
	MIRA(tinta > 100000 && k > 500, "dibujado, con suavizado (grises)");
	printf("  dibujado: %u glifos en %dx%d, %ld de avance; tinta %ld, %u puntos grises; suma FNV-1a 0x%08X\n", n, ANCHO, ALTO, av / 64, tinta, k, suma);
	hb_buffer_destroy(b);
	hb_font_destroy(f);
	FT_Done_FreeType(lib);
	if (mal_) printf("MTEXTO: %d mal de %d\n", mal_, bien_ + mal_);
	else printf("MTEXTO: todo bien (%d pruebas)\n", bien_);
	return mal_ ? 1 : 0;
}
