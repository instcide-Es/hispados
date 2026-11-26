/*
 * tarjeta_sim.c - Un MODELO de la tarjeta Broadcom BCM43224 (o, con
 * tsim_4313, de la BCM4313) para probar su controlador sin tenerla: la configuración PCI, los núcleos del bus interno
 * (ChipCommon, 802.11 y PCIe, con su EROM y una SPROM de revisión 8), los
 * registros del núcleo 802.11 (PHY, radio, memoria compartida) y los anillos
 * de DMA de envío y recepción. Lo que la «tarjeta» emite llega al punto de
 * acceso de mentira de wifi/nucleo/sim.c y lo que éste contesta entra por el
 * anillo de recepción.
 *
 * Lo usan la prueba del anfitrión (tests/wifi/chip_sim.c) y el núcleo de
 * pruebas HSPWSIM.BIN (el núcleo de verdad con esta tarjeta dentro, para
 * QEMU: tests/run_wifi_tests.sh).
 *
 * OJO: el modelo dice lo que el controlador ESPERA de la tarjeta, no lo que
 * la tarjeta hace: prueba nuestro código, no el hardware.
 *
 * (BCM4313) La BCM4313 (PCI 14E4:4727) es otra: núcleo 802.11 de revisión
 * 24, PHY LCN de revisión 1 con la radio 2064, una sola banda (2,4 GHz) y una
 * sola cadena. Lo que el modelo añade para ella es lo mínimo para que el PHY
 * LCN de brcmsmac (phy_lcn.c) pase por su puesta en marcha: el número de la
 * radio, que la 2064 se lee en «dirección | 0x200» y los bits de «hecho» de
 * sus esperas (la calibración de la resistencia, la estimación de IQ, el
 * sensor de temperatura). Las calibraciones dan lo que den con estos
 * registros: comprueban que el código las recorre, no que calibren.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include <defs.h>
#include "types.h"
#include "d11.h"
#include "tarjeta_sim.h"

unsigned long hl_jiffies(void);

/* ----------------------------------------------------- la tarjeta falsa */
#define BAR 0xE0000000u
#define CC 0x18000000u
#define D11 0x18001000u
#define PCIE 0x18003000u
#define ENV 0x18100000u		/* envoltorios: +0, +0x1000, +0x3000 */
#define EROM 0x18109000u

u8 tsim_cfg[256];
#define cfg tsim_cfg
static u8 r_cc[4096], r_d11[4096], r_pcie[4096], r_env[3][4096];
static u32 erom[32];
static u16 sprom[220];
static u16 phy[0x1000], radio_r[0x1000];
static u32 shm[4][0x4000];
static u32 intstatus_mac;

static u32 le(const u8 *m, int off, int tam)
{
	u32 v = 0;
	memcpy(&v, m + off, tam);
	return v;
}
static void pon(u8 *m, int off, int tam, u32 v) { memcpy(m + off, &v, tam); }

static u8 crc8(u8 crc, u8 d)
{
	int i;
	crc ^= d;
	for (i = 0; i < 8; i++)
		crc = (crc & 1) ? (crc >> 1) ^ 0xab : crc >> 1;
	return crc;
}

/* cola de estados de envío y el anillo de recepción */
static u32 txs_cola[64][2];
static int txs_cab, txs_fin;
static int tx_cur[6];
static int rx_cur;
int tsim_canal = 1;
/* (BCM4313) los canales a los que el PHY LCN se ha puesto (bit c) */
u32 tsim_lcn_canales;
/* (BCM4313) cuántas veces se ha esperado a cada calibración: 0 TX IQ/LO, 1
 * estimación de IQ (la de RX IQ y la potencia), 2 RCAL, 3 temperatura */
u32 tsim_lcn_cal[4];
/* (BCM4313) 1: el modelo es de una BCM4313 en vez de una BCM43224 (2: la
 * BCM4313 de la placa con amplificador de fuera y Bluetooth al lado) */
#ifdef TSIM_4313
int tsim_4313 = 1;
#else
int tsim_4313;
#endif
/* (HPRADIO) las miradas al bit RFD de phydebug que aún dirán «radio apagada
 * por la patilla W_DISABLE#» (la tecla del Wi-Fi de los HP); 0: encendida */
int tsim_rfd;
#define canal_radio tsim_canal
static struct radio *ap;	/* el punto de acceso de mentira (sim.c) */
u32 tsim_tx, tsim_rx, tsim_tx_malas, tsim_accesos;
#define tramas_tx tsim_tx
#define tramas_rx tsim_rx
#define tx_malas tsim_tx_malas
#define accesos tsim_accesos

static void anillo_rx(const o8 *trama, int n, int senal, int canal)
{
	u32 base = le(r_d11, 0x200 + 0x20 + 8, 4);	/* dmarcv.addrlow de la cola 0 */
	u32 ctl = le(r_d11, 0x200 + 0x20, 4);
	u32 ult = le(r_d11, 0x200 + 0x20 + 4, 4) & 0x1fff;
	u32 *d;
	u8 *b;
	struct d11rxhdr *h;
	int rxoff = (ctl >> 1) & 0x7f;

	(void)senal;
	if (!(ctl & 1) || !base)
		return;
	if (((base + rx_cur * 16) & 0x1fff) == ult)
		return;				/* sin descriptores libres */
	d = hl_virt(base + rx_cur * 16);
	b = hl_virt(d[2]);
	memset(b, 0, rxoff + 6);
	h = (struct d11rxhdr *)b;
	h->RxFrameSize = 6 + n + 4;
	h->PhyRxStatus_0 = 1;			/* OFDM */
	h->PhyRxStatus_1 = 0x4040;		/* (potencias, por poner algo) */
	h->RxChan = canal << 3;
	b[rxoff] = 0x0b;			/* PLCP: 6 Mbit/s */
	memcpy(b + rxoff + 6, trama, n);
	memset(b + rxoff + 6 + n, 0, 4);	/* (el FCS: el controlador no lo mira) */
	rx_cur = (rx_cur + 1) % 256;
	if (d[0] & 0x10000000)			/* fin de la tabla */
		rx_cur = 0;
	pon(r_d11, 0x200 + 0x20 + 0x10, 4, 0x10000000 | ((base + rx_cur * 16) & 0x1fff));
	pon(r_d11, 0x20, 4, le(r_d11, 0x20, 4) | I_RI);
	intstatus_mac |= MI_DMAINT;
	tramas_rx++;
}

/* el controlador ha dejado tramas en la cola i: «emitirlas» */
static void envia_cola(int i, u32 ptr)
{
	u32 base = le(r_d11, 0x200 + 0x40 * i + 8, 4);
	int hasta = ((ptr - base) & 0x1fff) / 16;
	static u8 t[4096];
	int n = 0;

	while (tx_cur[i] != hasta) {
		u32 *d = hl_virt(base + tx_cur[i] * 16);
		int c = d[1] & 0x7fff;
		if (d[0] & 0x80000000)
			n = 0;
		if (n + c <= (int)sizeof(t)) {
			memcpy(t + n, hl_virt(d[2]), c);
			n += c;
		}
		if (d[0] & 0x40000000) {
			struct d11txh *h = (struct d11txh *)t;
			u8 *f = t + D11_TXH_LEN + D11_PHY_HDR_LEN;
			int nf = n - D11_TXH_LEN - D11_PHY_HDR_LEN, acuse = 0;
			tramas_tx++;
			if (nf < 10 || nf > 2400) {
				tx_malas++;
			} else {
				/* la cabecera de Broadcom tiene que cuadrar con la trama */
				if (h->MacFrameControl != (f[0] | (f[1] << 8)))
					tx_malas++;
				if (!(f[4] & 1))
					acuse = 1;
				ap->canal(canal_radio);
				ap->enviar(f, nf, 0);
			}
			if (((txs_fin + 1) & 63) != txs_cab) {
				txs_cola[txs_fin][0] = ((u32)h->TxFrameID << 16) | TXS_V | (acuse ? TX_STATUS_ACK_RCV : 0);
				txs_cola[txs_fin][1] = 0;
				txs_fin = (txs_fin + 1) & 63;
			}
			intstatus_mac |= MI_TFS;
		}
		tx_cur[i] = (tx_cur[i] + 1) % 256;
		if (d[0] & 0x10000000)
			tx_cur[i] = 0;
	}
	pon(r_d11, 0x200 + 0x40 * i + 0x10, 4, 0x20000000 | (ptr & 0x1fff));
}

/* (BCM4313) El PHY LCN: lo que sus esperas miran. Los disparos se dan por
 * hechos en cuanto se leen. (Aquí, a diferencia del modelo de la 43224, lo
 * que se escribe con write_phy_reg, en 32 bits por 3FCh, se guarda y se lee
 * después: el PHY LCN lee y modifica mucho lo que acaba de escribir.) */
static u32 lcn_phy_lee(u32 r)
{
	u32 v = phy[r];

	switch (r) {
	case 0x451:			/* calibración de IQ/LO en marcha (bits 14 y 15) */
		tsim_lcn_cal[0]++;
		return v & 0x3fff;
	case 0x481:			/* estimación de IQ en marcha (bit 9) */
		tsim_lcn_cal[1]++;
		return v & ~0x200u;
	case 0x476:			/* sensor de temperatura/tensión: medida hecha */
		tsim_lcn_cal[3]++;
		return v | 0x8000;
	}
	return v;
}

static void lcn_phy_escribe(u32 r, u32 v)
{
	phy[r] = v;
	/* el canal: el que el PHY LCN programa al cambiarlo
	 * (wlc_phy_chanspec_set_lcnphy pone en 657h el ptcentreTs20 de su
	 * tabla lcnphy_sfo_cfg: 965 + 2 por canal; el 14, 994). La tarjeta
	 * oye y emite en ESE canal: si el PHY no lo cambia, no se ve ninguna
	 * red de otro canal */
	if (r == 0x657) {
		int c = v == 994 ? 14 : (int)(v - 965) / 2 + 1;
		if (c >= 1 && c <= 14 && (v == 994 || !((v - 965) & 1))) {
			canal_radio = c;
			tsim_lcn_canales |= 1u << c;
		}
	}
}

/* La radio 2064: se escribe en la dirección y se lee en «dirección | 0x200»;
 * sin el 0x200 (solo para leer su número), los registros 0 a 2 dicen
 * 0x2064, revisión 0 */
static u32 lcn_radio_lee(u32 r)
{
	if (!(r & 0x200)) {
		if (r == 0)
			return 0x00;		/* revisión y versión */
		if (r == 1)
			return 0x64;
		if (r == 2)
			return 0x20;
	}
	r &= 0x1ff;
	if (r == 0x05c) {		/* la calibración de la resistencia (RCAL), hecha */
		tsim_lcn_cal[2]++;
		return radio_r[r] | 0x20;
	}
	return radio_r[r];
}

static u32 plano_lee(u32 dir, int tam)
{
	u32 base = dir & ~0xfffu, off = dir & 0xfff;

	accesos++;
	if (base == EROM)
		return off / 4 < ARRAY_SIZE(erom) ? erom[off / 4] : 0xf;
	if (base == CC) {
		if (off >= 0x800 && off < 0x800 + 440)
			return sprom[(off - 0x800) / 2];
		if (off == 0x1e0)
			return le(r_cc, off, 4) | 0x00030000;	/* relojes ALP y HT listos */
		return le(r_cc, off, tam);
	}
	if (base == D11) {
		switch (off) {
		case 0x128:
			return intstatus_mac;
		case 0x170:
			return txs_cab != txs_fin ? txs_cola[txs_cab][0] : 0;
		case 0x174: {
			u32 v = 0;
			if (txs_cab != txs_fin) {
				v = txs_cola[txs_cab][1];
				txs_cab = (txs_cab + 1) & 63;
			}
			return v;
		}
		case 0x164:
			return shm[(le(r_d11, 0x160, 4) >> 16) & 3][le(r_d11, 0x160, 4) & 0x3fff];
		case 0x166:
			return shm[(le(r_d11, 0x160, 4) >> 16) & 3][le(r_d11, 0x160, 4) & 0x3fff] >> 16;
		case 0x1e0: {
			/* relojes listos, y los PLL que se piden (bits 8-10 -> 24-26) */
			u32 v = le(r_d11, off, 4);
			return v | 0x00030000 | ((v & 0x700) << 16);
		}
		case 0x3e0:
			if (tsim_4313)
				return 0x0801;		/* (BCM4313) PHY LCN, revisión 1 */
			return 0x0406;			/* PHY N, revisión 6 */
		case 0x3fe:
			if (tsim_4313)
				return lcn_phy_lee(le(r_d11, 0x3fc, 2) & 0xfff);
			if ((le(r_d11, 0x3fc, 2) & 0xfff) == 0x78 || (le(r_d11, 0x3fc, 2) & 0xfff) == 0x129)
				return 0;		/* los disparos (secuencias, medidas) se borran solos */
			return phy[le(r_d11, 0x3fc, 2) & 0xfff];
		case 0x3da:			/* (BCM4313: núcleo rev. 24, la radio va por 3D8h/3DAh) */
			if (tsim_4313)
				return lcn_radio_lee(le(r_d11, 0x3d8, 2) & 0xfff);
			break;
		case 0x3fa:
			if ((le(r_d11, 0x3f6, 2) & 0xfff) == 1)
				return ((5u << 28) | (0x2056 << 12) | 0x17f) & 0xffff;
			return radio_r[le(r_d11, 0x3f6, 2) & 0xfff];
		case 0x3f8:
			if ((le(r_d11, 0x3f6, 2) & 0xfff) == 1)
				return ((5u << 28) | (0x2056 << 12) | 0x17f) >> 16;
			return 0;
		case 0x180:				/* TSF: el reloj */
			return hl_jiffies() * 1000;
		case 0x158:				/* (HPRADIO) phydebug: el bit RFD (1 << 16) es la patilla W_DISABLE# */
			if (tsim_rfd > 0) {
				tsim_rfd--;
				return le(r_d11, off, tam) | (1u << 16);
			}
			break;
		}
		return le(r_d11, off, tam);
	}
	if (base == PCIE) {
		if (off == 0x128)		/* MDIO (la SerDes del PCIe): hecho en el acto */
			return le(r_pcie, off, tam) | 0x100;
		return le(r_pcie, off, tam);
	}
	if (base == ENV + 0x1000 && off == 0x500)
		return tsim_4313 ? 0x0005		/* (BCM4313) solo 2,4 GHz, y el reloj rápido listo */
				 : 0x000c;		/* doble banda, y el reloj rápido listo */
	if (base == ENV || base == ENV + 0x1000 || base == ENV + 0x3000)
		return le(r_env[(base - ENV) >> 12 == 3 ? 2 : (base - ENV) >> 12], off, tam);
	return 0xffffffff;
}

static void plano_escribe(u32 dir, int tam, u32 v)
{
	u32 base = dir & ~0xfffu, off = dir & 0xfff;
	int i;

	accesos++;
	if (base == CC) {
		pon(r_cc, off, tam, v);
		return;
	}
	if (base == PCIE) {
		pon(r_pcie, off, tam, v);
		return;
	}
	if (base == ENV || base == ENV + 0x1000 || base == ENV + 0x3000) {
		pon(r_env[(base - ENV) >> 12 == 3 ? 2 : (base - ENV) >> 12], off, tam, v);
		return;
	}
	if (base != D11)
		return;
	switch (off) {
	case 0x120:
		pon(r_d11, off, tam, v);
		/* con la MAC parada, el microcódigo avisa de que está suspendida */
		if (!(v & MCTL_EN_MAC))
			intstatus_mac |= MI_MACSSPNDD;
		return;
	case 0x128:
		intstatus_mac &= ~v;
		return;
	case 0x164:
		shm[(le(r_d11, 0x160, 4) >> 16) & 3][le(r_d11, 0x160, 4) & 0x3fff] = v;
		return;
	case 0x3fc:
		/* (BCM4313) write_phy_reg: dirección y dato de una vez, en 32 bits */
		if (tsim_4313 && tam == 4) {
			pon(r_d11, off, 2, v);
			lcn_phy_escribe(v & 0xfff, v >> 16);
			return;
		}
		break;
	case 0x3fe:
		if (tsim_4313) {
			lcn_phy_escribe(le(r_d11, 0x3fc, 2) & 0xfff, v & 0xffff);
			return;
		}
		phy[le(r_d11, 0x3fc, 2) & 0xfff] = v;
		return;
	case 0x3da:
		if (tsim_4313) {
			radio_r[le(r_d11, 0x3d8, 2) & 0x1ff] = v;	/* (la 2064 se lee en +0x200) */
			return;
		}
		break;
	case 0x3fa:
		radio_r[le(r_d11, 0x3f6, 2) & 0xfff] = v;
		return;
	}
	/* intstatus de las colas: se borra lo que se escribe */
	if (off >= 0x20 && off < 0x60 && !(off & 4)) {
		pon(r_d11, off, 4, le(r_d11, off, 4) & ~v);
		return;
	}
	pon(r_d11, off, tam, v);
	for (i = 0; i < 6; i++) {
		u32 b = 0x200 + 0x40 * i;
		if (off == b) {				/* control de envío */
			pon(r_d11, b + 0x10, 4, (v & 1) ? 0x20000000 : 0);
			if (!(v & 1))
				tx_cur[i] = 0;
		} else if (off == b + 4) {
			envia_cola(i, v);
		} else if (off == b + 8) {
			tx_cur[i] = 0;
		} else if (off == b + 0x20) {		/* control de recepción */
			pon(r_d11, b + 0x30, 4, (v & 1) ? 0x20000000 | (le(r_d11, b + 0x28, 4) & 0x1fff) : 0);
			if (i == 0 && !(v & 1))
				rx_cur = 0;
		} else if (off == b + 0x28 && i == 0) {
			rx_cur = 0;
		}
	}
}

u32 hl_sim_leer(const void *dir, int tam)
{
	u32 off = hl_fis(dir) - BAR, zona = off >> 12;

	off &= 0xfff;
	if (zona == 0)
		return plano_lee(le(cfg, 0x80, 4) + off, tam);
	if (zona == 1)
		return plano_lee(le(cfg, 0xac, 4) + off, tam);
	if (zona == 2)
		return plano_lee(PCIE + off, tam);
	if (zona == 3)
		return plano_lee(CC + off, tam);
	return 0xffffffff;
}

void hl_sim_escribir(const void *dir, int tam, u32 v)
{
	u32 off = hl_fis(dir) - BAR, zona = off >> 12;

	off &= 0xfff;
	if (zona == 0)
		plano_escribe(le(cfg, 0x80, 4) + off, tam, v);
	else if (zona == 1)
		plano_escribe(le(cfg, 0xac, 4) + off, tam, v);
	else if (zona == 2)
		plano_escribe(PCIE + off, tam, v);
	else if (zona == 3)
		plano_escribe(CC + off, tam, v);
}

u32 hl_sim_pci_lee(int w) { return le(cfg, w, 4); }
void hl_sim_pci_escribe(int w, u32 v)
{
	if (w < 4 || w == 8 || (w >= 0x10 && w < 0x28) || w == 0x2c)
		return;
	pon(cfg, w, 4, v);
}

void tarjeta_sim_monta(struct radio *punto_de_acceso)
{
	static const u8 mac[6] = { 0x00, 0x26, 0x82, 0x12, 0x34, 0x56 };
	int i, n = 0;
	u8 c = 0xff;

	ap = punto_de_acceso;
	sim_entrega = anillo_rx;

	/* PCI: 14E4:4353 (o 14E4:4727, la BCM4313), red, BAR0 de memoria de 64
	 * bits, IRQ 10; capacidades de energía (40h) y PCIe (D0h) */
	pon(cfg, 0, 4, tsim_4313 ? 0x472714e4 : 0x435314e4);
	pon(cfg, 4, 4, 0x00100000);
	pon(cfg, 8, 4, 0x02800001);
	pon(cfg, 0x10, 4, BAR | 4);
	pon(cfg, 0x2c, 4, tsim_4313 ? 0x1483103c : 0x1510103c);	/* (subsistemas de HP; el de la 4313, uno cualquiera) */
	cfg[0x34] = 0x40; cfg[0x3c] = 10; cfg[0x3d] = 1;
	cfg[0x40] = 1; cfg[0x41] = 0xd0; cfg[0x44] = 3;	/* dormida (D3): hay que despertarla */
	cfg[0xd0] = 0x10; cfg[0xd1] = 0;
	pon(cfg, 0x80, 4, CC);
	/* la EROM: los tres núcleos */
#define NUCLEO(id, rev, dir, env, maestros) do { \
		erom[n++] = (0x4bfu << 20) | ((id) << 8) | 1; \
		erom[n++] = ((u32)(rev) << 24) | (1 << 19) | (1 << 9) | ((maestros) << 4) | 1; \
		for (i = 0; i < (maestros); i++) erom[n++] = 0x3; \
		erom[n++] = (dir) | 0x5; \
		erom[n++] = (env) | 0x85; \
	} while (0)
	/* (BCM4313: ChipCommon rev. 36, 802.11 rev. 24, PCIe rev. 17) */
	NUCLEO(0x800, tsim_4313 ? 0x24 : 0x22, CC, ENV, 0);
	NUCLEO(0x812, tsim_4313 ? 0x18 : 0x17, D11, ENV + 0x1000, 1);
	NUCLEO(0x820, tsim_4313 ? 0x11 : 0x0f, PCIE, ENV + 0x3000, 1);
	erom[n++] = 0xf;
	/* ChipCommon: BCM43224 rev. 1 (o BCM4313 rev. 1, encapsulado 8), con PMU y SPROM */
	if (tsim_4313)
		pon(r_cc, 0, 4, 0x4313 | (1 << 16) | (8 << 20) | (3 << 24) | (1u << 28));
	else
		pon(r_cc, 0, 4, 43224 | (1 << 16) | (0xa << 20) | (3 << 24) | (1u << 28));
	pon(r_cc, 4, 4, 0x50000000 | (tsim_4313 ? 0x24 : 0x22));
	pon(r_cc, 0xfc, 4, EROM);
	pon(r_cc, 0x190, 4, 1);
	pon(r_cc, 0x604, 4, 0x00000006);		/* PMU rev. 6 */
	/* SPROM de revisión 8: placa, MAC, país, antenas y potencias */
	sprom[0x82 / 2] = 0x1250;			/* revisión de la placa */
	sprom[0x84 / 2] = 0x0200;			/* indicadores de la placa */
	sprom[0x8c / 2] = (mac[0] << 8) | mac[1];
	sprom[0x8e / 2] = (mac[2] << 8) | mac[3];
	sprom[0x90 / 2] = (mac[4] << 8) | mac[5];
	sprom[0x92 / 2] = ('E' << 8) | 'S';
	sprom[0x9c / 2] = 0x0303;			/* antenas disponibles */
	sprom[0x9e / 2] = 0x0202;			/* ganancia de las antenas */
	sprom[0xa2 / 2] = 0x0233;			/* dos cadenas de envío y dos de recepción */
	if (tsim_4313) {
		/* (BCM4313) sin Bluetooth al lado ni amplificador de fuera (iPA):
		 * los indicadores, a cero; una cadena de envío y una de recepción.
		 * Con tsim_4313 = 2, la otra placa: amplificador de fuera (ePA,
		 * BFL_FEM) y la antena compartida con el Bluetooth (BFL_FEM_BT) */
		sprom[0x84 / 2] = tsim_4313 == 2 ? 0x0800 : 0;
		sprom[0x86 / 2] = tsim_4313 == 2 ? 0x0040 : 0;
		sprom[0xa2 / 2] = 0x0211;
	}
	for (i = 0; i < 2; i++) {
		int b = (0xc0 + 0x20 * i) / 2;
		sprom[b] = 0x4a3e;			/* potencia máxima e ITSSI (2,4 GHz) */
		sprom[b + 1] = 0xfe9a; sprom[b + 2] = 0x16b4; sprom[b + 3] = 0xfa7e;	/* curva del amplificador */
	}
	sprom[219] = 8;
	for (i = 0; i < 219; i++) {
		c = crc8(c, sprom[i] & 0xff);
		c = crc8(c, sprom[i] >> 8);
	}
	c = crc8(c, sprom[219] & 0xff);
	sprom[219] |= (u16)(c ^ 0xff) << 8;
}

