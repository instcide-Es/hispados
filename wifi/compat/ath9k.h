/*
 * ath9k.h - En Linux, la cabecera del controlador ath9k para mac80211 (la
 * parte que aquí hace wifi/nucleo/radio_ath9k.c). De los fuentes de la capa
 * de la tarjeta (wifi/linux/ath/ath9k/), solo hw.c la incluye, y de ella usa
 * solo lo de aquí (y radio_ath9k.c, los plazos de calibración).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef HISPA_ATH9K_H
#define HISPA_ATH9K_H
/* (hw.c ya ha incluido hw.h) */
#define IS_HT_RATE(rate)   ((rate) & 0x80)
#define IS_CCK_RATE(rate)  (((rate) >= 0x18) && ((rate) <= 0x1e))
#define IS_OFDM_RATE(rate) (((rate) >= 0x8) && ((rate) <= 0xf))
enum { WLAN_RC_PHY_OFDM, WLAN_RC_PHY_CCK };
/* los plazos de la calibración periódica (los usa radio_ath9k.c) */
#define ATH_STA_SHORT_CALINTERVAL 1000		/* 1 segundo */
#define ATH_LONG_CALINTERVAL 30000		/* 30 segundos */
#define ATH_RESTART_CALINTERVAL 1200000		/* 20 minutos */
#endif
