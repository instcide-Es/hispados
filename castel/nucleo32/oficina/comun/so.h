/* so.h - la oficina de 32 bits (Pluma 2, y luego Cuentas 2...): lo que los
 * programas piden al sistema. Es lo único que cambia de un sistema a otro:
 * so_pentium.c (Hispa-Pentium, el anillo 3 con la int 0x80) y
 * so_anfitrion.c (Linux: las pruebas, con una ventana de mentira que se
 * maneja con un guion de sucesos y deja capturas; y el modelo para
 * Hispa-64). Todo lo demás de castel/nucleo32/oficina es C99 portable.
 * Sigue el patrón de Hispa-PDF (castel/nucleo32/pdf/so.h), con la ventana.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_SO_H
#define OFI_SO_H
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

/* ---- la memoria (malloc, realloc y free de siempre; NULL si no hay) ---- */
void *so_pide(size_t n);
void *so_repide(void *p, size_t n);
void so_suelta(void *p);
u32 so_memoria_usada(void);	/* lo pedido ahora (para medir) */

/* ---- los archivos (las rutas, en UTF-8; el sistema las pasa a su página) ---- */
/* un archivo entero a la memoria (de so_pide; con un 0 detrás); NULL si no se
 * puede o si mide más de «max». En *error, por qué (en español) */
u8 *so_lee_archivo(const char *ruta, u32 *largo, u32 max, const char **error);
/* escribir un archivo a trozos: abrirlo (creándolo o vaciándolo), los trozos
 * y cerrarlo. 0 si va bien */
void *so_crea(const char *ruta, const char **error);
int so_escribe(void *f, const void *d, u32 n);
int so_cierra(void *f);
int so_borra(const char *ruta);
int so_renombra(const char *de, const char *a);
/* ¿existe? (1 archivo, 2 carpeta, 0 no) */
int so_existe(const char *ruta);
/* las carpetas: abrirla, la entrada siguiente (1; 0 al acabar) y cerrarla */
typedef struct {
	char nombre[260];	/* el largo si lo tiene; si no, el 8+3 (UTF-8) */
	char corto[13];		/* el 8+3 (en Hispa-Pentium) */
	u32 tamano;
	int carpeta;
} so_entrada;
void *so_abre_carpeta(const char *ruta);
int so_lee_carpeta(void *c, so_entrada *e);
void so_cierra_carpeta(void *c);
/* ¿crea el sistema nombres largos? (Hispa-Pentium aún no: 8+3) */
int so_nombres_largos(void);
/* la unidad del sistema («C»), para las rutas fijas (\HISPA\..., \DOCUMENT\...) */
char so_unidad_sistema(void);

/* ---- lo demás del sistema ---- */
/* lo que dice el programa para quien mira (UTF-8): el puerto serie en
 * Hispa-Pentium, stderr en Linux; con «printf» (oficina/comun/texto.h) */
void so_dice(const char *t);
int so_argumentos(char *d, int cabe);	/* los argumentos (UTF-8) */
u32 so_tics(void);			/* los tics desde que arrancó */
u32 so_hz(void);			/* cuántos tics son un segundo */
void so_duerme(u32 tics);
/* la fecha y la hora de ahora (año completo, mes 1-12...) */
void so_ahora(int *ano, int *mes, int *dia, int *hora, int *min, int *seg);

/* el salto de los errores sin vuelta (sin memoria): como setjmp/longjmp */
#if defined(OFI_PENTIUM)
typedef u32 so_salto[8];
int so_marca(so_salto s) __attribute__((returns_twice));
void so_salta(so_salto s, int v) __attribute__((noreturn));
#else
#include <setjmp.h>
typedef jmp_buf so_salto;
#define so_marca(s) setjmp(s)
#define so_salta(s, v) longjmp(s, v)
#endif

/* la coma flotante (la libm, o el x87 en Pentium) */
double so_raiz(double x);
double so_suelo(double x);
double so_techo(double x);
double so_abs(double x);

/* ---- la ventana ---- */
/* las teclas: las letras, en Unicode (el sistema las pasa de su página);
 * las demás, de SO_T_BASE para arriba. Ctrl+letra llega como su número de
 * control (Ctrl+A = 1 ... Ctrl+Z = 26), con SO_CON_CONTROL en «mods» */
#define SO_T_BASE 0x110000u
enum {
	SO_T_ARRIBA = SO_T_BASE + 1, SO_T_ABAJO, SO_T_IZQUIERDA, SO_T_DERECHA,
	SO_T_INICIO, SO_T_FIN, SO_T_RE_PAG, SO_T_AV_PAG, SO_T_SUPR, SO_T_INSERTAR,
	SO_T_F1, SO_T_F2, SO_T_F3, SO_T_F4, SO_T_F5, SO_T_F6, SO_T_F7, SO_T_F8,
	SO_T_F9, SO_T_F10, SO_T_F11, SO_T_F12, SO_T_MAYUS_TAB, SO_T_CTRL_TAB,
	SO_T_ALT			/* + (letra - 'a'): Alt+letra, los menús */
};
#define SO_RETROCESO 8
#define SO_TAB 9
#define SO_INTRO 10
#define SO_ESC 27

#define SO_CON_MAYUS 1
#define SO_CON_CONTROL 2
#define SO_BOTON_IZQ 1
#define SO_BOTON_DER 2
#define SO_BOTON_CEN 4

enum {
	SO_NADA, SO_TECLA, SO_BAJA, SO_SUBE, SO_MUEVE, SO_CERRAR, SO_TAMANO,
	SO_FOCO, SO_SALE, SO_MENU, SO_RUEDA
};
typedef struct {
	int clase;
	int x, y;		/* el ratón (dentro de la ventana); SO_TAMANO: ancho y alto */
	u32 tecla;		/* SO_TECLA */
	int mods;		/* SO_CON_MAYUS | SO_CON_CONTROL */
	int botones;		/* los pulsados */
	int veces;		/* 1, 2 (doble clic), 3 */
	int pasos;		/* SO_RUEDA: con signo, hacia arriba positivos */
	int opcion;		/* SO_MENU: la elegida (desde 0; -1 ninguna) */
	u32 etiqueta;		/* SO_MENU: la que se dio al pedirlo */
	int foco;		/* SO_FOCO: 1 si pasa a ser la activa */
} so_suceso;

/* una ventana nueva (lo de dentro mide an x al); su número (0: no se puede) */
int so_ventana(const char *titulo, int an, int al);
void so_titulo(int v, const char *t);
void so_maximiza(int v);
void so_centra(int v);
void so_cierra_ventana(int v);
/* lo que mide la pantalla (para el tamaño de la ventana) */
void so_pantalla(int *an, int *al);
/* un trozo de puntos 0x00RRGGBB (de «paso» puntos por fila) a la ventana */
void so_mapa(int v, int x, int y, int an, int al, const u32 *px, int paso);
/* el suceso siguiente (1; 0 si no hay). Con «espera», hasta que llegue uno */
int so_lee_suceso(int v, so_suceso *s, int espera);
/* un menú emergente del sistema: las opciones (UTF-8) separadas por «|», «-»
 * es una raya; un bit por opción en gris. Lo elegido llega como SO_MENU */
void so_menu(int v, int x, int y, const char *opciones, u32 grises, u32 etiqueta);
/* ¿está Control pulsado ahora? (para Ctrl+flechas, que llegan sin marca) */
int so_control(void);
/* el portapapeles: guardar un texto (UTF-8); y pedirlo (de so_pide, o NULL) */
void so_al_portapapeles(const char *t, u32 n);
char *so_del_portapapeles(u32 *n);

/* (PINTURA32) el fondo del escritorio: una imagen 0x00RRGGBB de an x al
 * (ya a la medida de la pantalla, so_pantalla). 0 si se ha puesto. En
 * Hispa-Pentium, la llamada 172 (la del Mirador: el núcleo la guarda en
 * FONDO.HPM); en Linux, a OFI_CAPTURAS/fondo.ppm */
int so_fondo_escritorio(const u32 *px, int an, int al);
/* (PINTURA32) lo que mide la pantalla entera (so_pantalla da lo libre, sin
 * la barra de tareas: para las ventanas) */
void so_pantalla_entera(int *an, int *al);

/* [Hispa-64, etapa 11 a] imprimir en una impresora de red (IPP, con
 * IMPRIMIR): so_impresora deja en «d» la de siempre (su URI) y da 1 si la
 * hay; so_pon_impresora la guarda; so_imprime manda un PDF (una ruta como
 * las demás). Las dos, 0 si va bien, y en «msj» lo que se ha dicho. En
 * Hispa-Pentium y en Linux no hay impresoras: 0, y -1. */
int so_impresora(char *d, int cabe);
int so_pon_impresora(const char *uri, char *msj, int cabe);
int so_imprime(const char *pdf, const char *titulo, char *msj, int cabe);

/* lanzar otro programa sin esperarlo (una ventana nueva de Pluma, Hispa-PDF...) */
int so_lanza(const char *programa, const char *args);
/* la ruta del programa que corre (para lanzarse otra vez): en Hispa-Pentium,
 * \\HISPA\\ y el nombre que da el programa en ofi_programa («PLUMA.BIN») */
extern const char ofi_programa[];
void so_programa(char *d, u32 cabe);

/* el principio del programa (lo llama so_pentium.c o so_anfitrion.c) */
int ofi_principal(const char *args);

#endif
