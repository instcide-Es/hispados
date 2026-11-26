/* remota.h - (REMOTA) lo que comparten el servidor SSH (sshd.c) y la consola
 * remota «Dejar entrar a Claude» (remota.c, SSHD /REMOTA): las fichas de los
 * usuarios del núcleo, lo de sshd.c que usa la consola remota y lo de la
 * consola remota que mira sshd.c mientras atiende una conexión.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_REMOTA_H
#define HISPA_REMOTA_H
#include "ssh.h"

/* (USUARIOS) los usuarios del sistema (cuentas.cas; Ficha_usuario y
 * Pedido_usuario de usuario.cas: la misma forma) */
struct ficha_u {
	u32 id;
	u8 admin, entra_solo, con_clave, es_yo;
	u32 imagen;
	char nombre[24];
	char carpeta[12];
	u8 en_sesion, resto[15];
};
struct pedido_u {
	u32 orden, id, admin, entra_solo, imagen, n_clave, n_vieja;
	char nombre[24];
	u8 clave[128];
	u8 vieja[128];
};

/* ---- de sshd.c ---- */
extern struct ssh S;
int con_usuarios(void);
int soy_administrador(void);
int busca_usuario(const char *u, struct ficha_u *f);	/* u en UTF-8 */
int entra_usuario(const struct ficha_u *f, u32 a, const u8 *c, u32 nc);
int llave_equipo(int hacer);
void huella_equipo(char *t);
int lee_secreto(char *d, int cabe);
/* atiende una conexión aceptada: 1 si ha entrado alguien (y ya ha salido) */
int atiende(u32 c, u32 ip, u32 pu);

/* ---- de remota.c ---- */
extern int remota;		/* 1: SSHD /REMOTA (la consola remota), no el servicio */
int remota_principal(const char *args);
/* en las esperas de una conexión: lo de la ventana (o el teclado); 1 si hay
 * que cortar ya (el dueño ha pulsado «Echar» o cierra) */
int remota_vigila(void);
/* el código que ha mandado quien llega (la «contraseña» de SSH): 1 si es el
 * vigente (y se gasta); 0 si no (cuenta el fallo); -1 si hay que cortar: el
 * tercer fallo con ese código (sale otro) o no hay ninguno vigente (la pausa) */
int remota_codigo(const char *usuario, const u8 *c, u32 n);
/* ha entrado: lo enseña y lo apunta en REMOTA.LOG */
void remota_entra(const char *usuario);
/* antes de lanzar el intérprete: que sea del administrador que la abrió */
void remota_lanza(void);
/* lo que se hace dentro, para el registro (una orden, leer o escribir un archivo) */
void remota_hace(const char *que);
#endif
