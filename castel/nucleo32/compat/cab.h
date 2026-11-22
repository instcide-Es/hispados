/* cab.h - (COMPAT) leer los CAB de Microsoft (MSCF 1.3): sin comprimir,
 * MSZIP (con miniz) y LZX (lzx.c), sin Quantum ni los que siguen en otro
 * CAB. También el CAB que va dentro de un ejecutable que se extrae solo
 * (los vcredist_x86.exe, el redistribuible de DirectX). C99 portable; lo
 * que se lee, por una función (de un archivo a trozos, o de la memoria):
 * así un CAB de 100 MB no tiene que caber en la memoria.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_CAB_H
#define COMPAT_CAB_H
#include "so.h"

/* leer n octetos desde «pos» del origen: los leídos (n si va bien) o -1 */
typedef i32 (*cab_lee)(void *u, u32 pos, void *d, u32 n);

typedef struct {
	char nombre[160];	/* tal cual (con «\» si lleva carpetas) */
	u32 tam, desde;		/* lo que mide y dónde empieza en su carpeta */
	u16 carpeta, atributos;
} cab_archivo;

typedef struct {
	u32 inicio;		/* el primer CFDATA (desde el principio del origen) */
	u16 n_datos, tipo;	/* tipo: 0 nada, 1 MSZIP, 3 LZX (y los bits de la ventana en 8-12) */
} cab_carpeta;

typedef struct {
	cab_lee lee;
	void *u;
	u32 base, tam;		/* dónde empieza el CAB en el origen y lo que mide */
	u8 res_datos;
	u16 n_carpetas, n_archivos;
	cab_carpeta *carpetas;
	cab_archivo *archivos;
} cab;

/* abre el CAB que empieza en «base»: 0 si vale (y *error si no) */
int cab_abre(cab *c, cab_lee lee, void *u, u32 base, u32 tam_origen, const char **error);
void cab_cierra(cab *c);
/* el primer CAB que valga desde «desde» (para los ejecutables que se extraen
 * solos): su posición, o -1 */
i32 cab_busca(cab_lee lee, void *u, u32 tam_origen, u32 desde);

/* sacar archivos: «quiere» dice si uno se saca (1) o no (0); lo de dentro va
 * a «escribe» a trozos, en orden (último = 1 con el último trozo; un archivo
 * vacío, una llamada con n = 0). «avance», si no es NULL, con lo hecho y el
 * total (en octetos de lo comprimido). 0 si va bien; con -1, *error */
typedef int (*cab_quiere)(void *u, const cab_archivo *a);
typedef int (*cab_escribe)(void *u, const cab_archivo *a, const u8 *d, u32 n, int ultimo);
typedef void (*cab_avance)(void *u, u32 hecho, u32 total);
int cab_saca(cab *c, cab_quiere quiere, cab_escribe escribe, cab_avance avance, void *u, const char **error);

/* el origen más sencillo: un trozo de memoria (u = cab_memoria *) */
typedef struct { const u8 *d; u32 n; } cab_memoria;
i32 cab_lee_memoria(void *u, u32 pos, void *d, u32 n);
#endif
