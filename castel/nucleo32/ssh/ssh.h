/* ssh.h - el SSH-2 de Hispa-Pentium: lo común del cliente (SSH.BIN, sshc.c)
 * y del servidor (SSHD.BIN, sshd.c): el transporte (RFC 4253), el
 * intercambio de claves curve25519-sha256 (RFC 8731), la llave del equipo
 * ssh-ed25519 (RFC 8709), el cifrado chacha20-poly1305@openssh.com
 * (PROTOCOL.chacha20poly1305 de OpenSSH) y los mensajes de la autenticación
 * (RFC 4252) y de los canales (RFC 4254). Con la criptografía de Monocypher
 * (terceros/monocypher) y la de hispac/cripto.c.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_SSH_H
#define HISPA_SSH_H
#include "hispa.h"

#define SSH_DISCONNECT 1
#define SSH_IGNORE 2
#define SSH_UNIMPLEMENTED 3
#define SSH_DEBUG 4
#define SSH_SERVICE_REQUEST 5
#define SSH_SERVICE_ACCEPT 6
#define SSH_EXT_INFO 7
#define SSH_KEXINIT 20
#define SSH_NEWKEYS 21
#define SSH_KEX_ECDH_INIT 30
#define SSH_KEX_ECDH_REPLY 31
#define SSH_USERAUTH_REQUEST 50
#define SSH_USERAUTH_FAILURE 51
#define SSH_USERAUTH_SUCCESS 52
#define SSH_USERAUTH_BANNER 53
#define SSH_USERAUTH_PK_OK 60
#define SSH_GLOBAL_REQUEST 80
#define SSH_REQUEST_SUCCESS 81
#define SSH_REQUEST_FAILURE 82
#define SSH_CHANNEL_OPEN 90
#define SSH_CHANNEL_OPEN_CONFIRMATION 91
#define SSH_CHANNEL_OPEN_FAILURE 92
#define SSH_CHANNEL_WINDOW_ADJUST 93
#define SSH_CHANNEL_DATA 94
#define SSH_CHANNEL_EXTENDED_DATA 95
#define SSH_CHANNEL_EOF 96
#define SSH_CHANNEL_CLOSE 97
#define SSH_CHANNEL_REQUEST 98
#define SSH_CHANNEL_SUCCESS 99
#define SSH_CHANNEL_FAILURE 100

#define SSH_CABE 36000		/* lo más que mide un paquete (RFC 4253: 35000) */

/* para escribir un mensaje */
struct sbuf { u8 *d; u32 n, cabe; int mal; };
void sb_inicia(struct sbuf *b, u8 *d, u32 cabe);
void sb_u8(struct sbuf *b, u32 v);
void sb_u32(struct sbuf *b, u32 v);
void sb_datos(struct sbuf *b, const void *d, u32 n);
void sb_cad(struct sbuf *b, const void *d, u32 n);	/* string: el largo y los octetos */
void sb_texto(struct sbuf *b, const char *t);
void sb_mpint(struct sbuf *b, const u8 *d, u32 n);	/* un número sin signo, de mayor a menor */

/* para leer uno */
struct slee { const u8 *p; u32 n, i; int mal; };
void sl_inicia(struct slee *l, const u8 *p, u32 n);
u32 sl_u8(struct slee *l);
u32 sl_u32(struct slee *l);
const u8 *sl_cad(struct slee *l, u32 *n);
int sl_cad_es(const u8 *c, u32 n, const char *t);	/* ¿es ese texto? */
void sl_texto(struct slee *l, char *d, int cabe);	/* una string, como texto acabado en 0 */

struct ssh {
	int servidor;
	u32 sock;
	char v_cli[256], v_srv[256];
	u8 kx_mio[600], kx_suyo[2048];
	u32 n_mio, n_suyo;
	u8 sesion[32];
	int hay_sesion, estricto, cifrado_ent, cifrado_sal, recien;
	u32 seq_ent, seq_sal;
	u8 k_ent[64], k_sal[64];		/* chacha20-poly1305: 0-31 la principal (K_2), 32-63 la del largo (K_1) */
	u8 ent[SSH_CABE + 64];
	u32 ent_n, gastar;
	u8 sale[SSH_CABE + 64];
	/* el servidor: su llave (Ed25519 de Monocypher: la semilla y la pública) */
	u8 host_secreta[64], host_publica[32];
	char error[160];
	u32 limite;			/* (el tic hasta el que se espera el intercambio) */
};

/* lo pone el programa: el cliente comprueba la llave del servidor (blob: el
 * «ssh-ed25519» y la clave); 0 si se sigue */
int ssh_comprueba_servidor(struct ssh *s, const u8 *blob, u32 n);

void ssh_prepara(struct ssh *s, int servidor, u32 sock);
int ssh_empieza(struct ssh *s);		/* las versiones y el primer intercambio de claves: 0 si va */
int ssh_envia(struct ssh *s, const u8 *pl, u32 n);	/* 0, o -1 */
/* el mensaje siguiente (sin IGNORE, DEBUG ni los de un nuevo intercambio de
 * claves): 1 y su sitio; 0 si en «espera» tics no ha llegado nada; -1 si se
 * ha cortado (s->error dice por qué) */
int ssh_lee(struct ssh *s, const u8 **pl, u32 *n, u32 espera);
void ssh_corta(struct ssh *s, u32 razon, const char *texto);	/* DISCONNECT */
/* lo firmado para una llave (sesión, 50, usuario, servicio, «publickey», 1, alg, blob) */
u32 ssh_lo_firmado(struct ssh *s, u8 *d, u32 cabe, const char *usuario, const u8 *alg, u32 nalg, const u8 *blob, u32 nb);
void ssh_huella(const u8 *blob, u32 n, char *t);	/* «SHA256:...» (t: 60) */
void ssh_blob_ed25519(const u8 pub[32], u8 blob[51]);	/* «ssh-ed25519» y la clave */
#endif
