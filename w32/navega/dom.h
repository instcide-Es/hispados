/* dom.h - [JS] el DOM del Navegador de Hispa-486: lo que JavaScript hace con
 * el árbol del documento (dom.c) y la unión con el motor (script.c).
 * docs/486/NAVEGADOR.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 * Los textos del árbol van en la página 1252 (como los deja html.c), salvo el
 * código de los <script>, que va tal cual (UTF-8). Lo que sale hacia
 * JavaScript sale en UTF-8 (dom_serializa, dom_contenido). */
#ifndef DOM_H
#define DOM_H
#include "nav.h"

extern Pagina *dom_pag;                  /* la página en la que trabaja el DOM */
extern int dom_sucio;                    /* algo ha cambiado: hay que volver a maquetar */
extern int dom_sucio_ctl;                /*   y rehacer los controles de formulario */

const char *dom_nombre(const Nodo *n);   /* «div», «input»... ("" si no se sabe) */
int dom_etq(const char *nombre);         /* el código E_ de un nombre (E_OTRO si no está) */
Nodo *dom_crea(const char *nombre);
Nodo *dom_crea_texto(const char *t, int n);
Nodo *dom_ant(const Nodo *n);            /* el hermano de antes */
void dom_quita(Nodo *n);                 /* lo saca de su padre */
void dom_inserta(Nodo *padre, Nodo *n, Nodo *antes);   /* antes NULL: al final */
int dom_dentro(const Nodo *a, const Nodo *n);          /* ¿n está dentro de a (o es a)? */
void dom_pon_atr(Nodo *n, const char *nom, const char *val);
void dom_quita_atr(Nodo *n, const char *nom);
void dom_pon_texto(Nodo *n, const char *t, int len);   /* el texto de un nodo de texto */
void dom_pon_contenido(Nodo *n, const char *t, int len);   /* textContent = */
void dom_html(Nodo *n, const u8 *utf8, u32 len);       /* innerHTML = */
char *dom_serializa(const Nodo *n, int fuera, u32 *len);   /* innerHTML/outerHTML (malloc, UTF-8) */
char *dom_contenido(const Nodo *n, u32 *len);          /* textContent (malloc, UTF-8) */
/* los selectores de CSS: 1 casa, 0 no, -1 el selector no se entiende */
int dom_casa(Nodo *n, const char *sel, Nodo *ambito);
/* lo que casa dentro de raiz (sin contarla), en orden: cuántos (hasta max) */
int dom_busca(Nodo *raiz, const char *sel, Nodo **res, int max);
Nodo *dom_por_id(Nodo *raiz, const char *id);
int dom_oculto(const Nodo *n);           /* ¿no se ve (él o alguno de arriba)? */
void dom_prepara(Doc *d);                /* <html> y <body> si faltan (para document.body...) */

/* ---- script.c: lo que llama el Navegador (navega.c) ---- */
/* lo que JavaScript pide al Navegador */
typedef struct {
  void (*remaqueta)(void);
  void (*ir)(const char *url, int post, char *cuerpo, u32 n_cuerpo);   /* location.href = */
  void (*envia)(Nodo *forma, Nodo *boton);   /* form.submit(): sin el evento submit */
  void (*pon_url)(const char *url);      /* history.pushState, location.hash: la dirección de arriba */
  void (*historia)(int paso);            /* history.back() (-1), forward() (1), 0 recarga */
  void (*foco)(Nodo *n);                 /* elemento.focus(): su control o su enlace (NULL: nadie) */
  int (*cuadro)(const char *texto, int tipo);   /* 0 alert, 1 confirm: 1 si dice que sí */
  int (*pregunta)(const char *texto, const char *def, char *resp, int max);   /* prompt: 1 si acepta */
  void (*titulo)(const char *t);
  void (*estado)(const char *t);
  void (*a_la_vista)(Nodo *n);           /* scrollIntoView */
} JsNav;
extern int js_activo;                    /* JavaScript=NO en NAVEGA.INI lo apaga */
extern int js_tope_ms;                   /* lo más que puede tardar un guion o un evento */
extern int js_memoria_kb;                /* lo más que puede usar JavaScript (KB) */
void js_inicia(HANDLE vent, const JsNav *nav, const char *carpeta);
void js_fin(void);
void js_pagina(Pagina *p);               /* la página nueva, ya maquetada: sus <script> */
void js_suelta(void);                    /* la página se va */
void js_para_red(void);                  /* el Navegador va a pedir otra página: lo de JavaScript, fuera */
int js_ocupado(void);                    /* ¿hay guiones por cargar o peticiones (fetch) en marcha? */
void js_avanza(void);                    /* un paso de eso (desde el bucle de mensajes) */
void js_tic(void);                       /* WM_TIMER: los setTimeout vencidos */
#define JS_TIMER 0x4A53
/* los eventos: 1 si la página ha dicho preventDefault() (no hacer lo de siempre) */
int js_evento(Nodo *n, const char *tipo, Nodo *otro);  /* click, change, input, submit (otro: el botón)... */
int js_tecla(Nodo *n, int vk, int ch);                 /* keydown (n: el del foco, o NULL) */
void js_foco(Nodo *n);                                 /* el foco va a n (o a nadie): focus/blur */
int js_enlace(const char *href);                       /* «javascript:...»: 1 si lo ha hecho */
void js_redimensiona(void);
int js_en_pagina(void);                 /* ¿la página de ahora tiene JavaScript? */
#endif
