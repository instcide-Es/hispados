/* rtl_sim.h - El modelo de la controladora SDHCI con la RTL8723BS detrás
 * (tests/wifi/rtl_sim.c): para probar sdio.c y radio_rtl.c sin la tableta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef RTL_SIM_H
#define RTL_SIM_H
#include "nucleo.h"
#include "sdio.h"

/* monta el modelo con el punto de acceso de mentira «ap» (sim.c) detrás;
 * «espera_us» y «ms», el tiempo (el de mentira de la prueba o el de verdad).
 * Da cómo se ven los registros de la controladora */
const struct sdio_plat *rtl_sim_monta(struct radio *ap, void (*espera_us)(d32), d32 (*ms)(void));
/* el firmware que se espera (para comparar lo que llega); NULL: cualquiera */
void rtl_sim_fw(const o8 *fw, d32 n);

extern int rtl_sim_apagado;	/* 1: el chip no contesta (sin corriente) */
extern int rtl_sim_quejas;	/* lo que el modelo ha visto mal (va al registro) */
extern int rtl_sim_fw_bien;	/* el firmware ha llegado entero y en su sitio */
extern int rtl_sim_fw_corre;	/* y el 8051 lo ha arrancado */
extern d32 rtl_sim_tx, rtl_sim_rx, rtl_sim_rx_tirados;
extern d32 rtl_sim_h2c[8];	/* las órdenes al firmware (sus identificadores, por orden) */
extern int rtl_sim_nh2c;
extern int rtl_sim_lssi_mal;	/* escrituras de la radio (840h) que no han sido de 32 bits */
extern int rtl_sim_canal;	/* el canal de la radio (registro 18h de la radio) */
extern int rtl_sim_cd_forzada;	/* la controladora ha tenido que creerse que hay tarjeta */
#endif
