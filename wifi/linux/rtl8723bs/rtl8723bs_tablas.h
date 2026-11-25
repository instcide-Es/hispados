/* SPDX-License-Identifier: GPL-2.0 */
/* rtl8723bs_tablas.h - Las tablas de tablas_8723b.c (de Linux 6.6,
 * drivers/staging/rtl8723bs/hal/HalHWImg8723B_*.c; Copyright(c) 2007 - 2011
 * Realtek Corporation). */
#ifndef RTL8723BS_TABLAS_H
#define RTL8723BS_TABLAS_H
extern const unsigned int rtl_tabla_mac[], rtl_tabla_mac_n;		/* (registro, octeto) */
extern const unsigned int rtl_tabla_phy[], rtl_tabla_phy_n;		/* (registro, palabra) */
extern const unsigned int rtl_tabla_agc[], rtl_tabla_agc_n;		/* (registro, palabra) */
extern const unsigned int rtl_tabla_radioa[], rtl_tabla_radioa_n;	/* (registro de la radio, valor) */
extern const unsigned int rtl_tabla_phy_pg[], rtl_tabla_phy_pg_n;	/* (banda, registro, máscara, valor) */
#endif
