/* tarjeta_sim.h - El modelo de la BCM43224 (tests/wifi/tarjeta_sim.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef TARJETA_SIM_H
#define TARJETA_SIM_H
void tarjeta_sim_monta(struct radio *punto_de_acceso);
extern u8 tsim_cfg[256];
extern int tsim_canal;		/* el canal en que está la radio (lo dice quien la lleva) */
extern u32 tsim_tx, tsim_rx, tsim_tx_malas, tsim_accesos;
#endif
