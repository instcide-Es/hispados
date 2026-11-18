/* Cabecera de la biblioteca de C para compilar NetSurf (terceros/netsurf) en
 * NAVEGA2.EXE, el Navegador completo de Hispa-486 (w32/surf, docs/486/NAVEGADOR2.md):
 * lo de MSVCRT.DLL con sus estructuras, y lo que falta en w32/surf/apoyo.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HS_SETJMP_H
#define HS_SETJMP_H
typedef int jmp_buf[16];
int _setjmp(jmp_buf);
#define setjmp(b) _setjmp(b)
void longjmp(jmp_buf, int) __attribute__((noreturn));
#endif
