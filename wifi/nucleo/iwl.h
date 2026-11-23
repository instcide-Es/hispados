/*
 * iwl.h - Las Wi-Fi de Intel para el Wi-Fi de Hispa-DOS (WIFI.BIN de
 * Hispa-Pentium): lo que el servicio (castel/nucleo32/wifi/servicio.c) ve de
 * wifi/nucleo/radio_iwl.c. Un controlador nuevo, escrito aquí, que hace lo
 * que hace iwlwifi de Linux (el transporte PCIe «gen1» y la capa MVM) para
 * la familia 7000 (Wireless-AC 7265, 7265D y 3165, la del ASUS GL553VD) con
 * su firmware de Intel, que no va con el sistema: se busca en
 * \HISPA\FIRMWARE\ (docs/PENTIUM.md, «El Wi-Fi de Intel»).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef IWL_H
#define IWL_H

#include "nucleo.h"

/* ¿es una Wi-Fi de Intel? 0: de las que se llevan (familia 7000); 1: de
 * Intel, pero aún no (9000 CNVi, 22000/AX: se dice lo que es y se para);
 * -1: no es de Intel. «modelo»: su nombre (puede ser NULL) */
int iwl_reconoce(d32 id_pci, const char **modelo);

/* lo de cada tarjeta, como las demás de servicio.c */
int iwl_busca(void);				/* 0 hay; 1 no hay; <0 hay, pero no se puede */
const char *const *iwl_nombres_fw(void);	/* los archivos que valen, por orden; NULL al final */
int iwl_pon_fw(const o8 *datos, d32 tam, const char *nombre);	/* 0 si vale */
int iwl_monta(struct radio **r);
int iwl_enciende(void);
void iwl_cuentas(d32 *bien, d32 *mal, int *vel);
void iwl_resumen(void);

/* ------------------------------------------------ lo que pide la tarjeta
 * Quien lleva radio_iwl.c le da esto (en WIFI.BIN, radio_iwl.c mismo con
 * base.c; en el anfitrión, el modelo de tests/wifi/iwl_modelo.c) */
struct iwl_plat {
	d32 (*lee)(d32 reg);			/* un registro de 32 bits del BAR0 */
	void (*pon)(d32 reg, d32 v);
	void *(*dma)(d32 tam, d32 *fis);	/* memoria para la tarjeta, a cero, alineada a 4 K */
	void (*espera_us)(d32 us);
	d32 (*ms)(void);			/* milisegundos desde el arranque */
	d32 (*pci_lee)(int reg);
	void (*pci_pon)(int reg, d32 v);
};
/* el modelo de las pruebas pone aquí la suya antes de iwl_busca */
extern const struct iwl_plat *iwl_plat_prueba;

#endif
