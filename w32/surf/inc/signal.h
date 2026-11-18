/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SIGNAL_H
#define HS_SIGNAL_H
typedef int sig_atomic_t;
#define SIGINT 2
#define SIGSEGV 11
#define SIGPIPE 13
#define SIG_IGN ((void (*)(int))1)
#define SIG_DFL ((void (*)(int))0)
void (*signal(int, void (*)(int)))(int);
#endif
