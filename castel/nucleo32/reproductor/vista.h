/* vista.h - el Reproductor: la ventana (vista.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_VISTA_H
#define REPRO_VISTA_H
#include "lista.h"
/* la ventana entera; «args», los de la línea de órdenes (lo que hay que tocar) */
int vista_principal(const char *args);
int anade_argumento(lista *l, const char *a);
/* ¿es una opción (/ALEATORIO)? (en Linux, una ruta entera también empieza por «/») */
int es_opcion(const char *a);
const char *siguiente_ruta(const char *p, char *a, int cabe);
#endif
