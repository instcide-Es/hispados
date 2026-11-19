/*
 * nucleo.c - Las llamadas del residente (HSPWIFI.COM) al núcleo Wi-Fi de
 * Hispa-486 (HSPWIFI.BIN). El residente entra en modo protegido de 32 bits y
 * llama a nucleo_llamada(función, petición); la petición es un bloque de
 * dobles palabras en la memoria convencional (se da su dirección física).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include "llamadas.h"

int brcm_busca(void);
int brcm_monta(struct radio **r);
int brcm_enciende(void);
void brcm_cuentas(d32 *bien, d32 *mal, int *vel);
int ath_busca(void);
int ath_monta(struct radio **r);
int ath_enciende(void);
void ath_cuentas(d32 *bien, d32 *mal, int *vel);
void ath_resumen(void);

/* Las tarjetas que se saben llevar: cada una con su «radio» (el controlador
 * de Linux que la mueve y lo que lo une a w80211.c). Se usa la primera que
 * se encuentre en el PCI. La gestión 802.11, WPA2 y el residente son los
 * mismos para todas. */
struct conductor {
	const char *familia;
	int (*busca)(void);		/* 0 está, 1 no hay, <0 está pero no se puede usar */
	int (*monta)(struct radio **r);
	int (*enciende)(void);
	void (*cuentas)(d32 *bien, d32 *mal, int *vel);
	void (*resumen)(void);
};
static const struct conductor conductores[] = {
	{ "Broadcom BCM43224/43225 (brcmsmac)", brcm_busca, brcm_monta, brcm_enciende, brcm_cuentas, NULL },
	{ "Atheros AR9285/AR9280/AR9287 (ath9k)", ath_busca, ath_monta, ath_enciende, ath_cuentas, ath_resumen },
};
static const struct conductor *cond;

#ifdef CHIP_SIMULADO
/* núcleo de pruebas (HSPWSIM.BIN): el controlador de verdad contra el modelo
 * de la tarjeta de tests/wifi/tarjeta_sim.c y el punto de acceso de mentira */
#include "tarjeta_sim.h"
static struct radio *ap_sim;
static int (*canal_real)(int);
static void (*sondear_real)(void);
static int canal_sim(int c) { tsim_canal = c; return canal_real(c); }
static void sondear_sim(void) { ap_sim->canal(tsim_canal); ap_sim->sondear(); sondear_real(); }
#endif

extern char _fin_bss[];
static int roto;		/* hubo una excepción: no se toca nada más */
static int listo;		/* la radio está en marcha */
static int simulada;
static struct radio *la_radio;
static struct pbkdf2 pb;

/* cola de tramas Ethernet para arriba */
#define NCOLA 24
static u8 cola[NCOLA][1514];
static u16 cola_n[NCOLA];
static int cola_cab, cola_fin;
static u32 perdidas;

void w_entrega_eth(const o8 *t, int n)
{
	int sig = (cola_fin + 1) % NCOLA;

	if (sig == cola_cab || n > 1514) {
		perdidas++;
		return;
	}
	memcpy(cola[cola_fin], t, n);
	cola_n[cola_fin] = n;
	cola_fin = sig;
}

void nucleo_excepcion(u32 vector, u32 *pila)
{
	static const char *const nombres[] = {
		"división por cero", "depuración", "NMI", "punto de ruptura", "desbordamiento", "límites",
		"instrucción no válida", "sin coprocesador", "doble fallo", "coprocesador", "TSS no válido",
		"segmento ausente", "fallo de pila", "protección general", "fallo de página"
	};
	int con_codigo = vector == 8 || (vector >= 10 && vector <= 14) || vector == 17;

	roto = 1;
	hl_pantalla = 1;
	hl_log(0, "EXCEPCIÓN %u (%s) dentro del núcleo Wi-Fi\n", vector, vector < 15 ? nombres[vector] : "?");
	if (con_codigo)
		hl_log(0, "  código %08X, EIP %08X, CS %04X, indicadores %08X\n", pila[0], pila[1], pila[2] & 0xffff, pila[3]);
	else
		hl_log(0, "  EIP %08X, CS %04X, indicadores %08X\n", pila[0], pila[1] & 0xffff, pila[2]);
	hl_log(0, "  El Wi-Fi queda parado. Mande este registro (WIFI.LOG) para arreglarlo.\n");
}

/* las tarjetas de red del PCI, para el registro */
static void lista_pci(void)
{
	int bus, df;

	for (bus = 0; bus < 256; bus++)
		for (df = 0; df < 256; df++) {
			u32 id = hl_pci_lee(bus, df, 0), clase;
			if (id == 0xffffffff || (id & 0xffff) == 0xffff) {
				if (!(df & 7))
					df += 7;
				continue;
			}
			clase = hl_pci_lee(bus, df, 8);
			if ((clase >> 24) == 0x02 || (id & 0xffff) == 0x14e4 || (id & 0xffff) == 0x168c)
				hl_log(2, "PCI %02x:%02x.%d: %04X:%04X clase %06X rev. %02X\n", bus, df >> 3, df & 7,
				       id & 0xffff, id >> 16, clase >> 8, clase & 0xff);
			if (!(df & 7) && !(hl_pci_lee(bus, df, 0x0c) & 0x800000))
				df += 7;
		}
}

/* Para el registro: ¿ha dejado el firmware puesta la protección de DMA de
 * Intel (VT-d)? Con ella, la tarjeta no puede leer ni escribir la memoria
 * (los paquetes van por DMA): la radio se enciende pero no se oye ninguna
 * red. Pasó con el sonido del MacBook Air 2017 (lib/vtd.inc: de ahí salen
 * los registros). Solo mira; no cambia nada. */
static void mira_vtd(void)
{
	u32 id = hl_pci_lee(0, 0, 0), mch, *m, *u, bar, i;
	static const u16 donde[2] = { 0x5400, 0x5410 };	/* GFXVTBAR y VTDPVC0BAR */

	if ((id & 0xffff) != 0x8086 || (hl_pci_lee(0, 0, 8) >> 16) != 0x0600)
		return;
	mch = hl_pci_lee(0, 0, 0x48);
	if (hl_pci_lee(0, 0, 0x4c) || !(mch & 1) || (mch & 0x7ffe) || (mch & ~0x7fffu) < 0x100000)
		return;		/* sin MCHBAR de 32 KB: equipo de antes de 2011, sin estos registros */
	m = hl_virt(mch & ~0x7fffu);
	for (i = 0; i < 2; i++) {
		bar = m[donde[i] / 4];
		if (!(bar & 1) || m[donde[i] / 4 + 1] || (bar & ~0xfffu) < 0x100000)
			continue;
		u = hl_virt(bar & ~0xfffu);
		if (u[0] < 0x10 || u[0] > 0x7f || u[2] == 0xffffffff || !(u[2] & 0x1f00))
			continue;
		hl_log(2, "VT-d: unidad %s en %08X, versión %u.%u: traducción de DMA %s%s\n",
		       i ? "de los aparatos" : "de la gráfica", bar & ~0xfffu, u[0] >> 4, u[0] & 15,
		       (u[0x1c / 4] & 0x80000000) ? "ENCENDIDA" : "apagada",
		       ((u[2] & 0x60) && (u[0x64 / 4] & 1)) ? ", regiones protegidas PUESTAS" : "");
		if (i && (u[0x1c / 4] & 0x80000000))
			hl_log(0, "VT-d: el firmware ha dejado cerrada la DMA: la tarjeta no podrá recibir ni enviar (mande este registro)\n");
	}
}

static int inicia(struct ll_inicio *p)
{
	u32 fin_nucleo = ((u32)_fin_bss + 4095) & ~4095u, tope, t, u, pk;
	int r;

	hl_base = p->base;
	if (p->base & 0xffff) {
		hl_log(0, "la base del núcleo (%08X) no está alineada a 64 KB\n", p->base);
		return -22;
	}
	hl_nivel = (p->opciones & LL_OP_DETALLE) ? 3 : 2;
	hl_pantalla = (p->opciones & LL_OP_PANTALLA) != 0;
	simulada = (p->opciones & LL_OP_SIMULA) != 0;
	tope = fin_nucleo;
	if (p->fw_tam[0])
		tope = p->fw_des[0] + p->fw_tam[0];
	if (p->fw_tam[1] && p->fw_des[1] + p->fw_tam[1] > tope)
		tope = p->fw_des[1] + p->fw_tam[1];
	tope = (tope + 4095) & ~4095u;
	if (tope + 0x80000 > p->tam) {
		hl_log(0, "el bloque de memoria es pequeño (%u KB)\n", p->tam >> 10);
		return -20;
	}
	hl_monton((void *)tope, p->tam - tope);
	r = hl_tiempo_inicia();
	hl_log(2, "HSPWIFI: núcleo Wi-Fi de Hispa-486, versión %d.%02d (%s)\n", LL_VERSION / 100, LL_VERSION % 100, __DATE__);
	hl_log(2, "Memoria: bloque de %u KB en %08X; núcleo %u KB, montón %u KB\n",
	       p->tam >> 10, p->base, fin_nucleo >> 10, (p->tam - tope) >> 10);
#ifndef ANFITRION
	if (hl_pantalla) {
		/* ¿llega a la pantalla lo que se pinta con /PANTALLA? */
		volatile u16 *v = hl_virt(0xB8000 + 3998);
		u16 era = *v, a, b;
		*v = 0x075a; a = *v;
		*v = 0x07a5; b = *v;
		*v = era;
		hl_log(2, "Pantalla (/PANTALLA): la memoria de vídeo de texto (B8000) %s\n",
		       a == 0x075a && b == 0x07a5 ? "guarda lo escrito" : "NO guarda lo escrito: no se verá nada");
	}
#endif
	if (r < 0) {
		hl_log(0, "este procesador no tiene contador de tiempo (TSC) o no se puede medir (%d)\n", r);
		return -21;
	}
	hl_log(2, "Procesador: %u MHz (TSC)\n", hl_tsc_khz() / 1000);
	if (simulada) {
		hl_log(2, "RADIO SIMULADA: red «%s», canal 6 (para probar sin la tarjeta)\n", "HispaPrueba");
		sim_configura("HispaPrueba", "clave de prueba", 6);
		la_radio = sim_radio();
		w80211_inicia(la_radio);
		listo = 1;
		return 0;
	}
#ifdef CHIP_SIMULADO
	hl_log(2, "NÚCLEO DE PRUEBAS: tarjeta BCM43224 simulada, red «HispaPrueba» en el canal 6\n");
	sim_configura("HispaPrueba", "clave de prueba", 6);
	ap_sim = sim_radio();
	tarjeta_sim_monta(ap_sim);
#endif
	hl_fw_pon(0, "brcm/bcm43xx-0.fw", (const u8 *)p->fw_des[0], p->fw_tam[0]);
	hl_fw_pon(1, "brcm/bcm43xx_hdr-0.fw", (const u8 *)p->fw_des[1], p->fw_tam[1]);
	lista_pci();
	mira_vtd();
	for (r = 1, t = 0; t < ARRAY_SIZE(conductores) && r == 1; t++) {
		r = conductores[t].busca();
		if (r == 0) {
			cond = &conductores[t];
			hl_log(2, "Se usa la radio de las %s\n", cond->familia);
		}
	}
	if (r == 1) {
		hl_log(0, "no hay ninguna tarjeta Wi-Fi de las admitidas:\n");
		for (t = 0; t < ARRAY_SIZE(conductores); t++)
			hl_log(0, "  %s\n", conductores[t].familia);
		return -1;
	}
	if (r == 0)
		hl_log(2, "PASO 1 de 3 hecho: tarjeta encontrada. Sigue el 2: despertarla, leer sus datos de fábrica y montar el controlador\n");
	return r;
}

static int monta(void)
{
	u32 t, u, pk;
	int r;

	if (simulada)
		return 0;
	if (!cond)
		return -1;
	r = cond->monta(&la_radio);
	hl_monton_info(&t, &u, &pk);
	if (r == 0)
		hl_log(2, "PASO 2 de 3 hecho: controlador montado (montón: %u KB). Sigue el 3: encender la radio\n", u >> 10);
	return r;
}

static int enciende(void)
{
	u32 t, u, pk;
	int r;

	if (simulada)
		return 0;
	if (!la_radio)
		return -1;
	r = cond->enciende();
	if (r < 0)
		return r;
#ifdef CHIP_SIMULADO
	canal_real = la_radio->canal; la_radio->canal = canal_sim;
	sondear_real = la_radio->sondear; la_radio->sondear = sondear_sim;
#endif
	w80211_inicia(la_radio);
	listo = 1;
	hl_monton_info(&t, &u, &pk);
	if (hl_monton_repasa() < 0)
		return -23;
	hl_log(2, "PASO 3 de 3 hecho: radio en marcha (montón: %u KB, pico %u KB de %u)\n", u >> 10, pk >> 10, t >> 10);
	return 0;
}

static int llamada(u32 fn, u32 dir);

int nucleo_llamada(u32 fn, u32 dir)
{
	/* (el puerto CF8h, como estaba: ver hl_pci_dir en base.c) */
	u32 cf8 = hl_pci_dir();
	int r = llamada(fn, dir);

	hl_pci_dir_pon(cf8);
	return r;
}

static int llamada(u32 fn, u32 dir)
{
	u32 *p;

	/* en la primera llamada aún no se sabe la base: la trae EBX (entrada.asm
	 * la deja en hl_base antes de llamar aquí) */
	p = hl_virt(dir);
	if (fn == LL_REGISTRO)
		return hl_reg_lee(hl_virt(p[0]), p[1]);
	if (roto)
		return -99;
	switch (fn) {
	case LL_INICIA:
		return inicia((struct ll_inicio *)p);
	case LL_MONTA:
		return monta();
	case LL_ENCIENDE:
		return enciende();
	case LL_SONDEA:
		if (listo) {
			la_radio->sondear();
			w80211_tic();
		}
		return (cola_cab != cola_fin ? 1 : 0) | (hl_reg_hay() ? 2 : 0);
	case LL_RECIBE: {
		int n;
		if (cola_cab == cola_fin)
			return 0;
		n = cola_n[cola_cab];
		memcpy(hl_virt(p[0]), cola[cola_cab], n);
		cola_cab = (cola_cab + 1) % NCOLA;
		return n;
	}
	case LL_ENVIA:
		if (!listo)
			return -1;
		return w80211_enviar_eth(hl_virt(p[0]), p[1]);
	case LL_BARRE:
		if (!listo)
			return -1;
		w80211_barrer();
		return 0;
	case LL_REDES: {
		struct w_red *l;
		struct ll_red *s = hl_virt(p[0]);
		int n, i;
		if (!listo)
			return 0;
		n = w80211_redes(&l);
		if (n > (int)p[1])
			n = p[1];
		for (i = 0; i < n; i++, s++) {
			memcpy(s->bssid, l[i].bssid, 6);
			s->canal = l[i].canal;
			s->senal = l[i].senal;
			s->seguridad = l[i].seguridad;
			s->ssid_n = l[i].ssid_n;
			memcpy(s->ssid, l[i].ssid, 32);
		}
		return n;
	}
	case LL_CONECTA:
		if (!listo)
			return -1;
		w80211_conectar(hl_virt(p[0]), p[1], p[2] ? hl_virt(p[2]) : NULL);
		return 0;
	case LL_DESCONECTA:
		if (!listo)
			return -1;
		w80211_desconectar();
		return 0;
	case LL_ESTADO: {
		struct w_info *i = hl_virt(p[0]);
		if (!listo) {
			memset(i, 0, sizeof(*i));
			return -1;
		}
		w80211_estado(i);
		if (p[2] == 1) {
			/* para el registro: cómo está todo */
			u32 t, u, pk;
			d32 b = 0, m = 0;
			int v = 0;
			static const char *const est[] = { "parado", "buscando", "autenticando", "asociando", "claves", "CONECTADO" };
			if (cond)
				cond->cuentas(&b, &m, &v);
			hl_monton_info(&t, &u, &pk);
			if (hl_monton_repasa() == 0)
				hl_log(2, "ESTADO: el montón está entero\n");
			hl_log(2, "ESTADO: %s, fallo %d, red «%s», canal %d, señal %d dBm, seguridad %d\n",
			       est[i->estado], i->fallo, i->ssid, i->canal, i->senal, i->seguridad);
			hl_log(2, "ESTADO: enviados %u, recibidos %u, sin acuse %u, descartados %u, reconexiones %u, cola llena %u\n",
			       i->tx, i->rx, i->tx_err, i->rx_desc, i->reconexiones, perdidas);
			hl_log(2, "ESTADO: radio: acuses %u, sin acuse %u, velocidad %d,%d Mbit/s; montón %u KB (pico %u); paquetes vivos %u\n",
			       b, m, v / 2, (v & 1) * 5, u >> 10, pk >> 10, hl_skb_vivos());
			if (cond && cond->resumen)
				cond->resumen();
		}
		if (cond && p[1] >= sizeof(*i) + 12) {
			/* detrás: aciertos y fallos de envío y la velocidad */
			d32 x[3];
			int v;
			cond->cuentas(&x[0], &x[1], &v);
			x[2] = v;
			memcpy(i + 1, x, 12);
		} else if (p[1] >= sizeof(*i) + 12) {
			memset(i + 1, 0, 12);
		}
		return 0;
	}
	case LL_PMK_EMPIEZA:
		pbkdf2_empieza(&pb, hl_virt(p[0]), hl_virt(p[1]), p[2]);
		return 0;
	case LL_PMK_SIGUE:
		return pbkdf2_sigue(&pb, 128, hl_virt(p[0]));
	case LL_PARA:
		if (listo && la_radio->parar)
			la_radio->parar();
		listo = 0;
		return 0;
	case LL_MAC:
		if (!la_radio)
			return -1;
		memcpy(hl_virt(p[0]), la_radio->mac, 6);
		return 0;
	case LL_NOMBRE: {
		/* el nombre de la tarjeta, acabado en 0 */
		char *d = hl_virt(p[0]);
		const char *n = la_radio && la_radio->nombre ? la_radio->nombre : "?";
		u32 k;
		if (!p[1])
			return -1;
		for (k = 0; k + 1 < p[1] && n[k]; k++)
			d[k] = n[k];
		d[k] = 0;
		return k;
	}
	}
	return -98;
}
