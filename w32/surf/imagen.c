/* =============================================================================
 *  imagen.c - PNG y JPEG para NetSurf en Hispa-486 (NAVEGA2.EXE): con
 *  stb_image (terceros/stb, dominio público / MIT) en vez de libpng y libjpeg.
 *  Es un «content handler» de NetSurf como los suyos (content/handlers/image):
 *  al llegar la imagen entera se miran sus medidas, y se descodifica cuando
 *  hace falta pintarla (image_cache: la memoria de las imágenes de NetSurf).
 *  Los JPEG progresivos también; los PNG de 16 bits y con paleta, también.
 *  docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_GIF                 /* (los GIF, con libnsgif; los BMP, con libnsbmp) */
#define STBI_NO_BMP
#define STBI_NO_SIMD
#define STBI_ASSERT(x) ((void)0)
#define STBI_MAX_DIMENSIONS 8192
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* [WOFF] el zlib de stb_image, también para las letras WOFF 1.0 (woff.c):
   descomprime il bytes de i en o (hasta ol); da los bytes sacados o -1 */
int surf_zlib_descomprime(char *o, int ol, const char *i, int il) {
  return stbi_zlib_decode_buffer(o, ol, i, il);
}

static nserror im_crea(const content_handler *h, lwc_string *tipo, const struct http_parameter *par,
                       llcache_handle *ll, const char *cs, bool quirks, struct content **c) {
  struct content *n = calloc(1, sizeof(struct content));
  nserror e;
  if (!n) return NSERROR_NOMEM;
  e = content__init(n, h, tipo, par, ll, cs, quirks);
  if (e != NSERROR_OK) { free(n); return e; }
  *c = n;
  return NSERROR_OK;
}

/* descodificar (cuando hay que pintarla): RGBA, como quiere NetSurf */
static struct bitmap *im_descodifica(struct content *c) {
  size_t n;
  const uint8_t *d = content__get_source_data(c, &n);
  int w, h, k, y;
  unsigned char *px;
  struct bitmap *b;
  uint8_t *o;
  size_t paso;
  bool opaca = true;
  if (!d || !n) return NULL;
  px = stbi_load_from_memory(d, (int)n, &w, &h, &k, 4);
  if (!px) return NULL;
  for (y = 0; y < w * h && opaca; y++) if (px[y * 4 + 3] != 255) opaca = false;
  b = guit->bitmap->create(w, h, opaca ? BITMAP_OPAQUE : BITMAP_NONE);
  if (!b) { stbi_image_free(px); return NULL; }
  o = guit->bitmap->get_buffer(b);
  paso = guit->bitmap->get_rowstride(b);
  if (!o) { guit->bitmap->destroy(b); stbi_image_free(px); return NULL; }
  for (y = 0; y < h; y++) memcpy(o + y * paso, px + (size_t)y * w * 4, (size_t)w * 4);
  stbi_image_free(px);
  bitmap_format_to_client(b, &(bitmap_fmt_t){ .layout = BITMAP_LAYOUT_R8G8B8A8 });
  guit->bitmap->modified(b);
  return b;
}

/* llegada entera: sus medidas (sin descodificarla aún) */
static bool im_lista(struct content *c) {
  size_t n;
  const uint8_t *d = content__get_source_data(c, &n);
  int w, h, k;
  char *t;
  union content_msg_data m;
  if (!d || !stbi_info_from_memory(d, (int)n, &w, &h, &k)) {
    m.errordata.errorcode = NSERROR_UNKNOWN;
    m.errordata.errormsg = "La imagen no se entiende.";
    content_broadcast(c, CONTENT_MSG_ERROR, &m);
    return false;
  }
  c->width = w;
  c->height = h;
  c->size = (size_t)w * h * 4;
  image_cache_add(c, NULL, im_descodifica);
  t = messages_get_buff("PNGTitle", nsurl_access_leaf(llcache_handle_get_url(c->llcache)), w, h);
  if (t) { content__set_title(c, t); free(t); }
  content_set_ready(c);
  content_set_done(c);
  content_set_status(c, "");
  return true;
}

static nserror im_clona(const struct content *v, struct content **nc) {
  struct content *c = calloc(1, sizeof(struct content));
  nserror e;
  if (!c) return NSERROR_NOMEM;
  e = content__clone(v, c);
  if (e != NSERROR_OK) { content_destroy(c); return e; }
  if ((v->status == CONTENT_STATUS_READY || v->status == CONTENT_STATUS_DONE) && !im_lista(c)) {
    content_destroy(c);
    return NSERROR_CLONE_FAILED;
  }
  *nc = c;
  return NSERROR_OK;
}

static const content_handler im_manejo = {
  .create = im_crea, .data_complete = im_lista, .destroy = image_cache_destroy, .redraw = image_cache_redraw,
  .clone = im_clona, .get_internal = image_cache_get_internal, .type = image_cache_content_type,
  .is_opaque = image_cache_is_opaque, .no_share = false,
};

static const char *tipos_jpeg[] = { "image/jpeg", "image/jpg", "image/pjpeg" };
static const char *tipos_png[] = { "image/png", "image/x-png" };

nserror nsjpeg_init(void) {
  unsigned k;
  nserror e = NSERROR_OK;
  for (k = 0; k < sizeof tipos_jpeg / sizeof tipos_jpeg[0] && e == NSERROR_OK; k++)
    e = content_factory_register_handler(tipos_jpeg[k], &im_manejo);
  return e;
}
nserror nspng_init(void) {
  unsigned k;
  nserror e = NSERROR_OK;
  for (k = 0; k < sizeof tipos_png / sizeof tipos_png[0] && e == NSERROR_OK; k++)
    e = content_factory_register_handler(tipos_png[k], &im_manejo);
  return e;
}
