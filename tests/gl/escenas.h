/* tests/gl/escenas.h - las escenas de prueba de HispaGL (escenas.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ESCENAS_H
#define ESCENAS_H
typedef struct { const char *nombre; void (*dibuja)(int w, int h); } Escena;
extern const Escena escenas[];
void escenas_ini(void);
void escenas_engranajes(float ang);
/* la de medir: una sala como las de los juegos de 1997 (w32/gl/sala.c) */
#include "sala.h"
#endif
