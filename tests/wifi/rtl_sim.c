/*
 * rtl_sim.c - Un MODELO de lo que hay detrás de la tercera controladora
 * SDHCI de una tableta Bay Trail: la controladora (sus registros, las
 * órdenes, el puerto de datos por PIO), la tarjeta SDIO (CMD5, CMD3, CMD7,
 * CMD52 y CMD53, su CCCR, su CIS y el FBR de la función 1) y lo esencial de
 * la Realtek RTL8723BS: las secuencias de encendido, el EFUSE (con una MAC,
 * potencias y la antena), la descarga del firmware y su arranque, los
 * registros del BB y de la radio (por el LSSI), la tabla LLT, las órdenes al
 * firmware (H2C), los filtros de recepción (RCR, BSSID, RXFLTMAP) y las colas
 * de envío (HIQ, LOQ) y recepción (RX0), con sus descriptores. Detrás, el
 * punto de acceso de mentira de wifi/nucleo/sim.c: lo que se manda le llega
 * y lo que contesta entra por la cola de recepción.
 *
 * Sirve para las pruebas del anfitrión (tests/wifi/rtl_prueba.c) y para
 * WIFISIM.BIN con «chiprtl» (QEMU no tiene SDIO): radio_rtl_pc.c lo usa en
 * vez de la controladora de verdad.
 *
 * OJO: el modelo dice lo que el controlador ESPERA del chip (lo que hace el
 * de Linux), no lo que el chip hace: prueba nuestro código, no el hardware.
 * Lo que mira (y cuenta en rtl_sim_quejas): órdenes sin corriente o sin
 * reloj, el bloque y la cuenta de CMD53 que no cuadran con la controladora,
 * el EFUSE leído sin abrirlo (CFh = 69h), el firmware escrito sin la
 * descarga abierta o en otro sitio, la radio escrita de octeto en octeto,
 * los descriptores de envío que no cuadran con la trama y la cola, y la cola
 * de recepción leída de menos.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "rtl_sim.h"

void hl_log(int nivel, const char *f, ...) __attribute__((format(printf, 2, 3)));
extern void (*sim_entrega)(const o8 *trama, int n, int senal, int canal);

int rtl_sim_apagado, rtl_sim_quejas, rtl_sim_fw_bien, rtl_sim_fw_corre;
d32 rtl_sim_tx, rtl_sim_rx, rtl_sim_rx_tirados;
d32 rtl_sim_h2c[8];
int rtl_sim_nh2c, rtl_sim_lssi_mal, rtl_sim_canal, rtl_sim_cd_forzada;

static struct radio *ap;
static void (*esp)(d32);
static d32 (*reloj)(void);

static void queja(const char *q, d32 a)
{
	rtl_sim_quejas++;
	if (rtl_sim_quejas < 30)
		hl_log(0, "MODELO: %s (%X)\n", q, a);
}

static d32 le32(const o8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((d32)p[3] << 24); }
static p16 le16(const o8 *p) { return p[0] | (p[1] << 8); }
static void pon32(o8 *p, d32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* ============================================================ el chip */
static o8 m[0x8000];		/* los registros (IOREG) */
static o8 loc[0x100];		/* los registros locales de SDIO */
static o8 efu[512];		/* el EFUSE (en bruto) */
static o8 fwimg[0x8000];
static d32 fw_hasta;
static const o8 *fw_esp;
static d32 fw_esp_n;
static d32 rf[256];
static int de4;			/* lo que se escribe viene de un CMD53 de 4 octetos */
static o8 rxq[16384];
static int rxq_n;
static int pidio_corriente;

void rtl_sim_fw(const o8 *fw, d32 n)
{
	fw_esp = fw;
	fw_esp_n = n;
}

static int fw_cuadra(void)
{
	if (!fw_hasta)
		return 0;
	if (!fw_esp)
		return 1;
	return fw_hasta == fw_esp_n - 32 && memcmp(fwimg, fw_esp + 32, fw_esp_n - 32) == 0;
}

static o8 lee_reg(d32 a)
{
	a &= 0x7FFF;
	switch (a) {
	case 0x06: return m[a] | 0x02;			/* la corriente, lista */
	case 0x80: return (m[a] & ~(0x04 | 0x40 | 0x80)) | (fw_cuadra() ? 0x04 : 0) | (rtl_sim_fw_corre ? 0xC0 : 0);
	case 0xA0: return 0x01;
	case 0x1CC: return rtl_sim_fw_corre ? m[a] & 0xF0 : m[a];
	case 0x49D: return 0;
	}
	return m[a];
}

static void rf_escribe(d32 v)
{
	d32 r = (v >> 20) & 0xFF;
	rf[r] = v & 0xFFFFF;
	if (r == 0x18)
		rtl_sim_canal = rf[r] & 0xFF;
}

static void pon_reg(d32 a, o8 v)
{
	o8 antes;

	a &= 0x7FFF;
	antes = m[a];
	m[a] = v;
	if (a >= 0x1000 && a < 0x2000) {		/* la ventana del firmware */
		d32 p = (m[0x82] & 7) * 4096 + (a - 0x1000);
		if (!(m[0x80] & 1)) {
			queja("se escribe en la ventana del firmware sin abrir la descarga", a);
			return;
		}
		fwimg[p] = v;
		if (p + 1 > fw_hasta)
			fw_hasta = p + 1;
		return;
	}
	switch (a) {
	case 0x05:
		if (v & 1) {
			pidio_corriente = 1;
			m[a] &= ~1;
		}
		if (v & 2) {
			pidio_corriente = 0;
			m[a] &= ~2;
		}
		break;
	case 0x80:
		if ((v & 0x05) == 0x05) {	/* «reset FWDL chksum» (con la descarga abierta): se escribe otra vez */
			fw_hasta = 0;
			rtl_sim_fw_bien = 0;
		}
		break;
	case 0x03:				/* el 8051 (SYS_FUNC_EN bit 10) */
		if (!(antes & 4) && (v & 4) && (m[0x80] & 0x02)) {
			if (fw_cuadra()) {
				rtl_sim_fw_bien = 1;
				rtl_sim_fw_corre = 1;
			} else {
				queja("se arranca el 8051 con un firmware que no es el que se dio", fw_hasta);
			}
		}
		if ((antes & 4) && !(v & 4))
			rtl_sim_fw_corre = 0;
		break;
	case 0x33:				/* leer un octeto del EFUSE */
		if (!(v & 0x80)) {
			d32 d = m[0x31] | ((m[0x32] & 3) << 8);
			if (m[0xCF] != 0x69)
				queja("se lee el EFUSE sin abrirlo (CFh)", m[0xCF]);
			m[0x30] = efu[d & 511];
			m[0x33] = v | 0x80;
		}
		break;
	case 0x226:				/* AUTO_LLT: hecha en el acto */
		m[a] &= ~1;
		break;
	case 0x843:				/* LSSI: escribir en la radio */
		if (!de4)
			rtl_sim_lssi_mal++;
		rf_escribe(le32(m + 0x840));
		break;
	case 0x827:				/* HSSI 2: leer de la radio (el flanco del bit 31) */
		if (!(antes & 0x80) && (v & 0x80)) {
			d32 r = (le32(m + 0x824) >> 23) & 0xFF;
			pon32(m + 0x8a0, rf[r]);
			pon32(m + 0x8b8, rf[r]);
		}
		break;
	case 0x1D3: case 0x1D7: case 0x1DB: case 0x1DF: {	/* las cajas de H2C */
		int k = (a - 0x1D0) / 4;
		if (rtl_sim_nh2c < 8)
			rtl_sim_h2c[rtl_sim_nh2c] = m[0x1D0 + 4 * k];
		rtl_sim_nh2c++;
		if (!rtl_sim_fw_corre)
			queja("orden al firmware sin firmware", m[0x1D0 + 4 * k]);
		m[0x1CC] |= 1 << k;
		break;
	}
	}
}

static o8 lee_local(d32 a)
{
	a &= 0xFF;
	switch (a) {
	case 0x18: return (rxq_n ? 1 : 0) | (loc[a] & 0xFE);
	case 0x1C: return rxq_n;
	case 0x1D: return rxq_n >> 8;
	case 0x1E: return 8;
	case 0x20: return 0x0C;
	case 0x21: return 0;
	case 0x22: return 0x02;
	case 0x23: return 0xE7;
	case 0x86: return (loc[a] & 1) ? (loc[a] & ~2) : (loc[a] | 2);
	}
	return loc[a];
}

static void pon_local(d32 a, o8 v)
{
	a &= 0xFF;
	if (a >= 0x18 && a < 0x1C) {		/* HISR: escribir 1 borra */
		loc[a] &= ~v;
		return;
	}
	loc[a] = v;
}

/* una trama del punto de acceso: los filtros y a la cola de recepción */
static void llega(const o8 *t, int n, int senal, int canal)
{
	d32 rcr = le32(m + 0x608);
	int tipo = (t[0] >> 2) & 3, sub = t[0] >> 4, mc = t[4] & 1, cab, off;
	o8 *p;

	(void)senal;
	/* (0.8.12c) la radio con el modo (RF 00h [19:16]) a 0 está apagada: no
	 * oye nada (lo que pasaba en el Acer One 10 con las cabeceras de la tabla
	 * escritas como registros) */
	if (canal != rtl_sim_canal || !rtl_sim_fw_corre || !(m[0x100] & 0x80) || n < 24 || !((rf[0] >> 16) & 0xF)) {
		rtl_sim_rx_tirados++;
		return;
	}
	if (!mc && memcmp(t + 4, m + 0x610, 6) != 0) {
		rtl_sim_rx_tirados++;		/* no es para nosotros */
		return;
	}
	if (tipo == 0 && (sub == 8 || sub == 5) && (rcr & 0x80) && memcmp(t + 16, m + 0x618, 6) != 0) {
		rtl_sim_rx_tirados++;		/* CBSSID_BCN */
		return;
	}
	if (tipo == 2) {
		if (le16(m + 0x6A4) == 0) {
			rtl_sim_rx_tirados++;	/* RXFLTMAP2: sin datos */
			return;
		}
		if ((rcr & 0x40) && memcmp(t + 10, m + 0x618, 6) != 0) {
			rtl_sim_rx_tirados++;	/* CBSSID_DATA */
			return;
		}
	}
	cab = 24 + 32;				/* el descriptor y el estado del PHY (DRVINFO 4) */
	if (m[0x60F] != 4 || !(rcr & (1u << 28)))
		queja("la recepción no pide el estado del PHY (DRVINFO 4, APP_PHYST_RXFF)", m[0x60F]);
	off = (rxq_n + 7) & ~7;
	if (off + cab + n > (int)sizeof rxq) {
		rtl_sim_rx_tirados++;
		return;
	}
	p = rxq + off;
	memset(p, 0, cab);
	pon32(p, n | (4 << 16) | (1u << 26));
	if (tipo == 0 && sub == 8) {
		pon32(p + 12, 0);		/* CCK, 1 Mbit/s */
		p[24 + 5] = (6 << 5) | 6;	/* LNA 6, VGA 6: -46 dBm */
	} else {
		pon32(p + 12, 4);		/* OFDM, 6 Mbit/s */
		p[24 + 4] = (-47 + 110) << 1;
	}
	memcpy(p + cab, t, n);
	rxq_n = off + cab + n;
	rtl_sim_rx++;
}

/* una trama por una cola de envío */
static void manda(int dev, d32 dir, const o8 *b, int n)
{
	d32 w0 = le32(b), w1 = le32(b + 4);
	int largo = w0 & 0xFFFF, off = (w0 >> 16) & 0xFF, qsel = (w1 >> 8) & 0x1F, tipo;
	int l4 = (int)(dir & 0x1FFF) * 4;

	rtl_sim_tx++;
	if (!rtl_sim_fw_corre || !(m[0x100] & 0x40)) {
		queja("se manda con el envío apagado (CR MACTXEN) o sin firmware", m[0x100]);
		return;
	}
	if (off != 40 || largo < 10 || 40 + largo > n || l4 != ((40 + largo + 3) & ~3)) {
		queja("el descriptor de envío no cuadra con lo mandado", w0);
		return;
	}
	tipo = (b[40] >> 2) & 3;
	if (tipo == 2 ? (dev != 6 || qsel != 0) : (dev != 4 || qsel != 0x12)) {
		queja("la trama va por una cola que no es la suya", (dev << 8) | qsel);
		return;
	}
	if (!(le32(b + 12) & (1 << 8)))
		queja("el descriptor no dice la velocidad (USERATE)", le32(b + 12));
	ap->canal(rtl_sim_canal);
	ap->enviar(b + 40, largo, 0);
}

/* ============================================================== la tarjeta */
static o8 f0[0x1200];		/* la función 0: el CCCR, el FBR de la 1 y los CIS */
static int listo, rca_dada, elegida;

static void tarjeta_inicia(void)
{
	static const o8 cis0[] = { 0x21, 2, 0x0C, 0x00, 0x22, 4, 0x00, 0x00, 0x02, 0x32,
				   0x20, 4, 0x4C, 0x02, 0x23, 0xB7, 0xFF };
	static const o8 cis1[] = { 0x21, 2, 0x0C, 0x00, 0x22, 8, 0x01, 0x01, 0, 0, 0, 0, 0x00, 0x02, 0xFF };

	memset(f0, 0, sizeof f0);
	f0[0x00] = 0x43;		/* CCCR 3, SDIO 4 */
	f0[0x01] = 0x03;
	f0[0x08] = 0x13;		/* SDC, SMB, S4MI */
	f0[0x09] = 0x00; f0[0x0A] = 0x10; f0[0x0B] = 0x00;	/* el CIS común, en 1000h */
	f0[0x13] = 0x01;		/* SHS */
	f0[0x100] = 0x07;		/* la función 1: WLAN */
	f0[0x109] = 0x00; f0[0x10A] = 0x11; f0[0x10B] = 0x00;	/* su CIS, en 1100h */
	memcpy(f0 + 0x1000, cis0, sizeof cis0);
	memcpy(f0 + 0x1100, cis1, sizeof cis1);
	listo = rca_dada = elegida = 0;
}

static o8 f0_lee(d32 a)
{
	return a < sizeof f0 ? f0[a] : 0;
}

static void f0_pon(d32 a, o8 v)
{
	switch (a) {
	case 0x02:
		f0[0x02] = v & 0xFE;
		f0[0x03] = v & 0x02;		/* la función 1, lista en el acto */
		break;
	case 0x04: case 0x07: case 0x110: case 0x111:
		f0[a] = v;
		break;
	case 0x06:
		if (v & 0x08)			/* RES: reiniciar la E/S */
			tarjeta_inicia();
		break;
	case 0x13:
		f0[a] = (f0[a] & 1) | (v & 2);
		break;
	}
}

static o8 f1_lee(d32 a)
{
	int dev = (a >> 13) & 0xF;
	if (a & 0x10000)
		return lee_reg(a);
	if (dev == 0)
		return lee_local(a);
	queja("CMD52 de una cola", a);
	return 0;
}

static void f1_pon(d32 a, o8 v)
{
	int dev = (a >> 13) & 0xF;
	if (a & 0x10000)
		pon_reg(a, v);
	else if (dev == 0)
		pon_local(a, v);
	else
		queja("CMD52 a una cola", a);
}

/* CMD53 de la función 1 */
static void f1_lee_n(d32 a, o8 *b, int n)
{
	int dev = (a >> 13) & 0xF, i;

	if (!(a & 0x10000) && dev == 7) {	/* la cola de recepción */
		if (n < rxq_n)
			queja("se lee la cola de recepción de menos", (n << 16) | rxq_n);
		memcpy(b, rxq, n < rxq_n ? n : rxq_n);
		if (n > rxq_n)
			memset(b + rxq_n, 0, n - rxq_n);
		rxq_n = 0;
		return;
	}
	for (i = 0; i < n; i++)
		b[i] = f1_lee(a + i);
}

static void f1_pon_n(d32 a, const o8 *b, int n)
{
	int dev = (a >> 13) & 0xF, i;

	if (!(a & 0x10000) && dev >= 4 && dev <= 6) {
		manda(dev, a, b, n);
		return;
	}
	de4 = n == 4;
	for (i = 0; i < n; i++)
		f1_pon(a + i, b[i]);
	de4 = 0;
}

/* ======================================================== la controladora */
static o8 h[256];
static o8 xbuf[32768];
static int x_activo, x_lee, x_bloques, x_largo, x_hechos, x_pos, x_total;
static d32 x_dir;

static void sdhci_inicia(void)
{
	memset(h, 0, sizeof h);
	pon32(h + 0x40, 0x05206400);	/* 3,3 y 1,8 V, alta velocidad, reloj de 100 MHz */
	h[0xFE] = 0x02;			/* la norma 3.00 */
	x_activo = 0;
}

static void error_orden(void)
{
	h[0x32] |= 0x01;		/* tiempo pasado (Command Timeout) */
	h[0x31] |= 0x80;
}

static void responde(d32 r)
{
	pon32(h + 0x10, r);
	h[0x30] |= 0x01;
}

static void ejecuta(void)
{
	int idx = h[0x0F] & 0x3F, datos = h[0x0E] & 0x20;
	d32 arg = le32(h + 8);

	if (!(h[0x29] & 1) || !(h[0x2C] & 4)) {
		queja("orden sin corriente o sin reloj de tarjeta", idx);
		error_orden();
		return;
	}
	if (!(h[0x28] & 0x80) || !(h[0x28] & 0x40)) {
		/* la ranura dice que no hay tarjeta (no tiene patilla) */
		error_orden();
		return;
	}
	rtl_sim_cd_forzada = 1;
	if (rtl_sim_apagado) {
		if (idx != 0)
			error_orden();
		else
			h[0x30] |= 0x01;
		return;
	}
	switch (idx) {
	case 0:
		h[0x30] |= 0x01;
		return;
	case 5:
		if (arg & 0x00FFFFFF)
			listo = 1;
		responde((listo ? 0x80000000u : 0) | (1u << 28) | 0x00FF8000);
		return;
	case 3:
		if (!listo) {
			error_orden();
			return;
		}
		rca_dada = 1;
		responde(0x00010000);
		return;
	case 7:
		elegida = rca_dada && (arg >> 16) == 1;
		responde(0x00000700);
		h[0x30] |= 0x02;
		return;
	case 52: {
		int esc = arg >> 31, f = (arg >> 28) & 7;
		d32 a = (arg >> 9) & 0x1FFFF;
		o8 v = arg & 0xFF, r = 0;
		if (!elegida) {
			if (f == 0 && a == 6) {		/* (el reinicio de la E/S, antes de todo) */
				if (esc)
					f0_pon(a, v);
				responde(0x1000);
				return;
			}
			error_orden();
			return;
		}
		if (f == 0) {
			if (esc)
				f0_pon(a, v);
			r = f0_lee(a);
		} else if (f == 1) {
			if (!(f0[0x03] & 2)) {
				queja("CMD52 a la función 1 sin encenderla", a);
				responde(0x1000 | 0x0200);	/* FUNCTION_NUMBER */
				return;
			}
			if (esc)
				f1_pon(a, v);
			r = esc ? v : f1_lee(a);
		} else {
			responde(0x1000 | 0x0200);
			return;
		}
		responde(0x1000 | r);
		return;
	}
	case 53: {
		int esc = arg >> 31, f = (arg >> 28) & 7, bm = (arg >> 27) & 1, cnt = arg & 0x1FF;
		int blk = le16(h + 4) & 0xFFF, nb = le16(h + 6), b1 = le16(f0 + 0x110);
		int total = bm ? cnt * b1 : (cnt ? cnt : 512);
		if (!elegida || f != 1 || !datos || !(f0[0x03] & 2)) {
			queja("CMD53 sin elegir la tarjeta, sin datos o a otra función", arg);
			error_orden();
			return;
		}
		if (!(arg & (1u << 26)))
			queja("CMD53 sin la dirección que sube (OP code 0)", arg);
		if (bm ? (blk != b1 || nb != cnt || !(h[0x0C] & 0x20) != (cnt == 1)) : (nb != 1 || blk != total))
			queja("CMD53: el bloque o la cuenta de la controladora no cuadran", (blk << 16) | nb);
		if (!!(h[0x0C] & 0x10) == esc)
			queja("CMD53: la dirección del modo no cuadra con la orden", h[0x0C]);
		if (total > (int)sizeof xbuf) {
			queja("CMD53 demasiado grande", total);
			error_orden();
			return;
		}
		x_activo = 1;
		x_lee = !esc;
		x_bloques = bm ? cnt : 1;
		x_largo = bm ? b1 : total;
		x_total = total;
		x_hechos = x_pos = 0;
		x_dir = (arg >> 9) & 0x1FFFF;
		if (x_lee)
			f1_lee_n(x_dir, xbuf, total);
		responde(0x2000);
		h[0x30] |= x_lee ? 0x20 : 0x10;
		return;
	}
	}
	error_orden();
}

/* el puerto de datos: 4 octetos */
static d32 datos_lee(void)
{
	d32 v = 0;
	int i, k = x_hechos * x_largo + x_pos;

	if (!x_activo || !x_lee) {
		queja("se lee el puerto de datos sin datos", 0);
		return 0xFFFFFFFF;
	}
	for (i = 0; i < 4; i++)
		if (x_pos + i < x_largo)
			v |= (d32)xbuf[k + i] << (8 * i);
	x_pos += 4;
	if (x_pos >= x_largo) {
		x_pos = 0;
		if (++x_hechos >= x_bloques) {
			x_activo = 0;
			h[0x30] |= 0x02;
		} else {
			h[0x30] |= 0x20;
		}
	}
	return v;
}

static void datos_pon(d32 v)
{
	int i, k = x_hechos * x_largo + x_pos;

	if (!x_activo || x_lee) {
		queja("se escribe en el puerto de datos sin datos", 0);
		return;
	}
	for (i = 0; i < 4; i++)
		if (x_pos + i < x_largo)
			xbuf[k + i] = v >> (8 * i);
	x_pos += 4;
	if (x_pos >= x_largo) {
		x_pos = 0;
		if (++x_hechos >= x_bloques) {
			x_activo = 0;
			f1_pon_n(x_dir, xbuf, x_total);
			h[0x30] |= 0x02;
		} else {
			h[0x30] |= 0x10;
		}
	}
}

static int tarjeta_avisa(void)
{
	return (loc[0x14] & 1) && rxq_n > 0 && (f0[0x04] & 3) == 3 && (h[0x35] & 1);
}

static d32 s_lee(d32 r, int ancho)
{
	d32 v = 0;
	int i;

	r &= 0xFF;
	if (r == 0x20 && ancho == 4)
		return datos_lee();
	if (r >= 0x30 && r < 0x34) {
		if (tarjeta_avisa())
			h[0x31] |= 0x01;
		else
			h[0x31] &= ~0x01;
	}
	if (r >= 0x24 && r < 0x28) {
		d32 e = (1u << 20) | (x_activo ? 0x02 : 0) | ((h[0x28] & 0xC0) == 0xC0 ? (1u << 16) : 0);
		pon32(h + 0x24, e);
	}
	for (i = 0; i < ancho; i++)
		v |= (d32)h[(r + i) & 0xFF] << (8 * i);
	return v;
}

static void s_pon(d32 r, int ancho, d32 v)
{
	int i, orden = 0;

	r &= 0xFF;
	if (r == 0x20 && ancho == 4) {
		datos_pon(v);
		return;
	}
	for (i = 0; i < ancho; i++) {
		d32 a = (r + i) & 0xFF;
		o8 b = v >> (8 * i);
		if (a >= 0x30 && a < 0x34) {
			h[a] &= ~b;			/* escribir 1 borra */
		} else if (a == 0x2F) {
			if (b & 1) {
				o8 c = h[0x28];
				sdhci_inicia();
				(void)c;
			}
			if (b & 6)
				x_activo = 0;
			h[a] = 0;
		} else if (a >= 0x40 && a < 0x48) {
			/* las capacidades: de solo lectura */
		} else {
			h[a] = b;
			if (a == 0x2C)
				h[0x2C] = (b & 1) ? (b | 2) : (b & ~2);
			if (a == 0x0F)
				orden = 1;
		}
	}
	if (orden)
		ejecuta();
}

static struct sdio_plat plat;

const struct sdio_plat *rtl_sim_monta(struct radio *punto, void (*e)(d32), d32 (*ms)(void))
{
	int i;
	static const o8 mac[6] = { 0x40, 0x9F, 0x38, 0x8B, 0x23, 0x01 };
	o8 *q = efu;
#define PG(sec, d) do { if ((sec) < 16) *q++ = (o8)(((sec) & 15) << 4); \
			else { *q++ = (((sec) & 7) << 5) | 0x0F; *q++ = (((sec) & 0x78) << 1) | 0; } \
			memcpy(q, d, 8); q += 8; } while (0)
	o8 s[8];

	ap = punto;
	esp = e;
	reloj = ms;
	sim_entrega = llega;
	memset(m, 0, sizeof m);
	memset(loc, 0, sizeof loc);
	memset(rf, 0, sizeof rf);
	memset(efu, 0xFF, sizeof efu);
	rxq_n = 0;
	fw_hasta = 0;
	rtl_sim_fw_bien = rtl_sim_fw_corre = 0;
	rtl_sim_quejas = rtl_sim_nh2c = rtl_sim_lssi_mal = 0;
	rtl_sim_tx = rtl_sim_rx = rtl_sim_rx_tirados = 0;
	m[0x0A] = 0x20;				/* 9346CR: EEPROM_EN (el EFUSE se ha cargado) */
	pon32(m + 0xF0, 0x000110B5);		/* SYS_CFG (corte B) */
	pon32(m + 0x68, 1u << 18);		/* BT_FUNC_EN: hay Bluetooth */
	m[0x86 & 0xFF] = 0;
	loc[0x86] = 1;				/* «suspendida» hasta CARDDIS_TO_CARDEMU */
	/* el EFUSE: las potencias (0x10-0x1B), el cristal (0xB9), la antena
	 * (0xC3: una, la S1) y la MAC (0x11A) */
	s[0] = 0x2A; s[1] = 0x2A; s[2] = 0x2B; s[3] = 0x2B; s[4] = 0x2C; s[5] = 0x2C; s[6] = 0x26; s[7] = 0x26;
	PG(2, s);
	s[0] = 0x27; s[1] = 0x27; s[2] = 0x27; s[3] = 0x02; s[4] = 0xFF; s[5] = 0xFF; s[6] = 0xFF; s[7] = 0xFF;
	PG(3, s);
	memset(s, 0xFF, 8);
	s[1] = 0x22;				/* 0xB9: el cristal */
	PG(23, s);
	memset(s, 0xFF, 8);
	s[1] = 0x00; s[3] = 0x11;		/* 0xC1: la placa; 0xC3: una antena ([0] = 1, Ant_x1), S1 ([6] = 0) */
	PG(24, s);
	memset(s, 0xFF, 8);
	memcpy(s + 2, mac, 6);			/* 0x11A */
	PG(35, s);
	efu[0x1FB] = 0x05;			/* encapsulado TFBGA90 */
	for (i = 0; i < 256; i++)
		rf[i] = 0x33 * i;
	rf[0x18] = 0x0C01;
	tarjeta_inicia();
	sdhci_inicia();
	plat.lee = s_lee;
	plat.pon = s_pon;
	plat.espera_us = e;
	plat.ms = ms;
	return &plat;
}
