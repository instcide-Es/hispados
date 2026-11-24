/*
 * iwl_modelo.c - En el anfitrión: un modelo de una Intel Wireless-AC 7265D
 * (sus registros CSR, la periferia y la SRAM por HBUS, el canal de servicio
 * del FH, la cola de recepción con su rb_stts, la cola de órdenes con sus
 * TFD) y de lo que haría su firmware (ALIVE, el NVM, la calibración con el
 * PHY DB, las respuestas a las órdenes y un escaneo con tres redes), hecho a
 * mano a partir de la especificación de iwlwifi (docs/PENTIUM.md, «El Wi-Fi
 * de Intel»). Contra él corre el controlador de verdad
 * (wifi/nucleo/radio_iwl.c) y, encima, la gestión 802.11 (w80211.c): así se
 * prueba sin la tarjeta todo el protocolo, hasta la lista de redes.
 *
 * Lo que mira el modelo (y cuenta como fallo si no se cumple): que el
 * firmware llegue entero y a su sitio (octeto a octeto, en trozos de como
 * mucho 128 K y con LMPM_CHICK en la zona extendida), el «NIC access» antes
 * de tocar la periferia, la cabecera ancha de cada orden (grupo y largo), el
 * orden de las órdenes (el PHY DB devuelto entero antes de PHY_CFG, DQA antes
 * de la estación auxiliar, MCC_UPDATE antes de escanear), la forma de la
 * orden de escaneo (canales, la plantilla del probe con nuestra MAC) y que
 * el puntero de escritura de la recepción vaya de 8 en 8 sin dejar a la
 * tarjeta dos ranuras con el mismo búfer.
 *
 *   build/iwl_modelo [iwlwifi-7265D-29.ucode]
 *   (sin archivo, con un firmware de mentira hecho aquí; con él, además, con
 *   el de verdad: sus secciones se cargan en el modelo)
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "nucleo.h"
#include "iwl.h"
#include "iwl_fw.h"

int iwl_arranca(const struct iwl_plat *p, d32 id, const struct iwl_fw *fw, struct radio **r);
const char *const *iwl_nombres_para(d32 id, d32 rev);
const struct iwl_plat *iwl_plat_prueba;

/* ---------------------------------------------------------- el entorno */
static d32 reloj_us;
static int detalle;
void hl_log(int nivel, const char *f, ...)
{
	va_list ap;
	if (nivel > 2 && !detalle)
		return;
	printf("  | ");
	va_start(ap, f);
	vprintf(f, ap);
	va_end(ap);
}
unsigned long hl_jiffies(void) { return reloj_us / 1000; }
d32 hl_azar(void) { return rand(); }
void w_entrega_eth(const o8 *t, int n) { (void)t; (void)n; }

static int bien, mal;
static void mira(int c, const char *q)
{
	if (c)
		bien++;
	else {
		mal++;
		printf("FALLO: %s\n", q);
	}
}
static int fallos_modelo;
static void queja(const char *q, d32 a)
{
	fallos_modelo++;
	if (fallos_modelo < 20)
		printf("MODELO: %s (%08X)\n", q, a);
}

/* ------------------------------------------------- la memoria de DMA */
#define DMA_BASE 0x01000000u
#define DMA_TAM (4u << 20)
static o8 *dma_mem;
static d32 dma_usado;
static void *p_dma(d32 tam, d32 *fis)
{
	d32 o = (dma_usado + 4095) & ~4095u;
	if (o + tam > DMA_TAM)
		return NULL;
	dma_usado = o + tam;
	memset(dma_mem + o, 0, tam);
	*fis = DMA_BASE + o;
	return dma_mem + o;
}
static o8 *virt(d32 f, d32 n)
{
	if (f < DMA_BASE || f - DMA_BASE + n > DMA_TAM) {
		queja("DMA fuera de la memoria dada", f);
		return dma_mem;
	}
	return dma_mem + (f - DMA_BASE);
}
static p16 le16(const o8 *p) { return p[0] | (p[1] << 8); }
static d32 le32(const o8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((d32)p[3] << 24); }
static void pon16(o8 *p, p16 v) { p[0] = v; p[1] = v >> 8; }
static void pon32(o8 *p, d32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* ------------------------------------------------------- la tarjeta */
static const o8 MAC[6] = { 0x34, 0xE6, 0xAD, 0x12, 0x34, 0x56 };
static d32 csr[0x2000 / 4];
static d32 int_pend;
static d32 prph_dir, mem_dir;
#define NPRPH 64
static d32 prph_a[NPRPH], prph_v[NPRPH];
static int nprph;
static const struct iwl_fw *fw_esperado;
static o8 *sram;				/* lo cargado (por dirección: de 0 a 0x60000 y la de 0x800000) */
static d32 cargado_bytes, trozo_max;
static int imagen_viva = -1;			/* 0 RT, 1 INIT */
static int cuantos_alive, nic_access_falta;
static int hw_r;				/* la ranura siguiente que llenará la tarjeta */
static int rx_listo;
static int tx_r0;
/* lo que ha pasado con las órdenes */
static int vio_bt, vio_nvm, vio_dqa, vio_aux, vio_mcc, vio_scan, vio_phydb, vio_phydb_mal, vio_phycfg_rt, vio_power;
static int orden_mal, cabecera_mal, wptr_mal;
static int clave_n, clave_sta, clave_pos, clave_fl;	/* el último ADD_STA_KEY */
static o8 clave_k[16];
static d32 escaneo_canales;

static d32 *prph_ref(d32 a)
{
	int i;
	for (i = 0; i < nprph; i++)
		if (prph_a[i] == a)
			return &prph_v[i];
	if (nprph < NPRPH) {
		prph_a[nprph] = a;
		prph_v[nprph] = 0;
		return &prph_v[nprph++];
	}
	return &prph_v[0];
}
static int acceso(void) { return csr[0x24 / 4] & 8; }

/* el PHY DB que da INIT (y que la RT tiene que recibir igual) */
static o8 phydb_ej[5][40];
static int phydb_n[5] = { 30, 34, 6, 6, 12 };
static const int phydb_tipo[5] = { 1, 2, 4, 4, 5 };

/* ---- lo que la tarjeta pone en la cola de recepción ---- */
#define MAXPEND 64
static o8 pend[MAXPEND][1600];
static int pend_n[MAXPEND], pend_c, pend_f;
static void encola(int cmd, int grupo, int seq, const void *d, int n)
{
	o8 *p;
	if ((pend_f + 1) % MAXPEND == pend_c) {
		queja("la cola del modelo se llena", n);
		return;
	}
	p = pend[pend_f];
	pon32(p, (n + 4) & 0x3FFF);
	p[4] = cmd; p[5] = grupo;
	pon16(p + 6, seq);
	memcpy(p + 8, d, n);
	pend_n[pend_f] = 8 + n;
	pend_f = (pend_f + 1) % MAXPEND;
}
static void entrega(void)
{
	while (rx_listo && pend_c != pend_f) {
		int w = csr[0x1BC8 / 4] & 0xFF;
		o8 *rbd = virt(csr[0x1BC4 / 4] << 8, 1024), *rb, *st;
		if (hw_r == w)
			return;			/* sin ranuras: espera a que las dé */
		rb = virt(le32(rbd + 4 * hw_r) << 8, 4096);
		memcpy(rb, pend[pend_c], pend_n[pend_c]);
		pon32(rb + ((pend_n[pend_c] + 63) & ~63), 0x55550000);
		pend_c = (pend_c + 1) % MAXPEND;
		hw_r = (hw_r + 1) & 0xFF;
		st = virt(csr[0x1BC0 / 4] << 4, 16);
		pon16(st, hw_r);
		int_pend |= 0x80000000;
	}
}

static void baliza(int canal, const char *ssid, int rsn, int energia, int oculta)
{
	o8 phy[0x44], m[300];
	int n = 0, l = strlen(ssid);
	memset(phy, 0, sizeof phy);
	pon16(phy + 0x14, 1);
	pon16(phy + 0x16, canal);
	phy[0x1C] = energia;
	phy[0x1D] = energia + 6;
	encola(0xC0, 0, 0x8000, phy, sizeof phy);
	memset(m, 0, sizeof m);
	o8 *t = m + 4;
	t[0] = 0x80;
	memset(t + 4, 0xFF, 6);
	t[10] = 0x00; t[11] = 0x11; t[12] = 0x22; t[13] = 0x33; t[14] = 0x44; t[15] = canal;
	memcpy(t + 16, t + 10, 6);
	n = 24;
	memset(t + n, 0, 8); n += 8;
	t[n++] = 100; t[n++] = 0;
	t[n++] = rsn ? 0x11 : 0x01; t[n++] = 0x04;
	t[n++] = 0; t[n++] = oculta ? 0 : l;
	if (!oculta) { memcpy(t + n, ssid, l); n += l; }
	t[n++] = 1; t[n++] = 4; t[n++] = 0x82; t[n++] = 0x84; t[n++] = 0x8B; t[n++] = 0x96;
	t[n++] = 3; t[n++] = 1; t[n++] = canal;
	if (rsn) {
		static const o8 r[] = { 48, 20, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 4, 1, 0, 0x00, 0x0F, 0xAC, 2, 0, 0 };
		memcpy(t + n, r, sizeof r);
		n += sizeof r;
	}
	pon16(m, n);
	pon32(m + 4 + n, 3);			/* CRC_OK | OVERRUN_OK */
	encola(0xC1, 0, 0x8000, m, 4 + n + 4);
}

/* una orden de la cola 0 */
static void orden(const o8 *c, int total)
{
	int id = c[0], grupo = c[1], seq = le16(c + 2), n = le16(c + 4);
	const o8 *d = c + 8;
	o8 r[400];
	int rn = 4, i;

	memset(r, 0, sizeof r);
	if (n != total - 8 || (grupo != 1 && !(grupo == 5 && id == 0))) {
		cabecera_mal++;
		queja("cabecera ancha mal (grupo o largo)", (grupo << 8) | id);
	}
	switch (id | (grupo == 5 ? 0x500 : 0)) {
	case 0x9B: vio_bt++; break;
	case 0x88: {			/* NVM_ACCESS: el primer trozo */
		int sec = le16(d + 2);
		vio_nvm++;
		pon16(r, le16(d + 4)); pon16(r + 4, sec);
		if (sec == 0) {
			o8 *s = r + 8;
			s[0x2A] = MAC[1]; s[0x2B] = MAC[0]; s[0x2C] = MAC[3]; s[0x2D] = MAC[2]; s[0x2E] = MAC[5]; s[0x2F] = MAC[4];
			pon16(r + 2, 0x60);
			rn = 8 + 0x60;
		} else if (sec == 1) {
			o8 *s = r + 8;
			pon16(s, 0x0C11); pon16(s + 4, 0x0B);
			for (i = 0; i < 14; i++)
				pon16(s + 0x20 + 2 * i, i < 13 ? 0x109 : 0);
			pon16(r + 2, 0x100);
			rn = 8 + 0x100;
		} else {
			pon16(r + 6, 1);
			rn = 8;
		}
		break;
	}
	case 0x98: break;
	case 0x6A:
		if (imagen_viva == 1) {		/* INIT: calibra y da el PHY DB */
			for (i = 4; i >= 0; i--) {
				o8 b[48];
				pon16(b, phydb_tipo[i]);
				pon16(b + 2, phydb_n[i]);
				memcpy(b + 4, phydb_ej[i], phydb_n[i]);
				encola(0x6B, 0, 0x8000, b, 4 + phydb_n[i]);
			}
			encola(0x04, 0, 0x8000, r, 4);
		} else {
			if (vio_phydb != 5)
				queja("PHY_CFG en la RT sin haber devuelto todo el PHY DB", vio_phydb);
			vio_phycfg_rt++;
		}
		break;
	case 0x6C: {
		int t = le16(d), l = le16(d + 2), ok = 0;
		for (i = 0; i < 5; i++)
			if (phydb_tipo[i] == t && phydb_n[i] == l && memcmp(d + 4, phydb_ej[i], l) == 0)
				ok = 1;
		vio_phydb++;
		if (!ok)
			vio_phydb_mal++;
		break;
	}
	case 0x500: vio_dqa++; break;
	case 0x1D:
		if (!vio_dqa)
			queja("la cola auxiliar antes de DQA", 0);
		if (d[1] != 1 || d[3] != 1 || d[6] != 5 || d[4] != 1)
			queja("SCD_QUEUE_CFG de la auxiliar mal", le32(d));
		break;
	case 0x18:
		vio_aux++;
		if (d[0x10] != 1 || le32(d + 4) != 4 || le32(d + 0x28) != 2 || n != 44)
			queja("ADD_STA de la auxiliar mal", n);
		r[0] = 1;			/* ADD_STA_SUCCESS */
		break;
	case 0x17:			/* ADD_STA_KEY (v1, 64, o v2, 76) */
		clave_n = n;
		clave_sta = d[0]; clave_pos = d[1]; clave_fl = le16(d + 2);
		memcpy(clave_k, d + 4, 16);
		r[0] = 1;			/* ADD_STA_SUCCESS */
		break;
	case 0x08: break;
	case 0x77: vio_power++; break;
	case 0xC8:
		vio_mcc++;
		if (le16(d) != 0x5A5A || d[2] != 0x10)
			queja("MCC_UPDATE mal", le32(d));
		pon32(r, 0); r[4] = 'S'; r[5] = 'E';
		pon32(r + 12, 51);
		for (i = 0; i < 51; i++)
			pon32(r + 16 + 4 * i, i < 13 ? 0x109 : (i == 13 ? 0 : 0x101));
		rn = 16 + 4 * 51;
		break;
	case 0x51: {
		const o8 *pr = d + 0x4DC + 16;
		vio_scan++;
		if (!vio_mcc)
			queja("escanear sin MCC_UPDATE (LAR)", 0);
		if (n != 0x6EC || d[4] == 0 || d[4] > 40 || memcmp(pr + 10, MAC, 6) || pr[0] != 0x40 || le32(d + 0x0C) != 0x81
		    || d[0x28] != 1)
			queja("la orden de escaneo no es como debe", n);
		escaneo_canales = d[4];
		pon32(r, 0);
		encola(id, grupo, seq, r, 4);
		baliza(1, "Vecinos", 0, 70, 0);
		baliza(6, "HispaPrueba", 1, 47, 0);
		baliza(11, "", 1, 80, 1);
		{
			o8 f[12] = { 0, 1, 1, 0 };
			encola(0x6D, 0, 0x8000, f, 12);
		}
		return;
	}
	default:
		queja("una orden que el modelo no sabe", (grupo << 8) | id);
	}
	encola(id, grupo, seq, r, rn);
}

static void cola0(int hasta)
{
	o8 *ring = virt(csr[0x19D0 / 4] << 8, 256 * 128);
	while (tx_r0 != hasta) {
		o8 *t = ring + 128 * tx_r0, c[8192];
		int nt = t[3] & 31, k, tot = 0;
		for (k = 0; k < nt; k++) {
			d32 a = le32(t + 4 + 6 * k);
			int l = le16(t + 8 + 6 * k) >> 4;
			if (tot + l > (int)sizeof c) { queja("orden demasiado grande", tot + l); break; }
			memcpy(c + tot, virt(a, l), l);
			tot += l;
		}
		if (le16(c + 2) != tx_r0)
			queja("la secuencia de la orden no es su índice", le16(c + 2));
		orden(c, tot);
		tx_r0 = (tx_r0 + 1) & 0xFF;
	}
}

static int en_imagen(const struct iwl_imagen *im)
{
	int i;
	for (i = 0; i < im->n; i++) {
		const struct iwl_sec *s = &im->sec[i];
		d32 b;
		if (s->destino == IWL_SEP_CPU1_CPU2 || s->destino == IWL_SEP_PAGINACION)
			break;
		b = s->destino >= 0x800000 ? s->destino - 0x800000 + 0x60000 : s->destino;
		if (b + s->tam > 0x80000 || memcmp(sram + b, s->datos, s->tam))
			return 0;
	}
	return 1;
}

static void arranca_cpu(void)
{
	o8 a[0x44];
	if (en_imagen(&fw_esperado->img[IWL_IMG_RT]) && cargado_bytes == iwl_fw_octetos(&fw_esperado->img[IWL_IMG_RT]))
		imagen_viva = 0;
	else if (en_imagen(&fw_esperado->img[IWL_IMG_INIT]) && cargado_bytes == iwl_fw_octetos(&fw_esperado->img[IWL_IMG_INIT]))
		imagen_viva = 1;
	else {
		queja("el firmware no está entero en la SRAM", cargado_bytes);
		return;
	}
	cuantos_alive++;
	memset(a, 0, sizeof a);
	pon16(a, 0xCAFE);
	pon32(a + 4, 29);
	pon32(a + 0x28, 0x00800000);
	*prph_ref(0x02C00) = 0x00800000;
	encola(0x01, 0, 0x8000, a, sizeof a);
	int_pend |= 1;
}

static d32 p_lee(d32 r)
{
	entrega();
	switch (r) {
	case 0x008: return int_pend;
	case 0x028: return 0x218;		/* 7265D, paso 2 */
	case 0x450:
		if (!acceso())
			nic_access_falta++;
		return *prph_ref(prph_dir);
	case 0x1EB0: return 0x00FF0000;
	case 0x1C44: return 0x01000000;
	}
	if (r < 0x2000)
		return csr[r / 4];
	return 0;
}

static void p_pon(d32 r, d32 v)
{
	switch (r) {
	case 0x000:
		csr[0] = v;
		break;
	case 0x008: int_pend &= ~v; return;
	case 0x020:
		csr[0x20 / 4] = v | (v & 0x200 ? 0x100 : 0);
		if (v & 0x80) {			/* reinicio: todo de nuevo */
			imagen_viva = -1;
			rx_listo = 0;
		}
		if (v == 0)
			arranca_cpu();
		return;
	case 0x024:
		csr[0x24 / 4] = (v & ~1u) | 0x08000000 | (v & 4 ? 1 : 0);
		return;
	case 0x444: prph_dir = v & 0xFFFFF; return;
	case 0x448: prph_dir = v & 0xFFFFF; return;
	case 0x44C:
		if (!acceso())
			nic_access_falta++;
		*prph_ref(prph_dir) = v;
		if (prph_dir == 0x02C68)		/* SCD_QUEUE_RDPTR(0): la cola de órdenes, desde ahí */
			tx_r0 = v & 0xFF;
		return;
	case 0x410: mem_dir = v; return;
	case 0x418: mem_dir += 4; return;
	case 0x460: {
		int q = (v >> 8) & 31, i = v & 0xFF;
		if (q == 0 && (*prph_ref(0x02E54) & 1))	/* (solo con la cola activa: SCD_EN_CTRL) */
			cola0(i);
		return;
	}
	case 0x1BC8: {
		int w = v & 0xFF, k, j;
		if (w & 7) {
			wptr_mal++;
			queja("el puntero de recepción no es múltiplo de 8", w);
		}
		/* las ranuras que tiene la tarjeta no pueden compartir búfer */
		if (csr[0x1BC4 / 4] && rx_listo) {
			o8 *rbd = virt(csr[0x1BC4 / 4] << 8, 1024);
			for (k = hw_r; k != w; k = (k + 1) & 0xFF)
				for (j = (k + 1) & 0xFF; j != w; j = (j + 1) & 0xFF)
					if (le32(rbd + 4 * k) == le32(rbd + 4 * j)) {
						wptr_mal++;
						queja("dos ranuras de la tarjeta con el mismo búfer", (hw_r << 16) | (k << 8) | w);
						k = (w + 255) & 0xFF;
						break;
					}
		}
		csr[r / 4] = v;
		return;
	}
	case 0x1C00:
		csr[r / 4] = v;
		if (v & 0x80000000) {
			rx_listo = 1;
			hw_r = 0;
			pend_c = pend_f = 0;
		} else
			rx_listo = 0;
		return;
	case 0x1E20:			/* el canal de servicio */
		csr[r / 4] = v;
		if (v == 0x80100000) {
			d32 dst = csr[0x19C8 / 4], f = csr[0x1948 / 4], l = csr[0x194C / 4] & 0xFFFFF, b;
			if (!acceso())
				nic_access_falta++;
			if (l > 0x20000)
				queja("un trozo de más de 128 K", l);
			if (l > trozo_max)
				trozo_max = l;
			if (dst >= 0x40000 && dst <= 0x57FFF && !(*prph_ref(0x01FF8) & 1))
				queja("la zona extendida sin LMPM_CHICK", dst);
			b = dst >= 0x800000 ? dst - 0x800000 + 0x60000 : dst;
			if (b + l > 0x80000)
				queja("un trozo fuera de la SRAM", dst);
			else
				memcpy(sram + b, virt(f, l), l);
			cargado_bytes += l;
			int_pend |= 0x08000000;
		}
		return;
	case 0x1BC4:
		/* (el anillo nuevo: lo cargado antes ya no cuenta) */
		cargado_bytes = 0;
		break;
	}
	if (r < 0x2000)
		csr[r / 4] = v;
}

static void p_espera(d32 us) { reloj_us += us ? us : 1; }
static d32 p_ms(void) { reloj_us += 50; return reloj_us / 1000; }
static d32 p_pci(int r) { return r == 0 ? 0x095A8086 : 0; }
static void p_pcip(int r, d32 v) { (void)r; (void)v; }
static const struct iwl_plat plat = { p_lee, p_pon, p_dma, p_espera, p_ms, p_pci, p_pcip };

/* ---------------------------------------------- un firmware de mentira */
static o8 fwm[400000];
static int fwn;
static void t32(d32 v) { pon32(fwm + fwn, v); fwn += 4; }
static void tlv(d32 t, const void *d, d32 l)
{
	t32(t); t32(l);
	memcpy(fwm + fwn, d, l);
	fwn += (l + 3) & ~3u;
}
static void seccion(d32 tipo, d32 dst, d32 tam, int semilla)
{
	static o8 b[150000];
	d32 i;
	pon32(b, dst);
	for (i = 0; i < tam; i++)
		b[4 + i] = (o8)(i * 7 + semilla + (i >> 8));
	tlv(tipo, b, tam + 4);
}
static void hace_firmware(void)
{
	d32 v[3];
	fwn = 0;
	t32(0); t32(IWL_MAGIA);
	memset(fwm + fwn, 0, 64); strcpy((char *)fwm + fwn, "modelo de pruebas de Hispa"); fwn += 64;
	t32(29); t32(1); t32(0); t32(0);
	seccion(IWL_TLV_SEC_RT, 0x800000, 0x9000, 1);	/* más que un trozo */
	seccion(IWL_TLV_SEC_RT, 0x000000, 0x12000, 2);
	seccion(IWL_TLV_SEC_RT, 0x040000, 0x3000, 3);	/* la zona extendida */
	seccion(IWL_TLV_SEC_RT, IWL_SEP_CPU1_CPU2, 0, 0);
	seccion(IWL_TLV_SEC_INIT, 0x800000, 0x9000, 11);
	seccion(IWL_TLV_SEC_INIT, 0x000000, 0x10000, 12);
	seccion(IWL_TLV_SEC_INIT, 0x040000, 0x2000, 13);
	seccion(IWL_TLV_SEC_INIT, IWL_SEP_CPU1_CPU2, 0, 0);
	v[0] = 0x00330018; tlv(IWL_TLV_PHY_SKU, v, 4);
	v[0] = 0; v[1] = 0x008C15FB; v[2] = 0x9133; tlv(IWL_TLV_DEF_CALIB, v, 12);
	v[0] = 1; v[1] = 0x008C15FB; v[2] = 0; tlv(IWL_TLV_DEF_CALIB, v, 12);
	v[0] = 1; tlv(IWL_TLV_NUM_OF_CPU, v, 4);
	v[0] = 0; v[1] = 0x0305305A; tlv(IWL_TLV_ENABLED_CAPABILITIES, v, 8);
}

static int una_vez(const struct iwl_fw *fw, const char *que)
{
	struct radio *r;
	struct w_red *l;
	int n, i, vista_v = 0, vista_h = 0, vista_o = 0, e;
	d32 t0;

	printf("-- %s\n", que);
	memset(csr, 0, sizeof csr);
	int_pend = 0; nprph = 0; imagen_viva = -1; cuantos_alive = 0; nic_access_falta = 0;
	vio_bt = vio_nvm = vio_dqa = vio_aux = vio_mcc = vio_scan = vio_phydb = vio_phydb_mal = vio_phycfg_rt = vio_power = 0;
	orden_mal = cabecera_mal = wptr_mal = 0; fallos_modelo = 0; trozo_max = 0; tx_r0 = 0;
	fw_esperado = fw;
	e = iwl_arranca(&plat, 0x095A8086, fw, &r);
	mira(e == 0, "el controlador arranca la tarjeta (INIT, la calibración y la RT)");
	if (e)
		return -1;
	mira(cuantos_alive == 2 && imagen_viva == 0, "dos ALIVE (INIT y RT) y queda viva la RT, cargada entera y a su sitio");
	mira(trozo_max <= 0x20000 && trozo_max > 0, "el firmware, en trozos de 128 K como mucho");
	mira(nic_access_falta == 0, "la periferia y el FH, siempre con el «NIC access» pedido");
	mira(memcmp(r->mac, MAC, 6) == 0, "la MAC, la del NVM (con sus octetos cambiados de dos en dos)");
	mira(vio_nvm >= 2 && vio_bt == 2, "el NVM leído (secciones 0 y 1) y BT_CONFIG en las dos imágenes");
	mira(vio_phydb == 5 && vio_phydb_mal == 0 && vio_phycfg_rt == 1, "el PHY DB devuelto entero a la RT (5 trozos, iguales) antes de PHY_CFG");
	mira(vio_dqa == 1 && vio_aux == 1 && vio_mcc == 1 && vio_power == 1, "DQA, la estación auxiliar, POWER_TABLE y el país (MCC_UPDATE)");
	w80211_inicia(r);
	w80211_barrer();
	t0 = reloj_us;
	while (w80211_barriendo() && reloj_us - t0 < 5000000) {
		r->sondear();
		w80211_tic();
		reloj_us += 10000;
	}
	mira(vio_scan == 1 && escaneo_canales == 13, "un escaneo LMAC de los 13 canales que dice el país");
	n = w80211_redes(&l);
	for (i = 0; i < n; i++) {
		printf("     red: canal %2d, %4d dBm, «%s», seguridad %d\n", l[i].canal, l[i].senal, l[i].ssid, l[i].seguridad);
		if (l[i].canal == 1 && !strcmp((char *)l[i].ssid, "Vecinos") && l[i].senal == -70 && l[i].seguridad == W_SEG_ABIERTA)
			vista_v = 1;
		if (l[i].canal == 6 && !strcmp((char *)l[i].ssid, "HispaPrueba") && l[i].senal == -47 && l[i].seguridad == W_SEG_WPA2)
			vista_h = 1;
		if (l[i].canal == 11 && l[i].ssid_n == 0)
			vista_o = 1;
	}
	mira(vista_v && vista_h, "las redes en la lista, cada una en su canal, con su señal (la mejor antena) y su seguridad");
	mira(vista_o || n == 2, "la oculta: en la lista sin nombre, o fuera");
	mira(cabecera_mal == 0 && wptr_mal == 0, "cabeceras anchas bien y el puntero de recepción de 8 en 8, sin ranuras con el mismo búfer");
	/* (etapa 2, WPA2) las claves a la tarjeta, como las pide w80211 al
	 * conectar y al renovar: la de pares en la posición 0/1 alternando, la
	 * de grupo en la 2 + KeyID, y quitar la vieja */
	{
		o8 tk[16], rsc[6] = { 0 };
		int k;
		for (k = 0; k < 16; k++)
			tk[k] = 0xA0 + k;
		mira(r->clave && r->clave(1, 0, 1, tk, NULL) == 0 && (clave_n == 64 || clave_n == 76) && clave_sta == 0 &&
		     clave_pos == 1 && clave_fl == 0x0002 && memcmp(clave_k, tk, 16) == 0,
		     "ADD_STA_KEY: la clave por pares (CCM, KeyID 0) en la posición 1, del punto de acceso");
		mira(r->clave(0, 2, 0, tk, rsc) == 0 && clave_pos == 4 && clave_fl == (0x4000 | 0x0200 | 0x0002),
		     "ADD_STA_KEY: la de grupo con el KeyID 2, de multidifusión, en la posición 4");
		mira(r->clave(1, 0, 0, NULL, NULL) == 0 && clave_pos == 0 && clave_fl == (0x0800 | 0x0008),
		     "ADD_STA_KEY: quitar la de pares vieja (posición 0, NOT_VALID)");
	}
	mira(fallos_modelo == 0, "nada raro para el modelo");
	r->parar();
	return 0;
}

int main(int argc, char **argv)
{
	static struct iwl_fw fw;
	char e[120];
	int i;

	dma_mem = calloc(1, DMA_TAM);
	sram = calloc(1, 0x80000);
	for (i = 0; i < 5; i++)
		for (int k = 0; k < 40; k++)
			phydb_ej[i][k] = (o8)(i * 31 + k);
	phydb_ej[2][0] = 1; phydb_ej[2][1] = 0;		/* PAPD: el grupo 1 y el 0 */
	phydb_ej[3][0] = 0; phydb_ej[3][1] = 0;
	phydb_ej[4][0] = 0; phydb_ej[4][1] = 0;
	if (getenv("IWL_DETALLE"))
		detalle = 1;
	mira(iwl_nombres_para(0x095A8086, 0x218)[0] && !strcmp(iwl_nombres_para(0x095A8086, 0x218)[0], "iwlwifi-7265D-29.ucode"),
	     "la 7265D (CSR_HW_REV 210h) pide el 7265D-29");
	mira(!strcmp(iwl_nombres_para(0x095A8086, 0x144)[0], "iwlwifi-7265-17.ucode"), "la 7265 sin «D» pide el 7265-17");
	mira(iwl_reconoce(0x095A8086, NULL) == 0 && iwl_reconoce(0xA0F08086, NULL) == 1 && iwl_reconoce(0x816810EC, NULL) < 0,
	     "la 7265 se lleva; la CNVi de Tiger Lake se reconoce pero aún no; una Realtek no es de Intel");
	hace_firmware();
	mira(iwl_fw_lee(fwm, fwn, &fw, e, sizeof e) == 0, "el firmware de mentira se lee");
	una_vez(&fw, "con el firmware de mentira");
	if (argc > 1) {
		FILE *f = fopen(argv[1], "rb");
		static o8 real[4 << 20];
		long n;
		if (!f) { printf("no se puede abrir %s\n", argv[1]); return 1; }
		n = fread(real, 1, sizeof real, f);
		fclose(f);
		mira(iwl_fw_lee(real, n, &fw, e, sizeof e) == 0, "el firmware de verdad se lee");
		memset(sram, 0, 0x80000);
		una_vez(&fw, argv[1]);
	}
	printf("iwl_modelo: el controlador de la Intel 7265 contra el modelo: %d bien, %d fallos\n", bien, mal);
	return mal != 0;
}
