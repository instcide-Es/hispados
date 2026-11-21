/* =============================================================================
 *  rehaz.c - [JS] el Navegador completo de Hispa-486 (NAVEGA2.EXE): cuando
 *  JavaScript cambia la página (una clase, un atributo, un nodo nuevo), las
 *  cajas de NetSurf se hacen otra vez desde el árbol (con los estilos de
 *  nuevo) y se vuelve a maquetar, sin perder el sitio, lo escrito en los
 *  campos ni las imágenes ya bajadas. NetSurf 3.11 maqueta una vez (su
 *  «layout is static»): esto es lo que le falta para que el JavaScript de
 *  Bootstrap (menús, pestañas, ventanas) se vea.
 *  Se hace todo de una vez, al acabar el turno de JavaScript (guion.c), y
 *  solo si la página ya tiene cajas y NetSurf no las está haciendo.
 *  docs/486/NAVEGADOR2.md, «JavaScript».
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/corestrings.h"
#include "utils/talloc.h"
#include "content/content_protected.h"
#include "desktop/selection.h"
#include "html/html.h"
#include "html/private.h"
#include "html/box.h"
#include "html/box_construct.h"
#include "html/form_internal.h"
#include "html/object.h"
#include "html/interaction.h"
#include "css/select.h"

#include "surf.h"
#include "guion.h"

typedef unsigned int u32;
#ifdef SURF_ANFITRION
static u32 GetTickCount(void) { return 0; }
#else
u32 __attribute__((stdcall)) GetTickCount(void);
#endif

extern struct box html_hispa_sin_caja;
/* box_construct.c [HISPA]: las cajas de una vez, sin el planificador */
nserror dom_to_box_ya(dom_node *n, html_content *c);
/* css/select.c [HISPA]: los datos de libcss de un nodo, fuera (se rehacen) */
void nscss_hispa_olvida(dom_node *n);

static void rehaz_tic(void *p) { rehaz_cajas(p); }
void rehaz_pide(html_content *c) { framebuffer_schedule(0, rehaz_tic, c); }
void rehaz_olvida(html_content *c) { framebuffer_schedule(-1, rehaz_tic, c); }
bool rehaz_listo(html_content *c) {
  return c->layout && !c->box_conversion_context && !c->aborted && !c->base.locked
      && (c->base.status == CONTENT_STATUS_READY || c->base.status == CONTENT_STATUS_DONE);
}

/* un nodo (y lo de dentro) sin su caja: las suyas se van con las demás */
static void sin_caja(dom_node *n, int css) {
  NodoI *h;
  void *viejo = NULL;
  if (NI(n)->tipo == DOM_ELEMENT_NODE) {
    struct form_control *k = html_forms_hispa_ctl(n);
    dom_node_set_user_data(n, corestring_dom___ns_key_box_node_data, NULL, NULL, &viejo);
    if (k) k->box = NULL;
    if (css) nscss_hispa_olvida(n);
  }
  for (h = NI(n)->hijo; h; h = h->sig) sin_caja((dom_node *)h, css);
}
void rehaz_suelta_nodo(dom_node *n) { sin_caja(n, 0); }

static unsigned n_rehechas, t_rehechas;
void rehaz_cajas(html_content *c) {
  struct form *f;
  struct form_control *k;
  struct content_html_object *o;
  struct form_control *foco;
  dom_node *html = NULL;
  u32 t0 = GetTickCount();
  if (!rehaz_listo(c)) {
    /* (sin cajas todavía: las primeras ya verán lo cambiado; haciéndolas: luego) */
    if (c->box_conversion_context || c->base.locked) framebuffer_schedule(100, rehaz_tic, c);
    return;
  }
  if (dom_document_get_document_element(c->document, &html) != DOM_NO_ERR || !html) return;
  /* lo que apunta a las cajas de ahora */
  if (c->sel) selection_clear(c->sel, false);
  if (c->drag_type != HTML_DRAG_NONE) { union html_drag_owner nadie; nadie.no_owner = true; html_set_drag_type(c, HTML_DRAG_NONE, nadie, NULL); }
  if (c->selection_type != HTML_SELECTION_NONE) { union html_selection_owner nadie; nadie.none = true; html_set_selection(c, HTML_SELECTION_NONE, nadie, false); }
  if (c->focus_type == HTML_FOCUS_CONTENT) { union html_focus_owner yo; yo.self = true; html_set_focus(c, HTML_FOCUS_SELF, yo, true, 0, 0, 0, NULL); }
  /* el campo del cursor: su control (que se queda); luego, su caja nueva */
  foco = c->focus_type == HTML_FOCUS_TEXTAREA && c->focus_owner.textarea ? c->focus_owner.textarea->gadget : NULL;
  for (f = c->forms; f; f = f->prev) for (k = f->controls; k; k = k->next) k->box = NULL;
  for (o = c->object_list; o; o = o->next) if (o->box) o->box = &html_hispa_sin_caja;   /* (object.c [HISPA]) */
  sin_caja(html, 1);
  /* las cajas, fuera; y otra vez */
  talloc_free(c->bctx);
  c->bctx = NULL;
  c->layout = NULL;
  if (dom_to_box_ya(html, c) != NSERROR_OK || !c->layout) {
    NSLOG(netsurf, ERROR, "no se han podido rehacer las cajas");
    if (c->focus_type == HTML_FOCUS_TEXTAREA) { c->focus_type = HTML_FOCUS_SELF; c->focus_owner.self = true; }
    dom_node_unref(html);
    return;
  }
  dom_node_unref(html);
  if (c->focus_type == HTML_FOCUS_TEXTAREA) {
    if (foco && foco->box) c->focus_owner.textarea = foco->box;
    else { c->focus_type = HTML_FOCUS_SELF; c->focus_owner.self = true; }
  }
  content__reformat(&c->base, false, c->base.available_width, c->base.available_height);
  n_rehechas++;
  t_rehechas += GetTickCount() - t0;
  surf_reg("cajas otra vez (JavaScript): %u ms (%u veces, %u ms en total)", GetTickCount() - t0, n_rehechas, t_rehechas);
}

/* [HISPA] position: fixed y sticky (ver guion.h) */
int hispa_fijo_x, hispa_fijo_y;
/* (solo la de fuera: las cajas de dentro, anónimas o no, van con ella) */
static bool hay_fuera(const struct box *b, uint8_t que) {
  for (b = b->parent; b; b = b->parent)
    if (b->style && css_computed_position(b->style) == que) return true;
  return false;
}
bool box_hispa_fijo(const struct box *b) {
  return b->style && css_computed_position(b->style) == CSS_POSITION_FIXED && !hay_fuera(b, CSS_POSITION_FIXED);
}
/* sticky: se pinta al final, como lo que va por encima (si no, lo de detrás
 * en la página lo tapaba al pegarse arriba) */
bool box_hispa_pegado(const struct box *b) {
  return b->style && css_computed_position(b->style) == CSS_POSITION_STICKY && !hay_fuera(b, CSS_POSITION_STICKY);
}
bool box_hispa_hay_fijos(const struct box *b) {
  for (; b; b = b->next) {
    if (b->style) {
      uint8_t p = css_computed_position(b->style);
      if (p == CSS_POSITION_FIXED || p == CSS_POSITION_STICKY) return true;
    }
    if (box_hispa_hay_fijos(b->children)) return true;
  }
  return false;
}

/* lo que va por encima (ver guion.h) */
int box_hispa_z(const struct box *b) {
  css_fixed z = 0;
  if (!b->style || css_computed_z_index(b->style, &z) != CSS_Z_INDEX_SET) return 0;
  return FIXTOINT(z);
}
bool box_hispa_encima(const struct box *b) {
  uint8_t p;
  if (!b->style) return false;
  p = css_computed_position(b->style);
  if (p != CSS_POSITION_ABSOLUTE && p != CSS_POSITION_FIXED) return false;
  /* (lo escondido, como el menú lateral «off-canvas» de ClicGest, que
   * cssvar.c deja en visibility: hidden, no tapa los clics) */
  if (css_computed_visibility(b->style) == CSS_VISIBILITY_HIDDEN) return false;
  return box_hispa_z(b) > 0;
}
