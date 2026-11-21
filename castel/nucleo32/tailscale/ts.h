/* ts.h - TAILSCALE.BIN, el cliente de Tailscale de Hispa-Pentium: lo que
 * comparten ts.c (el programa y el servicio), control.c (el servidor de
 * control: ts2021, Noise y HTTP/2), derp.c (los relevos DERP) y json.c.
 * Escrito a partir de la documentación pública de Tailscale y de su código
 * (BSD-3, github.com/tailscale/tailscale) como referencia, sin copiarlo.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HISPA_TS_H
#define HISPA_TS_H
#include "hispa.h"
#include "wg.h"

#define TS_CAPVER 113		/* la versión de las capacidades que se dice (tailcfg.CapabilityVersion) */

/* ---- json.c: un trozo de JSON (dónde empieza y lo que mide) ---- */
struct jv { const char *p; u32 n; };
int j_valor(const char *t, u32 n, struct jv *v);	/* el primer valor de t */
int j_campo(struct jv o, const char *nombre, struct jv *v);	/* el campo de un objeto: 1 si está */
int j_elemento(struct jv a, int i, struct jv *v);	/* el elemento i de un arreglo */
int j_cuantos(struct jv a);				/* los elementos de un arreglo (o campos de un objeto) */
int j_par(struct jv o, int i, struct jv *nombre, struct jv *v);	/* el par i de un objeto */
int j_texto(struct jv v, char *d, int cabe);		/* una cadena, sin las comillas ni los escapes */
long long j_numero(struct jv v);
int j_es(struct jv v, const char *t);			/* ¿es esa cadena? */
int j_cierto(struct jv v);				/* true */
int j_nulo(struct jv v);
int j_ruta(struct jv o, const char *ruta, struct jv *v);	/* «Node.Addresses» */
/* para escribir: un texto con las comillas y los escapes de JSON */
int j_cadena(char *d, int cabe, const char *t);

/* ---- control.c ---- */
struct ts_control;
extern char ts_servidor[200];		/* «https://controlplane.tailscale.com» */
extern u8 ts_maquina[32], ts_maquina_pub[32], ts_nodo[32], ts_nodo_pub[32], ts_disco[32], ts_disco_pub[32];
extern u8 ts_control_pub[32];
extern char ts_error[200];
extern int ts_depura;			/* lo de HTTP/2, al registro */
extern u32 ts_espera_cabeceras;		/* (s) lo que se espera a que empiece la respuesta */
int ts_clave_control(void);		/* GET /key: la clave del servidor de control */
int ts_conecta(void);			/* ts2021: 0 si va */
void ts_desconecta(void);
/* un pedido (POST con JSON): da el código HTTP (200...), o -1. Con
 * «abierto», no espera al final: lo que llega se lee con ts_lee_cuerpo */
int ts_pide(const char *camino, const char *json, u32 n, int abierto);
int ts_lee_cuerpo(u8 *d, u32 cabe, u32 espera);	/* lo que haya del cuerpo: n; 0 nada; -1 fin o error */
int ts_cuerpo_entero(char *d, u32 cabe, u32 espera);	/* el cuerpo hasta el final */
int ts_conectado(void);

/* ---- derp.c ---- */
#define DERP_MAX 4
int derp_conecta(int region, const char *host, u32 ip, u32 puerto);	/* -1, o el número de la conexión */
void derp_cierra_todas(void);
int derp_manda(int region, const u8 dest[32], const u8 *d, u32 n);
/* lo que llega por cualquiera: el que lo manda y el paquete; 1 si hay */
int derp_recibe(u8 de[32], u8 *d, u32 cabe, u32 *n, int *region);
int derp_region_conectada(int region);
void derp_tic(void);
extern int derp_casa;			/* la región propia */

/* ---- ts.c ---- */
struct ts_region { int id; char codigo[16], nombre[48], host[96]; u32 ip, puerto; };
#define TS_MAX_REGIONES 48
extern struct ts_region ts_regiones[TS_MAX_REGIONES];
extern int ts_n_regiones;
struct ts_region *ts_region(int id);
#endif
