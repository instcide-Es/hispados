/* pe.h - (COMPAT) mirar un PE por encima: si es de 386, si es una DLL y el
 * nombre con que se exporta (el de su directorio de exportaciones: así se
 * sabe qué DLL es aunque el CAB la llame «FL_msvcr100_dll_x86...»).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_PE_H
#define COMPAT_PE_H
#include "so.h"
typedef struct { int dll; char nombre[64]; u32 n_exporta; u16 maquina; } pe_info;
/* 0 si es un PE de 386 (y lo que se sabe en *i); -1 si no */
int pe_mira(const u8 *d, u32 n, pe_info *i);
/* un recurso por su tipo (número: 10 RCDATA) y su nombre («LICENSE»): dónde
 * empieza en el archivo y lo que mide. 0 si está */
int pe_recurso(const u8 *d, u32 n, u32 tipo, const char *nombre, u32 *desde, u32 *tam);
#endif
