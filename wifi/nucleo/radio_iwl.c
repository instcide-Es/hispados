/*
 * radio_iwl.c - Las Wi-Fi de Intel de la familia 7000 (Wireless-AC 7265,
 * 7265D, 3165 y 3168: la del ASUS GL553VD) para el Wi-Fi de Hispa-DOS, con
 * el firmware de Intel (iwlwifi-7265D-29.ucode), que hace él mismo casi todo:
 * aquí está lo que hace iwlwifi de Linux por encima de él, escrito de nuevo
 * (iwlwifi es GPL-2.0/BSD; de allí solo se toman los hechos: los números de
 * los registros, los bits, la forma de las órdenes y su orden; lo de cada
 * paso dice de qué función de Linux 6.6 sale):
 *
 *   - el transporte PCIe «gen1» (pcie/trans.c, rx.c, tx.c): despertar la
 *     tarjeta (prepare_card_hw, apm_init), la cola de recepción (256 RBD,
 *     el «rb_stts»), las colas de envío (TFD de 128 octetos), la carga del
 *     firmware por el canal de servicio del FH, sección a sección, y el
 *     planificador (SCD) cuando el firmware dice ALIVE;
 *   - la capa MVM (mvm/fw.c, nvm.c, scan.c, sta.c): la imagen INIT (leer la
 *     MAC del NVM, calibrar y guardar el «PHY DB»), la imagen RT (devolverle
 *     el PHY DB, las antenas, DQA, la estación auxiliar, el país por LAR) y
 *     el ESCANEO, que hace el firmware (la orden LMAC: SCAN_OFFLOAD_REQUEST,
 *     porque este firmware no tiene el escaneo UMAC). Las balizas llegan
 *     como dos paquetes, el del PHY (0xC0, la señal y el canal) y la trama
 *     (0xC1), y van a la gestión 802.11 de siempre (w80211.c).
 *
 * ETAPA 1 (23/11/2026): encontrar la tarjeta, cargar el firmware, escanear y
 * ver las redes. Falta (etapa 2, docs/PENTIUM.md): asociarse (MAC_CONTEXT,
 * BINDING, TIME_QUOTA, ADD_STA del punto de acceso, TIME_EVENT, TX_CMD,
 * LQ_CMD) y WPA2 con CCMP por la tarjeta (ADD_STA_KEY). Sin interrupciones:
 * se sondea en cada vuelta del servicio.
 *
 * No depende de Linux ni del equipo: la tarjeta se ve por «struct iwl_plat»
 * (iwl.h). En WIFI.BIN la pone radio_iwl_pc.c; en el anfitrión, el modelo
 * de la tarjeta de tests/wifi/iwl_modelo.c (castel/pruebas.sh, parte 49).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "nucleo.h"
#include "iwl.h"
#include "iwl_fw.h"

/* ------------------------------------------------------------ registros */
#define CSR_HW_IF_CONFIG	0x000
#define CSR_INT_COALESCING	0x004
#define CSR_INT			0x008
#define CSR_INT_MASK		0x00C
#define CSR_FH_INT_STATUS	0x010
#define CSR_RESET		0x020
#define CSR_GP_CNTRL		0x024
#define CSR_HW_REV		0x028
#define CSR_GIO			0x03C
#define CSR_UCODE_DRV_GP1_CLR	0x05C
#define CSR_MBOX_SET		0x088
#define CSR_SHADOW_CTRL		0x0A8
#define CSR_GIO_CHICKEN		0x100
#define CSR_DBG_HPET_MEM	0x240
#define CSR_DBG_LINK_PWR	0x250
#define HBUS_MEM_WADDR		0x410
#define HBUS_MEM_WDAT		0x418
#define HBUS_PRPH_WADDR		0x444
#define HBUS_PRPH_RADDR		0x448
#define HBUS_PRPH_WDAT		0x44C
#define HBUS_PRPH_RDAT		0x450
#define HBUS_WRPTR		0x460

#define HW_IF_NIC_READY		0x00400000
#define HW_IF_PREPARE		0x08000000
#define HW_IF_HAP_WAKE_L1A	0x00080000
#define GP_MAC_CLOCK_READY	0x00000001
#define GP_INIT_DONE		0x00000004
#define GP_MAC_ACCESS_REQ	0x00000008
#define GP_GOING_TO_SLEEP	0x00000010
#define GP_RFKILL_RADIO_ON	0x08000000
#define INT_FH_RX		0x80000000
#define INT_HW_ERR		0x20000000
#define INT_FH_TX		0x08000000
#define INT_SW_ERR		0x02000000
#define INT_ALIVE		0x00000001

/* FH (la base es 0x1000) */
#define FH_RSCSR0_STTS_WPTR	0x1BC0
#define FH_RSCSR0_RBDCB_BASE	0x1BC4
#define FH_RSCSR0_WPTR		0x1BC8
#define FH_RSCSR0_RDPTR		0x1BCC
#define FH_RCSR0_CONFIG		0x1C00
#define FH_RCSR0_RBDCB_WPTR	0x1C08
#define FH_RCSR0_FLUSH		0x1C10
#define FH_RSSR_RX_STATUS	0x1C44
#define FH_KW_MEM_ADDR		0x197C
#define FH_TCSR_CONFIG(c)	(0x1D00 + 0x20 * (c))
#define FH_TCSR_BUF_STS(c)	(0x1D08 + 0x20 * (c))
#define FH_SRVC_SRAM_ADDR	0x19C8
#define FH_TFDIB_CTRL0		0x1948
#define FH_TFDIB_CTRL1		0x194C
#define FH_TX_CHICKEN		0x1E98
#define FH_TSSR_TX_STATUS	0x1EB0
#define FH_SRVC		9

/* periferia */
#define APMG_CLK_EN		0x3004
#define APMG_CLK_DIS		0x3008
#define APMG_PS_CTRL		0x300C
#define APMG_PCIDEV_STT		0x3010
#define APMG_RTC_INT_STT	0x301C
#define LMPM_CHICK		0xA01FF8
#define SCD_SRAM_BASE		0xA02C00
#define SCD_DRAM_BASE		0xA02C08
#define SCD_TXFACT		0xA02C10
#define SCD_QUEUE_RDPTR0	0xA02C68
#define SCD_QUEUE_STATUS0	0xA02D0C
#define SCD_GP_CTRL		0xA02DA8
#define SCD_AGGR_SEL		0xA02E48
#define SCD_EN_CTRL		0xA02E54

/* las órdenes (grupo 1 salvo DQA, grupo 5) y las notificaciones (grupo 0) */
#define N_ALIVE			0x01
#define N_ERROR			0x02
#define N_INIT_COMPLETE		0x04
#define C_PHY_CONTEXT		0x08
#define C_ADD_STA		0x18
#define C_SCD_QUEUE_CFG		0x1D
#define C_SCAN_LMAC		0x51
#define C_PHY_CFG		0x6A
#define N_PHY_DB		0x6B
#define C_PHY_DB		0x6C
#define N_SCAN_COMPLETE		0x6D
#define C_POWER_TABLE		0x77
#define C_NVM_ACCESS		0x88
#define C_TX_ANT		0x98
#define C_BT_CONFIG		0x9B
#define N_RX_PHY		0xC0
#define N_RX_MPDU		0xC1
#define C_MCC_UPDATE		0xC8
#define C_DQA_ENABLE		0x00	/* grupo 5 */

#define NRBD		256		/* el anillo de recepción */
#define NRB		64		/* búferes de verdad: la ranura s usa el s % NRB */
#define RBTAM		4096
#define NTFD		256
#define NCMD		32		/* los «primeros TB» de las órdenes */
#define TROZO		0x8000		/* lo que se carga de una vez del firmware */
#define CMDTAM		8192

/* ------------------------------------------------------------ el estado */
static const struct iwl_plat *P;
static const struct iwl_fw *FW;
static const char *modelo = "Intel";
static d32 hw_rev;
static o8 *ctl;			/* RBD (1 K) en 0, rb_stts en 0x400, primeros TB (32 x 64) en 0x800 */
static d32 ctl_f;
static o8 *rbs; static d32 rbs_f;
static o8 *tfd[2]; static d32 tfd_f[2];
static o8 *bc; static d32 bc_f;
static o8 *kw; static d32 kw_f;
static o8 *cmd; static d32 cmd_f;
static o8 *trozo; static d32 trozo_f;
static int rx_lee, rx_wptr = -1;
static int wp0;			/* la cola de órdenes (la 0) */
static int esperando, esp_idx, resp_listo, resp_n;
static o8 resp[2048];
static int alive, alive_ok, init_completo, escaneando;
static d32 scd_base;
static o8 ultima_phy[0x44]; static int hay_phy;
static d32 tx_bien, rx_tramas, rx_malas, errores_fw;
static int vivo;
static struct radio iwlr;
/* el PHY DB de la imagen INIT, para devolvérselo a la RT */
#define MAX_PHYDB 24
static struct { p16 tipo, n; d32 en; } phydb[MAX_PHYDB];
static int n_phydb;
static o8 phydb_datos[24576];
static d32 phydb_usado;
static o8 canal_flags[14];	/* 2,4 GHz: 1 vale, 2 escaneo activo */

static d32 R(d32 r) { return P->lee(r); }
static void W(d32 r, d32 v) { P->pon(r, v); }
static void PON_BITS(d32 r, d32 m) { W(r, R(r) | m); }
static void QUITA_BITS(d32 r, d32 m) { W(r, R(r) & ~m); }
static d32 PR(d32 a) { W(HBUS_PRPH_RADDR, (a & 0xFFFFF) | (3 << 24)); return R(HBUS_PRPH_RDAT); }
static void PW(d32 a, d32 v) { W(HBUS_PRPH_WADDR, (a & 0xFFFFF) | (3 << 24)); W(HBUS_PRPH_WDAT, v); }
static void PPON(d32 a, d32 m) { PW(a, PR(a) | m); }
static void PQUITA(d32 a, d32 m) { PW(a, PR(a) & ~m); }
static p16 le16(const o8 *p) { return p[0] | (p[1] << 8); }
static d32 le32(const o8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((d32)p[3] << 24); }
static void pon16(o8 *p, p16 v) { p[0] = v; p[1] = v >> 8; }
static void pon32(o8 *p, d32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* espera a que (registro & m) == v, hasta «us» microsegundos */
static int espera_reg(d32 r, d32 m, d32 v, d32 us)
{
	d32 t;

	for (t = 0; t <= us; t += 10) {
		if ((R(r) & m) == v)
			return 0;
		P->espera_us(10);
	}
	return -1;
}

/* ------------------------------------------------------- el «NIC access» */
/* Como apmg_wake_up_wa de Linux para la 7000: se pide una vez y se tiene
 * mientras la tarjeta esté en marcha (sin ahorro de energía; se sondea) */
static int coge_acceso(void)
{
	PON_BITS(CSR_GP_CNTRL, GP_MAC_ACCESS_REQ);
	if (espera_reg(CSR_GP_CNTRL, GP_MAC_CLOCK_READY | GP_GOING_TO_SLEEP, GP_MAC_CLOCK_READY, 15000) < 0) {
		hl_log(0, "iwl: la tarjeta no da acceso a sus registros (GP_CNTRL %08X)\n", R(CSR_GP_CNTRL));
		return -1;
	}
	return 0;
}

/* -------------------------------------------- despertar (pcie/trans.c) */
static int lista_hw(void)		/* iwl_pcie_set_hw_ready */
{
	PON_BITS(CSR_HW_IF_CONFIG, HW_IF_NIC_READY);
	if (espera_reg(CSR_HW_IF_CONFIG, HW_IF_NIC_READY, HW_IF_NIC_READY, 50) == 0) {
		PON_BITS(CSR_MBOX_SET, 0x20);	/* OS_ALIVE */
		return 0;
	}
	return -1;
}

static int prepara(void)		/* iwl_pcie_prepare_card_hw */
{
	int i;
	d32 t;

	if (lista_hw() == 0)
		return 0;
	PON_BITS(CSR_DBG_LINK_PWR, 0x80000000);
	P->espera_us(2000);
	for (i = 0; i < 10; i++) {
		PON_BITS(CSR_HW_IF_CONFIG, HW_IF_PREPARE);
		for (t = 0; t < 150000; t += 500) {
			if (lista_hw() == 0)
				return 0;
			P->espera_us(500);
		}
		P->espera_us(25000);
	}
	hl_log(0, "iwl: la tarjeta no se deja preparar (HW_IF_CONFIG %08X)\n", R(CSR_HW_IF_CONFIG));
	return -1;
}

static int apm_init(void)		/* iwl_pcie_apm_init, la 7000 */
{
	PON_BITS(CSR_GIO_CHICKEN, 0x20000000 | 0x00800000);
	PON_BITS(CSR_DBG_HPET_MEM, 0xFFFF0000);
	PON_BITS(CSR_HW_IF_CONFIG, HW_IF_HAP_WAKE_L1A);
	PON_BITS(CSR_GIO, 0x00000002);		/* sin L0s */
	PON_BITS(CSR_GP_CNTRL, GP_INIT_DONE);
	if (espera_reg(CSR_GP_CNTRL, GP_MAC_CLOCK_READY, GP_MAC_CLOCK_READY, 25000) < 0) {
		hl_log(0, "iwl: el reloj de la tarjeta no arranca (GP_CNTRL %08X)\n", R(CSR_GP_CNTRL));
		return -1;
	}
	if (coge_acceso() < 0)
		return -1;
	PW(APMG_CLK_EN, 0x200);			/* DMA_CLK_RQT */
	P->espera_us(20);
	PPON(APMG_PCIDEV_STT, 0x800);		/* L1_ACT_DIS */
	PW(APMG_RTC_INT_STT, 0x10000000);
	return 0;
}

static void sin_interrupciones(void)
{
	W(CSR_INT_MASK, 0);
	W(CSR_INT, 0xFFFFFFFF);
	W(CSR_FH_INT_STATUS, 0xFFFFFFFF);
}

static int arranca_hw(void)		/* _iwl_trans_pcie_start_hw */
{
	if (prepara() < 0)
		return -1;
	PON_BITS(CSR_RESET, 0x80);		/* SW_RESET */
	P->espera_us(6000);
	if (prepara() < 0 || apm_init() < 0)
		return -1;
	sin_interrupciones();
	W(CSR_INT_MASK, 0x80);
	return 0;
}

static void para_hw(void)		/* _iwl_trans_pcie_stop_device */
{
	int c;

	sin_interrupciones();
	PW(SCD_TXFACT, 0);
	for (c = 0; c < 8; c++)
		W(FH_TCSR_CONFIG(c), 0);
	espera_reg(FH_TSSR_TX_STATUS, 0x00FF0000, 0x00FF0000, 5000);
	W(FH_RCSR0_CONFIG, 0);
	espera_reg(FH_RSSR_RX_STATUS, 0x01000000, 0x01000000, 1000);
	PW(APMG_CLK_DIS, 0x200);
	P->espera_us(5);
	QUITA_BITS(CSR_GP_CNTRL, GP_MAC_ACCESS_REQ);
	PON_BITS(CSR_RESET, 0x200);		/* STOP_MASTER */
	espera_reg(CSR_RESET, 0x100, 0x100, 100);
	PON_BITS(CSR_RESET, 0x80);
	P->espera_us(6000);
	QUITA_BITS(CSR_GP_CNTRL, GP_INIT_DONE);
	PON_BITS(CSR_RESET, 0x80);
	P->espera_us(6000);
	prepara();
	sin_interrupciones();
	W(CSR_INT_MASK, 0x80);
	vivo = 0;
}

/* ------------------------------------------------ las colas (rx.c, tx.c) */
static void rx_init(void)
{
	int s;

	for (s = 0; s < NRBD; s++)
		pon32(ctl + 4 * s, (rbs_f + (s % NRB) * RBTAM) >> 8);
	memset(ctl + 0x400, 0, 16);
	W(FH_RCSR0_CONFIG, 0);
	W(FH_RCSR0_RBDCB_WPTR, 0);
	W(FH_RCSR0_FLUSH, 0);
	W(FH_RSCSR0_RDPTR, 0);
	W(FH_RSCSR0_WPTR, 0);
	W(FH_RSCSR0_RBDCB_BASE, ctl_f >> 8);
	W(FH_RSCSR0_STTS_WPTR, (ctl_f + 0x400) >> 4);
	/* activo, sin mirar el FIFO vacío, interrupción al anfitrión, RB de
	 * 4 K, el plazo 0x11 y el anillo de 2^8 */
	W(FH_RCSR0_CONFIG, 0x80801114);
	P->pon(CSR_INT_COALESCING, (R(CSR_INT_COALESCING) & ~0xFFu) | 0x40);
	rx_lee = 0;
	rx_wptr = -1;
}

/* la tarjeta puede llenar las ranuras de rx_lee a wptr-1: como mucho NRB-1,
 * para que dos ranuras que comparten búfer no estén nunca a la vez */
static void rx_repone(void)
{
	int w = ((rx_lee + NRB - 1) & 0xFF) & ~7;

	if (w != rx_wptr) {
		rx_wptr = w;
		W(FH_RSCSR0_WPTR, w);
	}
}

static d32 cbbc(int q)
{
	return q < 16 ? 0x19D0 + 4 * q : q < 20 ? 0x1BF0 + 4 * (q - 16) : 0x1B20 + 4 * (q - 20);
}

static void tx_init(void)
{
	int q;

	PW(SCD_TXFACT, 0);
	W(FH_KW_MEM_ADDR, kw_f >> 4);
	for (q = 0; q < 31; q++)
		W(cbbc(q), tfd_f[q == 0 ? 0 : 1] >> 8);
	PPON(SCD_GP_CTRL, 0x00040000 | 0x00000001);
	wp0 = 0;
}

static void sram_pon(d32 a, d32 v)
{
	W(HBUS_MEM_WADDR, a);
	W(HBUS_MEM_WDAT, v);
}

static void tx_arranca(void)		/* iwl_pcie_tx_start, tras ALIVE */
{
	d32 b = PR(SCD_SRAM_BASE), i;
	int c;

	if (b != scd_base)
		hl_log(1, "iwl: (!) el SCD dice %08X y el ALIVE %08X: se usa el del SCD\n", b, scd_base);
	scd_base = b;
	W(HBUS_MEM_WADDR, scd_base + 0x600);
	for (i = 0; i < 0x87; i++)
		W(HBUS_MEM_WDAT, 0);
	PW(SCD_DRAM_BASE, bc_f >> 10);
	/* la cola de órdenes: la 0, en el FIFO 7 */
	PW(SCD_EN_CTRL, 0);
	PW(SCD_QUEUE_STATUS0, 0x00080000);
	PQUITA(SCD_AGGR_SEL, 1);
	wp0 = 0;
	W(HBUS_WRPTR, 0);
	PW(SCD_QUEUE_RDPTR0, 0);
	sram_pon(scd_base + 0x600, 0);
	sram_pon(scd_base + 0x604, 0x00400040);
	PW(SCD_QUEUE_STATUS0, 0x017F001F);
	PW(SCD_EN_CTRL, 1);
	PW(SCD_TXFACT, 0xFF);
	for (c = 0; c < 8; c++)
		W(FH_TCSR_CONFIG(c), 0x80000008);
	PON_BITS(FH_TX_CHICKEN, 2);
	PQUITA(APMG_PCIDEV_STT, 0x800);
}

/* ------------------------------------------------------- lo que llega */
static void llega_trama(const o8 *d, int n)	/* REPLY_RX_MPDU_CMD */
{
	int bc_ = le16(d), senal = -100, canal = 0;
	d32 st;

	if (n < 4 + bc_ + 4 || bc_ < 24) {
		rx_malas++;
		return;
	}
	st = le32(d + 4 + bc_);
	if ((st & 3) != 3) {			/* CRC_OK y OVERRUN_OK */
		rx_malas++;
		return;
	}
	if (hay_phy) {
		int ea = ultima_phy[0x1C], eb = ultima_phy[0x1D];
		int sa = ea ? -ea : -128, sb = eb ? -eb : -128;
		senal = sa > sb ? sa : sb;
		canal = le16(ultima_phy + 0x16);
	}
	rx_tramas++;
	w80211_rx(d + 4, bc_, senal, canal);
}

static void guarda_phydb(const o8 *d, int n)
{
	int t, l, i, grupo;

	if (n < 4)
		return;
	t = le16(d);
	l = le16(d + 2);
	if (l > n - 4 || !l)
		return;
	grupo = (t == 4 || t == 5) && l >= 2 ? le16(d + 4) : -1;
	for (i = 0; i < n_phydb; i++)		/* una nueva del mismo tipo y grupo sustituye a la de antes */
		if (phydb[i].tipo == t && (grupo < 0 || le16(phydb_datos + phydb[i].en) == grupo) && phydb[i].n == l) {
			memcpy(phydb_datos + phydb[i].en, d + 4, l);
			return;
		}
	if (n_phydb >= MAX_PHYDB || phydb_usado + l > sizeof phydb_datos) {
		hl_log(0, "iwl: no cabe el PHY DB (tipo %d, %d octetos)\n", t, l);
		return;
	}
	phydb[n_phydb].tipo = t;
	phydb[n_phydb].n = l;
	phydb[n_phydb].en = phydb_usado;
	memcpy(phydb_datos + phydb_usado, d + 4, l);
	phydb_usado += (l + 3) & ~3u;
	n_phydb++;
}

static void paquete(const o8 *p, int dn)
{
	int c = p[4], seq = le16(p + 6);
	const o8 *d = p + 8;

	if (!(seq & 0x8000) && esperando && ((seq >> 8) & 0x1F) == 0 && (seq & 0xFF) == esp_idx) {
		resp_n = dn < (int)sizeof resp ? dn : (int)sizeof resp;
		memcpy(resp, d, resp_n);
		resp_listo = 1;
		esperando = 0;
		return;
	}
	switch (c) {
	case N_ALIVE:
		alive = 1;
		if (dn >= 0x44) {
			alive_ok = le16(d) == 0xCAFE;
			scd_base = le32(d + 0x28);
			hl_log(2, "iwl: ALIVE %s (estado %04X, firmware %u.%u, SCD en %08X, errores en %08X)\n",
			       alive_ok ? "bien" : "MAL", le16(d), le32(d + 4), le32(d + 8), scd_base, le32(d + 0x14));
		} else
			hl_log(0, "iwl: ALIVE de %d octetos: no es la forma que se espera\n", dn);
		break;
	case N_PHY_DB:
		guarda_phydb(d, dn);
		break;
	case N_INIT_COMPLETE:
		init_completo = 1;
		break;
	case N_RX_PHY:
		if (dn >= 0x44) {
			memcpy(ultima_phy, d, 0x44);
			hay_phy = 1;
		}
		break;
	case N_RX_MPDU:
		llega_trama(d, dn);
		break;
	case N_SCAN_COMPLETE:
		escaneando = 0;
		hl_log(2, "iwl: escaneo acabado (estado %d, %u tramas oídas)\n", dn > 2 ? d[2] : -1, rx_tramas);
		break;
	case N_ERROR:
		errores_fw++;
		hl_log(0, "iwl: el firmware rechaza una orden: error %08X, orden %02X, secuencia %04X\n",
		       dn >= 4 ? le32(d) : 0, dn >= 5 ? d[4] : 0, dn >= 7 ? le16(d + 5) : 0);
		break;
	}
}

static void un_rb(const o8 *b)
{
	int o = 0;

	while (o + 12 < RBTAM) {
		d32 lf = le32(b + o);
		int len = lf & 0x3FFF, total = len + 4;
		if (lf == 0x55550000 || total < 8 || o + ((total + 63) & ~63) > RBTAM)
			break;
		paquete(b + o, len - 4);
		o += (total + 63) & ~63;
	}
}

/* lo que haya llegado */
static void atiende(void)
{
	d32 in = R(CSR_INT);
	int r;

	if (in == 0xFFFFFFFF)
		return;
	if (in) {
		W(CSR_INT, in);
		if (in & INT_FH_RX)
			W(CSR_FH_INT_STATUS, 0x40030000);
		if (in & (INT_HW_ERR | INT_SW_ERR)) {
			errores_fw++;
			hl_log(0, "iwl: FALLO DEL FIRMWARE (CSR_INT %08X)\n", in);
		}
	}
	r = le16(ctl + 0x400) & 0xFF;
	while (rx_lee != r) {
		un_rb(rbs + (rx_lee % NRB) * RBTAM);
		rx_lee = (rx_lee + 1) & 0xFF;
	}
	rx_repone();
}

/* ------------------------------------------------------- las órdenes */
static int orden(int grupo, int id, const void *datos, int n, int espera_ms)
{
	o8 *t = tfd[0] + 128 * wp0, *primero = ctl + 0x800 + 64 * (wp0 % NCMD);
	int total = 8 + n, k, nt = 1, o;
	d32 t0;

	if (total > CMDTAM)
		return -1;
	cmd[0] = id; cmd[1] = grupo;
	pon16(cmd + 2, wp0);			/* cola 0, índice wp0 */
	pon16(cmd + 4, n);
	cmd[6] = cmd[7] = 0;
	memcpy(cmd + 8, datos, n);
	memset(t, 0, 128);
	k = total < 20 ? total : 20;
	memcpy(primero, cmd, k);
	pon32(t + 4, ctl_f + 0x800 + 64 * (wp0 % NCMD));
	pon16(t + 8, k << 4);
	for (o = k; o < total; o += 0xFF0, nt++) {
		int l = total - o < 0xFF0 ? total - o : 0xFF0;
		pon32(t + 4 + 6 * nt, cmd_f + o);
		pon16(t + 8 + 6 * nt, l << 4);
	}
	t[3] = nt;
	esp_idx = wp0;
	esperando = 1;
	resp_listo = 0;
	wp0 = (wp0 + 1) & 0xFF;
	W(HBUS_WRPTR, wp0);
	t0 = P->ms();
	while (!resp_listo) {
		atiende();
		if (P->ms() - t0 > (d32)espera_ms) {
			esperando = 0;
			hl_log(0, "iwl: la orden %02X (grupo %d) no tiene respuesta en %d ms\n", id, grupo, espera_ms);
			return -1;
		}
		if (!resp_listo)
			P->espera_us(200);
	}
	return 0;
}

static d32 estado_resp(void) { return resp_n >= 4 ? le32(resp) : 0xFFFFFFFF; }

/* ------------------------------------------------- cargar el firmware */
static int carga_imagen(const struct iwl_imagen *im, const char *cual)
{
	int i;
	d32 o, t0, total = 0;

	sin_interrupciones();
	if (!(R(CSR_GP_CNTRL) & GP_RFKILL_RADIO_ON))
		hl_log(1, "iwl: (!) el interruptor de la radio está apagado (rfkill)%s\n",
		       im == &FW->img[IWL_IMG_INIT] ? ": la de calibrar se carga igual" : "");
	W(CSR_UCODE_DRV_GP1_CLR, 0x2);
	W(CSR_UCODE_DRV_GP1_CLR, 0x4);
	W(CSR_INT, 0xFFFFFFFF);
	/* nic_init: apm, la alimentación, la radio (nic_config), las colas */
	if (apm_init() < 0)
		return -1;
	PW(APMG_PS_CTRL, PR(APMG_PS_CTRL) & ~0x03000000u);
	{
		d32 v = (R(CSR_HW_REV) & 0xF) | (((FW->phy_sku >> 0) & 3) << 10) | (((FW->phy_sku >> 2) & 3) << 14) |
			(((FW->phy_sku >> 4) & 3) << 12) | 0x200;
		W(CSR_HW_IF_CONFIG, (R(CSR_HW_IF_CONFIG) & ~0xFF0Fu) | v);
		PPON(APMG_PS_CTRL, 0x00400000);
	}
	rx_init();
	tx_init();
	PON_BITS(CSR_SHADOW_CTRL, 0x800FFFFF);
	W(CSR_INT_MASK, INT_FH_TX);
	W(CSR_UCODE_DRV_GP1_CLR, 0x2);
	W(CSR_UCODE_DRV_GP1_CLR, 0x2);
	for (i = 0; i < im->n; i++) {
		const struct iwl_sec *s = &im->sec[i];
		if (s->destino == IWL_SEP_CPU1_CPU2 || s->destino == IWL_SEP_PAGINACION)
			break;
		for (o = 0; o < s->tam; o += TROZO) {
			d32 l = s->tam - o < TROZO ? s->tam - o : TROZO, dst = s->destino + o;
			int ext = dst >= 0x40000 && dst <= 0x57FFF;
			memcpy(trozo, s->datos + o, l);
			if (ext)
				PPON(LMPM_CHICK, 1);
			W(FH_TCSR_CONFIG(FH_SRVC), 0);
			W(FH_SRVC_SRAM_ADDR, dst);
			W(FH_TFDIB_CTRL0, trozo_f);
			W(FH_TFDIB_CTRL1, l);
			W(FH_TCSR_BUF_STS(FH_SRVC), 0x00101003);
			W(FH_TCSR_CONFIG(FH_SRVC), 0x80100000);
			t0 = P->ms();
			while (!(R(CSR_INT) & INT_FH_TX)) {
				if (P->ms() - t0 > 5000) {
					hl_log(0, "iwl: la tarjeta no acaba de cargar el trozo %08X (%u octetos) de %s\n", dst, l, cual);
					return -1;
				}
				P->espera_us(20);
			}
			W(CSR_INT, INT_FH_TX);
			W(CSR_FH_INT_STATUS, 3);
			if (ext)
				PQUITA(LMPM_CHICK, 1);
			total += l;
		}
	}
	hl_log(2, "iwl: imagen %s cargada: %u octetos en %d secciones; se suelta la CPU\n", cual, total, i);
	W(CSR_INT_MASK, 0xBA00008B);
	alive = alive_ok = 0;
	W(CSR_RESET, 0);
	t0 = P->ms();
	while (!alive) {
		atiende();
		if (P->ms() - t0 > 1000) {
			hl_log(0, "iwl: el firmware (%s) no dice ALIVE en 1 s (CSR_INT %08X, GP_CNTRL %08X)\n", cual,
			       R(CSR_INT), R(CSR_GP_CNTRL));
			return -1;
		}
		P->espera_us(200);
	}
	if (!alive_ok)
		return -1;
	tx_arranca();
	vivo = 1;
	return 0;
}

/* ------------------------------------------------------------ MVM */
static int bt_config(void)
{
	o8 b[8];

	pon32(b, 1);			/* BT_COEX_NW */
	pon32(b + 4, 0x15);
	return orden(1, C_BT_CONFIG, b, 8, 2000);
}

static int ant_y_phy(int img)
{
	o8 b[12];

	pon32(b, 3);
	if (orden(1, C_TX_ANT, b, 4, 2000) < 0)
		return -1;
	pon32(b, FW->phy_sku);
	pon32(b + 4, FW->img[img].flujo);
	pon32(b + 8, FW->img[img].evento);
	return orden(1, C_PHY_CFG, b, 12, 2000);
}

/* el primer trozo (2 K) de la sección «sec» del NVM */
static int lee_nvm(int sec, o8 *d, int cabe)
{
	o8 b[8];
	int n;

	b[0] = 0; b[1] = 0;
	pon16(b + 2, sec);
	pon16(b + 4, 0);
	pon16(b + 6, 0x800);
	if (orden(1, C_NVM_ACCESS, b, 8, 2000) < 0 || resp_n < 8 || le16(resp + 6) != 0)
		return -1;
	n = le16(resp + 2);
	if (n > resp_n - 8)
		n = resp_n - 8;
	if (n > cabe)
		n = cabe;
	memcpy(d, resp + 8, n);
	return n;
}

static int fase_init(void)
{
	o8 nvm[0x800];
	int n, i;
	d32 t0;

	if (carga_imagen(&FW->img[IWL_IMG_INIT], "INIT") < 0)
		return -1;
	n_phydb = 0;
	phydb_usado = 0;
	init_completo = 0;
	bt_config();
	n = lee_nvm(0, nvm, sizeof nvm);
	if (n < 0x30) {
		hl_log(0, "iwl: no se puede leer la sección HW del NVM (%d)\n", n);
		return -1;
	}
	iwlr.mac[0] = nvm[0x2B]; iwlr.mac[1] = nvm[0x2A];
	iwlr.mac[2] = nvm[0x2D]; iwlr.mac[3] = nvm[0x2C];
	iwlr.mac[4] = nvm[0x2F]; iwlr.mac[5] = nvm[0x2E];
	hl_log(2, "iwl: MAC %02x:%02x:%02x:%02x:%02x:%02x (del NVM)\n", iwlr.mac[0], iwlr.mac[1], iwlr.mac[2],
	       iwlr.mac[3], iwlr.mac[4], iwlr.mac[5]);
	if (iwlr.mac[0] & 1 || !(iwlr.mac[0] | iwlr.mac[1] | iwlr.mac[2] | iwlr.mac[3] | iwlr.mac[4] | iwlr.mac[5]))
		hl_log(1, "iwl: (!) esa MAC no vale\n");
	n = lee_nvm(1, nvm, sizeof nvm);
	memset(canal_flags, 0, sizeof canal_flags);
	if (n >= 0x20 + 28) {
		hl_log(2, "iwl: NVM versión %04X, SKU %04X (%s%s%s%s)\n", le16(nvm), le16(nvm + 4),
		       le16(nvm + 4) & 1 ? "2,4 GHz" : "", le16(nvm + 4) & 2 ? ", 5 GHz" : "",
		       le16(nvm + 4) & 4 ? ", 11n" : "", le16(nvm + 4) & 8 ? ", 11ac" : "");
		for (i = 0; i < 14; i++) {
			p16 f = le16(nvm + 0x20 + 2 * i);
			canal_flags[i] = (f & 1 ? 1 : 0) | (f & 8 ? 2 : 0);
		}
	}
	if (ant_y_phy(IWL_IMG_INIT) < 0)
		return -1;
	t0 = P->ms();
	while (!init_completo) {
		atiende();
		if (P->ms() - t0 > 2000) {
			hl_log(0, "iwl: la calibración no acaba en 2 s (%d trozos del PHY DB)\n", n_phydb);
			return -1;
		}
		P->espera_us(500);
	}
	hl_log(2, "iwl: calibrada: %d trozos del PHY DB (%u octetos)\n", n_phydb, phydb_usado);
	return 0;
}

static int manda_phydb(void)
{
	static const int orden_tipos[4] = { 1, 2, 4, 5 };
	int k, i, n = 0;

	for (k = 0; k < 4; k++)
		for (i = 0; i < n_phydb; i++) {
			static o8 tmp[4 + 4096];
			if (phydb[i].tipo != orden_tipos[k] || phydb[i].n > 4096)
				continue;
			pon16(tmp, phydb[i].tipo);
			pon16(tmp + 2, phydb[i].n);
			memcpy(tmp + 4, phydb_datos + phydb[i].en, phydb[i].n);
			if (orden(1, C_PHY_DB, tmp, 4 + phydb[i].n, 2000) < 0)
				return -1;
			n++;
		}
	return n;
}

static int fase_rt(void)
{
	o8 b[44];
	int i, n;

	para_hw();
	if (arranca_hw() < 0 || carga_imagen(&FW->img[IWL_IMG_RT], "RT") < 0)
		return -1;
	memset(b, 0, 4);
	pon32(b, 3);
	if (orden(1, C_TX_ANT, b, 4, 2000) < 0)
		return -1;
	n = manda_phydb();
	if (n < 0)
		return -1;
	if (ant_y_phy(IWL_IMG_RT) < 0)
		return -1;
	bt_config();
	memset(b, 0, 4);			/* DQA: la cola de órdenes es la 0 */
	if (orden(5, C_DQA_ENABLE, b, 4, 2000) < 0)
		return -1;
	/* la estación auxiliar (la del escaneo): la 1, con la cola 1 en el FIFO 5 */
	W(HBUS_WRPTR, 0 | (1 << 8));
	memset(b, 0, 12);
	b[1] = 1; b[2] = 8; b[3] = 1; b[4] = 1; b[6] = 5; b[7] = 64;
	if (orden(1, C_SCD_QUEUE_CFG, b, 12, 2000) < 0)
		return -1;
	memset(b, 0, 44);
	pon16(b + 2, 0xFFFF);
	pon32(b + 4, 4);			/* MAC_INDEX_AUX, color 0 */
	b[0x10] = 1;
	pon32(b + 0x28, 2);
	if (orden(1, C_ADD_STA, b, 44, 2000) < 0 || (estado_resp() & 0xFF) != 1) {
		hl_log(0, "iwl: la estación auxiliar no se da de alta (%08X)\n", estado_resp());
		return -1;
	}
	/* el contexto PHY 0, en el canal 1 */
	memset(b, 0, 36);
	pon32(b + 4, 1);
	b[0x10] = 1; b[0x11] = 1;
	pon32(b + 0x14, 3);
	pon32(b + 0x18, 0x1406);
	orden(1, C_PHY_CONTEXT, b, 36, 2000);
	memset(b, 0, 4);			/* sin ahorro de energía (CAM): se sondea */
	orden(1, C_POWER_TABLE, b, 4, 2000);
	/* el país: el que diga el firmware (LAR) */
	memset(b, 0, 28);
	pon16(b, 0x5A5A);
	b[2] = 0x10;
	if (orden(1, C_MCC_UPDATE, b, 28, 2000) == 0 && resp_n >= 16) {
		d32 nc = le32(resp + 12);
		hl_log(2, "iwl: país «%c%c» (estado %u, %u canales)\n", resp[5], resp[4], le32(resp), nc);
		if (resp_n >= (int)(16 + 4 * nc) && nc >= 14)
			for (i = 0; i < 14; i++) {
				d32 f = le32(resp + 16 + 4 * i);
				canal_flags[i] = (f & 1 ? 1 : 0) | (f & 8 ? 2 : 0);
			}
	} else
		hl_log(1, "iwl: (!) MCC_UPDATE sin respuesta: el escaneo puede no ir\n");
	hl_log(2, "iwl: en marcha (%d órdenes del PHY DB devueltas)\n", n);
	return 0;
}

/* ------------------------------------------------------------ escanear */
static int escanea(void)
{
	static o8 s[0x6EC];
	static const o8 velocidades[16] = { 1, 8, 0x02, 0x04, 0x0B, 0x16, 0x0C, 0x12, 0x18, 0x24, 50, 4, 0x30, 0x48, 0x60, 0x6C };
	o8 *pr;
	int i, n = 0;

	memset(s, 0, sizeof s);
	s[5] = 10; s[6] = 110; s[7] = 44; s[8] = 90;
	pon16(s + 0x0A, 0x01B7);
	pon32(s + 0x0C, 0x81);			/* PASS_ALL | EXTENDED_DWELL: activo, con el SSID comodín */
	pon32(s + 0x18, 1);			/* empieza en 2,4 GHz */
	pon32(s + 0x1C, 0x44);
	pon32(s + 0x20, 0x3000); pon32(s + 0x24, 0x420A); s[0x28] = 1;
	pon32(s + 0x2C, 0x3000); pon32(s + 0x30, 0x400D); s[0x34] = 1;
	pon32(s + 0x2E0, 6);
	pon32(s + 0x2E4, 1);
	s[0x2EE] = 1; s[0x2EF] = 1;
	for (i = 0; i < 13; i++) {
		o8 *c;
		if (canal_flags[0] && !(canal_flags[i] & 1))
			continue;		/* (si el firmware dijo los canales, solo los que valen) */
		c = s + 0x2FC + 12 * n++;
		pon32(c, 0x10000002);
		pon16(c + 4, i + 1);
		pon16(c + 6, 1);
	}
	s[4] = n;
	pr = s + 0x4DC;
	pon16(pr + 0, 0); pon16(pr + 2, 26);
	pon16(pr + 4, 26); pon16(pr + 6, sizeof velocidades);
	pon16(pr + 8, 26 + sizeof velocidades); pon16(pr + 10, 0);
	pon16(pr + 12, 26 + sizeof velocidades); pon16(pr + 14, 0);
	pr += 16;
	pr[0] = 0x40;
	memset(pr + 4, 0xFF, 6);
	memcpy(pr + 10, iwlr.mac, 6);
	memset(pr + 16, 0xFF, 6);
	memcpy(pr + 26, velocidades, sizeof velocidades);
	hay_phy = 0;
	escaneando = 1;
	/* (0.8.10, el ASUS GL553VD: «no acepta el escaneo (FFFFFFFF)» y luego
	 * escaneaba bien) la respuesta puede no traer el estado (menos de 4
	 * octetos): FFFFFFFF es «no lo dice», no un fallo; solo vale como
	 * fallo un estado que diga algo distinto de 0 */
	if (orden(1, C_SCAN_LMAC, s, sizeof s, 2000) < 0 || (resp_n >= 4 && estado_resp() != 0)) {
		escaneando = 0;
		hl_log(0, "iwl: el firmware no acepta el escaneo (%08X)\n", estado_resp());
		return -1;
	}
	if (resp_n < 4)
		hl_log(2, "iwl: la respuesta al escaneo no trae su estado (%d octetos): se da por buena\n", resp_n);
	hl_log(2, "iwl: escaneando %d canales de 2,4 GHz\n", n);
	return 0;
}

/* ---------------------------------------------- la radio para w80211.c */
static int r_canal(int c) { (void)c; return 0; }		/* (el escaneo lo hace el firmware) */
static int r_enviar(const o8 *t, int n, int vel) { (void)t; (void)n; (void)vel; return -1; }	/* (etapa 2) */
/* (w80211 pide el barrido antes de empezar a escuchar: el escaneo se manda
 * en la vuelta siguiente, para que no se pierda ninguna baliza) */
static int escaneo_pedido;
static void r_sondear(void)
{
	if (!vivo)
		return;
	if (escaneo_pedido && !escaneando) {
		escaneo_pedido = 0;
		escanea();
	}
	atiende();
}
static void r_barrido(int empieza) { if (vivo && empieza) escaneo_pedido = 1; }
static void r_bss(const o8 *b, int a, int i, int c, const o8 *v, int nv) { (void)b; (void)a; (void)i; (void)c; (void)v; (void)nv; }
static void r_parar(void) { if (vivo) para_hw(); }

/* ------------------------------------------------------------ la entrada */
int iwl_reconoce(d32 id, const char **m)
{
	static const struct { p16 d; const char *n; int estado; } t[] = {
		{ 0x095A, "Intel Wireless-AC 7265", 0 }, { 0x095B, "Intel Wireless-AC 7265", 0 },
		{ 0x3165, "Intel Wireless-AC 3165", 0 }, { 0x3166, "Intel Wireless-AC 3165", 0 },
		{ 0x24FB, "Intel Wireless-AC 3168", 0 },
		{ 0x08B1, "Intel Wireless-AC 7260 (aún no: su firmware es de otra versión)", 1 },
		{ 0x08B2, "Intel Wireless-AC 7260 (aún no)", 1 }, { 0x08B3, "Intel Wireless-AC 3160 (aún no)", 1 },
		{ 0x08B4, "Intel Wireless-AC 3160 (aún no)", 1 },
		{ 0x24F3, "Intel Wireless 8260 (aún no: familia 8000)", 1 }, { 0x24FD, "Intel Wireless 8265 (aún no: familia 8000)", 1 },
		{ 0x9DF0, "Intel Wireless-AC 9560/9462 CNVi, Cannon Point-LP (aún no: familia 9000)", 1 },
		{ 0xA370, "Intel Wireless-AC 9560/9462 CNVi, Cannon Point-H (aún no: familia 9000)", 1 },
		{ 0x31DC, "Intel Wireless-AC 9560/9462 CNVi, Gemini Lake (aún no: familia 9000)", 1 },
		{ 0x30DC, "Intel Wireless-AC 9560 CNVi (aún no: familia 9000)", 1 },
		{ 0x2526, "Intel Wireless-AC 9260 (aún no: familia 9000)", 1 },
		{ 0x271B, "Intel Wireless-AC 9162 (aún no)", 1 }, { 0x271C, "Intel Wireless-AC 9162 (aún no)", 1 },
		{ 0x02F0, "Intel CNVi de Comet Lake: AX201 o 9560 (aún no: familia 22000, gen2)", 1 },
		{ 0x06F0, "Intel CNVi de Comet Lake-H: AX201 o 9560 (aún no: gen2)", 1 },
		{ 0x34F0, "Intel CNVi de Ice Lake: AX201 o 9560 (aún no: gen2)", 1 },
		{ 0x4DF0, "Intel CNVi de Jasper Lake: AX201 o 9560 (aún no: gen2)", 1 },
		{ 0xA0F0, "Intel CNVi de Tiger Lake-LP: AX201 o 9560 (aún no: familia 22000, gen2)", 1 },
		{ 0x43F0, "Intel CNVi de Tiger Lake-H: AX201 o 9560 (aún no: gen2)", 1 },
		{ 0x2723, "Intel Wi-Fi 6 AX200 (aún no: gen2)", 1 },
		{ 0x2725, "Intel Wi-Fi 6E AX210 (aún no: gen3)", 1 },
		{ 0x51F0, "Intel CNVi de Alder Lake-P: AX211/AX201 (aún no: gen3)", 1 },
		{ 0x51F1, "Intel CNVi de Raptor Lake-P: AX211/AX201 (aún no: gen3)", 1 },
		{ 0x54F0, "Intel CNVi de Alder Lake-N: AX211/AX201 (aún no: gen3)", 1 },
		{ 0x7A70, "Intel CNVi de Raptor Lake-S: AX211 (aún no: gen3)", 1 },
		{ 0x7AF0, "Intel CNVi de Alder Lake-S: AX211 (aún no: gen3)", 1 },
		{ 0x7E40, "Intel CNVi de Meteor Lake: AX211/BE200 (aún no)", 1 },
	};
	int i;

	if ((id & 0xFFFF) != 0x8086)
		return -1;
	for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++)
		if (t[i].d == (id >> 16)) {
			if (m)
				*m = t[i].n;
			return t[i].estado;
		}
	return -1;
}

/* los nombres de linux-firmware para esta tarjeta (por la revisión del chip) */
const char *const *iwl_nombres_para(d32 id, d32 rev)
{
	static const char *const n7265d[] = { "iwlwifi-7265D-29.ucode", NULL };
	static const char *const n7265[] = { "iwlwifi-7265-17.ucode", "iwlwifi-7265D-29.ucode", NULL };
	static const char *const n3168[] = { "iwlwifi-3168-29.ucode", NULL };
	int d = id >> 16;

	if (d == 0x24FB)
		return n3168;
	if ((d == 0x095A || d == 0x095B) && (rev & 0xFFF0) != 0x210)
		return n7265;
	return n7265d;
}

int iwl_arranca(const struct iwl_plat *p, d32 id, const struct iwl_fw *fw, struct radio **r)
{
	P = p;
	FW = fw;
	iwl_reconoce(id, &modelo);
	hw_rev = R(CSR_HW_REV);
	if (hw_rev == 0xFFFFFFFF) {
		hl_log(0, "iwl: la tarjeta no contesta (sus registros dan FF)\n");
		return -1;
	}
	hl_log(2, "iwl: %s, CSR_HW_REV %08X (tipo %03X%s, paso %u)\n", modelo, hw_rev, (hw_rev >> 4) & 0xFFF,
	       (hw_rev & 0xFFF0) == 0x210 ? ": 7265D" : "", (hw_rev & 0xC) >> 2);
	if (!fw->hay_phy_sku || !fw->img[IWL_IMG_INIT].n || iwl_fw_capa(fw, IWL_CAPA_UMAC_SCAN) || fw->n_cpu != 1) {
		hl_log(0, "iwl: este firmware no es de la familia 7000 (sin INIT o PHY_SKU, o con escaneo UMAC, o %u CPU)\n", fw->n_cpu);
		return -1;
	}
	if (!iwl_fw_capa(fw, IWL_CAPA_DQA))
		hl_log(1, "iwl: (!) este firmware no tiene DQA: la cola de órdenes de este controlador es la de DQA\n");
	if (!ctl) {
		ctl = P->dma(4096, &ctl_f);
		rbs = P->dma(NRB * RBTAM, &rbs_f);
		tfd[0] = P->dma(NTFD * 128, &tfd_f[0]);
		tfd[1] = P->dma(NTFD * 128, &tfd_f[1]);
		bc = P->dma(0x5000, &bc_f);
		kw = P->dma(4096, &kw_f);
		cmd = P->dma(CMDTAM, &cmd_f);
		trozo = P->dma(TROZO, &trozo_f);
		if (!ctl || !rbs || !tfd[0] || !tfd[1] || !bc || !kw || !cmd || !trozo) {
			hl_log(0, "iwl: no hay memoria de DMA para las colas\n");
			return -1;
		}
	}
	if (arranca_hw() < 0 || fase_init() < 0 || fase_rt() < 0)
		return -1;
	iwlr.nombre = modelo;
	iwlr.canal = r_canal;
	iwlr.enviar = r_enviar;
	iwlr.sondear = r_sondear;
	iwlr.barrido = r_barrido;
	iwlr.bss = r_bss;
	iwlr.parar = r_parar;
	iwlr.barre_sola = 1;
	*r = &iwlr;
	return 0;
}

int iwl_escaneando(void) { return escaneando; }

void iwl_cuentas(d32 *bien, d32 *mal, int *vel)
{
	*bien = tx_bien + rx_tramas;
	*mal = rx_malas + errores_fw;
	*vel = 0;
}

void iwl_resumen(void)
{
	hl_log(2, "iwl: %s: %u tramas oídas, %u malas, %u errores del firmware\n", modelo, rx_tramas, rx_malas, errores_fw);
}
