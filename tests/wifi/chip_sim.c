/*
 * chip_sim.c - Prueba en el anfitrión del controlador de la BCM43224 SIN la
 * tarjeta: los fuentes de brcmsmac y bcma, la capa de compatibilidad
 * (wifi/compat) y la radio (wifi/nucleo/radio_brcm.c) corren aquí contra un
 * MODELO de la tarjeta: su configuración PCI, los núcleos del bus interno
 * (ChipCommon, 802.11 y PCIe, con su EROM y una SPROM de revisión 8), los
 * registros del núcleo 802.11 (PHY, radio, memoria compartida) y los anillos
 * de DMA de envío y recepción. Lo que la «tarjeta» emite llega al punto de
 * acceso de mentira de wifi/nucleo/sim.c y lo que éste contesta entra por el
 * anillo de recepción: así se prueban de punta a punta la puesta en marcha
 * (con el microcódigo de verdad), la búsqueda, WPA2 y los datos.
 *
 * OJO: el modelo dice lo que el controlador ESPERA de la tarjeta, no lo que
 * la tarjeta hace: prueba nuestro código, no el hardware.
 *
 * Es un programa de Linux de 32 bits sin biblioteca (no hay libc de 32 bits
 * en el equipo de compilación): entra por _start y usa int 80h.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "hispa_linux.h"
#include "base.h"
#include "nucleo.h"
#include <defs.h>
#include "types.h"
#include "d11.h"

extern unsigned long hl_reloj_falso;
int brcm_monta(struct radio **r);
int brcm_enciende(void);
void brcm_cuentas(d32 *bien, d32 *mal, int *vel);
extern int sim_renovar_grupo;

/* ------------------------------------------------ lo mínimo de Linux */
static long llamada(long n, long a, long b, long c)
{
	long r;
	__asm__ __volatile__("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}
static void escribe(const char *s, int n) { llamada(4, 1, (long)s, n); }
static void sal(int c) { llamada(1, c, 0, 0); for (;;) ; }

static int charla;
static void vuelca_registro(void)
{
	static char b[4096];
	int n;
	while ((n = hl_reg_lee(b, sizeof(b))) > 0)
		if (charla)
			escribe(b, n);
}

static int fallos, hechas;
static void di(const char *s) { escribe(s, strlen(s)); }
static void comprueba(int c, const char *que)
{
	hechas++;
	vuelca_registro();
	if (!c) {
		fallos++;
		di("  FALLA: "); di(que); di("\n");
	} else if (charla) {
		di("  bien: "); di(que); di("\n");
	}
}

#include "tarjeta_sim.h"
#define cfg tsim_cfg
#define canal_radio tsim_canal
#define tramas_tx tsim_tx
#define tramas_rx tsim_rx
#define tx_malas tsim_tx_malas
#define accesos tsim_accesos
static struct radio *ap;
static u32 le(const u8 *m, int off, int tam) { u32 v = 0; memcpy(&v, m + off, tam); return v; }
extern u8 r_d11_sim[];

/* ------------------------------------------------------------ la prueba */
static struct radio *r;
static int (*canal_de_verdad)(int);
static o8 ultima[1600];
static int ultima_n, entregadas;

void w_entrega_eth(const o8 *t, int n) { memcpy(ultima, t, n); ultima_n = n; entregadas++; }

static int mi_canal(int c)
{
	canal_radio = c;
	return canal_de_verdad(c);
}

static void corre(int ms)
{
	while (ms > 0) {
		hl_reloj_falso += 5;
		ms -= 5;
		ap->canal(canal_radio);
		ap->sondear();		/* las balizas del punto de acceso, al anillo */
		r->sondear();
		w80211_tic();
	}
	vuelca_registro();
}

static int espera_estado(int e, int ms)
{
	struct w_info i;
	while (ms > 0) {
		corre(10);
		ms -= 10;
		w80211_estado(&i);
		if (i.estado == e)
			return 1;
	}
	return 0;
}

static int manda_arp(void)
{
	o8 t[60];
	memset(t, 0, sizeof(t));
	memset(t, 255, 6);
	memcpy(t + 6, r->mac, 6);
	t[12] = 8; t[13] = 6;
	t[14] = 0; t[15] = 1; t[16] = 8; t[17] = 0; t[18] = 6; t[19] = 4; t[20] = 0; t[21] = 1;
	memcpy(t + 22, r->mac, 6);
	t[28] = 192; t[29] = 168; t[30] = 77; t[31] = 10;
	t[38] = 192; t[39] = 168; t[40] = 77; t[41] = 1;
	return w80211_enviar_eth(t, 60);
}

/* un eco de ICMP de n octetos a 192.168.77.1 */
static int manda_eco(int n)
{
	static const o8 ap_mac[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x01 };
	static o8 t[1514];
	d32 s = 0;
	int i, l = 20 + 8 + n;

	memset(t, 0, sizeof(t));
	memcpy(t, ap_mac, 6);
	memcpy(t + 6, r->mac, 6);
	t[12] = 8; t[13] = 0;
	t[14] = 0x45; t[16] = l >> 8; t[17] = l; t[22] = 64; t[23] = 1;
	t[26] = 192; t[27] = 168; t[28] = 77; t[29] = 10;
	t[30] = 192; t[31] = 168; t[32] = 77; t[33] = 1;
	t[34] = 8;
	for (i = 0; i < n; i++)
		t[42 + i] = i * 7;
	for (i = 34; i < 42 + n; i += 2)
		s += (t[i] << 8) | t[i + 1];
	while (s >> 16)
		s = (s & 0xffff) + (s >> 16);
	s = ~s;
	t[36] = s >> 8; t[37] = s;
	return w80211_enviar_eth(t, 14 + l);
}

extern const u8 fw_ini[], fw_fin[], fwh_ini[], fwh_fin[];
__asm__(".section .rodata\n"
	".globl fw_ini, fw_fin, fwh_ini, fwh_fin\n"
	"fw_ini: .incbin \"wifi/firmware/bcm43xx-0.fw\"\nfw_fin:\n"
	"fwh_ini: .incbin \"wifi/firmware/bcm43xx_hdr-0.fw\"\nfwh_fin:\n"
	".text\n");

static char monton[3 * 1024 * 1024];

static void prueba(void)
{
	struct w_info i;
	struct w_red *l;
	struct pbkdf2 p;
	o8 pmk[32];
	d32 bien, mal;
	u32 t, u, pk;
	int k, n, vel;

	hl_base = 0;
	hl_nivel = 3;
	hl_monton(monton, sizeof(monton));
	hl_tiempo_inicia();
	hl_reloj_falso = 1000;
	sim_configura("Biblioteca", "clave de la prueba", 6);
	ap = sim_radio();
	tarjeta_sim_monta(ap);
	hl_fw_pon(0, "brcm/bcm43xx-0.fw", fw_ini, fw_fin - fw_ini);
	hl_fw_pon(1, "brcm/bcm43xx_hdr-0.fw", fwh_ini, fwh_fin - fwh_ini);

	k = brcm_monta(&r);
	comprueba(k == 0, "se encuentra la tarjeta y se monta el controlador");
	if (k)
		return;
	comprueba(r->mac[0] == 0x00 && r->mac[1] == 0x26 && r->mac[5] == 0x56, "la MAC sale de la SPROM");
	comprueba((cfg[5] & 4) != 0 && (cfg[4] & 6) == 6, "PCI: memoria y bus master encendidos, interrupción apagada");
	comprueba((cfg[0x44] & 3) == 0, "PCI: la tarjeta dormida (D3) se despierta");
	k = brcm_enciende();
	comprueba(k == 0, "la radio se enciende (microcódigo, PHY N)");
	if (k)
		return;
	canal_de_verdad = r->canal;
	r->canal = mi_canal;
	w80211_inicia(r);
	corre(1200);			/* que pase el vigilante de un segundo */
	comprueba(1, "un segundo en marcha (vigilante y calibración periódica)");

	/* búsqueda */
	w80211_barrer();
	corre(13 * 130 + 100);
	n = w80211_redes(&l);
	comprueba(n == 3, "búsqueda: se ven las tres redes (por el anillo de recepción)");
	for (k = 0; k < n; k++)
		if (l[k].ssid_n == 10)
			comprueba(l[k].canal == 6 && l[k].seguridad == W_SEG_WPA2, "búsqueda: «Biblioteca» en el canal 6 con WPA2");
	comprueba(tramas_tx >= 11 && tx_malas == 0, "búsqueda: los sondeos salen por el anillo de envío, bien formados");

	/* WPA2 */
	pbkdf2_empieza(&p, "clave de la prueba", (const o8 *)"Biblioteca", 10);
	while (!pbkdf2_sigue(&p, 4096, pmk))
		;
	w80211_conectar((const o8 *)"Biblioteca", 10, pmk);
	comprueba(espera_estado(W_CONECTADO, 5000), "conecta con WPA2 a través del controlador");
	w80211_estado(&i);
	comprueba(i.canal == 6, "queda en el canal 6");
	entregadas = 0;
	comprueba(manda_arp() == 0, "ARP cifrado");
	corre(60);
	comprueba(entregadas == 1 && ultima[21] == 2, "llega la respuesta de ARP");
	/* tramas grandes y muchas: la cola de envío no se atasca ni se pierde memoria */
	entregadas = 0;
	for (k = 0; k < 300; k++) {
		manda_eco(k % 3 == 0 ? 1400 : 56);
		corre(10);
	}
	comprueba(entregadas == 300, "300 ecos (pequeños y de 1400 octetos) van y vuelven");
	comprueba(ultima_n == 14 + 28 + 56 || ultima_n == 14 + 28 + 1400, "el último eco, con su tamaño");
	brcm_cuentas(&bien, &mal, &vel);
	comprueba(bien >= 300 && tx_malas == 0, "acuses de las tramas enviadas");
	comprueba(vel == 108, "la velocidad sube hasta 54 Mbit/s con los acuses");
	sim_renovar_grupo = 1;
	corre(100);
	entregadas = 0;
	manda_arp();
	corre(60);
	comprueba(entregadas == 1, "tras renovar la clave de grupo, sigue");
	corre(3000);
	comprueba(hl_skb_vivos() < 80, "no se quedan paquetes sin soltar");
	hl_monton_info(&t, &u, &pk);
	comprueba(pk < 2 * 1024 * 1024, "el montón no pasa de 2 MB");
	w80211_desconectar();
	corre(100);
	r->parar();
	comprueba((cfg[4] & 4) == 0, "al parar, la tarjeta deja el bus");
	hl_log(2, "PRUEBA: accesos a registros %u, tramas enviadas %u, recibidas %u; montón %u KB (pico %u KB)\n",
	       accesos, tramas_tx, tramas_rx, u >> 10, pk >> 10);
}

char _fin_bss[4];

void entrada_c(long *pila)
{
	char b[64];

	charla = pila[0] > 1;
	prueba();
	vuelca_registro();
	snprintf(b, sizeof(b), "Wi-Fi (chip simulado): %d comprobaciones, %d fallos\n", hechas, fallos);
	di(b);
	sal(fallos != 0);
}

__asm__(".globl _start\n_start:\n\tmovl %esp, %eax\n\tandl $-16, %esp\n\tsubl $12, %esp\n\tpushl %eax\n\tcall entrada_c\n");
