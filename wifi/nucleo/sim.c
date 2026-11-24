/*
 * sim.c - Una radio de mentira con su punto de acceso, para probar sin la
 * tarjeta (HSPWIFI /SIMULA en QEMU y las pruebas del anfitrión): contesta a
 * los sondeos, autentica, asocia, hace de autenticador de WPA2 (acuerdo de 4
 * pasos y clave de grupo, y sus renovaciones: la de pares con un acuerdo de 4
 * pasos nuevo cifrado con la de antes, la de grupo con el KeyID alternando 1
 * y 2 y usándola solo cuando la estación contesta, como hostapd), cifra y
 * descifra con CCMP y, detrás, hay una red mínima: un servidor DHCP, ARP y
 * eco de ICMP en 192.168.77.1; y, para las pruebas, un «chorro» de datos
 * numerados (una descarga larga) que se puede seguir desde donde se quedó.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#include "nucleo.h"

#define COLA 12
#define TRAMA 1700
#define SIM_TROZO 1400

static struct {
	char ssid[33];
	int ssid_n;
	int canal;
	int abierta;
	o8 pmk[32];
	int canal_sta;
	/* estación */
	int autenticada, asociada;
	o8 sta[6];
	/* autenticador */
	o8 anonce[32], ptk[48], gtk[16];
	int gtk_id;
	o8 rep[8];
	int paso;		/* 1: mandado el 1; 3: mandado el 3; 4: hecho */
	struct aes tk, gk;
	o8 pn_tx[6], pn_g[6], pn_rx[6];
	d32 t_baliza, t_renueva;
	/* la renovación de la de pares: 1 mandado el 1; 3 mandado el 3 */
	int rk, rk_intentos;
	o8 tptk[48];
	d32 t_ptk, t_rk;
	o8 snonce[32];
	/* la de grupo nueva, mandada y aún sin usar (hasta el 2 de grupo) */
	o8 gtk_nueva[16];
	int id_nueva, grupo_pend, grupo_intentos;
	d32 t_gtk, t_grupo;
	p16 seq;
	/* cola hacia la estación */
	o8 cola[COLA][TRAMA];
	int cola_n[COLA];
	int cab, fin;
	o8 ip_dada[4];
} s;

int sim_fallos;
/* por dónde sale lo que emite el punto de acceso (la prueba del chip simulado
 * lo desvía al anillo de recepción de la tarjeta de mentira) */
void (*sim_entrega)(const o8 *trama, int n, int senal, int canal) = w80211_rx;
int sim_renovar_grupo;		/* las pruebas lo ponen a 1 */
int sim_renovar_pares;		/* las pruebas: renovar la clave por pares ya */
int sim_renovar_pares_cada, sim_renovar_grupo_cada;	/* ms; 0 nunca */
int sim_vieja_tras_4;		/* las pruebas: al renovar, una trama con la clave vieja tras el paso 4 */
int sim_pierde_4;		/* las pruebas: el próximo paso 4 de una renovación «se pierde» */
int sim_snonce_repetido;	/* las pruebas: SNonces iguales en acuerdos distintos */
int sim_renov_ptk, sim_renov_gtk;	/* las pruebas: renovaciones acabadas */
long sim_chorro_sig, sim_chorro_fin;	/* las pruebas: el chorro (trozos de SIM_TROZO) */
int sim_echar;			/* las pruebas: mandar una desautenticación */
int sim_silencio;		/* las pruebas: el punto de acceso desaparece */
int sim_gemela;			/* las pruebas: otra red, ABIERTA, con el mismo nombre (una falsa), en el canal 1 */
int sim_ausente;		/* (CORTES) las pruebas: el punto de acceso desaparece del todo (ni balizas) */
int sim_ip_otra;		/* (CORTES) las pruebas: DHCP da otra IP (192.168.77.11) */
static const o8 ap_mac[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x01 };
static const o8 otra_mac[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x02 };
static const o8 tkip_mac[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x03 };
static const o8 gemela_mac[6] = { 0x02, 0x48, 0x53, 0xaa, 0x00, 0x04 };
static const o8 ip_ap[4] = { 192, 168, 77, 1 };
static const o8 todos[6] = { 255, 255, 255, 255, 255, 255 };
static struct radio sim;
static void chorro(void);

static void encola(const o8 *t, int n)
{
	int sig = (s.fin + 1) % COLA;

	if (sig == s.cab || n > TRAMA)
		return;
	memcpy(s.cola[s.fin], t, n);
	s.cola_n[s.fin] = n;
	s.fin = sig;
}

static int cab_gestion(o8 *t, int sub, const o8 *a1, const o8 *bssid)
{
	memset(t, 0, 24);
	t[0] = sub;
	memcpy(t + 4, a1, 6);
	memcpy(t + 10, bssid, 6);
	memcpy(t + 16, bssid, 6);
	s.seq++;
	t[22] = s.seq << 4; t[23] = s.seq >> 4;
	return 24;
}

/* baliza o respuesta de sondeo de una de las redes de mentira */
static void baliza(int sub, const o8 *a1, const o8 *bssid, const char *ssid, int canal, int seg)
{
	static const o8 rsn[] = { 48, 20, 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 4,
				  1, 0, 0x00, 0x0f, 0xac, 2, 0, 0 };
	static const o8 wpa1[] = { 221, 22, 0x00, 0x50, 0xf2, 1, 1, 0, 0x00, 0x50, 0xf2, 2,
				   1, 0, 0x00, 0x50, 0xf2, 2, 1, 0, 0x00, 0x50, 0xf2, 2 };
	static const o8 vel[] = { 1, 8, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 50, 4, 0x30, 0x48, 0x60, 0x6c };
	o8 t[200];
	int l = cab_gestion(t, sub, a1, bssid), n = strlen(ssid);
	p16 cap = 0x0401 | (seg ? 0x10 : 0);

	memset(t + l, 0, 8); l += 8;
	t[l++] = 100; t[l++] = 0;
	t[l++] = cap; t[l++] = cap >> 8;
	t[l++] = 0; t[l++] = n; memcpy(t + l, ssid, n); l += n;
	memcpy(t + l, vel, sizeof(vel)); l += sizeof(vel);
	t[l++] = 3; t[l++] = 1; t[l++] = canal;
	if (seg == W_SEG_WPA2) {
		memcpy(t + l, rsn, sizeof(rsn)); l += sizeof(rsn);
	} else if (seg == W_SEG_WPA) {
		memcpy(t + l, wpa1, sizeof(wpa1)); l += sizeof(wpa1);
	}
	encola(t, l);
}

static void balizas(int sub, const o8 *a1)
{
	if (s.canal_sta == s.canal && !sim_silencio)
		baliza(sub, a1, ap_mac, s.ssid, s.canal, s.abierta ? 0 : W_SEG_WPA2);
	if (s.canal_sta == 11)
		baliza(sub, a1, otra_mac, "Vecina abierta", 11, 0);
	if (s.canal_sta == 3)
		baliza(sub, a1, tkip_mac, "Vecina vieja", 3, W_SEG_WPA);
	if (sim_gemela && s.canal_sta == 1)
		baliza(sub, a1, gemela_mac, s.ssid, 1, 0);
}

/* datos del punto de acceso a la estación (Ethernet II -> 802.11, cifrado) */
static void ap_datos(const o8 *destino, const o8 *origen, p16 tipo, const o8 *carga, int n, int cifrar)
{
	static o8 t[TRAMA];
	static const o8 snap[6] = { 0xaa, 0xaa, 3, 0, 0, 0 };
	int l = 24, i, grupo = destino[0] & 1;

	memset(t, 0, 24);
	t[0] = 0x08; t[1] = 0x02;
	memcpy(t + 4, destino, 6);
	memcpy(t + 10, ap_mac, 6);
	memcpy(t + 16, origen, 6);
	s.seq++;
	t[22] = s.seq << 4; t[23] = s.seq >> 4;
	memcpy(t + l, snap, 6); l += 6;
	t[l++] = tipo >> 8; t[l++] = tipo;
	memcpy(t + l, carga, n); l += n;
	if (cifrar) {
		o8 *pn = grupo ? s.pn_g : s.pn_tx;
		for (i = 0; i < 6 && ++pn[i] == 0; i++)
			;
		l = ccmp_cifra(grupo ? &s.gk : &s.tk, t, l, 24, pn, grupo ? s.gtk_id : 0);
	}
	encola(t, l);
}

static int eapol_k(const o8 *kck, o8 *e, int info, const o8 *nonce, const o8 *datos, int nd, int mic)
{
	int n = 99 + nd, i;
	o8 m[20];

	memset(e, 0, n);
	e[0] = 2; e[1] = 3; e[2] = (n - 4) >> 8; e[3] = n - 4;
	e[4] = 2; e[5] = info >> 8; e[6] = info;
	e[8] = 16;
	for (i = 7; i >= 0 && ++s.rep[i] == 0; i--)
		;
	memcpy(e + 9, s.rep, 8);
	if (nonce)
		memcpy(e + 17, nonce, 32);
	e[97] = nd >> 8; e[98] = nd;
	if (nd)
		memcpy(e + 99, datos, nd);
	if (mic) {
		hmac_sha1(kck, 16, e, n, m);
		memcpy(e + 81, m, 16);
	}
	return n;
}

static int eapol(o8 *e, int info, const o8 *nonce, const o8 *datos, int nd, int mic)
{
	return eapol_k(s.ptk, e, info, nonce, datos, nd, mic);
}

static void nuevo_anonce(void)
{
	int i;
	d32 r;

	for (i = 0; i < 32; i += 4) {
		r = hl_azar() ^ (hl_jiffies() * 2654435761u);
		memcpy(s.anonce + i, &r, 4);
	}
}

static void manda_paso1(void)
{
	o8 e[120];

	nuevo_anonce();
	s.paso = 1;
	ap_datos(s.sta, ap_mac, 0x888e, e, eapol(e, 0x008a, s.anonce, NULL, 0, 0), 0);
}

/* renovar la clave por pares: un paso 1 nuevo, cifrado con la de ahora (que
 * sigue valiendo hasta el paso 4) */
static void renueva_pares(int otra_vez)
{
	o8 e[120];

	if (!otra_vez) {
		nuevo_anonce();
		s.rk_intentos = 0;
	}
	s.rk = 1;
	s.t_rk = hl_jiffies();
	ap_datos(s.sta, ap_mac, 0x888e, e, eapol(e, 0x008a, s.anonce, NULL, 0, 0), 1);
}

/* la clave de grupo en su KDE, envuelta con la KEK */
static int kde_grupo_k(o8 *sal, int con_rsn, const o8 *kek, const o8 *gtk, int id)
{
	static const o8 rsn[] = { 48, 20, 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 4,
				  1, 0, 0x00, 0x0f, 0xac, 2, 0, 0 };
	o8 d[80];
	int n = 0;

	if (con_rsn) {
		memcpy(d, rsn, sizeof(rsn));
		n = sizeof(rsn);
	}
	d[n++] = 0xdd; d[n++] = 22; d[n++] = 0x00; d[n++] = 0x0f; d[n++] = 0xac; d[n++] = 1;
	d[n++] = id; d[n++] = 0;
	memcpy(d + n, gtk, 16); n += 16;
	if (n & 7) {
		d[n++] = 0xdd;
		while (n & 7)
			d[n++] = 0;
	}
	aes_envuelve(kek, d, n, sal);
	return n + 8;
}

static int kde_grupo(o8 *sal, int con_rsn)
{
	return kde_grupo_k(sal, con_rsn, s.ptk + 16, s.gtk, s.gtk_id);
}

static void nueva_gtk(void)
{
	int i;
	d32 r;

	for (i = 0; i < 16; i += 4) {
		r = hl_azar() ^ 0x5a5a1234;
		memcpy(s.gtk + i, &r, 4);
	}
	s.gtk_id = s.gtk_id == 1 ? 2 : 1;
	aes_clave(&s.gk, s.gtk);
	memset(s.pn_g, 0, 6);
}

/* renovar la de grupo: se manda la nueva (con el otro KeyID), y se sigue
 * cifrando con la de antes hasta que la estación contesta (como hostapd) */
static void renueva_grupo(int otra_vez)
{
	o8 k[80], r[200];
	int i, nd;
	d32 x;

	if (!otra_vez) {
		for (i = 0; i < 16; i += 4) {
			x = hl_azar() ^ 0x5a5a1234;
			memcpy(s.gtk_nueva + i, &x, 4);
		}
		s.id_nueva = s.gtk_id == 1 ? 2 : 1;
		s.grupo_intentos = 0;
	}
	s.grupo_pend = 1;
	s.t_grupo = hl_jiffies();
	nd = kde_grupo_k(k, 0, s.ptk + 16, s.gtk_nueva, s.id_nueva);
	ap_datos(s.sta, ap_mac, 0x888e, r, eapol(r, 0x1382, NULL, k, nd, 1), 1);
	chorro();		/* detrás, más con la de grupo de antes */
}

static void grupo_puesta(void)
{
	memcpy(s.gtk, s.gtk_nueva, 16);
	s.gtk_id = s.id_nueva;
	aes_clave(&s.gk, s.gtk);
	memset(s.pn_g, 0, 6);
	s.grupo_pend = 0;
	s.t_gtk = hl_jiffies();
	sim_renov_gtk++;
}

static void echar(int motivo)
{
	o8 t[30];
	int l = cab_gestion(t, 0xc0, s.sta, ap_mac);

	t[l++] = motivo; t[l++] = 0;
	encola(t, l);
	s.autenticada = s.asociada = 0;
	s.paso = 0;
	s.rk = 0;
	s.grupo_pend = 0;
}

/* una trama de prueba (tipo 0x88b5) cifrada con la clave y el contador dados */
static void manda_marca(const struct aes *k, o8 *pn, o8 marca)
{
	static o8 t[80];
	static const o8 snap[8] = { 0xaa, 0xaa, 3, 0, 0, 0, 0x88, 0xb5 };
	int l = 24, i;

	memset(t, 0, 24);
	t[0] = 0x08; t[1] = 0x02;
	memcpy(t + 4, s.sta, 6);
	memcpy(t + 10, ap_mac, 6);
	memcpy(t + 16, ap_mac, 6);
	s.seq++;
	t[22] = s.seq << 4; t[23] = s.seq >> 4;
	memcpy(t + l, snap, 8); l += 8;
	memset(t + l, marca, 16); l += 16;
	for (i = 0; i < 6 && ++pn[i] == 0; i++)
		;
	l = ccmp_cifra(k, t, l, 24, pn, 0);
	encola(t, l);
}

/* renovación, paso 3: firmado con la PTK nueva y cifrado con la de antes */
static void manda_paso3_renov(void)
{
	o8 k[80], r[200];
	int nd = kde_grupo_k(k, 1, s.tptk + 16, s.gtk, s.gtk_id);

	s.rk = 3;
	s.t_rk = hl_jiffies();
	ap_datos(s.sta, ap_mac, 0x888e, r, eapol_k(s.tptk, r, 0x13ca, s.anonce, k, nd, 1), 1);
	chorro();		/* detrás, más datos con la de antes: llegarán tras el paso 4 */
}

static void ap_eapol(const o8 *e, int n)
{
	o8 copia[300], mic[20], r[200], k[80];
	int info, nd;

	if (n < 99 || n > (int)sizeof(copia))
		return;
	info = (e[5] << 8) | e[6];
	nd = (e[97] << 8) | e[98];
	if (s.rk == 1 && (info & 0x0308) == 0x0108) {
		/* renovación, paso 2 (cifrado con la de ahora): la PTK nueva */
		wpa_ptk(s.pmk, ap_mac, s.sta, s.anonce, e + 17, s.tptk);
		memcpy(copia, e, n);
		memset(copia + 81, 0, 16);
		hmac_sha1(s.tptk, 16, copia, 99 + nd, mic);
		if (memcmp(mic, e + 81, 16) != 0)
			return;
		if (memcmp(s.snonce, e + 17, 32) == 0)
			sim_snonce_repetido++;
		memcpy(s.snonce, e + 17, 32);
		s.rk_intentos = 0;
		manda_paso3_renov();
	} else if (s.rk == 3 && (info & 0x0308) == 0x0308) {
		/* renovación, paso 4 (cifrado aún con la de antes): la nueva vale */
		struct aes vieja = s.tk;
		o8 pn_vieja[6];
		memcpy(copia, e, n);
		memset(copia + 81, 0, 16);
		hmac_sha1(s.tptk, 16, copia, 99 + nd, mic);
		if (memcmp(mic, e + 81, 16) != 0)
			return;
		if (sim_pierde_4) {
			sim_pierde_4 = 0;	/* no ha llegado: en 1 s se repite el 3 */
			return;
		}
		if (sim_vieja_tras_4)
			manda_marca(&s.tk, s.pn_tx, 'A');	/* ya iba de camino */
		memcpy(pn_vieja, s.pn_tx, 6);
		memcpy(s.ptk, s.tptk, 48);
		aes_clave(&s.tk, s.ptk + 32);
		memset(s.pn_tx, 0, 6);
		memset(s.pn_rx, 0, 6);
		s.rk = 0;
		s.t_ptk = hl_jiffies();
		sim_renov_ptk++;
		if (sim_vieja_tras_4) {
			sim_vieja_tras_4 = 0;
			manda_marca(&s.tk, s.pn_tx, 'B');	/* con la nueva */
			manda_marca(&vieja, pn_vieja, 'C');	/* con la vieja, ya tarde: no vale */
		}
	} else if (s.grupo_pend && (info & 0x0308) == 0x0300) {
		/* el 2 de grupo: desde ahora, la de grupo nueva */
		memcpy(copia, e, n);
		memset(copia + 81, 0, 16);
		hmac_sha1(s.ptk, 16, copia, 99 + nd, mic);
		if (memcmp(mic, e + 81, 16) != 0)
			return;
		grupo_puesta();
	} else if (s.paso == 1 && (info & 0x0108) == 0x0108) {
		/* paso 2: sacar la PTK y comprobar el MIC */
		wpa_ptk(s.pmk, ap_mac, s.sta, s.anonce, e + 17, s.ptk);
		if (memcmp(s.snonce, e + 17, 32) == 0)
			sim_snonce_repetido++;
		memcpy(s.snonce, e + 17, 32);
		memcpy(copia, e, n);
		memset(copia + 81, 0, 16);
		hmac_sha1(s.ptk, 16, copia, 99 + nd, mic);
		if (memcmp(mic, e + 81, 16) != 0) {
			echar(15);	/* acuerdo de claves fallido */
			return;
		}
		if (!s.gtk_id)
			nueva_gtk();
		nd = kde_grupo(k, 1);
		s.paso = 3;
		ap_datos(s.sta, ap_mac, 0x888e, r, eapol(r, 0x13ca, s.anonce, k, nd, 1), 0);
	} else if (s.paso == 3 && (info & 0x0308) == 0x0308) {
		memcpy(copia, e, n);
		memset(copia + 81, 0, 16);
		hmac_sha1(s.ptk, 16, copia, 99 + nd, mic);
		if (memcmp(mic, e + 81, 16) != 0)
			return;
		aes_clave(&s.tk, s.ptk + 32);
		memset(s.pn_tx, 0, 6);
		memset(s.pn_rx, 0, 6);
		s.paso = 4;
		s.rk = 0;
		s.grupo_pend = 0;
		s.t_ptk = s.t_gtk = hl_jiffies();
	}
}

/* ---- la red de detrás: DHCP, ARP y eco ---- */
static p16 suma(const o8 *d, int n, d32 ini)
{
	d32 x = ini;
	int i;

	for (i = 0; i + 1 < n; i += 2)
		x += (d[i] << 8) | d[i + 1];
	if (n & 1)
		x += d[n - 1] << 8;
	while (x >> 16)
		x = (x & 0xffff) + (x >> 16);
	return ~x;
}

static void manda_ip(const o8 *destino_mac, const o8 *ip_d, int proto, const o8 *carga, int n)
{
	static o8 p[1600];

	memset(p, 0, 20);
	p[0] = 0x45; p[2] = (20 + n) >> 8; p[3] = 20 + n; p[8] = 64; p[9] = proto;
	memcpy(p + 12, ip_ap, 4);
	memcpy(p + 16, ip_d, 4);
	{ p16 c = suma(p, 20, 0); p[10] = c >> 8; p[11] = c; }
	memcpy(p + 20, carga, n);
	ap_datos(destino_mac, ap_mac, 0x0800, p, 20 + n, s.paso == 4);
}

static void dhcp(const o8 *origen, const o8 *b, int n)
{
	static o8 u[400];
	o8 *r = u + 8, *o;
	int tipo = 0, i = 240, difunde;

	if (n < 240 || b[0] != 1)
		return;
	while (i + 2 <= n && b[i] != 255) {
		if (b[i] == 0) { i++; continue; }
		if (b[i] == 53)
			tipo = b[i + 2];
		i += 2 + b[i + 1];
	}
	if (tipo != 1 && tipo != 3)
		return;
	memset(u, 0, sizeof(u));
	r[0] = 2; r[1] = 1; r[2] = 6;
	memcpy(r + 4, b + 4, 4);		/* xid */
	memcpy(r + 10, b + 10, 2);		/* indicadores */
	s.ip_dada[0] = 192; s.ip_dada[1] = 168; s.ip_dada[2] = 77; s.ip_dada[3] = sim_ip_otra ? 11 : 10;	/* (CORTES) */
	memcpy(r + 16, s.ip_dada, 4);
	memcpy(r + 20, ip_ap, 4);
	memcpy(r + 28, b + 28, 16);
	r[236] = 99; r[237] = 130; r[238] = 83; r[239] = 99;
	o = r + 240;
	*o++ = 53; *o++ = 1; *o++ = tipo == 1 ? 2 : 5;
	*o++ = 54; *o++ = 4; memcpy(o, ip_ap, 4); o += 4;
	*o++ = 51; *o++ = 4; *o++ = 0; *o++ = 0; *o++ = 0x0e; *o++ = 0x10;
	*o++ = 1; *o++ = 4; *o++ = 255; *o++ = 255; *o++ = 255; *o++ = 0;
	*o++ = 3; *o++ = 4; memcpy(o, ip_ap, 4); o += 4;
	*o++ = 6; *o++ = 4; memcpy(o, ip_ap, 4); o += 4;
	*o++ = 255;
	n = 300;
	u[0] = 0; u[1] = 67; u[2] = 0; u[3] = 68; u[4] = (n + 8) >> 8; u[5] = n + 8;
	/* se contesta a todos (como muchos routers): prueba la clave de grupo */
	difunde = 1;
	{
		static const o8 ip_todos[4] = { 255, 255, 255, 255 };
		manda_ip(difunde ? todos : origen, ip_todos, 17, u, n + 8);
	}
}

static void red(const o8 *destino, const o8 *origen, p16 tipo, const o8 *d, int n)
{
	if (tipo == 0x0806 && n >= 28 && d[7] == 1 && memcmp(d + 24, ip_ap, 4) == 0) {
		o8 r[28];
		memcpy(r, d, 8);
		r[7] = 2;
		memcpy(r + 8, ap_mac, 6); memcpy(r + 14, ip_ap, 4);
		memcpy(r + 18, d + 8, 10);
		ap_datos(origen, ap_mac, 0x0806, r, 28, s.paso == 4);
	} else if (tipo == 0x0800 && n >= 28 && (d[0] & 0x0f) == 5) {
		int largo = (d[2] << 8) | d[3];
		if (largo > n)
			return;
		if (d[9] == 17 && d[22] == 0 && d[23] == 67) {
			dhcp(origen, d + 28, largo - 28);
		} else if (d[9] == 1 && d[20] == 8 && memcmp(d + 16, ip_ap, 4) == 0) {
			static o8 r[1500];
			p16 c;
			memcpy(r, d + 20, largo - 20);
			r[0] = 0; r[2] = 0; r[3] = 0;
			c = suma(r, largo - 20, 0);
			r[2] = c >> 8; r[3] = c;
			manda_ip(origen, d + 12, 1, r, largo - 20);
		}
	}
	(void)destino;
}

/* ---- lo que emite la estación ---- */
static int sim_enviar(const o8 *t, int n, int vel)
{
	static o8 c[2400];
	int sub = t[0] & 0xfc;

	(void)vel;
	if (n > (int)sizeof(c))
		return -1;
	if (sim_fallos > 0) {
		sim_fallos--;
		w80211_tx_hecho(0);
		return 0;
	}
	if (sim_ausente)
		return 0;		/* (CORTES) no hay nadie */
	if (sub == 0x40) {
		/* sondeo: contestan las redes de ese canal (la oculta, solo por su nombre) */
		balizas(0x50, t + 10);
		return 0;
	}
	if (s.canal_sta != s.canal || sim_silencio || memcmp(t + 4, ap_mac, 6) != 0)
		return 0;
	w80211_tx_hecho(1);
	if (sub == 0xb0) {
		o8 r[30];
		int l = cab_gestion(r, 0xb0, t + 10, ap_mac);
		r[l++] = 0; r[l++] = 0; r[l++] = 2; r[l++] = 0; r[l++] = 0; r[l++] = 0;
		encola(r, l);
		memcpy(s.sta, t + 10, 6);
		s.autenticada = 1; s.asociada = 0; s.paso = 0;
	} else if (sub == 0x00 && s.autenticada) {
		static const o8 vel_[] = { 1, 8, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24 };
		o8 r[60];
		int l = cab_gestion(r, 0x10, t + 10, ap_mac);
		r[l++] = 0x01; r[l++] = 0x04; r[l++] = 0; r[l++] = 0; r[l++] = 1; r[l++] = 0xc0;
		memcpy(r + l, vel_, sizeof(vel_)); l += sizeof(vel_);
		encola(r, l);
		s.asociada = 1;
		memset(s.rep, 0, 8);
		if (!s.abierta)
			manda_paso1();
		else
			s.paso = 0;
	} else if (sub == 0xc0) {
		s.autenticada = s.asociada = 0;
		s.paso = 0;
	} else if (sub == 0x08 && s.asociada && (t[1] & 3) == 1) {
		int tipo, cab = 24;
		memcpy(c, t, n);
		if (c[1] & 0x40) {
			o8 pn[6];
			if (s.paso != 4)
				return 0;
			n = ccmp_descifra(&s.tk, c, n, cab, pn);
			if (n < 0) {
				hl_log(0, "SIM: trama de la estación con el MIC mal\n");
				return 0;
			}
		} else if (!s.abierta && s.paso == 4) {
			return 0;	/* sin cifrar con las claves puestas: se tira */
		}
		if (n < cab + 8)
			return 0;
		tipo = (c[cab + 6] << 8) | c[cab + 7];
		if (tipo == 0x888e)
			ap_eapol(c + cab + 8, n - cab - 8);
		else if (s.abierta || s.paso == 4)
			red(c + 16, c + 10, tipo, c + cab + 8, n - cab - 8);
	}
	return 0;
}

static int sim_canal(int c)
{
	s.canal_sta = c;
	return 0;
}

/* el chorro de las pruebas: trozos numerados (tipo 0x88b6: el número en 4
 * octetos y SIM_TROZO - 4 octetos que salen de él), uno de cada 16 a todos
 * (con la clave de grupo); los que quepan en la cola, hasta 6 cada vez */
static void chorro(void)
{
	static o8 d[SIM_TROZO];
	int k, i;

	for (k = 0; k < 6 && sim_chorro_sig < sim_chorro_fin; k++) {
		long n = sim_chorro_sig;
		if ((s.fin - s.cab + COLA) % COLA >= COLA - 5)
			break;		/* sitio para lo demás (el acuerdo, ARP...) */
		d[0] = n; d[1] = n >> 8; d[2] = n >> 16; d[3] = n >> 24;
		for (i = 4; i < SIM_TROZO; i++)
			d[i] = (o8)(n * 31 + i);
		ap_datos(n % 16 == 15 ? todos : s.sta, ap_mac, 0x88b6, d, SIM_TROZO, 1);
		sim_chorro_sig++;
	}
}

static void sim_sondear(void)
{
	d32 ya = hl_jiffies();

	if ((int)(ya - s.t_baliza) >= 100 && !sim_ausente) {
		s.t_baliza = ya;
		balizas(0x80, todos);
	}
	if (sim_ausente) {
		s.cab = s.fin;	/* (CORTES) lo que iba a mandar, perdido */
		s.autenticada = s.asociada = 0;
		s.paso = 0;
		return;
	}
	if (s.paso == 4 && s.asociada) {
		/* las renovaciones: a petición de la prueba o cada tanto; si la
		 * estación no contesta en 1 s, otra vez, y a la cuarta se la echa
		 * (como hostapd: «4-way handshake timeout», motivo 15; «group key
		 * handshake timeout», motivo 16) */
		if (s.rk == 0 && !s.grupo_pend && (sim_renovar_pares ||
		    (sim_renovar_pares_cada && (int)(ya - s.t_ptk) >= sim_renovar_pares_cada))) {
			sim_renovar_pares = 0;
			renueva_pares(0);
		} else if (s.rk && (int)(ya - s.t_rk) >= 1000) {
			if (++s.rk_intentos >= 4) {
				echar(15);
				return;
			}
			if (s.rk == 1)
				renueva_pares(1);
			else
				manda_paso3_renov();	/* no le ha llegado el 3, o a nosotros su 4 */
		}
		if (s.rk == 0 && !s.grupo_pend && (sim_renovar_grupo ||
		    (sim_renovar_grupo_cada && (int)(ya - s.t_gtk) >= sim_renovar_grupo_cada))) {
			sim_renovar_grupo = 0;
			renueva_grupo(0);
		} else if (s.grupo_pend && (int)(ya - s.t_grupo) >= 1000) {
			if (++s.grupo_intentos >= 4) {
				echar(16);
				return;
			}
			renueva_grupo(1);
		}
		chorro();
	}
	if (sim_echar && s.asociada) {
		sim_echar = 0;
		echar(1);
	}
	while (s.cab != s.fin) {
		int i = s.cab;
		s.cab = (s.cab + 1) % COLA;
		sim_entrega(s.cola[i], s.cola_n[i], -47, s.canal_sta);
	}
}

static void sim_bss(const o8 *b, int as, int in, int rc, const o8 *v, int nv)
{
	(void)b; (void)as; (void)in; (void)rc; (void)v; (void)nv;
}

void sim_configura(const char *ssid, const char *clave, int canal)
{
	struct pbkdf2 p;

	memset(&s, 0, sizeof(s));
	s.ssid_n = strlen(ssid);
	memcpy(s.ssid, ssid, s.ssid_n + 1);
	s.canal = canal;
	s.canal_sta = 1;
	s.abierta = clave == NULL;
	if (clave) {
		pbkdf2_empieza(&p, clave, (const o8 *)ssid, s.ssid_n);
		while (!pbkdf2_sigue(&p, 4096, s.pmk))
			;
	}
}

struct radio *sim_radio(void)
{
	static const o8 mac[6] = { 0x02, 0x48, 0x53, 0x57, 0x46, 0x01 };

	sim.nombre = "Radio simulada";
	memcpy(sim.mac, mac, 6);
	sim.canal = sim_canal;
	sim.enviar = sim_enviar;
	sim.sondear = sim_sondear;
	sim.bss = sim_bss;
	return &sim;
}
