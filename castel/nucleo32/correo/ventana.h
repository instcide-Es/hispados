/* ventana.h - Correo: lo que comparten las piezas de la ventana: ventana.c
 * (los tres paneles, la barra, el bucle), vista.c (el mensaje), redacta.c
 * (escribir), dialogos.c (los controles y los cuadros: campos, botones,
 * preguntas, elegir carpeta o archivo, el asistente de cuentas, la libreta,
 * los ajustes) e iconos.c (los dibujitos: nada se dice solo con el color).
 * Todo en C portable sobre ui.h y so.h.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_VENTANA_H
#define CORREO_VENTANA_H
#include "motor.h"
#include "ui.h"
#include "html.h"

/* los colores (los de controles.cas) */
#define C_FONDO 0xE9EDF2
#define C_PAPEL 0xFFFFFF
#define C_TINTA 0x111827
#define C_SUAVE 0x4B5563
#define C_AZUL 0x1D4ED8
#define C_AZUL_CLARO 0xDBEAFE
#define C_ROJO 0x991B1B
#define C_ROJO_CLARO 0xFEE2E2
#define C_AMBAR_CLARO 0xFEF3C7
#define C_VERDE 0x166534
#define C_BORDE 0x6B7280
#define C_GRIS 0xD1D5DB
#define C_BARRA 0xF3F4F6
#define C_ELEGIDO 0x1E40AF
#define LA UI_LETRA_AN
#define LT UI_LETRA_AL
#define ALTO_BOTON 26
#define FILA 20			/* lo alto de una fila de las listas */

/* ---- la ventana y su estado ---- */
extern int van, val;		/* lo que mide */
extern Sesion *ses;		/* la cuenta abierta */
extern int cuenta_abierta;	/* (en cuentas[]) */
extern void (*repinta_fondo)(void);	/* lo de debajo de un cuadro */
void repinta_todo(void);
void di_estado(const char *f, ...);
int espera_red(void *u);	/* (red_espera) pinta y mira «Cancelar» */

/* ---- los dibujitos (iconos.c): 12 x 12, con «#» la tinta ---- */
#define IC_ESTRELLA 0
#define IC_ESTRELLA_VACIA 1
#define IC_CLIP 2
#define IC_AVISO 3
#define IC_SOBRE 4		/* sin leer */
#define IC_SUBE 5		/* ▲ */
#define IC_BAJA 6		/* ▼ */
#define IC_RESPONDIDO 7
#define IC_CANDADO 8
#define IC_CANDADO_ABIERTO 9
#define IC_HILO 10
void icono(int cual, int x, int y, u32 tinta, u32 fondo);
/* el icono del programa (32 x 32), para la barra */
void icono_correo(int x, int y);

/* ---- los controles (dialogos.c) ---- */
void zonas_olvida(void);
void zona(int id, int x, int y, int an, int al);
int zona_en(int x, int y);
int ancho_texto(const char *t);		/* en puntos (8 por letra) */
void texto(int x, int y, const char *t, int max, u32 c);
void negrita(int x, int y, const char *t, int max, u32 c);
int boton(int id, int x, int y, int an, const char *t, int principal, int apagado, int foco);
void marco_hundido(int x, int y, int an, int al, u32 fondo);
void caja(int x, int y, int an, int al, const char *titulo);

/* un campo de una línea (o de varias: el editor de redacta.c es otro) */
#define CAMPO_MAX 1024
typedef struct {
	u32 t[CAMPO_MAX];	/* las letras (Unicode) */
	int n, cur, desde;
	int oculto;		/* (contraseñas: asteriscos) */
	int sel;		/* el otro extremo de lo seleccionado (-1: nada) */
} Campo;
void campo_pon(Campo *c, const char *utf8);
void campo_saca(const Campo *c, char *d, u32 cabe);	/* a UTF-8 */
void campo_pinta(Campo *c, int x, int y, int an, int foco);
/* una tecla: 1 si la ha usado */
int campo_tecla(Campo *c, u32 k, u32 mod);
void campo_clic(Campo *c, int x, int px);

/* los cuadros (con su bucle; lo de debajo lo pinta repinta_fondo) */
int pregunta(const char *titulo, const char *t, const char *b1, const char *b2, const char *b3);
void aviso(const char *titulo, const char *t);
/* un aviso rojo (lo peligroso): dice «[!]» delante y lleva sus dos botones */
int pregunta_peligro(const char *titulo, const char *t, const char *si, const char *no);
int pide_texto(const char *titulo, const char *pregunta_, char *t, u32 cabe, int oculto);
/* elegir de una lista (las opciones, separadas por «\n»): su número, o -1 */
int elige(const char *titulo, const char *opciones, int marcada);
/* elegir un archivo (para_guardar: con su nombre, que se puede cambiar) */
int elige_archivo(const char *titulo, char *ruta, u32 cabe, int para_guardar, const char *filtro);
/* el asistente de una cuenta nueva (o cambiar una: i >= 0): 1 si se ha guardado */
int asistente_cuenta(int i);
/* la libreta de direcciones; con «elegir», devuelve la dirección elegida */
int ventana_libreta(char *elegida, u32 cabe);
void ventana_ajustes(void);
/* una barrita de progreso con «Cancelar» encima de todo (mientras se espera) */
void progreso(const char *que, u32 hecho, u32 total);
extern int cancelado;

/* ---- el mensaje (vista.c) ---- */
void vista_abre(Ficha *f);
void vista_cierra(void);
void vista_pinta(int x, int y, int an, int al);
int vista_raton(int x, int y, u32 botones);
int vista_tecla(u32 k, u32 mod);
void vista_rueda(int pasos);
int vista_hay(void);
int vista_abre_datos(u8 *d, u32 n);	/* (suyo: lo suelta vista_cierra) */
u32 uid_abierto(void);
/* lo que se responde (redacta.c lo usa): en memoria nueva */
Mensaje *vista_mensaje(void);
const u8 *vista_datos(u32 *n);
char *vista_texto_plano(u32 *n);
/* los adjuntos: abrir (por Escudo) y guardar */
void vista_adjunto(int k, int guardar);

/* ---- escribir (redacta.c) ---- */
#define RD_NUEVO 0
#define RD_RESPONDER 1
#define RD_TODOS 2
#define RD_REENVIAR 3
#define RD_BORRADOR 4
/* abre la redacción (en la misma ventana, encima de todo, hasta enviar o
 * cerrar); «para»: un mailto: o una dirección */
void redacta(int como, const char *para);

#endif
