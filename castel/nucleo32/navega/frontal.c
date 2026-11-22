/* =============================================================================
 *  frontal.c - NAVEGA.BIN, el Navegador de Hispa-Pentium: NetSurf
 *  (terceros/netsurf, GPL-2.0) en una ventana del escritorio de Hispa-Pentium.
 *  docs/PENTIUM.md, «El Navegador».
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  El frontal de NetSurf hecho nuestro, como w32/surf/surf.c en Hispa-486,
 *  pero sobre las llamadas del núcleo (32 a 40 y 80: la ventana, dibujar un
 *  mapa de puntos, los sucesos, los menús emergentes, el portapapeles):
 *    - arriba, las pestañas (con su «×» y la «+») y la barra: Atrás,
 *      Adelante, Recargar (Parar mientras carga), Inicio, la dirección (con el
 *      candado de las cifradas) y el menú (≡);
 *    - la página, que NetSurf pinta en un búfer de 32 bits (libnsfb,
 *      ns/plot.c); la barra de desplazamiento; abajo, la línea de estado;
 *    - lo de la ventana también se pinta en búferes de 32 bits, con las
 *      letras DejaVu de la página (letra.c), y se da al núcleo con la
 *      llamada 34 (OP_MAPA), solo lo que ha cambiado;
 *    - el ratón (la rueda: el suceso EV_RUEDA del núcleo 0.8.3), el teclado
 *      (de la página 437 a Unicode), el portapapeles del sistema, el menú
 *      del botón derecho, los cuadros de alert/confirm (otra ventana) y el
 *      planificador de NetSurf en el bucle de los sucesos.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utf8.h"
#include "netsurf/netsurf.h"
#include "netsurf/misc.h"
#include "netsurf/window.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "netsurf/mouse.h"
#include "netsurf/plotters.h"
#include "netsurf/clipboard.h"
#include "netsurf/fetch.h"
#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "netsurf/layout.h"
#include "netsurf/download.h"
#include "utils/file.h"
#include "html/box.h"
#include "html/box_inspect.h"
struct hlcache_handle;
struct box *html_get_box_tree(struct hlcache_handle *h);   /* (html/html_save.h) */
#include "html/html.h"
#include "html/private.h"
#include "utils/corestrings.h"
#include "guion.h"

#include "netsurf/browser.h"
#include "surf.h"
#include "rh.h"
#include "cssvar.h"
#include "woff.h"
#include "pentium.h"
#include "frontal.h"

typedef uint32_t u32;
typedef uint8_t u8;

#define VERSION_NAVEGADOR "0.2.2"

/* ---- medidas (puntos) ---- */
#define PEST_H 30              /* las pestañas */
#define BARRA_H 38             /* la barra: botones y dirección */
#define ARRIBA_H (PEST_H + BARRA_H)
#define ESTADO_H 22
#define DESP_W 14              /* la barra de desplazamiento */
#define PEST_MAX 220           /* lo más ancha que es una pestaña */
#define N_PESTANAS 12

/* ---- los colores (0xRRGGBB: los del escritorio, azules y verdes) ---- */
#define C_TINTA 0x14294A
#define C_GRIS 0x8C99AB
#define C_FONDO_PEST 0xC4DCF8
#define C_PEST 0xDCE9FA
#define C_PEST_SOBRE 0xE6F0FC
#define C_BARRA_ARR 0xF6FAFF
#define C_BARRA_ABA 0xE6F0FC
#define C_RAYA 0x9DB9DE
#define C_BOTON_SOBRE 0xCFE1F8
#define C_CAMPO_BORDE 0x8FB0DA
#define C_FOCO 0x2E7D32
#define C_SELECCION 0x2F6FD0
#define C_VERDE 0x2E8B57
#define C_OCRE 0xC77700
#define C_ESTADO 0xEAF3FE
#define C_PISTA 0xEEF3FA
#define C_PULGAR 0xA9C3E6
#define C_PULGAR_SOBRE 0x7FA2D2

/* ---- de NetSurf y lo nuestro de w32/surf ---- */
void ram_register_surface(void);        /* libnsfb (sin constructores) */
void memoria_cuenta(unsigned *bloques, unsigned *ahora, unsigned *max);
int msvcrt_arranca(void *h, u32 razon, void *x) __attribute__((stdcall));
int hspjs_arranca(void *h, u32 razon, void *x) __attribute__((stdcall));
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema);
u32 pent_memoria_kb(void);
extern int guion_memoria_kb;
char *__attribute__((stdcall)) GetCommandLineA(void);
u32 __attribute__((stdcall)) GetModuleFileNameA(void *, char *, u32);
int __attribute__((stdcall)) CreateDirectoryA(const char *, void *);

/* =============================================================================
 *  La página 437 (la del núcleo: el teclado, el portapapeles, los títulos)
 * ============================================================================= */
/* (0.8.5) la 437 de Hispa (tools/hispa437.py): con Á, Í, Ó, Ú, € y las demás
 * de más en los códigos de los recuadros mezclados (como la letra del núcleo) */
static const uint16_t alto437[128] = {
  0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7, 0xEA, 0xEB, 0xE8, 0xEF, 0xEE, 0xEC, 0xC4, 0xC5,
  0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9, 0xFF, 0xD6, 0xDC, 0xA2, 0xA3, 0xA5, 0x20A7, 0x192,
  0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF, 0x2310, 0xAC, 0xBD, 0xBC, 0xA1, 0xAB, 0xBB,
  0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0xC0, 0xC1, 0xC2, 0xC3, 0x2563, 0x2551, 0x2557, 0x255D, 0xC8, 0xCA, 0x2510,
  0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0xCB, 0xCC, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0xCD,
  0xCE, 0xCF, 0xD2, 0xD3, 0xD4, 0xD5, 0xD9, 0xDA, 0xDB, 0x2518, 0x250C, 0x2588, 0x2584, 0xE3, 0xF5, 0x2580,
  0x3B1, 0xDF, 0x152, 0x3C0, 0x3A3, 0x153, 0xB5, 0xA9, 0xAE, 0x398, 0x3A9, 0x3B4, 0x221E, 0x3C6, 0x3B5, 0x2229,
  0x2261, 0xB1, 0x2265, 0x2264, 0x20AC, 0x2026, 0xF7, 0x2248, 0xB0, 0x2219, 0xB7, 0x221A, 0x207F, 0xB2, 0x25A0, 0xA0 };
static u32 de437(u8 c) { return c < 0x80 ? c : alto437[c - 0x80]; }
static int a437(u32 u)
{
  int k;
  if (u < 0x80) return (int)u;
  for (k = 0; k < 128; k++) if (alto437[k] == u) return 0x80 + k;
  /* lo que no está: sin la tilde, o «?» */
  switch (u) {
  case 0xC0: case 0xC1: case 0xC2: case 0xC3: return 'A';
  case 0xC8: case 0xCA: case 0xCB: return 'E';
  case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'I';
  case 0xD2: case 0xD3: case 0xD4: case 0xD5: return 'O';
  case 0xD9: case 0xDA: case 0xDB: return 'U';
  case 0xE3: return 'a';
  case 0xF5: return 'o';
  case 0x2018: case 0x2019: return '\'';
  case 0x201C: case 0x201D: return '"';
  case 0x2013: case 0x2014: return '-';
  case 0x2026: return '.';
  case 0x20AC: return 'E';
  }
  return '?';
}
static int pon_utf8(u32 c, char *d)
{
  if (c < 0x80) { d[0] = (char)c; return 1; }
  if (c < 0x800) { d[0] = (char)(0xC0 | c >> 6); d[1] = (char)(0x80 | (c & 63)); return 2; }
  if (c < 0x10000) { d[0] = (char)(0xE0 | c >> 12); d[1] = (char)(0x80 | (c >> 6 & 63)); d[2] = (char)(0x80 | (c & 63)); return 3; }
  d[0] = (char)(0xF0 | c >> 18); d[1] = (char)(0x80 | (c >> 12 & 63)); d[2] = (char)(0x80 | (c >> 6 & 63)); d[3] = (char)(0x80 | (c & 63));
  return 4;
}
int frontal_437_a_utf8(const char *t, int n, char *d, int cabe)
{
  int k, l = 0;
  for (k = 0; k < n && l + 5 < cabe; k++) {
    u8 c = (u8)t[k];
    if (c == '\r') continue;
    l += pon_utf8(de437(c), d + l);
  }
  d[l] = 0;
  return l;
}
int frontal_utf8_a_437(const char *t, int n, char *d, int cabe)
{
  int k = 0, l = 0;
  while (k < n && l + 1 < cabe) {
    u32 c = utf8_to_ucs4(t + k, n - k);
    k = utf8_next(t, n, k);
    if (c == '\r') continue;
    d[l++] = (char)a437(c);
  }
  d[l] = 0;
  return l;
}
/* de la 1252 (los textos de guion.c y de MessageBoxA) a UTF-8 */
static void de_1252(const char *t, char *d, int cabe)
{
  char *r = NULL;
  if (utf8_from_enc(t, "WINDOWS-1252", 0, &r, NULL) == NSERROR_OK && r) { snprintf(d, cabe, "%s", r); free(r); }
  else snprintf(d, cabe, "%s", t);
}

/* =============================================================================
 *  Los búferes y dárselos a la ventana
 * ============================================================================= */
static u32 vent;               /* la ventana del núcleo */
static int an, al;             /* lo de dentro de la ventana */
static int pw, ph;             /* la zona de la página */
static nsfb_t *lienzo;         /* la página (pw x ph) */
static nsfb_t *arriba;         /* las pestañas y la barra (an x ARRIBA_H) */
static nsfb_t *pie;            /* la línea de estado (an x ESTADO_H) */
static nsfb_t *desp_fb;        /* la barra de desplazamiento (DESP_W x ph) */
static u32 *temporal;          /* para dar un trozo que no es de filas enteras */
static u32 n_temporal;

static void dibuja(u32 v, struct orden *o) { sc(SC_DIBUJAR, v, (u32)o, 0); }
/* un trozo (x, y, w, h) del búfer fb a la ventana v, en (vx, vy) */
static void sube_a(u32 v, nsfb_t *fb, int x, int y, int w, int h, int vx, int vy)
{
  uint8_t *px;
  int paso, fw, fh, k;
  struct orden o;
  if (!fb) return;
  nsfb_get_buffer(fb, &px, &paso);
  nsfb_get_geometry(fb, &fw, &fh, NULL);
  if (x < 0) { w += x; vx -= x; x = 0; }
  if (y < 0) { h += y; vy -= y; y = 0; }
  if (x + w > fw) w = fw - x;
  if (y + h > fh) h = fh - y;
  if (w <= 0 || h <= 0) return;
  memset(&o, 0, sizeof o);
  o.op = OP_MAPA;
  o.x = vx; o.y = vy; o.ancho = w; o.alto = h;
  if (x == 0 && w * 4 == paso) {
    o.datos = px + y * paso;
  } else {
    u32 n = (u32)w * h;
    if (n > n_temporal) {
      free(temporal);
      temporal = malloc(n * 4);
      n_temporal = temporal ? n : 0;
      if (!temporal) return;
    }
    for (k = 0; k < h; k++) memcpy(temporal + k * w, px + (y + k) * paso + x * 4, w * 4);
    o.datos = temporal;
  }
  dibuja(v, &o);
}
static void sube(nsfb_t *fb, int x, int y, int w, int h, int vx, int vy) { sube_a(vent, fb, x, y, w, h, vx, vy); }
static nsfb_t *nuevo_fb(nsfb_t *viejo, int w, int h)
{
  nsfb_t *f;
  if (viejo) nsfb_free(viejo);
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  f = nsfb_new(NSFB_SURFACE_RAM);
  if (!f) return NULL;
  nsfb_set_geometry(f, w, h, NSFB_FMT_XRGB8888);
  if (nsfb_init(f) == -1) { nsfb_free(f); return NULL; }
  return f;
}

/* ---- dibujar en un búfer: rectángulos, redondeados, rayas, flechas y letras ---- */
typedef struct { u32 *p; int w, h, paso; } Pl;
static Pl pl_de(nsfb_t *fb)
{
  Pl r;
  uint8_t *px;
  nsfb_get_buffer(fb, &px, &r.paso);
  nsfb_get_geometry(fb, &r.w, &r.h, NULL);
  r.p = (u32 *)px;
  r.paso /= 4;
  return r;
}
static void caja(Pl *p, int x, int y, int w, int h, u32 c)
{
  int i, j;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > p->w) w = p->w - x;
  if (y + h > p->h) h = p->h - y;
  for (j = 0; j < h; j++) { u32 *f = p->p + (y + j) * p->paso + x; for (i = 0; i < w; i++) f[i] = c; }
}
static u32 mezcla(u32 a, u32 b, int alfa)      /* alfa de 0 (a) a 256 (b) */
{
  u32 r = ((a >> 16 & 255) * (256 - alfa) + (b >> 16 & 255) * alfa) >> 8;
  u32 g = ((a >> 8 & 255) * (256 - alfa) + (b >> 8 & 255) * alfa) >> 8;
  u32 z = ((a & 255) * (256 - alfa) + (b & 255) * alfa) >> 8;
  return r << 16 | g << 8 | z;
}
static void punto(Pl *p, int x, int y, u32 c, int alfa)
{
  u32 *q;
  if (x < 0 || y < 0 || x >= p->w || y >= p->h || alfa <= 0) return;
  q = p->p + y * p->paso + x;
  *q = alfa >= 256 ? c : mezcla(*q & 0xFFFFFF, c, alfa);
}
static int raiz(int v) { int r = 0; while ((r + 1) * (r + 1) <= v) r++; return r; }
/* un rectángulo con las esquinas redondas (suavizadas); de arriba (top) y/o abajo */
static void redondo(Pl *p, int x, int y, int w, int h, int r, u32 c, int arriba_r, int abajo_r)
{
  int j;
  if (r * 2 > h) r = h / 2;
  if (r * 2 > w) r = w / 2;
  for (j = 0; j < h; j++) {
    int dy = -1, sangra = 0, borde = 0, i;
    if (j < r && arriba_r) dy = r - j;
    else if (j >= h - r && abajo_r) dy = j - (h - r - 1);
    if (dy > 0) {
      /* el ancho que falta a esa altura del círculo, en 1/16 de punto */
      int d2 = r * r * 256 - (dy * 16 - 8) * (dy * 16 - 8);
      int fuera = r * 16 - (d2 > 0 ? raiz(d2) : 0);
      sangra = fuera >> 4;
      borde = 256 - ((fuera & 15) << 4);
    }
    caja(p, x + sangra + (borde < 256 && dy > 0), y + j, w - 2 * sangra - 2 * (borde < 256 && dy > 0), 1, c);
    if (dy > 0 && borde < 256) {
      punto(p, x + sangra, y + j, c, borde);
      punto(p, x + w - 1 - sangra, y + j, c, borde);
    }
    (void)i;
  }
}
/* una raya de grosor g (de (x0, y0) a (x1, y1), en 1/16 de punto: suavizada) */
static void raya(Pl *p, int x0, int y0, int x1, int y1, int g16, u32 c)
{
  int minx = (x0 < x1 ? x0 : x1) - g16, maxx = (x0 > x1 ? x0 : x1) + g16;
  int miny = (y0 < y1 ? y0 : y1) - g16, maxy = (y0 > y1 ? y0 : y1) + g16;
  int64_t dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy;
  int x, y;
  for (y = miny >> 4; y <= (maxy >> 4) + 1; y++)
    for (x = minx >> 4; x <= (maxx >> 4) + 1; x++) {
      int64_t px = x * 16 + 8 - x0, py = y * 16 + 8 - y0, t, ex, ey, d2;
      int d, a;
      t = l2 ? (px * dx + py * dy) * 256 / l2 : 0;
      if (t < 0) t = 0;
      if (t > 256) t = 256;
      ex = px - dx * t / 256; ey = py - dy * t / 256;
      d2 = ex * ex + ey * ey;
      d = raiz((int)(d2 > 0x3FFFFFFF ? 0x3FFFFFFF : d2));
      a = (g16 / 2 + 8 - d) * 16;
      if (a > 256) a = 256;
      punto(p, x, y, c, a);
    }
}

/* ---- las letras de la ventana (las de la página: letra.c) ---- */
static plot_font_style_t estilo_de(int px, int negrita, u32 color)
{
  plot_font_style_t s;
  memset(&s, 0, sizeof s);
  s.family = PLOT_FONT_FAMILY_SANS_SERIF;
  s.size = px * 72 * PLOT_STYLE_SCALE / 96;
  s.weight = negrita ? 700 : 400;
  s.flags = FONTF_NONE;
  /* (NetSurf: 0xBBGGRR) */
  s.foreground = (color >> 16 & 255) | (color & 0xFF00) | (color & 255) << 16;
  s.background = 0xFFFFFF;
  return s;
}
static int ancho_de(const char *t, int n, int px, int negrita)
{
  plot_font_style_t s = estilo_de(px, negrita, 0);
  int w = 0;
  if (n < 0) n = strlen(t);
  surf_tabla_letras->width(&s, t, n, &w);
  return w;
}
/* el texto t (UTF-8) en el búfer, con su línea en (x, y) de alto h */
static void texto(nsfb_t *fb, int x, int y, int h, const char *t, int n, int px, int negrita, u32 color)
{
  plot_font_style_t s = estilo_de(px, negrita, color);
  if (n < 0) n = strlen(t);
  if (n <= 0) return;
  letra_pinta(fb, &s, x, y + letra_base(&s, h), t, n);
}
/* lo que cabe de t en «cabe» puntos, con «…» si se corta: su largo en octetos, y el ancho en *w */
static int corta(const char *t, int cabe, int px, int negrita, int *w, int *con_puntos)
{
  plot_font_style_t s = estilo_de(px, negrita, 0);
  size_t off = 0;
  int x = 0, n = strlen(t);
  *con_puntos = 0;
  surf_tabla_letras->width(&s, t, n, &x);
  if (x <= cabe) { *w = x; return n; }
  cabe -= ancho_de("\xe2\x80\xa6", 3, px, negrita);
  if (cabe < 0) cabe = 0;
  surf_tabla_letras->position(&s, t, n, cabe, &off, &x);
  while (off > 0 && x > cabe) { off = utf8_prev(t, off); surf_tabla_letras->width(&s, t, off, &x); }
  *w = x;
  *con_puntos = 1;
  return (int)off;
}
static void texto_cortado(nsfb_t *fb, int x, int y, int h, const char *t, int cabe, int px, int negrita, u32 color)
{
  int w, puntos, n = corta(t, cabe, px, negrita, &w, &puntos);
  texto(fb, x, y, h, t, n, px, negrita, color);
  if (puntos) texto(fb, x + w, y, h, "\xe2\x80\xa6", 3, px, negrita, color);
}
static void fb_entero(nsfb_t *fb)
{
  int w, h;
  nsfb_bbox_t c;
  nsfb_get_geometry(fb, &w, &h, NULL);
  c.x0 = 0; c.y0 = 0; c.x1 = w; c.y1 = h;
  nsfb_plot_set_clip(fb, &c);
}

/* =============================================================================
 *  Las pestañas (una gui_window de NetSurf cada una)
 * ============================================================================= */
struct gui_window {
  struct browser_window *bw;
  int sx, sy;                  /* desde dónde se ve la página (puntos) */
  int ext_w, ext_h;            /* lo que mide la página */
  int caret_x, caret_y, caret_h;   /* el cursor de un campo (caret_h 0: no hay) */
  int cargando;
  char titulo[200];
};
static struct gui_window *pest[N_PESTANAS];
static int n_pest;
static struct gui_window *actual;
static struct { int x0, y0, x1, y1; } sucio;   /* lo que hay que repintar de la página */
static int subir_toda;         /* la página se ha movido en el búfer: darla entera a la ventana */
static int salir;
static char estado[400];       /* UTF-8 */
static char aviso_estado[300]; /* (de las descargas: tiene prioridad) */
static u32 aviso_hasta;
static char inicio[400] = "https://www.google.es/";
static char idioma[32] = "es";
static char galletas[300];
char frontal_descargas[260];
static char *registro;
void surf_reg(const char *f, ...)
{
  va_list ap;
  FILE *r;
  if (!registro || !(r = fopen(registro, "a"))) return;
  fprintf(r, "%u ", (unsigned)pent_tics());
  va_start(ap, f);
  vfprintf(r, f, ap);
  va_end(ap);
  fputc('\n', r);
  fclose(r);
}
static int indice(struct gui_window *g)
{
  int k;
  for (k = 0; k < n_pest; k++) if (pest[k] == g) return k;
  return -1;
}

/* la dirección: un campo de texto propio, en Unicode */
static u32 dir[1024];
static int n_dir, dir_cur, dir_desp, foco_dir, dir_sel;
/* la barra de desplazamiento: arrastrar el pulgar */
static int arrastra = -1, arr_y0, arr_sy0;
static int boton_dentro;       /* se pulsó dentro de la página (el botón izquierdo) */
static int sobre = -1;         /* lo que hay debajo del ratón en la barra (para resaltarlo) */
static int sobre_desp;

static int cargando(void) { return actual && actual->cargando; }

/* =============================================================================
 *  La barra de arriba: pestañas, botones, la dirección y el menú
 * ============================================================================= */
enum { B_ATRAS, B_ADELANTE, B_RECARGA, B_INICIO, B_MENU, B_N };
#define S_PEST 100             /* (sobre: 100 + k la pestaña k; 200 + k su «×»; 300 la «+») */
static struct { int x, y, w, h; } b_r[B_N], dir_r, pest_r[N_PESTANAS], cierra_r[N_PESTANAS], mas_r;
static int b_activo(int k)
{
  if (!actual) return 0;
  if (k == B_ATRAS) return browser_window_back_available(actual->bw);
  if (k == B_ADELANTE) return browser_window_forward_available(actual->bw);
  return 1;
}
static int dentro(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
static void coloca(void)
{
  int k, x = 6, w, n = n_pest ? n_pest : 1;
  pw = an - DESP_W; ph = al - ARRIBA_H - ESTADO_H;
  if (pw < 64) pw = 64;
  if (ph < 32) ph = 32;
  cssvar_ancho = pw; cssvar_alto = ph;          /* (vw y vh en clamp(), cssvar.c) */
  /* las pestañas */
  w = (an - 6 - 40) / n;
  if (w > PEST_MAX) w = PEST_MAX;
  if (w < 60) w = 60;
  for (k = 0; k < n_pest; k++) {
    pest_r[k].x = x; pest_r[k].y = 5; pest_r[k].w = w - 2; pest_r[k].h = PEST_H - 5;
    cierra_r[k].x = x + w - 2 - 22; cierra_r[k].y = 9; cierra_r[k].w = 18; cierra_r[k].h = 18;
    x += w;
  }
  mas_r.x = x + 4; mas_r.y = 7; mas_r.w = 24; mas_r.h = 22;
  /* los botones */
  x = 6;
  for (k = 0; k < B_MENU; k++) {
    b_r[k].x = x; b_r[k].y = PEST_H + 4; b_r[k].w = 30; b_r[k].h = 30;
    x += 32;
  }
  b_r[B_MENU].x = an - 6 - 30; b_r[B_MENU].y = PEST_H + 4; b_r[B_MENU].w = 30; b_r[B_MENU].h = 30;
  dir_r.x = x + 6; dir_r.y = PEST_H + 5; dir_r.w = b_r[B_MENU].x - 6 - dir_r.x; dir_r.h = 28;
}
/* los dibujos de los botones (en 1/16 de punto: g16 es el grosor) */
static void icono(Pl *p, int k, int x, int y, u32 c)
{
  int cx = (x + 15) * 16, cy = (y + 15) * 16;
  switch (k) {
  case B_ATRAS:
    raya(p, cx + 40, cy - 88, cx - 48, cy, 36, c);
    raya(p, cx - 48, cy, cx + 40, cy + 88, 36, c);
    break;
  case B_ADELANTE:
    raya(p, cx - 40, cy - 88, cx + 48, cy, 36, c);
    raya(p, cx + 48, cy, cx - 40, cy + 88, 36, c);
    break;
  case B_RECARGA:
    if (cargando()) {        /* parar: una cruz */
      raya(p, cx - 72, cy - 72, cx + 72, cy + 72, 36, c);
      raya(p, cx + 72, cy - 72, cx - 72, cy + 72, 36, c);
    } else {                 /* recargar: tres cuartos de círculo y su punta */
      static const short arco[][2] = { { 80, -80 }, { 105, -40 }, { 112, 0 }, { 105, 43 }, { 80, 80 }, { 43, 105 }, { 0, 112 },
                                       { -43, 105 }, { -80, 80 }, { -105, 43 }, { -112, 0 }, { -105, -43 }, { -80, -80 }, { -43, -105 }, { 0, -112 } };
      int i;
      for (i = 0; i + 1 < (int)(sizeof arco / sizeof arco[0]); i++)
        raya(p, cx + arco[i][0], cy + arco[i][1], cx + arco[i + 1][0], cy + arco[i + 1][1], 32, c);
      raya(p, cx + 0, cy - 112, cx - 40, cy - 150, 32, c);
      raya(p, cx + 0, cy - 112, cx - 40, cy - 72, 32, c);
    }
    break;
  case B_INICIO:             /* una casa */
    raya(p, cx - 112, cy - 8, cx, cy - 112, 32, c);
    raya(p, cx, cy - 112, cx + 112, cy - 8, 32, c);
    raya(p, cx - 80, cy - 30, cx - 80, cy + 104, 32, c);
    raya(p, cx + 80, cy - 30, cx + 80, cy + 104, 32, c);
    raya(p, cx - 80, cy + 104, cx + 80, cy + 104, 32, c);
    raya(p, cx - 20, cy + 104, cx - 20, cy + 40, 28, c);
    raya(p, cx + 20, cy + 40, cx + 20, cy + 104, 28, c);
    raya(p, cx - 20, cy + 40, cx + 20, cy + 40, 28, c);
    break;
  case B_MENU:               /* tres rayas */
    raya(p, cx - 88, cy - 72, cx + 88, cy - 72, 32, c);
    raya(p, cx - 88, cy, cx + 88, cy, 32, c);
    raya(p, cx - 88, cy + 72, cx + 88, cy + 72, 32, c);
    break;
  }
}
static void candado(Pl *p, int x, int y, u32 c)
{
  int cx = x * 16, cy = y * 16;
  /* el arco y el cuerpo */
  raya(p, cx + 48, cy + 112, cx + 48, cy + 64, 28, c);
  raya(p, cx + 48, cy + 64, cx + 80, cy + 32, 28, c);
  raya(p, cx + 80, cy + 32, cx + 112, cy + 32, 28, c);
  raya(p, cx + 112, cy + 32, cx + 144, cy + 64, 28, c);
  raya(p, cx + 144, cy + 64, cx + 144, cy + 112, 28, c);
  redondo(p, x + 1, y + 7, 11, 9, 2, c, 1, 1);
}
static void dir_utf8(char *d, int cabe)
{
  int k, l = 0;
  for (k = 0; k < n_dir && l + 5 < cabe; k++) l += pon_utf8(dir[k], d + l);
  d[l] = 0;
}
/* el ancho de las n primeras letras de la dirección */
static int dir_ancho(int desde, int n)
{
  char b[4200];
  int k, l = 0;
  for (k = desde; k < desde + n && k < n_dir && l + 5 < (int)sizeof b; k++) l += pon_utf8(dir[k], b + l);
  return ancho_de(b, l, 14, 0);
}
static int https_actual(void)
{
  nsurl *u = actual ? browser_window_access_url(actual->bw) : NULL;
  return u && !strncmp(nsurl_access(u), "https:", 6) && surf_cifrada;
}
static void pinta_barra(void)
{
  Pl p;
  int k, y, x0;
  if (!arriba) return;
  fb_entero(arriba);
  p = pl_de(arriba);
  /* el fondo de las pestañas */
  caja(&p, 0, 0, an, PEST_H, C_FONDO_PEST);
  for (k = 0; k < n_pest; k++) {
    int act = pest[k] == actual, so = sobre == S_PEST + k || sobre == 200 + k;
    u32 c = act ? C_BARRA_ARR : so ? C_PEST_SOBRE : C_PEST;
    const char *t = pest[k]->titulo[0] ? pest[k]->titulo : "Nueva pesta\xc3\xb1" "a";
    redondo(&p, pest_r[k].x, pest_r[k].y, pest_r[k].w, pest_r[k].h + 6, 8, c, 1, 0);
    if (pest[k]->cargando) {   /* mientras carga: un punto ocre */
      redondo(&p, pest_r[k].x + 9, pest_r[k].y + 9, 8, 8, 4, C_OCRE, 1, 1);
      x0 = pest_r[k].x + 22;
    } else x0 = pest_r[k].x + 10;
    texto_cortado(arriba, x0, pest_r[k].y, pest_r[k].h, t, cierra_r[k].x - x0 - 4, 13, act, C_TINTA);
    if (sobre == 200 + k) redondo(&p, cierra_r[k].x, cierra_r[k].y, cierra_r[k].w, cierra_r[k].h, 9, C_BOTON_SOBRE, 1, 1);
    raya(&p, (cierra_r[k].x + 5) * 16, (cierra_r[k].y + 5) * 16, (cierra_r[k].x + 13) * 16, (cierra_r[k].y + 13) * 16, 22, act || so ? C_TINTA : C_GRIS);
    raya(&p, (cierra_r[k].x + 13) * 16, (cierra_r[k].y + 5) * 16, (cierra_r[k].x + 5) * 16, (cierra_r[k].y + 13) * 16, 22, act || so ? C_TINTA : C_GRIS);
  }
  if (n_pest < N_PESTANAS) {
    if (sobre == 300) redondo(&p, mas_r.x, mas_r.y, mas_r.w, mas_r.h, 8, C_PEST_SOBRE, 1, 1);
    raya(&p, (mas_r.x + 12) * 16, (mas_r.y + 5) * 16, (mas_r.x + 12) * 16, (mas_r.y + 17) * 16, 28, C_TINTA);
    raya(&p, (mas_r.x + 6) * 16, (mas_r.y + 11) * 16, (mas_r.x + 18) * 16, (mas_r.y + 11) * 16, 28, C_TINTA);
  }
  /* la barra, con su degradado */
  for (y = PEST_H; y < ARRIBA_H - 1; y++) caja(&p, 0, y, an, 1, mezcla(C_BARRA_ARR, C_BARRA_ABA, (y - PEST_H) * 256 / BARRA_H));
  caja(&p, 0, ARRIBA_H - 1, an, 1, C_RAYA);
  for (k = 0; k < B_N; k++) {
    int act = b_activo(k);
    if (sobre == k && act) redondo(&p, b_r[k].x, b_r[k].y, b_r[k].w, b_r[k].h, 8, C_BOTON_SOBRE, 1, 1);
    icono(&p, k, b_r[k].x, b_r[k].y, !act ? 0xB4BFCE : k == B_RECARGA && cargando() ? C_OCRE : C_TINTA);
  }
  /* la dirección */
  redondo(&p, dir_r.x, dir_r.y, dir_r.w, dir_r.h, 14, foco_dir ? C_FOCO : C_CAMPO_BORDE, 1, 1);
  redondo(&p, dir_r.x + 1 + foco_dir, dir_r.y + 1 + foco_dir, dir_r.w - 2 - 2 * foco_dir, dir_r.h - 2 - 2 * foco_dir, 13 - foco_dir, 0xFFFFFF, 1, 1);
  {
    int tx = dir_r.x + 14, cabe, vis, k2;
    char b[4200];
    if (!foco_dir && https_actual()) { candado(&p, tx - 2, dir_r.y + 6, C_VERDE); tx += 18; }
    cabe = dir_r.x + dir_r.w - 14 - tx;
    if (!foco_dir) dir_desp = 0;
    else {
      if (dir_cur < dir_desp) dir_desp = dir_cur;
      while (dir_desp < dir_cur && dir_ancho(dir_desp, dir_cur - dir_desp) > cabe - 4) dir_desp++;
    }
    /* cuántas letras caben desde dir_desp */
    for (vis = 0; dir_desp + vis < n_dir && dir_ancho(dir_desp, vis + 1) <= cabe; vis++) ;
    {
      int l = 0;
      for (k2 = dir_desp; k2 < dir_desp + vis && l + 5 < (int)sizeof b; k2++) l += pon_utf8(dir[k2], b + l);
      b[l] = 0;
      if (foco_dir && dir_sel && l) {
        int w = ancho_de(b, l, 14, 0);
        caja(&p, tx - 1, dir_r.y + 5, w + 2, dir_r.h - 10, C_SELECCION);
        texto(arriba, tx, dir_r.y, dir_r.h, b, l, 14, 0, 0xFFFFFF);
      } else if (!foco_dir) {
        /* sin el foco: el servidor en negro y lo demás en gris (como los grandes) */
        const char *s = strstr(b, "://"), *f;
        if (s && (f = strchr(s + 3, '/'))) {
          int w1 = ancho_de(b, f - b, 14, 0);
          texto(arriba, tx, dir_r.y, dir_r.h, b, f - b, 14, 0, C_TINTA);
          texto(arriba, tx + w1, dir_r.y, dir_r.h, f, l - (f - b), 14, 0, 0x66758A);
        } else texto(arriba, tx, dir_r.y, dir_r.h, b, l, 14, 0, C_TINTA);
      } else texto(arriba, tx, dir_r.y, dir_r.h, b, l, 14, 0, C_TINTA);
    }
    if (foco_dir && !dir_sel) caja(&p, tx + dir_ancho(dir_desp, dir_cur - dir_desp), dir_r.y + 6, 2, dir_r.h - 12, C_FOCO);
  }
  sube(arriba, 0, 0, an, ARRIBA_H, 0, 0);
}
static void pinta_estado(void)
{
  Pl p;
  int carga = cargando();
  const char *d = carga ? "Cargando\xe2\x80\xa6" : https_actual() ? "Conexi\xc3\xb3n cifrada" : "";
  const char *t = aviso_estado[0] && (int)(pent_tics() - aviso_hasta) < 0 ? aviso_estado : estado;
  int wd;
  if (!pie) return;
  fb_entero(pie);
  p = pl_de(pie);
  caja(&p, 0, 0, an, ESTADO_H, C_ESTADO);
  caja(&p, 0, 0, an, 1, C_RAYA);
  wd = *d ? ancho_de(d, -1, 12, 0) : 0;
  texto_cortado(pie, 8, 1, ESTADO_H - 1, t, an - 24 - wd, 12, 0, C_TINTA);
  if (*d) {
    if (!carga) candado(&p, an - 10 - wd - 18, 3, C_VERDE);
    texto(pie, an - 8 - wd, 1, ESTADO_H - 1, d, -1, 12, 0, carga ? C_OCRE : C_VERDE);
  }
  sube(pie, 0, 0, an, ESTADO_H, 0, al - ESTADO_H);
}
/* la barra de desplazamiento: el pulgar */
static void pulgar(int *y, int *h)
{
  int alto = ph - 4, tot = actual && actual->ext_h > ph ? actual->ext_h : ph;
  *h = (int)((int64_t)alto * ph / tot);
  if (*h < 24) *h = 24;
  if (*h > alto) *h = alto;
  *y = 2 + (tot > ph && actual ? (int)((int64_t)actual->sy * (alto - *h) / (tot - ph)) : 0);
}
static void pinta_desp(void)
{
  Pl p;
  int y, h;
  if (!desp_fb) return;
  fb_entero(desp_fb);
  p = pl_de(desp_fb);
  caja(&p, 0, 0, DESP_W, ph, C_PISTA);
  caja(&p, 0, 0, 1, ph, 0xDCE5F2);
  if (actual && actual->ext_h > ph) {
    pulgar(&y, &h);
    redondo(&p, 3, y, DESP_W - 6, h, 4, sobre_desp || arrastra >= 0 ? C_PULGAR_SOBRE : C_PULGAR, 1, 1);
  }
  sube(desp_fb, 0, 0, DESP_W, ph, pw, ARRIBA_H);
}
static void repinta_barras(void)
{
  pinta_barra();
  pinta_estado();
  pinta_desp();
}
void surf_estado(const char *t)
{
  if (!t) t = "";
  if (!strcmp(t, estado)) return;
  snprintf(estado, sizeof estado, "%s", t);
  if (vent) pinta_estado();
}
void frontal_aviso_estado(const char *t)
{
  snprintf(aviso_estado, sizeof aviso_estado, "%s", t ? t : "");
  aviso_hasta = pent_tics() + 8 * pent_hz();
  if (vent) pinta_estado();
}

/* =============================================================================
 *  La página: el búfer, lo sucio y darlo a la ventana
 * ============================================================================= */
static void ensucia(int x0, int y0, int x1, int y1)
{
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > pw) x1 = pw;
  if (y1 > ph) y1 = ph;
  if (x0 >= x1 || y0 >= y1) return;
  if (sucio.x0 >= sucio.x1) { sucio.x0 = x0; sucio.y0 = y0; sucio.x1 = x1; sucio.y1 = y1; return; }
  if (x0 < sucio.x0) sucio.x0 = x0;
  if (y0 < sucio.y0) sucio.y0 = y0;
  if (x1 > sucio.x1) sucio.x1 = x1;
  if (y1 > sucio.y1) sucio.y1 = y1;
}
static void ensucia_todo(void) { ensucia(0, 0, pw, ph); }
static void pinta_sucio(void)
{
  struct redraw_context ctx = { .interactive = true, .background_images = true, .plot = &fb_plotters };
  struct rect clip;
  nsfb_bbox_t c;
  u32 t0;
  if (sucio.x0 >= sucio.x1 || !actual || !lienzo) return;
  clip.x0 = sucio.x0; clip.y0 = sucio.y0; clip.x1 = sucio.x1; clip.y1 = sucio.y1;
  sucio.x0 = sucio.x1 = 0;
  c.x0 = clip.x0; c.y0 = clip.y0; c.x1 = clip.x1; c.y1 = clip.y1;
  framebuffer_set_surface(lienzo);
  nsfb_plot_set_clip(lienzo, &c);
  nsfb_plot_rectangle_fill(lienzo, &c, 0xFFFFFFFF);
  t0 = pent_tics();
  browser_window_redraw(actual->bw, -actual->sx, -actual->sy, &clip, &ctx);
  surf_reg("pinta %d,%d-%d,%d: %u tics", clip.x0, clip.y0, clip.x1, clip.y1, (unsigned)(pent_tics() - t0));
  if (actual->caret_h) {
    nsfb_bbox_t l;
    nsfb_plot_pen_t pen;
    l.x0 = l.x1 = actual->caret_x - actual->sx;
    l.y0 = actual->caret_y - actual->sy;
    l.y1 = l.y0 + actual->caret_h;
    pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
    pen.stroke_width = 1;
    pen.stroke_colour = 0xFF000000;
    nsfb_plot_line(lienzo, &l, &pen);
  }
  if (subir_toda) { subir_toda = 0; sube(lienzo, 0, 0, pw, ph, 0, ARRIBA_H); }
  else sube(lienzo, clip.x0, clip.y0, clip.x1 - clip.x0, clip.y1 - clip.y0, clip.x0, ARRIBA_H + clip.y0);
}
static void nuevos_buferes(void)
{
  static int hecho;
  if (!hecho) { ram_register_surface(); hecho = 1; }
  lienzo = nuevo_fb(lienzo, pw, ph);
  arriba = nuevo_fb(arriba, an, ARRIBA_H);
  pie = nuevo_fb(pie, an, ESTADO_H);
  desp_fb = nuevo_fb(desp_fb, DESP_W, ph);
  if (lienzo) framebuffer_set_surface(lienzo);
  surf_reg("lienzo %dx%d", pw, ph);
  ensucia_todo();
}

/* ¿tiene la página cajas fijas (position: fixed o sticky)? Entonces, al
 * desplazar, se pinta entera: mover lo ya pintado se llevaría la barra
 * lateral de ClicGest con la página (y se veía cortada). Se mira una vez por
 * árbol de cajas (JavaScript lo rehace entero: rehaz.c) */
static int hay_fijos(void)
{
  static const struct box *visto;
  static int hay;
  const struct box *b = NULL;
  if (actual && browser_window_get_content(actual->bw) && content_get_type(browser_window_get_content(actual->bw)) == CONTENT_HTML)
    b = html_get_box_tree(browser_window_get_content(actual->bw));
  if (!b) return 0;
  if (b != visto) { visto = b; hay = box_hispa_hay_fijos(b); }
  return hay;
}

/* el desplazamiento: hasta dónde se puede */
static void desplaza_a(int x, int y)
{
  int mx, my;
  if (!actual) return;
  mx = actual->ext_w - pw; my = actual->ext_h - ph;
  if (x > mx) x = mx;
  if (y > my) y = my;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x == actual->sx && y == actual->sy) return;
  {
    int dy = y - actual->sy;
    if (x == actual->sx && sucio.x0 >= sucio.x1 && lienzo && dy && (dy < 0 ? -dy : dy) < ph * 3 / 4 && !hay_fijos()) {
      /* solo de arriba abajo: lo ya pintado se mueve en el búfer y se pinta
       * solo la franja que entra (en un Pentium, mucho menos que la página) */
      uint8_t *px;
      int paso, k;
      nsfb_get_buffer(lienzo, &px, &paso);
      if (dy > 0) for (k = 0; k < ph - dy; k++) memcpy(px + k * paso, px + (k + dy) * paso, pw * 4);
      else for (k = ph - 1; k >= -dy; k--) memcpy(px + k * paso, px + (k + dy) * paso, pw * 4);
      actual->sy = y;
      if (dy > 0) ensucia(0, ph - dy, pw, ph); else ensucia(0, 0, pw, -dy);
      subir_toda = 1;
    } else {
      actual->sx = x; actual->sy = y;
      ensucia_todo();
    }
  }
  pinta_desp();
}
static void desplaza(int dx, int dy) { if (actual) desplaza_a(actual->sx + dx, actual->sy + dy); }

/* =============================================================================
 *  Las ventanas de NetSurf (gui_window_table): aquí, las pestañas
 * ============================================================================= */
static void pon_titulo(void)
{
  char b[260], c[200];
  const char *t = actual && actual->titulo[0] ? actual->titulo : "";
  int n;
  /* (en el título caben 63 letras: el núcleo, MAX_TÍTULO) */
  n = frontal_utf8_a_437(t, strlen(t), c, sizeof c);
  if (n > 48) { c[45] = 0; strcat(c, "..."); }
  snprintf(b, sizeof b, "%s%sNavegador", c, *c ? " - " : "");
  memcpy(c, b, 63);
  c[63] = 0;
  if (vent) sc(SC_TITULO, vent, (u32)c, 0);
}
static void dir_de(const char *u8)
{
  int k = 0, n = strlen(u8);
  n_dir = 0;
  while (k < n && n_dir < (int)(sizeof dir / sizeof dir[0])) {
    dir[n_dir++] = utf8_to_ucs4(u8 + k, n - k);
    k = utf8_next(u8, n, k);
  }
  dir_cur = n_dir; dir_desp = 0;
}
static void a_pestana(struct gui_window *g)
{
  if (!g) return;
  actual = g;
  foco_dir = 0;
  {
    nsurl *u = browser_window_access_url(g->bw);
    dir_de(u ? nsurl_access(u) : "");
  }
  ensucia_todo();
  pon_titulo();
  repinta_barras();
}
static struct gui_window *v_crea(struct browser_window *bw, struct gui_window *existe, gui_window_create_flags f)
{
  struct gui_window *g;
  (void)existe;
  if (n_pest == N_PESTANAS) return NULL;
  g = calloc(1, sizeof *g);
  if (!g) return NULL;
  g->bw = bw;
  pest[n_pest++] = g;
  coloca();
  if (!actual || !(f & GW_CREATE_TAB) || (f & GW_CREATE_FOREGROUND)) {
    actual = g;
    foco_dir = (f & GW_CREATE_FOCUS_LOCATION) != 0;
    n_dir = dir_cur = 0;
    ensucia_todo();
  }
  if (vent) repinta_barras();
  return g;
}
static void v_destruye(struct gui_window *g)
{
  int k = indice(g);
  if (k < 0) return;
  memmove(pest + k, pest + k + 1, (n_pest - k - 1) * sizeof pest[0]);
  n_pest--;
  if (actual == g) actual = n_pest ? pest[k < n_pest ? k : n_pest - 1] : NULL;
  free(g);
  if (!n_pest) { salir = 1; return; }
  coloca();
  if (actual) a_pestana(actual);
}
static nserror v_invalida(struct gui_window *g, const struct rect *r)
{
  if (g != actual) return NSERROR_OK;
  if (!r) ensucia_todo();
  else ensucia(r->x0 - g->sx, r->y0 - g->sy, r->x1 - g->sx, r->y1 - g->sy);
  return NSERROR_OK;
}
static bool v_da_desp(struct gui_window *g, int *sx, int *sy) { *sx = g->sx; *sy = g->sy; return true; }
static nserror v_pon_desp(struct gui_window *g, const struct rect *r)
{
  if (g != actual) { g->sx = r->x0; g->sy = r->y0; return NSERROR_OK; }
  desplaza_a(r->x0, r->y0);
  return NSERROR_OK;
}
static nserror v_medidas(struct gui_window *g, int *w, int *h) { (void)g; *w = pw; *h = ph; return NSERROR_OK; }
static void v_extension(struct gui_window *g)
{
  int w, h;
  if (browser_window_get_extents(g->bw, true, &w, &h) == NSERROR_OK) { g->ext_w = w; g->ext_h = h; }
  if (g == actual) { desplaza_a(g->sx, g->sy); repinta_barras(); }
}
static nserror v_suceso(struct gui_window *g, enum gui_window_event e)
{
  switch (e) {
  case GW_EVENT_UPDATE_EXTENT: v_extension(g); break;
  case GW_EVENT_REMOVE_CARET:
    if (g->caret_h) { v_invalida(g, &(struct rect){ g->caret_x - 1, g->caret_y, g->caret_x + 2, g->caret_y + g->caret_h }); g->caret_h = 0; }
    break;
  case GW_EVENT_START_THROBBER:
    g->cargando = 1;
    pent_serie("navega: cargando");
    repinta_barras();
    break;
  case GW_EVENT_STOP_THROBBER: {
    unsigned b, a, m;
    nsurl *u = browser_window_access_url(g->bw);
    g->cargando = 0;
    memoria_cuenta(&b, &a, &m);
    pent_serie("navega: hecho %s [%u KB en uso, %u KB lo m\xa0s, %u KB del sistema, %u marcos libres]", u ? nsurl_access(u) : "", a, m,
               (unsigned)pent_memoria_kb(), (unsigned)sc(SC_DATO, DATO_MARCOS_LIBRES, 0, 0));
    repinta_barras();
    break;
  }
  default: break;
  }
  return NSERROR_OK;
}
static void v_titulo(struct gui_window *g, const char *t)
{
  snprintf(g->titulo, sizeof g->titulo, "%s", t ? t : "");
  if (g == actual) pon_titulo();
  if (vent) pinta_barra();
}
static nserror v_url(struct gui_window *g, struct nsurl *u)
{
  if (g != actual || foco_dir) return NSERROR_OK;
  dir_de(nsurl_access(u));
  if (vent) pinta_barra();
  return NSERROR_OK;
}
static void v_estado(struct gui_window *g, const char *t)
{
  if (registro) {
    unsigned b, a, m;
    memoria_cuenta(&b, &a, &m);
    surf_reg("estado: %s  [memoria: %u KB pedidos, %u KB en uso, %u KB lo m\xc3\xa1s]", t, b, a, m);
  }
  if (g == actual) surf_estado(t);
}
static void v_puntero(struct gui_window *g, enum gui_pointer_shape s) { (void)g; (void)s; }   /* (el núcleo no deja cambiar la flecha) */
static void v_cursor(struct gui_window *g, int x, int y, int h, const struct rect *clip)
{
  (void)clip;
  if (g->caret_h) v_invalida(g, &(struct rect){ g->caret_x - 1, g->caret_y, g->caret_x + 2, g->caret_y + g->caret_h });
  g->caret_x = x; g->caret_y = y; g->caret_h = h;
  v_invalida(g, &(struct rect){ x - 1, y, x + 2, y + h });
}
static void v_consola(struct gui_window *g, browser_window_console_source src, const char *m, size_t n,
                      browser_window_console_flags f)
{
  (void)g; (void)src; (void)m; (void)n; (void)f;
}

/* [JS] lo que pide guion.c a la ventana */
void *surf_ventana(void) { return (void *)vent; }
void surf_desp(int *x, int *y) { *x = actual ? actual->sx : 0; *y = actual ? actual->sy : 0; }
void surf_ve_a(int x, int y) { desplaza_a(x, y); }
void surf_pon_dir(const char *u8)
{
  if (foco_dir) return;
  dir_de(u8);
  if (vent) pinta_barra();
}

static struct gui_window_table tabla_ventana = {
  .create = v_crea, .destroy = v_destruye, .invalidate = v_invalida, .get_scroll = v_da_desp,
  .set_scroll = v_pon_desp, .get_dimensions = v_medidas, .event = v_suceso,
  .set_title = v_titulo, .set_url = v_url, .set_status = v_estado, .set_pointer = v_puntero,
  .place_caret = v_cursor, .console_log = v_consola,
};

/* =============================================================================
 *  Los cuadros (alert, confirm, los avisos): otra ventana, en el centro
 * ============================================================================= */
static int cuadro(const char *t8, const char *titulo8, int con_cancelar)
{
  char lineas[12][200], tit[120];
  int nl = 0, w = 460, h, k, sobre_b = -1, r = -1;
  const char *p = t8;
  u32 v;
  nsfb_t *fb;
  struct suceso s;
  struct { int x, y, w, h; } bt[2];
  /* partir el texto en líneas que quepan */
  while (*p && nl < 12) {
    int n = 0, ult = -1, wx;
    while (p[n] && p[n] != '\n') {
      int m = utf8_next(p, strlen(p), n);
      wx = ancho_de(p, m, 13, 0);
      if (wx > w - 40 && n > 0) break;
      if (p[n] == ' ') ult = n;
      n = m;
    }
    if (p[n] && p[n] != '\n' && ult > 0) n = ult;
    snprintf(lineas[nl++], sizeof lineas[0], "%.*s", n, p);
    p += n;
    if (*p == '\n' || *p == ' ') p++;
  }
  if (!nl) { strcpy(lineas[0], ""); nl = 1; }
  h = 28 + nl * 20 + 58;
  frontal_utf8_a_437(titulo8, strlen(titulo8), tit, sizeof tit);
  v = sc(SC_VENTANA, (u32)tit, w, h);
  if (ES_ERROR(v)) return 1;
  sc(SC_COLOCAR, v, 0xFFFFFFFFu, 0);
  fb = nuevo_fb(NULL, w, h);
  if (!fb) { sc(SC_CERRAR_VENTANA, v, 0, 0); return 1; }
  bt[0].w = 100; bt[0].h = 30; bt[0].y = h - 44;
  bt[1] = bt[0];
  if (con_cancelar) { bt[0].x = w - 20 - 100 - 10 - 100; bt[1].x = w - 20 - 100; }
  else bt[0].x = w - 20 - 100;
  for (;;) {
    Pl pl;
    fb_entero(fb);
    pl = pl_de(fb);
    caja(&pl, 0, 0, w, h, 0xFFFFFF);
    caja(&pl, 0, h - 60, w, 60, C_ESTADO);
    caja(&pl, 0, h - 60, w, 1, 0xDCE5F2);
    for (k = 0; k < nl; k++) texto(fb, 20, 18 + k * 20, 20, lineas[k], -1, 13, 0, C_TINTA);
    for (k = 0; k < 1 + con_cancelar; k++) {
      const char *et = k == 0 ? "Aceptar" : "Cancelar";
      u32 c = k == 0 ? (sobre_b == 0 ? 0x2563B8 : 0x2F6FD0) : (sobre_b == 1 ? 0xC8D9F0 : 0xDCE7F6);
      redondo(&pl, bt[k].x, bt[k].y, bt[k].w, bt[k].h, 8, c, 1, 1);
      texto(fb, bt[k].x + (bt[k].w - ancho_de(et, -1, 13, 1)) / 2, bt[k].y, bt[k].h, et, -1, 13, 1, k == 0 ? 0xFFFFFF : C_TINTA);
    }
    sube_a(v, fb, 0, 0, w, h, 0, 0);
    if (r >= 0) break;
    if (sc(SC_SUCESO, v, (u32)&s, 1) != 1) { r = 0; break; }
    switch (s.clase) {
    case EV_CERRAR: r = con_cancelar ? 2 : 1; break;
    case EV_TECLA:
      if (s.a == '\n' || s.a == '\r' || s.a == ' ') r = 1;
      else if (s.a == 27) r = con_cancelar ? 2 : 1;
      break;
    case EV_RATON_MUEVE: {
      int nuevo = -1;
      for (k = 0; k < 1 + con_cancelar; k++) if (dentro((int)s.a, (int)s.b, bt[k].x, bt[k].y, bt[k].w, bt[k].h)) nuevo = k;
      if (nuevo == sobre_b) continue;
      sobre_b = nuevo;
      break;
    }
    case EV_RATON_SUBE:
      for (k = 0; k < 1 + con_cancelar; k++) if (dentro((int)s.a, (int)s.b, bt[k].x, bt[k].y, bt[k].w, bt[k].h)) r = k + 1;
      break;
    }
  }
  sc(SC_CERRAR_VENTANA, v, 0, 0);
  nsfb_free(fb);
  ensucia_todo();
  return r;
}
/* MessageBoxA (los textos, en la 1252): 1 Aceptar, 2 Cancelar */
int frontal_aviso(const char *texto1252, const char *titulo1252, uint32_t tipo)
{
  char t[1200], ti[200];
  de_1252(texto1252 ? texto1252 : "", t, sizeof t);
  de_1252(titulo1252 ? titulo1252 : "Navegador", ti, sizeof ti);
  pent_serie("navega: cuadro: %s", t);
  return cuadro(t, ti, (tipo & 15) == 1);
}
static nserror m_avisa(const char *a, const char *b)
{
  char t[700];
  snprintf(t, sizeof t, "%s%s%s", a ? a : "", b ? "\n" : "", b ? b : "");
  cuadro(t, "Navegador", 0);
  return NSERROR_OK;
}
static void m_fin(void) { }
static struct gui_misc_table tabla_varios = { .schedule = framebuffer_schedule, .quit = m_fin };

/* el portapapeles del sistema (la llamada 40: un texto en la página 437) */
static void pp_da(char **b, size_t *n)
{
  u32 m = sc(SC_PORTAPAPELES, 2, 0, 0);
  char *t;
  *b = NULL; *n = 0;
  if (ES_ERROR(m) || !m) return;
  t = malloc(m + 1);
  if (!t) return;
  m = sc(SC_PORTAPAPELES, 1, (u32)t, m);
  if (!ES_ERROR(m) && (*b = malloc(m * 3 + 1))) *n = frontal_437_a_utf8(t, m, *b, m * 3 + 1);
  free(t);
}
static void pp_pon(const char *b, size_t n, nsclipboard_styles s[], int ns)
{
  char *t = malloc(n + 1);
  (void)s; (void)ns;
  if (!t) return;
  n = frontal_utf8_a_437(b, n, t, n + 1);
  sc(SC_PORTAPAPELES, 0, (u32)t, n);
  free(t);
}
static struct gui_clipboard_table tabla_papel = { .get = pp_da, .set = pp_pon };

/* =============================================================================
 *  Ir a una dirección
 * ============================================================================= */
static void ir_en(struct gui_window *g, const char *u8)
{
  nsurl *u;
  nserror e;
  if (!g) return;
  e = nsurl_create(u8, &u);
  if (e != NSERROR_OK) {
    char b[1100];
    snprintf(b, sizeof b, "http://%s", u8);
    e = nsurl_create(b, &u);
  }
  if (e != NSERROR_OK) { m_avisa("La direcci\xc3\xb3n no vale.", u8); return; }
  pent_serie("navega: ir a %s", nsurl_access(u));
  browser_window_navigate(g->bw, u, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
  nsurl_unref(u);
}
static void ir(const char *u8) { ir_en(actual, u8); }
/* lo escrito en la dirección: una dirección, una ruta del disco, o se busca */
static void ir_dir(void)
{
  char b[4200], c[4400];
  char *q = b;
  dir_utf8(b, sizeof b);
  while (*q == ' ') q++;
  if (!*q) return;
  foco_dir = 0;
  if (q[1] == ':' && (q[2] == '\\' || q[2] == '/')) {      /* C:\CARPETA\PAGINA.HTM */
    nsurl *u;
    if (netsurf_path_to_nsurl(q, &u) == NSERROR_OK) {
      browser_window_navigate(actual->bw, u, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
      nsurl_unref(u);
      repinta_barras();
      return;
    }
  }
  if (!strstr(q, "://") && strncasecmp(q, "about:", 6) && strncasecmp(q, "resource:", 9) && strncasecmp(q, "data:", 5) &&
      strncasecmp(q, "file:", 5)) {
    /* sin espacios y con un punto (o «localhost»): una dirección; si no, se busca */
    if (!strchr(q, ' ') && (strchr(q, '.') || !strncasecmp(q, "localhost", 9))) snprintf(c, sizeof c, "http://%s", q);
    else {
      int l = snprintf(c, sizeof c, "https://www.google.es/search?q="), k;
      for (k = 0; q[k] && l < (int)sizeof c - 4; k++) {
        u8 ch = (u8)q[k];
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '.' || ch == '_') c[l++] = (char)ch;
        else if (ch == ' ') c[l++] = '+';
        else l += snprintf(c + l, sizeof c - l, "%%%02X", ch);
      }
      c[l] = 0;
    }
    q = c;
  }
  ir(q);
  repinta_barras();
}
static void nueva_pestana(const char *u8, int delante)
{
  nsurl *u = NULL;
  struct browser_window *bw;
  if (n_pest == N_PESTANAS) { frontal_aviso_estado("No caben m\xc3\xa1s pesta\xc3\xb1" "as."); return; }
  if (nsurl_create(u8, &u) != NSERROR_OK) return;
  if (browser_window_create(BW_CREATE_HISTORY | BW_CREATE_TAB | (delante ? BW_CREATE_FOREGROUND : 0), u, NULL,
                            actual ? actual->bw : NULL, &bw) != NSERROR_OK) { nsurl_unref(u); return; }
  nsurl_unref(u);
  if (delante) a_pestana(pest[n_pest - 1]);
  else repinta_barras();
}
static void cierra_pestana(int k)
{
  if (k < 0 || k >= n_pest) return;
  browser_window_destroy(pest[k]->bw);     /* (llama a v_destruye) */
}

/* =============================================================================
 *  El ratón y el teclado
 * ============================================================================= */
static int mods_raton;         /* las teclas de cambio del último suceso del ratón */
static browser_mouse_state mods(void)
{
  return (mods_raton & CON_MAYUS ? BROWSER_MOUSE_MOD_1 : 0) | (mods_raton & CON_CONTROL ? BROWSER_MOUSE_MOD_2 : 0);
}
static void boton(int k)
{
  if (!actual) return;
  switch (k) {
  case B_ATRAS: if (browser_window_back_available(actual->bw)) browser_window_history_back(actual->bw, false); break;
  case B_ADELANTE: if (browser_window_forward_available(actual->bw)) browser_window_history_forward(actual->bw, false); break;
  case B_RECARGA: if (cargando()) browser_window_stop(actual->bw); else browser_window_reload(actual->bw, true); break;
  case B_INICIO: ir(inicio); break;
  }
  repinta_barras();
}
static void foco_a_dir(int todo)
{
  foco_dir = 1; dir_sel = todo; dir_cur = n_dir;
  pinta_barra();
}
/* ---- la versión para móvil o la de escritorio, por sitio (navega 0.2.2) ----
 * Como «Ver versión de escritorio» de los navegadores de los teléfonos, al
 * revés: el Navegador es de escritorio y, para un sitio, se puede pedir la
 * versión para móvil (el User-Agent con «Mobile», lo que miran las webs que
 * deciden por él) y volver a la de escritorio. Se recuerda por servidor en
 * NAVEGA2.INI («Movil=servidor», una línea cada uno: es el archivo de lo que
 * guarda el Navegador por su cuenta; NAVEGA.INI es el de las opciones). Las
 * webs que recuerdan lo elegido en su propia galleta (ClicGest: cg_vista)
 * mandan con ella: allí se vuelve con su enlace «Ver versión de ordenador». */
#define AGENTE_ESCRITORIO "HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium)"   /* (el de siempre: SURF_UA en navega.mk) */
extern const char *(*http_agente)(const char *servidor);          /* (w32/navega/http.c) */
#define AGENTE_MOVIL "HispaSurf/0.2 (NetSurf 3.11; Hispa-Pentium; Mobile)"
#define N_MOVIL 32
static char movil[N_MOVIL][128];
static int n_movil;
static int es_movil(const char *servidor)
{
  int k;
  if (!servidor || !*servidor) return 0;
  for (k = 0; k < n_movil; k++) if (!strcasecmp(movil[k], servidor)) return 1;
  return 0;
}
static const char *agente_de(const char *servidor)
{
  return es_movil(servidor) ? AGENTE_MOVIL : AGENTE_ESCRITORIO;
}
static void movil_ruta(char *n, size_t cabe) { snprintf(n, cabe, "%s\\NAVEGA2.INI", surf_carpeta); }
static void movil_lee(void)
{
  char n[300], l[300];
  FILE *f;
  movil_ruta(n, sizeof n);
  n_movil = 0;
  if (!(f = fopen(n, "r"))) return;
  while (fgets(l, sizeof l, f)) {
    char *e = l + strlen(l);
    while (e > l && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) e--;
    *e = 0;
    if (!strncasecmp(l, "Movil=", 6) && l[6] && strlen(l + 6) < sizeof movil[0] && n_movil < N_MOVIL && !es_movil(l + 6))
      snprintf(movil[n_movil++], sizeof movil[0], "%s", l + 6);
  }
  fclose(f);
}
/* (guarda la lista; las otras líneas de NAVEGA2.INI, como estaban) */
static void movil_guarda(void)
{
  char n[300], l[300], *otras = NULL;
  size_t lo = 0;
  int k;
  FILE *f;
  movil_ruta(n, sizeof n);
  if ((f = fopen(n, "r"))) {
    while (fgets(l, sizeof l, f)) {
      size_t m = strlen(l);
      char *q;
      if (!strncasecmp(l, "Movil=", 6)) continue;
      if (!(q = realloc(otras, lo + m + 1))) break;
      otras = q; memcpy(otras + lo, l, m); lo += m; otras[lo] = 0;
    }
    fclose(f);
  }
  if ((f = fopen(n, "w"))) {
    if (otras) fwrite(otras, 1, lo, f);
    for (k = 0; k < n_movil; k++) fprintf(f, "Movil=%s\n", movil[k]);   /* (en modo texto: \r\n) */
    fclose(f);
  } else pent_serie("navega: no se puede guardar %s", n);
  free(otras);
}
/* el servidor de la página de ahora (en s, vacío si no es http ni https) */
static void servidor_actual(char *s, size_t cabe)
{
  nsurl *u = actual ? browser_window_access_url(actual->bw) : NULL;
  lwc_string *esq, *h;
  s[0] = 0;
  if (!u) return;
  esq = nsurl_get_component(u, NSURL_SCHEME);
  h = nsurl_get_component(u, NSURL_HOST);
  if (esq && h && (!strcasecmp(lwc_string_data(esq), "http") || !strcasecmp(lwc_string_data(esq), "https")))
    snprintf(s, cabe, "%.*s", (int)lwc_string_length(h), lwc_string_data(h));
  if (esq) lwc_string_unref(esq);
  if (h) lwc_string_unref(h);
}
/* el texto de la opción del menú (página 437) para el sitio de ahora */
static const char *texto_version(void)
{
  char s[128];
  servidor_actual(s, sizeof s);
  return es_movil(s) ? "Ver la versi\xa2n de escritorio" : "Ver la versi\xa2n para m\xa2vil";
}
static void cambia_version(void)
{
  char s[128];
  int k;
  servidor_actual(s, sizeof s);
  if (!s[0]) return;
  if (es_movil(s)) {
    for (k = 0; k < n_movil; k++)
      if (!strcasecmp(movil[k], s)) { memmove(movil[k], movil[k + 1], (size_t)(n_movil - k - 1) * sizeof movil[0]); n_movil--; break; }
    pent_serie("navega: %s, la versi\xa2n de escritorio", s);
  } else {
    if (n_movil == N_MOVIL) { memmove(movil[0], movil[1], (size_t)(N_MOVIL - 1) * sizeof movil[0]); n_movil--; }
    snprintf(movil[n_movil++], sizeof movil[0], "%s", s);
    pent_serie("navega: %s, la versi\xa2n para m\xa2vil", s);
  }
  movil_guarda();
  /* otra vez, sin lo guardado (la página de la otra versión) */
  browser_window_reload(actual->bw, true);
}

/* ---- los menús (llamada 39; los textos en la página 437: «á» es \xa0...) ---- */
enum { MENU_PRINCIPAL = 1, MENU_PAGINA = 2 };
static char menu_enlace[2048];  /* el enlace de debajo del ratón, para el menú de la página */
static void menu(int x, int y, const char *opciones, u32 grises, u32 etiqueta)
{
  struct pide_menu m;
  m.x = x; m.y = y; m.opciones = opciones; m.inactivas = grises; m.etiqueta = etiqueta;
  sc(SC_MENU, vent, (u32)&m, 0);
}
static void menu_principal(void)
{
  char op[400], s[128];
  servidor_actual(s, sizeof s);
  snprintf(op, sizeof op, "Nueva pesta\xa4" "a (Ctrl+T)|Cerrar la pesta\xa4" "a (Ctrl+W)|-|Copiar la direcci\xa2n|Ver las descargas|"
           "%s|-|P\xa0gina de inicio|Acerca del Navegador|-|Salir", texto_version());
  menu(b_r[B_MENU].x + b_r[B_MENU].w - 230, b_r[B_MENU].y + b_r[B_MENU].h, op, s[0] ? 0 : 1u << 5, MENU_PRINCIPAL);
}
static void copia_texto(const char *u8)
{
  pp_pon(u8, strlen(u8), NULL, 0);
}
static void acerca(void)
{
  char t[700];
  unsigned b, a, m;
  memoria_cuenta(&b, &a, &m);
  snprintf(t, sizeof t, "Navegador de Hispa-Pentium %s\n"
           "NetSurf 3.11 (GPL-2.0), QuickJS (MIT), BearSSL (MIT, en TLS.BIN), Brotli (MIT), letras DejaVu.\n"
           "Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.\n"
           "Memoria: %u KB en uso (lo m\xc3\xa1s, %u KB); %u KB pedidos al sistema.", VERSION_NAVEGADOR, a, m, (unsigned)pent_memoria_kb());
  cuadro(t, "Acerca del Navegador", 0);
}
static void elegido(u32 etiqueta, u32 op)
{
  if (op == SIN_ELEGIR || !actual) return;
  if (etiqueta == MENU_PRINCIPAL) {
    switch (op) {
    case 0: nueva_pestana(inicio, 1); break;
    case 1: cierra_pestana(indice(actual)); break;
    case 3: { nsurl *u = browser_window_access_url(actual->bw); if (u) copia_texto(nsurl_access(u)); break; }
    case 4: {
      char b[400];
      nsurl *u;
      CreateDirectoryA(frontal_descargas, NULL);
      snprintf(b, sizeof b, "%s\\", frontal_descargas);
      if (netsurf_path_to_nsurl(b, &u) == NSERROR_OK) { browser_window_navigate(actual->bw, u, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL); nsurl_unref(u); }
      break;
    }
    case 5: cambia_version(); break;
    case 7: ir(inicio); break;
    case 8: acerca(); break;
    case 10: salir = 1; break;
    }
  } else if (etiqueta == MENU_PAGINA) {
    int hay = menu_enlace[0] != 0;
    /* con enlace: Abrir en otra pestaña | Copiar la dirección del enlace | Descargar el enlace | - | ... */
    if (hay) {
      if (op == 0) { nueva_pestana(menu_enlace, 0); return; }
      if (op == 1) { copia_texto(menu_enlace); return; }
      if (op == 2) {
        nsurl *u;
        if (nsurl_create(menu_enlace, &u) == NSERROR_OK) {
          browser_window_navigate(actual->bw, u, browser_window_access_url(actual->bw), BW_NAVIGATE_DOWNLOAD, NULL, NULL, NULL);
          nsurl_unref(u);
        }
        return;
      }
      op -= 4;
    }
    switch (op) {
    case 0: boton(B_ATRAS); break;
    case 1: boton(B_ADELANTE); break;
    case 2: boton(B_RECARGA); break;
    case 4: browser_window_key_press(actual->bw, NS_KEY_COPY_SELECTION); break;
    case 5: browser_window_key_press(actual->bw, NS_KEY_PASTE); break;
    case 6: browser_window_key_press(actual->bw, NS_KEY_SELECT_ALL); break;
    case 8: cambia_version(); break;
    }
  }
}
static void menu_pagina(int x, int y)
{
  struct browser_window_features f;
  char op[600];
  u32 grises = 0;
  int base = 0;
  memset(&f, 0, sizeof f);
  menu_enlace[0] = 0;
  if (browser_window_get_features(actual->bw, x - 0 + actual->sx, y - ARRIBA_H + actual->sy, &f) == NSERROR_OK && f.link)
    snprintf(menu_enlace, sizeof menu_enlace, "%s", nsurl_access(f.link));
  op[0] = 0;
  if (menu_enlace[0]) {
    strcat(op, "Abrir el enlace en otra pesta\xa4" "a|Copiar la direcci\xa2n del enlace|Descargar el enlace|-|");
    base = 4;
  }
  strcat(op, "Atr\xa0s|Adelante|Recargar|-|Copiar|Pegar|Seleccionar todo|-|");
  strcat(op, texto_version());
  {
    char s[128];
    servidor_actual(s, sizeof s);
    if (!s[0]) grises |= 1u << (base + 8);
  }
  if (!browser_window_back_available(actual->bw)) grises |= 1u << base;
  if (!browser_window_forward_available(actual->bw)) grises |= 1u << (base + 1);
  menu(x, y, op, grises, MENU_PAGINA);
}

static void clic(int x, int y, u32 c)
{
  int k;
  if (y < PEST_H) {
    for (k = 0; k < n_pest; k++) {
      if (dentro(x, y, cierra_r[k].x, cierra_r[k].y, cierra_r[k].w, cierra_r[k].h)) { cierra_pestana(k); return; }
      if (dentro(x, y, pest_r[k].x, pest_r[k].y, pest_r[k].w, pest_r[k].h)) { if (pest[k] != actual) a_pestana(pest[k]); return; }
    }
    if (n_pest < N_PESTANAS && dentro(x, y, mas_r.x, mas_r.y, mas_r.w, mas_r.h)) nueva_pestana(inicio, 1);
    return;
  }
  if (y < ARRIBA_H) {
    for (k = 0; k < B_N; k++)
      if (dentro(x, y, b_r[k].x, b_r[k].y, b_r[k].w, b_r[k].h)) {
        foco_dir = 0;
        if (k == B_MENU) { pinta_barra(); menu_principal(); }
        else boton(k);
        return;
      }
    if (dentro(x, y, dir_r.x, dir_r.y, dir_r.w, dir_r.h)) {
      int tx = dir_r.x + 14, i;
      if (!foco_dir) { foco_a_dir(1); return; }
      if ((c >> 16) >= 2) { dir_sel = 1; pinta_barra(); return; }   /* (dos clics: todo) */
      dir_sel = 0;
      for (i = dir_desp; i < n_dir && tx + dir_ancho(dir_desp, i - dir_desp + 1) - dir_ancho(i, 1) / 2 < x; i++) ;
      dir_cur = i;
      pinta_barra();
    }
    return;
  }
  if (foco_dir) { foco_dir = 0; pinta_barra(); }
  if (y >= al - ESTADO_H) return;
  if (x >= pw) {                     /* la barra de desplazamiento */
    int py, h;
    pulgar(&py, &h);
    y -= ARRIBA_H;
    if (y < py) desplaza(0, -(ph - 40));
    else if (y >= py + h) desplaza(0, ph - 40);
    else { arrastra = 1; arr_y0 = y; arr_sy0 = actual ? actual->sy : 0; pinta_desp(); }
    return;
  }
  if (actual) {
    boton_dentro = 1;
    browser_window_mouse_click(actual->bw, BROWSER_MOUSE_PRESS_1 | mods(), x + actual->sx, y - ARRIBA_H + actual->sy);
  }
}
static void suelta(int x, int y)
{
  if (arrastra >= 0) { arrastra = -1; pinta_desp(); return; }
  if (boton_dentro && actual) {
    boton_dentro = 0;
    browser_window_mouse_click(actual->bw, BROWSER_MOUSE_CLICK_1 | mods(), x + actual->sx, y - ARRIBA_H + actual->sy);
  }
}
static int que_hay(int x, int y)
{
  int k;
  if (y < PEST_H) {
    for (k = 0; k < n_pest; k++) {
      if (dentro(x, y, cierra_r[k].x, cierra_r[k].y, cierra_r[k].w, cierra_r[k].h)) return 200 + k;
      if (dentro(x, y, pest_r[k].x, pest_r[k].y, pest_r[k].w, pest_r[k].h)) return S_PEST + k;
    }
    if (dentro(x, y, mas_r.x, mas_r.y, mas_r.w, mas_r.h)) return 300;
    return -1;
  }
  if (y < ARRIBA_H) for (k = 0; k < B_N; k++) if (dentro(x, y, b_r[k].x, b_r[k].y, b_r[k].w, b_r[k].h)) return k;
  return -1;
}
static void mueve(int x, int y, u32 botones)
{
  int s = que_hay(x, y), sd = x >= pw && y >= ARRIBA_H && y < al - ESTADO_H;
  if (s != sobre) { sobre = s; pinta_barra(); }
  if (sd != sobre_desp) { sobre_desp = sd; pinta_desp(); }
  if (arrastra >= 0 && actual) {
    int py, h, tot = actual->ext_h;
    pulgar(&py, &h);
    if (ph - 4 > h && tot > ph) desplaza_a(actual->sx, arr_sy0 + (int)((int64_t)(y - ARRIBA_H - arr_y0) * (tot - ph) / (ph - 4 - h)));
    return;
  }
  if (!actual) return;
  if ((y >= ARRIBA_H && y < ARRIBA_H + ph && x < pw) || boton_dentro)
    browser_window_mouse_track(actual->bw, ((botones & BOTON_IZQUIERDO) ? BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON : 0) | mods(),
                               x + actual->sx, y - ARRIBA_H + actual->sy);
  else surf_estado("");
}
static void rueda(int pasos, int x, int y, u32 m)
{
  int dy = -pasos * 60;
  if (!actual) return;
  if (m & CON_MAYUS) { desplaza(dy, 0); return; }
  if (x < pw && y >= ARRIBA_H && y < ARRIBA_H + ph &&
      browser_window_scroll_at_point(actual->bw, x + actual->sx, y - ARRIBA_H + actual->sy, 0, dy)) return;
  desplaza(0, dy);
}

/* una tecla en el campo de la dirección (u: Unicode, o una especial) */
static void dir_quita_sel(void) { if (dir_sel) { n_dir = dir_cur = 0; dir_sel = 0; } }
static void dir_pega(void)
{
  char *b;
  size_t n, k = 0;
  pp_da(&b, &n);
  if (!b) return;
  dir_quita_sel();
  while (k < n && n_dir < (int)(sizeof dir / sizeof dir[0]) - 1) {
    u32 c = utf8_to_ucs4(b + k, n - k);
    k = utf8_next(b, n, k);
    if (c < ' ') continue;
    memmove(dir + dir_cur + 1, dir + dir_cur, (n_dir - dir_cur) * sizeof dir[0]);
    dir[dir_cur++] = c; n_dir++;
  }
  free(b);
}
static int tecla_dir(u32 k, u32 u)
{
  if (u) {
    if (u == '\n' || u == '\r') { ir_dir(); return 1; }
    if (u == 27) { foco_dir = 0; if (actual) { nsurl *x = browser_window_access_url(actual->bw); dir_de(x ? nsurl_access(x) : ""); } pinta_barra(); return 1; }
    if (u == 1) { dir_sel = 1; }
    else if (u == 3) { char b[4200]; dir_utf8(b, sizeof b); copia_texto(b); return 1; }
    else if (u == 24) { char b[4200]; dir_utf8(b, sizeof b); copia_texto(b); n_dir = dir_cur = 0; dir_sel = 0; }
    else if (u == 22) dir_pega();
    else if (u == 8) {
      if (dir_sel) dir_quita_sel();
      else if (dir_cur > 0) { memmove(dir + dir_cur - 1, dir + dir_cur, (n_dir - dir_cur) * sizeof dir[0]); n_dir--; dir_cur--; }
    } else if (u >= 32 && n_dir < (int)(sizeof dir / sizeof dir[0]) - 1) {
      dir_quita_sel();
      memmove(dir + dir_cur + 1, dir + dir_cur, (n_dir - dir_cur) * sizeof dir[0]);
      dir[dir_cur++] = u; n_dir++;
    } else return 0;
    pinta_barra();
    return 1;
  }
  switch (k) {
  case T_IZQUIERDA: if (dir_cur > 0) dir_cur--; dir_sel = 0; break;
  case T_DERECHA: if (dir_cur < n_dir) dir_cur++; dir_sel = 0; break;
  case T_INICIO: dir_cur = 0; dir_sel = 0; break;
  case T_FIN: dir_cur = n_dir; dir_sel = 0; break;
  case T_SUPR:
    if (dir_sel) dir_quita_sel();
    else if (dir_cur < n_dir) { memmove(dir + dir_cur, dir + dir_cur + 1, (n_dir - dir_cur - 1) * sizeof dir[0]); n_dir--; }
    break;
  default: return 0;
  }
  pinta_barra();
  return 1;
}
/* una tecla del núcleo: k (letra de la 437, o 0x100 + código), con Mayús (b) */
static void tecla(u32 k, u32 con_mayus)
{
  uint32_t ns = 0, u = 0;
  if (k == T_F6 || k == 12) { foco_a_dir(1); return; }            /* F6, Ctrl+L */
  if (k == T_F1 && registro && actual && browser_window_get_content(actual->bw) && content_get_type(browser_window_get_content(actual->bw)) == CONTENT_HTML) {
    /* (con SURF.LOG: F1 deja el árbol de cajas de la página en CAJAS.TXT, para ver qué pasa) */
    char n[300];
    FILE *f;
    struct box *b = html_get_box_tree(browser_window_get_content(actual->bw));
    snprintf(n, sizeof n, "%s\\CAJAS.TXT", surf_carpeta);
    if (b && (f = fopen(n, "w"))) {
      html_content *hc = (html_content *)hlcache_handle_get_content(browser_window_get_content(actual->bw));
      dom_node *raiz = NULL;
      box_dump(f, b, 0, false);
      /* y el árbol de libdom (los elementos, con su id y su clase, y si tienen caja) */
      fprintf(f, "== DOM ==\n");
      if (hc && dom_document_get_document_element(hc->document, &raiz) == DOM_NO_ERR && raiz) {
        dom_node *pila[64];
        int hondo[64], np = 0;
        pila[np] = raiz; hondo[np++] = 0;
        while (np) {
          dom_node *x = pila[--np];
          int h = hondo[np];
          NodoI *c;
          if (NI(x)->tipo == DOM_ELEMENT_NODE) {
            dom_string *id = sel_atr(x, "id"), *cl = sel_atr(x, "class");
            void *caja = NULL;
            dom_node_get_user_data(x, corestring_dom___ns_key_box_node_data, &caja);
            fprintf(f, "%*s<%.*s id=%.40s class=%.80s>%s\n", h * 2, "", (int)dom_string_byte_length(NI(x)->nombre), dom_string_data(NI(x)->nombre),
                    id ? dom_string_data(id) : "", cl ? dom_string_data(cl) : "", caja ? "" : " (sin caja)");
            if (id) dom_string_unref(id);
            if (cl) dom_string_unref(cl);
            if (h < 5) {
              int k0 = np;
              for (c = NI(x)->hijo; c && np < 63; c = c->sig) if (c->tipo == DOM_ELEMENT_NODE) { pila[np] = (dom_node *)c; hondo[np++] = h + 1; }
              /* (al revés, para sacarlos en orden) */
              { int i = k0, j = np - 1; while (i < j) { dom_node *t = pila[i]; pila[i] = pila[j]; pila[j] = t; i++; j--; } }
            }
          }
        }
        dom_node_unref(raiz);
      }
      fclose(f);
      frontal_aviso_estado("El \xc3\xa1rbol de cajas, en CAJAS.TXT");
    }
    return;
  }
  if (k == T_F5 || k == 18) { boton(B_RECARGA); return; }         /* F5, Ctrl+R */
  if (k == 20) { nueva_pestana(inicio, 1); return; }               /* Ctrl+T */
  if (k == 23) { cierra_pestana(indice(actual)); return; }         /* Ctrl+W */
  if ((k == T_CTRL_TAB || k == T_CTRL_MAYUS_TAB) && n_pest > 1) {    /* Ctrl+Tab, Ctrl+Mayús+Tab (con el núcleo de controles-erp) */
    int i = indice(actual) + (k == T_CTRL_TAB ? 1 : n_pest - 1);
    a_pestana(pest[i % n_pest]);
    return;
  }
  if (k == 27 && cargando() && !foco_dir) { browser_window_stop(actual->bw); return; }
  if (k < 0x100) u = de437((u8)k);
  if (foco_dir) { tecla_dir(k, u); return; }
  if (!actual) return;
  if (k < 0x100) {
    if (u == '\n' || u == '\r') ns = NS_KEY_NL;
    else if (u == 8) ns = NS_KEY_DELETE_LEFT;
    else if (u == 9) ns = NS_KEY_TAB;
    else if (u == 27) ns = NS_KEY_ESCAPE;
    else if (u == 1 || u == 3 || u == 22 || u == 24) ns = u;     /* Ctrl+A, C, V, X */
    else if (u == 26) ns = NS_KEY_UNDO;
    else if (u == 25) ns = NS_KEY_REDO;
    else if (u < 32) return;
    else ns = u;
    if (browser_window_key_press(actual->bw, ns)) return;
    if (u == ' ') desplaza(0, ph - 40);
    else if (ns == NS_KEY_DELETE_LEFT) boton(B_ATRAS);
    return;
  }
  switch (k) {
  case T_MAYUS_TAB: ns = NS_KEY_SHIFT_TAB; break;
  case T_IZQUIERDA: ns = NS_KEY_LEFT; break;
  case T_DERECHA: ns = NS_KEY_RIGHT; break;
  case T_ARRIBA: ns = NS_KEY_UP; break;
  case T_ABAJO: ns = NS_KEY_DOWN; break;
  case T_RE_PAG: ns = NS_KEY_PAGE_UP; break;
  case T_AV_PAG: ns = NS_KEY_PAGE_DOWN; break;
  case T_INICIO: ns = NS_KEY_LINE_START; break;
  case T_FIN: ns = NS_KEY_LINE_END; break;
  case T_SUPR: ns = NS_KEY_DELETE_RIGHT; break;
  default: return;
  }
  (void)con_mayus;
  if (browser_window_key_press(actual->bw, ns)) return;
  /* si no era para un campo: mover la página */
  switch (k) {
  case T_ARRIBA: desplaza(0, -40); break;
  case T_ABAJO: desplaza(0, 40); break;
  case T_IZQUIERDA: desplaza(-40, 0); break;
  case T_DERECHA: desplaza(40, 0); break;
  case T_RE_PAG: desplaza(0, -(ph - 40)); break;
  case T_AV_PAG: desplaza(0, ph - 40); break;
  case T_INICIO: desplaza_a(0, 0); break;
  case T_FIN: desplaza_a(0, actual->ext_h); break;
  }
}

static void cambia_tamano(int w, int h)
{
  int k;
  if (w < 200) w = 200;
  if (h < 140) h = 140;
  if (w == an && h == al && lienzo) return;
  an = w; al = h;
  coloca();
  nuevos_buferes();
  for (k = 0; k < n_pest; k++) browser_window_schedule_reformat(pest[k]->bw);
  repinta_barras();
}
static void atiende(struct suceso *s)
{
  int x = (int)s->a, y = (int)s->b;
  switch (s->clase) {
  case EV_CERRAR: salir = 1; break;
  case EV_TAMANO: cambia_tamano(x, y); ensucia_todo(); break;
  case EV_TECLA: tecla(s->a, s->b); break;
  case EV_RATON_BAJA:
    mods_raton = s->c & (CON_MAYUS | CON_CONTROL);
    if (s->c & BOTON_DERECHO) {
      if (y >= ARRIBA_H && y < ARRIBA_H + ph && x < pw && actual) menu_pagina(x, y);
      return;
    }
    if (s->c & BOTON_IZQUIERDO) clic(x, y, s->c);
    break;
  case EV_RATON_SUBE: mods_raton = s->c & (CON_MAYUS | CON_CONTROL); suelta(x, y); break;
  case EV_RATON_MUEVE: mueve(x, y, s->c); break;
  case EV_SALE: if (sobre >= 0 || sobre_desp) { sobre = -1; sobre_desp = 0; pinta_barra(); pinta_desp(); } break;
  case EV_RUEDA: surf_reg("rueda %d", (int8_t)(s->c & 0xFF)); rueda((int8_t)(s->c & 0xFF), x, y, s->c & (CON_MAYUS | CON_CONTROL)); break;
  case EV_MENU: elegido(s->b, s->a); break;
  }
}

/* =============================================================================
 *  NAVEGA.INI, las opciones de NetSurf y el arranque
 * ============================================================================= */
static nserror opciones(struct nsoption_s *d)
{
  (void)d;
  nsoption_set_bool(core_select_menu, true);
  nsoption_set_bool(enable_javascript, true);     /* [JS] guion.c (QuickJS) */
  nsoption_set_int(memory_cache_size, 4 * 1024 * 1024);
  nsoption_set_int(max_fetchers, 8);
  nsoption_set_int(max_fetchers_per_host, 4);
  nsoption_set_int(max_cached_fetch_handles, 2);
  nsoption_set_int(font_size, 120);
  nsoption_set_int(font_min_size, 80);
  nsoption_set_bool(animate_images, false);
  return NSERROR_OK;
}
static void lee_ini(void)
{
  char n[300], l[500];
  FILE *f;
  snprintf(n, sizeof n, "%s\\NAVEGA.INI", surf_carpeta);
  f = fopen(n, "r");
  if (!f) return;
  while (fgets(l, sizeof l, f)) {
    char *e = l + strlen(l), *c;
    if ((c = strchr(l, ';'))) e = c;       /* (lo de detrás de «;», un comentario) */
    while (e > l && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) e--;
    *e = 0;
    if (!strncasecmp(l, "Inicio=", 7) && l[7]) snprintf(inicio, sizeof inicio, "%s", l + 7);
    if (!strncasecmp(l, "Idioma=", 7) && l[7]) snprintf(idioma, sizeof idioma, "%s", l + 7);
    if (!strncasecmp(l, "Descargas=", 10) && l[10]) snprintf(frontal_descargas, sizeof frontal_descargas, "%s", l + 10);
  }
  fclose(f);
}
static void muere(const char *t)
{
  pent_serie("navega: %s", t);
  if (vent) cuadro(t, "Navegador", 0);
  else {
    /* sin ventana aún: una pequeña para decirlo */
    cuadro(t, "Navegador", 0);
  }
  sc(SC_ACABAR, 1, 0, 0);
}

int navega_principal(void)
{
  const char *cmd;
  char b[600], *argv[4];
  static char regi[300], regns[300];
  int w = (int)sc(SC_DATO, DATO_ANCHO_PANTALLA, 0, 0), h = (int)sc(SC_DATO, DATO_ALTO_PANTALLA, 0, 0), argc = 1;
  struct netsurf_table tabla = {
    .misc = &tabla_varios, .window = &tabla_ventana, .clipboard = &tabla_papel,
  };
  nserror e;
  char *p;
  struct suceso s;

  msvcrt_arranca(NULL, 1, NULL);
  hspjs_arranca(NULL, 1, NULL);
  cmd = GetCommandLineA();
  tabla.file = surf_tabla_archivos;
  tabla.fetch = surf_tabla_pedir;
  tabla.utf8 = surf_tabla_utf8;
  tabla.bitmap = surf_tabla_mapas;
  tabla.layout = surf_tabla_letras;
  tabla.download = frontal_tabla_descargas;
  GetModuleFileNameA(NULL, surf_carpeta, sizeof surf_carpeta);
  p = strrchr(surf_carpeta, '\\');
  if (p) *p = 0; else strcpy(surf_carpeta, ".");
  snprintf(frontal_descargas, sizeof frontal_descargas, "%c:\\DESCARGA", surf_carpeta[0]);
  lee_ini();
  movil_lee();
  http_agente = agente_de;
  pent_serie("navega: Navegador %s, en %s", VERSION_NAVEGADOR, surf_carpeta);
  if (netsurf_register(&tabla) != NSERROR_OK) muere("No se ha podido preparar NetSurf.");
  /* el registro (para ver qué pasa): con SURF.LOG junto al programa */
  argv[0] = "navega";
  snprintf(regi, sizeof regi, "%s\\SURF.LOG", surf_carpeta);
  {
    FILE *f = fopen(regi, "r");
    if (f) { fclose(f); registro = regi; argv[1] = "-V"; snprintf(regns, sizeof regns, "%s\\NS.LOG", surf_carpeta); argv[2] = regns; argc = 3; }
  }
  argv[argc] = NULL;
  nslog_init(NULL, &argc, argv);
  if (nsoption_init(opciones, &nsoptions, &nsoptions_default) != NSERROR_OK) muere("Faltan las opciones.");
  nsoption_set_charp(accept_language, strdup(idioma));
  snprintf(b, sizeof b, "%s\\RECURSOS\\MESSAGES", surf_carpeta);
  messages_add_from_file(b);
  snprintf(b, sizeof b, "%s\\FUENTES", surf_carpeta);
  if (!letra_inicia(b)) muere("No se encuentran las letras (FUENTES\\SANS.TTF junto al programa).");
  browser_set_dpi(96);
  e = netsurf_init(NULL);
  if (e != NSERROR_OK) {
    snprintf(b, sizeof b, "NetSurf no ha podido empezar: %s", messages_get_errorcode(e));
    muere(b);
  }
  rh_inicia(idioma);
  snprintf(b, sizeof b, "%s\\RECURSOS", surf_carpeta);
  woff_carpeta(b);
  snprintf(galletas, sizeof galletas, "%s\\SURFGAL.TXT", surf_carpeta);
  rh_galletas_carga(galletas);
  guion_memoria_kb = 32768;      /* (en un Pentium hay más memoria que en un 486) */

  /* la ventana: casi toda la pantalla, en el centro */
  if (w <= 0 || ES_ERROR((u32)w)) w = 800;
  if (h <= 0 || ES_ERROR((u32)h)) h = 600;
  an = w - 40; al = h - 90;
  if (an > 1600) an = 1600;
  if (al > 1100) al = 1100;
  if (an < 400) an = 400;
  if (al < 300) al = 300;
  vent = sc(SC_VENTANA, (u32)"Navegador", an, al);
  if (ES_ERROR(vent)) { vent = 0; muere("No se puede abrir la ventana (\xc2\xbfest\xc3\xa1 en marcha el escritorio?)."); }
  sc(SC_COLOCAR, vent, 0xFFFFFFFFu, 0);
  coloca();
  nuevos_buferes();
  repinta_barras();

  /* la dirección de los argumentos, si la hay; si no, el inicio */
  while (*cmd && *cmd != ' ') cmd++;
  while (*cmd == ' ') cmd++;
  {
    nsurl *u = NULL;
    struct browser_window *bw;
    char d[1100];
    if (*cmd) {
      if (cmd[1] == ':') netsurf_path_to_nsurl(cmd, &u);
      else {
        frontal_437_a_utf8(cmd, strlen(cmd), d, sizeof d);
        if (nsurl_create(d, &u) != NSERROR_OK) { snprintf(b, sizeof b, "http://%s", d); nsurl_create(b, &u); }
      }
    } else nsurl_create(inicio, &u);
    if (!u || browser_window_create(BW_CREATE_HISTORY, u, NULL, NULL, &bw) != NSERROR_OK)
      muere("No se puede abrir la primera p\xc3\xa1gina.");
    pent_serie("navega: primera %s", nsurl_access(u));
    nsurl_unref(u);
  }

  while (!salir) {
    int t, k;
    while (!salir && sc(SC_SUCESO, vent, (u32)&s, 0) == 1) atiende(&s);
    if (salir) break;
    t = schedule_run();
    pinta_sucio();
    if (salir) break;
    if (t == 0) continue;
    if (t < 0 && !red_ocupada()) {
      /* nada que hacer: esperar a la persona */
      if (sc(SC_SUCESO, vent, (u32)&s, 1) == 1) atiende(&s);
      continue;
    }
    if (t < 0 || t > 200) t = 200;
    /* esperar lo que dice el planificador, tic a tic, mirando los sucesos */
    for (k = (int)(((uint64_t)t * pent_hz() + 999) / 1000); k > 0 && !salir; k--) {
      sc(SC_DORMIR, 1, 0, 0);
      if (sc(SC_SUCESO, vent, (u32)&s, 0) == 1) { atiende(&s); break; }
    }
  }
  rh_galletas_guarda(galletas);
  red_fin();
  pent_serie("navega: adi\xa2s");
  return 0;
}
