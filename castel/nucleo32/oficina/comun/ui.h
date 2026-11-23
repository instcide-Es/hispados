/* ui.h - la oficina de 32 bits: la cara común de los programas (Pluma 2,
 * Cuentas 2...): la barra de menús, las barras de herramientas con iconos
 * propios y listas desplegables (Estilo, Fuente, Tamaño), la regla con las
 * sangrías y las tabulaciones que se arrastran, las barras de
 * desplazamiento, la barra de estado y los cuadros de diálogo, pintados en
 * un lienzo (oficina/comun/lienzo.h) con la letra de la interfaz (DejaVu
 * Sans). Nada depende solo del color: lo activo va hundido y con una raya,
 * el foco con un borde grueso, lo apagado en gris y con su texto aclarado.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_UI_H
#define OFI_UI_H
#include "so.h"
#include "lienzo.h"

/* ---- los colores ---- */
#define UI_FONDO 0xEEF2F7		/* las barras */
#define UI_FONDO2 0xF7F9FC		/* la segunda barra, los cuadros */
#define UI_RAYA 0xC3CDDA
#define UI_RAYA_FUERTE 0x8794A6
#define UI_TINTA 0x18222F
#define UI_TINTA_GRIS 0x8A94A3
#define UI_ACENTO 0x2563C9		/* el foco, lo elegido */
#define UI_ENCIMA 0xDCE6F5		/* el ratón encima */
#define UI_PULSADO 0xC4D5EF		/* pulsado, o activo */
#define UI_BLANCO 0xFFFFFF
#define UI_AVISO 0xFFF4D6

/* ---- los iconos (oficina/comun/iconos.c) ---- */
enum {
	IC_NADA, IC_NUEVO, IC_ABRIR, IC_GUARDAR, IC_IMPRIMIR, IC_VISTA_PREVIA, IC_ORTOGRAFIA, IC_CORTAR, IC_COPIAR,
	IC_PEGAR, IC_BROCHA, IC_DESHACER, IC_REHACER, IC_TABLA, IC_IMAGEN, IC_NEGRITA, IC_CURSIVA, IC_SUBRAYADO,
	IC_TACHADO, IC_IZQUIERDA, IC_CENTRO, IC_DERECHA, IC_JUSTIFICADO, IC_VINETAS, IC_NUMEROS, IC_SANGRIA_MENOS,
	IC_SANGRIA_MAS, IC_COLOR_LETRA, IC_RESALTE, IC_BUSCAR, IC_ZOOM, IC_PDF, IC_MARCAS, IC_SALTO, IC_BORDES,
	IC_AYUDA, IC_SUPERINDICE, IC_SUBINDICE,
	/* (CUENTAS32) los de la hoja de cálculo */
	IC_AUTOSUMA, IC_FX, IC_ORDEN_ASC, IC_ORDEN_DESC, IC_GRAFICO, IC_MONEDA, IC_PORCENTAJE, IC_MILES, IC_DEC_MAS,
	IC_DEC_MENOS, IC_COMBINAR, IC_RELLENO, IC_FILTRO, IC_COMENTARIO, IC_INMOVILIZAR, IC_N
};
/* el icono en (x, y), de lado «t» puntos; «c» la tinta principal; apagado: en gris */
void ui_icono(lienzo *l, int icono, int x, int y, int t, int apagado);

/* ---- la barra de menús ---- */
/* los rótulos («&Archivo|&Edición|...»): los pinta en (x, y, an); «abierto»: el
 * que tiene su menú abierto (-1 ninguno); «encima»: el que tiene el ratón */
int ui_barra_menus(lienzo *l, int x, int y, int an, const char *rotulos, int abierto, int encima);
/* el rótulo que hay en x (-1 ninguno); en *x0, dónde empieza (para abrir su menú debajo) */
int ui_menu_en(const char *rotulos, int x0_barra, int x, int *x0);
/* el rótulo de la letra de Alt+letra (-1 ninguno) */
int ui_menu_letra(const char *rotulos, u32 letra);
int ui_alto_menus(void);

/* ---- las barras de herramientas ---- */
enum { UB_BOTON, UB_CONMUTA, UB_SEPARA, UB_LISTA, UB_COLOR };
typedef struct {
	int id;
	int tipo;		/* UB_* */
	int icono;		/* IC_* (UB_BOTON, UB_CONMUTA, UB_COLOR) */
	const char *ayuda;	/* lo que sale en la barra de estado («Negrita (Ctrl+N)») */
	int ancho;		/* UB_LISTA: en puntos */
	/* lo que cambia: */
	int activo;		/* UB_CONMUTA: hundido */
	int apagado;
	char texto[64];		/* UB_LISTA: lo que se ve */
	u32 color;		/* UB_COLOR: la raya de debajo del icono */
	/* (lo pone ui_barra) */
	int x, y, an, al;
} ui_boton;
/* pinta la barra (los botones, de izquierda a derecha); da lo alto */
int ui_barra(lienzo *l, int x, int y, int an, ui_boton *b, int n, int encima, int pulsado);
int ui_alto_barra(void);
/* el botón que hay en (x, y): su índice o -1. En UB_LISTA y UB_COLOR, *flecha
 * dice si es en la flechita de la derecha */
int ui_boton_en(const ui_boton *b, int n, int x, int y, int *flecha);
ui_boton *ui_boton_id(ui_boton *b, int n, int id);

/* ---- la regla ---- */
typedef struct {
	/* en twips (y la conversión a puntos de pantalla) */
	i32 ancho_pag;		/* el ancho del papel */
	i32 marg_izq, marg_der;
	i32 sang_izq, sang_der, sang_primera;	/* (desde los márgenes) */
	i32 tabs[16];
	u8 tipo_tab[16];
	int n_tabs;
	i32 inicio, fin;	/* (una celda: lo que se puede usar de la regla) */
	/* la pantalla */
	int x_pag;		/* dónde empieza el papel en la ventana */
	int zoom;		/* en tantos por ciento */
	int tab_nuevo;		/* el tipo de tabulación que se pone con un clic (TAB_*) */
} ui_regla;
int ui_alto_regla(void);
void ui_regla_pinta(lienzo *l, int x, int y, int an, const ui_regla *r, int arrastrando);
/* lo que hay en x de la regla: RG_* y, para las tabulaciones, *cual */
enum { RG_NADA, RG_PRIMERA, RG_IZQ, RG_DER, RG_TAB, RG_MARG_IZQ, RG_MARG_DER, RG_ZONA, RG_TIPO };
int ui_regla_en(const ui_regla *r, int x, int y_rel, int *cual);
i32 ui_regla_tw(const ui_regla *r, int x);	/* de la pantalla a twips desde el margen */
int ui_regla_px(const ui_regla *r, i32 tw);	/* de twips desde el margen a la pantalla */

/* ---- las barras de desplazamiento ---- */
typedef struct {
	int x, y, an, al;	/* dónde (vertical si al > an) */
	i32 total, visto, pos;	/* lo que hay, lo que se ve y dónde empieza lo que se ve */
} ui_desp;
#define UI_DESP 15
void ui_desp_pinta(lienzo *l, const ui_desp *d, int encima);
/* lo que hay en (x, y): 0 nada; 1 la flecha de arriba/izquierda; 2 la de abajo;
 * 3 la franja de antes del tirador; 4 la de después; 5 el tirador */
int ui_desp_en(const ui_desp *d, int x, int y);
/* la posición con el tirador arrastrado de y0 a y (con la pos de cuando se pulsó) */
i32 ui_desp_arrastra(const ui_desp *d, i32 pos0, int de, int a);

/* ---- la barra de estado ---- */
int ui_alto_estado(void);
/* los trozos separados por «|»; el último, a la derecha */
void ui_estado(lienzo *l, int x, int y, int an, const char *trozos);

/* ---- los cuadros de diálogo ---- */
enum { UC_ROTULO, UC_CAMPO, UC_BOTON, UC_CASILLA, UC_OPCION, UC_LISTA, UC_DESPLEGABLE, UC_NUMERO, UC_COLOR, UC_TEXTO, UC_MUESTRA };
typedef struct {
	int tipo;		/* UC_* */
	int id;
	int x, y, an, al;	/* dentro del cuadro (al: 0 lo de siempre) */
	const char *rotulo;	/* el texto (del rótulo, botón, casilla...; «&» subraya la letra) */
	char valor[260];	/* (campo, número) lo escrito */
	int marcado;		/* (casilla, opción) */
	int grupo;		/* (opción) las de un grupo se excluyen */
	/* (lista y desplegable) las opciones, separadas por «|», y la elegida */
	const char *opciones;
	int elegida;
	int primera;		/* (lista) la primera que se ve */
	int principal;		/* (botón) el de Intro */
	int cancela;		/* (botón) el de Esc */
	int apagado;
	int cursor, ancla;	/* (campo) el cursor y lo elegido, en letras */
	u32 color;		/* (color) el elegido */
	/* (número) los límites y el paso, en décimas */
	int min, max, paso, decimales;
	const char *unidad;	/* (número) «cm», «pt» */
} ui_control;

typedef struct {
	const char *titulo;
	int an, al;		/* lo que mide (sin la barra del título) */
	ui_control *c;
	int n;
	int foco;		/* el control con el teclado */
	int x, y;		/* (lo pone ui_cuadro_pinta: dónde va, centrado) */
	int pulsado;		/* el botón pulsado con el ratón */
	int cerrado;		/* el id del botón con que se ha cerrado (o -1: Esc) */
	int menu_de;		/* (desplegable) el control cuyo menú está abierto */
	int arrastra;		/* (campo) se está eligiendo con el ratón */
	void (*pinta_extra)(void *u, lienzo *l, const ui_control *c, int x, int y);	/* (UC_MUESTRA) */
	void *u;
} ui_cuadro;

int ui_alto_control(void);
void ui_cuadro_pinta(lienzo *l, ui_cuadro *q);
/* un suceso para el cuadro: 1 si ha cambiado algo (hay que pintar); el
 * cuadro se cierra (q->cerrado) con un botón, Intro o Esc. Las listas
 * desplegables piden su menú con so_menu (etiqueta UI_ETIQUETA_MENU) y la
 * respuesta llega por aquí también */
#define UI_ETIQUETA_MENU 0x7E00
int ui_cuadro_suceso(ui_cuadro *q, int v, const so_suceso *s);
ui_control *ui_control_id(ui_cuadro *q, int id);
/* «valor» de un control de número, en décimas */
int ui_numero(const ui_control *c);
void ui_pon_numero(ui_control *c, int decimas);

/* la letra subrayada de «&Archivo» y el texto sin el «&» */
void ui_sin_amp(const char *s, char *d, u32 cabe, int *subrayada);
void ui_texto_amp(lienzo *l, int x, int y, const char *s, u32 c, int negrita);
int ui_ancho_amp(const char *s);

#endif
