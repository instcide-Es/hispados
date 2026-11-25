/*
 * sdio.h - SDIO sobre una controladora SDHCI (la tercera de las tabletas Bay
 * Trail, la del Wi-Fi), para el Wi-Fi de Hispa-DOS (WIFI.BIN): arrancar la
 * tarjeta (CMD5, CMD3, CMD7), leer su CCCR, su CIS y el FBR de la función 1,
 * los 4 bits y la alta velocidad, y las órdenes de E/S: CMD52 (un octeto) y
 * CMD53 (de octeto en octeto o por bloques), por PIO. Sin interrupciones: la
 * de la tarjeta (la línea DAT1) se mira en el registro de avisos.
 *
 * Según las normas públicas «SD Host Controller Simplified Specification
 * 3.00» y «SDIO Simplified Specification 3.00» (escrito aquí).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef SDIO_H
#define SDIO_H

#include "nucleo.h"

/* lo que hace falta de la controladora: sus registros (de 1, 2 o 4
 * octetos) y el tiempo. En WIFI.BIN lo da radio_rtl_pc.c (la memoria de la
 * controladora a la vista); en las pruebas, el modelo de tests/wifi/rtl_sim.c */
struct sdio_plat {
	d32 (*lee)(d32 reg, int ancho);
	void (*pon)(d32 reg, int ancho, d32 v);
	void (*espera_us)(d32 us);
	d32 (*ms)(void);
};

struct sdio {
	const struct sdio_plat *p;
	d32 version;		/* la de la norma SDHCI: 0 = 1.00, 1 = 2.00, 2 = 3.00 */
	d32 cap, cap2;
	d32 base_khz, khz;
	int ancho;		/* 1 o 4 bits */
	int alta;		/* alta velocidad (50 MHz) */
	p16 rca;
	d32 ocr;
	int nfunc;		/* las funciones de E/S que dice la tarjeta (sin la 0) */
	o8 cccr, sdio_rev, sd_rev, caps, hs;
	p16 manf, carta;	/* el CISTPL_MANFID: fabricante y tarjeta */
	p16 bloque0_max;	/* lo que dice su CISTPL_FUNCE (función 0) */
	o8 interfaz1;		/* el código de la función 1 (FBR) */
	p16 bloque1;		/* el bloque de la función 1 (el que se usa: 512) */
	d32 resp;		/* la última respuesta (los 32 bits) */
	d32 ultimo_error;	/* los avisos de error de la última orden que ha fallado */
	d32 ordenes52, ordenes53, errores;
};

/* arranca la controladora y la tarjeta; 0 bien. Lo que pasa, al registro */
int sdio_arranca(struct sdio *s, const struct sdio_plat *p);
/* CMD52: un octeto de la función f en la dirección dir (17 bits) */
int sdio_lee8(struct sdio *s, int f, d32 dir, o8 *v);
int sdio_pon8(struct sdio *s, int f, d32 dir, o8 v);
/* CMD53 con la dirección que sube (OP code 1): n octetos. Hasta el bloque
 * (512), de octeto en octeto; si son más, por bloques (n se redondea al
 * bloque: quien llama da el sitio). Una sola orden siempre (las colas de la
 * Realtek llevan en la dirección lo que se manda) */
int sdio_lee(struct sdio *s, int f, d32 dir, void *b, int n);
int sdio_pon(struct sdio *s, int f, d32 dir, const void *b, int n);
/* ¿pide algo la tarjeta? (la interrupción de la tarjeta, por DAT1) */
int sdio_avisa(struct sdio *s);

#endif
