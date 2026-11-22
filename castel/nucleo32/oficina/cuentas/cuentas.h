/* cuentas.h - Cuentas 2: lo que se dicen sus partes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CUENTAS_H
#define CUENTAS_H
#include "so.h"

#define CUENTAS_VERSION "2.0"

const char *cu_arg(const char *s, char *d, u32 cabe);
/* la ventana (ventana.c), con el libro que se abre (o NULL) */
int cuentas_ventana(const char *archivo);
/* lo que tarda: recalcular una hoja de n fórmulas y desplazarse (ventana.c) */
int cuentas_mide(int formulas);
/* un guion de pruebas (prueba.c): da 0 si todo va bien */
int cuentas_guion(const char *ruta);

#endif
