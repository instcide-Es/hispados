/* envio.h - (TERMO) «Enviar a Carlos» (envio.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ENVIO_H
#define ENVIO_H
/* ¿hay ENVIO.INI en esa carpeta (X:\TERMO)? */
int envio_hay(const char *carpeta_termo);
/* sube lo de la carpeta (y, si unidad, su \ARRANQUE.TXT y \ACPI.TXT): 0 todo
 * bien; 1 no se puede (sin preparar, nada que mandar); 2 sin red; 3 algo sin
 * mandar. en_consola: lo dice en la pantalla; si no, en ENVIO.EST */
int envio_principal(const char *carpeta_termo, char unidad, int en_consola);
#endif
