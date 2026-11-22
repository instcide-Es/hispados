/* red.h - Correo: una conexión con su búfer (renglones y trozos de tantos
 * octetos, para IMAP, SMTP y POP3), un HTTP pequeño (la autoconfiguración y
 * las imágenes de fuera, si se piden) y los registros SRV del DNS. C99
 * portable sobre so.h.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_RED_H
#define CORREO_RED_H
#include "so.h"

#define CON_MAX_RENGLON (1u << 20)	/* un renglón de más de 1 MB: el servidor miente */

typedef struct {
	int c;			/* la conexión de so.h (0: ninguna) */
	int modo;		/* SO_PLANO, SO_TLS, SO_STARTTLS_... */
	u8 *b;			/* lo que ha llegado y aún no se ha leído */
	u32 ini, fin, cabe;
	u32 espera_ms;		/* lo que se espera a que llegue algo (de entrada, 60 s) */
	u32 recibidos, enviados;
	int error;		/* el último (SO_E_...), 0 si ninguno */
	char texto[240];	/* el porqué, en español */
	char cifrado[96];	/* «TLS 1.2, ECDHE-ECDSA, ...» (vacío: sin cifrar) */
	char servidor[128];
	u32 puerto;
} Con;

/* lo que hace la ventana mientras se espera a la red (repintar, mirar si se
 * ha pulsado «Cancelar»): devuelve 1 para cancelar */
extern int (*red_espera)(void *u);
extern void *red_espera_u;

int con_abre(Con *k, const char *servidor, u32 puerto, int modo, u32 espera_ms);
void con_cierra(Con *k);
/* un renglón entero (sin el CRLF ni el LF), en «*r» (vale hasta la llamada
 * siguiente; acaba en 0). Da su largo, o <0 si error (k->texto) */
int con_renglon(Con *k, char **r);
/* exactamente n octetos (un literal de IMAP), a d */
int con_lee(Con *k, u8 *d, u32 n);
int con_envia(Con *k, const void *d, u32 n);
int con_envia_texto(Con *k, const char *f, ...);
/* ¿hay algo ya llegado (o que llegue en espera_ms)? 1 sí, 0 no, <0 error */
int con_hay(Con *k, u32 espera_ms);
/* el texto de un error de so.h */
const char *con_texto_error(int e);

/* ---- HTTP (GET), para la autoconfiguración y las imágenes ----
 * Da el código (200...) o <0; el cuerpo, en memoria nueva (con un 0 detrás),
 * a lo sumo «max» octetos. Sigue hasta 3 redirecciones. «solo_https»: no va
 * a http:// */
int http_get(const char *url, u8 **cuerpo, u32 *largo, u32 max, int solo_https, char *error, int cabe);

/* ---- el DNS: el registro SRV de «nombre» (_imaps._tcp.dominio): 1 si lo
 * hay (el de menos prioridad), 0 si no ---- */
int dns_srv(const char *nombre, char *servidor, int cabe, u32 *puerto);
/* (para las pruebas) la pregunta y lo que se saca de la respuesta */
u32 dns_pregunta(const char *nombre, u16 id, u16 tipo, u8 *d, u32 cabe);
int dns_lee_srv(const u8 *r, u32 n, u16 id, char *servidor, int cabe, u32 *puerto);

#endif
