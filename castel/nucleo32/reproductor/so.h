/* so.h - el Reproductor: lo que el programa pide al sistema. Es lo único que
 * cambia de un sistema a otro: so_pentium.c (Hispa-Pentium, el anillo 3 con
 * la int 0x80) y so_anfitrion.c (Linux, para las pruebas y las capturas; y
 * lo mismo valdrá para Hispa-64). Todo lo demás de castel/nucleo32/reproductor
 * es C99 portable, sin nada del sistema (como Hispa-PDF).
 *
 * Los textos que se ven van en UTF-8; las rutas, tal como las quiere el
 * sistema (en Hispa-Pentium, la página 437): so_a_utf8() las pasa a UTF-8
 * para enseñarlas y so_de_utf8() al revés.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef REPRO_SO_H
#define REPRO_SO_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int16_t i16;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

#define MAX_RUTA 260

/* ---- la memoria (malloc, realloc y free de siempre; NULL si no hay) ---- */
void *so_pide(size_t n);
void *so_repide(void *p, size_t n);
void so_suelta(void *p);
u32 so_memoria_usada(void);		/* lo pedido ahora (para medir) */

/* ---- los archivos, a trozos ---- */
typedef struct so_archivo so_archivo;
so_archivo *so_abre(const char *ruta);	/* para leer; NULL si no se puede */
int so_lee(so_archivo *a, void *d, u32 n);	/* los leídos (0: el final; -1: error) */
int so_salta_a(so_archivo *a, u64 pos);	/* 0 si va bien */
u64 so_largo(so_archivo *a);
u64 so_donde(so_archivo *a);
void so_cierra(so_archivo *a);
so_archivo *so_crea(const char *ruta);	/* para escribir (lo vacía); NULL si no */
int so_escribe(so_archivo *a, const void *d, u32 n);	/* 0 si va bien */
int so_borra(const char *ruta);
int so_haz_carpeta(const char *ruta);
int so_existe(const char *ruta, int *es_carpeta);	/* 1 si está */
/* un archivo entero (de so_pide; con un 0 detrás); NULL si no está o mide
 * más de «max» */
u8 *so_lee_todo(const char *ruta, u32 *largo, u32 max);

/* ---- las carpetas ---- */
typedef struct {
	char nombre[MAX_RUTA];		/* el que se usa para abrirlo (en el sistema) */
	char visto[MAX_RUTA];		/* el que se enseña (UTF-8: el largo, si lo tiene) */
	int carpeta;
	u64 tam;
} so_entrada;
void *so_abre_carpeta(const char *ruta);
int so_siguiente(void *c, so_entrada *e);	/* 1, o 0 si no hay más */
void so_cierra_carpeta(void *c);
/* las unidades que hay: hasta «cabe» letras en «letras» (y en «que», por
 * cada una, 'C' disco, 'U' memoria USB, 'D' lector de CD o DVD, 'I' imagen,
 * 'M' memoria); las cuenta */
int so_unidades(char *letras, char *que, int cabe);
char so_separador(void);		/* '\\' en Hispa-Pentium, '/' en Linux */
/* la carpeta donde el programa guarda lo suyo (con el separador detrás) */
void so_carpeta_propia(char *d, int cabe);
/* la carpeta para empezar a buscar música */
void so_carpeta_musica(char *d, int cabe);

/* los nombres del sistema, a UTF-8 y al revés */
void so_a_utf8(const char *sis, char *d, int cabe);
void so_de_utf8(const char *utf, char *d, int cabe);

/* ---- el tiempo ---- */
u32 so_ms(void);			/* milésimas desde que arrancó (da la vuelta) */
void so_duerme(u32 ms);
u32 so_ciclos(void);			/* para medir lo que cuesta algo (rdtsc o así) */

/* ---- lo que dice el programa ---- */
void so_dice(const char *t);		/* a la consola o al terminal (UTF-8) */
void so_registro(const char *t);	/* al registro (el puerto serie en Pentium) */
int so_argumentos(char *d, int cabe);	/* la línea de los argumentos */
int so_en_consola(void);		/* 1 si no hay escritorio o va en un terminal */
int so_tecla(void);			/* (en la consola) una tecla, o 0 si no hay */

/* ---- el sonido ---- */
/* abre la salida a esa frecuencia (estéreo de 16 bits); 0 si no hay
 * tarjeta. so_hz_tarjeta(), la de la tarjeta (para no remuestrear dos veces) */
u32 so_hz_tarjeta(void);
int so_sonido_abre(u32 hz);
/* da las tramas (estéreo de 16 bits) que quepan sin esperar; devuelve las
 * que han entrado */
u32 so_sonido_da(const i16 *d, u32 tramas);
u32 so_sonido_en_cola(void);		/* las tramas que aún no han sonado */
u64 so_sonido_tocadas(void);		/* las que han salido desde que se abrió */
void so_sonido_vacia(void);		/* tira lo que haya en la cola */
void so_sonido_pausa(int si);
void so_sonido_volumen(int v);		/* 0 a 100: el de este programa en el mezclador */
int so_sonido_volumen_general(int v);	/* -1: solo preguntar */
void so_sonido_cierra(void);

/* ---- el CD de música (la llamada 134 en Pentium) ---- */
typedef struct {
	int pistas;			/* 0 si no es un CD de música */
	int primera;
	struct { u32 lba; u8 control; u8 numero; } pista[100];	/* y pista[pistas]: el final */
} so_toc;
/* los lectores de CD: sus letras */
int so_lectores_cd(char *letras, int cabe);
int so_cd_toc(char letra, so_toc *t);	/* 0 si va bien */
/* n sectores de sonido (2352 octetos cada uno) desde el lba; 0 si va bien */
int so_cd_lee(char letra, u32 lba, u32 n, void *d);
int so_cd_expulsa(char letra);

/* ---- las ventanas ---- */
#define EV_NADA 0
#define EV_TECLA 1		/* a: la tecla (T_...); b: CON_MAYUS... */
#define EV_BAJA 2		/* a, b: dónde; c: los botones y las teclas de cambio, y las veces (2 o 3) */
#define EV_SUBE 3
#define EV_MUEVE 4
#define EV_CERRAR 5
#define EV_TAMANO 6		/* a, b: lo que mide ahora */
#define EV_FOCO 7
#define EV_SALE 9
#define EV_MENU 10		/* a: la opción (SIN_ELEGIR); b: la etiqueta */
#define EV_UNIDADES 12
#define EV_RUEDA 13		/* a, b: dónde; c: los pasos (con signo, el octeto bajo) */
#define EV_SOLTADO 14		/* (REPRODUCTOR) se ha soltado algo de Mi equipo: so_soltado() */
#define EV_BANDEJA 15		/* (REPRODUCTOR) el icono de la bandeja: a, lo elegido (SIN_ELEGIR: un clic) */
#define SIN_ELEGIR 0xFFFFFFFFu

#define B_IZQUIERDO 1
#define B_DERECHO 2
#define CON_MAYUS 0x100
#define CON_CONTROL 0x200
#define VECES(c) (((c) >> 16) & 0xFF)

/* las teclas (como las da Hispa-Pentium; en Linux, las mismas) */
#define T_ESPECIAL 0x100
#define T_ARRIBA 0x148
#define T_ABAJO 0x150
#define T_IZQUIERDA 0x14B
#define T_DERECHA 0x14D
#define T_INICIO 0x147
#define T_FIN 0x14F
#define T_SUPR 0x153
#define T_RE_PAG 0x149
#define T_AV_PAG 0x151
#define T_F1 0x13B
#define T_INSERT 0x152

typedef struct { u32 clase, a, b, c; } so_suceso;

/* un lienzo: 0x00RRGGBB, fila a fila */
typedef struct { int an, al; u32 *p; } lienzo;

int so_hay_escritorio(void);
u32 so_ventana(const char *titulo, int an, int al);	/* 0 si no se puede */
void so_titulo(u32 v, const char *t);
void so_vuelca(u32 v, const lienzo *l, int x, int y, int an, int al);	/* ese trozo, a la ventana */
int so_suceso_de(u32 v, so_suceso *s);	/* 1 si había uno */
void so_cambia_tamano(u32 v, int an, int al);	/* lo de dentro de la ventana */
void so_trae(u32 v);			/* delante (y sin minimizar) */
int so_menu(u32 v, int x, int y, const char *opciones, u32 grises, u32 etiqueta);
int so_soltado(char *ruta, int cabe);	/* lo soltado (EV_SOLTADO): la ruta */
/* el icono de la bandeja (16 x 16, 0xAARRGGBB), su ayuda y las opciones de su
 * menú (separadas por «|»); NULL lo quita */
void so_bandeja(u32 v, const u32 *icono, const char *ayuda, const char *opciones);
void so_al_portapapeles(const char *t);
/* abrir otro programa (Mi equipo en una carpeta...) */
void so_lanza(const char *programa, const char *args);

/* ---- una sola copia: si ya hay un Reproductor en marcha, se le pasa lo
 * pedido y se acaba ---- */
int so_otro_en_marcha(const char *pedido);	/* 1 si se le ha pasado */
int so_pedido_nuevo(char *d, int cabe);	/* (el que está) 1 si le han pedido algo */

/* ---- el salto de los errores sin vuelta ---- */
/* la coma flotante: la libm, o lo del x87 en Pentium (en so_pentium.c) */
#endif
