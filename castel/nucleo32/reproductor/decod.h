/* decod.h - el Reproductor: los decodificadores. Cada formato da tramas de
 * coma flotante (de -1 a 1), entrelazadas, en estéreo (lo mono se dobla; lo
 * de más de dos canales se mezcla a dos), a su frecuencia.
 *
 * Abrir es de a poco: dec_abre() solo mira la cabeza del archivo, y
 * dec_paso() hace un trozo corto de lo que falta (leer 64 K de un OGG o de un
 * módulo, que van enteros a la memoria; buscar la primera trama de un MP3...)
 * hasta que da 1 (listo) o -1 (no se puede). Así el motor sigue dando
 * sonido mientras se abre la canción siguiente (sin huecos, y el fundido).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_DECOD_H
#define REPRO_DECOD_H
#include "so.h"

enum {
	F_NADA, F_MP3, F_VORBIS, F_FLAC, F_WAV, F_MOD, F_CDDA, F_OPUS
};

/* de dónde salen los octetos: un archivo, o un trozo de él (una pista de
 * una imagen .BIN de CD, con su hoja .CUE), o una pista de un CD de verdad */
typedef struct {
	char ruta[MAX_RUTA];
	u64 desde, largo;		/* (un trozo: largo > 0) */
	char lector;			/* (un CD: su letra; 0 si no) */
	u32 lba, sectores;		/* (un CD: la pista) */
	u32 tramo_desde, tramo_hasta;	/* (una pista de una hoja .CUE sobre un FLAC, un WAV o
					 * un MP3 enteros: en 75 avos de segundo; hasta = 0, al final) */
} origen;

typedef struct dec dec;
struct dec {
	int formato;
	char nombre_formato[32];	/* «MP3», «OGG Vorbis», «FLAC», «WAV (PCM)»... */
	u32 hz;
	int canales;			/* los del archivo (lo que sale es siempre estéreo) */
	int bits;			/* (los de PCM y FLAC; 0 si no vienen al caso) */
	u64 total;			/* las tramas (0: no se sabe) */
	u32 kbps;			/* (medio) */
	int buscable;
	u64 pos;			/* la trama por la que va */
	int listo, acabado;
	char error[96];

	/* lo de cada formato */
	void *priv;
	int (*paso)(dec *d);
	int (*lee)(dec *d, float *s, int tramas);
	int (*busca)(dec *d, u64 trama);
	void (*cierra)(dec *d);

	origen o;
	so_archivo *a;
	/* (los que van enteros a la memoria) */
	u8 *mem;
	u32 mem_largo, mem_leido;
};

/* lo que dice la cabeza de un archivo (y su nombre) */
int dec_formato(const u8 *cabeza, u32 n, const char *ruta);
int dec_abre(dec *d, const origen *o);	/* 0 si se ha podido empezar */
int dec_paso(dec *d);			/* 1 listo; 0 aún no; -1 no se puede */
/* las tramas leídas (0: se ha acabado) */
int dec_lee(dec *d, float *s, int tramas);
int dec_busca(dec *d, u64 trama);
void dec_cierra(dec *d);
const char *dec_ext_de(const char *ruta);	/* la extensión en mayúsculas («MP3») */
int dec_ext_vale(const char *ruta);	/* ¿es de los que se tocan? (por la extensión) */

/* cada formato (decod_*.c) */
int dmp3_abre(dec *d);
int dvorbis_abre(dec *d);
int dflac_abre(dec *d);
int dwav_abre(dec *d);
int dmod_abre(dec *d);
int dcdda_abre(dec *d);

/* lo de más de dos canales (o uno), a estéreo; vale con e == s */
void a_estereo(const float *e, int canales, float *s, int tramas);

/* lo común: leer del origen (respeta el trozo) */
int org_lee(dec *d, void *b, u32 n);
int org_salta(dec *d, u64 pos);
u64 org_largo(dec *d);
#define TROZO_CARGA (64u * 1024)
int org_carga_paso(dec *d, u32 max);	/* (los que van a la memoria) 1 cuando está todo */
#endif
