/* pinfis.h - (JUEGOS) la física del pinball (pinfis.c): la bola rueda por el
 * plano de la mesa (2D, en unidades de la mesa: unos milímetros), con la
 * gravedad de la mesa inclinada, y choca con segmentos, círculos y flippers
 * a paso fijo, con tantos subpasos como haga falta para que nada se mueva más
 * de un cuarto del radio de la bola en cada uno (así no atraviesa nada a
 * ninguna velocidad). No llama al sistema: se prueba en el anfitrión
 * (prueba/prueba_pinball.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PINFIS_H
#define PINFIS_H

#define CAPA_MESA 1
#define CAPA_RAMPA 2
#define CAPA_TODAS 3

/* lo que es cada cosa con la que se choca */
enum { F_PARED, F_GOMA, F_HONDA, F_DIANA, F_PUERTA, F_BUMPER, F_POSTE, F_FLIPPER, F_BOLA };

struct f_seg {
	float ax, ay, bx, by;
	float r;		/* lo gruesa (la mitad): la bola choca a radio + r de la raya */
	float rebote;
	unsigned char capas, tipo, activo, relleno;
	short id;
	float x0, y0, x1, y1;	/* (lo que ocupa, con su grosor: lo calcula f_paso la primera vez) */
};
struct f_circ {
	float x, y, r;
	float rebote;
	unsigned char capas, tipo, activo, relleno;
	short id;
};
/* un flipper: un palo de radio r0 en el eje y r1 en la punta, que gira entre
 * «reposo» y «arriba» (radianes, en la mesa: x a la derecha, y hacia el
 * jugador); a «vel_sube» y «vel_baja» radianes por segundo */
struct f_flip {
	float px, py, largo, r0, r1;
	float reposo, arriba, ang, w;
	float vel_sube, vel_baja;
	int pulsado, activo;
};
/* una raya que avisa cuando la bola la cruza (los carriles, la rampa) */
struct f_sensor {
	float ax, ay, bx, by;
	unsigned char capas, relleno[3];
	short id;
};
struct f_bola {
	float x, y, vx, vy;
	int capa, viva;
};

#define F_MAX_BOLAS 4
#define F_MAX_FLIPS 4
struct f_mesa {
	struct f_seg *segs;
	int n_segs;
	struct f_circ *circs;
	int n_circs;
	struct f_sensor *sensores;
	int n_sensores;
	struct f_flip flips[F_MAX_FLIPS];
	int n_flips;
	struct f_bola bolas[F_MAX_BOLAS];
	float radio;		/* el de la bola */
	float g;		/* la gravedad en el plano (hacia +y) */
	float g_rampa;		/* en la rampa (que sube más) */
	float roce;		/* lo que frena rodar, por segundo */
	float roce_choque;	/* lo que se pierde de lado en cada choque (0,02) */
	float tope;		/* la velocidad más alta */
	float patada_bumper, patada_honda;
	/* lo que pasa (los puntos, los sonidos): quién, el número de la cosa, la fuerza y la bola */
	void (*choque)(int tipo, int id, float fuerza, int bola);
	/* la bola ha cruzado un sensor (sentido: 1 de delante atrás, -1 al revés) */
	void (*cruce)(int id, int bola, int sentido);
	/* las cuentas (las pruebas) */
	long subpasos, choques;
	int preparada;		/* (las cajas de los segmentos ya están hechas) */
};

void f_paso(struct f_mesa *m, float dt);
/* un empujón a la mesa: la bola se lleva (vx, vy) */
void f_empuja(struct f_mesa *m, float vx, float vy);
/* la punta de un flipper */
void f_punta(const struct f_flip *f, float *x, float *y);
#endif
