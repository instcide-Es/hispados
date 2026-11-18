/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SYS_SELECT_H
#define HS_SYS_SELECT_H
#include <sys/time.h>
#define FD_SETSIZE 64
typedef struct fd_set { unsigned fd_count; unsigned fd_array[FD_SETSIZE]; } fd_set;
#define FD_ZERO(s) ((s)->fd_count = 0)
#define FD_SET(f, s) ((s)->fd_array[(s)->fd_count++] = (f))
#define FD_ISSET(f, s) 0
#endif
