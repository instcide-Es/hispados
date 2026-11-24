/*
 * nucleo.h - Lo común del núcleo Wi-Fi de Hispa-486 (HSPWIFI.BIN): la radio,
 * la gestión 802.11 (w80211.c), WPA2 (wpa.c) y las llamadas del residente.
 * Estos archivos no dependen de Linux ni del equipo: se compilan igual para
 * el núcleo (32 bits, sin sistema) y para las pruebas del anfitrión.
 *
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 */
#ifndef NUCLEO_H
#define NUCLEO_H

#include <stddef.h>
#include <stdarg.h>

typedef unsigned char o8;	/* octeto */
typedef unsigned short p16;	/* palabra */
typedef unsigned int d32;	/* doble palabra */
typedef unsigned long long c64;

void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);

/* registro: 0 error, 1 aviso, 2 normal, 3 detalle */
void hl_log(int nivel, const char *f, ...) __attribute__((format(printf, 2, 3)));
unsigned long hl_jiffies(void);		/* milisegundos desde el arranque */
d32 hl_azar(void);

/* ------------------------------------------------------------- la radio */
struct radio {
	const char *nombre;
	o8 mac[6];
	int (*canal)(int ch);			/* 1..13 */
	/* trama 802.11 completa (sin FCS); vel: en medios Mbit/s (2 = 1 Mbit/s),
	 * o 0 = la que la radio vea mejor (datos) */
	int (*enviar)(const o8 *trama, int n, int vel);
	void (*sondear)(void);			/* entrega lo recibido con w80211_rx */
	void (*barrido)(int empieza);
	/* el punto de acceso: NULL = ninguno. vels: sus velocidades (en medios
	 * Mbit/s, con el bit 7 en las básicas) */
	void (*bss)(const o8 *bssid, int asociado, int intervalo, int ranura_corta,
		    const o8 *vels, int nvel);
	void (*parar)(void);
	/* (INTEL) 1: la tarjeta escanea ella sola (el firmware de las Intel):
	 * las balizas llegan con su canal de verdad, no el del barrido */
	int barre_sola;
	/* (INTEL) las claves de WPA2, para la tarjeta que cifra ella (CCMP en
	 * el firmware: ADD_STA_KEY); NULL en las que no (Broadcom, Atheros, la
	 * simulada: CCMP lo hace wpa.c). par 1: la de pares (KeyID 0), en la
	 * ranura 0 o 1 (la nueva va en la otra; la vieja se quita cuando ya
	 * llegan tramas con la nueva); par 0: la de grupo «id» (1 a 3), con su
	 * contador (rsc, 6 octetos, el bajo primero). tk NULL: quitarla */
	int (*clave)(int par, int id, int ranura, const o8 *tk, const o8 *rsc);
};
extern struct radio *radio;

/* ------------------------------------------------------- gestión 802.11 */
#define W_MAX_REDES 24
struct w_red {
	o8 bssid[6];
	o8 ssid[33];
	o8 ssid_n;
	o8 canal;
	signed char senal;		/* dBm */
	o8 seguridad;			/* W_SEG_* */
	o8 rsn[40];			/* el elemento RSN que se mandará */
	o8 rsn_n;
	p16 capacidad;
	p16 intervalo;
	o8 velocidades[16];
	o8 nvel;
	d32 visto;
};
#define W_SEG_ABIERTA 0
#define W_SEG_WEP 1
#define W_SEG_WPA 2		/* WPA (TKIP): no se admite */
#define W_SEG_WPA2 3		/* WPA2-Personal con CCMP */
#define W_SEG_OTRA 4		/* WPA2-Empresa, WPA3 solo... */
#define W_SEG_WPA2_TKIP 5	/* WPA2 con la clave de grupo TKIP (modo mixto) */

enum w_estado {
	W_PARADO, W_BUSCANDO, W_AUTENTICANDO, W_ASOCIANDO, W_CLAVES, W_CONECTADO
};
/* motivos del último fallo (para WIFI y el registro) */
enum w_fallo {
	W_F_NADA, W_F_NO_ESTA, W_F_SIN_RESPUESTA_AUT, W_F_AUT_RECHAZADA,
	W_F_SIN_RESPUESTA_ASOC, W_F_ASOC_RECHAZADA, W_F_CLAVE, W_F_SEGURIDAD,
	W_F_EXPULSADO, W_F_PERDIDA,
	W_F_ACUERDO		/* el acuerdo de claves no acaba y han llegado mensajes con el MIC mal: no se culpa a la contraseña */
};
struct w_info {
	o8 estado;
	o8 fallo;
	o8 canal;
	signed char senal;
	o8 bssid[6];
	o8 ssid[33];
	o8 ssid_n;
	o8 seguridad;
	o8 nredes;
	o8 barriendo;
	o8 mac[6];
	d32 tx, rx, tx_err, rx_desc, reconexiones;
	d32 renov_ptk, renov_gtk;	/* renovaciones de las claves de WPA2 (por pares y de grupo) */
} __attribute__((packed));

void w80211_inicia(struct radio *r);
void w80211_rx(const o8 *trama, int n, int senal, int canal);
void w80211_tic(void);				/* máquina de estados */
void w80211_barrer(void);
int w80211_barriendo(void);
int w80211_redes(struct w_red **lista);
void w80211_conectar(const o8 *ssid, int n, const o8 *pmk);	/* pmk NULL: abierta */
void w80211_desconectar(void);
void w80211_estado(struct w_info *i);
int w80211_enviar_eth(const o8 *trama, int n);	/* Ethernet II -> 802.11 */
/* lo que llega para arriba (Ethernet II): lo pone el núcleo o la prueba */
void w_entrega_eth(const o8 *trama, int n);
void w80211_tx_hecho(int acuse);

/* ----------------------------------------------------------------- WPA2 */
void sha1(const o8 *d, size_t n, o8 *sal);
void hmac_sha1(const o8 *k, size_t kn, const o8 *d, size_t n, o8 *sal);
void hmac_sha1_v(const o8 *k, size_t kn, int nv, const o8 **v, const size_t *vn, o8 *sal);
/* PBKDF2-HMAC-SHA1 de 4096 vueltas, 32 octetos: la PMK de WPA2-Personal.
 * Por trozos, para no parar el equipo: se llama hasta que devuelve 1. */
struct pbkdf2 { o8 u[2][20], t[2][20]; o8 clave[64]; o8 clave_n; o8 ssid[36]; o8 ssid_n; int vuelta; };
void pbkdf2_empieza(struct pbkdf2 *p, const char *clave, const o8 *ssid, int ssid_n);
int pbkdf2_sigue(struct pbkdf2 *p, int vueltas, o8 *pmk);
void pbkdf2_sha1(const char *clave, const o8 *ssid, int ssid_n, int vueltas, o8 *sal, int nsal);
void prf_sha1(const o8 *k, size_t kn, const char *etiqueta, const o8 *d, size_t n, o8 *sal, size_t nsal);
struct aes { d32 rk[44]; };
void aes_clave(struct aes *a, const o8 *k);
void aes_cifra(const struct aes *a, const o8 *e, o8 *s);
void aes_descifra(const struct aes *a, const o8 *e, o8 *s);
int aes_desenvuelve(const o8 *kek, const o8 *e, int n, o8 *s);	/* RFC 3394; n = octetos de salida */
void aes_envuelve(const o8 *kek, const o8 *e, int n, o8 *s);
/* CCMP: trama 802.11 con cabecera; devuelve la longitud nueva o -1 */
int ccmp_cifra(const struct aes *a, o8 *trama, int n, int cab, const o8 *pn, int id_clave);
int ccmp_descifra(const struct aes *a, o8 *trama, int n, int cab, o8 *pn_sal);

/* el suplicante: acuerdo de 4 pasos, su renovación y la clave de grupo */
struct wpa_gk {			/* una clave de grupo (por su KeyID, 1 a 3) */
	o8 clave[16];
	struct aes a;
	o8 pn[6];			/* el último número de paquete aceptado */
	o8 puesta;
};
struct wpa {
	o8 pmk[32];
	o8 aa[6], spa[6];		/* punto de acceso y nosotros */
	o8 anonce[32], snonce[32];
	o8 ptk[48];			/* la puesta: KCK 16, KEK 16, TK 16 */
	o8 tptk[48];			/* la del acuerdo en curso (no se usa hasta el paso 3) */
	struct wpa_gk g[4];		/* las de grupo, por KeyID: la de ahora y la anterior */
	o8 gtk_id;			/* la última de grupo puesta */
	o8 repeticion[8];
	o8 hay_rep;
	o8 rsn[40]; o8 rsn_n;		/* nuestro elemento RSN */
	o8 rsn_ap[64]; o8 rsn_ap_n;	/* el de la baliza */
	o8 paso;			/* 0 nada, 1 mandado el 2, 2 hecho (claves puestas) */
	o8 renovando;			/* con las claves puestas, mandado el 2 de un acuerdo nuevo */
	o8 ptk_puesta, gtk_puesta;
	struct aes tk;
	o8 pn_tx[6];
	o8 pn_rx[6];
	/* la clave por pares de antes de renovarla: las tramas que ya venían
	 * con ella se siguen aceptando hasta que llega una con la nueva (o
	 * pasan W_T_VIEJA ms); el paso 4 va cifrado con ella (el punto de acceso
	 * no pone la nueva hasta recibirlo) */
	struct aes tk_vieja;
	o8 pn_tx_vieja[6], pn_rx_vieja[6];
	o8 hay_vieja;
	d32 t_vieja;
	/* lo que dice wpa_eapol a w80211 con cada respuesta */
	o8 resp_cifra;			/* 0 en claro, 1 con la clave puesta, 2 con la vieja */
	o8 nueva_ptk;			/* 1: se acaba de poner una por pares (para la tarjeta) */
	o8 nueva_gtk;			/* el KeyID de la de grupo que se acaba de poner (0 ninguna) */
	o8 quita_gtk;			/* el KeyID de la de grupo que ya no vale (0 ninguna) */
	o8 ranura;			/* (INTEL) la ranura de la tarjeta de la de pares puesta */
	int fallos_mic;
	d32 renov_ptk, renov_gtk;	/* cuántas veces se han renovado */
};
#define W_T_VIEJA 10000		/* lo más que se guarda la clave por pares vieja */
void wpa_empieza(struct wpa *w, const o8 *pmk, const o8 *aa, const o8 *spa, const o8 *rsn, int rsn_n);
/* un EAPOL recibido (desde la versión); devuelve la respuesta en sal (su
 * longitud; 0 nada, -1 fallo: clave equivocada) */
int wpa_eapol(struct wpa *w, const o8 *e, int n, o8 *sal);
void wpa_ptk(const o8 *pmk, const o8 *aa, const o8 *spa, const o8 *anonce, const o8 *snonce, o8 *ptk);

/* ---------------------------------------------------- radio de mentira */
struct radio *sim_radio(void);
void sim_configura(const char *ssid, const char *clave, int canal);
extern void (*sim_entrega)(const o8 *trama, int n, int senal, int canal);
extern int sim_fallos;		/* para las pruebas: tramas que se «pierden» */
extern int sim_gemela;		/* para las pruebas: una red abierta falsa con el mismo nombre */
/* para las pruebas: cada cuántos ms renueva el punto de acceso de mentira la
 * clave por pares (un acuerdo de 4 pasos nuevo) y la de grupo; 0 nunca */
extern int sim_renovar_pares_cada, sim_renovar_grupo_cada;

#endif
