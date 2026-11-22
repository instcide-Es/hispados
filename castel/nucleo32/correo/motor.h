/* motor.h - Correo: lo que se hace con una cuenta (motor.c), lo mismo para
 * la ventana que para las órdenes (orden.c): abrir lo guardado, conectar,
 * traer las carpetas y las fichas (en tramos, lo nuevo primero), bajar un
 * mensaje, marcar, mover, borrar a la papelera, buscar en el servidor,
 * enviar (y guardar en Enviados), los borradores, IDLE, y el POP3.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_MOTOR_H
#define CORREO_MOTOR_H
#include "correo.h"

#define TRAMO 300		/* fichas por pedido */

typedef struct {
	int ic;			/* el número de la cuenta en cuentas[] */
	Cuenta *c;
	char clave[128];
	Imap imap;
	int conectada;
	int sin_red;		/* se ha intentado y no hay: se ve lo guardado */
	Carpeta car[MAX_CARPETAS];
	int n_car;
	int sel;		/* la carpeta abierta (en car[]), -1 ninguna */
	Buzon buz;
	int buz_abierto;
	char estado[300];	/* lo que dice la barra de abajo */
	char error[400];
	u32 nuevos;		/* lo nuevo de la última vez (para el aviso) */
	u32 ult_ruido;		/* (IDLE) */
	u32 desde_noop;
	/* (lo que avanza al traer fichas) */
	void (*avance)(void *u, u32 hecho, u32 total, const char *que);
	void *avance_u;
} Sesion;

int motor_abre(Sesion *s, int ic);
void motor_cierra(Sesion *s);
/* con la clave de s->clave (o la guardada) */
int motor_conecta(Sesion *s);
void motor_desconecta(Sesion *s);
int motor_carpetas(Sesion *s);
/* abre la carpeta k (lo guardado) y, si hay conexión, la pone al día */
int motor_elige(Sesion *s, int k, int al_dia);
int motor_al_dia(Sesion *s);
/* el mensaje entero (de lo guardado o del servidor; se guarda) */
u8 *motor_mensaje(Sesion *s, Ficha *f, u32 *n);
int motor_marca(Sesion *s, Ficha **v, int n, u32 banderas, int poner);
int motor_mueve(Sesion *s, Ficha **v, int n, int destino);
int motor_borra(Sesion *s, Ficha **v, int n);
/* buscar en el servidor (TEXT, FROM o SUBJECT): las fichas que casan, en
 * memoria nueva (punteros al buzón) */
int motor_busca(Sesion *s, const char *texto, int donde, Ficha ***v, int *n);
/* y en lo bajado (sin red) */
int motor_busca_local(Sesion *s, const char *texto, Ficha ***v, int *n);
int motor_envia(Sesion *s, const Redactado *r, const char *destinos);
int motor_borrador(Sesion *s, const Redactado *r, u32 *uid_viejo);
/* mira si hay algo nuevo (IDLE si se puede, NOOP si no): 1 si ha cambiado algo */
int motor_mira(Sesion *s, u32 espera_ms);
int motor_carpeta_especial(Sesion *s, u32 marca);
/* el POP3: baja lo nuevo a la bandeja de aquí */
int motor_pop3(Sesion *s);
#endif
