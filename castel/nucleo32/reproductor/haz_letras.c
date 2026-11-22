/* haz_letras.c - el Reproductor: hace (en el anfitrión, al compilar) las
 * letras de la ventana: DejaVu Sans y DejaVu Sans Bold (terceros/dejavu) a
 * cuatro tamaños, con stb_truetype (terceros/stb), cada letra con sus 256
 * tonos. Saca un .c con las letras ya hechas puntos: así el programa no
 * rasteriza nada en marcha (en el Atom, ni un milisegundo).
 *   haz_letras SANS.TTF SANSN.TTF > letras.c
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>
#include <stdlib.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

static unsigned char *lee(const char *r)
{
	FILE *f = fopen(r, "rb");
	long n;
	unsigned char *d;

	if (!f) {
		fprintf(stderr, "haz_letras: no está %s\n", r);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	d = malloc((size_t)n);
	if (fread(d, 1, (size_t)n, f) != (size_t)n)
		exit(1);
	fclose(f);
	return d;
}

static int letras[600], n_letras;

static void pon_letras(void)
{
	int c;
	static const int mas[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x2122, 0x266A,
		0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x25B6, 0 };

	for (c = 32; c < 127; c++)
		letras[n_letras++] = c;
	for (c = 160; c < 0x180; c++)
		letras[n_letras++] = c;
	for (c = 0; mas[c]; c++)
		letras[n_letras++] = mas[c];
	/* (en orden: el programa las busca partiendo por la mitad) */
	for (c = 1; c < n_letras; c++) {
		int t = letras[c], j;

		for (j = c; j > 0 && letras[j - 1] > t; j--)
			letras[j] = letras[j - 1];
		letras[j] = t;
	}
}

/* una letra a un tamaño: sus puntos, y donde va */
static void hace(const char *nombre, unsigned char *ttf, int alto)
{
	stbtt_fontinfo f;
	float esc;
	int asc, desc, gap, i, total = 0, k;

	stbtt_InitFont(&f, ttf, stbtt_GetFontOffsetForIndex(ttf, 0));
	esc = stbtt_ScaleForPixelHeight(&f, (float)alto);
	stbtt_GetFontVMetrics(&f, &asc, &desc, &gap);
	printf("static const u8 p_%s[] = {\n", nombre);
	{
		int off = 0;
		static int offs[600], an[600], al[600], dx[600], dy[600], av[600], hay[600];

		for (i = 0; i < n_letras; i++) {
			int g = stbtt_FindGlyphIndex(&f, letras[i]), w, h, x0, y0, adv, lsb;
			unsigned char *b;

			hay[i] = g != 0 || letras[i] == 32;
			if (!hay[i])
				continue;
			stbtt_GetGlyphHMetrics(&f, g, &adv, &lsb);
			b = stbtt_GetGlyphBitmapSubpixel(&f, esc, esc, 0.0f, 0.0f, g, &w, &h, &x0, &y0);
			offs[i] = off;
			an[i] = w;
			al[i] = h;
			dx[i] = x0;
			dy[i] = y0 + (int)(asc * esc + 0.5f);
			av[i] = (int)(adv * esc + 0.5f);
			for (k = 0; k < w * h; k++) {
				printf("%d,", b[k]);
				if (++total % 32 == 0)
					printf("\n");
			}
			off += w * h;
			stbtt_FreeBitmap(b, NULL);
		}
		printf("0};\nstatic const letra l_%s[] = {\n", nombre);
		k = 0;
		for (i = 0; i < n_letras; i++)
			if (hay[i]) {
				printf("{%d,%d,%d,%d,%d,%d,%d},\n", letras[i], an[i], al[i], dx[i], dy[i], av[i], offs[i]);
				k++;
			}
		printf("};\n");
		printf("const fuente f_%s = { %d, %d, %d, l_%s, p_%s };\n\n", nombre, alto, (int)((asc - desc + gap) * esc + 0.5f), k,
		       nombre, nombre);
	}
}

int main(int argc, char **argv)
{
	unsigned char *n, *b;

	if (argc < 3) {
		fprintf(stderr, "uso: haz_letras SANS.TTF NEGRITA.TTF > letras.c\n");
		return 2;
	}
	n = lee(argv[1]);
	b = lee(argv[2]);
	pon_letras();
	printf("/* letras.c - hecho por haz_letras.c (no se edita): DejaVu Sans, de\n"
	       " * terceros/dejavu (licencia de Bitstream Vera y DejaVu, libre). */\n");
	printf("#include \"dibujo.h\"\n\n");
	hace("normal12", n, 13);
	hace("normal14", n, 15);
	hace("negrita14", b, 15);
	hace("negrita20", b, 21);
	hace("negrita12", b, 13);
	return 0;
}
