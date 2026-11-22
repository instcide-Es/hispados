/* historia.h - Pintura 2: deshacer y rehacer sin límite práctico, por
 * zonas. La lámina se parte en baldosas de 64 x 64; un paso (una
 * pincelada, un relleno, una figura...) guarda solo las baldosas que de
 * verdad ha cambiado, como eran antes, y todas juntas comprimidas (Flate,
 * de miniz). Lo que cambia el tamaño (girar, recortar, cambiar el tamaño)
 * guarda la imagen entera, también comprimida. Si la historia pasa de su
 * tope de memoria, se olvidan los pasos más viejos.
 *
 * Mientras dura un paso: his_toca() antes de cambiar una zona (guarda sus
 * baldosas como eran), his_viejo() da cómo era un punto y his_cob() la
 * «cobertura» del trazo en ese punto (para que una pincelada translúcida
 * no se oscurezca donde pasa dos veces); his_restaura() lo deja como
 * estaba (para dibujar otra vez una figura mientras se arrastra).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PIN_HISTORIA_H
#define PIN_HISTORIA_H
#include "lamina.h"

#define BALDOSA 64

/* la lámina de la que se guarda la historia (al abrir o al cambiar de
 * tamaño); con «olvida», sin historia */
void his_lamina(lamina *l, int olvida);
void his_suelta(void);
void his_tope(u32 octetos);		/* (48 MB si no se dice) */

void his_empieza(void);
int his_en_paso(void);
/* [x0, x1) x [y0, y1) va a cambiar (recortado a la lámina). 0 si no hay
 * memoria para guardarlo (entonces ese paso no se podrá deshacer) */
int his_toca(int x0, int y0, int x1, int y1);
/* cómo era el punto (x, y) al empezar el paso (dentro de lo tocado) */
u32 his_viejo(int x, int y);
/* la cobertura del trazo en (x, y) (dentro de lo tocado; NULL sin memoria) */
u8 *his_cob(int x, int y);
/* todo como al empezar el paso (sigue abierto); en *zona lo que cambia */
void his_restaura(int zona[4]);
/* el paso, acabado: a la historia (si ha cambiado algo); en zona, lo que cambió */
void his_acaba(const char *nombre);
void his_cancela(int zona[4]);
/* un paso que cambia la lámina entera (y quizá su tamaño): «antes» es como
 * era (se comprime y se suelta); la lámina de ahora es la de his_lamina */
int his_entera(const char *nombre, lamina *antes);

int his_se_deshace(void);
int his_se_rehace(void);
const char *his_que_deshace(void);
const char *his_que_rehace(void);
/* deshacer y rehacer: 1 si se ha hecho; *tamano: si ha cambiado el tamaño
 * de la lámina (entonces *l es otra); zona: lo que ha cambiado */
int his_deshace(lamina *l, int *tamano, int zona[4]);
int his_rehace(lamina *l, int *tamano, int zona[4]);
u32 his_memoria(void);
int his_pasos(void);

#endif
