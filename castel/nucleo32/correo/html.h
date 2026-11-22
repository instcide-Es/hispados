/* html.h - Correo: lo que se enseña de un mensaje, como un texto con formato
 * (un «documento»): de HTML (html.c, sin JavaScript: los <script>, <style>,
 * los on...= y lo de javascript: se tiran; las imágenes de fuera no se
 * cargan: solo se apuntan) y de texto (también format=flowed y las citas
 * con «>»). Lo pinta la ventana (vista.c), párrafo a párrafo, cortando los
 * renglones a lo ancho que haya.
 * C99 portable (so.h). Lo que llega es hostil: topes, sin recursión.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef CORREO_HTML_H
#define CORREO_HTML_H
#include "so.h"

/* el estilo de un tramo (bits) */
#define E_NEGRITA 0x0001
#define E_CURSIVA 0x0002
#define E_SUBRAYADO 0x0004
#define E_MONO 0x0008		/* <pre>, <code>, <tt>: no se juntan los espacios */
#define E_ENLACE 0x0010		/* «enlace» dice cuál */
#define E_TITULO 0x0020		/* <h1>-<h3> (en negrita y con hueco) */
#define E_TACHADO 0x0040
#define E_IMAGEN 0x0080		/* el tramo es una imagen (U+FFFC): «imagen» dice cuál */
#define E_PEQUENO 0x0100	/* <small>, <sub>, <sup> */

/* lo que es un párrafo (cada uno empieza en un renglón nuevo) */
#define P_NORMAL 0
#define P_TITULO 1
#define P_LISTA 2		/* con su viñeta o su número delante (ya en el texto) */
#define P_CITA 3		/* (con «nivel») */
#define P_RAYA 4		/* <hr> */
#define P_PRE 5		/* sin cortar los renglones si se puede (MONO) */
#define P_TABLA 6		/* una fila: las celdas separadas por \t */

typedef struct {
	u32 ini, n;		/* en Doc.texto */
	u16 estilo;		/* E_... */
	u16 enlace;		/* (E_ENLACE) el número en Doc.enlaces; (E_IMAGEN) en Doc.imagenes */
} Tramo;

typedef struct {
	u32 ini, fin;		/* los tramos: Doc.tramos[ini .. fin-1] */
	u8 clase;		/* P_... */
	u8 nivel;		/* la sangría (listas dentro de listas, citas dentro de citas) */
	u8 hueco;		/* renglones en blanco delante (0 a 2) */
	u8 alinea;		/* 0 izquierda, 1 centro, 2 derecha */
} Parrafo;

typedef struct {
	char *href;		/* adonde lleva de verdad (UTF-8, sin tocar) */
	char *visible;		/* lo que se ve (el texto del enlace) */
	int enganoso;		/* lo que se ve parece una dirección (o un dominio) y
				 * el de verdad es de otro sitio: «aviso» */
	char motivo[160];	/* (si engañoso) «dice banco.es y lleva a malo.example» */
} Enlace;

typedef struct {
	char *src;		/* «cid:xxx» (dentro del mensaje), «https://...» (de fuera) o «data:» */
	char *alt;
	int ancho, alto;	/* los que dice el HTML (0 si no) */
	int remota;		/* es de fuera (http, https, //...) */
	int rastreo;		/* parece un píxel de rastreo (1x1, 0x0, oculta) */
} Imagen;

typedef struct {
	char *texto;		/* UTF-8 */
	u32 n;
	Tramo *tramos;
	u32 n_tramos;
	Parrafo *parrafos;
	u32 n_parrafos;
	Enlace *enlaces;
	u32 n_enlaces;
	Imagen *imagenes;
	u32 n_imagenes;
	int habia_scripts;	/* llevaba <script>, on...= o javascript: (quitados) */
	int remotas;		/* cuántas imágenes de fuera (no cargadas) */
	int enganosos;		/* cuántos enlaces engañosos */
	int cortado;		/* era demasiado: no se ha leído todo */
} Doc;

#define HTML_MAX_TEXTO (4u << 20)	/* lo más que da (en octetos de texto) */
#define HTML_MAX_ANIDADO 64		/* etiquetas una dentro de otra */

/* de HTML (UTF-8: el juego ya lo ha pasado mime_texto; un <meta charset>
 * no se mira) a documento. NULL si no hay memoria */
Doc *html_a_doc(const char *html, u32 n);
/* de texto (UTF-8) a documento: las citas («> », también «>>»), las
 * direcciones (http://, https://, www., mailto: y buzon@dominio) como enlaces,
 * y la firma («-- ») en pequeño */
Doc *texto_a_doc(const char *t, u32 n);
void doc_libera(Doc *d);
/* el documento como texto plano (para responder citando, para «Copiar» y
 * para buscar): en memoria nueva; las listas con su viñeta, las citas con
 * «> », los enlaces como «texto <href>» si no son iguales */
char *doc_a_texto(const Doc *d, u32 *largo);
/* de un dominio (o una dirección), el «registrable» (lo de delante del
 * sufijo público corriente: www.banco.es -> banco.es; a.b.co.uk -> b.co.uk) */
void html_dominio_base(const char *host, char *d, int cabe);

#endif
