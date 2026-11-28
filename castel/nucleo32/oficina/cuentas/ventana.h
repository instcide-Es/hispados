/* ventana.h - Cuentas 2: lo que comparten la ventana (ventana.c), la vista
 * de la hoja (vista.c) y los cuadros de diálogo (dialogos.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CU_VENTANA_H
#define CU_VENTANA_H
#include "libro.h"
#include "ui.h"

/* las órdenes (de los menús, las barras y las teclas) */
enum {
	O_NADA,
	/* Archivo */
	O_NUEVO, O_ABRIR, O_CERRAR, O_GUARDAR, O_GUARDAR_COMO, O_EXPORTAR_PDF, O_PREPARAR, O_AREA, O_QUITAR_AREA,
	O_PREVIA, O_IMPRIMIR, O_PROPIEDADES, O_SALIR,
	/* Edición */
	O_DESHACER, O_REHACER, O_CORTAR, O_COPIAR, O_PEGAR, O_PEGADO_ESPECIAL, O_RELLENAR_ABAJO, O_RELLENAR_DERECHA,
	O_SERIES, O_BORRAR_TODO, O_BORRAR_FORMATOS, O_BORRAR_CONTENIDO, O_BORRAR_COMENTARIOS, O_ELIMINAR,
	O_ELIMINAR_HOJA, O_MOVER_HOJA, O_BUSCAR, O_REEMPLAZAR, O_IR_A, O_TODO,
	/* Ver */
	O_VER_BARRA_FORMULAS, O_VER_ESTADO, O_VER_BARRAS, O_COMPLETA, O_ZOOM, O_VER_FORMULAS, O_VER_REJILLA,
	/* Insertar */
	O_INS_CELDAS, O_INS_FILAS, O_INS_COLUMNAS, O_INS_HOJA, O_GRAFICO, O_INS_SALTO, O_QUITAR_SALTO, O_FUNCION,
	O_NOMBRE, O_COMENTARIO, O_INS_IMAGEN,
	/* Formato */
	O_FMT_CELDAS, O_ALTO_FILA, O_AUTO_FILA, O_OCULTAR_FILAS, O_MOSTRAR_FILAS, O_ANCHO_COL, O_AUTO_COL,
	O_OCULTAR_COLS, O_MOSTRAR_COLS, O_ANCHO_ESTANDAR, O_NOMBRE_HOJA, O_COMBINAR, O_SEPARAR, O_CONDICIONAL,
	O_QUITAR_CONDICIONAL, O_AUTOFORMATO, O_ESTILO,
	/* Herramientas */
	O_ORTOGRAFIA, O_PROTEGER, O_RECALCULAR, O_OPCIONES,
	/* Datos */
	O_ORDENAR, O_AUTOFILTRO, O_MOSTRAR_TODO, O_FORMULARIO, O_SUBTOTALES, O_VALIDACION, O_TEXTO_COLUMNAS,
	O_DINAMICA,
	/* Ventana y ? */
	O_NUEVA_VENTANA, O_INMOVILIZAR, O_DIVIDIR, O_LIBRO1, O_CONTENIDO, O_FUNCIONES, O_ACERCA,
	/* (de las barras) */
	O_AUTOSUMA, O_ORDEN_ASC, O_ORDEN_DESC, O_NEGRITA, O_CURSIVA, O_SUBRAYADO, O_IZQ, O_CENTRO, O_DER,
	O_COMBINAR_CENTRAR, O_MONEDA, O_PORCENTAJE, O_MILES, O_DEC_MAS, O_DEC_MENOS, O_BORDES, O_BORDES_MENU,
	O_RELLENO, O_RELLENO_MENU, O_COLOR, O_COLOR_MENU, O_BROCHA, O_LISTA_FUENTE, O_LISTA_TAMANO, O_LISTA_ZOOM,
	O_HOJA_SIG, O_HOJA_ANT, O_FECHA_HOY, O_N
};

/* lo que se está haciendo con el ratón */
enum { AR_NADA, AR_SEL, AR_COLS, AR_FILAS, AR_ANCHO, AR_ALTO, AR_RELLENO, AR_MOVER, AR_GRAFICO, AR_GRAF_TAM,
       AR_DESP_V, AR_DESP_H, AR_PESTANA };

/* la geometría de la hoja en la ventana (la rehace vs_coloca) */
typedef struct {
	int x, y, an, al;		/* lo de las celdas con sus rótulos */
	int rot_an, rot_al;		/* lo que miden los rótulos de las filas y de las columnas */
	int cx, cy, can, cal;		/* lo de las celdas solo */
	int zoom;
	int fx, fy;			/* dónde acaba lo inmovilizado (x e y de pantalla) */
} geom;

typedef struct {
	int v;				/* la ventana del sistema */
	lienzo l;
	libro *lib;
	char ruta[600];
	int formato;
	char aviso[300];
	int sucio, todo;
	ui_cuadro *cuadro;
	void (*al_cerrar)(int id);
	int pendiente;			/* (¿Guardar los cambios?) la orden que espera */
	char carpeta[600];
	int sale;
	/* lo que se ve */
	int ver_barras, ver_formulas_barra, ver_estado, completa, ver_formulas;
	geom g;
	/* corregir una celda */
	int editando;			/* 0 no; 1 en la celda; 2 en la barra de fórmulas */
	int ed_nuevo;			/* (Introducir: las flechas acaban; Modificar: mueven el cursor) */
	u32 ed[4096];
	u32 ed_n, ed_cur, ed_ancla;
	u32 ed_f, ed_c;			/* la celda que se corrige */
	u16 ed_hoja;
	/* señalar (escribiendo una fórmula, las flechas o el ratón meten la referencia) */
	int senala;
	u32 sen_ini, sen_fin;		/* lo que ocupa la referencia metida, en ed */
	u32 sen_f, sen_c, sen_f0, sen_c0;	/* la celda señalada (y la de su ancla) */
	/* lo elegido */
	u32 ancla_f, ancla_c;
	int tab_col;			/* (Tab, Tab, Intro: a la fila de abajo, en la columna del primer Tab) */
	int grafico;			/* el gráfico elegido (-1 ninguno) */
	/* el ratón */
	int arrastra;
	int arr_x, arr_y, arr_i;
	u32 arr_f, arr_c;
	i32 arr_v0, arr_v1;
	rango arr_r;
	int raton_x, raton_y;
	const char *ayuda_encima;
	/* la brocha */
	int con_brocha;
	u16 brocha;
	/* buscar */
	char buscado[200], reemplazo[200];
	int bus_opc;
	/* la vista preliminar */
	int previa, pag_previa;
	int tam_ui;
	u32 color_letra, color_relleno;
	int borde_elegido;
} cuentas_t;
extern cuentas_t C;

hoja *cu_hoja(void);		/* la que se ve */
void cu_orden(int o);
void cu_aviso(const char *f, ...);
void cu_titulo(void);
void cu_pinta(void);
int cu_abre(const char *ruta);
int cu_guarda(const char *ruta, int formato);
void cu_cambio(void);		/* tras un cambio del libro: repintar, el cursor a la vista */
void cu_ve_celda(u32 f, u32 c);	/* la celda, a la vista */
void cu_elige(u32 f, u32 c, int ampliar);	/* el cursor a (f, c) (con Mayús: amplía lo elegido) */
void cu_sel_rango(rango r);
rango cu_seleccion(void);	/* lo elegido (o la celda activa) */

/* ---- la vista (vista.c) ---- */
void vs_coloca(int x, int y, int an, int al);
void vs_pinta(lienzo *l);
/* lo que hay en (x, y): VS_* y la fila y la columna */
enum { VS_NADA, VS_CELDA, VS_ROT_COL, VS_ROT_FILA, VS_ESQUINA, VS_BORDE_COL, VS_BORDE_FILA, VS_RELLENO, VS_GRAFICO,
       VS_GRAF_TAM, VS_FILTRO, VS_BORDE_SEL };
int vs_en(int x, int y, u32 *f, u32 *c, int *i);
int vs_col_x(u32 c, int *x);		/* dónde empieza en la pantalla (0 si no se ve) */
int vs_fila_y(u32 f, int *y);
int vs_celda_rect(u32 f, u32 c, int *x, int *y, int *an, int *al);
void vs_grafico_rect(const grafico *g, int *x, int *y, int *an, int *al);
/* cuántas filas y columnas caben en lo que se mueve */
u32 vs_filas_vistas(void);
u32 vs_cols_vistas(void);
/* el desplazamiento (lo que se mueve) para que se vea (f, c) */
void vs_a_la_vista(u32 f, u32 c);
int vs_ancho_px(u32 c);
int vs_alto_px(u32 f);

/* ---- los cuadros (dialogos.c) ---- */
void cuadro_abre(ui_cuadro *q, void (*f)(int id));
void dlg_aviso(const char *titulo, const char *t);
#ifdef OFI_HISPA64
void dlg_imprimir(void);	/* [Hispa-64, etapa 11 a] por IPP (IMPRIMIR) o a PDF */
#endif
void dlg_guardar_cambios(int orden);
void dlg_abrir(int modo);	/* 0 abrir, 1 guardar como, 2 exportar a PDF */
void dlg_formato_celdas(int pestana);
void dlg_buscar(int reemplazar);
void dlg_ir_a(void);
void dlg_pegado_especial(void);
void dlg_eliminar(int insertar);
void dlg_series(void);
void dlg_ordenar(void);
void dlg_grafico(int i);	/* (i < 0: uno nuevo con lo elegido) */
void dlg_nombre(void);
void dlg_comentario(void);
void dlg_condicional(void);
void dlg_ancho(int fila);
void dlg_zoom(void);
void dlg_nombre_hoja(void);
void dlg_mover_hoja(void);
void dlg_preparar(void);
void dlg_opciones(void);
void dlg_funcion(void);
void dlg_filtro(u32 c);
void dlg_acerca(void);
void dlg_ayuda(void);
void dlg_csv(int guardar, const char *ruta);

#endif
