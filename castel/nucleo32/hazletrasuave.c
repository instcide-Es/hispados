/* hazletrasuave.c - ayuda de hazletrasuave.py: rasteriza las letras de una
   letra TrueType a un tamaño, con FreeType (suavizadas, 256 tonos), y las
   saca en texto: «M ascendente descendente», «G código avance izq arriba
   ancho alto puntos-en-hex» y «K código1 código2 ajuste» (el kerning, en
   puntos enteros). Los códigos (Unicode) van en la línea de órdenes.
   uso: hazletrasuave LETRA.ttf PUNTOS MODO U1 U2 ...   (MODO: 0 ligero,
   1 el de la letra, 2 sin ajustar a la rejilla)
   Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    FT_Library lib;
    FT_Face f;
    int i, j, x, y, modo, n;
    FT_Int32 carga;
    if (argc < 5) return 2;
    if (FT_Init_FreeType(&lib) || FT_New_Face(lib, argv[1], 0, &f)) return 1;
    FT_Set_Pixel_Sizes(f, 0, atoi(argv[2]));
    modo = atoi(argv[3]);
    carga = modo == 0 ? FT_LOAD_TARGET_LIGHT : modo == 1 ? FT_LOAD_TARGET_NORMAL : FT_LOAD_NO_HINTING;
    printf("M %ld %ld\n", f->size->metrics.ascender >> 6, -(f->size->metrics.descender >> 6));
    n = argc - 4;
    for (i = 0; i < n; i++) {
        unsigned long u = strtoul(argv[4 + i], 0, 0);
        FT_UInt g = FT_Get_Char_Index(f, u);
        FT_GlyphSlot s;
        if (FT_Load_Glyph(f, g, carga | FT_LOAD_RENDER)) return 3;
        s = f->glyph;
        printf("G %lu %ld %d %d %u %u ", u, (s->advance.x + 32) >> 6, s->bitmap_left, s->bitmap_top,
               s->bitmap.width, s->bitmap.rows);
        for (y = 0; y < (int)s->bitmap.rows; y++)
            for (x = 0; x < (int)s->bitmap.width; x++)
                printf("%02x", s->bitmap.buffer[y * s->bitmap.pitch + x]);
        printf("\n");
    }
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++) {
            unsigned long a = strtoul(argv[4 + i], 0, 0), b = strtoul(argv[4 + j], 0, 0);
            FT_Vector k;
            FT_Get_Kerning(f, FT_Get_Char_Index(f, a), FT_Get_Char_Index(f, b), FT_KERNING_UNFITTED, &k);
            if (k.x != 0) printf("K %lu %lu %ld\n", a, b, k.x);
        }
    FT_Done_Face(f);
    FT_Done_FreeType(lib);
    return 0;
}
