/* pintura.h - Pintura 2: lo que se dicen sus partes (la ventana, la vista
 * del lienzo, las herramientas, los cuadros y los archivos). Un estado
 * por programa: cada ventana de Pintura es un programa.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PINTURA_H
#define PINTURA_H
#include "so.h"
#include "lienzo.h"
#include "ui.h"
#include "texto.h"
#include "lamina.h"
#include "historia.h"
#include "dibujo.h"
#include "colores.h"

#define PINTURA_VERSION "2.0"

/* las herramientas (en este orden, en la caja: de dos en dos) */
enum {
	H_SELECCION, H_LIBRE, H_LAPIZ, H_PINCEL, H_AEROGRAFO, H_BORRADOR, H_RELLENO, H_CUENTAGOTAS,
	H_TEXTO, H_LUPA, H_RAYA, H_CURVA, H_RECTANGULO, H_REDONDEADO, H_ELIPSE, H_POLIGONO, H_N
};
const char *herr_nombre(int h);
const char *herr_ayuda(int h);		/* «Pincel (P): ...» */
u32 herr_tecla(int h);			/* su letra */
void pin_icono(lienzo *l, int h, int x, int y, int t, int apagado);

/* las órdenes (de los menús, la barra y las teclas) */
enum {
	O_NADA,
	O_NUEVO, O_ABRIR, O_GUARDAR, O_GUARDAR_COMO, O_PDF, O_IMPRIMIR, O_FONDO, O_PROPIEDADES, O_SALIR,
	O_DESHACER, O_REHACER, O_CORTAR, O_COPIAR, O_PEGAR, O_BORRAR_SEL, O_TODO, O_SIN_SEL, O_COPIAR_A, O_PEGAR_DE,
	O_VER_CAJA, O_VER_OPCIONES, O_VER_PALETA, O_VER_ESTADO, O_VER_REGLAS, O_VER_REJILLA, O_ACERCAR, O_ALEJAR,
	O_ZOOM_100, O_AJUSTAR,
	O_TAMANO, O_INCLINAR, O_GIRA_DER, O_GIRA_IZQ, O_GIRA_180, O_GIRAR, O_VOLTEA_H, O_VOLTEA_V, O_RECORTAR,
	O_ATRIBUTOS, O_LIMPIAR,
	O_EDITAR_COLORES, O_CAMBIA_COLORES, O_INVERTIR, O_GRISES, O_BRILLO, O_PALETA_FABRICA,
	O_AYUDA, O_TECLAS, O_ACERCA,
	O_HERR0,			/* + la herramienta */
	O_LISTA = O_HERR0 + H_N,	/* (las listas de la barra de opciones) */
	O_L_GROSOR, O_L_FORMA, O_L_SUAVE, O_L_RELLENO, O_L_TOLERANCIA, O_L_BORRA, O_L_DENSIDAD, O_L_SELECCION,
	O_L_LETRA, O_L_TAMANO, O_L_FONDO_TEXTO, O_L_ZOOM, O_NEGRITA, O_CURSIVA,
	O_N
};

/* lo elegido */
typedef struct {
	int activa;		/* hay algo elegido */
	int x, y, an, al;	/* su caja, en la lámina */
	int *libre;		/* (a mano alzada) los puntos (x, y), o NULL */
	int n_libre;
	int flota;		/* se ha levantado: va en «flotante», encima */
	lamina flotante;	/* (con alfa 0 fuera de lo elegido a mano alzada) */
} seleccion_t;

/* la caja de texto que se está escribiendo */
typedef struct {
	int activa;
	int x, y;		/* arriba a la izquierda, en la lámina */
	u32 t[2000];
	u32 n;
	lamina dibujo;		/* el texto pintado (se pone encima en la vista) */
	int an, al;		/* lo que mide */
} caja_texto;

typedef struct {
	int v;				/* la ventana del sistema */
	lienzo l;			/* lo que se ve de la ventana */
	lamina img;			/* la imagen */
	char ruta[600];			/* «» si es nueva */
	int formato;			/* FI_* de formatos.h */
	int calidad;			/* la del JPG */
	int cambiado;
	char aviso[300];
	u32 c1, c2;			/* el color 1 (botón izquierdo) y el 2 (derecho) */
	int herr, herr_antes;		/* (el cuentagotas vuelve a la de antes) */
	/* las opciones de las herramientas */
	int grosor_pincel, forma, grosor_figura, grosor_borrador, radio_aero, densidad;
	int suave, relleno, tolerancia, borra_transparente, sel_transparente;
	char letra[64];
	int tam_letra, negrita, cursiva, fondo_texto;
	/* la vista */
	int zoom;			/* en milésimas (1000 = 100 %) */
	int dx, dy;			/* el desplazamiento (puntos de la pantalla) */
	int ver_caja, ver_opciones, ver_paleta, ver_estado, ver_reglas, ver_rejilla;
	seleccion_t sel;
	caja_texto tx;
	lamina papel;			/* lo copiado (dentro de Pintura) */
	/* los cuadros */
	ui_cuadro *cuadro;
	void (*al_cerrar)(int id);
	void (*al_cambiar)(void);	/* (tras cada cambio en el cuadro) */
	int (*al_raton)(const so_suceso *s);	/* (antes que el cuadro: la muestra de colores) */
	int pendiente;			/* (¿Guardar los cambios?) la orden que espera */
	char carpeta[600];
	int sale;
	int sucio, todo;		/* hay que repintar (todo: también las barras) */
	int tam_ui;
} pintura_t;
extern pintura_t P;

/* ---- la ventana (ventana.c) ---- */
void pin_orden(int o);
void pin_aviso(const char *f, ...);
void pin_titulo(void);
void pin_cambio_imagen(void);		/* la lámina ha cambiado de tamaño (o es otra) */
void pin_repinta_zona(int zona[4]);	/* lo que ha cambiado de la lámina, a la pantalla */
void pin_repinta_todo(void);
void pin_pon_herramienta(int h);
void pin_elige_color(int cual, u32 c);	/* (cual: 1 o 2) */
/* para las medidas: pintar ya lo pendiente */
void pin_pinta(void);

/* ---- la vista del lienzo (vista.c) ---- */
typedef struct {
	int x, y, an, al;		/* dónde va en la ventana */
	int ox, oy;			/* dónde cae el punto (0, 0) de la lámina */
	int san, sal;			/* lo que mide la lámina en la pantalla */
} vista_t;
extern vista_t VI;
void vista_coloca(int x, int y, int an, int al);
void vista_pinta(lienzo *L, int x0, int y0, int x1, int y1);
/* de la pantalla a la lámina: el punto (entero) y la posición fina */
void vista_a_lamina(int sx, int sy, int *ix, int *iy);
void vista_a_lamina_f(int sx, int sy, float *fx, float *fy);
int vista_de_lamina_x(int ix);
int vista_de_lamina_y(int iy);
/* la zona de la lámina, en la pantalla (x0, y0, x1, y1) */
void vista_zona(const int zona[4], int *x0, int *y0, int *x1, int *y1);
int vista_total_x(void);
int vista_total_y(void);
void vista_limita(void);		/* el desplazamiento, dentro de lo que hay */
void vista_zoom_en(int zoom, int sx, int sy);	/* con el punto de la pantalla quieto */
int vista_zoom_siguiente(int z, int mas);
int vista_zoom_ajustado(void);
/* las reglas */
int regla_alto(void);
int regla_ancho(void);
void regla_pinta(lienzo *L, int arriba, int x, int y, int an, int al, int raton);
/* el cursor del pincel (su contorno en la pantalla) */
extern int cursor_x, cursor_y, cursor_r;	/* (r <= 0: sin él) */
void vista_cursor_zona(int *x0, int *y0, int *x1, int *y1);
/* el asa de abajo a la derecha (cambiar el tamaño del lienzo): 0 ninguna, 1 derecha, 2 abajo, 3 la esquina */
int vista_asa_en(int sx, int sy);
extern int asa_an, asa_al;		/* (mientras se arrastra: el tamaño nuevo; 0 si no) */
/* (PINTURA32) el asa que tiene el ratón encima (0 ninguna; como vista_asa_en)
 * y dónde está el ratón: el asa se resalta y junto a la flecha sale la de
 * cambiar el tamaño (el sistema no tiene punteros con otra forma) */
extern int asa_encima, asa_mx, asa_my;

/* ---- las herramientas (herramientas.c) ---- */
void herr_baja(const so_suceso *s);
void herr_mueve(const so_suceso *s);
void herr_sube(const so_suceso *s);
int herr_tecla_caja(const so_suceso *s);	/* (la caja de texto, el polígono...) 1 si era suya */
void herr_fija(void);			/* lo pendiente (la selección, el texto, el polígono), a la lámina */
void herr_cancela(void);
int herr_arrastrando(void);
void herr_texto_rehaz(void);		/* (la letra ha cambiado) */
/* lo elegido */
void sel_nada(void);
void sel_todo(void);
int sel_hay(void);
void sel_levanta(int copia);
void sel_borra(void);
int sel_copia(void);
void sel_pega(const lamina *o, int x, int y, const char *que);
/* lo que hace una operación con lo elegido (o NULL: la imagen entera) */
lamina *sel_lamina_para_operar(void);
void sel_cambiada(void);		/* (la flotante ha cambiado de tamaño) */
void herr_texto_pinta_cursor(lienzo *L);

/* ---- los cuadros (dialogos.c) ---- */
void cuadro_abre(ui_cuadro *q, void (*al_cerrar)(int id));
void dlg_aviso(const char *titulo, const char *texto);
void dlg_guardar_cambios(int orden);
void dlg_abrir(int modo);		/* DLG_* */
enum { DLG_ABRIR, DLG_GUARDAR, DLG_PDF, DLG_PEGAR_DE, DLG_COPIAR_A };
void dlg_nuevo(void);
void dlg_tamano(void);
void dlg_inclinar(void);
void dlg_girar(void);
void dlg_atributos(void);
void dlg_brillo(void);
void dlg_colores(int cual);
void dlg_calidad(const char *ruta);
void dlg_ayuda(int teclas);
void dlg_acerca(void);
void dlg_propiedades(void);

/* ---- los archivos (archivo.c) ---- */
int pin_abre(const char *ruta);
int pin_guarda(const char *ruta, int formato);
int pin_pdf(const char *ruta);
int pin_fondo(void);
/* una imagen de un archivo (para «Pegar desde»): a *d; 0 si no (con el aviso) */
int pin_lee_imagen(const char *ruta, lamina *d, char *err, u32 cabe);
int pin_guarda_lamina(const lamina *l, const char *ruta, int formato, int calidad, char *err, u32 cabe);
/* la ruta con su extensión (y en 8+3 si el sistema no hace nombres largos) */
void pin_ruta_con_ext(char *ruta, u32 cabe, int formato);

/* las operaciones de la imagen (o de lo elegido) (ventana.c) */
enum { A_GIRA90, A_GIRA, A_VOLTEA, A_INVIERTE, A_GRISES, A_BRILLO, A_ESCALA, A_INCLINA, A_LIENZO, A_RECORTA, A_LIMPIA };
void pin_aplica(int que, int a, int b, double g, const char *nombre);
int herr_quieto(void);
extern int paleta_editada;		/* (dos clics en la paleta: el color que se cambia; -1 ninguno) */

/* ---- principal.c ---- */
const char *pin_arg(const char *s, char *d, u32 cabe);
int pintura_ventana(const char *archivo);
int pintura_mide(int an, int al, int pasos);

#endif
