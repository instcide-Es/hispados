/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SYS_TYPES_H
#define HS_SYS_TYPES_H
#include <stddef.h>
#include <stdint.h>
typedef int ssize_t;
#ifndef HS_TIME_T
#define HS_TIME_T
typedef long time_t;
#endif
typedef long off_t;
typedef int pid_t;
typedef unsigned short mode_t;
typedef unsigned int dev_t;
typedef unsigned short ino_t;
typedef short nlink_t;
typedef short uid_t;
typedef short gid_t;
typedef long suseconds_t;
typedef unsigned int u_int;
typedef unsigned char u_char;
#endif
