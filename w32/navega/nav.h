/* nav.h - lo común del Navegador de Hispa-486 (w32/navega, docs/486/NAVEGADOR.md)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef NAV_H
#define NAV_H
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)

/* ---- MSVCRT ---- */
void *malloc(u32);
void *realloc(void *, u32);
void free(void *);
void *memcpy(void *, const void *, u32);
void *memmove(void *, const void *, u32);
void *memset(void *, int, u32);
int memcmp(const void *, const void *, u32);
u32 strlen(const char *);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, u32);
int _stricmp(const char *, const char *);
int _strnicmp(const char *, const char *, u32);
char *strchr(const char *, int);
char *strrchr(const char *, int);
char *strstr(const char *, const char *);
int atoi(const char *);
int sprintf(char *, const char *, ...);

/* ---- textos y memoria (html.c) ---- */
typedef struct Trozo { struct Trozo *sig; u32 usado, tam; } Trozo;
typedef struct { Trozo *t; } Arena;
void *ar_pide(Arena *, u32);
char *ar_texto(Arena *, const char *, int);       /* copia n letras (-1: todas) y pone el 0 */
void ar_suelta(Arena *);
int min_c(int c);                                  /* a minúscula (ASCII) */
int utf8_a_1252(const u8 *o, u32 n, char *d);      /* -> letras escritas en d */
int a_utf8(const char *o, int n, char *d);         /* página 1252 -> UTF-8 */

/* ---- el documento (html.c) ---- */
enum {
  E_OTRO, E_HTML, E_HEAD, E_BODY, E_TITLE, E_META, E_LINK, E_BASE, E_SCRIPT, E_STYLE, E_NOSCRIPT, E_TEMPLATE,
  E_DIV, E_P, E_H1, E_H2, E_H3, E_H4, E_H5, E_H6, E_UL, E_OL, E_LI, E_DL, E_DT, E_DD, E_PRE, E_BLOCKQUOTE,
  E_HR, E_BR, E_TABLE, E_CAPTION, E_THEAD, E_TBODY, E_TFOOT, E_TR, E_TD, E_TH, E_FORM, E_FIELDSET, E_LEGEND,
  E_INPUT, E_SELECT, E_OPTGROUP, E_OPTION, E_TEXTAREA, E_BUTTON, E_LABEL, E_A, E_IMG, E_B, E_STRONG, E_I, E_EM,
  E_U, E_S, E_SMALL, E_CODE, E_SPAN, E_FONT, E_CENTER, E_SECTION, E_ARTICLE, E_HEADER, E_FOOTER, E_MAIN, E_NAV,
  E_ASIDE, E_DETAILS, E_SUMMARY, E_DIALOG, E_SVG, E_IFRAME, E_ADDRESS, E_FIGURE, E_FIGCAPTION, E_MARK, E_DEL,
  E_INS, E_SUB, E_SUP, E_KBD, E_VAR, E_SAMP, E_CITE, E_Q, E_ABBR, E_TIME, E_COL, E_COLGROUP, E_OBJECT, E_CANVAS,
  E_VIDEO, E_AUDIO, E_DATALIST, E_WBR, E_AREA, E_MAP, E_SOURCE, E_EMBED, E_TRACK, E_PICTURE, E_BIG, E_TT, E_N
};
#define EF_BLOQUE 1      /* empieza y acaba línea */
#define EF_VACIO 2       /* sin contenido ni etiqueta de cierre */
#define EF_CRUDO 4       /* su contenido es texto tal cual (script, style) */
#define EF_CRUDO_ENT 8   /*   o texto con entidades (title, textarea) */
#define EF_FUERA 16      /* no se enseña (script, style, head...) */
extern const u8 etq_band[E_N];

typedef struct Nodo Nodo;
struct Nodo {
  u8 texto;              /* 1: es texto (t, n); 0: un elemento */
  u8 etq;
  u8 oculto;             /* (estilo) 1 no se enseña */
  u8 abierto;            /* details, nav y aside plegables: 1 desplegado */
  u8 est;                /* (estilo) negrita, alineación... (EN_*) */
  u8 fg, bg;             /* (estilo) colores de la página, 0 = el heredado */
  short n_atr;
  short ctl;             /* su control de formulario + 1 */
  Nodo *padre, *hijo, *ult, *sig;
  char **atr;            /* nombre, valor, nombre, valor... */
  char *t;
  u32 n;
};
#define EN_NEGRITA 1
#define EN_CENTRO 2
#define EN_DERECHA 4
#define EN_PLEGABLE 8    /* nav o aside grande: sale plegado, con su línea */
#define EN_BOTON 16      /* un enlace con aspecto de botón (.btn) */
#define EN_VISTO 32      /* el estilo ya está calculado */
#define EN_SUBRAYA 64
#define EN_BLOQUE 128     /* la hoja de estilo lo hace bloque (display: block, flex...) */
const char *atr(const Nodo *, const char *);       /* NULL si no lo tiene */
int tiene_clase(const Nodo *, const char *);

typedef struct {
  Arena ar;
  Nodo *raiz, *body;
  char *titulo;
  char *base;            /* <base href> o NULL */
  char *css;             /* los <style>, uno tras otro */
  u32 n_css;
} Doc;
/* fuente: el HTML (UTF-8, o la página 1252 si latin1); se queda en la arena */
void doc_lee(Doc *, const u8 *fuente, u32 n, int latin1);
void doc_suelta(Doc *);
void texto_de(const Nodo *, char *b, int max);     /* el texto de dentro, junto */

/* ---- direcciones y HTTP (http.c) ---- */
typedef struct {
  int seguro, puerto;
  char servidor[128];
  char *ruta;            /* con la consulta; sin el fragmento */
} Url;
char *url_absoluta(const char *base, const char *rel);  /* malloc; NULL si no vale */
int url_parte(const char *u, Url *d);              /* d->ruta, malloc */
void url_codifica(const char *t, int n, char **d, u32 *nd, u32 *cap);  /* %XX, + */

typedef struct TLS TLS;
enum { H_NADA, H_DNS, H_CONECTA, H_TLS, H_ENVIA, H_CABECERAS, H_CUERPO, H_HECHO, H_ERROR };
typedef struct {
  /* la petición */
  char *url;
  int post;
  char *cuerpo; u32 n_cuerpo;
  /* la marcha */
  int fase, reusada, reintento, enviado;
  Url u;
  char *pet; u32 n_pet;
  u32 t0;
  /* la respuesta */
  int estado;            /* 200, 302... */
  char *cab; u32 n_cab, cap_cab;
  u8 *datos; u32 n_datos, cap_datos;
  int troceado, largo, hasta_cierre, cerrar;
  u32 trozo_q; int trozo_f;
  char tipo[80];
  char error[200];
} Pet;
void http_inicia(void);
void http_fin(void);
void http_empieza(Pet *p);       /* con url, post y cuerpo puestos (se los queda) */
int http_avanza(Pet *p);         /* 0 sigue, 1 hecha, -1 error (p->error) */
void http_suelta(Pet *p);
const char *http_cabecera(const Pet *p, const char *nombre);  /* hasta el CR */
const char *http_cifrado(void);  /* de la conexión en uso: "" si no va cifrada */
extern char http_idioma[32];

/* ---- la página: del documento a la rejilla de letras (pagina.c) ---- */
enum { C_TEXTO, C_CLAVE, C_CASILLA, C_RADIO, C_ENVIAR, C_BOTON, C_REINICIO, C_LISTA, C_AREA, C_OCULTO, C_ARCHIVO };
typedef struct {
  u8 tipo, desactivado, sololee, marcado, marcado0, visible, obligado, cambia_envia;
  u8 solo_js;                             /* un botón que solo hace algo con JavaScript */
  Nodo *nodo, *forma;
  const char *nombre;
  char *valor; int n_valor, cap_valor;   /* texto, área, y el valor de las casillas */
  char *valor0;
  int max;
  int x, y, w, h;                         /* en letras, en la página */
  int cursor, desp;                       /* texto: dónde se escribe y desde dónde se ve */
  Nodo **opc; int n_opc, elegida, elegida0;  /* lista */
} Ctl;
typedef struct { u8 est, fg, bg; short enlace, ctl; } Atr;
#define A_NEGRITA 1
#define A_SUBRAYA 2
#define A_RAYA 4         /* una raya horizontal en vez de letras */
#define A_TACHA 8
#define A_BOTON 16       /* enlace con aspecto de botón */
typedef struct { u16 n; u8 *c; u16 *a; } Fila;
typedef struct { const char *href; Nodo *nodo; int x, y; } Enlace;
typedef struct { u8 enlace; short n; } Foco;   /* enlace: 1 = enlace n; 0 = control n */
typedef struct {
  Doc doc;
  char *url;
  Fila *filas; int n_filas, cap_filas;
  int ancho;                              /* la fila más larga */
  int y_cont;                             /* la fila donde empieza lo principal (<main>), o -1 */
  Atr *atrs; int n_atrs, cap_atrs;
  Enlace *enl; int n_enl, cap_enl;
  Ctl *ctl; int n_ctl, cap_ctl;
  Foco *foco; int n_foco, cap_foco;
  u32 color[64]; int n_color;
  void *reglas; int n_reglas;
} Pagina;
/* colores fijos de la página (índices de Pagina.color) */
enum { K_TINTA = 1, K_FONDO, K_ENLACE, K_GRIS, K_ROJO, K_VERDE, K_TITULO, K_ZEBRA, K_MARCA, K_CABEZA, K_AVISO, K_N };
Pagina *pag_nueva(const char *url, const u8 *fuente, u32 n, int latin1, int texto_plano);
void pag_maqueta(Pagina *, int columnas);
void pag_suelta(Pagina *);
/* el formulario del control «con» (el que se ha pulsado, o -1): la dirección
 * y, si es POST, el cuerpo; devuelve 0 si no hay a dónde enviar. aviso: lo
 * que hay que confirmar antes (onsubmit con confirm) o "" */
int pag_formulario(Pagina *, Nodo *forma, int con, char **url, char **cuerpo, u32 *n_cuerpo, int *post);
const char *pag_confirma(Nodo *n, char *b, int max);   /* el texto de confirm('...') o NULL */
void ctl_pon(Ctl *, const char *t, int n);
int pag_ctl_de_label(Pagina *, Nodo *label);          /* el control de un <label>, o -1 */
#endif
