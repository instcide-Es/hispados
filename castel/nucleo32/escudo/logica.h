/* logica.h - ESCUDO.BIN: lo que escudo.c (las revisiones, la cuarentena, el
 * registro, las opciones y las órdenes) le da a ventana.c (la ventana).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ESCUDO_LOGICA_H
#define ESCUDO_LOGICA_H
#include "pentium.h"

#define RUTA 260
extern char dir_escudo[RUTA];		/* C:\HISPA\ESCUDO (o C:\ESCUDO, sin instalar) */
extern char dir_cuarentena[RUTA];	/* C:\HISPA\CUARENTENA */
extern char prog_escudo[RUTA];		/* C:\HISPA\ESCUDO.BIN */

typedef struct opciones {
	int preguntar;			/* al conectar una memoria o meter un CD */
	int descargas;			/* revisar lo que baja el Navegador */
	int antes_exe;			/* revisar los .EXE de Windows antes de abrirlos */
	int zip;			/* mirar dentro de los ZIP */
	int cuarentena_sola;		/* poner en cuarentena sin preguntar */
	int actualizar_solo;		/* actualizar las firmas una vez al día */
	u32 max_mb;			/* lo más que se lee entero de un archivo */
	u32 ultima_act;			/* AAAAMMDD: la última vez que se intentó actualizar */
} opciones;
extern opciones op;
void lee_opciones(void);
void guarda_opciones(void);

/* las firmas */
extern info_firmas inf;
extern int firmas_cargadas;
int carga_firmas(void);			/* 0 si hay alguna (o solo la de EICAR) */
u32 dias_firmas(void);			/* los días desde las firmas (0xFFFF: no hay) */
void texto_estado(char *d, int cabe, int *bien);

/* una revisión */
#define MAX_HALLADOS 64
typedef struct hallado {
	char ruta[RUTA];
	char nombre[MAX_NOMBRE_VIRUS];
	char dentro[MAX_DENTRO];
	int aviso;			/* 1: no es un virus, un aviso (AUTORUN.INF) */
	int hecho;			/* 0 nada; 1 en cuarentena; 2 borrado; 3 dejado */
} hallado;
typedef struct revision {
	char raiz[RUTA];
	u32 contados, octetos_total;	/* (lo contado antes: para la barra) */
	u32 revisados, octetos, sin_mirar, cifrados, errores;
	u32 n_hallados;
	hallado h[MAX_HALLADOS];
	int cortada, acabada, contando;
	char actual[RUTA];
	u32 empezo_ms, ms;
} revision;
extern revision rev;
/* revisa la ruta (archivo, carpeta o «X:»; «*», todas las unidades) */
void revisa(const char *ruta, int lista);
extern void (*al_avanzar)(void);	/* (ventana.c) para pintar la barra */

/* la cuarentena */
#define MAX_CUARENTENA 200
typedef struct en_cuarentena {
	char archivo[16];		/* Qnnnnnnn.CUA */
	char ruta[RUTA];		/* de dónde salió */
	char nombre[MAX_NOMBRE_VIRUS];
	char cuando[20];
	u32 tam;
} en_cuarentena;
extern en_cuarentena cua[MAX_CUARENTENA];
extern u32 n_cua;
void lee_cuarentena(void);
int a_cuarentena(const char *ruta, const char *nombre);	/* 0 bien */
int restaura(u32 i, char *donde);	/* 0 bien; deja en «donde» adónde ha vuelto */
int borra_de_cuarentena(u32 i);
int borra_archivo(const char *ruta, const char *nombre);

void apunta(const char *f, ...);	/* al registro (REGISTRO.TXT) y al puerto serie */
int lee_registro(char *d, u32 cabe);	/* las últimas líneas */

/* (CORTAFUEGOS) el cortafuegos: las reglas (CORTAFUE.INI, las escribe Escudo
 * y las lee RED.BIN) y lo que dice RED.BIN (O_CORTAFUEGOS) */
#define CF_NADIE 0
#define CF_TODOS 1
#define CF_RED 2
#define CF_LOCAL 3
#define CF_TAILSCALE 4
#define CF_IP 5
#define MAX_CF 16
typedef struct regla_cf { u32 puerto, de; char ip[24]; } regla_cf;
typedef struct escucha_cf { u32 puerto, tarea; int udp; } escucha_cf;
typedef struct bloqueo_cf { u32 hora, proto, puerto; char ip[16]; } bloqueo_cf;
typedef struct cortafuegos {
	int activo, avisar;
	u32 ping;			/* 0 no, 1 red, 2 todos */
	regla_cf reglas[MAX_CF];
	u32 n_reglas;
	/* lo de RED.BIN (si está en marcha) */
	int hay_red;
	u32 bloqueados;
	escucha_cf escuchas[MAX_CF];
	u32 n_escuchas;
	escucha_cf pendientes[4];
	u32 n_pendientes;
	bloqueo_cf ultimos[8];
	u32 n_ultimos;
} cortafuegos;
extern cortafuegos cf;
void lee_cortafuegos(void);		/* CORTAFUE.INI y, si hay red, el informe de RED.BIN */
int guarda_cortafuegos(void);		/* escribe CORTAFUE.INI y se lo dice a RED.BIN */
void pon_regla_cf(u32 puerto, u32 de);	/* (CF_NADIE: que no entre; y la guarda) */
const char *nombre_puerto(u32 puerto);	/* «SSH»... o «» */
const char *texto_de(u32 de);
int pregunta_puerto(u32 puerto, u32 tarea);	/* (ventana.c) «¿Dejar que...?»: CF_... o -1 */
int ssh_encendido(void);
int apaga_ssh(void);
int actualiza_firmas(void);		/* lanza PAQUETES ACTUALIZA: 0 si ha ido bien */
/* lo mismo sin dejar la ventana parada: «cada» se llama unas diez veces por
 * segundo (pinta y atiende la ventana) y da 1 si se ha pulsado «Cancelar»;
 * mientras, act_hecho/act_total/act_texto dicen por dónde va (AVANCE.TXT de
 * PAQUETES) y, al acabar, act_dice es lo último que ha dicho. 0 bien, -1
 * mal, -2 sin PAQUETES, -3 cancelado */
int actualiza_firmas_con(int (*cada)(void));
extern u32 act_hecho, act_total;
extern char act_texto[100], act_dice[200];
extern int act_cancelando;

/* ventana.c */
int ventana_principal(const char *revisar_ya, int pestana);
int ventana_pregunta(char letra, const char *que);	/* 1: revisar */
#define H_DEJAR 0
#define H_CUARENTENA 1
#define H_BORRAR 2
#define H_ABRIR 3
int ventana_hallazgo(const char *ruta, const char *nombre, const char *dentro, int antes_de_abrir);
#endif
