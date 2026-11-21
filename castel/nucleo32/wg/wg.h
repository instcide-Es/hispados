/* wg.h - WireGuard en Hispa-Pentium: el protocolo (el «whitepaper» de
 * WireGuard, de J. A. Donenfeld, y wireguard.com/protocol), escrito aquí: el
 * apretón de manos Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s, las galletas
 * contra los ataques (mac1, mac2), los mensajes de datos con su ventana
 * contra las repeticiones y los tiempos (rehacer las claves cada 2
 * minutos, mantener viva la conexión). Lo usan WG.BIN (wgmain.c: un túnel
 * como el de wg-quick) y TAILSCALE.BIN (que los manda también por DERP).
 * Solo IPv4. Con Monocypher (X25519, ChaCha20-Poly1305) y hispac/cripto.c
 * (BLAKE2s, el HKDF).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_WG_H
#define HISPA_WG_H
#include "hispa.h"

#define WG_MAX_PARES 64
#define WG_MAX_REDES 8
#define WG_MTU 1280

struct wg_claves {
	int vale, soy_inicio;
	u32 mio, suyo;			/* los índices: el nuestro y el de la otra punta */
	u8 enviar[32], recibir[32];
	u64 env, rec_max, ventana;	/* lo enviado; lo más alto recibido y los 64 de antes */
	u32 nacida;			/* el tic en que se hizo */
};

struct wg_par {
	int usado;
	u8 pub[32], psk[32];
	u8 pre_ss[32];			/* DH(nuestra estática, la suya): se calcula una vez */
	u8 mac1_clave[32], galleta_clave[32];
	u32 ep_ip, ep_puerto;		/* por dónde se le llega por UDP (0: no se sabe) */
	u32 red[WG_MAX_REDES], masc[WG_MAX_REDES];
	int n_redes;
	u32 mantener;			/* PersistentKeepalive (s; 0: no) */
	char nombre[64];
	void *dato;			/* (lo del programa: Tailscale lo usa para saber por dónde va) */
	/* el apretón en curso, como iniciador */
	int hs_curso;
	u32 hs_indice, hs_empezado, hs_primero, hs_reintentos;
	u8 hs_e[32], hs_h[32], hs_ck[32];
	u8 tai_max[12];			/* la marca de tiempo más alta que ha mandado él */
	struct wg_claves act, ant, sig;	/* sig: la del respondedor, hasta que llegue el primer dato */
	u32 ult_env, ult_rec, ult_hs, ult_datos_rec, ult_datos_env;
	u8 galleta[16];
	u32 galleta_tic;
	u8 ult_mac1[16];
	u8 pend[WG_MTU + 64];		/* un paquete que espera al apretón */
	u32 n_pend;
	u64 rx, tx;			/* octetos */
	u32 hs_fecha;			/* (el último apretón: fecha y hora como en la FAT) */
};

extern struct wg_par wg_pares[WG_MAX_PARES];
extern u8 wg_priv[32], wg_pub[32];

/* lo pone el programa: manda un mensaje de WireGuard a ese par (UDP o DERP) */
int wg_manda(struct wg_par *p, const u8 *d, u32 n);

void wg_inicia(const u8 priv[32]);
struct wg_par *wg_nuevo_par(const u8 pub[32]);
struct wg_par *wg_par_de_clave(const u8 pub[32]);
void wg_quita_par(struct wg_par *p);
/* lo que llega (de UDP: ip y puerto; de DERP: ip = 0): si es un paquete IP
 * para dentro, lo deja en «sale» y da su largo; 0 si no. *quien: de qué par */
int wg_entra(const u8 *d, u32 n, u32 ip, u32 puerto, u8 *sale, struct wg_par **quien);
/* un paquete IP que sale: al par de su destino (0, o -1 si no es de nadie) */
int wg_sale(const u8 *ip_paquete, u32 n);
void wg_tic(void);			/* los tiempos: cada décima de segundo, más o menos */
int wg_es_mensaje(const u8 *d, u32 n);	/* ¿es de WireGuard (1 a 4)? */
struct wg_par *wg_par_de_ip(u32 ip);
int wg_de_base64(const char *t, u8 d[32]);	/* una clave en base64: 1 si vale */
void wg_a_base64(const u8 d[32], char *t);	/* (t: 45) */
#endif
