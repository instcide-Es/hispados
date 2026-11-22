/* so.h - Correo: lo único que el programa pide al sistema. Es lo único que
 * cambia de un sistema a otro: so_pentium.c (Hispa-Pentium, el anillo 3 con
 * la int 0x80, la red de RED.BIN y TLS.BIN) y so_anfitrion.c (Linux: las
 * pruebas con ASan, y el modelo para Hispa-64). Todo lo demás de
 * castel/nucleo32/correo es C99 portable, sin nada del sistema (como
 * Hispa-PDF, castel/nucleo32/pdf/so.h).
 *
 * Los textos van siempre en UTF-8 dentro del programa; el sistema los pasa a
 * su página al pintarlos o al usarlos como ruta (so_pentium.c: la 437 de
 * Hispa, con Á, Í, Ó, Ú y el euro).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_SO_H
#define CORREO_SO_H
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

/* ---- la memoria (malloc, realloc y free de siempre; NULL si no hay) ---- */
void *so_pide(size_t n);
void *so_repide(void *p, size_t n);
void so_suelta(void *p);
/* lo que lleva pedido el programa (para los límites y las medidas) */
u32 so_memoria_usada(void);

/* ---- los archivos (las rutas, en UTF-8 y con la barra del sistema:
 * so_barra(); 8+3 en Hispa-Pentium) ---- */
char so_barra(void);
/* entero a la memoria (de so_pide, con un 0 detrás que no cuenta); NULL si no
 * se puede o si mide más de «max» */
u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max);
#define SO_LEER 0
#define SO_CREAR 1	/* crearlo o dejarlo vacío */
#define SO_ANADIR 2	/* escribir al final (creándolo si no está) */
#define SO_CAMBIAR 3	/* leer y escribir, sin vaciarlo (y crearlo si no está) */
void *so_abre(const char *ruta, int modo);
int so_lee(void *f, void *d, u32 n);			/* los leídos, 0 al final, <0 error */
int so_escribe(void *f, const void *d, u32 n);	/* 0 si se ha escrito todo */
int so_posicion(void *f, u32 pos);			/* ir a «pos» desde el principio; 0 si va */
u32 so_largo(void *f);
int so_cierra(void *f);				/* 0 si va */
int so_borra(const char *ruta);
int so_renombra(const char *de, const char *a);	/* (en la misma carpeta, o a otra) */
int so_haz_carpeta(const char *ruta);		/* 0 si la ha hecho o ya estaba */
int so_existe(const char *ruta, int *es_carpeta);
/* lo de una carpeta: f() por cada cosa (sin . ni ..); si f da distinto de 0, se para */
int so_carpeta(const char *ruta, int (*f)(void *u, const char *nombre, u32 tam, int es_carpeta), void *u);

/* dónde guarda el correo: la carpeta del usuario (\USUARIOS\<USUARIO>\CORREO
 * si hay usuarios) o \HISPA\CORREO (o \CORREO sin \HISPA), que es un
 * secreto de CORREO.BIN: solo lo lee él. Sin la barra del final */
int so_carpeta_datos(char *d, int cabe);
/* una carpeta de paso para los adjuntos que se abren con otro programa (la
 * tiene que poder leer ese programa, y Escudo): X:\CORREO.TMP, o la que haya */
int so_carpeta_paso(char *d, int cabe);
/* la de los documentos del usuario (donde se guarda un adjunto, de entrada) */
int so_carpeta_documentos(char *d, int cabe);

/* ---- el tiempo ---- */
u32 so_tics(void);
u32 so_hz(void);
void so_duerme(u32 ms);
/* ahora, en segundos desde 1970 (UTC), y la diferencia de la hora del equipo
 * con la UTC, en minutos (en Hispa-Pentium, el reloj del PC va en hora de
 * España: +60 o +120) */
i64 so_ahora(void);
int so_zona(i64 cuando);
void so_azar(void *d, u32 n);

/* ---- lo que pasa: «correo: ...» al puerto serie (las pruebas lo leen) o a
 * la salida de error en Linux ---- */
void so_apunta(const char *f, ...);
/* lo que se dice en la consola (las órdenes: CORREO /ORDEN ...) */
void so_dice(const char *t);

/* ---- la red ----
 * Una conexión: TCP (sin cifrar: solo si la cuenta lo permite, para los
 * servidores locales), TLS desde el principio (993, 465, 995) o con
 * STARTTLS (143, 587, 110): lo plano de antes del apretón de manos lo hace el
 * sistema (TLS.BIN en Hispa-Pentium), que solo da la conexión cuando ya está
 * cifrada y con el certificado y el nombre comprobados. */
#define SO_PLANO 0
#define SO_TLS 1
#define SO_STARTTLS_IMAP 2
#define SO_STARTTLS_SMTP 3
#define SO_STARTTLS_POP3 4
/* los errores (negativos) */
#define SO_E_ESPERA (-2)	/* (so_recibe) no ha llegado nada aún */
#define SO_E_RED (-3)		/* no hay red */
#define SO_E_NOMBRE (-4)	/* no se encuentra el servidor */
#define SO_E_CONEXION (-5)	/* no contesta, o rechaza la conexión */
#define SO_E_CERTIFICADO (-6)	/* el certificado no vale (o es de otro nombre) */
#define SO_E_TLS (-7)		/* la conexión segura no se ha podido hacer */
#define SO_E_STARTTLS (-8)	/* el servidor no ofrece STARTTLS */
#define SO_E_CORTADA (-9)	/* se ha cortado */
#define SO_E_OTRO (-10)
/* da el número de la conexión (>0) o un error; en «error», el porqué en
 * español (el texto de TLS.BIN, con el detalle del certificado) */
int so_conecta(const char *servidor, u32 puerto, int modo, u32 espera_ms, char *error, int cabe);
int so_envia(int c, const void *d, u32 n);		/* 0 si se ha enviado todo */
/* lo que haya llegado (esperando como mucho espera_ms): n > 0, 0 si el
 * servidor ha cerrado, SO_E_ESPERA o un error */
int so_recibe(int c, void *d, u32 cabe, u32 espera_ms);
void so_cierra_con(int c);
/* «TLS 1.2, ECDHE-ECDSA, ChaCha20-Poly1305»; 0 si no está cifrada */
int so_cifrado(int c, char *d, int cabe);
/* una pregunta de DNS (el paquete entero, por UDP al servidor DNS del
 * equipo): los octetos de la respuesta, o <0 si no hay (o si el sistema no
 * sabe preguntarlo). Para los registros SRV (autocfg.c) */
int so_dns(const u8 *pregunta, u32 n, u8 *respuesta, u32 cabe, u32 espera_ms);
/* ¿hay red? (sin esperar) */
int so_hay_red(void);

/* ---- los otros programas ---- */
/* lanzar y esperar a que acabe: su código de salida, o <0 si no se ha podido */
int so_lanza_espera(const char *programa, const char *argumentos);
/* lanzar y soltarlo */
int so_lanza(const char *programa, const char *argumentos);
/* ¿existe ese programa del sistema? (\HISPA\X.BIN o \X.BIN de la unidad del
 * sistema): su ruta entera, en «ruta» */
int so_programa(const char *nombre, char *ruta, int cabe);
/* los argumentos del programa (UTF-8) */
int so_argumentos(char *d, int cabe);

/* ---- el portapapeles (texto UTF-8) ---- */
int so_al_portapapeles(const char *t, u32 n);
char *so_del_portapapeles(u32 *n);	/* de so_pide; NULL si no hay */

/* ---- (PUNTEROS) el puntero del ratón: la forma de una zona de la ventana
 * (la llamada 177: 7, la «I» de escribir; con an = 0, se quitan todas). El
 * núcleo la pone sin esperar al programa (Correo no recibe el ratón que
 * solo se mueve). En Linux, nada. ---- */
#define SO_P_FLECHA 0
#define SO_P_TEXTO 7
void so_puntero_zona(int x, int y, int an, int al, int forma);

/* ---- el salto de los errores sin vuelta ---- */
#if defined(CORREO_PENTIUM)
typedef u32 so_salto[8];
int so_marca(so_salto s) __attribute__((returns_twice));
void so_salta(so_salto s, int v) __attribute__((noreturn));
#else
#include <setjmp.h>
typedef jmp_buf so_salto;
#define so_marca(s) setjmp(s)
#define so_salta(s, v) longjmp(s, v)
#endif

#endif
