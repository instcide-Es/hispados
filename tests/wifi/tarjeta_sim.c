/*
 * tarjeta_sim.c - Un MODELO de la tarjeta Broadcom BCM43224 para probar su
 * controlador sin tenerla: la configuración PCI, los núcleos del bus interno
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
			return 0x0406;			/* PHY N, revisión 6 */
		case 0x3fe:
			if ((le(r_d11, 0x3fc, 2) & 0xfff) == 0x78)
				return 0;		/* los disparos de secuencias se borran solos */
			return phy[le(r_d11, 0x3fc, 2) & 0xfff];
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
		}
		return le(r_d11, off, tam);
	}
	if (base == PCIE)
		return le(r_pcie, off, tam);
	if (base == ENV + 0x1000 && off == 0x500)
		return 0x000c;				/* doble banda, y el reloj rápido listo */
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
	case 0x3fe:
		phy[le(r_d11, 0x3fc, 2) & 0xfff] = v;
		return;
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

	/* PCI: 14E4:4353, red, BAR0 de memoria de 64 bits, IRQ 10; capacidades
	 * de energía (40h) y PCIe (D0h) */
	pon(cfg, 0, 4, 0x435314e4);
	pon(cfg, 4, 4, 0x00100000);
	pon(cfg, 8, 4, 0x02800001);
	pon(cfg, 0x10, 4, BAR | 4);
	pon(cfg, 0x2c, 4, 0x1510103c);
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
	NUCLEO(0x800, 0x22, CC, ENV, 0);
	NUCLEO(0x812, 0x17, D11, ENV + 0x1000, 1);
	NUCLEO(0x820, 0x0f, PCIE, ENV + 0x3000, 1);
	erom[n++] = 0xf;
	/* ChipCommon: BCM43224 rev. 1, con PMU y SPROM */
	pon(r_cc, 0, 4, 43224 | (1 << 16) | (0xa << 20) | (3 << 24) | (1u << 28));
	pon(r_cc, 4, 4, 0x50000000 | 0x22);
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

