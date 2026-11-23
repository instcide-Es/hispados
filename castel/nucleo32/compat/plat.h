/* plat.h - (COMPAT) lo que el Asistente pide al sistema y no está en la
 * biblioteca común de la oficina (oficina/comun/so.h): correr WIN32
 * COMPRUEBA con su salida a un archivo, las carpetas del usuario, el azar,
 * el modo administrador, leer un archivo grande a trozos, la red (TCP y
 * TLS) y colocar la ventanita del aviso. plat_pentium.c (Hispa-Pentium) y
 * plat_anfitrion.c (Linux, para las pruebas).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef COMPAT_PLAT_H
#define COMPAT_PLAT_H
#include "so.h"

/* corre «WIN32 COMPRUEBA args» con lo que escribe en el archivo «salida»
 * (de la 437 tal cual). Mientras, llama a «espera» (para repintar), si no es
 * NULL. 0 si ha acabado (*codigo, el suyo); -1 si no se puede (*error) */
int plat_comprueba(const char *args, const char *salida, int *codigo, void (*espera)(void *), void *u, char *error, u32 cabe);
/* la carpeta del usuario: «C:\USUARIOS\CARLOS» (UTF-8, sin «\» detrás) */
int plat_carpeta_usuario(char *d, u32 cabe);
/* WIN32.LOG (la misma que usa WIN32.BIN) */
int plat_ruta_log(char *d, u32 cabe);
/* la carpeta de lo del sistema del Asistente («C:\HISPA\COMPAT») y la de las DLL («C:\WIN32») */
void plat_carpeta_compat(char *d, u32 cabe);
void plat_carpeta_win32(char *d, u32 cabe);
/* \HISPA\PAQUETES */
void plat_carpeta_paquetes(char *d, u32 cabe);
int plat_haz_carpeta(const char *ruta);
int plat_azar(u8 *d, u32 n);
/* el modo administrador: ¿se tiene? ¿se puede pedir (lo confirma el núcleo)? */
int plat_es_admin(void);
int plat_pide_admin(void);
void plat_deja_admin(void);
/* colocar la ventana v en (x, y) de la pantalla */
void plat_coloca(int v, int x, int y);
/* lanzar otro programa suelto (COMPAT.BIN con argumentos, el .EXE) */
int plat_lanza(const char *programa, const char *args);
/* leer un archivo grande a trozos */
void *plat_abre(const char *ruta, u32 *tam);
i32 plat_lee_en(void *f, u32 pos, void *d, u32 n);
void plat_cierra(void *f);

/* escribir en la Consola (o en la salida, en Linux): UTF-8 */
void plat_escribe(const char *t);
/* ¿hay escritorio? (si no, sin ventanas: se dice en la Consola) */
int plat_hay_escritorio(void);

/* ---- la red ---- */
typedef struct plat_con plat_con;
/* conectar con host:puerto (cifrado si «tls»): NULL y *error si no */
plat_con *plat_conecta(const char *host, u32 puerto, int tls, char *error, u32 cabe);
int plat_envia(plat_con *c, const void *d, u32 n);		/* 0 bien */
i32 plat_recibe(plat_con *c, void *d, u32 cabe, u32 segundos);	/* los leídos; 0 al cerrar; -1 mal */
void plat_desconecta(plat_con *c);
/* ¿hay red? (espera hasta «segundos» a que la haya) */
int plat_hay_red(u32 segundos);
#endif
