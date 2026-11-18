/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_DIRENT_H
#define HS_DIRENT_H
#include <sys/types.h>
struct dirent { ino_t d_ino; unsigned char d_type; char d_name[260]; };
#define DT_UNKNOWN 0
#define DT_DIR 4
#define DT_REG 8
typedef struct HS_DIR DIR;
DIR *opendir(const char *); struct dirent *readdir(DIR *); int closedir(DIR *);
int scandir(const char *, struct dirent ***, int (*)(const struct dirent *), int (*)(const struct dirent **, const struct dirent **));
int alphasort(const struct dirent **, const struct dirent **);
#endif
