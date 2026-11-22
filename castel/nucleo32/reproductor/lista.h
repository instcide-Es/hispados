/* lista.h - el Reproductor: la lista de reproducción (añadir archivos,
 * carpetas enteras —de a poco—, las pistas de un CD o de una hoja .CUE;
 * abrir y guardar .M3U, .M3U8 y .PLS; quitar, mover, ordenar; aleatorio y
 * repetir). Portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_LISTA_H
#define REPRO_LISTA_H
#include "decod.h"

typedef struct {
	origen o;
	char titulo[128];		/* lo que se enseña (de las etiquetas, o el nombre) */
	char artista[96];
	char album[96];
	u32 duracion;			/* en segundos (0: aún no se sabe) */
	u8 leida;			/* ya se han mirado sus etiquetas */
	u8 fallo;			/* no se ha podido tocar */
	u8 marcada;			/* elegida en la lista (para quitarla, moverla) */
} cancion;

enum { REPITE_NO, REPITE_TODO, REPITE_UNA };

#define MAX_CANCIONES 20000
#define MAX_PENDIENTES 512

typedef struct {
	cancion *c;
	int n, cabe;
	int actual;			/* la que suena (-1: ninguna) */
	int *orden;			/* (aleatorio) el orden barajado */
	int pos_orden;
	int aleatorio, repetir;
	/* las carpetas que quedan por mirar (añadir una carpeta es de a poco) */
	char *pendientes[MAX_PENDIENTES];
	int n_pendientes;
	int anadidas;			/* las de la carpeta que se está añadiendo */
	int cambios;			/* sube con cada cambio (para pintarla otra vez) */
} lista;

void lista_nueva(lista *l);
void lista_vacia(lista *l);
int lista_anade(lista *l, const origen *o, const char *titulo);	/* su número, o -1 */
int lista_anade_archivo(lista *l, const char *ruta);	/* (y las .M3U, .PLS, .CUE) */
void lista_anade_carpeta(lista *l, const char *ruta);	/* se apunta; lista_paso la mira */
int lista_paso(lista *l);		/* mira una carpeta pendiente; 1 si queda trabajo */
int lista_anade_cue(lista *l, const char *ruta);
int lista_anade_cd(lista *l, char letra, const so_toc *t);
int lista_lee(lista *l, const char *ruta);	/* .M3U, .M3U8 o .PLS: las que ha añadido, o -1 */
int lista_guarda(lista *l, const char *ruta);	/* por la extensión: .PLS o .M3U (UTF-8 con #EXTM3U) */
void lista_quita(lista *l, int i);
void lista_quita_marcadas(lista *l);
void lista_mueve(lista *l, int de, int a);
void lista_ordena(lista *l, int por);	/* 0 título, 1 artista, 2 ruta */
/* la siguiente y la anterior según aleatorio y repetir (-1: se acabó) */
int lista_siguiente(lista *l, int a_mano);
int lista_anterior(lista *l);
void lista_baraja(lista *l);
u32 lista_duracion(lista *l, int *sin_saber);
/* (para enseñarla) «Artista — Título» o el nombre */
void lista_nombre(const cancion *c, char *d, int cabe);
void origen_de_ruta(origen *o, const char *ruta);
#endif
