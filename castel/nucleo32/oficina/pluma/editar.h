/* editar.h - Pluma 2: los cambios del documento (escribir, borrar, partir
 * y juntar párrafos, el formato, los estilos, las listas, las tablas, las
 * imágenes, copiar y pegar, buscar y reemplazar) y deshacer y rehacer sin
 * límite. Cada cambio guarda los párrafos que toca tal como estaban (y, si
 * cambian, las tablas y la página): deshacer es volver a ponerlos.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef PLUMA_EDITAR_H
#define PLUMA_EDITAR_H
#include "doc.h"

typedef struct {
	u32 p, o;			/* el párrafo y la letra */
} pos;

/* lo que se cambia del formato de la letra (las marcas de ed_formato) */
enum {
	CF_FAMILIA = 1, CF_TAM = 2, CF_NEGRITA = 4, CF_CURSIVA = 8, CF_SUBRAYADO = 16, CF_TACHADO = 32,
	CF_COLOR = 64, CF_RESALTE = 128, CF_VERT = 256, CF_VERSALITAS = 512, CF_TODO = 0xFFFF
};
/* y del párrafo */
enum {
	CP_ALIN = 1, CP_SANG_IZQ = 2, CP_SANG_DER = 4, CP_SANG_PRIMERA = 8, CP_ANTES = 16, CP_DESPUES = 32,
	CP_INTER = 64, CP_TABS = 128, CP_SALTO = 256, CP_JUNTOS = 512, CP_LISTA = 1024, CP_FONDO = 2048
};

typedef struct paso paso;

typedef struct {
	documento *d;
	pos cur, ancla;			/* lo elegido: de ancla a cur */
	fmt_letra pendiente;		/* el formato de lo que se escriba (sin nada elegido) */
	int hay_pendiente;
	/* deshacer y rehacer */
	paso *pasos;
	u32 n_pasos, cabe_pasos, hechos;	/* hechos: los que se pueden deshacer */
	int abierto;			/* hay un paso empezado */
	u32 version_guardada;		/* (para saber si hay cambios sin guardar) */
} editor;

void ed_inicia(editor *e, documento *d);
void ed_suelta(editor *e);
/* lo elegido, en orden; 1 si hay algo */
int ed_eleccion(const editor *e, pos *a, pos *b);
int pos_menor(pos a, pos b);
void ed_elige(editor *e, pos a, pos b);
void ed_elige_todo(editor *e);
void ed_cursor(editor *e, pos c, int ampliar);	/* mueve el cursor (con «ampliar», sin soltar lo elegido) */

/* deshacer y rehacer */
int ed_deshacer(editor *e);
int ed_rehacer(editor *e);
int ed_se_deshace(const editor *e);
int ed_se_rehace(const editor *e);
/* (para quien cambie algo de fuera: guardar cómo está, y luego cerrar el paso) */
void ed_empieza(editor *e, u32 p0, u32 p1, int clase);
void ed_acaba(editor *e, u32 n_ahora);
#define PASO_TABLAS 0x100		/* (en «clase») guardar también las tablas */
#define PASO_PAGINA 0x200		/* y la página (márgenes, encabezado, pie) */

/* escribir y borrar */
void ed_escribe(editor *e, const u32 *t, u32 n);
void ed_escribe_letra(editor *e, u32 c);
void ed_intro(editor *e);
void ed_salto_linea(editor *e);
void ed_salto_pagina(editor *e);
void ed_retroceso(editor *e, int palabra);
void ed_suprime(editor *e, int palabra);
void ed_borra_eleccion(editor *e);

/* el formato */
void ed_formato(editor *e, u32 marcas, const fmt_letra *v);
/* el formato que se ve en el cursor (el de lo elegido, o el pendiente) */
fmt_letra ed_formato_actual(editor *e);
void ed_formato_parrafo(editor *e, u32 marcas, const fmt_parrafo *v);
void ed_estilo(editor *e, int estilo);
void ed_lista(editor *e, int tipo);	/* LISTA_*: la pone, o la quita si ya está */
void ed_sangria(editor *e, int mas);
void ed_mayusculas(editor *e, int modo);	/* 0 minúsculas, 1 MAYÚSCULAS, 2 Tipo oración, 3 Cada Palabra */
void ed_copia_formato(editor *e, const fmt_letra *f, const fmt_parrafo *p);

/* tablas e imágenes */
void ed_tabla(editor *e, int filas, int cols);
void ed_fila(editor *e, int debajo);
void ed_columna(editor *e, int derecha);
void ed_quita_fila(editor *e);
void ed_quita_columna(editor *e);
void ed_quita_tabla(editor *e);
void ed_une_celdas(editor *e);
void ed_divide_celda(editor *e);
void ed_bordes(editor *e, int bordes);
void ed_celda_siguiente(editor *e, int atras);	/* Tab en una tabla (al final, una fila nueva) */
void ed_imagen(editor *e, u8 *datos, u32 n);	/* (los datos pasan al documento) */
void ed_ajuste_imagen(editor *e, int ajuste);
void ed_tamano_imagen(editor *e, i32 ancho, i32 alto);
int ed_imagen_elegida(const editor *e);	/* la imagen elegida (sola), o -1 */
void ed_campo(editor *e, u32 campo);
void ed_nota(editor *e);

/* copiar y pegar: el texto (UTF-8, para el portapapeles del sistema) */
char *ed_copia(editor *e);
void ed_corta(editor *e);
/* pega: si el texto es el que se copió de aquí, con su formato */
void ed_pega(editor *e, const char *utf8, u32 n);

/* buscar y reemplazar (sin mirar mayúsculas ni tildes, salvo con «exacto») */
int ed_busca(editor *e, const u32 *t, u32 n, int exacto, int atras);
int ed_reemplaza(editor *e, const u32 *t, u32 n, const u32 *por, u32 m, int exacto);
int ed_reemplaza_todo(editor *e, const u32 *t, u32 n, const u32 *por, u32 m, int exacto);

/* moverse (sin la maqueta: a la izquierda, a la derecha, por palabras) */
pos ed_izquierda(const editor *e, pos c, int palabra);
pos ed_derecha(const editor *e, pos c, int palabra);
/* la palabra que lleva la posición (de a a b) */
void ed_palabra(const editor *e, pos c, pos *a, pos *b);

/* contar */
void ed_cuenta(const documento *d, u32 *palabras, u32 *letras, u32 *letras_sin_esp, u32 *parrafos);

#endif
