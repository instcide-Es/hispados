/* guion.h - JavaScript en el Navegador completo de Hispa-486 (NAVEGA2.EXE):
 * lo común de guion.c (el motor, QuickJS de HSPJS.DLL, unido al DOM de
 * libdom), selector.c (los selectores de CSS sobre libdom) y rehaz.c (volver
 * a hacer las cajas de NetSurf cuando JavaScript cambia la página).
 * docs/486/NAVEGADOR2.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef GUION_H
#define GUION_H
#include <stdbool.h>
#include <dom/dom.h>

/* Lo de dentro de un nodo de libdom (libdom/src/core/node.h), solo lo del
 * principio y solo para leer: así se recorre el árbol sin pedir y soltar
 * referencias en cada paso (que es lo lento). */
typedef struct NodoI {
  dom_node base;
  const void *vt;
  dom_string *nombre, *valor;
  dom_node_type tipo;
  struct NodoI *padre, *hijo, *ult, *ant, *sig;
  struct dom_document *doc;
} NodoI;
#define NI(n) ((NodoI *)(n))
#define ES_ELEM(n) ((n) && NI(n)->tipo == DOM_ELEMENT_NODE)

/* selector.c: 1 casa, 0 no, -1 el selector no se entiende */
int sel_casa(dom_node *n, const char *sel, dom_node *ambito);
/* lo que casa dentro de raiz (sin contarla), en orden: cuántos (hasta max; -1 malo) */
int sel_busca(dom_node *raiz, const char *sel, dom_node **res, int max);
/* el valor de un atributo (con su referencia: dom_string_unref) o NULL */
dom_string *sel_atr(dom_node *n, const char *nombre);
int sel_etq(dom_node *n, const char *nombre);   /* ¿es <nombre>? (sin mirar mayúsculas) */
void sel_fin(void);

/* rehaz.c: las cajas de nuevo (struct html_content *) */
struct html_content;
void rehaz_cajas(struct html_content *c);
void rehaz_pide(struct html_content *c);        /* cuando se pueda (al acabar el turno) */
void rehaz_olvida(struct html_content *c);      /* la página se va */
void rehaz_suelta_nodo(dom_node *n);            /* un nodo sale del árbol: sus cajas, olvidadas */
struct form_control *html_forms_hispa_ctl(dom_node *n);   /* (forms.c [HISPA]) */
bool rehaz_listo(struct html_content *c);
/* lo que va por encima: position absolute o fixed con z-index > 0 (redraw.c
 * lo pinta al final, interaction.c le da los clics) */
struct box;
bool box_hispa_encima(const struct box *b);
int box_hispa_z(const struct box *b);       /* ¿hay cajas y no se están haciendo? */

/* guion.c, para los ganchos [HISPA] de NetSurf */
struct jsthread;
struct jsthread *guion_de(dom_node *n);         /* el JavaScript de la página de ese nodo */
/* un evento de la persona: true si la página ha dicho preventDefault() */
bool guion_evento(struct jsthread *t, dom_node *n, const char *tipo, dom_node *otro);
bool guion_tecla(struct jsthread *t, dom_node *n, const char *tipo, unsigned tecla);
void guion_foco(struct jsthread *t, dom_node *n);
/* lo mismo, desde el planificador (cuando no se puede en medio de NetSurf) */
void guion_evento_luego(struct jsthread *t, dom_node *n, const char *tipo);
void guion_foco_luego(struct jsthread *t, dom_node *n);
int guion_ocupado(void);                        /* ¿hay JavaScript con trabajo (red, temporizadores)? */
#endif
