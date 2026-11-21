/* zip.h - el motor de los ZIP de Comprimidos (COMPRIM.BIN, Hispa-Pentium):
 * leer el directorio central (con ZIP64), sacar (deflate y guardado; con
 * contraseña ZipCrypto o AES de WinZip), probar, y hacer un ZIP nuevo o
 * rehacer uno añadiendo y quitando miembros. No sabe de discos: lo que
 * toca de archivos y carpetas lo da la plataforma (las pl_ de abajo: en
 * Hispa-Pentium, comprim.c con las llamadas al núcleo; en el anfitrión,
 * anfitrion.c con POSIX, para las pruebas y el fuzz con ASan).
 *
 * La seguridad (los ZIP vienen de Internet):
 *   - nombres con «..», absolutos («/x», «\x», «C:x»), con letras de
 *     control, vacíos a medias o de más de ZIP_HONDO carpetas: se ven en la
 *     lista, pero no se sacan nunca (ZIP_E_RUTA);
 *   - bombas: un tope de miembros (ZIP_MAX_MIEMBROS), de lo que mide todo
 *     descomprimido (zip_limites) y de la proporción de cada uno; los datos
 *     que se pisan (la bomba «solapada» de Fifield) se rechazan al abrir; y
 *     al descomprimir nunca se escribe más de lo que dice el directorio;
 *   - al añadir carpetas se baja como mucho ZIP_HONDO niveles (sin
 *     recursión sin fondo) y hasta ZIP_MAX_MIEMBROS archivos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ZIP_H
#define ZIP_H
#include <stdint.h>
#include <stddef.h>

#define ZIP_MAX_MIEMBROS 65000u
#define ZIP_HONDO 32
#define ZIP_MAX_NOMBRE 1024
#define ZIP_MAX_RUTA 520

/* los errores (negativos); zip_texto_error los dice en español */
enum {
	ZIP_BIEN = 0,
	ZIP_E_ABRIR = -1,	/* no se puede abrir (o no está) */
	ZIP_E_NO_ZIP = -2,	/* no es un ZIP (no tiene su directorio) */
	ZIP_E_ROTO = -3,	/* está roto: el directorio no cuadra */
	ZIP_E_MEMORIA = -4,
	ZIP_E_DEMASIADOS = -5,	/* más miembros de los que se admiten */
	ZIP_E_BOMBA = -6,	/* descomprimido mediría demasiado */
	ZIP_E_SOLAPADO = -7,	/* datos de un miembro que pisan los de otro */
	ZIP_E_RUTA = -8,	/* nombre peligroso (.., absoluto...) */
	ZIP_E_METODO = -9,	/* un método de compresión que no se sabe */
	ZIP_E_CLAVE = -10,	/* hace falta la contraseña, o no es esa */
	ZIP_E_CRC = -11,	/* los datos no dan su CRC (o su firma, en AES) */
	ZIP_E_DATOS = -12,	/* el deflate está mal */
	ZIP_E_LEER = -13,
	ZIP_E_ESCRIBIR = -14,	/* no se puede escribir (¿disco lleno?) */
	ZIP_E_CREAR = -15,	/* no se puede crear el archivo o la carpeta */
	ZIP_E_YA_ESTA = -16,	/* ya está y no se ha pedido reemplazarlo */
	ZIP_E_GRANDE = -17,	/* más de 4 GB: no cabe en FAT */
	ZIP_E_CORTADO = -18,	/* lo ha parado el usuario */
	ZIP_E_HONDO = -19,	/* carpetas de más de ZIP_HONDO niveles */
	ZIP_E_ENLACE = -20,	/* un enlace simbólico: no se saca */
	ZIP_E_NO_HAY = -21,	/* no hay tal miembro */
	ZIP_E_FUERTE = -22,	/* cifrado «fuerte» de PKWARE: no se sabe */
};
const char *zip_texto_error(int e);

/* ---- lo que da la plataforma ---- */
#define PL_LEER 0
#define PL_CREAR 2
int pl_abrir(const char *ruta, int modo);	/* >= 0 su manejador; < 0, no */
int32_t pl_leer(int h, void *b, uint32_t n);	/* los leídos (0 al final), < 0 error */
int32_t pl_escribir(int h, const void *b, uint32_t n);	/* los escritos */
int pl_ir(int h, uint64_t pos);			/* 0 bien */
uint64_t pl_largo(int h);
int pl_cortar(int h);				/* corta el archivo donde está (0 bien) */
void pl_cerrar(int h);
int pl_que_es(const char *ruta);		/* 0 no está; 1 archivo; 2 carpeta */
int pl_hacer_carpeta(const char *ruta);		/* 0 bien (o ya estaba) */
int pl_borrar(const char *ruta);
int pl_renombrar(const char *de, const char *a);	/* (de un archivo, en la misma carpeta) */
char pl_barra(void);				/* '\\' o '/' */
/* el nombre de aquí para el trozo «comp» (UTF-8) de un miembro, dentro de
 * la carpeta (ya de aquí) «carpeta»: en Hispa-Pentium, de 8+3 */
void pl_nombre_local(const char *carpeta, const char *comp, int es_carpeta, char *sale, int cabe);
/* lo que hay en una carpeta: llama a «cada» con el nombre de dentro del ZIP
 * (UTF-8, el largo si lo hay), el de aquí, si es carpeta, lo que mide y su
 * fecha y hora del DOS. Da < 0 si no se puede leer */
typedef int (*pl_cada)(void *u, const char *largo, const char *local, int carpeta, uint64_t tam, uint16_t fecha, uint16_t hora);
int pl_listar(const char *carpeta, pl_cada cada, void *u);
/* de un archivo suelto: su nombre largo (UTF-8), si es carpeta, lo que mide y su fecha */
int pl_ficha(const char *ruta, char *largo, int cabe, int *carpeta, uint64_t *tam, uint16_t *fecha, uint16_t *hora);
void pl_poner_fecha(const char *ruta, uint16_t fecha, uint16_t hora);	/* (puede no hacer nada) */
/* cómo va (para la barra de progreso); da distinto de 0 si hay que parar */
int pl_progreso(uint64_t hecho, uint64_t total, const char *nombre);

/* ---- un ZIP abierto ---- */
#define ZC_NADA 0
#define ZC_ZIPCRYPTO 1
#define ZC_AES 2
typedef struct {
	char *nombre;		/* UTF-8, con «/» (las carpetas acaban en «/») */
	uint64_t comp, tam, local;	/* comprimido, descomprimido, dónde su cabecera */
	uint32_t crc;
	uint16_t metodo, marcas, fecha, hora;
	uint16_t metodo_real;	/* (AES) el de debajo */
	uint8_t carpeta, cifrado, aes_fuerza, aes_version;
	int8_t peligro;		/* 0, o el error por el que no se saca (ZIP_E_RUTA...) */
	uint8_t enlace;
	uint16_t nombre_n, extra_n;	/* (del directorio central: para copiar el miembro tal cual) */
	uint32_t ext_atrib;
	uint16_t hecho_por, version;
	uint32_t central;	/* dónde está su entrada en el directorio (en la memoria) */
} zmiembro;

typedef struct {
	int h;
	uint64_t largo;
	zmiembro *m;
	uint32_t n;
	uint8_t *dir;		/* el directorio central, en la memoria */
	uint64_t dir_n, dir_en;
	uint64_t total_tam, total_comp;
	int zip64, cifrados, peligrosos;
} zarchivo;

typedef struct {
	uint64_t max_total;	/* lo que puede medir todo lo que se saca */
	uint64_t max_uno;	/* y cada miembro */
	uint32_t max_proporcion;	/* descomprimido / comprimido, para los de más de 16 MB */
} zlimites;
extern zlimites zip_limites;

int zip_abrir(zarchivo *z, const char *ruta);
void zip_cerrar(zarchivo *z);
/* ¿hace falta contraseña para alguno de los elegidos (NULL: todos)? */
int zip_pide_clave(const zarchivo *z, const uint8_t *elegidos);
/* saca el miembro i a «destino» (un archivo de aquí); con destino NULL solo
 * lo prueba (descomprime, mira el CRC y no escribe) */
int zip_sacar(zarchivo *z, uint32_t i, const char *destino, const char *clave);
/* saca los elegidos (NULL: todos) debajo de «carpeta», con sus carpetas;
 * «quita» es lo que se les quita del principio del nombre (la carpeta del
 * ZIP en la que se está, «» si ninguna). reemplazar: 0, lo que ya está se
 * deja (y se cuenta); 1, se reemplaza. Lo que no se ha podido sacar se
 * cuenta en *fallos (y el último error se da) */
typedef struct {
	uint32_t sacados, saltados, fallos;
	int ultimo_error;
	char ultimo[ZIP_MAX_NOMBRE];	/* el último que ha fallado */
	char ultimo_local[ZIP_MAX_RUTA];	/* y el último sacado, con su ruta de aquí */
} zcuenta;
int zip_sacar_todo(zarchivo *z, const char *carpeta, const uint8_t *elegidos, const char *quita, const char *clave, int reemplazar, zcuenta *c);

/* ---- hacer un ZIP ---- */
typedef struct {
	char *local;		/* la ruta de aquí */
	char *nombre;		/* el nombre dentro del ZIP (UTF-8; las carpetas acaban en «/») */
	uint64_t tam;
	uint16_t fecha, hora;
	uint8_t carpeta;
} zfuente;
typedef struct {
	zfuente *f;
	uint32_t n, cabe;
	int error;
} zfuentes;
/* añade a la lista la ruta de aquí (un archivo o una carpeta con todo lo de
 * dentro) con el nombre «base» dentro del ZIP (NULL: el suyo) */
int zip_juntar(zfuentes *l, const char *ruta, const char *base);
extern const char *zip_excluir[2];	/* lo que no se mete (el ZIP que se hace y su temporal) */
void zip_suelta_fuentes(zfuentes *l);
/* hace «ruta» con lo que tenga «viejo» (si no es NULL), menos lo que diga
 * «quitar» (un octeto por miembro; NULL, nada), más lo de «nuevos» (lo que
 * se llame igual que uno viejo, lo reemplaza). nivel: 0 (solo guardar) a 9.
 * Se escribe aparte (temporal) y luego se cambia por el de antes */
int zip_hacer(const char *ruta, const char *temporal, zarchivo *viejo, const uint8_t *quitar, zfuentes *nuevos, int nivel);

/* ---- los nombres ---- */
int utf8_a_437(const char *u, char *s, int cabe);	/* da el largo */
int de_437_a_utf8(const unsigned char *t, int n, char *s, int cabe);
uint32_t zip_crc32(uint32_t crc, const void *p, size_t n);
#endif
