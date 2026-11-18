/* llamadas.h - Las funciones del núcleo Wi-Fi que llama el residente
 * (los mismos números están en drivers/hspwifi.asm).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef LLAMADAS_H
#define LLAMADAS_H
#define LL_VERSION 10		/* 0.10 */
#define LL_INICIA 0		/* petición: struct ll_inicio. Busca la tarjeta */
#define LL_ENCIENDE 1		/* enciende la radio (tras LL_MONTA) */
#define LL_SONDEA 2		/* -> bit 0: hay una trama; bit 1: hay registro */
#define LL_RECIBE 3		/* [0] búfer -> octetos */
#define LL_ENVIA 4		/* [0] trama, [1] octetos */
#define LL_BARRE 5
#define LL_REDES 6		/* [0] búfer de struct ll_red, [1] cuántas caben -> cuántas */
#define LL_CONECTA 7		/* [0] nombre, [1] su longitud, [2] PMK (32 octetos) o 0 */
#define LL_DESCONECTA 8
#define LL_ESTADO 9		/* [0] búfer (struct w_info + 12), [1] su tamaño */
#define LL_REGISTRO 10		/* [0] búfer, [1] tamaño -> octetos copiados */
#define LL_PMK_EMPIEZA 11	/* [0] contraseña (acabada en 0), [1] nombre, [2] longitud */
#define LL_PMK_SIGUE 12		/* [0] PMK (32) -> 1 cuando ha terminado */
#define LL_PARA 13
#define LL_MAC 14		/* [0] búfer de 6 */
#define LL_MONTA 15		/* (solo el residente) entre LL_INICIA y LL_ENCIENDE */
#define LL_OP_SIMULA 1
#define LL_OP_DETALLE 2
#define LL_OP_PANTALLA 4
struct ll_inicio {
	unsigned int base;	/* dirección física del bloque (y base de los segmentos) */
	unsigned int tam;	/* su tamaño */
	unsigned int fw_des[2];	/* dónde están los dos archivos del microcódigo (desde la base) */
	unsigned int fw_tam[2];
	unsigned int opciones;
};
struct ll_red {
	unsigned char bssid[6];
	unsigned char canal;
	signed char senal;
	unsigned char seguridad;
	unsigned char ssid_n;
	unsigned char ssid[32];
} __attribute__((packed));
#endif
