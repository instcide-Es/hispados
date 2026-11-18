/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SYS_UTSNAME_H
#define HS_SYS_UTSNAME_H
struct utsname { char sysname[32], nodename[32], release[32], version[32], machine[32]; };
int uname(struct utsname *);
#endif
