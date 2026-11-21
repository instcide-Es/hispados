/*
 * prueba_wifi.c - Pruebas en el anfitrión de la parte del Wi-Fi que no
 * depende de la tarjeta: los vectores de las normas (SHA-1, HMAC, PBKDF2,
 * PRF, AES, envoltorio de claves, CCMP) y la gestión 802.11 con WPA2 contra
 * el punto de acceso de mentira de wifi/nucleo/sim.c (búsqueda, asociación,
 * acuerdo de 4 pasos, DHCP por la clave de grupo, ARP, eco, renovación de la
 * clave de grupo, contraseña equivocada, tramas perdidas, expulsión, pérdida
 * de la red y reconexión).
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nucleo.h"

static unsigned long reloj = 1000;
static int fallos, hechas, charla;
static o8 ultima[1600];
static int ultima_n, entregadas;
extern int sim_renovar_grupo, sim_echar, sim_silencio;

void hl_log(int nivel, const char *f, ...)
{
	va_list a;
	if (!charla)
		return;
	va_start(a, f);
	printf("    [%lu] ", reloj);
	vprintf(f, a);
	va_end(a);
	(void)nivel;
}
unsigned long hl_jiffies(void) { return reloj; }
d32 hl_azar(void) { return rand(); }
void w_entrega_eth(const o8 *t, int n) { memcpy(ultima, t, n); ultima_n = n; entregadas++; }

static void comprueba(int c, const char *que)
{
	hechas++;
	if (!c) {
		fallos++;
		printf("  FALLA: %s\n", que);
	} else if (charla) {
		printf("  bien: %s\n", que);
	}
}

static int hex(const char *h, o8 *s)
{
	int n = 0;
	while (h[0] && h[1]) {
		unsigned v;
		sscanf(h, "%2x", &v);
		s[n++] = v;
		h += 2;
	}
	return n;
}

static void igual(const o8 *d, const char *h, const char *que)
{
	o8 e[256];
	int n = hex(h, e);
	comprueba(memcmp(d, e, n) == 0, que);
}

static void vectores(void)
{
	o8 s[80], k[96], d[128], c[160];
	struct aes a;
	struct pbkdf2 p;
	int n;

	/* SHA-1 (FIPS 180) */
	sha1((const o8 *)"abc", 3, s);
	igual(s, "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 «abc»");
	sha1((const o8 *)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, s);
	igual(s, "84983e441c3bd26ebaae4aa1f95129e5e54670f1", "SHA-1 de dos bloques");
	/* HMAC-SHA1 (RFC 2202) */
	memset(k, 0x0b, 20);
	hmac_sha1(k, 20, (const o8 *)"Hi There", 8, s);
	igual(s, "b617318655057264e28bc0b6fb378c8ef146be00", "HMAC-SHA1 RFC 2202 n.º 1");
	hmac_sha1((const o8 *)"Jefe", 4, (const o8 *)"what do ya want for nothing?", 28, s);
	igual(s, "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79", "HMAC-SHA1 RFC 2202 n.º 2");
	memset(k, 0xaa, 80);
	hmac_sha1(k, 80, (const o8 *)"Test Using Larger Than Block-Size Key - Hash Key First", 54, s);
	igual(s, "aa4ae5e15272d00e95705637ce8a3b55ed402112", "HMAC-SHA1 con clave de 80 octetos");
	/* PBKDF2 (RFC 6070) */
	pbkdf2_sha1("password", (const o8 *)"salt", 4, 1, s, 20);
	igual(s, "0c60c80f961f0e71f3a9b524af6012062fe037a6", "PBKDF2 RFC 6070, 1 vuelta");
	pbkdf2_sha1("password", (const o8 *)"salt", 4, 4096, s, 20);
	igual(s, "4b007901b765489abead49d926f721d065a429c1", "PBKDF2 RFC 6070, 4096 vueltas");
	/* la PMK (IEEE 802.11i, H.4.3) */
	pbkdf2_empieza(&p, "password", (const o8 *)"IEEE", 4);
	n = 0;
	while (!pbkdf2_sigue(&p, 100, s))
		n++;
	igual(s, "f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e", "PMK «password»/«IEEE»");
	comprueba(n >= 40, "la PMK se calcula por trozos");
	pbkdf2_empieza(&p, "ThisIsAPassword", (const o8 *)"ThisIsASSID", 11);
	while (!pbkdf2_sigue(&p, 4096, s))
		;
	igual(s, "0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", "PMK «ThisIsAPassword»/«ThisIsASSID»");
	pbkdf2_empieza(&p, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", (const o8 *)"ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ", 32);
	while (!pbkdf2_sigue(&p, 4096, s))
		;
	igual(s, "becb93866bb8c3832cb777c2f559807c8c59afcb6eae734885001300a981cc62", "PMK con nombre de 32 letras");
	/* PRF (IEEE 802.11i, H.6.3) */
	memset(k, 0x0b, 20);
	prf_sha1(k, 20, "prefix", (const o8 *)"Hi There", 8, s, 64);
	igual(s, "bcd4c650b30b9684951829e0d75f9d54b862175ed9f00606e17d8da35402ffee"
	      "75df78c3d31e0f889f012120c0862beb67753e7439ae242edb8373698356cf5a", "PRF n.º 1");
	prf_sha1((const o8 *)"Jefe", 4, "prefix", (const o8 *)"what do ya want for nothing?", 28, s, 64);
	igual(s, "51f4de5b33f249adf81aeb713a3c20f4fe631446fabdfa58244759ae58ef9009"
	      "a99abf4eac2ca5fa87e692c440eb40023e7babb206d61de7b92f41529092b8fc", "PRF n.º 2");
	/* la PTK: PRF-384 con las direcciones y los números ordenados */
	{
		o8 pmk[32], aa[6], spa[6], an[32], sn[32];
		hex("0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", pmk);
		hex("a0a1a1a3a4a5", aa); hex("b0b1b2b3b4b5", spa);
		hex("e0e1e2e3e4e5e6e7e8e9f0f1f2f3f4f5f6f7f8f9", an);
		hex("c0c1c2c3c4c5c6c7c8c9d0d1d2d3d4d5d6d7d8d9", sn);
		memset(an + 20, 0, 12); memset(sn + 20, 0, 12);
		wpa_ptk(pmk, aa, spa, an, sn, s);
		/* el valor, calculado aparte con hmac de Python */
		igual(s, "67abf518ab0dbd7a80014f2744bd7731c1c72129b07fe4ed65dcf9a1b98c57f2"
		      "4b9ffcff38b0582b34f81deb59a78b95", "PTK (KCK, KEK y TK)");
	}
	/* AES-128 (FIPS 197, apéndice C.1) */
	hex("000102030405060708090a0b0c0d0e0f", k);
	hex("00112233445566778899aabbccddeeff", d);
	aes_clave(&a, k);
	aes_cifra(&a, d, c);
	igual(c, "69c4e0d86a7b0430d8cdb78070b4c55a", "AES-128 cifrar (FIPS 197)");
	aes_descifra(&a, c, c);
	igual(c, "00112233445566778899aabbccddeeff", "AES-128 descifrar");
	hex("2b7e151628aed2a6abf7158809cf4f3c", k);
	hex("3243f6a8885a308d313198a2e0370734", d);
	aes_clave(&a, k);
	aes_cifra(&a, d, c);
	igual(c, "3925841d02dc09fbdc118597196a0b32", "AES-128 (FIPS 197, apéndice B)");
	/* envoltorio de claves (RFC 3394, 4.1) */
	hex("000102030405060708090a0b0c0d0e0f", k);
	hex("00112233445566778899aabbccddeeff", d);
	aes_envuelve(k, d, 16, c);
	igual(c, "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5", "envoltorio de claves RFC 3394");
	comprueba(aes_desenvuelve(k, c, 16, s) == 0 && memcmp(s, d, 16) == 0, "desenvolver");
	c[3] ^= 1;
	comprueba(aes_desenvuelve(k, c, 16, s) < 0, "desenvolver: lo alterado se rechaza");
	/* CCMP (IEEE 802.11-2007, H.8.3.1, vector de prueba 1) */
	hex("c97c1f67ce371185514a8a19f2bdd52f", k);
	aes_clave(&a, k);
	n = hex("0848c32c0fd2e128a57c5030f1844408abaea5b8fcba8033"
		"f8ba1a55d02f85ae967bb62fb6cda8eb7e78a050", c);
	hex("0ce7769703b5", d);
	n = ccmp_cifra(&a, c, n, 24, d, 0);
	comprueba(n == 60, "CCMP: longitud");
	igual(c, "0848c32c0fd2e128a57c5030f1844408abaea5b8fcba8033" "0ce70020769703b5"
	      "f3d0a2fe9a3dbf2342a643e43246e80c3c04d019" "7845ce0b16f97623", "CCMP: vector 1 de IEEE 802.11");
	n = ccmp_descifra(&a, c, n, 24, s);
	comprueba(n == 44, "CCMP: descifrar");
	igual(c + 24, "f8ba1a55d02f85ae967bb62fb6cda8eb7e78a050", "CCMP: texto recuperado");
	igual(s, "0ce7769703b5", "CCMP: número de paquete");
	n = ccmp_cifra(&a, c, 44, 24, d, 0);
	c[30] ^= 0x10;
	comprueba(ccmp_descifra(&a, c, n, 24, s) < 0, "CCMP: una trama alterada se rechaza");
}

static void corre(int ms)
{
	while (ms > 0) {
		reloj += 10;
		ms -= 10;
		radio->sondear();
		w80211_tic();
	}
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

static void gestion(void)
{
	struct w_info i;
	struct w_red *l;
	o8 pmk[32], mala[32];
	int n, k, veces;

	sim_configura("Biblioteca", "clave de la prueba", 6);
	w80211_inicia(sim_radio());
	pmk_de("clave de la prueba", "Biblioteca", pmk);
	pmk_de("otra clave distinta", "Biblioteca", mala);

	/* búsqueda */
	w80211_barrer();
	comprueba(w80211_barriendo(), "la búsqueda empieza");
	corre(13 * 130 + 50);
	comprueba(!w80211_barriendo(), "la búsqueda acaba (13 canales)");
	n = w80211_redes(&l);
	comprueba(n == 3, "se ven las tres redes");
	for (k = 0; k < n; k++) {
		if (!strcmp((char *)l[k].ssid, "Biblioteca"))
			comprueba(l[k].canal == 6 && l[k].seguridad == W_SEG_WPA2 && l[k].senal == -47, "Biblioteca: canal 6, WPA2");
		if (!strcmp((char *)l[k].ssid, "Vecina abierta"))
			comprueba(l[k].canal == 11 && l[k].seguridad == W_SEG_ABIERTA, "Vecina abierta: canal 11, abierta");
		if (!strcmp((char *)l[k].ssid, "Vecina vieja"))
			comprueba(l[k].canal == 3 && l[k].seguridad == W_SEG_WPA, "Vecina vieja: canal 3, WPA");
	}

	/* conectar con WPA2 */
	w80211_conectar((const o8 *)"Biblioteca", 10, pmk);
	comprueba(espera_estado(W_CONECTADO, 4000), "conecta con WPA2 (acuerdo de 4 pasos)");
	w80211_estado(&i);
	comprueba(i.canal == 6 && memcmp(i.bssid, mac_ap, 6) == 0 && i.seguridad == W_SEG_WPA2, "estado: canal y punto de acceso");

	/* ARP (respuesta dirigida, clave de pareja) */
	entregadas = 0;
	comprueba(manda_arp() == 0, "se envía un ARP cifrado");
	corre(50);
	comprueba(entregadas == 1 && ultima_n == 42 && ultima[12] == 8 && ultima[13] == 6 && ultima[21] == 2 &&
		  memcmp(ultima + 22, mac_ap, 6) == 0 && memcmp(ultima, radio->mac, 6) == 0, "llega la respuesta de ARP");
	/* DHCP (respuesta a todos, clave de grupo) */
	entregadas = 0;
	comprueba(manda_dhcp() == 0, "se envía el DHCP DISCOVER");
	corre(50);
	comprueba(entregadas == 1 && ultima[0] == 255 && ultima[23] == 17 && ultima[42] == 2 &&
		  ultima[42 + 16] == 192 && ultima[42 + 19] == 10 && ultima[42 + 242] == 2, "llega el OFFER por la clave de grupo");

	/* renovación de la clave de grupo */
	sim_renovar_grupo = 1;
	corre(100);
	entregadas = 0;
	manda_dhcp();
	corre(50);
	comprueba(entregadas == 1 && ultima[42 + 242] == 2, "tras renovar la clave de grupo, sigue llegando lo de todos");
	entregadas = 0;
	manda_arp();
	corre(50);
	comprueba(entregadas == 1, "y lo dirigido");

	/* búsqueda estando conectado: vuelve a su canal y sigue */
	w80211_barrer();
	corre(13 * 130 + 100);
	w80211_estado(&i);
	comprueba(i.estado == W_CONECTADO && i.nredes == 3, "buscar estando conectado no corta");
	entregadas = 0;
	manda_arp();
	corre(50);
	comprueba(entregadas == 1, "tras buscar, la red sigue");

	/* el punto de acceso nos echa: reconecta solo */
	sim_echar = 1;
	corre(100);
	w80211_estado(&i);
	comprueba(i.estado != W_CONECTADO && i.fallo == W_F_EXPULSADO, "expulsión: se entera");
	comprueba(espera_estado(W_CONECTADO, 12000), "tras la expulsión, reconecta solo");
	entregadas = 0;
	manda_arp();
	corre(50);
	comprueba(entregadas == 1, "y la red vuelve a ir (claves nuevas)");

	/* la red desaparece y vuelve */
	sim_silencio = 1;
	corre(9000);
	w80211_estado(&i);
	comprueba(i.estado != W_CONECTADO, "sin punto de acceso: se da por perdida");
	corre(8000);
	w80211_estado(&i);
	comprueba(i.estado != W_CONECTADO && i.fallo == W_F_NO_ESTA, "mientras no está: «la red no aparece»");
	sim_silencio = 0;
	comprueba(espera_estado(W_CONECTADO, 12000), "cuando vuelve, reconecta");
	w80211_estado(&i);
	comprueba(i.reconexiones == 2, "cuenta las reconexiones");

	/* tramas perdidas: reintenta la autenticación y la asociación */
	w80211_desconectar();
	corre(200);
	for (veces = 1; veces <= 3; veces++) {
		w80211_conectar((const o8 *)"Biblioteca", 10, pmk);
		corre(13 * 130 + 100);		/* la búsqueda */
		sim_fallos = veces;
		comprueba(espera_estado(W_CONECTADO, 6000), "conecta aunque se pierdan tramas");
		w80211_desconectar();
		corre(200);
	}

	/* contraseña equivocada */
	w80211_conectar((const o8 *)"Biblioteca", 10, mala);
	comprueba(!espera_estado(W_CONECTADO, 5000), "con otra contraseña no conecta");
	w80211_estado(&i);
	comprueba(i.fallo == W_F_CLAVE, "y dice que es la contraseña");
	comprueba(manda_arp() < 0, "sin conexión no se envía nada");

	/* red que no existe; red con seguridad que no se admite; abierta */
	w80211_conectar((const o8 *)"NoExiste", 8, pmk);
	corre(3000);
	w80211_estado(&i);
	comprueba(i.estado != W_CONECTADO && i.fallo == W_F_NO_ESTA, "red que no existe");
	w80211_conectar((const o8 *)"Vecina vieja", 12, pmk);
	corre(3000);
	w80211_estado(&i);
	comprueba(i.fallo == W_F_SEGURIDAD, "red WPA (TKIP): no se admite y lo dice");
	w80211_conectar((const o8 *)"Biblioteca", 10, NULL);
	corre(3000);
	w80211_estado(&i);
	comprueba(i.fallo == W_F_SEGURIDAD, "red con contraseña sin darla: lo dice");
	w80211_desconectar();

	/* una red abierta */
	sim_configura("Abierta", NULL, 1);
	w80211_inicia(sim_radio());
	w80211_conectar((const o8 *)"Abierta", 7, NULL);
	comprueba(espera_estado(W_CONECTADO, 4000), "conecta con una red abierta");
	entregadas = 0;
	manda_arp();
	corre(50);
	comprueba(entregadas == 1 && ultima[21] == 2, "ARP por la red abierta");
	entregadas = 0;
	manda_dhcp();
	corre(50);
	comprueba(entregadas == 1, "DHCP por la red abierta");
}

/* (seguridad) el suplicante, a mano: un mensaje con el MIC mal no corta el
 * acuerdo (puede no venir del punto de acceso); la misma clave de grupo otra
 * vez no se vuelve a poner ni pone su contador a cero (KRACK de grupo) */
static int eapol_firmado(struct wpa *w, o8 *e, int info, int rep, const o8 *gtk, int id, int mic_bueno, int rsc)
{
	o8 d[24], mic[20];
	int nd = 32, n = 99 + nd;

	memset(e, 0, n);
	e[0] = 2; e[1] = 3; e[2] = (n - 4) >> 8; e[3] = n - 4;
	e[4] = 2; e[5] = info >> 8; e[6] = info;
	e[16] = rep;
	e[65] = rsc;		/* Key RSC: el contador de la clave de grupo */
	memset(d, 0, sizeof d);
	d[0] = 0xdd; d[1] = 22; d[2] = 0x00; d[3] = 0x0f; d[4] = 0xac; d[5] = 1; d[6] = id;
	memcpy(d + 8, gtk, 16);
	aes_envuelve(w->ptk + 16, d, 24, e + 99);
	e[97] = 0; e[98] = nd;
	hmac_sha1(w->ptk, 16, e, n, mic);
	memcpy(e + 81, mic, 16);
	if (!mic_bueno)
		e[81] ^= 0x55;
	return n;
}

static void suplicante(void)
{
	static const o8 aa[6] = { 2, 0, 0, 0, 0, 1 }, spa[6] = { 2, 0, 0, 0, 0, 2 };
	struct wpa w;
	o8 pmk[32], e[300], r[300], g1[16], g2[16];
	int n, k;

	memset(pmk, 7, 32);
	memset(g1, 0x11, 16);
	memset(g2, 0x22, 16);
	/* paso 1 y, luego, un «paso 3» con el MIC mal */
	wpa_empieza(&w, pmk, aa, spa, NULL, 0);
	memset(e, 0, 99);
	e[0] = 2; e[1] = 3; e[4] = 2; e[5] = 0x00; e[6] = 0x8a; e[16] = 1;
	for (k = 0; k < 32; k++)
		e[17 + k] = k;
	comprueba(wpa_eapol(&w, e, 99, r) > 0 && w.paso == 1, "suplicante: paso 1, se manda el 2");
	n = eapol_firmado(&w, e, 0x13ca, 2, g1, 1, 0, 0);
	for (k = 0; k < 32; k++)
		e[17 + k] = k;
	n = wpa_eapol(&w, e, 99 + 32, r);
	comprueba(n == 0 && w.fallos_mic == 1 && w.paso == 1 && !w.ptk_puesta,
		  "un mensaje con el MIC mal se ignora (no corta ni culpa a la contraseña) y se cuenta");
	/* con las claves puestas: la misma clave de grupo, otra vez */
	w.paso = 2; w.ptk_puesta = 1; w.hay_rep = 1; memset(w.repeticion, 0, 8); w.repeticion[7] = 2;
	memcpy(w.gtk, g1, 16); w.gtk_id = 1; w.gtk_puesta = 1; aes_clave(&w.gk, g1);
	memset(w.pn_rx_g, 0, 6); w.pn_rx_g[0] = 5;
	n = eapol_firmado(&w, e, 0x1382, 3, g1, 1, 1, 0);
	comprueba(wpa_eapol(&w, e, n, r) > 0 && w.pn_rx_g[0] == 5,
		  "la misma clave de grupo otra vez: no se vuelve a poner y su contador no vuelve a cero (KRACK)");
	n = eapol_firmado(&w, e, 0x1382, 4, g2, 2, 1, 9);
	comprueba(wpa_eapol(&w, e, n, r) > 0 && w.gtk_id == 2 && memcmp(w.gtk, g2, 16) == 0 && w.pn_rx_g[0] == 9,
		  "una clave de grupo nueva sí, con el contador que dice el mensaje (Key RSC)");
}

/* (seguridad) una red ABIERTA con el mismo nombre que la WPA2 (una falsa):
 * con contraseña se elige la WPA2, aunque la otra se vea antes */
static void gemela(void)
{
	struct w_info i;
	o8 pmk[32];

	sim_configura("Biblioteca", "clave de la prueba", 6);
	sim_gemela = 1;
	w80211_inicia(sim_radio());
	pmk_de("clave de la prueba", "Biblioteca", pmk);
	w80211_conectar((const o8 *)"Biblioteca", 10, pmk);
	comprueba(espera_estado(W_CONECTADO, 6000), "con una «Biblioteca» abierta falsa al lado, conecta");
	w80211_estado(&i);
	comprueba(memcmp(i.bssid, mac_ap, 6) == 0 && i.seguridad == W_SEG_WPA2, "y es con la de WPA2, no con la abierta");
	w80211_desconectar();
	sim_gemela = 0;
}

int main(int argc, char **argv)
{
	charla = argc > 1;
	vectores();
	gestion();
	suplicante();
	gemela();
	printf("Wi-Fi (anfitrión): %d comprobaciones, %d fallos\n", hechas, fallos);
	return fallos != 0;
}
