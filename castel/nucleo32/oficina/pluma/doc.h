/* doc.h - Pluma 2: el documento (C99 portable; nada del sistema).
 *
 * Las medidas, en vigésimos de punto («twips»: 1440 por pulgada, 567 por
 * centímetro); el tamaño de la letra, en medios puntos (24 = 12 pt).
 *
 * Un documento es una lista de párrafos. Cada párrafo lleva su texto en
 * UTF-32 (una letra por posición) y, para cada letra, el número de su
 * formato de letra (la tabla de formatos del documento: cada formato
 * distinto está una sola vez). El formato del párrafo va en el párrafo.
 * Además del Unicode, en el texto pueden ir:
 *   - 0x110000 + n: la imagen n (en la línea, o anclada aquí si flota);
 *   - CAMPO_PAGINA, CAMPO_PAGINAS: el número de la página y cuántas hay;
 *   - 0x2028: un salto de línea dentro del párrafo (Mayús+Intro);
 *   - '\t': una tabulación.
 * Una tabla son párrafos seguidos con el mismo número de tabla: cada celda
 * es un párrafo (con sus saltos de línea), por filas; la celda que se une
 * con otras lleva cuántas columnas y filas abarca, y las que quedan debajo
 * van marcadas como «cubiertas» (vacías, no se ven).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_DOC_H
#define PLUMA_DOC_H
#include "so.h"

#define TW_CM 567
#define TW_PT 20

#define IMAGEN_0 0x110000u		/* + el número de la imagen */
#define IMAGEN_MAX 0x10000u
#define CAMPO_PAGINA 0x120001u
#define CAMPO_PAGINAS 0x120002u
#define NOTA_0 0x130000u		/* + el número de la nota al pie */
#define SALTO_LINEA 0x2028u
#define es_imagen(c) ((c) >= IMAGEN_0 && (c) < IMAGEN_0 + IMAGEN_MAX)
#define es_nota(c) ((c) >= NOTA_0 && (c) < NOTA_0 + 0x10000u)
#define es_objeto(c) ((c) >= 0x110000u)

#define SIN_COLOR 0xFF000000u		/* (el resaltado: ninguno; la letra: la de siempre, negro) */

/* el formato de una letra */
typedef struct {
	char familia[48];
	u16 tam;			/* medios puntos */
	u8 negrita, cursiva, subrayado, tachado;
	u8 vert;			/* 0 normal, 1 superíndice, 2 subíndice */
	u8 versalitas;			/* (se lee y se guarda; se pinta como mayúsculas pequeñas) */
	u32 color;			/* 0xRRGGBB, o SIN_COLOR (automático) */
	u32 resalte;			/* 0xRRGGBB, o SIN_COLOR */
} fmt_letra;

/* las alineaciones */
enum { AL_IZQ, AL_CENTRO, AL_DER, AL_JUST };
/* el interlineado */
enum { INTER_MULTIPLE, INTER_EXACTO, INTER_MINIMO };
/* las listas */
enum { LISTA_NO, LISTA_VINETAS, LISTA_NUMEROS, LISTA_LETRAS, LISTA_ROMANOS };
/* las tabulaciones */
enum { TAB_IZQ, TAB_CENTRO, TAB_DER, TAB_DECIMAL };
#define MAX_TABS 12

typedef struct {
	i32 pos;			/* desde el margen izquierdo */
	u8 tipo;			/* TAB_* */
	u8 relleno;			/* 0 nada; '.', '-', '_' */
} tabulacion;

/* el formato de un párrafo */
typedef struct {
	u8 alin;			/* AL_* */
	u8 inter_tipo;			/* INTER_* */
	u8 lista;			/* LISTA_* */
	u8 nivel;			/* de la lista: 0-8 */
	u8 salto_antes;			/* salto de página antes */
	u8 juntos;			/* con el siguiente en la misma página */
	u8 n_tabs;
	u8 nivel_titulo;		/* 0, o 1-9 (para el índice y los marcadores del PDF) */
	i32 sang_izq, sang_der;		/* las sangrías */
	i32 sang_primera;		/* la de la primera línea (negativa: francesa) */
	i32 antes, despues;		/* el espacio antes y después */
	i32 inter;			/* MULTIPLE: en centésimas (100 sencillo, 150, 200); EXACTO y MINIMO: twips */
	u16 estilo;			/* el estilo con nombre (doc->est) */
	u16 lista_id;			/* la numeración que sigue (las del mismo número cuentan juntas) */
	u32 inicio_lista;		/* (si no es 0) el número con que empieza */
	tabulacion tabs[MAX_TABS];
	/* la tabla */
	u16 tabla;			/* 0: no; si no, el número de la tabla + 1 */
	u16 fila, col;
	u8 abarca_cols, abarca_filas;	/* (la celda que se une con otras) */
	u8 cubierta;			/* tapada por la celda unida de al lado o de arriba */
	u32 fondo;			/* el fondo (de la celda), o SIN_COLOR */
} fmt_parrafo;

/* una línea del párrafo, ya maquetada (maqueta.c) */
typedef struct {
	u32 ini, fin;			/* las letras [ini, fin) */
	i32 y;				/* desde arriba del párrafo */
	i32 alto, base;			/* lo alto de la línea y dónde va su línea de base */
	i32 x0;				/* dónde empieza (desde el borde del texto) */
	i32 ancho;			/* lo que mide lo escrito */
	i32 extra;			/* (justificado) lo que se añade en cada espacio, en milésimas de twip */
	i32 izq;			/* dónde empezaría a la izquierda (las tabulaciones se cuentan desde aquí) */
	u16 espacios;			/* (justificado) cuántos espacios lleva */
	u8 ultima;			/* acaba el párrafo (o en un salto de línea) */
} linea;

typedef struct {
	u32 *t;				/* el texto */
	u16 *f;				/* el formato de cada letra (doc->fl) */
	u32 n, cabe;
	fmt_parrafo p;
	/* lo maquetado (maqueta.c): para «ancho» */
	linea *lin;
	u32 n_lin, cabe_lin;
	i32 alto;			/* sin el espacio antes y después */
	i32 ancho_maq;			/* el ancho para el que está hecho (-1: hay que rehacerlo) */
	u32 version;			/* (cambia con cada cambio: las faltas de ortografía) */
	u8 *faltas;			/* (orto) una marca por letra: 1 empieza una falta; o NULL */
	u32 faltas_version;
} parrafo;

/* las tablas */
#define MAX_COLS 63
enum { BORDE_NO, BORDE_TODOS, BORDE_FUERA, BORDE_CABECERA };
typedef struct {
	u16 filas, cols;
	i32 ancho[MAX_COLS];		/* el de cada columna */
	u8 bordes;			/* BORDE_* */
	u8 cabecera;			/* la primera fila, en negrita y con fondo, y se repite */
	u16 grosor;			/* el de las rayas */
	u32 color;			/* el de las rayas */
	i32 margen;			/* de la celda a su texto */
	u8 alin;			/* la tabla: a la izquierda, centrada, a la derecha */
	u8 viva;			/* (0: quitada) */
} tabla;

/* las imágenes */
enum { IMG_PNG = 1, IMG_JPG, IMG_BMP, IMG_GIF };
enum { AJUSTE_LINEA, AJUSTE_IZQ, AJUSTE_DER };
typedef struct {
	u8 *datos;			/* el archivo, tal cual (se guarda así: sin perder nada) */
	u32 n;
	u8 tipo;			/* IMG_* */
	u8 ajuste;			/* AJUSTE_* */
	int px_an, px_al;		/* sus puntos */
	u32 *px;			/* (descomprimida, cuando hace falta; con alfa) */
	i32 ancho, alto;		/* lo que mide en el documento */
	char nombre[40];		/* (para ODT y DOCX: «imagen1.png») */
} imagen;

/* los estilos con nombre */
typedef struct {
	char nombre[40];
	fmt_letra letra;
	fmt_parrafo p;			/* (sin lo de la tabla) */
	u16 siguiente;			/* el de después de Intro */
	u8 de_serie;			/* los que trae Pluma (no se borran) */
} estilo;
enum { EST_NORMAL, EST_TITULO, EST_T1, EST_T2, EST_T3, EST_SUBTITULO, EST_CITA, EST_LISTA, EST_ENC, EST_PIE,
       EST_NOTA, EST_SIN_ESPACIO, EST_N };

/* una página, ya hecha (maqueta.c): los trozos que lleva */
typedef struct {
	u32 par;			/* el párrafo */
	u32 lin0, lin1;			/* sus líneas [lin0, lin1) (o, en una fila de tabla, todas) */
	i32 y;				/* dónde empieza (desde arriba de la zona del texto) */
	u32 fila_fin;			/* (fila de tabla) el párrafo de después de la fila */
	i32 alto_fila;			/* (fila de tabla) lo alto de la fila */
} trozo;
typedef struct {
	u32 t0, n_t;			/* sus trozos (doc->trz[t0 .. t0+n_t)) */
	u32 nota0, n_notas;		/* las notas al pie que van en ella */
	i32 alto_notas;
} pagina;

typedef struct documento {
	parrafo *par;
	u32 n_par, cabe_par;
	fmt_letra *fl;			/* los formatos de letra */
	u32 n_fl, cabe_fl;
	estilo *est;
	u32 n_est, cabe_est;
	tabla *tab;
	u32 n_tab, cabe_tab;
	imagen *img;
	u32 n_img, cabe_img;
	parrafo *notas;			/* las notas al pie (un párrafo cada una) */
	u32 n_notas, cabe_notas;
	/* la página */
	i32 pag_an, pag_al;
	i32 marg_izq, marg_der, marg_sup, marg_inf;
	i32 dist_enc, dist_pie;		/* del borde al encabezado y al pie */
	parrafo enc, pie;		/* el encabezado y el pie (un párrafo cada uno) */
	u8 hay_enc, hay_pie;
	u8 enc_no_primera;		/* sin encabezado ni pie en la primera página */
	u32 num_inicio;			/* el número de la primera página */
	/* lo de los resúmenes */
	char titulo[120], autor[80], asunto[120];
	/* lo maquetado */
	pagina *pag;
	u32 n_pag, cabe_pag;
	trozo *trz;
	u32 n_trz, cabe_trz;
	u32 version;			/* cambia con cada cambio */
	int cambiado;			/* desde que se guardó */
	int sin_memoria;		/* algo no se pudo hacer: no había memoria */
} documento;

/* ---- el documento ---- */
documento *doc_nuevo(void);		/* vacío: un párrafo Normal, A4 con márgenes de 2,5 cm */
void doc_suelta(documento *d);
void doc_estilos_de_serie(documento *d);
fmt_letra fmt_letra_normal(void);	/* DejaVu Serif de 12 */
fmt_parrafo fmt_parrafo_normal(void);
/* el número del formato (lo añade si no está) */
u16 doc_fmt(documento *d, const fmt_letra *f);
int fmt_iguales(const fmt_letra *a, const fmt_letra *b);
/* el estilo por su nombre (sin mirar mayúsculas); -1 si no está */
int doc_estilo(const documento *d, const char *nombre);
int doc_nuevo_estilo(documento *d, const estilo *e);
/* el formato de letra del estilo (para escribir en un párrafo vacío) */
u16 doc_fmt_estilo(documento *d, int estilo);

/* ---- los párrafos ---- */
parrafo *doc_inserta_parrafos(documento *d, u32 en, u32 cuantos);	/* vacíos */
void doc_quita_parrafos(documento *d, u32 desde, u32 cuantos);
int par_cabe(parrafo *p, u32 n);	/* que quepan n letras (0 si no hay memoria) */
int par_inserta(parrafo *p, u32 en, const u32 *t, const u16 *f, u32 n);
int par_inserta_uno(parrafo *p, u32 en, u32 c, u16 f);
void par_quita(parrafo *p, u32 desde, u32 hasta);
void par_suelta(parrafo *p);	/* el texto y lo maquetado (no el párrafo) */
int par_copia(parrafo *d, const parrafo *o);	/* (copia entera, sin lo maquetado) */
void par_cambia(documento *d, parrafo *p);	/* hay que volver a maquetarlo */
/* el formato de letra que se usa al escribir en «o» (el de la letra de antes) */
u16 par_fmt_en(documento *d, const parrafo *p, u32 o);

/* ---- tablas e imágenes ---- */
int doc_nueva_tabla(documento *d, u16 filas, u16 cols, i32 ancho_total);
/* la celda (fila, col) de la tabla t: su párrafo, o -1 */
int doc_celda(const documento *d, u32 t, u32 fila, u32 col);
/* el primer párrafo de la tabla y cuántos lleva */
u32 doc_tabla_ini(const documento *d, u32 p, u32 *n);
/* una imagen (los datos se quedan en el documento: no se copian); su número o -1 */
int doc_nueva_imagen(documento *d, u8 *datos, u32 n);
/* que la imagen tenga sus puntos (los descomprime); 0 si no se puede */
int doc_imagen_px(documento *d, int i);
int doc_nueva_nota(documento *d);

/* ---- lo que miden las cosas ---- */
i32 doc_ancho_texto(const documento *d);	/* entre márgenes */
i32 doc_alto_texto(const documento *d);

/* ---- los textos de los números de una lista («1.», «a)», «iv.») ---- */
void doc_numero_lista(int tipo, u32 n, char *t, int nivel);

#endif
