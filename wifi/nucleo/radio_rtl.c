// SPDX-License-Identifier: GPL-2.0
/******************************************************************************
 *
 * Copyright(c) 2007 - 2012 Realtek Corporation. All rights reserved.
 *
 ******************************************************************************/
/*
 * radio_rtl.c - La Realtek RTL8723BS (Wi-Fi 802.11b/g/n por SDIO: la de las
 * tabletas Bay Trail, el Acer One 10 S1002 y el Medion S2217) para el Wi-Fi
 * de Hispa-DOS (WIFI.BIN de Hispa-Pentium).
 *
 * SALE DEL CONTROLADOR DE LINUX: drivers/staging/rtl8723bs de Linux 6.6
 * (GPL-2.0; Copyright Realtek, el aviso de arriba, y los contribuidores de
 * Linux). No se ha copiado entero (son 58.000 líneas: la gestión 802.11 de
 * Realtek, el modo punto de acceso, el ahorro de energía, la coexistencia
 * con el Bluetooth...): de él se ha traído, con sus mismos registros,
 * valores y orden, lo mínimo para encender el chip, cargarle el firmware y
 * mandar y recibir tramas; la gestión 802.11 y WPA2 son las de siempre
 * (w80211.c y wpa.c: el cifrado CCMP se hace aquí, por software). Lo de cada
 * paso dice de qué función de Linux sale. Las tablas (MAC_REG, PHY_REG,
 * AGC_TAB y RadioA) van tal cual en wifi/linux/rtl8723bs/tablas_8723b.c.
 * Por ser de Linux, este archivo es GPL-2.0 (sin «o posterior»), como
 * WIFI.BIN entero.
 *
 * Lo que se hace, por orden (Linux: rtw_drv_init y rtl8723bs_hal_init):
 *   - SDIO (sdio.c): la función 1 con bloques de 512 (sdio_init);
 *   - encender el chip (CardEnable: la secuencia CARDDIS_TO_CARDEMU y
 *     CARDEMU_TO_ACT de hal_pwr_seq.h) y leer el EFUSE (la MAC, las
 *     potencias, el cristal, las antenas: _ReadAdapterInfo8723BS);
 *   - el firmware rtl8723bs_nic.bin (de linux-firmware: no va con el
 *     sistema; servicio.c lo busca en X:\FIRMWARE...): rtl8723b_FirmwareDownload;
 *   - el MAC, el BB y la radio con las tablas, la calibración LC, las colas,
 *     el LLT, la recepción y lo de la antena compartida con el Bluetooth
 *     (EXhalbtc8723b1ant_PowerOnSetting e InitHwConfig «solo Wi-Fi»);
 *   - y luego: el canal, recibir de la cola RX0 (sd_int_dpc,
 *     rtl8723bs_recv_tasklet) y mandar por las colas HIQ/LOQ con el
 *     descriptor de 40 octetos (rtl8723b_fill_default_txdesc).
 * Sin interrupciones: en cada vuelta del servicio se mira la de la tarjeta
 * (y, por si acaso, cada 20 ms el registro HISR del chip).
 *
 * Lo que NO se hace (Linux sí): la calibración IQ (PHY_IQCalibrate_8723B),
 * el control dinámico (DIG, el seguimiento de la potencia por temperatura),
 * las potencias por velocidad (PHY_REG_PG), 802.11n y el cifrado por
 * hardware. Las tramas de datos van a una velocidad fija (la mejor de las
 * del punto de acceso hasta 24 Mbit/s; si se pierden, el chip baja solo).
 */
#include "nucleo.h"
#include "sdio.h"
#include "rtl.h"
#include "rtl8723bs_tablas.h"

#define BIT(n) (1u << (n))

/* --------------------------------------- las direcciones (hal_com_reg.h) */
#define SDIO_LOCAL_BASE		0x10250000u
#define WLAN_IOREG_DEVICE_ID	8
#define WLAN_TX_HIQ_DEVICE_ID	4
#define WLAN_TX_MIQ_DEVICE_ID	5
#define WLAN_TX_LOQ_DEVICE_ID	6
#define WLAN_RX0FF_DEVICE_ID	7

#define SDIO_REG_TX_CTRL	0x0000
#define SDIO_REG_HIMR		0x0014
#define SDIO_REG_HISR		0x0018
#define SDIO_REG_RX0_REQ_LEN	0x001C
#define SDIO_REG_OQT_FREE_PG	0x001E
#define SDIO_REG_FREE_TXPG	0x0020
#define SDIO_REG_HSUS_CTRL	0x0086
#define SDIO_HISR_RX_REQUEST	BIT(0)
#define SDIO_HISR_AVAL		BIT(1)
#define MASK_SDIO_HISR_CLEAR	0x00FE00FCu	/* TXERR RXERR TXFOVW RXFOVW TXBCNOK TXBCNERR; C2HCMD CPWM1/2 HSISR GTINT3/4 PSTIMEOUT */

#define REG_SYS_FUNC_EN		0x0002
#define REG_APS_FSMCO		0x0004
#define REG_SYS_CLKR		0x0008
#define REG_9346CR		0x000A
#define REG_RSV_CTRL		0x001C
#define REG_RF_CTRL		0x001F
#define REG_AFE_XTAL_CTRL	0x0024
#define REG_MAC_PHY_CTRL	0x002C
#define REG_EFUSE_CTRL		0x0030
#define REG_EFUSE_TEST		0x0034
#define REG_PWR_DATA		0x0038
#define REG_GPIO_INTM		0x0048
#define REG_LEDCFG0		0x004C
#define REG_LEDCFG2		0x004E
#define REG_GPIO_IO_SEL_2	0x0062
#define REG_PAD_CTRL1		0x0064
#define REG_MULTI_FUNC_CTRL	0x0068
#define REG_MCUFWDL		0x0080
#define REG_EFUSE_ACCESS	0x00CF
#define REG_SYS_CFG		0x00F0
#define REG_CR			0x0100
#define REG_PBP			0x0104
#define REG_TRXDMA_CTRL		0x010C
#define REG_TRXFF_BNDY		0x0114
#define REG_HIMR		0x0120
#define REG_HISR		0x0124
#define REG_HMETFR		0x01CC
#define REG_HMEBOX_0		0x01D0
#define REG_HMEBOX_EXT0		0x01F0
#define REG_RQPN		0x0200
#define REG_TDECTRL		0x0208
#define REG_RQPN_NPQ		0x0214
#define REG_AUTO_LLT		0x0224
#define REG_RXDMA_AGG_PG_TH	0x0280
#define REG_RXDMA_MODE_CTRL	0x0290
#define REG_FWHW_TXQ_CTRL	0x0420
#define REG_HWSEQ_CTRL		0x0423
#define REG_TXPKTBUF_BCNQ_BDNY	0x0424
#define REG_TXPKTBUF_MGQ_BDNY	0x0425
#define REG_SPEC_SIFS		0x0428
#define REG_RL			0x042A
#define REG_RRSR		0x0440
#define REG_ARFR0		0x0444
#define REG_ARFR1		0x044C
#define REG_AMPDU_MAX_TIME	0x0456
#define REG_TXPKTBUF_WMAC_LBK	0x045D
#define REG_PKT_VO_VI_LIFE_TIME	0x04C0
#define REG_PKT_BE_BK_LIFE_TIME	0x04C2
#define REG_MAX_AGGR_NUM	0x04CA
#define REG_BAR_MODE_CTRL	0x04CC
#define REG_EARLY_MODE_CONTROL	0x04D0
#define REG_EDCA_VO_PARAM	0x0500
#define REG_EDCA_VI_PARAM	0x0504
#define REG_EDCA_BE_PARAM	0x0508
#define REG_EDCA_BK_PARAM	0x050C
#define REG_BCNTCFG		0x0510
#define REG_PIFS		0x0512
#define REG_SIFS_CTX		0x0514
#define REG_SIFS_TRX		0x0516
#define REG_SLOT		0x051B
#define REG_TXPAUSE		0x0522
#define REG_TBTT_PROHIBIT	0x0540
#define REG_BCN_CTRL		0x0550
#define REG_DUAL_TSF_RST	0x0553
#define REG_BCN_INTERVAL	0x0554
#define REG_BCNDMATIM		0x0559
#define REG_SECONDARY_CCA_CTRL	0x0577
#define REG_BWOPMODE		0x0603
#define REG_RCR			0x0608
#define REG_RX_PKT_LIMIT	0x060C
#define REG_RX_DRVINFO_SZ	0x060F
#define REG_MACID		0x0610
#define REG_BSSID		0x0618
#define REG_MAR			0x0620
#define REG_MAC_SPEC_SIFS	0x063A
#define REG_ACKTO		0x0640
#define REG_NAV_UPPER		0x0652
#define REG_TRXPTCL_CTL		0x0668
#define REG_CAMCMD		0x0670
#define REG_SECCFG		0x0680
#define REG_RXFLTMAP0		0x06A0
#define REG_RXFLTMAP1		0x06A2
#define REG_RXFLTMAP2		0x06A4
#define REG_DATA_SC		0x0483
#define MSR			(REG_CR + 2)

#define FWDL_CHKSUM_RPT		BIT(2)
#define MCUFWDL_RDY		BIT(1)
#define WINTINI_RDY		BIT(6)
#define RAM_DL_SEL		BIT(7)

#define RCR_APM			BIT(1)
#define RCR_AM			BIT(2)
#define RCR_AB			BIT(3)
#define RCR_CBSSID_DATA		BIT(6)
#define RCR_CBSSID_BCN		BIT(7)
#define RCR_AMF			BIT(13)
#define RCR_HTC_LOC_CTRL	BIT(14)
#define RCR_APP_PHYST_RXFF	BIT(28)
#define RCR_APP_ICV		BIT(29)
#define RCR_APP_MIC		BIT(30)

#define DIS_TSF_UDT		BIT(4)
#define EN_BCN_FUNCTION		BIT(3)
#define DIS_ATIM		BIT(0)

/* el BB y la radio (hal_phy_reg_8723b.h, Hal8192CPhyReg.h) */
#define rFPGA0_XA_HSSIParameter1 0x820
#define rFPGA0_XA_HSSIParameter2 0x824
#define rFPGA0_XA_LSSIParameter	0x840
#define rFPGA0_XA_RFInterfaceOE	0x860
#define rFPGA0_XAB_RFInterfaceSW 0x870
#define rFPGA0_XA_LSSIReadBack	0x8a0
#define TransceiverA_HSPI_Readback 0x8b8
#define rFPGA0_RFMOD		0x800
#define rFPGA1_RFMOD		0x900
#define rOFDM0_TxPseudoNoiseWgt	0xce4
#define rTxAGC_A_Rate18_06	0xe00
#define rTxAGC_A_Rate54_24	0xe04
#define rTxAGC_A_CCK1_Mcs32	0xe08
#define rTxAGC_A_Mcs03_Mcs00	0xe10
#define rTxAGC_A_Mcs07_Mcs04	0xe14
#define rTxAGC_B_CCK11_A_CCK2_11 0x86c
#define RF_CHNLBW		0x18
#define bRFRegOffsetMask	0xfffff
#define bMaskDWord		0xffffffff

/* el EFUSE (hal_pg.h) */
#define EFUSE_MAP_LEN		512
#define EEPROM_TX_PWR_INX	0x10
#define EEPROM_XTAL		0xB9
#define EEPROM_RF_BOARD_OPTION	0xC1
#define EEPROM_RF_BT_SETTING	0xC3
#define EEPROM_MAC_ADDR_8723BS	0x11A

/* las colas (rtl8723b_hal.h) */
#define TX_TOTAL_PAGE_NUMBER	(0xFF - 0x08)
#define TX_PAGE_BOUNDARY	(TX_TOTAL_PAGE_NUMBER + 1)
#define NORMAL_PAGE_NUM_HPQ	0x0C
#define NORMAL_PAGE_NUM_LPQ	0x02
#define NORMAL_PAGE_NUM_NPQ	0x02
#define RX_DMA_BOUNDARY		(0x4000 - 0x80 - 1)
#define TXDESC_SIZE		40
#define RXDESC_SIZE		24
#define PAGE_SIZE_TX		128
#define QSLT_BE			0x0
#define QSLT_MGNT		0x12
#define FW_START		0x1000
#define FW_MAX			0x8000

/* ------------------------------------------------------------- el estado */
static struct sdio sd;
static struct radio rtl;
static int vivo, mac_on, fw_listo, encendida;
static const o8 *fw;
static d32 fw_tam;
static p16 fw_ver, fw_sub, fw_firma;
static o8 efuse[EFUSE_MAP_LEN];
static int autoload_mal;
static o8 cristal = 0x20;
static o8 idx_cck[14], idx_bw40[14];
static signed char dif_ofdm, dif_bw20;
static o8 ant_path, ant_num1 = 1, bt_existe, paquete;
static d32 sys_cfg;
static d32 rf18;			/* RfRegChnlVal[0] */
static d32 rcr;
static o8 h2c_caja;
static o8 rx_fifo;
static d32 t_sondeo;
static int asociado, barriendo;
static o8 bssid_ap[6];
static int vel_datos = 48;		/* en medios Mbit/s (24 Mbit/s) */
static d32 rx_bien, rx_mal, tx_bien, tx_mal, tx_sin_sitio;
int rtl_fallos_es, rtl_canal;
static char nombre[48];

static void espera_us(d32 us) { sd.p->espera_us(us); }
static void espera_ms(d32 ms) { while (ms--) sd.p->espera_us(1000); }
static d32 ahora(void) { return sd.p->ms(); }

/* -------------------------------------- la E/S (sdio_ops.c: _cvrt2ftaddr) */
static d32 ftaddr(d32 a)
{
	switch (a >> 16) {
	case 0x1025: return (0u << 13) | (a & 0x0FFF);
	case 0x1031: return (WLAN_TX_HIQ_DEVICE_ID << 13) | (a & 0x1FFF);
	case 0x1032: return (WLAN_TX_MIQ_DEVICE_ID << 13) | (a & 0x1FFF);
	case 0x1033: return (WLAN_TX_LOQ_DEVICE_ID << 13) | (a & 0x1FFF);
	case 0x1034: return (WLAN_RX0FF_DEVICE_ID << 13) | (a & 0x0003);
	}
	return ((d32)WLAN_IOREG_DEVICE_ID << 13) | (a & 0x7FFF);
}

static void falla(void)
{
	rtl_fallos_es++;
	if (rtl_fallos_es == 1 || rtl_fallos_es == 10 || rtl_fallos_es == 100)
		hl_log(1, "rtl: la E/S por SDIO falla (%d veces; error %05X)\n", rtl_fallos_es, sd.ultimo_error);
}

static o8 cmd52_lee(d32 fa)
{
	o8 v = 0xFF;
	if (sdio_lee8(&sd, 1, fa, &v) < 0)
		falla();
	return v;
}
static void cmd52_pon(d32 fa, o8 v)
{
	if (sdio_pon8(&sd, 1, fa, v) < 0)
		falla();
}

/* ¿por CMD52? (sdio_read32: lo de antes de 0x100, o con el MAC apagado) */
static int por52(d32 a)
{
	return !mac_on || ((a >> 16) != 0x1025 && (a & 0x7FFF) < 0x100) || (ftaddr(a) & 3);
}

static o8 rd8(d32 a) { return cmd52_lee(ftaddr(a)); }
static void wr8(d32 a, o8 v) { cmd52_pon(ftaddr(a), v); }
static p16 rd16(d32 a) { d32 f = ftaddr(a); return cmd52_lee(f) | (cmd52_lee(f + 1) << 8); }
static void wr16(d32 a, p16 v) { d32 f = ftaddr(a); cmd52_pon(f, v); cmd52_pon(f + 1, v >> 8); }
static d32 rd32(d32 a)
{
	d32 f = ftaddr(a), v = 0;
	o8 b[4];
	int i;

	if (por52(a)) {
		for (i = 0; i < 4; i++)
			v |= (d32)cmd52_lee(f + i) << (8 * i);
		return v;
	}
	if (sdio_lee(&sd, 1, f, b, 4) < 0) {
		falla();
		return 0xFFFFFFFF;
	}
	return b[0] | (b[1] << 8) | (b[2] << 16) | ((d32)b[3] << 24);
}
static void wr32(d32 a, d32 v)
{
	d32 f = ftaddr(a);
	o8 b[4] = { v, v >> 8, v >> 16, v >> 24 };
	int i;

	if (por52(a)) {
		for (i = 0; i < 4; i++)
			cmd52_pon(f + i, b[i]);
		return;
	}
	if (sdio_pon(&sd, 1, f, b, 4) < 0)
		falla();
}
#define LOCAL(r) (SDIO_LOCAL_BASE | (r))

/* --------------------- el BB y la radio (rtl8723b_phycfg.c, sin cambios) */
static int desplaza(d32 m)
{
	int i;
	for (i = 0; i < 32 && !((m >> i) & 1); i++)
		;
	return i;
}
static d32 bb_lee(d32 r, d32 m) { return (rd32(r) & m) >> desplaza(m); }
static void bb_pon(d32 r, d32 m, d32 v)
{
	if (m != bMaskDWord)
		v = (rd32(r) & ~m) | ((v << desplaza(m)) & m);
	wr32(r, v);
}
/* phy_RFSerialRead_8723B (el camino A) */
static d32 rf_lee(d32 off, d32 m)
{
	d32 t;

	off &= 0xff;
	t = bb_lee(rFPGA0_XA_HSSIParameter2, bMaskDWord);
	t = (t & ~0x7f800000u) | (off << 23) | 0x80000000u;
	bb_pon(rFPGA0_XA_HSSIParameter2, bMaskDWord, t & ~0x80000000u);
	t = bb_lee(rFPGA0_XA_HSSIParameter2, bMaskDWord);
	bb_pon(rFPGA0_XA_HSSIParameter2, bMaskDWord, t & ~0x80000000u);
	bb_pon(rFPGA0_XA_HSSIParameter2, bMaskDWord, t | 0x80000000u);
	espera_us(10 + 2 * 50 + 10);
	if (bb_lee(rFPGA0_XA_HSSIParameter1, BIT(8)))
		t = bb_lee(TransceiverA_HSPI_Readback, 0xfffff);
	else
		t = bb_lee(rFPGA0_XA_LSSIReadBack, 0xfffff);
	return (t & m) >> desplaza(m);
}
/* phy_RFSerialWrite_8723B y PHY_SetRFReg_8723B (sin enmascarar el dato,
 * como Linux) */
static void rf_pon(d32 off, d32 m, d32 v)
{
	if (m != bRFRegOffsetMask)
		v = (rf_lee(off, bRFRegOffsetMask) & ~m) | (v << desplaza(m));
	bb_pon(rFPGA0_XA_LSSIParameter, bMaskDWord, (((off & 0xff) << 20) | (v & 0x000fffff)) & 0x0fffffff);
}

/* ------------------------------ las secuencias de encendido (hal_pwr_seq.h) */
enum { P_ESC, P_MIRA, P_ESPERA, P_FIN };
struct paso { p16 dir; o8 sdio, orden, mascara, valor; };
/* RTL8723B_TRANS_CARDDIS_TO_CARDEMU y RTL8723B_TRANS_CARDEMU_TO_ACT (lo de SDIO) */
static const struct paso encender[] = {
	{ 0x0005, 0, P_ESC, BIT(3) | BIT(7), 0 },
	{ 0x0086, 1, P_ESC, BIT(0), 0 },
	{ 0x0086, 1, P_MIRA, BIT(1), BIT(1) },
	{ 0x0005, 0, P_ESC, BIT(3) | BIT(4), 0 },
	{ 0x0023, 0, P_ESC, BIT(4), 0 },
	{ 0x0020, 0, P_ESC, BIT(0), BIT(0) },
	{ 0x0067, 0, P_ESC, BIT(4), 0 },
	{ 1, 0, P_ESPERA, 0, 0 },
	{ 0x0000, 0, P_ESC, BIT(5), 0 },
	{ 0x0005, 0, P_ESC, BIT(4) | BIT(3) | BIT(2), 0 },
	{ 0x0006, 0, P_MIRA, BIT(1), BIT(1) },
	{ 0x0006, 0, P_ESC, BIT(0), BIT(0) },
	{ 0x0005, 0, P_ESC, BIT(7), 0 },
	{ 0x0005, 0, P_ESC, BIT(4) | BIT(3), 0 },
	{ 0x0005, 0, P_ESC, BIT(0), BIT(0) },
	{ 0x0005, 0, P_MIRA, BIT(0), 0 },
	{ 0x0010, 0, P_ESC, BIT(6), BIT(6) },
	{ 0x0049, 0, P_ESC, BIT(1), BIT(1) },
	{ 0x0063, 0, P_ESC, BIT(1), BIT(1) },
	{ 0x0062, 0, P_ESC, BIT(1), 0 },
	{ 0x0058, 0, P_ESC, BIT(0), BIT(0) },
	{ 0x005A, 0, P_ESC, BIT(1), BIT(1) },
	{ 0x0069, 0, P_ESC, BIT(6), BIT(6) },
	{ 0, 0, P_FIN, 0, 0 },
};
/* RTL8723B_TRANS_ACT_TO_CARDEMU y RTL8723B_TRANS_CARDEMU_TO_CARDDIS */
static const struct paso apagar[] = {
	{ 0x001F, 0, P_ESC, 0xFF, 0 },
	{ 0x0049, 0, P_ESC, BIT(1), 0 },
	{ 0x0006, 0, P_ESC, BIT(0), BIT(0) },
	{ 0x0005, 0, P_ESC, BIT(1), BIT(1) },
	{ 0x0005, 0, P_MIRA, BIT(1), 0 },
	{ 0x0010, 0, P_ESC, BIT(6), 0 },
	{ 0x0000, 0, P_ESC, BIT(5), BIT(5) },
	{ 0x0020, 0, P_ESC, BIT(0), 0 },
	{ 0x0007, 0, P_ESC, 0xFF, 0x20 },
	{ 0x0005, 0, P_ESC, BIT(3) | BIT(4), BIT(3) },
	{ 0x0023, 0, P_ESC, BIT(4), BIT(4) },
	{ 0x0086, 1, P_ESC, BIT(0), BIT(0) },
	{ 0x0086, 1, P_MIRA, BIT(1), 0 },
	{ 0, 0, P_FIN, 0, 0 },
};

/* HalPwrSeqCmdParsing: 0 bien; -1 si una espera no acaba (dice cuál) */
static int secuencia(const struct paso *p, const char *que)
{
	o8 v;
	d32 t0;

	for (; p->orden != P_FIN; p++) {
		d32 a = p->sdio ? LOCAL(p->dir) : p->dir;
		switch (p->orden) {
		case P_ESC:
			v = rd8(a);
			wr8(a, (v & ~p->mascara) | (p->valor & p->mascara));
			break;
		case P_MIRA:
			t0 = ahora();
			while (((v = rd8(a)) & p->mascara) != (p->valor & p->mascara)) {
				if (ahora() - t0 > 60) {
					hl_log(0, "rtl: %s: el registro %s%03X no llega a %02X (vale %02X)\n", que,
					       p->sdio ? "SDIO " : "", p->dir, p->valor & p->mascara, v);
					return -1;
				}
				espera_us(10);
			}
			break;
		case P_ESPERA:
			espera_ms(p->dir);
			break;
		}
	}
	return 0;
}

/* CardEnable */
static int enciende_chip(void)
{
	if (mac_on)
		return 0;
	wr8(REG_RSV_CTRL, 0);
	if (secuencia(encender, "encender") < 0)
		return -1;
	mac_on = 1;
	return 0;
}

/* CardDisableRTL8723BSdio (sin el del firmware: lo para el 8051) */
static void apaga_chip(void)
{
	o8 v;

	if (!mac_on)
		return;
	if ((rd8(REG_MCUFWDL) & RAM_DL_SEL) && fw_listo) {
		/* rtl8723b_FirmwareSelfReset */
		int k = 100;
		wr8(REG_HMETFR + 3, 0x20);
		while ((rd8(REG_SYS_FUNC_EN + 1) & BIT(2)) && --k)
			espera_us(50);
		if (!k)
			wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) & ~BIT(2));
	}
	wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) & ~BIT(2));
	wr8(REG_MCUFWDL, 0);
	v = rd8(REG_RSV_CTRL + 1);
	wr8(REG_RSV_CTRL + 1, v & ~BIT(0));
	wr8(REG_RSV_CTRL + 1, v | BIT(0));
	mac_on = 0;
	secuencia(apagar, "apagar");
	fw_listo = 0;
}

/* _InitPowerOn_8723BS (menos hal_btcoex_PowerOnSetting, que va aparte) */
static int enciende_mac(void)
{
	if (enciende_chip() < 0)
		return -1;
	wr8(REG_GPIO_INTM + 1, rd8(REG_GPIO_INTM + 1) | BIT(1));
	wr8(REG_GPIO_IO_SEL_2 + 1, rd8(REG_GPIO_IO_SEL_2 + 1) | BIT(1));
	wr16(REG_APS_FSMCO, rd16(REG_APS_FSMCO) | BIT(4));		/* EnPDN */
	wr8(REG_CR, 0);
	wr16(REG_CR, rd16(REG_CR) | 0x063F);	/* HCI_TXDMA, HCI_RXDMA, TXDMA, RXDMA, PROTOCOL, SCHEDULE, ENSEC, CALTMR */
	return 0;
}

/* EXhalbtc8723b1ant_PowerOnSetting (SDIO; la antena por EFUSE 0xC3[6]) */
static void antena_al_encender(void)
{
	o8 local = 0;

	wr8(0x67, 0x20);
	wr16(0x2, rd16(0x2) | BIT(0) | BIT(1));
	wr8(0x765, 0x18);
	wr8(0x76e, 0x4);
	if (ant_path == 0) {
		wr32(0x948, 0x280);		/* S1 (la principal) */
	} else {
		wr32(0x948, 0x0);		/* S0 */
		local |= 1;
	}
	if (!ant_num1)
		local |= 4;
	wr8(LOCAL(0x60), local);
}

/* --------------------------------------------- el EFUSE (rtl8723b_hal_init.c) */
static o8 efuse_octeto(p16 dir)
{
	int k;

	wr16(REG_EFUSE_TEST, rd16(REG_EFUSE_TEST) & ~BIT(11));
	wr8(REG_EFUSE_CTRL + 1, dir & 0xff);
	wr8(REG_EFUSE_CTRL + 2, ((dir >> 8) & 0x03) | (rd8(REG_EFUSE_CTRL + 2) & 0xFC));
	wr8(REG_EFUSE_CTRL + 3, rd8(REG_EFUSE_CTRL + 3) & 0x7f);
	for (k = 0; k < 1000 && !(rd8(REG_EFUSE_CTRL + 3) & 0x80); k++)
		espera_ms(1);
	return k < 100 ? rd8(REG_EFUSE_CTRL) : 0xFF;
}

/* Hal_EfusePowerSwitch (para leer) */
static void efuse_corriente(int si)
{
	p16 v;

	if (!si) {
		wr8(REG_EFUSE_ACCESS, 0x00);
		return;
	}
	v = rd8(LOCAL(SDIO_REG_HSUS_CTRL));
	if (v & 1) {
		int k;
		wr8(LOCAL(SDIO_REG_HSUS_CTRL), v & ~1);
		for (k = 0; k < 100 && (rd8(LOCAL(SDIO_REG_HSUS_CTRL)) & 3) != 2; k++)
			espera_ms(10);
	}
	wr8(REG_EFUSE_ACCESS, 0x69);
	v = rd16(REG_SYS_FUNC_EN);
	if (!(v & BIT(12)))
		wr16(REG_SYS_FUNC_EN, v | BIT(12));
	v = rd16(REG_SYS_CLKR);
	if (!(v & BIT(5)) || !(v & BIT(1)))
		wr16(REG_SYS_CLKR, v | BIT(5) | BIT(1));
}

/* hal_ReadEFuse_WiFi: los paquetes del EFUSE al mapa de 512 */
static int lee_efuse(void)
{
	p16 a = 0;
	int i, usados;
	o8 h, x, off, wden;

	memset(efuse, 0xFF, sizeof efuse);
	efuse_corriente(1);
	wr32(REG_EFUSE_TEST, (rd32(REG_EFUSE_TEST) & ~0x300u));	/* el banco 0 (Wi-Fi) */
	while (a < 512) {
		h = efuse_octeto(a++);
		if (h == 0xFF)
			break;
		if ((h & 0x1F) == 0x0F) {		/* cabecera larga */
			off = (h & 0xE0) >> 5;
			x = efuse_octeto(a++);
			if ((x & 0x0F) == 0x0F)
				continue;
			off |= (x & 0xF0) >> 1;
			wden = x & 0x0F;
		} else {
			off = (h >> 4) & 0x0F;
			wden = h & 0x0F;
		}
		if (off < 64) {
			p16 d = off * 8;
			for (i = 0; i < 4; i++, d += 2)
				if (!(wden & (1 << i))) {
					efuse[d] = efuse_octeto(a++);
					efuse[d + 1] = efuse_octeto(a++);
				}
		} else {
			for (i = 0; i < 4; i++)
				if (!(wden & (1 << i)))
					a += 2;
		}
	}
	usados = a;
	paquete = efuse_octeto(0x1FB) & 7;		/* Hal_EfuseParsePackageType_8723B */
	efuse_corriente(0);
	return usados;
}

/* _ReadEfuseInfo8723BS: lo que se usa */
static void mira_efuse(void)
{
	int i, g;
	o8 b, *p = efuse + EEPROM_TX_PWR_INX;
	static const o8 grupo[14] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4 };
	o8 cck[6], bw40[5];

	if (autoload_mal) {
		static const o8 m[6] = { 0x00, 0xE0, 0x4C, 0xb7, 0x23, 0x00 };
		memcpy(rtl.mac, m, 6);
	} else {
		memcpy(rtl.mac, efuse + EEPROM_MAC_ADDR_8723BS, 6);
	}
	/* Hal_ReadPowerValueFromPROM_8723B: el camino A */
	if (autoload_mal || p[1] == 0xFF) {
		memset(cck, 0x2D, sizeof cck);
		memset(bw40, 0x2D, sizeof bw40);
		dif_bw20 = 2;
		dif_ofdm = 4;
	} else {
		for (i = 0; i < 6; i++)
			cck[i] = p[i] == 0xFF ? 0x2D : p[i];
		for (i = 0; i < 5; i++)
			bw40[i] = p[6 + i] == 0xFF ? 0x2D : p[6 + i];
		b = p[11];
		if (b == 0xFF) {
			dif_bw20 = 2;
			dif_ofdm = 4;
		} else {
			dif_bw20 = (b >> 4) & 0x0F;
			if (dif_bw20 & 8)
				dif_bw20 |= 0xF0;
			dif_ofdm = b & 0x0F;
			if (dif_ofdm & 8)
				dif_ofdm |= 0xF0;
		}
	}
	for (i = 0; i < 14; i++) {
		g = grupo[i];
		idx_cck[i] = i == 13 ? cck[5] : cck[g];
		idx_bw40[i] = bw40[g];
	}
	/* Hal_EfuseParseXtal_8723B */
	cristal = autoload_mal || efuse[EEPROM_XTAL] == 0xFF ? 0x20 : efuse[EEPROM_XTAL];
	/* Hal_EfuseParseBTCoexistInfo_8723B */
	bt_existe = !autoload_mal && (rd32(REG_MULTI_FUNC_CTRL) & BIT(18));
	b = efuse[EEPROM_RF_BT_SETTING];
	if (autoload_mal) {
		ant_num1 = 1;
		ant_path = 0;
	} else if (b != 0xFF) {
		ant_num1 = !(b & 1);
		ant_path = (b & BIT(6)) ? 1 : 0;
	} else {
		ant_num1 = 1;
		ant_path = paquete == 6 ? 1 : 0;	/* (QFN68: S0) */
	}
}

/* -------------------------------------- el firmware (rtl8723b_hal_init.c) */
static const char *const nombres_fw[] = { "rtl8723bs_nic.bin", NULL };
const char *const *rtl_nombres_fw(void) { return nombres_fw; }

int rtl_es_fw(const o8 *d, d32 n)
{
	return n > 32 && n <= FW_MAX && ((d[0] | (d[1] << 8)) & 0xFFF0) == 0x5300;
}

int rtl_pon_fw(const o8 *d, d32 n, const char *nom)
{
	(void)nom;
	if (!rtl_es_fw(d, n))
		return -1;
	fw = d;
	fw_tam = n;
	fw_firma = d[0] | (d[1] << 8);
	fw_ver = d[4] | (d[5] << 8);
	fw_sub = d[6] | (d[7] << 8);
	return 0;
}

/* _FWDownloadEnable */
static void descarga_fw(int si)
{
	o8 t;
	int k = 0;

	if (si) {
		wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) | 0x04);	/* el 8051 */
		t = rd8(REG_MCUFWDL);
		wr8(REG_MCUFWDL, t | 0x01);
		do {
			t = rd8(REG_MCUFWDL);
			if (t & 0x01)
				break;
			wr8(REG_MCUFWDL, t | 0x01);
			espera_ms(1);
		} while (k++ < 100);
		wr8(REG_MCUFWDL + 2, rd8(REG_MCUFWDL + 2) & 0xf7);	/* reiniciar el 8051 */
	} else {
		wr8(REG_MCUFWDL, rd8(REG_MCUFWDL) & 0xfe);
	}
}

/* _WriteFW: por páginas de 4 K, de 4 en 4 octetos (_BlockWrite) */
static void escribe_fw(const o8 *b, d32 n)
{
	d32 pag, i, k;

	for (pag = 0; pag * 4096 < n; pag++) {
		d32 t = n - pag * 4096 > 4096 ? 4096 : n - pag * 4096;
		const o8 *q = b + pag * 4096;
		wr8(REG_MCUFWDL + 2, (rd8(REG_MCUFWDL + 2) & 0xF8) | (pag & 7));
		for (i = 0; i + 4 <= t; i += 4)
			wr32(FW_START + i, q[i] | (q[i + 1] << 8) | (q[i + 2] << 16) | ((d32)q[i + 3] << 24));
		for (k = i; k < t; k++)
			wr8(FW_START + k, q[k]);
	}
}

/* _8051Reset8723 */
static void reinicia_8051(void)
{
	wr8(REG_RSV_CTRL + 1, rd8(REG_RSV_CTRL + 1) & ~BIT(0));
	wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) & ~BIT(2));
	wr8(REG_RSV_CTRL + 1, rd8(REG_RSV_CTRL + 1) | BIT(0));
	wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) | BIT(2));
}

/* rtl8723b_FirmwareDownload */
static int carga_fw(void)
{
	const o8 *b = fw;
	d32 n = fw_tam, v, t0;
	int vez = 0, bien = 0;
	o8 t;

	t = rd8(0xa3);
	wr8(0xa3, (t & 0xf8) | 0x02);
	t = rd8(0xa0) & 3;
	if (t != 1)
		hl_log(2, "rtl: (estado de energía 0xA0: %u)\n", t);
	b += 32;			/* la cabecera */
	n -= 32;
	if (rd8(REG_MCUFWDL) & RAM_DL_SEL) {	/* el 8051 ya corre un firmware: que se pare */
		wr8(REG_MCUFWDL, 0);
		wr8(REG_HMETFR + 3, 0x20);
		for (v = 100; (rd8(REG_SYS_FUNC_EN + 1) & BIT(2)) && --v;)
			espera_us(50);
		if (!v)
			wr8(REG_SYS_FUNC_EN + 1, rd8(REG_SYS_FUNC_EN + 1) & ~BIT(2));
	}
	descarga_fw(1);
	t0 = ahora();
	while (vez++ < 3 || ahora() - t0 < 500) {
		d32 t1;
		wr8(REG_MCUFWDL, rd8(REG_MCUFWDL) | FWDL_CHKSUM_RPT);
		escribe_fw(b, n);
		t1 = ahora();
		do {
			v = rd32(REG_MCUFWDL);
			if (v & FWDL_CHKSUM_RPT)
				break;
		} while (ahora() - t1 < 50);
		if (v & FWDL_CHKSUM_RPT) {
			bien = 1;
			break;
		}
		hl_log(1, "rtl: el firmware no ha llegado bien (MCUFWDL %08X): otra vez\n", v);
		if (vez > 6)
			break;
	}
	descarga_fw(0);
	if (!bien) {
		hl_log(0, "rtl: el chip no da por bueno el firmware (MCUFWDL %08X)\n", rd32(REG_MCUFWDL));
		return -1;
	}
	/* _FWFreeToGo */
	v = rd32(REG_MCUFWDL);
	v |= MCUFWDL_RDY;
	v &= ~WINTINI_RDY;
	wr32(REG_MCUFWDL, v);
	reinicia_8051();
	t0 = ahora();
	do {
		v = rd32(REG_MCUFWDL);
		if (v & WINTINI_RDY)
			break;
		espera_us(100);
	} while (ahora() - t0 < 200);
	if (!(v & WINTINI_RDY)) {
		hl_log(0, "rtl: el firmware no arranca (MCUFWDL %08X)\n", v);
		return -1;
	}
	hl_log(2, "rtl: firmware %u.%u (firma %04X, %u octetos) en marcha, en %u ms\n", fw_ver, fw_sub, fw_firma, fw_tam, ahora() - t0);
	/* rtl8723b_InitializeFirmwareVars */
	wr8(REG_HMETFR, 0x0f);
	h2c_caja = 0;
	fw_listo = 1;
	return 0;
}

/* FillH2CCmd8723B: una orden al firmware */
static int h2c(o8 id, const o8 *d, int n)
{
	d32 c = id, x = 0;
	int k, i;

	for (k = 0; k < 100 && (rd8(REG_HMETFR) & BIT(h2c_caja)); k++)
		espera_us(10);
	if (k == 100) {
		hl_log(1, "rtl: el firmware no recoge la orden %02X (caja %u)\n", id, h2c_caja);
		return -1;
	}
	for (i = 0; i < n && i < 3; i++)
		c |= (d32)d[i] << (8 * (i + 1));
	for (i = 3; i < n && i < 7; i++)
		x |= (d32)d[i] << (8 * (i - 3));
	if (n > 3)
		wr32(REG_HMEBOX_EXT0 + h2c_caja * 4, x);
	wr32(REG_HMEBOX_0 + h2c_caja * 4, c);
	h2c_caja = (h2c_caja + 1) % 4;
	return 0;
}

/* halbtc8723b1ant_SetAntPath(..., BTC_ANT_PATH_WIFI, ...) con el
 * conmutador de dentro (la RTL8723BS: el firmware nuevo, «0xc0000» o más) */
static void antena_wifi(int inicio)
{
	d32 ver = ((d32)fw_ver << 16) | fw_sub;
	o8 p[2];

	if (inicio) {
		rf_pon(0x1, 0xfffff, 0x780);
		if (ver >= 0x180000) {
			p[0] = 1;
			h2c(0x6E, p, 1);
		} else {
			wr8(0x765, 0x18);
		}
		wr8(0x76e, 0x4);
		wr8(0x67, rd8(0x67) | 0x20);
		wr8(0x39, rd8(0x39) | 0x8);
		wr8(0x974, 0xff);
		wr8(0x944, rd8(0x944) | 0x3);
		wr8(0x930, 0x77);
		wr32(REG_LEDCFG0, (rd32(REG_LEDCFG0) | BIT(23)) & ~BIT(24));
		wr8(REG_PAD_CTRL1, rd8(REG_PAD_CTRL1) & ~1);
		p[0] = ant_path ? 1 : 0;	/* «antenna inverse» */
		p[1] = 0;			/* conmutador de dentro */
		h2c(0x65, p, 2);
	} else {
		if (ver >= 0x180000) {
			if (rd8(0x765) != 0) {
				p[0] = 0;
				h2c(0x6E, p, 1);
			}
		} else {
			int k;
			for (k = 0; k <= 20 && (rd8(0x49d) & 1); k++)
				espera_ms(50);
			wr8(0x765, 0x0);
		}
		if (rd8(0x76e) != 0xc)
			wr8(0x76e, 0xc);
	}
	wr32(0x948, ant_path ? 0x280 : 0x0);	/* «BTC_ANT_PATH_WIFI»: S1 0, S0 280h */
}

/* halbtc8723b1ant_InitHwConfig(bWifiOnly = true) */
static void antena_solo_wifi(void)
{
	static const o8 cero[5];

	if (!bt_existe)		/* (Linux: sin Bluetooth no hay InitHwConfig; 0x948 queda en 280h) */
		return;
	wr8(0x550, rd8(0x550) | 0x8);
	wr8(0x790, 0x5);
	wr8(0x778, 0x1);
	wr8(0x40, rd8(0x40) | 0x20);
	antena_wifi(1);
	h2c(0x60, cero, 5);		/* PsTdma apagado, tipo 9 */
	antena_wifi(0);
	wr32(0x6c0, 0x55555555);	/* CoexTable 0 */
	wr32(0x6c4, 0x55555555);
	wr32(0x6c8, 0xffffff);
	wr8(0x6cc, 0x3);
}

/* -------------------------------------------------------- MAC, BB y radio */
static void tabla_bb(const unsigned *t, unsigned n)
{
	unsigned i;

	for (i = 0; i + 1 < n; i += 2) {
		d32 a = t[i], v = t[i + 1];
		/* odm_ConfigBB_PHY_8723B: las esperas */
		if (a == 0xfe)
			espera_ms(50);
		else if (a == 0xfd)
			espera_ms(5);
		else if (a == 0xfc)
			espera_ms(1);
		else if (a == 0xfb)
			espera_us(50);
		else if (a == 0xfa)
			espera_us(5);
		else if (a == 0xf9)
			espera_us(1);
		else
			bb_pon(a, bMaskDWord, v);
		espera_us(1);
	}
}

/* odm_ConfigRFReg_8723B */
static void tabla_radio(void)
{
	unsigned i;
	int k;

	for (i = 0; i + 1 < rtl_tabla_radioa_n; i += 2) {
		d32 a = rtl_tabla_radioa[i], v = rtl_tabla_radioa[i + 1];
		if (a == 0xfe || a == 0xffe) {
			espera_ms(50);
			continue;
		}
		rf_pon(a, bRFRegOffsetMask, v);
		espera_us(1);
		if (a == 0xb6)
			for (k = 0; k <= 5 && (rf_lee(a, bMaskDWord) >> 8) != (v >> 8); k++)
				rf_pon(a, bRFRegOffsetMask, v);
		if (a == 0xb2)
			for (k = 0; k <= 5 && rf_lee(a, bMaskDWord) != v; k++) {
				rf_pon(a, bRFRegOffsetMask, v);
				rf_pon(0x18, bRFRegOffsetMask, 0x0fc07);
			}
	}
}

/* PHY_BBConfig8723B */
static void config_bb(void)
{
	unsigned i;

	for (i = 0; i + 1 < rtl_tabla_mac_n; i += 2)		/* PHY_MACConfig8723B */
		wr8(rtl_tabla_mac[i], rtl_tabla_mac[i + 1]);
	wr16(REG_SYS_FUNC_EN, rd16(REG_SYS_FUNC_EN) | BIT(13) | BIT(0) | BIT(1));
	wr32(0x948, 0x280);
	wr8(REG_RF_CTRL, 0x07);			/* RF_EN, RF_RSTB, RF_SDMRSTB */
	espera_ms(1);
	rf_pon(0x1, 0xfffff, 0x780);
	wr8(REG_SYS_FUNC_EN, 0xE3);		/* PPLL, PCIEA, DIO_PCIE, BB_GLB_RSTn, BBRSTB */
	wr8(REG_AFE_XTAL_CTRL + 1, 0x80);
	tabla_bb(rtl_tabla_phy, rtl_tabla_phy_n);
	tabla_bb(rtl_tabla_agc, rtl_tabla_agc_n);
	bb_pon(REG_MAC_PHY_CTRL, 0xFFF000, (cristal & 0x3F) | ((cristal & 0x3F) << 6));
}

/* PHY_RF6052_Config8723B y phy_LCK_8723B */
static void config_radio(void)
{
	d32 u = bb_lee(rFPGA0_XAB_RFInterfaceSW, 0x10);

	bb_pon(rFPGA0_XA_RFInterfaceOE, 0x10 << 16, 1);
	espera_us(1);
	bb_pon(rFPGA0_XA_RFInterfaceOE, 0x10, 1);
	espera_us(1);
	bb_pon(rFPGA0_XA_HSSIParameter2, 0x400, 0);
	espera_us(1);
	bb_pon(rFPGA0_XA_HSSIParameter2, 0x800, 0);
	espera_us(1);
	tabla_radio();
	bb_pon(rFPGA0_XAB_RFInterfaceSW, 0x10, u);
	rf_pon(0xB0, bRFRegOffsetMask, 0xDFBE0);
	rf_pon(RF_CHNLBW, bRFRegOffsetMask, 0x8C01);
	espera_ms(200);
	rf_pon(0xB0, bRFRegOffsetMask, 0xDFFE0);
}

/* PHY_LCCalibrate_8723B (con la radio parada: el envío en pausa) */
static void calibra_lc(void)
{
	d32 lc;
	o8 t = rd8(0xd03);

	if (t & 0x70)
		wr8(0xd03, t & 0x8F);
	else
		wr8(REG_TXPAUSE, 0xFF);
	lc = rf_lee(RF_CHNLBW, 0xfff);
	rf_pon(0xB0, bRFRegOffsetMask, 0xDFBE0);
	rf_pon(RF_CHNLBW, 0xfff, lc | 0x08000);
	espera_ms(100);
	rf_pon(0xB0, bRFRegOffsetMask, 0xDFFE0);
	if (paquete >= 2 && paquete != 6)	/* (Linux: PackageType >= 2; 26 MHz, el canal 10) */
		rf_pon(RF_CHNLBW, 0xfff, lc);
	if (t & 0x70)
		wr8(0xd03, t);
	else
		wr8(REG_TXPAUSE, 0x00);
}

/* PHY_SetTxPowerLevel8723B: las potencias del canal (sin las de cada velocidad) */
static void potencias(int c)
{
	o8 k = idx_cck[c - 1], o = idx_bw40[c - 1] + dif_ofdm, n = idx_bw40[c - 1] + dif_bw20;

	if (o > 0x3F)
		o = 0x3F;
	if (n > 0x3F)
		n = 0x3F;
	bb_pon(rTxAGC_A_CCK1_Mcs32, 0x0000FF00, k);
	bb_pon(rTxAGC_B_CCK11_A_CCK2_11, 0xFFFFFF00, k | (k << 8) | (k << 16));
	wr32(rTxAGC_A_Rate18_06, o | (o << 8) | (o << 16) | ((d32)o << 24));
	wr32(rTxAGC_A_Rate54_24, o | (o << 8) | (o << 16) | ((d32)o << 24));
	wr32(rTxAGC_A_Mcs03_Mcs00, n | (n << 8) | (n << 16) | ((d32)n << 24));
	wr32(rTxAGC_A_Mcs07_Mcs04, n | (n << 8) | (n << 16) | ((d32)n << 24));
}

/* PHY_SetSwChnlBWMode8723B(canal, 20 MHz) */
static int canal(int c)
{
	if (!vivo || c < 1 || c > 14)
		return -1;
	if (c == rtl_canal)
		return 0;
	rf18 = (rf18 & 0xfffff00) | c;
	rf_pon(RF_CHNLBW, 0x3FF, rf18);
	rtl_canal = c;
	potencias(c);
	return 0;
}

static void ancho_20(void)
{
	wr16(REG_TRXPTCL_CTL, rd16(REG_TRXPTCL_CTL) & 0xFE7F);
	wr8(REG_DATA_SC, 0);
	bb_pon(rFPGA0_RFMOD, 1, 0);
	bb_pon(rFPGA1_RFMOD, 1, 0);
	bb_pon(rOFDM0_TxPseudoNoiseWgt, BIT(31) | BIT(30), 0);
	rf18 = (rf18 & 0xfffff3ff) | BIT(10) | BIT(11);		/* PHY_RF6052SetBandwidth8723B */
	rf_pon(RF_CHNLBW, bRFRegOffsetMask, rf18);
}

/* las colas, el LLT y la recepción (de rtl8723bs_hal_init) */
static int config_mac(void)
{
	d32 v, t0;
	p16 q;
	int hq = NORMAL_PAGE_NUM_HPQ, lq = NORMAL_PAGE_NUM_LPQ, nq = NORMAL_PAGE_NUM_NPQ;
	int pub = TX_TOTAL_PAGE_NUMBER - hq - lq - nq, i;

	/* _InitQueueReservedPage */
	wr8(REG_RQPN_NPQ, nq);
	wr32(REG_RQPN, hq | (lq << 8) | (pub << 16) | BIT(31));
	q = (pub + hq + 1) >> 1;
	wr16(0x218, q | (q << 8));
	q = (pub + nq + 1) >> 1;
	wr16(0x21A, q | (q << 8));
	q = (pub + lq + 1) >> 1;
	wr16(0x21C, q | (q << 8));
	/* _InitTxBufferBoundary */
	wr8(REG_TXPKTBUF_BCNQ_BDNY, TX_PAGE_BOUNDARY);
	wr8(REG_TXPKTBUF_MGQ_BDNY, TX_PAGE_BOUNDARY);
	wr8(REG_TXPKTBUF_WMAC_LBK, TX_PAGE_BOUNDARY);
	wr8(REG_TRXFF_BNDY, TX_PAGE_BOUNDARY);
	wr8(REG_TDECTRL + 1, TX_PAGE_BOUNDARY);
	/* rtl8723b_InitLLTTable */
	wr32(REG_AUTO_LLT, rd32(REG_AUTO_LLT) | BIT(16));
	t0 = ahora();
	while ((v = rd32(REG_AUTO_LLT)) & BIT(16)) {
		if (ahora() - t0 > 1000) {
			hl_log(0, "rtl: la tabla LLT no se hace (%08X)\n", v);
			return -1;
		}
		espera_ms(1);
	}
	/* _InitNormalChipThreeOutEpPriority: BE y BK baja, VI normal, VO/MGT/HI alta */
	q = (rd16(REG_TRXDMA_CTRL) & 0x7) | (1 << 8) | (1 << 10) | (2 << 6) | (3 << 4) | (3 << 12) | (3 << 14);
	wr16(REG_TRXDMA_CTRL, q);
	wr16(REG_TRXFF_BNDY + 2, RX_DMA_BOUNDARY);	/* _InitPageBoundary */
	wr8(REG_PBP, 0x11);				/* 128 octetos */
	wr8(REG_RX_DRVINFO_SZ, 4);			/* con el estado del PHY */
	for (i = 0; i < 6; i++)				/* hal_init_macaddr */
		wr8(REG_MACID + i, rtl.mac[i]);
	wr32(REG_CR, (rd32(REG_CR) & ~0x30000u) | (2u << 16));	/* _InitNetworkType */
	/* _InitWMACSetting */
	rcr = RCR_APM | RCR_AM | RCR_AB | RCR_CBSSID_DATA | RCR_CBSSID_BCN | RCR_AMF | RCR_HTC_LOC_CTRL |
	      RCR_APP_PHYST_RXFF | RCR_APP_ICV | RCR_APP_MIC;
	wr32(REG_RCR, rcr);
	wr32(REG_MAR, 0xFFFFFFFF);
	wr32(REG_MAR + 4, 0xFFFFFFFF);
	wr16(REG_RXFLTMAP2, 0xFFFF);
	wr16(REG_RXFLTMAP1, 0x400);
	wr16(REG_RXFLTMAP0, 0xFFFF);
	/* _InitAdaptiveCtrl */
	wr32(REG_RRSR, (rd32(REG_RRSR) & ~0xFFFFFu) | 0xFFFF1);
	wr16(REG_SPEC_SIFS, 0x1010);
	wr16(REG_RL, 0x3030);
	/* _InitEDCA */
	wr16(REG_SPEC_SIFS, 0x100a);
	wr16(REG_MAC_SPEC_SIFS, 0x100a);
	wr16(REG_SIFS_CTX, 0x100a);
	wr16(REG_SIFS_TRX, 0x100a);
	wr32(REG_EDCA_BE_PARAM, 0x005EA42B);
	wr32(REG_EDCA_BK_PARAM, 0x0000A44F);
	wr32(REG_EDCA_VI_PARAM, 0x005EA324);
	wr32(REG_EDCA_VO_PARAM, 0x002FA226);
	/* _InitRetryFunction */
	wr8(REG_FWHW_TXQ_CTRL, rd8(REG_FWHW_TXQ_CTRL) | BIT(7));
	wr8(REG_ACKTO, 0x40);
	/* _initSdioAggregationSetting */
	wr8(REG_RXDMA_AGG_PG_TH + 1, 0x06);
	wr8(REG_RXDMA_AGG_PG_TH, 0x06);
	wr8(REG_TRXDMA_CTRL, rd8(REG_TRXDMA_CTRL) | BIT(2));
	wr8(REG_RXDMA_MODE_CTRL, BIT(1) | (3 << 2));
	wr8(REG_BWOPMODE, BIT(2));			/* _InitOperationMode: 20 MHz */
	/* rtl8723b_InitBeaconParameters */
	wr16(REG_BCN_CTRL, DIS_TSF_UDT | (DIS_TSF_UDT << 8) | EN_BCN_FUNCTION);
	wr16(REG_TBTT_PROHIBIT, 0x6404);
	wr8(REG_BCNDMATIM, 0x02);
	wr16(REG_BCNTCFG, 0x660F);
	/* _InitInterrupt */
	wr32(REG_HISR, 0);
	wr32(REG_HIMR, 0);
	/* _InitBurstPktLen_8723BS */
	wr8(0x4c7, rd8(0x4c7) | BIT(7));
	wr8(REG_RX_PKT_LIMIT, 0x18);
	wr8(REG_MAX_AGGR_NUM, 0x1F);
	wr8(REG_PIFS, 0x00);
	wr8(REG_FWHW_TXQ_CTRL, rd8(REG_FWHW_TXQ_CTRL) & ~BIT(7));
	wr8(REG_AMPDU_MAX_TIME, 0x70);
	wr32(REG_ARFR0, 0x00000010);
	wr32(REG_ARFR0 + 4, 0xfffff000);
	wr32(REG_ARFR1, 0x00000010);
	wr32(REG_ARFR1 + 4, 0x003ff000);
	wr8(REG_SECONDARY_CCA_CTRL, 0x3);
	wr8(0x976, 0);
	wr16(REG_PKT_VO_VI_LIFE_TIME, 0x0400);
	wr16(REG_PKT_BE_BK_LIFE_TIME, 0x0400);
	wr32(REG_CAMCMD, BIT(31) | BIT(30));		/* invalidate_cam_all */
	wr8(REG_SECCFG, 0);				/* sin cifrado por hardware: CCMP lo hace wpa.c */
	return 0;
}

/* hw_var_set_opmode(_HW_STATE_STATION_) */
static void modo_estacion(void)
{
	wr8(REG_BCN_CTRL, rd8(REG_BCN_CTRL) | DIS_TSF_UDT);
	wr8(MSR, (rd8(MSR) & 0x0c) | 0x02);
	wr8(REG_TXPAUSE, rd8(REG_TXPAUSE) | BIT(6));	/* StopTxBeacon */
	wr8(REG_BCN_CTRL, DIS_TSF_UDT | EN_BCN_FUNCTION | DIS_ATIM);
}

/* ------------------------------------------------------------- recibir */
static void entrega(o8 *p, int n)
{
	int i = 0, vueltas = 0;

	while (i + RXDESC_SIZE <= n && vueltas++ < 64) {
		d32 w0 = p[i] | (p[i + 1] << 8) | (p[i + 2] << 16) | ((d32)p[i + 3] << 24);
		d32 w2 = p[i + 8] | (p[i + 9] << 8) | (p[i + 10] << 16) | ((d32)p[i + 11] << 24);
		d32 w3 = p[i + 12] | (p[i + 13] << 8);
		int largo = w0 & 0x3FFF, info = ((w0 >> 16) & 0xF) * 8, mueve = (w0 >> 24) & 3;
		int fisico = (w0 >> 26) & 1, c2h = (w2 >> 28) & 1, vel = w3 & 0x7F;
		int ini = i + RXDESC_SIZE + info + mueve, senal = -60;

		if (largo == 0 || ini + largo > n)
			break;
		if (w0 & (BIT(14) | BIT(15))) {		/* CRC o ICV mal */
			rx_mal++;
		} else if (!c2h) {
			if (fisico && info >= 8) {
				o8 *f = p + i + RXDESC_SIZE;
				if (vel <= 3) {		/* CCK: odm_cck_rssi */
					o8 lna = (f[5] & 0xE0) >> 5, vga = f[5] & 0x1F;
					senal = lna == 6 ? -34 - 2 * vga : lna == 4 ? -14 - 2 * vga : lna == 1 ? 6 - 2 * vga :
						lna == 0 ? 16 - 2 * vga : -60;
				} else {
					senal = ((f[4] >> 1) & 0x7f) - 110;
				}
			}
			rx_bien++;
			w80211_rx(p + ini, largo, senal, rtl_canal);
		}
		i = (ini + largo + 7) & ~7;
	}
}

static o8 rxbuf[16384 + 512] __attribute__((aligned(4)));

static void sondear(void)
{
	o8 h = 0;
	int k;

	if (!vivo || !encendida)
		return;
	if (!sdio_avisa(&sd) && ahora() - t_sondeo < 20)
		return;
	t_sondeo = ahora();
	h = cmd52_lee(ftaddr(LOCAL(SDIO_REG_HISR)));
	if (h & MASK_SDIO_HISR_CLEAR & 0xFF)
		cmd52_pon(ftaddr(LOCAL(SDIO_REG_HISR)), h & MASK_SDIO_HISR_CLEAR & 0xFF);
	for (k = 0; k < 16; k++) {
		d32 f = ftaddr(LOCAL(SDIO_REG_RX0_REQ_LEN));
		int n = cmd52_lee(f) | (cmd52_lee(f + 1) << 8), l;
		if (n == 0 || n == 0xFFFF)
			break;
		if (n > 16384) {
			hl_log(1, "rtl: la cola de recepción dice %d octetos: se tira\n", n);
			n = 16384;
		}
		l = (n + 3) & ~3;
		if (l > 512)
			l = (l + 511) & ~511;
		if (sdio_lee(&sd, 1, (WLAN_RX0FF_DEVICE_ID << 13) | (rx_fifo++ & 3), rxbuf, l) < 0) {
			falla();
			break;
		}
		entrega(rxbuf, n);
	}
}

/* --------------------------------------------------------------- mandar */
/* la velocidad del descriptor (DESC_RATE...) en medios Mbit/s */
static int vel_hw(int v)
{
	switch (v) {
	case 2: return 0;
	case 4: return 1;
	case 11: return 2;
	case 22: return 3;
	case 12: return 4;
	case 18: return 5;
	case 24: return 6;
	case 36: return 7;
	case 48: return 8;
	case 72: return 9;
	case 96: return 10;
	case 108: return 11;
	}
	return 0;
}

static o8 txbuf[TXDESC_SIZE + 2400 + 512] __attribute__((aligned(4)));

static void pon_le32(o8 *p, d32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

static int enviar(const o8 *t, int n, int vel)
{
	int datos = (t[0] & 0x0C) == 0x08, mc = t[4] & 1, qos = datos && (t[0] & 0x80);
	int dev = datos ? WLAN_TX_LOQ_DEVICE_ID : WLAN_TX_HIQ_DEVICE_ID, cola = datos ? 2 : 0;
	int paginas = (TXDESC_SIZE + n + PAGE_SIZE_TX - 1) / PAGE_SIZE_TX, l, k, hw;
	o8 libres[4];
	d32 w, sum = 0;

	if (!vivo || !encendida || n < 10 || n > 2400)
		return -1;
	if (!vel)
		vel = vel_datos;
	hw = vel_hw(vel);
	/* sitio en la cola (rtw_hal_sdio_query_tx_freepage) */
	for (k = 0; k < 20; k++) {
		if (sdio_lee(&sd, 1, ftaddr(LOCAL(SDIO_REG_FREE_TXPG)), libres, 4) < 0) {
			falla();
			return -1;
		}
		if (libres[cola] + libres[3] > paginas + 1 && cmd52_lee(ftaddr(LOCAL(SDIO_REG_OQT_FREE_PG))) >= 1)
			break;
		espera_us(500);
	}
	if (k == 20) {
		tx_sin_sitio++;
		return -1;
	}
	/* el descriptor (rtl8723b_fill_default_txdesc) */
	memset(txbuf, 0, TXDESC_SIZE);
	pon_le32(txbuf, n | (TXDESC_SIZE << 16) | (mc ? BIT(24) : 0));
	pon_le32(txbuf + 4, ((datos ? QSLT_BE : QSLT_MGNT) << 8) | ((hw <= 3 ? 8 : datos ? 6 : 7) << 16));
	pon_le32(txbuf + 8, datos ? BIT(16) : 0);		/* BK: sin agregar */
	pon_le32(txbuf + 12, BIT(8));				/* USERATE */
	w = hw | (datos ? 0x1F << 8 : 0) | BIT(17) | ((datos ? 0x30 : 12) << 18);	/* tope de la vuelta atrás, reintentos */
	pon_le32(txbuf + 16, w);
	pon_le32(txbuf + 32, qos ? 0 : BIT(15));		/* EN_HWSEQ */
	/* rtl8723b_cal_txdesc_chksum (con «|=», como Linux) */
	for (k = 0; k < 16; k++)
		sum |= txbuf[2 * k] | (txbuf[2 * k + 1] << 8);
	txbuf[28] = sum;
	txbuf[29] = sum >> 8;
	memcpy(txbuf + TXDESC_SIZE, t, n);
	l = (TXDESC_SIZE + n + 3) & ~3;
	k = l;
	if (k > 512) {
		k = (k + 511) & ~511;
		memset(txbuf + l, 0, k - l);
	}
	/* sdio_write_port: la cola y el largo, en la dirección */
	if (sdio_pon(&sd, 1, ((d32)dev << 13) | ((l >> 2) & 0x1FFF), txbuf, k) < 0) {
		falla();
		tx_mal++;
		return -1;
	}
	tx_bien++;
	return 0;
}

/* ------------------------------------------------- el punto de acceso */
static void barrido(int empieza)
{
	if (!vivo || !encendida)
		return;
	barriendo = empieza;
	/* hw_var_set_mlme_sitesurvey */
	if (empieza) {
		wr32(REG_RCR, rcr & ~RCR_CBSSID_BCN);
		wr16(REG_RXFLTMAP2, asociado ? 0xFFFF : 0);
		wr8(REG_BCN_CTRL, rd8(REG_BCN_CTRL) | DIS_TSF_UDT);
	} else {
		if (asociado) {
			wr16(REG_RXFLTMAP2, 0xFFFF);
			wr8(REG_BCN_CTRL, rd8(REG_BCN_CTRL) & ~DIS_TSF_UDT);
			wr32(REG_RCR, rcr);
		}
	}
}

static void bss(const o8 *b, int as, int intervalo, int corta, const o8 *v, int nv)
{
	int i, mejor = 0;

	if (!vivo || !encendida)
		return;
	if (!b) {
		/* hw_var_set_mlme_disconnect */
		asociado = 0;
		memset(bssid_ap, 0, 6);
		for (i = 0; i < 6; i++)
			wr8(REG_BSSID + i, 0);
		wr16(REG_RXFLTMAP2, 0);
		wr8(REG_DUAL_TSF_RST, BIT(0));
		wr8(REG_BCN_CTRL, rd8(REG_BCN_CTRL) | DIS_TSF_UDT);
		wr32(REG_RCR, rcr & ~RCR_CBSSID_BCN);	/* que se oigan las balizas de todos */
		return;
	}
	memcpy(bssid_ap, b, 6);
	for (i = 0; i < 6; i++)			/* hw_var_set_bssid */
		wr8(REG_BSSID + i, b[i]);
	/* la velocidad de los datos: la mejor del punto de acceso hasta 24 Mbit/s */
	for (i = 0; i < nv; i++) {
		int x = v[i] & 0x7f;
		if (x <= 48 && x > mejor && x != 22 && x != 11)
			mejor = x;
	}
	if (!mejor)
		for (i = 0; i < nv; i++)
			if ((v[i] & 0x7f) > mejor && (v[i] & 0x7f) <= 22)
				mejor = v[i] & 0x7f;
	vel_datos = mejor ? mejor : 2;
	if (!as) {
		/* hw_var_set_mlme_join(0): datos y el filtro del BSSID */
		wr16(REG_RXFLTMAP2, 0xFFFF);
		wr32(REG_RCR, rcr);
		wr16(REG_RL, (48 << 8) | 48);
	} else {
		/* hw_var_set_mlme_join(2), el intervalo y la ranura */
		asociado = 1;
		wr8(REG_BCN_CTRL, rd8(REG_BCN_CTRL) & ~DIS_TSF_UDT);
		if (intervalo)
			wr16(REG_BCN_INTERVAL, intervalo);
		wr8(REG_SLOT, corta ? 9 : 20);
	}
}

static void parar(void)
{
	if (!vivo)
		return;
	encendida = 0;
	apaga_chip();
	vivo = 0;
}

/* ------------------------------------------------------------- el arranque */
int rtl_arranca(const struct sdio_plat *p, struct radio **r)
{
	int e, u;
	o8 v;

	vivo = mac_on = fw_listo = encendida = 0;
	rtl_fallos_es = 0;
	rtl_canal = 0;
	e = sdio_arranca(&sd, p);
	if (e < 0)
		return e;
	if (sd.manf != 0x024C)
		hl_log(1, "rtl: la tarjeta SDIO no es de Realtek (fabricante %04X, tarjeta %04X): se prueba igual\n", sd.manf, sd.carta);
	else if (sd.carta != 0xB723 && sd.carta != 0x0523 && sd.carta != 0x0623 && sd.carta != 0x0525 &&
		 sd.carta != 0x0626 && sd.carta != 0x0627)
		hl_log(1, "rtl: Realtek %04X: no es de las 8723BS que conoce Linux: se prueba igual\n", sd.carta);
	vivo = 1;
	/* _ReadAdapterInfo8723BS: encender, leer el EFUSE y apagar */
	if (enciende_mac() < 0) {
		vivo = 0;
		return -10;
	}
	sys_cfg = rd32(REG_SYS_CFG);
	wr8(0x4e, rd8(0x4e) | BIT(6));
	wr32(REG_EFUSE_TEST, (rd32(REG_EFUSE_TEST) & ~0x300u));	/* _EfuseCellSel */
	v = rd8(REG_9346CR);
	autoload_mal = !(v & BIT(5));
	u = lee_efuse();
	mira_efuse();
	hl_log(2, "rtl: chip RTL8723B%s, corte %c (SYS_CFG %08X); EFUSE %s (%d octetos usados), encapsulado %u\n",
	       (sys_cfg & BIT(23)) ? " de pruebas" : "", 'A' + ((sys_cfg >> 12) & 0xF), sys_cfg,
	       autoload_mal ? "SIN CARGAR (MAC de fábrica de Realtek)" : "bien", u, paquete);
	hl_log(2, "rtl: MAC %02X:%02X:%02X:%02X:%02X:%02X; cristal %02X; antena %s (%s), Bluetooth %s; potencias CCK %02X, OFDM %02X%+d\n",
	       rtl.mac[0], rtl.mac[1], rtl.mac[2], rtl.mac[3], rtl.mac[4], rtl.mac[5], cristal,
	       ant_path ? "S0 (la auxiliar)" : "S1 (la principal)", ant_num1 ? "una, compartida" : "dos",
	       bt_existe ? "en el chip" : "no", idx_cck[0], idx_bw40[0], dif_ofdm);
	wr8(0x67, 0x00);		/* la antena, al Bluetooth (como Linux, hasta encender) */
	apaga_chip();
	memcpy(nombre, "Realtek RTL8723BS (SDIO)", 25);
	rtl.nombre = nombre;
	rtl.canal = canal;
	rtl.enviar = enviar;
	rtl.sondear = sondear;
	rtl.barrido = barrido;
	rtl.bss = bss;
	rtl.parar = parar;
	rtl.barre_sola = 0;
	rtl.clave = NULL;
	*r = &rtl;
	return 0;
}

int rtl_enciende(void)
{
	d32 t0 = ahora();
	o8 libres[4], cero4[4] = { 0, 0, 0, 0 };

	if (!vivo)
		return -1;
	if (!fw) {
		hl_log(0, "rtl: falta el firmware\n");
		return -2;
	}
	/* rtl8723bs_hal_init */
	if (enciende_mac() < 0)
		return -3;
	antena_al_encender();
	wr16(REG_PWR_DATA, rd16(REG_PWR_DATA) | BIT(11));
	wr32(REG_LEDCFG0, rd32(REG_LEDCFG0) | BIT(23));
	wr8(REG_PAD_CTRL1, rd8(REG_PAD_CTRL1) & ~BIT(0));
	wr8(REG_EARLY_MODE_CONTROL, 0);
	if (carga_fw() < 0)
		return -4;
	config_bb();
	config_radio();
	rf18 = rf_lee(RF_CHNLBW, bRFRegOffsetMask);
	if (config_mac() < 0)
		return -5;
	rtl_canal = 0;
	ancho_20();
	canal(1);
	wr8(REG_LEDCFG2, rd8(REG_LEDCFG2) | BIT(7));		/* rtl8723b_InitAntenna_Selection */
	wr32(REG_BAR_MODE_CTRL, 0x0201ffff);
	wr8(REG_HWSEQ_CTRL, 0xFF);
	if (sdio_pon(&sd, 1, ftaddr(LOCAL(SDIO_REG_TX_CTRL)), cero4, 4) < 0)
		falla();
	if (sdio_lee(&sd, 1, ftaddr(LOCAL(SDIO_REG_FREE_TXPG)), libres, 4) < 0)
		falla();
	wr8(REG_CR, rd8(REG_CR) | BIT(6) | BIT(7));		/* MACTXEN, MACRXEN */
	wr8(REG_NAV_UPPER, (30000 + 127) / 128);
	wr32(REG_FWHW_TXQ_CTRL, rd32(REG_FWHW_TXQ_CTRL) | BIT(12));
	calibra_lc();
	antena_solo_wifi();
	modo_estacion();
	/* sin enlace: que se oigan todas las balizas; datos no */
	wr16(REG_RXFLTMAP2, 0);
	wr32(REG_RCR, rcr & ~RCR_CBSSID_BCN);
	/* EnableInterrupt8723BSdio: la interrupción de la tarjeta, con lo recibido */
	{
		o8 m[4] = { SDIO_HISR_RX_REQUEST | SDIO_HISR_AVAL, 0, 0, 0 };
		if (sdio_pon(&sd, 1, ftaddr(LOCAL(SDIO_REG_HIMR)), m, 4) < 0)
			falla();
		wr8(0x01AF, 0xFF);		/* REG_C2HEVT_CLEAR = C2H_EVT_HOST_CLOSE */
	}
	encendida = 1;
	t_sondeo = ahora();
	hl_log(2, "rtl: en marcha en %u ms: canal 1, 20 MHz; colas libres %u/%u/%u/%u; RF 18h %05X; errores de E/S %d\n",
	       ahora() - t0, libres[0], libres[1], libres[2], libres[3], rf18, rtl_fallos_es);
	return rtl_fallos_es > 50 ? -6 : 0;
}

void rtl_cuentas(d32 *bien, d32 *mal, int *vel)
{
	*bien = rx_bien;
	*mal = rx_mal;
	*vel = vel_datos;
}

void rtl_resumen(void)
{
	hl_log(2, "rtl: recibidas %u (malas %u); mandadas %u (fallos %u, sin sitio %u); E/S: %u CMD52, %u CMD53, %u errores; canal %d\n",
	       rx_bien, rx_mal, tx_bien, tx_mal, tx_sin_sitio, sd.ordenes52, sd.ordenes53, sd.errores, rtl_canal);
}
