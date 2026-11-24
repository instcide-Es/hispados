/* tarjeta_sim.h - El modelo de la BCM43224 (y de la BCM4313) (tests/wifi/tarjeta_sim.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef TARJETA_SIM_H
#define TARJETA_SIM_H
void tarjeta_sim_monta(struct radio *punto_de_acceso);
extern u8 tsim_cfg[256];
extern int tsim_4313;		/* (BCM4313) 1: el modelo es de una BCM4313 (antes de tarjeta_sim_monta) */
extern u32 tsim_lcn_canales;	/* (BCM4313) los canales que ha programado el PHY LCN (bit c) */
extern u32 tsim_lcn_cal[4];	/* (BCM4313) esperas de calibración: TX IQ/LO, IQ, RCAL, temperatura */
extern int tsim_canal;		/* el canal en que está la radio (lo dice quien la lleva; en la BCM4313, el
				 * que programa el PHY LCN: lo pone el modelo) */
extern u32 tsim_tx, tsim_rx, tsim_tx_malas, tsim_accesos;
#endif
