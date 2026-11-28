/* ventana.h - Pluma 2: lo que comparten la ventana (ventana.c) y los
 * cuadros de diálogo (dialogos.c).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_VENTANA_H
#define PLUMA_VENTANA_H
#include "doc.h"
#include "editar.h"
#include "vista.h"
#include "ui.h"

/* las órdenes (de los menús, las barras y las teclas) */
enum {
	O_NADA,
	/* Archivo */
	O_NUEVO, O_ABRIR, O_CERRAR, O_GUARDAR, O_GUARDAR_COMO, O_GUARDAR_TODO, O_EXPORTAR_PDF, O_BUSCAR_ARCHIVO,
	O_RESUMEN, O_PLANTILLAS, O_PREPARAR, O_PREVIA, O_IMPRIMIR, O_SALIR,
	/* Edición */
	O_DESHACER, O_REHACER, O_CORTAR, O_COPIAR, O_PEGAR, O_PEGAR_SIN, O_BORRAR, O_TODO, O_BUSCAR, O_REEMPLAZAR,
	O_IR_A, O_AUTOTEXTO, O_MARCADOR, O_VINCULOS, O_OBJETO,
	/* Ver */
	O_NORMAL, O_ESQUEMA, O_MAESTRO, O_COMPLETA, O_BARRAS, O_REGLA, O_ENC_PIE, O_NOTAS, O_ANOTACIONES, O_MARCAS, O_ZOOM,
	/* Insertar */
	O_SALTO, O_ANOTACION, O_FECHA, O_CAMPO, O_SIMBOLO, O_FORMULARIO, O_NOTA, O_EPIGRAFE, O_REFERENCIA, O_INDICE,
	O_INS_ARCHIVO, O_MARCO, O_IMAGEN, O_INS_OBJETO, O_BASE_DATOS,
	/* Formato */
	O_FUENTES, O_PARRAFO, O_TABULACIONES, O_BORDES, O_COLUMNAS, O_MAYUS, O_CAPITAL, O_LISTAS, O_NUM_TITULOS,
	O_AUTOFORMATO, O_GALERIA, O_ESTILO, O_FMT_MARCO, O_FMT_IMAGEN, O_DIBUJAR,
	/* Herramientas */
	O_ORTOGRAFIA, O_SINONIMOS, O_GUIONES, O_IDIOMA, O_CONTAR, O_AUTOCORRECCION, O_COMBINAR, O_SOBRES, O_PROTEGER,
	O_REVISIONES, O_MACROS, O_PERSONALIZAR, O_OPCIONES,
	/* Tabla */
	O_TABLA, O_FILA_ARRIBA, O_FILA_ABAJO, O_COL_IZQ, O_COL_DER, O_ELIMINAR_CELDAS, O_UNIR, O_DIVIDIR, O_SEL_FILA,
	O_SEL_COLUMNA, O_SEL_TABLA, O_AUTOFORMATO_TABLA, O_ANCHO_CELDA, O_TITULOS, O_TEXTO_A_TABLA, O_ORDENAR,
	O_FORMULA, O_DIVIDIR_TABLA, O_CUADRICULA,
	/* Ventana y ? */
	O_NUEVA_VENTANA, O_ORGANIZAR, O_DIVIDIR_VENTANA, O_DOCUMENTO1, O_CONTENIDO, O_BUSCAR_AYUDA, O_INTRODUCCION,
	O_EJEMPLOS, O_SUGERENCIAS, O_AYUDA_PLUMA, O_ACERCA,
	/* (de las barras) */
	O_NEGRITA, O_CURSIVA, O_SUBRAYADO, O_TACHADO, O_SUPER, O_SUB, O_IZQ, O_CENTRO, O_DER, O_JUST, O_VINETAS,
	O_NUMEROS, O_SANGRIA_MENOS, O_SANGRIA_MAS, O_COLOR, O_RESALTE, O_BROCHA, O_LISTA_ESTILO, O_LISTA_FUENTE,
	O_LISTA_TAMANO, O_LISTA_ZOOM, O_COLOR_MENU, O_RESALTE_MENU, O_MAS_GRANDE, O_MAS_PEQUENA, O_SALTO_PAGINA,
	O_N
};

/* el estado de la ventana (uno por proceso: cada ventana de Pluma es un programa) */
typedef struct {
	int v;				/* la ventana del sistema */
	lienzo l;
	documento *d;
	editor e;
	vista vi;
	char ruta[600];			/* el documento («» si es nuevo) */
	int formato;			/* FMT_* */
	char aviso[300];		/* lo que dice la barra de estado */
	int ver_barras, ver_regla, completa;
	int sucio;			/* hay que repintar */
	int todo;			/* (y todo: las barras también) */
	ui_cuadro *cuadro;		/* el cuadro abierto (o NULL) */
	void (*al_cerrar)(int id);	/* lo que se hace al cerrar el cuadro */
	int pendiente;			/* (¿Guardar los cambios?) la orden que espera */
	char carpeta[600];		/* la de abrir y guardar */
	fmt_letra brocha;		/* el formato copiado con la brocha */
	fmt_parrafo brocha_p;
	int con_brocha;
	u32 color_letra, color_resalte;	/* los de los botones */
	int orto_al_escribir;		/* subrayar las faltas mientras se escribe */
	int sale;
	int tam_ui;			/* la letra de la interfaz (puntos) */
	char ultimo_buscado[200];
	u32 ultimo_reemplazo_n;
} pluma_t;
extern pluma_t P;

void pl_orden(int o);
void pl_aviso(const char *f, ...);
void pl_titulo(void);
void pl_pinta_todo(void);
int pl_abre(const char *ruta);
int pl_guarda(const char *ruta, int formato);
void pl_cambio(void);		/* tras un cambio del documento: repintar, el cursor a la vista */
/* los cuadros (dialogos.c) */
void cuadro_abre(ui_cuadro *q, void (*al_cerrar)(int id));
void dlg_abrir(int guardar, int exportar);
void dlg_fuente(void);
void dlg_parrafo(void);
void dlg_tabulaciones(void);
void dlg_pagina(void);
void dlg_tabla(void);
void dlg_imagen(void);
void dlg_buscar(int reemplazar);
void dlg_ir_a(void);
void dlg_zoom(void);
void dlg_contar(void);
void dlg_enc_pie(void);
void dlg_fecha(void);
void dlg_simbolo(void);
void dlg_salto(void);
void dlg_estilo(void);
void dlg_resumen(void);
void dlg_ortografia(void);
void dlg_opciones(void);
void dlg_acerca(void);
void dlg_ayuda(void);
void dlg_mayus(void);
void dlg_listas(void);
void dlg_bordes(void);
void dlg_eliminar_celdas(void);
void dlg_ancho_celda(void);
void dlg_campo(void);
void dlg_idioma(void);
void dlg_aviso(const char *titulo, const char *texto);
#ifdef OFI_HISPA64
void dlg_imprimir(void);	/* [Hispa-64, etapa 11 a] por IPP (IMPRIMIR) o a PDF */
#endif
/* ¿guardar los cambios? (al acabar, la orden «pendiente» sigue) */
void dlg_guardar_cambios(int orden);
/* las imágenes de un archivo: los datos (de so_pide) o NULL */
u8 *pl_lee_imagen(const char *ruta, u32 *n, char *error, u32 cabe);

/* el corrector (orto_pluma.c) */
void pl_orto_inicia(void);
int pl_orto_hay(void);
void pl_orto_faltas(documento *d, parrafo *p);
int pl_orto_sugiere(const u32 *pal, u32 n, char sug[][64], int max);
void pl_orto_anade(const u32 *pal, u32 n);
void pl_orto_omite(const u32 *pal, u32 n);
int pl_orto_bien(const u32 *pal, u32 n);

#endif
