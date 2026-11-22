/* motor.h - el Reproductor: el motor que hace sonar la lista. Un solo hilo:
 * motor_ciclo() se llama a cada vuelta del bucle del programa y mantiene la
 * cola del sonido llena (unos 250 ms por delante) con trozos cortos, para
 * que la ventana no se pare; mientras tanto abre la canción siguiente (sin
 * huecos y el fundido), y sabe qué se está oyendo de verdad (lo que ha
 * salido por la tarjeta, no lo que se ha decodificado).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_MOTOR_H
#define REPRO_MOTOR_H
#include "lista.h"
#include "dsp.h"
#include "etiquetas.h"

enum { M_PARADO, M_SONANDO, M_PAUSA };
enum { RG_NO, RG_PISTA, RG_DISCO };

typedef struct {
	dec d;
	int cancion;			/* su número en la lista */
	int abierta, lista;		/* dec_abre hecho; dec_paso dio 1 */
	int fallo;
	remuestreo rm;
	estira es;
	float ganancia;			/* (ReplayGain) */
	u64 fin_tramo;			/* (una pista de una hoja .CUE) la trama donde acaba; 0: al final */
	int acabada;
	int vaciando;			/* el decodificador ya no da más: se saca lo que queda dentro */
	u64 entrada, sacadas;		/* lo que ha entrado (del decodificador) y lo que ha salido */
	u64 heredadas;			/* (sin huecos) lo de la de antes que aún estaba dentro */
	etiquetas et;			/* las suyas (sin la carátula) */
} voz;

typedef struct {
	u64 salida;			/* la trama de la salida donde empieza */
	int cancion;
	u64 pos;			/* la trama de la canción ahí */
	u32 hz;				/* (de la canción) */
	float velocidad;
} marca;
#define MAX_MARCAS 64

#define VIS_N 16384			/* lo último que ha salido, para la visualización (en mono) */

typedef struct {
	lista *l;
	int estado;
	u32 hz;				/* la de la salida */
	voz v[2];			/* la que suena y la siguiente */
	int va;				/* cuál de las dos es la que suena */
	ecualizador eq;
	float velocidad;
	int volumen;			/* 0 a 100 (el de este programa en el mezclador) */
	int fundido;			/* el fundido entre canciones, en segundos (0: no) */
	int sin_huecos;
	int replaygain;			/* RG_NO, RG_PISTA, RG_DISCO */
	u64 salida;			/* las tramas dadas a la tarjeta desde que se abrió */
	marca marcas[MAX_MARCAS];
	int n_marcas, primera_marca;
	float vis[VIS_N];
	int sin_tarjeta;		/* (no hay tarjeta: el tiempo pasa igual) */
	u32 ms_sin_tarjeta;
	int oida;			/* la canción que se oye ahora (de la lista) */
	u32 pos_oida_ms;
	int cambio;			/* sube cuando cambia lo que se oye */
	char error[128];		/* lo último que ha fallado (para la barra de estado) */
	u32 ciclos_dsp, ciclos_dec, ms_medidos;	/* (para medir) */
	u64 tramas_medidas;
	int objetivo_ms;
	int cebando;			/* (al empezar y tras buscar) la tarjeta en pausa hasta que haya cola */
	float *tmp1, *tmp2, *tmp3;
	int tmp_n;
	i16 *s16;
} motor;

int motor_prepara(motor *m, lista *l);
void motor_ciclo(motor *m, u32 presupuesto_ms);	/* lo que puede tardar como mucho */
void motor_toca(motor *m, int cancion);		/* desde el principio */
void motor_pausa(motor *m);			/* pausa o sigue */
void motor_para(motor *m);
void motor_siguiente(motor *m);
void motor_anterior(motor *m);
void motor_busca_ms(motor *m, u32 ms);
void motor_velocidad(motor *m, float v);
void motor_volumen(motor *m, int v);
u32 motor_pos_ms(motor *m);			/* lo que se oye ahora */
u32 motor_total_ms(motor *m);
voz *motor_voz_oida(motor *m);
/* las últimas «n» muestras (en mono) de lo que se oye */
void motor_lo_oido(motor *m, float *d, int n);
int motor_cola_ms(motor *m);			/* lo que hay en la cola de la tarjeta */
void motor_cierra(motor *m);
extern u32 dmod_hz;
/* la duración (segundos) sin decodificarla entera; 0 si no se sabe */
u32 duracion_rapida(const origen *o);
#endif
