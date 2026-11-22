/* etiquetas.h - el Reproductor: lo que dice cada canción de sí misma (ID3v1,
 * ID3v2.2/2.3/2.4, los comentarios de Vorbis de los OGG y los FLAC, el INFO
 * de los WAV), su carátula (la imagen tal cual: JPEG o PNG) y el ReplayGain.
 * Todo en UTF-8. Portable; se prueba con ASan en el anfitrión (pruebas/).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_ETIQUETAS_H
#define REPRO_ETIQUETAS_H
#include "so.h"

typedef struct {
	char titulo[128], artista[128], album[128], anyo[16], pista[16], genero[48], comentario[128];
	int hay_rg_pista, hay_rg_album;
	int rg_pista, rg_album;		/* en centésimas de dB */
	int rg_pista_pico, rg_album_pico;	/* en milésimas (1000 = a tope) */
	u8 *caratula;			/* (de so_pide) la imagen, o NULL */
	u32 caratula_largo;
} etiquetas;

#define MAX_CARATULA (8u * 1024 * 1024)

/* de un archivo (por su cabeza y su cola); con «caratula», también la imagen */
int etiquetas_lee(const char *ruta, etiquetas *e, int caratula);
/* de lo que ya está en la memoria (para el fuzz y las pruebas) */
void etiquetas_id3v2(const u8 *d, u32 n, etiquetas *e, int caratula);
void etiquetas_id3v1(const u8 *d, u32 n, etiquetas *e);
void etiquetas_vorbis(const u8 *d, u32 n, etiquetas *e, int caratula);
void etiquetas_flac(const u8 *d, u32 n, etiquetas *e, int caratula);
void etiquetas_ogg(const u8 *d, u32 n, etiquetas *e, int caratula);
void etiquetas_wav(const u8 *d, u32 n, etiquetas *e, int caratula);
void etiquetas_suelta(etiquetas *e);
const char *genero_id3(int n);
#endif
