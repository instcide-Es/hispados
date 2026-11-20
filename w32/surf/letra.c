/* =============================================================================
 *  letra.c - las letras del Navegador completo (NAVEGA2.EXE): de ancho
 *  variable, de tipos TrueType libres (DejaVu, en \HISPADOS\INTERNET\FUENTES)
 *  pintadas con stb_truetype (terceros/stb, dominio público / MIT) con sus
 *  grises. Da a NetSurf la tabla de medir (gui_layout_table) y pinta el texto
 *  en el búfer de la página (libnsfb). docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/utf8.h"
#include "utils/nsoption.h"
#include "netsurf/layout.h"
#include "netsurf/plot_style.h"
#include "netsurf/utf8.h"
#include "utils/errors.h"

#include "surf.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_assert(x) ((void)0)
#include "stb_truetype.h"

/* los tipos: el nombre del archivo (8+3) en la carpeta de las letras */
enum { T_SANS, T_SANS_N, T_SANS_C, T_SANS_NC, T_SERIF, T_SERIF_N, T_MONO, T_MONO_N, T_N };
static const char *const t_archivo[T_N] = {
  "SANS.TTF", "SANSN.TTF", "SANSC.TTF", "SANSNC.TTF", "SERIF.TTF", "SERIFN.TTF", "MONO.TTF", "MONON.TTF" };
/* después de las nuestras, las letras de la web (@font-face: webletra.c),
 * hasta WEB_N y WEB_MAX octetos entre todas */
#define WEB_N 16
#define WEB_MAX (2560u << 10)
#define T_TOT (T_N + WEB_N)
#define NEGRITA 0x80          /* (en el número de tipo: negrita hecha aquí) */
static struct {
  unsigned char *datos; stbtt_fontinfo f; int bien; int asc, desc, salto;
  char familia[48]; int peso, cursiva, peso_real, propio; size_t tam;
} tipo[T_TOT];
static uint32_t web_total;
static char carpeta[260];

/* la memoria de las letras ya pintadas: (tipo, tamaño, letra) -> su dibujo */
typedef struct {
  uint32_t letra;
  uint16_t px;
  uint8_t t, usada;
  int avance;                 /* en 1/65536 de punto */
  short x0, y0, w, h;         /* respecto a la línea de base */
  unsigned char *gris;        /* w x h, 0..255 */
} Glifo;
#define GLIFOS 1024
static Glifo glifo[GLIFOS];
static uint32_t gris_total;   /* lo que ocupan los dibujos (octetos) */
#define GRIS_MAX (256u << 10)

static unsigned char *lee(const char *n, uint32_t *tam) {
  FILE *f = fopen(n, "rb");
  unsigned char *d;
  long l;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); l = ftell(f); fseek(f, 0, SEEK_SET);
  d = l > 0 ? malloc(l) : NULL;
  if (d && fread(d, 1, l, f) != (size_t)l) { free(d); d = NULL; }
  fclose(f);
  if (tam) *tam = l;
  return d;
}

bool letra_inicia(const char *dir) {
  char n[300];
  int k, alguno = 0;
  snprintf(carpeta, sizeof carpeta, "%s", dir);
  for (k = 0; k < T_N; k++) {
    snprintf(n, sizeof n, "%s" SURF_SEP "%s", carpeta, t_archivo[k]);
    tipo[k].datos = lee(n, NULL);
    if (!tipo[k].datos) continue;
    if (!stbtt_InitFont(&tipo[k].f, tipo[k].datos, stbtt_GetFontOffsetForIndex(tipo[k].datos, 0))) {
      free(tipo[k].datos); tipo[k].datos = NULL; continue;
    }
    stbtt_GetFontVMetrics(&tipo[k].f, &tipo[k].asc, &tipo[k].desc, &tipo[k].salto);
    tipo[k].bien = 1;
    alguno = 1;
  }
  return alguno;
}

void letra_fin(void) {
  int k;
  for (k = 0; k < GLIFOS; k++) free(glifo[k].gris);
  for (k = 0; k < T_TOT; k++) if (k < T_N || tipo[k].propio) free(tipo[k].datos);
  memset(glifo, 0, sizeof glifo);
  memset(tipo, 0, sizeof tipo);
}

/* el tipo para un estilo de NetSurf (si no está, el más parecido que haya) */
/* la letra de la web que pide el estilo (por su familia), o -1 */
static int tipo_web(const plot_font_style_t *s) {
  int cursiva = (s->flags & (FONTF_ITALIC | FONTF_OBLIQUE)) != 0, k, mejor = -1, d_mejor = 1 << 30;
  lwc_string *const *f;
  if (!s->families || web_total == 0) return -1;
  for (f = s->families; *f; f++) {
    const char *n = lwc_string_data(*f);
    size_t l = lwc_string_length(*f);
    for (k = T_N; k < T_TOT; k++) {
      int d;
      if (!tipo[k].bien || strlen(tipo[k].familia) != l || strncasecmp(tipo[k].familia, n, l)) continue;
      /* la más parecida: el mismo estilo y el peso más cerca */
      d = abs(tipo[k].peso - s->weight) + (tipo[k].cursiva != cursiva ? 2000 : 0);
      if (d < d_mejor) { d_mejor = d; mejor = k; }
    }
    if (mejor >= 0) break;
  }
  if (mejor < 0) return -1;
  /* sin negrita de verdad (una letra variable o solo la normal): se hace */
  if (s->weight >= 600 && tipo[mejor].peso_real < 550) mejor |= NEGRITA;
  return mejor;
}

static int tipo_de(const plot_font_style_t *s) {
  int negrita = s->weight >= 600, cursiva = (s->flags & (FONTF_ITALIC | FONTF_OBLIQUE)) != 0, t;
  t = tipo_web(s);
  if (t >= 0) return t;
  switch (s->family) {
  case PLOT_FONT_FAMILY_SERIF: t = negrita ? T_SERIF_N : T_SERIF; break;
  case PLOT_FONT_FAMILY_MONOSPACE: t = negrita ? T_MONO_N : T_MONO; break;
  default: t = negrita ? (cursiva ? T_SANS_NC : T_SANS_N) : (cursiva ? T_SANS_C : T_SANS); break;
  }
  if (tipo[t].bien) return t;
  if (t == T_SANS_NC && tipo[T_SANS_N].bien) return T_SANS_N;
  if ((t == T_SERIF_N || t == T_MONO_N) && tipo[t - 1].bien) return t - 1;
  if (negrita && tipo[T_SANS_N].bien) return T_SANS_N;
  if (cursiva && tipo[T_SANS_C].bien) return T_SANS_C;
  for (t = 0; t < T_N; t++) if (tipo[t].bien) return t;
  return 0;
}
/* el tamaño en puntos de la pantalla (CSS: 96 por pulgada) */
static int px_de(const plot_font_style_t *s) {
  int px = (int)((int64_t)s->size * 96 / 72 / PLOT_STYLE_SCALE);
  if (px < 6) px = 6;
  if (px > 200) px = 200;
  return px;
}

static Glifo *glifo_de(int t, int px, uint32_t c) {
  uint32_t h = (c * 2654435761u ^ (uint32_t)px * 40503u ^ (uint32_t)t * 977u) & (GLIFOS - 1);
  Glifo *g = &glifo[h];
  stbtt_fontinfo *f;
  float esc;
  int av, lsb, i, x0, y0, x1, y1, neg = t & NEGRITA, tt = t, ti = t & ~NEGRITA;
  if (g->usada && g->letra == c && g->px == px && g->t == t) return g;
  if (!tipo[ti].bien) return NULL;
  f = &tipo[ti].f;
  i = stbtt_FindGlyphIndex(f, c);
  if (!i) {                             /* la letra no está en este tipo (ni el espacio, en los iconos): en otro */
    int k;
    for (k = 0; k < T_N; k++) if (tipo[k].bien && stbtt_FindGlyphIndex(&tipo[k].f, c)) { f = &tipo[k].f; break; }
    i = stbtt_FindGlyphIndex(f, c);
  }
  if (g->gris) { gris_total -= g->w * g->h; free(g->gris); }
  memset(g, 0, sizeof *g);
  esc = stbtt_ScaleForMappingEmToPixels(f, (float)px);
  stbtt_GetGlyphHMetrics(f, i, &av, &lsb);
  g->letra = c; g->px = px; g->t = tt; g->usada = 1;
  g->avance = (int)(av * esc * 65536.0f + 0.5f);
  stbtt_GetGlyphBitmapBox(f, i, esc, esc, &x0, &y0, &x1, &y1);
  if (x1 > x0 && y1 > y0 && c > ' ') {
    int w = x1 - x0 + (neg ? 1 : 0), hh = y1 - y0;
    g->gris = malloc(w * hh);
    if (g->gris) {
      stbtt_MakeGlyphBitmap(f, g->gris, x1 - x0, hh, w, esc, esc, i);
      if (neg) {
        /* negrita hecha aquí: cada punto, con el de su izquierda */
        int yy, xx;
        for (yy = 0; yy < hh; yy++) {
          unsigned char *r = g->gris + yy * w;
          r[w - 1] = 0;
          for (xx = w - 1; xx > 0; xx--) if (r[xx - 1] > r[xx]) r[xx] = r[xx - 1];
        }
      }
      g->x0 = x0; g->y0 = y0; g->w = w; g->h = hh;
      gris_total += g->w * g->h;
    }
  }
  if (neg && c > ' ') g->avance += 65536;
  /* si la memoria de los dibujos se llena, se vacía entera (se rehace sola) */
  if (gris_total > GRIS_MAX) {
    int k;
    for (k = 0; k < GLIFOS; k++) if (&glifo[k] != g && glifo[k].gris) {
      gris_total -= glifo[k].w * glifo[k].h; free(glifo[k].gris); glifo[k].gris = NULL; glifo[k].usada = 0;
    }
  }
  return g;
}

/* lo que avanza la letra c (1/65536 de punto) */
static int avance(int t, int px, uint32_t c) {
  Glifo *g;
  if (c == 0xA0) c = ' ';
  if (c < ' ') return 0;
  g = glifo_de(t, px, c);
  return g ? g->avance : px << 15;
}

static nserror l_ancho(const plot_font_style_t *s, const char *t, size_t n, int *w) {
  int tp = tipo_de(s), px = px_de(s);
  size_t i = 0;
  int64_t x = 0;
  while (i < n) {
    x += avance(tp, px, utf8_to_ucs4(t + i, n - i));
    i = utf8_next(t, n, i);
  }
  *w = (int)((x + 0x8000) >> 16);
  return NSERROR_OK;
}

static nserror l_posicion(const plot_font_style_t *s, const char *t, size_t n, int x, size_t *off, int *ax) {
  int tp = tipo_de(s), px = px_de(s);
  size_t i = 0;
  int64_t p = 0;
  while (i < n) {
    int a = avance(tp, px, utf8_to_ucs4(t + i, n - i));
    if (((p + a / 2) >> 16) > x) break;
    p += a;
    i = utf8_next(t, n, i);
  }
  *off = i;
  *ax = (int)((p + 0x8000) >> 16);
  return NSERROR_OK;
}

/* dónde partir la línea para que quepa en x (ver gui_layout_table.split) */
static nserror l_parte(const plot_font_style_t *s, const char *t, size_t n, int x, size_t *off, int *ax) {
  int tp = tipo_de(s), px = px_de(s);
  size_t i = 0, esp = 0;
  int64_t p = 0, p_esp = 0;
  while (i < n) {
    if (t[i] == ' ') { esp = i; p_esp = p; }
    p += avance(tp, px, utf8_to_ucs4(t + i, n - i));
    if (((p + 0x8000) >> 16) > x && esp != 0) {
      *off = esp;
      *ax = (int)((p_esp + 0x8000) >> 16);
      return NSERROR_OK;
    }
    i = utf8_next(t, n, i);
  }
  *off = n;
  *ax = (int)((p + 0x8000) >> 16);
  return NSERROR_OK;
}

/* pinta el texto con la línea de base en (x, y) */
void letra_pinta(nsfb_t *fb, const plot_font_style_t *s, int x, int y, const char *t, size_t n) {
  int tp = tipo_de(s), px = px_de(s);
  size_t i = 0;
  int64_t p = (int64_t)x << 16;
  int sub = (s->flags & FONTF_SMALLCAPS) != 0;
  (void)sub;
  while (i < n) {
    uint32_t c = utf8_to_ucs4(t + i, n - i);
    Glifo *g;
    i = utf8_next(t, n, i);
    if (c == 0xA0) c = ' ';
    if (c < ' ') continue;
    g = glifo_de(tp, px, c);
    if (!g) { p += px << 15; continue; }
    if (g->gris) {
      nsfb_bbox_t r;
      r.x0 = (int)(p >> 16) + g->x0;
      r.y0 = y + g->y0;
      r.x1 = r.x0 + g->w;
      r.y1 = r.y0 + g->h;
      nsfb_plot_glyph8(fb, &r, g->gris, g->w, s->foreground);
    }
    p += g->avance;
  }
}

/* la línea de base en una línea de alto «alto» (NetSurf: redraw.c, [HISPA]):
 * el alto de la letra (ascendente + descendente) centrado en la línea */
int letra_base(const plot_font_style_t *s, int alto) {
  int t = tipo_de(s) & ~NEGRITA, px = px_de(s);
  float esc, a, d;
  if (!tipo[t].bien) return alto * 3 / 4;
  esc = stbtt_ScaleForMappingEmToPixels(&tipo[t].f, (float)px);
  a = tipo[t].asc * esc;
  d = -tipo[t].desc * esc;
  return (int)((alto - (a + d)) / 2 + a + 0.5f);
}

/* ---- la página 1252 de los programas Hispa32 (los nombres de archivo, los cuadros) ---- */
static nserror a_local(const char *t, size_t n, char **r) { return utf8_to_enc(t, "WINDOWS-1252", n, r); }
static nserror de_local(const char *t, size_t n, char **r) { return utf8_from_enc(t, "WINDOWS-1252", n, r, NULL); }

static struct gui_layout_table tabla_letras = { .width = l_ancho, .position = l_posicion, .split = l_parte };
struct gui_layout_table *surf_tabla_letras = &tabla_letras;
static struct gui_utf8_table tabla_utf8 = { .utf8_to_local = a_local, .local_to_utf8 = de_local };
struct gui_utf8_table *surf_tabla_utf8 = &tabla_utf8;

/* ---- las letras de la web (webletra.c) ---- */
#include "webletra.h"

/* el peso de la letra (OS/2, usWeightClass); 400 si no lo dice */
static int peso_de(const unsigned char *d, int ini) {
  stbtt_uint32 o = stbtt__find_table((stbtt_uint8 *)d, ini, "OS/2");
  int p;
  if (!o) return 400;
  p = (d[o + 4] << 8) | d[o + 5];
  return p >= 100 && p <= 1000 ? p : 400;
}

bool letra_web(const char *familia, int peso, int cursiva, unsigned char *sfnt, size_t n) {
  int k, libre = -1, ini;
  if (!sfnt || n < 12) return false;
  for (k = T_N; k < T_TOT; k++) {
    if (tipo[k].bien && !strcasecmp(tipo[k].familia, familia) && tipo[k].peso == peso && tipo[k].cursiva == cursiva)
      return false;                       /* (ya está) */
    if (!tipo[k].bien && libre < 0) libre = k;
  }
  if (libre < 0 || web_total + n > WEB_MAX) { surf_reg("letra de la web %s: no cabe", familia); return false; }
  ini = stbtt_GetFontOffsetForIndex(sfnt, 0);
  if (ini < 0) return false;
  /* la misma letra que otra (Plus Jakarta Sans: una para todos los pesos): se comparte */
  for (k = T_N; k < T_TOT; k++)
    if (tipo[k].bien && tipo[k].propio && tipo[k].tam == n && !memcmp(tipo[k].datos, sfnt, n)) break;
  if (k < T_TOT) {
    tipo[libre] = tipo[k];
    tipo[libre].propio = 0;
    free(sfnt);
  } else {
    memset(&tipo[libre], 0, sizeof tipo[libre]);
    if (!stbtt_InitFont(&tipo[libre].f, sfnt, ini)) return false;
    tipo[libre].datos = sfnt;
    tipo[libre].propio = 1;
    tipo[libre].tam = n;
    tipo[libre].peso_real = peso_de(sfnt, ini);
    stbtt_GetFontVMetrics(&tipo[libre].f, &tipo[libre].asc, &tipo[libre].desc, &tipo[libre].salto);
    web_total += n;
  }
  snprintf(tipo[libre].familia, sizeof tipo[libre].familia, "%s", familia);
  tipo[libre].peso = peso;
  tipo[libre].cursiva = cursiva;
  tipo[libre].bien = 1;
  return true;
}
