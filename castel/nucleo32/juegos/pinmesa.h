/* pinmesa.h - (JUEGOS) la mesa de Hispa-Pinball, «La Feria» (pinmesa.c): lo
 * que choca (para la física, pinfis.c), las piezas con volumen (para pintarlas
 * en perspectiva, pinball.c), las luces y el dibujo del suelo de la mesa.
 * Las medidas son de la mesa: 500 de ancho (con el carril del lanzador) por
 * 1000 de largo; x a la derecha, y hacia el jugador.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PINMESA_H
#define PINMESA_H
#include "juego.h"
#include "pinfis.h"

#define MESA_AN 500.0f
#define MESA_LARGO 1000.0f
#define RADIO_BOLA 12.5f
/* dónde descansa la bola en el lanzador, y la goma del émbolo */
#define LANZA_X 468.0f
#define LANZA_Y 958.0f
#define EMBOLO_Y 971.0f
/* la tómbola (el agujero que se traga la bola y la devuelve) */
#define TOMBOLA_X 300.0f
#define TOMBOLA_Y 585.0f
/* la rampa (la montaña rusa): sube de RAMPA_Y0 a RAMPA_Y1 por x de RAMPA_X0 a RAMPA_X1 */
#define RAMPA_X0 14.0f
#define RAMPA_X1 84.0f
#define RAMPA_Y0 470.0f
#define RAMPA_Y1 250.0f
#define RAMPA_ALTO 52.0f

/* los números de las cosas (f_seg.id, f_circ.id, f_sensor.id) */
enum {
	ID_NADA = 0,
	ID_BUMPER0 = 10, ID_BUMPER1, ID_BUMPER2,
	ID_HONDA_I = 20, ID_HONDA_D,
	ID_DIANA0 = 30, ID_DIANA1, ID_DIANA2,
	ID_PUERTA = 40,
	S_CARRIL0 = 50, S_CARRIL1, S_CARRIL2,	/* F E R (arriba) */
	S_ENTRA_I = 60, S_ENTRA_D,		/* I A (los pasillos de los flippers) */
	S_FUERA_I = 65, S_FUERA_D,		/* los de fuera */
	S_RAMPA_ENTRA = 70, S_RAMPA_SALE,
	S_LANZADOR = 75,			/* la bola sale del carril del lanzador */
};

/* las luces */
enum {
	LUZ_F, LUZ_E, LUZ_R, LUZ_I, LUZ_A,
	LUZ_RAMPA1, LUZ_RAMPA2, LUZ_RAMPA3,
	LUZ_DIANA_L1, LUZ_DIANA_L2, LUZ_DIANA_L3,	/* las lucecitas de delante de las dianas */
	LUZ_DIANA1, LUZ_DIANA2, LUZ_DIANA3,		/* las dianas mismas (encendida: de pie) */
	LUZ_TOMBOLA, LUZ_EXTRA,
	LUZ_BUMPER1, LUZ_BUMPER2, LUZ_BUMPER3,
	LUZ_X2, LUZ_X3, LUZ_X4, LUZ_X5,
	LUZ_MISION1, LUZ_MISION2, LUZ_MISION3, LUZ_MISION4, LUZ_MISION5, LUZ_GRAN,
	LUZ_OTRA_VEZ,
	N_LUCES
};

/* una pieza con volumen: un prisma de planta convexa (hasta 20 puntos) entre z0 y z1 */
#define PZ_MAX_PUNTOS 20
struct pieza {
	float p[2 * PZ_MAX_PUNTOS];
	int n;
	float z0, z1;
	u32 lado, arriba;	/* los colores de los lados y de arriba */
	u32 arriba_luz;		/* (con luz: el de arriba encendida) */
	short luz;		/* -1, ninguna; si no, la luz: solo lo de arriba (o, si «solo_luz», todo) */
	u8 solo_luz;		/* la pieza solo se ve con la luz encendida (las dianas de pie) */
	u8 brillo;		/* lo de arriba con un brillo (plástico, metal) */
	float orden;		/* para pintarlas de atrás adelante */
	u8 inclinada;		/* lo de arriba no es plano: cada punto a su altura (zt) */
	float zt[PZ_MAX_PUNTOS];
};
/* la altura de la rampa en y (0 abajo, RAMPA_ALTO arriba) */
float altura_rampa(float y);
#define MAX_PIEZAS 400
extern struct pieza piezas[MAX_PIEZAS];
extern int n_piezas;

extern struct f_seg mesa_segs[];
extern int mesa_n_segs;
extern struct f_circ mesa_circs[];
extern int mesa_n_circs;
extern struct f_sensor mesa_sensores[];
extern int mesa_n_sensores;

/* hace todo (una vez): la física de la mesa m y las piezas */
void mesa_construye(struct f_mesa *m);
/* el dibujo del suelo, a «kt» puntos por unidad: apagado, encendido y el
 * mapa de las luces (en cada punto, 0 o la luz + 1) */
void mesa_suelo(lienzo *apagado, lienzo *encendido, u8 *luces, float kt);
/* dónde está cada luz (en la mesa): para que el jugador sepa qué es */
extern const char *const nombre_luz[N_LUCES];
#endif
