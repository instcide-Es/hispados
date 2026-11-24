/*
 * iwl_fw.h - El firmware de las Wi-Fi de Intel (los archivos «.ucode» de
 * linux-firmware: iwlwifi-7265D-29.ucode, iwlwifi-9000-pu-b0-jf-b0-46.ucode,
 * iwlwifi-QuZ-a0-hr-b0-NN.ucode...) leído para el Wi-Fi de Hispa-DOS: su
 * cabecera, sus TLV, las secciones de cada imagen (la de arranque, INIT, y
 * la de trabajo, RT) y lo que dice de sí mismo (capacidades, calibración).
 *
 * El formato es el que documenta fw/file.h de iwlwifi (Linux, doble licencia
 * GPL-2.0/BSD): los números de los tipos de TLV y el orden de los campos se
 * toman de ahí (son el formato del archivo, no código); lo demás está escrito
 * aquí de nuevo. El firmware NO es libre (licencia de redistribución de
 * Intel): no va en el repositorio ni en las imágenes; el sistema lo busca en
 * \HISPA\FIRMWARE\ (docs/PENTIUM.md, «El Wi-Fi de Intel»).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef IWL_FW_H
#define IWL_FW_H

#include "nucleo.h"

#define IWL_MAGIA		0x0a4c5749	/* «IWL\n» */

/* los tipos de TLV (fw/file.h, enum iwl_ucode_tlv_type) que se miran */
#define IWL_TLV_FLAGS		18
#define IWL_TLV_SEC_RT		19
#define IWL_TLV_SEC_INIT	20
#define IWL_TLV_SEC_WOWLAN	21
#define IWL_TLV_DEF_CALIB	22
#define IWL_TLV_PHY_SKU		23
#define IWL_TLV_SECURE_SEC_RT	24
#define IWL_TLV_SECURE_SEC_INIT	25
#define IWL_TLV_SECURE_SEC_WOWLAN 26
#define IWL_TLV_NUM_OF_CPU	27
#define IWL_TLV_CSCHEME		28
#define IWL_TLV_API_CHANGES_SET	29
#define IWL_TLV_ENABLED_CAPABILITIES 30
#define IWL_TLV_N_SCAN_CHANNELS	31
#define IWL_TLV_PAGING		32
#define IWL_TLV_FW_VERSION	36
#define IWL_TLV_CMD_VERSIONS	48
#define IWL_TLV_IML		52

/* las «secciones» que en realidad separan (su dirección de destino) */
#define IWL_SEP_CPU1_CPU2	0xFFFFCCCC
#define IWL_SEP_PAGINACION	0xAAAABBBB

/* los bits de capacidad que mira el controlador (fw/file.h, enum iwl_ucode_tlv_capa) */
#define IWL_CAPA_LAR		1
#define IWL_CAPA_UMAC_SCAN	2
#define IWL_CAPA_DQA		12
#define IWL_CAPA_LAR_MULTI_MCC	29
/* y de API (enum iwl_ucode_tlv_api) */
#define IWL_API_WIFI_MCC_UPDATE	9
#define IWL_API_NEW_VERSION	20
#define IWL_API_TKIP_MIC_KEYS	29	/* ADD_STA_KEY de 76 octetos (la v2) */
#define IWL_API_ADAPTIVE_DWELL	32
#define IWL_API_ADAPTIVE_DWELL_V2 42
#define IWL_API_SCAN_EXT_CHAN_VER 58

/* qué imagen */
#define IWL_IMG_RT		0
#define IWL_IMG_INIT		1
#define IWL_IMG_WOWLAN		2

#define IWL_MAX_SEC		32
struct iwl_sec {
	d32 destino;			/* dónde va en la SRAM de la tarjeta (o un separador) */
	d32 tam;
	const o8 *datos;		/* dentro del archivo, tal cual */
};
struct iwl_imagen {
	struct iwl_sec sec[IWL_MAX_SEC];
	int n;
	int segura;			/* venía en SECURE_SEC_* (la 8000 en adelante) */
	d32 flujo, evento;		/* DEF_CALIB: los disparadores de calibración */
	int hay_calib;
};

/* una orden y sus versiones (TLV 48: cmd, grupo, versión, versión de la notificación) */
struct iwl_ver_orden { o8 orden, grupo, ver, ver_notif; };

#define IWL_MAX_VER_ORDEN	128
struct iwl_fw {
	char texto[65];			/* «Intel(R) ... 29.9ef079ed.0» */
	d32 ver, build;
	d32 mayor, menor, local;	/* TLV 36, si lo trae */
	struct iwl_imagen img[3];
	d32 phy_sku;
	int hay_phy_sku;
	d32 api[4];			/* API_CHANGES_SET: los bits, de 32 en 32 */
	d32 capa[4];			/* ENABLED_CAPABILITIES */
	d32 flags;			/* TLV 18 */
	d32 n_cpu;
	d32 n_canales_escaneo;
	d32 paginacion;			/* TLV 32: el tamaño de la memoria paginada */
	const o8 *iml; d32 iml_tam;	/* TLV 52 (las 22000/AX en adelante) */
	struct iwl_ver_orden vo[IWL_MAX_VER_ORDEN];
	int n_vo;
	int n_tlv;
	int tlv_sin_saber;		/* tipos que no se miran (no es un error) */
};

/* lee el archivo (que se queda en la memoria: las secciones apuntan dentro).
 * 0 si vale; si no, -1 y el porqué en «error» */
int iwl_fw_lee(const o8 *d, d32 n, struct iwl_fw *fw, char *error, int cabe);
/* ¿tiene el bit «b» de capacidad (ENABLED_CAPABILITIES) o de API? */
int iwl_fw_capa(const struct iwl_fw *fw, int b);
int iwl_fw_api(const struct iwl_fw *fw, int b);
/* la versión de una orden (TLV 48), o «si_no» si no lo dice */
int iwl_fw_ver_orden(const struct iwl_fw *fw, int grupo, int orden, int si_no);
/* cuántos octetos hay que cargar de una imagen (sin separadores) */
d32 iwl_fw_octetos(const struct iwl_imagen *im);

#endif
