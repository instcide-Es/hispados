/* cssvar.h - las variables de CSS para NetSurf (w32/surf/cssvar.c)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CSSVAR_H
#define CSSVAR_H
#include <stddef.h>
/* la hoja con los var() cambiados (malloc; *sale octetos) o NULL si no hay nada que cambiar */
char *cssvar_expande(const char *css, size_t n, size_t *sale);
/* lo mismo para un atributo style */
char *cssvar_estilo(const char *t, size_t n, size_t *sale);
/* la medida de la página (para vw y vh en clamp(), min() y max()) */
extern int cssvar_ancho, cssvar_alto;
/* olvidar las variables de :root (al empezar otra página) */
void cssvar_olvida(void);
#endif
