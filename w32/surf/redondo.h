/* redondo.h - esquinas redondas y sombras del Navegador completo (redondo.c);
 * lo usa el pintado de NetSurf (content/handlers/html/redraw.c, [HISPA]).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REDONDO_H
#define REDONDO_H
#include <stdbool.h>
#include "netsurf/types.h"
/* la caja (borde exterior x0,y0-x1,y1, en puntos de la página pintada), con
 * los radios (arriba-izq., arriba-der., abajo-der., abajo-izq.), el fondo y
 * un borde liso de un color (anchos: arriba, derecha, abajo, izquierda), con
 * su opacidad (0..256: rgba(...)) */
bool redondo_caja(int x0, int y0, int x1, int y1, const int radio[4], colour fondo, bool con_fondo,
                  const int ancho[4], colour borde, bool con_borde, int alfa_fondo, int alfa_borde);
/* la sombra de fuera de la caja (box-shadow): desplazada (dx, dy),
 * difuminada (difu), agrandada (ext), del color con su opacidad (0..256) */
bool redondo_sombra(int x0, int y0, int x1, int y1, const int radio[4], int dx, int dy, int difu, int ext,
                    colour color, int alfa);
#endif
