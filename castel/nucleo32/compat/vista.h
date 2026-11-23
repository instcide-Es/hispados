/* vista.h - (COMPAT) el resultado de una comprobación, en renglones para
 * leer: lo que falta agrupado por DLL, con para qué sirve cada una, y
 * aparte lo que no hace falta. Lo usan la ventana y la Consola.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_VISTA_H
#define COMPAT_VISTA_H
#include "compat.h"
enum { R_TITULO, R_BIEN, R_MAL, R_APARTADO, R_DLL, R_FUNC, R_NOTA, R_GRIS };
typedef struct { int clase, sangria; char t[240]; } renglon;
typedef struct { renglon *r; int n, cabe; } vista;
int vista_haz(vista *v, const c_informe *i, const char *nombre);
void vista_suelta(vista *v);
/* en texto, para la Consola (de so_pide) */
char *vista_texto(const vista *v);
#endif
