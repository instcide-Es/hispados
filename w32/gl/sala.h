/* sala.h - la sala de medir de HispaGL (sala.c): como las de los juegos de
 * 1997-1999 (paredes con mapa de luz por multitextura, modelos con luz por
 * vértice, mezcla, el arma delante con glDepthRange, la barra en 2D).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HGL_SALA_H
#define HGL_SALA_H
void juego_ini(void);
void juego_dibuja(int w, int h, int cuadro);
void juego_filtro(int min, int mag);
#endif
