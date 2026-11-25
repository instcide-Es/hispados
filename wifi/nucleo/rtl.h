/*
 * rtl.h - La Realtek RTL8723BS (Wi-Fi por SDIO de las tabletas Bay Trail:
 * el Acer One 10 S1002 y el Medion S2217) para el Wi-Fi de Hispa-DOS: lo que
 * el servicio (castel/nucleo32/wifi/servicio.c) ve de wifi/nucleo/radio_rtl.c
 * (el controlador, sacado del de Linux) y de radio_rtl_pc.c (dónde está la
 * controladora SDIO y cómo se enciende el chip en un equipo de verdad).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior
 * (radio_rtl.c, que sale de Linux, es GPL-2.0: ver su cabecera).
 */
#ifndef RTL_H
#define RTL_H

#include "nucleo.h"
#include "sdio.h"

/* radio_rtl.c */
const char *const *rtl_nombres_fw(void);	/* los nombres del firmware, por orden; NULL al final */
int rtl_pon_fw(const o8 *datos, d32 tam, const char *nombre);	/* 0 si vale (rtl8723bs_nic.bin) */
int rtl_es_fw(const o8 *datos, d32 tam);	/* ¿lleva la cabecera de un firmware de la 8723B? */
/* la tarjeta por la controladora de «p»: SDIO, su EFUSE (la MAC); 0 bien */
int rtl_arranca(const struct sdio_plat *p, struct radio **r);
int rtl_enciende(void);				/* el firmware, el MAC, el BB y la radio */
void rtl_cuentas(d32 *bien, d32 *mal, int *vel);
void rtl_resumen(void);
/* para las pruebas: cuántas veces ha fallado la E/S y el canal puesto */
extern int rtl_fallos_es, rtl_canal;

/* radio_rtl_pc.c (lo de servicio.c) */
int rtl_busca(void);				/* 0 hay; 1 no hay; <0 hay, pero no se puede */
int rtl_monta(struct radio **r);
/* las pruebas (WIFISIM con «chiprtl») ponen aquí el modelo de la tarjeta */
extern const struct sdio_plat *rtl_plat_prueba;

#endif
