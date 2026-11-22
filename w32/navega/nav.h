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
#ifdef NAV_ANFITRION
/* [JS] la prueba en el anfitrión (tests/navjs/hazlo.sh): las del sistema */
#include <stdio.h>
#include <time.h>
unsigned long strtoul(const char *, char **, int);
#else
u32 strtoul(const char *, char **, int);
long time(long *);
/* [JS] script.c trae antes el stdio.h de w32/js/inc (el de QuickJS): su FILE */
#ifndef EOF
typedef struct FILE FILE;
FILE *fopen(const char *, const char *);
char *fgets(char *, int, FILE *);
int fputs(const char *, FILE *);
u32 fread(void *, u32, u32, FILE *);
u32 fwrite(const void *, u32, u32, FILE *);
int fclose(FILE *);
#endif
int fseek(FILE *, long, int);
long ftell(FILE *);
int remove(const char *);
#endif

/* cómo se presenta el Navegador a los servidores (User-Agent) */
#define NAV_VERSION "0.3"
#ifndef HTTP_AGENTE               /* [NETSURF] el Navegador completo (w32/surf) se presenta con el suyo */
#define HTTP_AGENTE "HispaNavegador/" NAV_VERSION " (Hispa-486)"
#endif

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
extern const char *const etq_nom[E_N];    /* [JS] los nombres («div»...) */

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
  char tipo_cuerpo[96];  /* el Content-Type del POST ("": de formulario) */
  int cruzada;           /* la pide una página de otro sitio (SameSite) */
  int sub;               /* es una imagen de la página, no la página */
  u32 max;               /* lo más que se acepta (0: 4 MB) */
  FILE *disco;           /* una descarga: el cuerpo va a este archivo, no a la memoria */
  int decidido;          /* (el Navegador ya ha mirado las cabeceras) */
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
  const char *js_cab;    /* [JS] cabeceras de más («Nombre: valor\r\n»...; las de fetch). Se pone tras http_empieza */
  const char *js_metodo; /* [JS] otro método (PUT, DELETE...) o NULL */
} Pet;
void http_inicia(void);
void http_fin(void);
void http_empieza(Pet *p);       /* con url, post y cuerpo puestos (se los queda) */
int http_avanza(Pet *p);         /* 0 sigue, 1 hecha, -1 error (p->error) */
void http_suelta(Pet *p);
const char *http_cabecera(const Pet *p, const char *nombre);  /* hasta el CR */
const char *http_cifrado(void);  /* de la conexión en uso: "" si no va cifrada */
extern char http_idioma[32];
extern const char *(*http_agente)(const char *servidor);
u32 http_fecha(const char *t, const char *fin);    /* fecha de HTTP -> segundos desde 1970 */
u32 http_ahora(void);
int http_galletas_guarda(const char *ruta);
void http_galletas_carga(const char *ruta);
int http_galletas_de(const char *servidor);
void http_galletas_borra(void);
extern int http_galletas_cambio;
char *http_galletas(const char *url);              /* [JS] document.cookie: «a=b; c=d» (malloc) o NULL */
void http_pon_galleta(const char *url, const char *linea);   /* [JS] document.cookie = «a=b; path=/» */
int nav_cruzada(const char *de, const char *a);     /* de otro sitio (SameSite): navega.c */

/* ---- la página: del documento a la rejilla de letras (pagina.c) ---- */
enum { C_TEXTO, C_CLAVE, C_CASILLA, C_RADIO, C_ENVIAR, C_BOTON, C_REINICIO, C_LISTA, C_AREA, C_OCULTO, C_ARCHIVO };
typedef struct {
  u8 tipo, desactivado, sololee, marcado, marcado0, visible, obligado, cambia_envia;
  u8 solo_js;                             /* un botón que solo hace algo con JavaScript */
  u8 peligro;                             /* un botón de lo que no tiene vuelta (btn-danger): granate y con «!» */
  Nodo *nodo, *forma;
  const char *nombre;
  char *valor; int n_valor, cap_valor;   /* texto, área, y el valor de las casillas */
  char *valor0;
  int max;
  int x, y, w, h;                         /* en letras, en la página */
  int cursor, desp;                       /* texto: dónde se escribe y desde dónde se ve */
  Nodo **opc; int n_opc, elegida, elegida0;  /* lista */
} Ctl;
typedef struct { u8 est, fg, bg; short enlace, ctl, img; } Atr;
#define A_NEGRITA 1
#define A_SUBRAYA 2
#define A_RAYA 4         /* una raya horizontal en vez de letras */
#define A_TACHA 8
#define A_BOTON 16       /* enlace con aspecto de botón (.btn): se pinta con su marco */
#define A_PELIGRO 32     /*   y de lo que no tiene vuelta (btn-danger): en granate */
typedef struct { u16 n; u8 *c; u16 *a; } Fila;
typedef struct { const char *href; Nodo *nodo; int x, y; } Enlace;
typedef struct { u8 enlace; short n; } Foco;   /* enlace: 1 = enlace n; 0 = control n */
/* las imágenes (<img>): se bajan después de la página, una a una */
#define NAV_LW 8                 /* una letra de la rejilla, en puntos */
#define NAV_LH 18                /* una fila */
#define IMG_MAX_W 360            /* lo más grande que se ve una imagen */
#define IMG_MAX_H 270
enum { IM_PENDIENTE, IM_BAJANDO, IM_LISTA, IM_FALLO };
typedef struct {
  Nodo *nodo;
  char *src;             /* la dirección entera (malloc); NULL: no se puede pedir */
  int pw, ph;            /* lo que pide la página (puntos; 0: no lo dice) */
  int w, h;              /* lo que se pinta (puntos), cuando ya está */
  int x, y, cw, ch;      /* dónde va en la rejilla (letras, filas); y < 0: no se ve */
  u8 estado;             /* IM_* */
  u8 visto;              /* la maqueta ha llegado a ella (no está escondida) */
  u8 *px;                /* w x h, filas de (w + 3) & ~3, colores de la paleta */
} Img;
typedef struct {
  Doc doc;
  char *url;
  Fila *filas; int n_filas, cap_filas;
  int ancho;                              /* la fila más larga */
  int y_cont;                             /* la fila donde empieza lo principal (<main>), o -1 */
  int y_menu, y_menu_ini;                 /* el primer menú (<nav>, .navbar): la fila de debajo y la suya (-1: no hay) */
  const char *ancla; int y_ancla;         /* la marca de la dirección (#nombre, sin «#»; la pone quien
                                           * maqueta) y la fila donde está (-1: no está) */
  int autofoco;                           /* el control con autofocus (el primero a la vista), o -1 */
  Atr *atrs; int n_atrs, cap_atrs;
  Enlace *enl; int n_enl, cap_enl;
  Ctl *ctl; int n_ctl, cap_ctl;
  Foco *foco; int n_foco, cap_foco;
  u32 color[64]; int n_color;
  void *reglas; int n_reglas;
  Img *img; int n_img, cap_img;
} Pagina;
/* colores fijos de la página (índices de Pagina.color) */
enum { K_TINTA = 1, K_FONDO, K_ENLACE, K_GRIS, K_ROJO, K_VERDE, K_TITULO, K_ZEBRA, K_MARCA, K_CABEZA, K_AVISO, K_N };
Pagina *pag_nueva(const char *url, const u8 *fuente, u32 n, int latin1, int texto_plano);
void pag_maqueta(Pagina *, int columnas);
int pag_foco_y(const Pagina *, int f);                /* la fila del foco f (enlace o control) */
int pag_foco_desde(const Pagina *, int fila);         /* el primer foco de esa fila o de más abajo, o -1 */
int pag_tab_inicio(const Pagina *);                   /* la fila donde empieza el Tab: lo principal o lo de tras el menú */
void pag_rotulo_boton(const Ctl *, char *b, int max); /* el texto de un botón tal como se ve (con «! » si es peligroso) */
void pag_suelta(Pagina *);
/* el formulario del control «con» (el que se ha pulsado, o -1): la dirección
 * y, si es POST, el cuerpo; devuelve 0 si no hay a dónde enviar. aviso: lo
 * que hay que confirmar antes (onsubmit con confirm) o "" */
int pag_formulario(Pagina *, Nodo *forma, int con, char **url, char **cuerpo, u32 *n_cuerpo, int *post, char *tipo);
int pag_multipart(Nodo *forma, int con, Pagina *p);   /* ¿se envía como multipart/form-data? */
int pag_patron(const char *patron, const char *t, int n);   /* ¿casa entero con el pattern? */
long pag_archivo_mide(const char *ruta);              /* -1 no se puede leer, -2 demasiado grande */
int pag_refresco(Pagina *, char **url);               /* <meta http-equiv=refresh>: segundos (-1: no hay) */
/* imagen.c */
u8 *img_descodifica(const u8 *d, u32 n, int pw, int ph, int max_w, int max_h, int *w, int *h, char *error);
const char *pag_confirma(Nodo *n, char *b, int max);   /* el texto de confirm('...') o NULL */
void ctl_pon(Ctl *, const char *t, int n);
int pag_ctl_de_label(Pagina *, Nodo *label);          /* el control de un <label>, o -1 */
void pag_ctl_rehaz(Pagina *, int con_js);             /* [JS] los controles otra vez (el árbol ha cambiado) */
void pag_rotulo_vacio(const Ctl *, char *b, int max); /* [JS] el rótulo de un botón sin texto */
extern int (*pag_clicable)(Nodo *);                   /* [JS] ¿tiene un clic de JavaScript? (NULL sin JavaScript) */
#endif
