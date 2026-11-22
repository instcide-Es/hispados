/* xml.h - la oficina de 32 bits: el XML de los documentos (ODF y OOXML:
 * .ODT, .DOCX, .ODS, .XLSX...). Lee a un árbol, sin fiarse: sin recursión,
 * con tope de profundidad (XML_HONDO) y de nodos, las entidades de siempre
 * (&amp; &lt; &gt; &quot; &apos; &#n; &#xn;), CDATA; las DTD se saltan
 * (nada de entidades de fuera). Los nombres se guardan tal cual (con su
 * prefijo: «w:p», «text:span»); al buscar, un nombre sin «:» casa con el
 * nombre sin prefijo («p» casa con «w:p»), y uno con «:», entero. Y unas
 * ayudas para escribirlo (sobre buf_xml de texto.h).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef OFI_XML_H
#define OFI_XML_H
#include "so.h"
#include "texto.h"

#define XML_HONDO 256
#define XML_MAX_NODOS (4u * 1024 * 1024)

typedef struct xml_nodo {
	const char *nombre;		/* NULL: un texto */
	const char *texto;		/* (un texto) ya sin entidades, UTF-8 */
	u32 largo;			/* (un texto) lo que mide */
	struct xml_nodo *padre, *hijo, *sig;
	const char **atr;		/* nombre, valor, nombre, valor... */
	u32 n_atr;
} xml_nodo;

typedef struct xml_doc xml_doc;

/* lee el XML (d, n: se copia lo que haga falta); NULL si no hay memoria o
 * no tiene ni un elemento */
xml_doc *xml_lee(const char *d, u32 n);
void xml_suelta(xml_doc *x);
xml_nodo *xml_raiz(xml_doc *x);		/* el primer elemento */

/* el nombre sin el prefijo */
const char *xml_local(const char *nombre);
int xml_es(const xml_nodo *n, const char *nombre);
/* el primer hijo (elemento) de ese nombre; NULL si no hay */
xml_nodo *xml_hijo(const xml_nodo *n, const char *nombre);
/* el siguiente hermano (elemento) de ese nombre (NULL: cualquiera) */
xml_nodo *xml_sig(const xml_nodo *n, const char *nombre);
/* el primer hijo elemento (cualquiera) */
xml_nodo *xml_primero(const xml_nodo *n);
/* por un camino de hijos: «office:body/office:text» */
xml_nodo *xml_camino(const xml_nodo *n, const char *camino);
/* un atributo (NULL si no está) */
const char *xml_atr(const xml_nodo *n, const char *nombre);
/* todo el texto de dentro, seguido, al bufer */
void xml_texto(const xml_nodo *n, bufer *b);

/* escribir: « nombre="valor"» (con las entidades) */
void xml_pon_atr(bufer *b, const char *nombre, const char *valor);
void xml_pon_atr_num(bufer *b, const char *nombre, long v);
/* «<nombre>texto</nombre>» */
void xml_pon_elem(bufer *b, const char *nombre, const char *texto);

#endif
