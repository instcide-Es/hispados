/* webletra.h - las letras de la web (@font-face) del Navegador completo
 * (webletra.c); lo usan css.c y html.c de NetSurf ([HISPA]).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef WEBLETRA_H
#define WEBLETRA_H
#include <stddef.h>
#include <stdbool.h>
struct html_content;
/* lee las @font-face y las familias que se usan del texto de una hoja
 * (base: su dirección, para las url() relativas) */
void webletra_hoja(const char *css, size_t n, const char *base);
/* pide las letras que usa la página y no hay; true si la página tiene que
 * esperarlas (se le suma a base.active y, al llegar, sigue sola) */
bool webletra_pide(struct html_content *html);
/* la página se va: que no la avisen */
void webletra_olvida(struct html_content *html);
/* letra.c: una letra de la web ya en sfnt (malloc; letra.c se queda con
 * ella); false si no se puede leer */
bool letra_web(const char *familia, int peso, int cursiva, unsigned char *sfnt, size_t n);
#endif
