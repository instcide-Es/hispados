/* compat.h - (COMPAT) el Asistente de compatibilidad de Hispa-Pentium
 * (COMPAT.BIN): qué le falta a un programa de Windows para correr con
 * WIN32.BIN, dicho claro y agrupado por DLL; el informe que se manda (con
 * permiso, cada vez); lo comprobado de cada usuario y el aviso cuando ya
 * está; y los componentes redistribuibles de Microsoft que se bajan de sus
 * URL oficiales. Todo en C portable con la biblioteca común de la oficina
 * (castel/nucleo32/oficina/comun) y lo de cada sistema en plat.h.
 * docs/PENTIUM.md, «El Asistente de compatibilidad».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_H
#define COMPAT_H
#include "so.h"

#define COMPAT_VERSION "1.0"
#define RUTA_MAX 260

/* ---- lo que dice WIN32 COMPRUEBA, leído (informe.c) ---- */
enum { A_VENTANAS, A_CONSOLA, A_PROGRAMA, A_DLL, A_NO_PE, A_NO_ESTA };
typedef struct {
	char nombre[64];	/* «HL.EXE» (UTF-8) */
	char ruta[RUTA_MAX];	/* la entera, si se sabe (para la suma); si no, «» */
	int clase;		/* A_* */
	u32 n_dll, n_fun, n_faltan;
	u32 tam;		/* (enviar.c) lo que mide */
	char sha[65];		/* (enviar.c) su SHA-256, o «» */
	int sin_importancia;	/* saber.c: el motivo (SI_*), o 0 si importa */
} c_archivo;

typedef struct {
	int archivo;		/* de quién */
	char dll[40];		/* «WINMM.DLL» */
	char fn[80];		/* «mixerOpen» o «#12»; «» si falta la DLL entera */
	u32 n;			/* (la DLL entera) cuántas funciones le pide */
	int no32;		/* (la DLL entera) está, pero no es de 32 bits */
} c_falta;

typedef struct {
	c_archivo *a;
	int n_a, cabe_a;
	c_falta *f;
	int n_f, cabe_f;
	u32 total_faltan;	/* lo que dice COMPRUEBA al final */
	int leido_total;	/* (ha llegado hasta «En total») */
	int maquina;		/* era la salida /MAQUINA */
} c_informe;

void inf_vacia(c_informe *i);
void inf_suelta(c_informe *i);
/* lee lo que ha escrito COMPRUEBA (UTF-8, ya pasado de la 437); 0 si se entiende */
int inf_lee(c_informe *i, const char *texto, u32 n);
/* las funciones distintas que faltan (sin repetir la misma de dos archivos);
 * «importantes»: sin contar las de los archivos que no hacen falta */
u32 inf_faltan(const c_informe *i, int solo_importantes);
/* la clave de una falta, para comparar: «WINMM.DLL!mixerOpen» */
void inf_clave(const c_falta *f, char *d, u32 cabe);

/* ---- lo que se sabe de las DLL y de los archivos (saber.c) ---- */
enum { SI_NADA, SI_INSTALADOR, SI_SERVIDOR, SI_GLIDE, SI_ACTUALIZA, SI_FALLOS, SI_STEAM, SI_EDITOR };
typedef struct {
	const char *dll;	/* en mayúsculas, con «.DLL» */
	const char *tema;	/* «Sonido», «Red»... */
	const char *para;	/* para qué sirve, en llano */
	int sin_importancia;	/* 1: no suele hacer falta (opcional) */
	const char *nota;	/* (o NULL) lo que hay que saber */
} c_saber;
const c_saber *saber_dll(const char *dll);
/* ¿es de los que no hacen falta para usar el programa (el instalador, el
 * servidor dedicado, lo de Glide...)? SI_* y, en *por_que, por qué */
int saber_archivo(const char *nombre, const char **por_que);
const char *saber_motivo(int si);
/* marca los archivos del informe que no hacen falta */
void saber_marca(c_informe *i);

/* ---- lo comprobado de cada usuario (historia.c) ---- */
typedef struct {
	char ruta[RUTA_MAX];	/* lo que se comprobó (un .EXE o una carpeta) */
	char nombre[64];	/* cómo se dice («Half-Life», o el nombre de la carpeta) */
	char win32[24];		/* la versión de win32 con que se comprobó */
	u32 faltan, importantes;
	char fecha[20];		/* «22/11/2026 21:30» */
	char id[9];		/* el de su archivo (P<id>.TXT): 8 cifras hexadecimales */
} c_programa;

int hist_carpeta(char *d, u32 cabe);		/* \USUARIOS\<USUARIO>\COMPAT, hecha */
int hist_lee(c_programa **p, int *n);		/* la lista (de so_pide) */
int hist_guarda(const c_programa *p, int n);
/* apunta (o pone al día) lo comprobado de «ruta» con su informe */
int hist_apunta(const char *ruta, const char *nombre, const char *win32, const c_informe *i);
/* las claves de lo que faltaba (de su P<id>.TXT): de so_pide, «clave\n» */
char *hist_faltas(const char *id);
int hist_olvida(const char *ruta);
void hist_id(const char *ruta, char id[9]);
/* el nombre para decir: el de la carpeta, o el del programa sin «.EXE» */
void hist_nombre(const char *ruta, char *d, u32 cabe);

/* ---- comprobar (comprueba.c) ---- */
/* corre WIN32 COMPRUEBA sobre «ruta» y lo lee; las sumas y los tamaños, si
 * «con_sumas». 0 si va bien; si no, *error */
int comprueba(const char *ruta, c_informe *i, int con_sumas, char *error, u32 cabe);
/* las versiones (de \HISPA\PAQUETES): de win32 y de base */
void version_win32(char *d, u32 cabe);
void version_hispa(char *d, u32 cabe);

/* ---- el informe que se manda (enviar.c) ---- */
/* el texto, exactamente lo que va (de so_pide; UTF-8, líneas con \n) */
char *env_texto(const c_informe *i, const char *win32, const char *hispa);
/* lo sube: 0 bien; 1 el servidor no lo quiere (*por_que); 2 sin red o no se
 * llega (se puede probar luego) */
int env_manda(const char *texto, char *por_que, u32 cabe);

/* ---- los avisos (revisa.c) ---- */
/* vuelve a comprobar lo de este usuario si win32 ha cambiado: los avisos,
 * uno por línea (de so_pide, o NULL si no hay nada que contar) */
char *revisa(int forzar);

/* ---- los componentes de Microsoft (redist.c) ---- */
typedef struct {
	char id[16];		/* «vc2010» */
	char nombre[96];
	char url[200];		/* https://download.microsoft.com/... */
	u32 tam;
	char sha[65];		/* el SHA-256 de lo que se baja (medido) */
	char licencia[64];	/* el archivo de la licencia dentro del paquete («3082\\eula.rtf»), o «» */
	char licencia_url[200];	/* y dónde leerla en la web */
	char interior[64];	/* el CAB de dentro donde van las DLL («vc_red.cab»), o «» */
	char dlls[256];		/* las DLL que se sacan, separadas por comas */
	char para[200];		/* para qué sirve */
} c_redist;
int red_lista(c_redist **r, int *n);		/* REDIST.LST */
/* ¿está ya puesto? (las DLL en \WIN32): 2 todo, 1 parte, 0 nada */
int red_puesto(const c_redist *r);
typedef void (*red_avance)(void *u, const char *que, u32 hecho, u32 total);
/* baja el paquete a la carpeta del usuario y comprueba su suma */
int red_baja(const c_redist *r, char *ruta, u32 cabe, red_avance av, void *u, char *error, u32 cabe_e);
/* el texto de la licencia que trae el paquete bajado (UTF-8, de so_pide) */
char *red_licencia(const c_redist *r, const char *ruta, char *error, u32 cabe_e);
/* saca las DLL a \WIN32 (con permiso de administrador): cuántas */
int red_saca(const c_redist *r, const char *ruta, red_avance av, void *u, char *error, u32 cabe_e);
/* (las pruebas y el anfitrión) sacar las DLL de un paquete a una carpeta */
int red_saca_a(const c_redist *r, const char *ruta, const char *carpeta, red_avance av, void *u, char *error, u32 cabe_e);

int red_cab(const char *ruta, const char *nombre, const char *destino, void (*di)(const char *f, ...), char *error, u32 cabe);

/* ---- la ventana (ventana.c) ---- */
int ventana_principal(const char *ruta, const char *fallo);
int ventana_aviso(const char *avisos);

/* ---- utilidades (comun.c) ---- */
char *lee_texto(const char *ruta, u32 *n, u32 max);	/* (de so_pide, con 0 detrás) */
int escribe_texto(const char *ruta, const char *t, u32 n);
void une_ruta(char *d, u32 cabe, const char *a, const char *b);
void a_mayusculas(char *s);
#endif
