/* dsp.h - el Reproductor: lo que se le hace al sonido entre el decodificador
 * y la tarjeta, en estéreo de coma flotante:
 *   - remuestrear (de la frecuencia de la canción a la de la tarjeta), con
 *     un filtro de seno cardinal enventanado (Kaiser), 16 coeficientes;
 *   - la velocidad (de 0,5 a 2) sin cambiar el tono: WSOLA (trozos de 40 ms
 *     que se solapan por la mitad, cada uno donde mejor encaja);
 *   - el ecualizador de 10 bandas (60, 170, 310, 600 Hz, 1, 3, 6, 12, 14 y
 *     16 kHz; de -12 a +12 dB) con su preamplificador: filtros de pico
 *     (biquad) en cascada; las bandas a 0 dB no se calculan;
 *   - el espectro (FFT de 512) para la visualización.
 * Portable.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_DSP_H
#define REPRO_DSP_H
#include "so.h"

/* ---- remuestrear ---- */
#define RM_TAPS 16
#define RM_FASES 256
typedef struct {
	u32 de, a;			/* las frecuencias */
	u64 paso, fase;			/* 32.32: lo que se avanza en la entrada por cada salida */
	float h[RM_FASES + 1][RM_TAPS];
	float hist[2 * RM_TAPS * 2];	/* lo último de la entrada (para el filtro) */
	float *ent;			/* la entrada pendiente */
	int en_ent, cabe_ent;
	int directo;
} remuestreo;
int rm_prepara(remuestreo *r, u32 de, u32 a);
/* mete «n» tramas y saca las que salgan (hasta «cabe»): las sacadas */
int rm_hace(remuestreo *r, const float *e, int n, float *s, int cabe);
void rm_vacia(remuestreo *r);
void rm_suelta(remuestreo *r);

/* ---- la velocidad sin cambiar el tono ---- */
typedef struct {
	u32 hz;
	float velocidad;		/* 1: tal cual */
	int n, hop, busca;		/* el trozo, lo que avanza la salida, lo que se busca alrededor */
	float *ent;			/* lo que ha entrado y no se ha usado */
	int en_ent, cabe_ent;
	double pos;			/* dónde va el siguiente trozo en la entrada */
	int prev;			/* dónde empezó el de antes */
	float *cola;			/* la segunda mitad del trozo de antes (para solapar) */
	float *ventana;
	int primero;
} estira;
int es_prepara(estira *e, u32 hz);
void es_velocidad(estira *e, float v);
int es_hace(estira *e, const float *ent, int n, float *s, int cabe);
void es_vacia(estira *e);
void es_suelta(estira *e);

/* ---- el ecualizador ---- */
#define BANDAS 10
extern const int frecuencias_eq[BANDAS];
typedef struct {
	int activo;
	int preamp;			/* en décimas de dB */
	int banda[BANDAS];		/* en décimas de dB (-120 a 120) */
	u32 hz;
	float b0[BANDAS], b1[BANDAS], b2[BANDAS], a1[BANDAS], a2[BANDAS];
	float z[BANDAS][2][2];		/* el estado de cada banda y lado */
	float g_pre;
	int calculado;
} ecualizador;
void eq_prepara(ecualizador *q, u32 hz);
void eq_cambia(ecualizador *q);		/* tras tocar las bandas o el preamplificador */
void eq_hace(ecualizador *q, float *s, int tramas);
void eq_vacia(ecualizador *q);

typedef struct { const char *nombre; signed char v[BANDAS + 1]; } preajuste;	/* v[0], el preamplificador; en dB */
extern const preajuste preajustes[];
extern const int n_preajustes;

/* ---- el espectro: de 512 muestras, 256 magnitudes (de 0 a 1, aprox.) ---- */
#define FFT_N 512
void espectro(const float *mono, float *mag);

/* de dB (en centésimas) a veces */
float db_a_veces(int centesimas);
float potf_(float x, float y);
#endif
