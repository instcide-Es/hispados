/* escudo.h - ESCUDO.BIN, el antivirus de Hispa-Pentium: lo que se dicen el
 * motor (motor.c, inflar.c, cvd.c, sumas.c: sin nada de ninguna plataforma)
 * y la plataforma (pentium.c en Hispa-Pentium; anfitrion.c en Linux, para las
 * pruebas y para hacer FIRMAS.ESC en el VPS).
 *
 * El formato de las firmas es el de ClamAV (de Cisco Talos: docs.clamav.net,
 * «Signatures»), leído con un motor propio y pequeño; no lleva código de
 * libclamav. Ver castel/nucleo32/escudo/LEEME.txt y docs/PENTIUM.md, «Escudo».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ESCUDO_H
#define ESCUDO_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;

#define ESCUDO_VERSION "0.2"
/* el «nivel de funciones» de ClamAV que dice tener el motor: las firmas que
 * piden uno mayor se dejan (como hace ClamAV); las de uno menor se leen si
 * su formato se entiende (lo que no, se cuenta como «no soportado») */
#define ESCUDO_FLEVEL 230

/* ---- lo que da la plataforma ---- */
void *pl_reserva(u32 n);		/* malloc (0 si no hay) */
void *pl_rereserva(void *p, u32 n);	/* realloc */
void pl_libera(void *p);
int pl_abrir(const char *ruta);		/* para leer: <0 si no se puede */
int pl_crear(const char *ruta);		/* para escribir (vacío) */
int pl_leer(int h, void *d, u32 n);	/* los leídos (0: el final; <0: error) */
int pl_escribir(int h, const void *d, u32 n);
int pl_mover(int h, u32 pos);		/* desde el principio */
u32 pl_largo(int h);
void pl_cerrar(int h);
u32 pl_ms(void);			/* milisegundos desde algún momento */
void pl_apunta(const char *f, ...);	/* lo que pasa (el puerto serie / stderr): «escudo: ...» */
/* ¿hay que parar? (el botón «Cancelar»): lo mira el motor de vez en cuando */
int pl_cortar(void);

/* ---- la biblioteca (libc.c en Hispa-Pentium; la de verdad en Linux) ---- */
int formatear(char *d, int cabe, const char *p, ...);
#ifdef __STDC_HOSTED__
#if __STDC_HOSTED__ == 0
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
char *strchr(const char *, int);
char *strrchr(const char *, int);
#else
#include <string.h>
#endif
#endif

/* ---- las sumas (sumas.c) ---- */
typedef struct { u32 h[4]; u64 n; u8 b[64]; } md5_ctx;
typedef struct { u32 h[5]; u64 n; u8 b[64]; } sha1_ctx;
typedef struct { u32 h[8]; u64 n; u8 b[64]; } sha256_ctx;
void md5_inicia(md5_ctx *c);
void md5_suma(md5_ctx *c, const void *d, u32 n);
void md5_acaba(md5_ctx *c, u8 r[16]);
void sha1_inicia(sha1_ctx *c);
void sha1_suma(sha1_ctx *c, const void *d, u32 n);
void sha1_acaba(sha1_ctx *c, u8 r[20]);
void sha256_inicia(sha256_ctx *c);
void sha256_suma(sha256_ctx *c, const void *d, u32 n);
void sha256_acaba(sha256_ctx *c, u8 r[32]);

/* ---- descomprimir deflate (inflar.c): RFC 1951, sin ventana fija ----
 * Lee la entrada con «lee» (da los octetos que haya, 0 al final) y saca lo
 * descomprimido con «saca» (si da distinto de 0, se para). */
typedef struct inflar {
	int (*lee)(void *yo, u8 *d, u32 cabe);
	int (*saca)(void *yo, const u8 *d, u32 n);
	void *yo;
	/* (lo de dentro) */
	u8 ent[4096];
	u32 ent_n, ent_i;
	int ent_fin;
	u32 bits, nbits;
	u8 *ventana;			/* 32 K */
	u32 vpos;
	u64 total;			/* lo sacado */
	u64 tope;			/* no sacar más de esto (contra las «bombas»): 0, sin tope */
	u64 leidos;			/* lo que se ha leído de la entrada */
} inflar;
#define INF_BIEN 0
#define INF_MAL -1			/* los datos no son deflate */
#define INF_PARADO -2			/* «saca» ha dicho que pare */
#define INF_TOPE -3			/* pasa del tope */
#define INF_SIN_MEMORIA -4
int inflar_todo(inflar *z);		/* (y libera su ventana) */
/* gzip (RFC 1952): salta la cabecera y descomprime */
int inflar_gzip(inflar *z);

/* ---- los resultados ---- */
#define MAX_NOMBRE_VIRUS 96
#define MAX_DENTRO 200
typedef struct resultado {
	int hallado;			/* 1: hay algo */
	char nombre[MAX_NOMBRE_VIRUS];	/* el de la firma (de ClamAV) */
	char dentro[MAX_DENTRO];	/* dónde: «» (el archivo) o «DENTRO.ZIP > a.exe» */
	int cifrados;			/* miembros de un ZIP cifrados (no se pueden mirar) */
	int sin_mirar;			/* trozos que no se han mirado (demasiado grandes, formato que no se lleva) */
	u32 octetos;			/* lo mirado (con lo de dentro de los ZIP) */
	int cortado;			/* se ha cancelado */
} resultado;

/* ---- lo que se sabe de las firmas cargadas ---- */
typedef struct info_firmas {
	u32 sumas;			/* de archivo y de sección PE (en memoria o en FIRMAS.ESC) */
	u32 limpias;			/* las de «no es un virus» (.fp/.sfp) */
	u32 cuerpo;			/* .ndb */
	u32 logicas;			/* .ldb */
	u32 no_soportadas;		/* las que el motor no sabe mirar (y se dejan) */
	u32 pua;			/* las de «programas no deseados» (.hdu, .ndu...: no se cargan) */
	u32 fecha;			/* AAAAMMDD de lo más nuevo (de la cabecera del .cvd o de FIRMAS.ESC) */
	u32 version_daily, version_main, version_bytecode;
	u32 memoria_k;			/* lo que ocupan en memoria */
	char error[160];		/* si algo no ha ido bien (sigue con lo demás) */
} info_firmas;

/* ---- el motor (motor.c) ---- */
void motor_inicia(void);
/* carga un archivo de firmas: .hdb .hsb .mdb .msb .ndb .ldb .fp .sfp (texto
 * de ClamAV), .cvd/.cld (comprobando su firma digital) o FIRMAS.ESC (el
 * índice propio). Da 0 si bien. */
int motor_carga(const char *ruta, info_firmas *inf);
int motor_carga_como(const char *ruta, const char *nombre, info_firmas *inf);
/* cargar desde un texto ya en memoria (el nombre dice qué es: «x.ndb») */
int motor_carga_texto(const char *nombre, const char *t, u32 n, info_firmas *inf);
/* después de cargar todo: ordena y hace los índices */
void motor_prepara(info_firmas *inf);
void motor_suelta(void);
/* revisa un archivo (lo lee el motor) */
int motor_revisa_archivo(const char *ruta, const char *nombre, resultado *r);
/* revisa lo que hay en memoria */
int motor_revisa_memoria(const u8 *d, u32 n, const char *nombre, resultado *r);
/* el tope de lo que se lee entero en memoria para las firmas del cuerpo */
extern u32 motor_max_archivo;
extern int motor_zip;			/* mirar dentro de los ZIP */
/* hacer FIRMAS.ESC con lo cargado (anfitrion.c y ESCUDO /CONVIERTE) */
int motor_escribe_esc(const char *ruta, int familias, u32 fecha);
void motor_corpus(const u8 *d, u32 n);	/* (al hacer FIRMAS.ESC) un archivo limpio, para elegir los disparadores */	/* limite: octetos (0: sin)*/

/* ---- los .cvd (cvd.c) ---- */
typedef struct cabecera_cvd {
	char fecha_texto[40];
	u32 version, firmas, flevel;
	char md5[33];
	char firma[400];
	char autor[40];
	u32 segundos;			/* cuándo se hizo (desde 1970) */
} cabecera_cvd;
int cvd_cabecera(const u8 *c512, cabecera_cvd *c);
/* la firma digital de ClamAV (RSA de 1024 bits sobre la suma MD5 de lo que
 * va detrás de la cabecera): 1 si es buena */
int cvd_firma_buena(const cabecera_cvd *c, const u8 md5[16]);
/* recorre el .tar.gz de detrás de la cabecera (o el .tar sin comprimir),
 * dando cada archivo a «uno» (nombre, datos, largo) */
typedef int (*cvd_archivo)(void *yo, const char *nombre, const u8 *d, u32 n);
int cvd_recorre(int h, u32 desde, cvd_archivo uno, void *yo, char *error, int cabe);

/* ---- ayudas ---- */
u32 fecha_de_segundos(u32 s);		/* AAAAMMDD (UTC) */
int es_hex(int c);
int val_hex(int c);
#endif
