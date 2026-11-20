/* frontal.h - NAVEGA.BIN, el Navegador de Hispa-Pentium: lo que se dicen el
 * frontal (frontal.c) y las descargas (descarga.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef FRONTAL_H
#define FRONTAL_H
#include <stdint.h>

struct gui_download_table;
extern struct gui_download_table *frontal_tabla_descargas;
/* la línea de estado (UTF-8): mientras baja una descarga, lo que lleva */
void frontal_aviso_estado(const char *t);
/* la carpeta de las descargas (C:\DESCARGA) */
extern char frontal_descargas[260];
/* de la página 437 (el núcleo: el portapapeles, el teclado) a UTF-8 y al revés */
int frontal_437_a_utf8(const char *t, int n, char *d, int cabe);
int frontal_utf8_a_437(const char *t, int n, char *d, int cabe);
#endif
