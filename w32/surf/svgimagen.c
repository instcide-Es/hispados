/* =============================================================================
 *  svgimagen.c - las imágenes SVG para NetSurf en el Navegador completo
 *  (NAVEGA2.EXE de Hispa-486, NAVEGA.BIN de Hispa-Pentium): con NanoSVG
 *  (terceros/nanosvg, licencia zlib) en vez de libsvgtiny, que necesita un
 *  lector de XML para libdom (expat o libxml2) que aquí no hay.
 *  Es un «content handler» como imagen.c: al llegar el SVG entero se leen sus
 *  medidas (width/height o, si no, las de viewBox) y se pinta en un mapa de
 *  puntos cuando hace falta (image_cache), con algo más de resolución si es
 *  pequeño (los iconos: NetSurf lo encoge al pintarlo, que queda mejor que
 *  agrandarlo). Lo que más se ve: los iconos de las webs de hoy, como el botón
 *  del menú plegable de Bootstrap (navbar-toggler-icon: tres rayas en un SVG
 *  dentro de un data:), sin el que la cabecera de clicgest.es no enseñaba
 *  dónde está «Acceder» con una ventana de menos de 1200 puntos.
 *  NanoSVG no lee rgba(): se cambia por rgb() (sin la transparencia) antes.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include "utils/utils.h"
#include "utils/messages.h"
#include "netsurf/bitmap.h"
#include "content/llcache.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/content_factory.h"
#include "desktop/gui_internal.h"
#include "desktop/bitmap.h"
#include "image/image_cache.h"
#include "surf.h"

/* acosf (NanoSVG: cuántos trozos lleva una curva o una punta redonda): la
 * biblioteca de C de Hispa-486 no la tiene; esta es la de Abramowitz y Stegun
 * (4.4.45, error < 0,0001), que basta para eso */
static float svg_acosf(float x) {
  float a, r;
  if (x > 1.0f) x = 1.0f;
  if (x < -1.0f) x = -1.0f;
  a = x < 0 ? -x : x;
  r = sqrtf(1.0f - a) * (1.5707288f + a * (-0.2121144f + a * (0.0742610f - 0.0187293f * a)));
  return x < 0 ? 3.14159265f - r : r;
}
#define acosf(x) svg_acosf(x)
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "../../terceros/nanosvg/nanosvg.h"
#include "../../terceros/nanosvg/nanosvgrast.h"

#define SVG_LADO_MAX 2048       /* (lo más que se pinta de lado) */
#define SVG_LADO_ICONO 64       /* (lo de menos: un icono se pinta a 64 y se encoge) */

/* una copia con su 0 final y rgba(r, g, b, a) -> rgb(r, g, b) (malloc) */
static char *svg_texto(const uint8_t *d, size_t n) {
  char *t = malloc(n + 1), *p, *q;
  if (!t) return NULL;
  memcpy(t, d, n);
  t[n] = 0;
  for (p = t; (p = strstr(p, "rgba(")) != NULL; ) {
    int comas = 0;
    memmove(p + 3, p + 4, strlen(p + 4) + 1);      /* «rgba(» -> «rgb(» */
    for (q = p + 4; *q && *q != ')'; q++) {
      if (*q == ',' && ++comas == 3) {               /* «, a)» fuera */
        char *f = strchr(q, ')');
        if (f) memmove(q, f, strlen(f) + 1);
        break;
      }
    }
    p += 4;
  }
  return t;
}

/* lee el SVG (NULL si no se entiende o no mide nada) */
static NSVGimage *svg_lee(struct content *c) {
  size_t n;
  const uint8_t *d = content__get_source_data(c, &n);
  NSVGimage *im;
  char *t;
  if (!d || !n) return NULL;
  if (!(t = svg_texto(d, n))) return NULL;
  im = nsvgParse(t, "px", 96.0f);
  if (!im) surf_reg("svg: no se entiende (%u octetos)", (unsigned)n);
  free(t);
  if (im && (!(im->width > 0.5f) || !(im->height > 0.5f))) { nsvgDelete(im); im = NULL; }
  return im;
}

static nserror svg_crea(const content_handler *h, lwc_string *tipo, const struct http_parameter *par,
                        llcache_handle *ll, const char *cs, bool quirks, struct content **c) {
  struct content *n = calloc(1, sizeof(struct content));
  nserror e;
  if (!n) return NSERROR_NOMEM;
  e = content__init(n, h, tipo, par, ll, cs, quirks);
  if (e != NSERROR_OK) { free(n); return e; }
  *c = n;
  return NSERROR_OK;
}

/* lo que se pinta: la escala (los pequeños, más; nada de más de SVG_LADO_MAX) */
static float svg_escala(float w, float h) {
  float m = w > h ? w : h, e = 1.0f;
  if (m < SVG_LADO_ICONO) e = SVG_LADO_ICONO / m;
  if (m * e > SVG_LADO_MAX) e = SVG_LADO_MAX / m;
  return e;
}

/* pintarlo (cuando hace falta): RGBA, como quiere NetSurf */
static struct bitmap *svg_descodifica(struct content *c) {
  NSVGimage *im = svg_lee(c);
  NSVGrasterizer *r;
  struct bitmap *b;
  unsigned char *px;
  uint8_t *o;
  size_t paso;
  float e;
  int w, h, y;
  if (!im) return NULL;
  e = svg_escala(im->width, im->height);
  w = (int)ceilf(im->width * e);
  h = (int)ceilf(im->height * e);
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  px = calloc((size_t)w * h, 4);
  r = nsvgCreateRasterizer();
  if (!px || !r) { free(px); if (r) nsvgDeleteRasterizer(r); nsvgDelete(im); return NULL; }
  nsvgRasterize(r, im, 0, 0, e, px, w, h, w * 4);
  nsvgDeleteRasterizer(r);
  nsvgDelete(im);
  b = guit->bitmap->create(w, h, BITMAP_NONE);
  if (!b) { free(px); return NULL; }
  o = guit->bitmap->get_buffer(b);
  paso = guit->bitmap->get_rowstride(b);
  if (!o) { guit->bitmap->destroy(b); free(px); return NULL; }
  for (y = 0; y < h; y++) memcpy(o + y * paso, px + (size_t)y * w * 4, (size_t)w * 4);
  free(px);
  /* (NanoSVG da el color ya multiplicado por la transparencia) */
  bitmap_format_to_client(b, &(bitmap_fmt_t){ .layout = BITMAP_LAYOUT_R8G8B8A8, .pma = true });
  guit->bitmap->modified(b);
  return b;
}

/* llegado entero: sus medidas (sin pintarlo aún) */
static bool svg_lista(struct content *c) {
  NSVGimage *im = svg_lee(c);
  union content_msg_data m;
  char *t;
  if (!im) {
    m.errordata.errorcode = NSERROR_UNKNOWN;
    m.errordata.errormsg = "La imagen SVG no se entiende.";
    content_broadcast(c, CONTENT_MSG_ERROR, &m);
    return false;
  }
  c->width = (int)ceilf(im->width);
  c->height = (int)ceilf(im->height);
  {
    float e = svg_escala(im->width, im->height);
    c->size = (size_t)ceilf(im->width * e) * (size_t)ceilf(im->height * e) * 4;
  }
  nsvgDelete(im);
  image_cache_add(c, NULL, svg_descodifica);
  t = messages_get_buff("PNGTitle", nsurl_access_leaf(llcache_handle_get_url(c->llcache)), c->width, c->height);
  if (t) { content__set_title(c, t); free(t); }
  content_set_ready(c);
  content_set_done(c);
  content_set_status(c, "");
  return true;
}

static nserror svg_clona(const struct content *v, struct content **nc) {
  struct content *c = calloc(1, sizeof(struct content));
  nserror e;
  if (!c) return NSERROR_NOMEM;
  e = content__clone(v, c);
  if (e != NSERROR_OK) { content_destroy(c); return e; }
  if ((v->status == CONTENT_STATUS_READY || v->status == CONTENT_STATUS_DONE) && !svg_lista(c)) {
    content_destroy(c);
    return NSERROR_CLONE_FAILED;
  }
  *nc = c;
  return NSERROR_OK;
}

static const content_handler svg_manejo = {
  .create = svg_crea, .data_complete = svg_lista, .destroy = image_cache_destroy, .redraw = image_cache_redraw,
  .clone = svg_clona, .get_internal = image_cache_get_internal, .type = image_cache_content_type,
  .is_opaque = image_cache_is_opaque, .no_share = false,
};

/* (la llama nspng_init, de imagen.c) */
nserror surf_svg_inicia(void) {
  return content_factory_register_handler("image/svg+xml", &svg_manejo);
}
