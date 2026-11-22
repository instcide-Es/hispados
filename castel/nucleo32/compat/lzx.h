/* lzx.h - (COMPAT) el descompresor LZX de los CAB de Microsoft (los
 * redistribuibles: los vcredist_x86.exe y el DirectX de junio de 2010 van
 * en LZX:21). Escrito aquí, de la descripción pública del formato
 * ([MS-PATCH] LZX DELTA y la de los CAB, «LZX Data Compression» del SDK
 * de los CAB): bloques literales, alineados y sin comprimir, la traducción
 * de las llamadas E8 de Intel, ventana de 2^15 a 2^21. C99 portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_LZX_H
#define COMPAT_LZX_H
#include "so.h"

typedef struct lzx lzx;
/* lo que da los octetos comprimidos, a trozos: 1 con un trozo (*d, *n > 0),
 * 0 si no hay más, -1 si falla */
typedef int (*lzx_tira)(void *u, const u8 **d, u32 *n);

/* uno nuevo, con ventana de 2^bits (15 a 21): NULL si no hay memoria o no vale */
lzx *lzx_nuevo(int bits);
void lzx_suelta(lzx *z);
/* al empezar una carpeta del CAB: todo a cero, y de dónde se lee */
void lzx_empieza(lzx *z, lzx_tira tira, void *u);
/* el trozo siguiente de lo descomprimido: n octetos (32768, o menos el
 * último de la carpeta) a «d». 0 si va bien; -1 si los datos están mal
 * (en lzx_error, por qué) */
int lzx_trozo(lzx *z, u8 *d, u32 n);
const char *lzx_error(const lzx *z);
#endif
