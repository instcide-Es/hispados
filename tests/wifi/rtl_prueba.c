/*
 * rtl_prueba.c - En el anfitrión: el controlador de la Realtek RTL8723BS
 * (wifi/nucleo/radio_rtl.c) y el SDIO (wifi/nucleo/sdio.c) contra el modelo
 * de la controladora SDHCI, la tarjeta SDIO y el chip (tests/wifi/rtl_sim.c),
 * con la gestión 802.11 y WPA2 de siempre (w80211.c, wpa.c) encima y el punto
 * de acceso de mentira (sim.c) detrás: encender, cargar el firmware, leer
 * el EFUSE, buscar redes, conectar con WPA2, ARP y DHCP, renovar las claves.
 * Y lo que tiene que salir mal: el chip apagado (no contesta a CMD5), un
 * archivo que no es el firmware, sin firmware.
 *
 *   build/rtl_prueba [rtl8723bs_nic.bin]
 *   (sin archivo, con un firmware de mentira hecho aquí; con él, el de
 *   verdad, que el modelo compara octeto a octeto con lo que le llega)
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "nucleo.h"
#include "sdio.h"
#include "rtl.h"
#include "rtl_sim.h"

extern int sim_renovar_pares, sim_renovar_grupo, sim_renov_ptk, sim_renov_gtk;

static unsigned long long reloj_us;
static int charla;
static char ultimo_registro[64][400];
static int n_registro;

void hl_log(int nivel, const char *f, ...)
{
	va_list a;
	char b[400];

	va_start(a, f);
	vsnprintf(b, sizeof b, f, a);
	va_end(a);
	if (charla || nivel == 0)
		printf("    [%llu] %s", reloj_us / 1000, b);
	snprintf(ultimo_registro[n_registro % 64], 400, "%s", b);
	n_registro++;
}
unsigned long hl_jiffies(void) { return reloj_us / 1000; }
d32 hl_azar(void) { return rand(); }
static void espera_us(d32 us) { reloj_us += us; }
static d32 ms(void) { return reloj_us / 1000; }

static int entregadas, ultima_n;
static o8 ultima[1600];
void w_entrega_eth(const o8 *t, int n)
{
	entregadas++;
	ultima_n = n < (int)sizeof ultima ? n : (int)sizeof ultima;
	memcpy(ultima, t, ultima_n);
}

static int bien, mal;
static void comprueba(int c, const char *que)
{
	if (c) {
		bien++;
		printf("  bien  %s\n", que);
	} else {
		mal++;
		printf("  FALLA %s\n", que);
	}
}
static int dijo(const char *trozo)
{
	int i;
	for (i = n_registro > 64 ? n_registro - 64 : 0; i < n_registro; i++)
		if (strstr(ultimo_registro[i % 64], trozo))
			return 1;
	return 0;
}

static struct radio *ap;

static void corre(int cuantos_ms)
{
	while (cuantos_ms-- > 0) {
		ap->canal(rtl_canal);
		ap->sondear();
		radio->sondear();
		w80211_tic();
		reloj_us += 1000;
	}
}

static int espera_estado(int e, int cuantos_ms)
{
	struct w_info i;
	while (cuantos_ms > 0) {
		corre(10);
		cuantos_ms -= 10;
		w80211_estado(&i);
		if (i.estado == e)
			return 1;
	}
	return 0;
}

static void pmk_de(const char *clave, const char *ssid, o8 *pmk)
{
	struct pbkdf2 p;
	pbkdf2_empieza(&p, clave, (const o8 *)ssid, strlen(ssid));
	while (!pbkdf2_sigue(&p, 4096, pmk))
		;
}

static const o8 mac_ap[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x01 };

static int manda_arp(void)
{
	o8 t[60];
	memset(t, 0, sizeof(t));
	memset(t, 255, 6);
	memcpy(t + 6, radio->mac, 6);
	t[12] = 8; t[13] = 6;
	t[14] = 0; t[15] = 1; t[16] = 8; t[17] = 0; t[18] = 6; t[19] = 4; t[20] = 0; t[21] = 1;
	memcpy(t + 22, radio->mac, 6);
	t[28] = 192; t[29] = 168; t[30] = 77; t[31] = 10;
	t[38] = 192; t[39] = 168; t[40] = 77; t[41] = 1;
	return w80211_enviar_eth(t, 60);
}

static int manda_dhcp(void)
{
	static o8 t[342];
	memset(t, 0, sizeof(t));
	memset(t, 255, 6);
	memcpy(t + 6, radio->mac, 6);
	t[12] = 8; t[13] = 0;
	t[14] = 0x45; t[16] = 328 >> 8; t[17] = 328 & 255; t[22] = 64; t[23] = 17;
	memset(t + 30, 255, 4);
	t[34] = 0; t[35] = 68; t[36] = 0; t[37] = 67; t[38] = 308 >> 8; t[39] = 308 & 255;
	t[42] = 1; t[43] = 1; t[44] = 6; t[46] = 0x12; t[47] = 0x34;
	memcpy(t + 42 + 28, radio->mac, 6);
	t[42 + 236] = 99; t[42 + 237] = 130; t[42 + 238] = 83; t[42 + 239] = 99;
	t[42 + 240] = 53; t[42 + 241] = 1; t[42 + 242] = 1; t[42 + 243] = 255;
	return w80211_enviar_eth(t, 342);
}

int main(int argc, char **argv)
{
	static o8 fw[0x8000];
	d32 nfw = 0;
	const struct sdio_plat *p;
	struct w_info i;
	struct w_red *l;
	o8 pmk[32], basura[64];
	int e, n, k, vio_red = 0, vio_abierta = 0, vio_vieja = 0, h65 = 0, h60 = 0, h6e = 0;
	unsigned long long t0;

	charla = getenv("CHARLA") != NULL;
	if (argc > 1) {
		FILE *f = fopen(argv[1], "rb");
		if (!f) {
			printf("no se puede abrir %s\n", argv[1]);
			return 2;
		}
		nfw = fread(fw, 1, sizeof fw, f);
		fclose(f);
		printf("== RTL8723BS con el firmware de verdad (%s, %u octetos) ==\n", argv[1], nfw);
	} else {
		/* uno de mentira: la cabecera de la 8723B (5301h, versión 35) y octetos */
		nfw = 32108;
		for (k = 0; k < (int)nfw; k++)
			fw[k] = (o8)(k * 131 + (k >> 7));
		memset(fw, 0, 32);
		fw[0] = 0x01; fw[1] = 0x53; fw[2] = 0x10; fw[4] = 0x23;
		printf("== RTL8723BS con un firmware de mentira (%u octetos) ==\n", nfw);
	}

	/* lo que no es el firmware */
	memset(basura, 0x41, sizeof basura);
	comprueba(rtl_pon_fw(basura, sizeof basura, "x") < 0, "un archivo que no es el firmware no se acepta");
	comprueba(rtl_pon_fw(fw, nfw, "rtl8723bs_nic.bin") == 0 && rtl_es_fw(fw, nfw), "el firmware se acepta (cabecera 53xxh)");

	/* el chip apagado: no contesta a CMD5 */
	sim_configura("HispaPrueba", "clave de prueba", 6);
	ap = sim_radio();
	rtl_sim_apagado = 1;
	p = rtl_sim_monta(ap, espera_us, ms);
	e = rtl_arranca(p, &radio);
	comprueba(e == -2 && dijo("no contesta a CMD5"), "con el chip apagado: «no contesta a CMD5» (y no sigue)");
	comprueba(rtl_sim_cd_forzada, "la ranura sin patilla de «hay tarjeta»: se fuerza (CD test)");

	/* el de verdad: SDIO, encender y el EFUSE */
	rtl_sim_apagado = 0;
	n_registro = 0;
	p = rtl_sim_monta(ap, espera_us, ms);
	rtl_sim_fw(fw, nfw);
	t0 = reloj_us;
	e = rtl_arranca(p, &radio);
	comprueba(e == 0, "SDIO: CMD5, CMD3, CMD7, el CCCR, el CIS y la función 1 con bloques de 512");
	comprueba(dijo("fabricante 024C, tarjeta B723"), "el CIS: Realtek (024C), 8723BS (B723)");
	comprueba(dijo("lista: 4 bits, 50000 kHz (alta velocidad)"), "4 bits y alta velocidad (50 MHz)");
	comprueba(e == 0 && radio->mac[0] == 0x40 && radio->mac[1] == 0x9F && radio->mac[5] == 0x01, "la MAC, del EFUSE (40:9F:38:8B:23:01)");
	comprueba(dijo("cristal 22") && dijo("antena S1 (la principal) (una, compartida), Bluetooth en el chip"), "el EFUSE: el cristal, la antena y el Bluetooth");
	comprueba(dijo("potencias CCK 2A, OFDM 26 (dif. 2)"), "el EFUSE: las potencias");
	if (e)
		return 1;
	/* sin firmware no se enciende (se prueba antes de dárselo... aquí ya lo
	 * tiene: el caso lo mira servicio.c, que no llama a rtl_enciende) */
	e = rtl_enciende();
	comprueba(e == 0, "se enciende: el firmware, el MAC, el BB, la radio, las colas, el LLT");
	comprueba(rtl_sim_fw_bien && rtl_sim_fw_corre, "el firmware llega entero, en su sitio y por páginas, y el 8051 lo arranca");
	comprueba(dijo("firmware 35.0 (firma 5301"), "WIFI.LOG dice la versión del firmware (35.0)");
	comprueba(rtl_sim_lssi_mal == 0, "la radio se escribe siempre con 32 bits (CMD53), nunca de octeto en octeto");
	for (k = 0; k < rtl_sim_nh2c && k < 8; k++) {
		h65 |= rtl_sim_h2c[k] == 0x65;
		h60 |= rtl_sim_h2c[k] == 0x60;
		h6e |= rtl_sim_h2c[k] == 0x6E;
	}
	comprueba(h65 && h60 && h6e, "la antena compartida con el Bluetooth, para el Wi-Fi (órdenes 65h, 60h y 6Eh al firmware)");
	comprueba(rtl_canal == 1 && rtl_sim_canal == 1, "la radio, en el canal 1");
	comprueba(dijo("tras su tabla: 00h 10000") && dijo("51h 6B10E"),
		  "la tabla de la radio con sus condiciones (00h en espera, no a 0; 51h la del encapsulado TFBGA90)");
	comprueba(dijo("prueba de la antena 5") && dijo("la antena: se queda la 0"),
		  "la prueba de las antenas al encender (6 maneras; se queda la de Linux si oyen igual)");
	printf("  (encender: %llu ms de reloj; %d quejas del modelo hasta aquí)\n", (reloj_us - t0) / 1000, rtl_sim_quejas);

	/* buscar redes */
	w80211_inicia(radio);
	w80211_barrer();
	comprueba(w80211_barriendo(), "la búsqueda empieza");
	corre(13 * 130 + 200);
	comprueba(!w80211_barriendo(), "la búsqueda acaba (13 canales)");
	n = w80211_redes(&l);
	for (k = 0; k < n; k++) {
		if (!strcmp((char *)l[k].ssid, "HispaPrueba") && l[k].canal == 6 && l[k].seguridad == W_SEG_WPA2 && l[k].senal < -40 && l[k].senal > -55)
			vio_red = 1;
		if (!strcmp((char *)l[k].ssid, "Vecina abierta") && l[k].canal == 11 && l[k].seguridad == W_SEG_ABIERTA)
			vio_abierta = 1;
		if (!strcmp((char *)l[k].ssid, "Vecina vieja") && l[k].canal == 3 && l[k].seguridad == W_SEG_WPA)
			vio_vieja = 1;
	}
	comprueba(vio_red, "se ve «HispaPrueba»: canal 6, WPA2, la señal del estado del PHY (-46 dBm)");
	comprueba(vio_abierta && vio_vieja, "y las otras dos: «Vecina abierta» (11) y «Vecina vieja» (3, WPA)");
	comprueba(rtl_sim_rx >= 3, "las balizas llegan por la cola de recepción (RX0)");

	/* conectar con WPA2 */
	pmk_de("clave de prueba", "HispaPrueba", pmk);
	w80211_conectar((const o8 *)"HispaPrueba", 11, pmk);
	comprueba(espera_estado(W_CONECTADO, 6000), "conecta con WPA2: autenticación, asociación y el acuerdo de 4 pasos");
	w80211_estado(&i);
	comprueba(i.canal == 6 && memcmp(i.bssid, mac_ap, 6) == 0 && i.seguridad == W_SEG_WPA2, "conectada: canal 6, el punto de acceso, WPA2");
	entregadas = 0;
	comprueba(manda_arp() == 0, "se envía un ARP (cifrado con CCMP por software, por la cola LOQ)");
	corre(50);
	comprueba(entregadas == 1 && ultima[12] == 8 && ultima[13] == 6 && ultima[21] == 2 && memcmp(ultima + 22, mac_ap, 6) == 0,
		  "llega la respuesta de ARP (descifrada)");
	entregadas = 0;
	comprueba(manda_dhcp() == 0, "se envía el DHCP DISCOVER");
	corre(50);
	comprueba(entregadas == 1 && ultima[0] == 255 && ultima[23] == 17 && ultima[42] == 2, "llega el OFFER (a todos: la clave de grupo)");
	sim_renovar_pares = 1;
	sim_renovar_grupo = 1;
	corre(300);
	entregadas = 0;
	manda_arp();
	corre(50);
	w80211_estado(&i);
	comprueba(i.estado == W_CONECTADO && entregadas == 1 && sim_renov_ptk >= 1 && sim_renov_gtk >= 1,
		  "se renuevan las claves (de pares y de grupo) sin cortar");
	comprueba(rtl_sim_quejas == 0, "el modelo no ha visto nada mal");
	comprueba(rtl_fallos_es == 0, "ninguna orden de SDIO ha fallado");
	rtl_resumen();
	printf("  (tramas: %u mandadas, %u recibidas, %u tiradas por los filtros del chip)\n", rtl_sim_tx, rtl_sim_rx, rtl_sim_rx_tirados);
	radio->parar();
	printf("RTL8723BS contra su modelo: %d bien, %d fallos\n", bien, mal);
	return mal != 0;
}
