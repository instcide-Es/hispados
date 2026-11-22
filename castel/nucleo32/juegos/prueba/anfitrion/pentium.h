/* pentium.h de mentira, para probar en el anfitrión (Linux) lo de los juegos
 * que no necesita la pantalla: las llamadas al sistema van a sc() de
 * prueba/sistema.c. Las constantes son las de ../../comprim/pentium.h.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PENTIUM_ANFITRION_H
#define PENTIUM_ANFITRION_H
#include <stdint.h>
#define sc sc_de_hispa_pentium
#include "../../../comprim/pentium.h"
#undef sc
uint32_t sc(uint32_t n, uint32_t a, uint32_t b, uint32_t c);
#endif
