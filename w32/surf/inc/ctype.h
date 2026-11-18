/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_CTYPE_H
#define HS_CTYPE_H
int isalnum(int); int isalpha(int); int iscntrl(int); int isdigit(int); int isgraph(int);
int islower(int); int isprint(int); int ispunct(int); int isspace(int); int isupper(int);
int isxdigit(int); int tolower(int); int toupper(int);
#define isascii(c) ((unsigned)(c) < 128)
static inline int isblank(int c) { return c == ' ' || c == '\t'; }
#endif
