/* =============================================================================
 *  guion.c - JavaScript en el Navegador completo de Hispa-486 (NAVEGA2.EXE):
 *  la interfaz de NetSurf para un motor de JavaScript (content/handlers/
 *  javascript/js.h) hecha con QuickJS (HSPJS.DLL, la del Navegador propio:
 *  ES2020, que es lo que usa ClicGest: funciones flecha, clases, fetch) sobre
 *  el árbol de libdom que maqueta NetSurf.
 *
 *  Como en el Navegador propio (w32/navega/script.c), el DOM y lo de window
 *  (document, eventos, fetch, localStorage...) lo hace el preludio en
 *  JavaScript, w32/navega/dom.js (el mismo archivo), sobre lo poco que da
 *  este (el objeto N); w32/surf/surf.js le añade lo de NetSurf (las medidas de
 *  verdad de las cajas, getComputedStyle, el desplazamiento). Aquí:
 *   - un motor (JSRuntime y JSContext) por página: se suelta entero al irse;
 *   - un objeto por nodo de libdom (siempre el mismo);
 *   - tras cada guion, evento o temporizador («un turno»), si el árbol ha
 *     cambiado, NetSurf vuelve a hacer las cajas de la página (rehaz.c);
 *   - los temporizadores, con el planificador de NetSurf; fetch y
 *     XMLHttpRequest, por la cola de red.c (la misma conexión); localStorage
 *     en NAVEGA2.ALM; la consola y los errores, en SURFJS.LOG;
 *   - un tope de tiempo por turno: si una página se queda dando vueltas, se
 *     para ese guion y el Navegador sigue.
 *  Lo compilado (el preludio, Bootstrap...) se guarda mientras el Navegador
 *  está abierto: las páginas siguientes de ClicGest no lo vuelven a compilar.
 *  docs/486/NAVEGADOR2.md, «JavaScript».
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "quickjs.h"

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/corestrings.h"
#include "utils/nsoption.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/textarea.h"
#include "javascript/js.h"
#include "javascript/content.h"
#include "html/private.h"
#include "html/box.h"
#include "html/box_construct.h"
#include "html/box_inspect.h"
#include "html/form_internal.h"
#include "html/interaction.h"
#include <dom/bindings/hubbub/parser.h>
#include <libcss/libcss.h>

#include "surf.h"
#include "rh.h"
#include "guion.h"
#include "surf_js.h"                    /* (build/surf: dom.js y surf.js, como texto) */

#define WINAPI __attribute__((stdcall))
typedef void *HANDLE;
typedef uint32_t u32;
typedef uint8_t u8;
u32 WINAPI GetTickCount(void);
int WINAPI MessageBoxA(HANDLE, const char *, const char *, u32);
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema);   /* (HSPJS.DLL) */
void HjReinicia(void);
/* surf.c */
HANDLE surf_ventana(void);
void surf_desp(int *x, int *y);
void surf_ve_a(int x, int y);
void surf_pon_dir(const char *u8);

#define AGENTE "HispaSurf/0.1 (NetSurf 3.11; Hispa-DOS 486)"   /* (el de http.c: SURF_UA en surf.mk) */

int guion_tope_ms = 15000;              /* lo más que puede tardar un turno */
int guion_memoria_kb = 8192;            /* lo más que puede usar el JavaScript de una página */

/* =============================================================================
 *  El registro (SURFJS.LOG): la consola de las páginas y sus errores
 * ============================================================================= */
static char reg_buf[8192];
static u32 n_reg;
static char reg_ruta[300];
static void reg_vacia(void) {
  FILE *f;
  if (!n_reg || !reg_ruta[0]) { n_reg = 0; return; }
  if ((f = fopen(reg_ruta, "ab"))) { fwrite(reg_buf, 1, n_reg, f); fclose(f); }
  n_reg = 0;
}
static void reg(const char *pre, const char *t, u32 n) {
  u32 lp = strlen(pre);
  if (n_reg + lp + n + 4 > sizeof reg_buf) reg_vacia();
  if (lp + n + 4 > sizeof reg_buf) n = sizeof reg_buf - lp - 4;
  memcpy(reg_buf + n_reg, pre, lp); n_reg += lp;
  memcpy(reg_buf + n_reg, t, n); n_reg += n;
  reg_buf[n_reg++] = '\r'; reg_buf[n_reg++] = '\n';
}
static void regf(const char *pre, const char *f, ...) {
  char b[600];
  va_list ap;
  va_start(ap, f);
  vsnprintf(b, sizeof b, f, ap);
  va_end(ap);
  reg(pre, b, strlen(b));
}

/* =============================================================================
 *  El motor de una página
 * ============================================================================= */
typedef struct { dom_node *n; JSValue v; } Env;
typedef struct Temp { struct Temp *sig; struct jsthread *t; int id; } Temp;
typedef struct Diferido { struct Diferido *sig; int que; dom_node *n, *otro; char *texto; int k; } Diferido;
enum { D_IR = 1, D_ENVIA, D_HISTORIA, D_EVENTO, D_FOCO };
struct jsheap { int tope; };
struct jsthread {
  struct jsthread *sig;
  JSRuntime *rt;
  JSContext *cx;
  struct browser_window *bw;
  html_content *html;
  dom_document *doc;
  Env *tabla;
  u32 n_tabla, cap_tabla;
  JSValue proto_el, proto_tx, proto_doc, fn_evento, fn_tic, fn_llega, fn_foco, fn_cargado, fn_informa;
  int nivel, cerrado, fase, sucio;
  u32 limite, t_pagina;
  Temp *temps;
  Diferido *dif;
  char *url_js;                         /* la dirección de pushState o del #hash */
};
static struct jsthread *hilos;          /* los de todas las páginas vivas */
static struct jsthread *H;              /* el de ahora */
static JSContext *cx;                   /* el suyo */
static JSClassID clase_nodo;
static int n_motores;
static int sin_evento_submit;           /* form.submit(): sin el evento submit */
static u8 *preludio_bc, *surf_bc;
static size_t n_preludio_bc, n_surf_bc;

#define ENTRA(t) struct jsthread *h_antes = H; JSContext *c_antes = cx; H = (t); cx = (t)->cx
#define SALE() H = h_antes; cx = c_antes

/* ---- la tabla nodo -> objeto (los tiene vivos, y una referencia al nodo,
 * hasta que se va la página) ---- */
static u32 hash_p(const void *p) { return ((u32)p >> 2) * 2654435761u; }
static Env *env_de(dom_node *n) {
  u32 k;
  if (!H->cap_tabla) return NULL;
  for (k = hash_p(n) & (H->cap_tabla - 1); H->tabla[k].n; k = (k + 1) & (H->cap_tabla - 1)) if (H->tabla[k].n == n) return &H->tabla[k];
  return NULL;
}
static Env *env_pon(dom_node *n) {
  u32 k;
  if (H->n_tabla * 2 >= H->cap_tabla) {
    Env *viejo = H->tabla;
    u32 c = H->cap_tabla, j;
    H->cap_tabla = c ? c * 2 : 1024;
    H->tabla = calloc(H->cap_tabla, sizeof(Env));
    if (!H->tabla) { H->tabla = viejo; H->cap_tabla = c; return NULL; }
    for (j = 0; j < c; j++)
      if (viejo[j].n) {
        for (k = hash_p(viejo[j].n) & (H->cap_tabla - 1); H->tabla[k].n; k = (k + 1) & (H->cap_tabla - 1)) ;
        H->tabla[k] = viejo[j];
      }
    free(viejo);
  }
  for (k = hash_p(n) & (H->cap_tabla - 1); H->tabla[k].n; k = (k + 1) & (H->cap_tabla - 1)) ;
  H->tabla[k].n = n; H->tabla[k].v = JS_UNDEFINED;
  dom_node_ref(n);
  H->n_tabla++;
  return &H->tabla[k];
}
static JSValue envuelve(dom_node *n) {
  Env *e;
  JSValue o, proto;
  if (!n) return JS_NULL;
  if ((e = env_de(n)) && !JS_IsUndefined(e->v)) return JS_DupValue(cx, e->v);
  switch (NI(n)->tipo) {
  case DOM_ELEMENT_NODE: proto = H->proto_el; break;
  case DOM_DOCUMENT_NODE: proto = H->proto_doc; break;
  default: proto = H->proto_tx; break;
  }
  o = JS_NewObjectProtoClass(cx, proto, clase_nodo);
  if (JS_IsException(o)) return o;
  JS_SetOpaque(o, n);
  if (!e) e = env_pon(n);
  if (e) e->v = JS_DupValue(cx, o);
  return o;
}
static dom_node *nodo_de(JSValueConst v) { return JS_GetOpaque(v, clase_nodo); }

/* ---- textos ---- */
static JSValue a_js(dom_string *d) {
  return d ? JS_NewStringLen(cx, dom_string_data(d), dom_string_byte_length(d)) : JS_NewString(cx, "");
}
static dom_string *ds_de(JSValueConst v) {
  size_t n;
  const char *u = JS_ToCStringLen(cx, &n, v);
  dom_string *d = NULL;
  if (!u) return NULL;
  dom_string_create((const uint8_t *)u, n, &d);
  JS_FreeCString(cx, u);
  return d;
}
static int conectado(dom_node *n) {
  NodoI *x;
  for (x = NI(n); x; x = x->padre) if (x->tipo == DOM_DOCUMENT_NODE) return 1;
  return 0;
}
static int etq(dom_node *n, const char *nombre) { return sel_etq(n, nombre); }
static int es_guion(dom_node *n) { return etq(n, "script") || etq(n, "style"); }

/* ---- los controles de formulario de NetSurf ---- */
static struct form_control *ctl_de(dom_node *n) {
  struct form *f;
  struct form_control *k;
  struct box *b;
  if (!H->html) return NULL;
  for (f = H->html->forms; f; f = f->prev)
    for (k = f->controls; k; k = k->next) if (k->node == n) return k;
  if (etq(n, "input") || etq(n, "select") || etq(n, "textarea") || etq(n, "button")) return html_forms_get_control_for_node(H->html->forms, n);
  b = box_for_node(n);
  return b ? b->gadget : NULL;
}
static void repinta(struct form_control *k) {
  if (k && k->box && H->html && H->html->layout) html__redraw_a_box(H->html, k->box);
}
static void ensucia(void) { H->sucio = 1; }

/* =============================================================================
 *  Lo que da guion.c a dom.js (el objeto N)
 * ============================================================================= */
#define FN(nombre) static JSValue nombre(JSContext *c, JSValueConst este, int argc, JSValueConst *a)
#define NODO(i, n) dom_node *n = argc > i ? nodo_de(a[i]) : NULL; if (!n) return JS_ThrowTypeError(c, "no es un nodo")
#define USA (void)c; (void)este; (void)argc
FN(f_es) { USA; return JS_NewBool(c, argc > 0 && nodo_de(a[0]) != NULL); }
FN(f_tipo) {
  USA; NODO(0, n);
  switch (NI(n)->tipo) {
  case DOM_ELEMENT_NODE: return JS_NewInt32(c, 1);
  case DOM_DOCUMENT_NODE: return JS_NewInt32(c, 9);
  case DOM_COMMENT_NODE: return JS_NewInt32(c, 8);
  case DOM_DOCUMENT_TYPE_NODE: return JS_NewInt32(c, 10);
  case DOM_DOCUMENT_FRAGMENT_NODE: return JS_NewInt32(c, 11);
  default: return JS_NewInt32(c, 3);
  }
}
FN(f_padre) { USA; NODO(0, n); return envuelve((dom_node *)NI(n)->padre); }
FN(f_hijo) { USA; NODO(0, n); return envuelve((dom_node *)NI(n)->hijo); }
FN(f_ult) { USA; NODO(0, n); return envuelve((dom_node *)NI(n)->ult); }
FN(f_sig) { USA; NODO(0, n); return envuelve((dom_node *)NI(n)->sig); }
FN(f_ant) { USA; NODO(0, n); return envuelve((dom_node *)NI(n)->ant); }
FN(f_conectado) { USA; NODO(0, n); return JS_NewBool(c, conectado(n)); }
FN(f_dentro) {
  NodoI *x;
  USA; NODO(0, p); NODO(1, n);
  for (x = NI(n); x; x = x->padre) if (x == NI(p)) return JS_TRUE;
  return JS_FALSE;
}
FN(f_nombre) {
  char b[64];
  size_t l, k;
  USA; NODO(0, n);
  if (NI(n)->tipo != DOM_ELEMENT_NODE || !NI(n)->nombre) return JS_NewString(c, NI(n)->tipo == DOM_ELEMENT_NODE ? "" : "#text");
  l = dom_string_byte_length(NI(n)->nombre);
  if (l > 63) l = 63;
  for (k = 0; k < l; k++) { char x = dom_string_data(NI(n)->nombre)[k]; b[k] = x >= 'A' && x <= 'Z' ? x + 32 : x; }
  return JS_NewStringLen(c, b, l);
}
FN(f_texto) {
  dom_string *s = NULL;
  JSValue r;
  USA; NODO(0, n);
  if (NI(n)->tipo == DOM_ELEMENT_NODE || NI(n)->tipo == DOM_DOCUMENT_NODE) return JS_NewString(c, "");
  dom_characterdata_get_data((dom_characterdata *)n, &s);
  r = a_js(s);
  if (s) dom_string_unref(s);
  return r;
}
FN(f_pon_texto) {
  dom_string *s;
  USA; NODO(0, n);
  if (NI(n)->tipo == DOM_ELEMENT_NODE || NI(n)->tipo == DOM_DOCUMENT_NODE) return JS_UNDEFINED;
  if (!(s = ds_de(a[1]))) return JS_EXCEPTION;
  dom_characterdata_set_data((dom_characterdata *)n, s);
  dom_string_unref(s);
  ensucia();
  return JS_UNDEFINED;
}
FN(f_contenido) {
  dom_string *s = NULL;
  JSValue r;
  USA; NODO(0, n);
  dom_node_get_text_content(n, &s);
  r = a_js(s);
  if (s) dom_string_unref(s);
  return r;
}
static void suelta_hijos(dom_node *n) {
  NodoI *h;
  for (h = NI(n)->hijo; h; h = h->sig) rehaz_suelta_nodo((dom_node *)h);
}
FN(f_pon_contenido) {
  dom_string *s;
  USA; NODO(0, n);
  if (!(s = ds_de(a[1]))) return JS_EXCEPTION;
  suelta_hijos(n);
  dom_node_set_text_content(n, s);
  dom_string_unref(s);
  ensucia();
  return JS_UNDEFINED;
}
FN(f_inserta) {
  dom_node *ref, *r = NULL;
  dom_exception e;
  USA; NODO(0, p); NODO(1, n);
  ref = argc > 2 ? nodo_de(a[2]) : NULL;
  if (ref && (dom_node *)NI(ref)->padre != p) ref = NULL;
#ifdef PERFIL
  { extern unsigned long long pf_dom; unsigned long long t0; __asm__ volatile("rdtsc" : "=A"(t0));
  e = dom_node_insert_before(p, n, ref, &r);
  { unsigned long long t1; __asm__ volatile("rdtsc" : "=A"(t1)); pf_dom += t1 - t0; } }
#else
  e = dom_node_insert_before(p, n, ref, &r);
#endif
  if (r) dom_node_unref(r);
  if (e == DOM_HIERARCHY_REQUEST_ERR) return JS_ThrowTypeError(c, "HierarchyRequestError: ese nodo no puede ir ahí");
  if (e != DOM_NO_ERR) return JS_ThrowTypeError(c, "no se ha podido insertar el nodo (%d)", (int)e);
  ensucia();
  return JS_UNDEFINED;
}
FN(f_quita) {
  dom_node *r = NULL;
  USA; NODO(0, n);
  if (!NI(n)->padre) return JS_UNDEFINED;
  rehaz_suelta_nodo(n);
  dom_node_remove_child((dom_node *)NI(n)->padre, n, &r);
  if (r) dom_node_unref(r);
  ensucia();
  return JS_UNDEFINED;
}
FN(f_clona) {
  dom_node *r = NULL;
  JSValue v;
  USA; NODO(0, n);
  dom_node_clone_node(n, argc > 1 && JS_ToBool(c, a[1]), &r);
  v = envuelve(r);
  if (r) dom_node_unref(r);
  return v;
}
FN(f_crea_texto) {
  dom_string *s;
  dom_text *t = NULL;
  JSValue v;
  USA;
  if (!(s = ds_de(a[0]))) return JS_EXCEPTION;
  dom_document_create_text_node(H->doc, s, &t);
  dom_string_unref(s);
  v = envuelve((dom_node *)t);
  if (t) dom_node_unref(t);
  return v;
}
FN(f_crea) {
  dom_string *s;
  dom_element *e = NULL;
  JSValue v;
  USA;
  if (!(s = ds_de(a[0]))) return JS_EXCEPTION;
  dom_document_create_element(H->doc, s, &e);
  dom_string_unref(s);
  if (!e) return JS_ThrowTypeError(c, "InvalidCharacterError: no es un nombre de etiqueta");
  v = envuelve((dom_node *)e);
  dom_node_unref(e);
  return v;
}
FN(f_atr) {
  dom_string *nom, *v = NULL;
  JSValue r;
  USA; NODO(0, n);
  if (!ES_ELEM(n) || !(nom = ds_de(a[1]))) return JS_NULL;
  dom_element_get_attribute(n, nom, &v);
  dom_string_unref(nom);
  if (!v) return JS_NULL;
  r = a_js(v);
  dom_string_unref(v);
  return r;
}
FN(f_atrs) {
  JSValue r = JS_NewArray(c);
  dom_namednodemap *m = NULL;
  dom_ulong l = 0, k;
  USA; NODO(0, n);
  if (!ES_ELEM(n) || dom_node_get_attributes(n, &m) != DOM_NO_ERR || !m) return r;
  dom_namednodemap_get_length(m, &l);
  for (k = 0; k < l; k++) {
    dom_attr *at = NULL;
    dom_string *nm = NULL;
    if (dom_namednodemap_item(m, k, (void *)&at) == DOM_NO_ERR && at) {
      dom_attr_get_name(at, &nm);
      JS_SetPropertyUint32(c, r, k, a_js(nm));
      if (nm) dom_string_unref(nm);
      dom_node_unref(at);
    }
  }
  dom_namednodemap_unref(m);
  return r;
}
/* Popper (los menús de Bootstrap) coloca con «inset: 0 auto auto 0» y
 * «transform: translate(x, y)», que NetSurf no tiene: se cambian por left y
 * top (lo mismo, en un elemento con position: absolute) */
static char *sin_transform(const char *s) {
  const char *t = strstr(s, "transform:");
  double x = 0, y = 0;
  char *r;
  if (!t || (!strstr(t, "translate(") && !strstr(t, "translate3d("))) return NULL;
  {
    const char *p = strstr(t, "translate");
    p = strchr(p, '(');
    if (!p || sscanf(p + 1, "%lfpx, %lfpx", &x, &y) < 2) return NULL;
  }
  r = malloc(strlen(s) + 80);
  if (!r) return NULL;
  {
    const char *f = strchr(t, ';');
    size_t a = t - s;
    memcpy(r, s, a);
    r[a] = 0;
    strcat(r, f ? f + 1 : "");
    sprintf(r + strlen(r), "; left: %dpx; top: %dpx; right: auto; bottom: auto", (int)x, (int)y);
  }
  return r;
}
static void sincroniza(dom_node *n, const char *nom, int quita);
FN(f_pon_atr) {
  dom_string *nom, *v;
  const char *nm;
  size_t ln;
  USA; NODO(0, n);
  if (!ES_ELEM(n)) return JS_UNDEFINED;
  if (!(nom = ds_de(a[1]))) return JS_EXCEPTION;
  if (!(v = ds_de(a[2]))) { dom_string_unref(nom); return JS_EXCEPTION; }
  nm = dom_string_data(nom); ln = dom_string_byte_length(nom);
  if (ln == 5 && !memcmp(nm, "style", 5)) {
    char *s2 = sin_transform(dom_string_data(v));
    if (s2) { dom_string *v2 = NULL; dom_string_create((const uint8_t *)s2, strlen(s2), &v2); free(s2); if (v2) { dom_string_unref(v); v = v2; } }
  }
#ifdef PERFIL
  { extern unsigned long long pf_dom2; unsigned long long t0; __asm__ volatile("rdtsc" : "=A"(t0));
  dom_element_set_attribute(n, nom, v);
  { unsigned long long t1; __asm__ volatile("rdtsc" : "=A"(t1)); pf_dom2 += t1 - t0; } }
#else
  dom_element_set_attribute(n, nom, v);
#endif
  sincroniza(n, nm, 0);
  dom_string_unref(nom); dom_string_unref(v);
  ensucia();
  return JS_UNDEFINED;
}
FN(f_quita_atr) {
  dom_string *nom;
  USA; NODO(0, n);
  if (!ES_ELEM(n)) return JS_UNDEFINED;
  if (!(nom = ds_de(a[1]))) return JS_EXCEPTION;
  dom_element_remove_attribute(n, nom);
  sincroniza(n, dom_string_data(nom), 1);
  dom_string_unref(nom);
  ensucia();
  return JS_UNDEFINED;
}
/* los atributos que NetSurf lee una vez, al hacer el control: se cambian en él */
static void sincroniza(dom_node *n, const char *nom, int quita) {
  struct form_control *k;
  if (!etq(n, "input") && !etq(n, "textarea") && !etq(n, "select") && !etq(n, "button")) return;
  if (!(k = ctl_de(n))) return;
  if (!strcmp(nom, "disabled")) { k->disabled = !quita; repinta(k); }
  else if (!strcmp(nom, "type") && etq(n, "input")) {
    /* el «Ver» de la contraseña: password <-> text */
    dom_string *t = sel_atr(n, "type");
    int clave = t && !strcasecmp(dom_string_data(t), "password");
    int texto = !t || !strcasecmp(dom_string_data(t), "text") || !strcasecmp(dom_string_data(t), "email")
                || !strcasecmp(dom_string_data(t), "search") || !strcasecmp(dom_string_data(t), "tel")
                || !strcasecmp(dom_string_data(t), "url") || !strcasecmp(dom_string_data(t), "number");
    if (t) dom_string_unref(t);
    if ((k->type == GADGET_PASSWORD && texto) || (k->type == GADGET_TEXTBOX && clave)) {
      k->type = clave ? GADGET_PASSWORD : GADGET_TEXTBOX;
      if (k->data.text.ta) {
        /* el campo se hace otra vez al rehacer las cajas (box_textarea.c), con su texto */
        if (H->html && H->html->focus_type == HTML_FOCUS_TEXTAREA && H->html->focus_owner.textarea
            && H->html->focus_owner.textarea->gadget == k) {
          union html_focus_owner yo;
          yo.self = true;
          html_set_focus(H->html, HTML_FOCUS_SELF, yo, true, 0, 0, 0, NULL);
        }
        textarea_destroy(k->data.text.ta);
        k->data.text.ta = NULL;
      }
    }
  }
}
/* ---- innerHTML y outerHTML: el árbol en HTML ---- */
typedef struct { char *b; size_t n, cap; } Buf;
static void b_pon(Buf *b, const char *t, size_t n) {
  if (b->n + n + 1 > b->cap) {
    size_t c = (b->cap + n) * 2 + 256;
    char *q = realloc(b->b, c);
    if (!q) return;
    b->b = q; b->cap = c;
  }
  memcpy(b->b + b->n, t, n);
  b->n += n;
}
static void b_esc(Buf *b, const char *t, size_t n, int atr) {
  size_t k, i = 0;
  for (k = 0; k < n; k++) {
    const char *r = t[k] == '&' ? "&amp;" : t[k] == '<' && !atr ? "&lt;" : t[k] == '>' && !atr ? "&gt;" : t[k] == '"' && atr ? "&quot;" : NULL;
    if (r) { b_pon(b, t + i, k - i); b_pon(b, r, strlen(r)); i = k + 1; }
  }
  b_pon(b, t + i, n - i);
}
static int es_vacio(dom_node *n) {
  static const char *const v[] = { "area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr", NULL };
  int k;
  for (k = 0; v[k]; k++) if (etq(n, v[k])) return 1;
  return 0;
}
static void serializa(Buf *b, dom_node *n, int crudo) {
  NodoI *h;
  switch (NI(n)->tipo) {
  case DOM_TEXT_NODE: case DOM_CDATA_SECTION_NODE:
    if (NI(n)->valor) {
      if (crudo) b_pon(b, dom_string_data(NI(n)->valor), dom_string_byte_length(NI(n)->valor));
      else b_esc(b, dom_string_data(NI(n)->valor), dom_string_byte_length(NI(n)->valor), 0);
    }
    return;
  case DOM_COMMENT_NODE:
    b_pon(b, "<!--", 4);
    if (NI(n)->valor) b_pon(b, dom_string_data(NI(n)->valor), dom_string_byte_length(NI(n)->valor));
    b_pon(b, "-->", 3);
    return;
  case DOM_ELEMENT_NODE: {
    char nom[64];
    size_t l = NI(n)->nombre ? dom_string_byte_length(NI(n)->nombre) : 0, k;
    dom_namednodemap *m = NULL;
    dom_ulong na = 0;
    if (l > 63) l = 63;
    for (k = 0; k < l; k++) { char x = dom_string_data(NI(n)->nombre)[k]; nom[k] = x >= 'A' && x <= 'Z' ? x + 32 : x; }
    b_pon(b, "<", 1); b_pon(b, nom, l);
    if (dom_node_get_attributes(n, &m) == DOM_NO_ERR && m) {
      dom_namednodemap_get_length(m, &na);
      for (k = 0; k < na; k++) {
        dom_attr *at = NULL;
        dom_string *an = NULL, *av = NULL;
        if (dom_namednodemap_item(m, k, (void *)&at) != DOM_NO_ERR || !at) continue;
        dom_attr_get_name(at, &an); dom_attr_get_value(at, &av);
        b_pon(b, " ", 1);
        if (an) b_pon(b, dom_string_data(an), dom_string_byte_length(an));
        b_pon(b, "=\"", 2);
        if (av) b_esc(b, dom_string_data(av), dom_string_byte_length(av), 1);
        b_pon(b, "\"", 1);
        if (an) dom_string_unref(an);
        if (av) dom_string_unref(av);
        dom_node_unref(at);
      }
      dom_namednodemap_unref(m);
    }
    b_pon(b, ">", 1);
    if (es_vacio(n)) return;
    for (h = NI(n)->hijo; h; h = h->sig) serializa(b, (dom_node *)h, es_guion(n));
    b_pon(b, "</", 2); b_pon(b, nom, l); b_pon(b, ">", 1);
    return;
  }
  default:
    for (h = NI(n)->hijo; h; h = h->sig) serializa(b, (dom_node *)h, 0);
  }
}
FN(f_html) {
  Buf b = { 0 };
  NodoI *h;
  JSValue r;
  USA; NODO(0, n);
  if (argc > 1 && JS_ToBool(c, a[1])) serializa(&b, n, 0);
  else for (h = NI(n)->hijo; h; h = h->sig) serializa(&b, (dom_node *)h, es_guion(n));
  r = JS_NewStringLen(c, b.b ? b.b : "", b.n);
  free(b.b);
  return r;
}
/* innerHTML = : con el lector de fragmentos de libdom (hubbub); lo de dentro
 * de una tabla o de una lista de opciones se lee dentro de las suyas */
static dom_node *busca_etq(dom_node *n, const char *e) {
  NodoI *h;
  if (etq(n, e)) return n;
  for (h = NI(n)->hijo; h; h = h->sig) { dom_node *r = busca_etq((dom_node *)h, e); if (r) return r; }
  return NULL;
}
static int pon_html(dom_node *el, const char *s, size_t n) {
  dom_hubbub_parser_params pp;
  dom_hubbub_parser *parser = NULL;
  dom_document_fragment *frag = NULL;
  dom_node *de, *h;
  const char *pre = "", *post = "", *dentro_de = "body";
  char *todo;
  size_t lp, lq;
  if (etq(el, "tbody") || etq(el, "thead") || etq(el, "tfoot") || etq(el, "table")) { pre = "<table><tbody>"; post = "</tbody></table>"; dentro_de = "tbody"; }
  else if (etq(el, "tr")) { pre = "<table><tbody><tr>"; post = "</tr></tbody></table>"; dentro_de = "tr"; }
  else if (etq(el, "select") || etq(el, "optgroup") || etq(el, "datalist")) { pre = "<select>"; post = "</select>"; dentro_de = "select"; }
  lp = strlen(pre); lq = strlen(post);
  if (!(todo = malloc(lp + n + lq + 1))) return 0;
  memcpy(todo, pre, lp); memcpy(todo + lp, s, n); memcpy(todo + lp + n, post, lq);
  memset(&pp, 0, sizeof pp);
  pp.enc = "UTF-8"; pp.fix_enc = true; pp.enable_script = false;
  if (dom_hubbub_fragment_parser_create(&pp, H->doc, &parser, &frag) != DOM_HUBBUB_OK) { free(todo); return 0; }
  dom_hubbub_parser_parse_chunk(parser, (const uint8_t *)todo, lp + n + lq);
  dom_hubbub_parser_completed(parser);
  free(todo);
  /* fuera lo de antes */
  suelta_hijos(el);
  while ((h = (dom_node *)NI(el)->hijo)) { dom_node *r = NULL; dom_node_remove_child(el, h, &r); if (r) dom_node_unref(r); }
  de = frag ? busca_etq((dom_node *)frag, dentro_de) : NULL;
  if (de && !strcmp(dentro_de, "body")) {
    /* (el lector de hubbub pone <html><body>: lo de dentro del body) */
  }
  if (de) while ((h = (dom_node *)NI(de)->hijo)) {
    dom_node *r = NULL, *q = NULL;
    dom_node_remove_child(de, h, &q);
    if (q) { dom_node_append_child(el, q, &r); if (r) dom_node_unref(r); dom_node_unref(q); }
    else break;
  }
  dom_hubbub_parser_destroy(parser);
  if (frag) dom_node_unref(frag);
  return 1;
}
FN(f_pon_html) {
  size_t m;
  const char *u;
  USA; NODO(0, n);
  if (!ES_ELEM(n)) return JS_UNDEFINED;
  if (!(u = JS_ToCStringLen(c, &m, a[1]))) return JS_EXCEPTION;
  if (es_guion(n) || etq(n, "textarea") || etq(n, "title")) {
    dom_string *s = NULL;
    dom_string_create((const uint8_t *)u, m, &s);
    if (s) { suelta_hijos(n); dom_node_set_text_content(n, s); dom_string_unref(s); }
  } else pon_html(n, u, m);
  JS_FreeCString(c, u);
  ensucia();
  return JS_UNDEFINED;
}
#define BUSCA_MAX 8192
static dom_node *encontrados[BUSCA_MAX];
FN(f_busca) {
  const char *sel;
  int k, m, uno;
  JSValue r;
  USA; NODO(0, n);
  uno = argc > 2 && JS_ToBool(c, a[2]);
  if (!(sel = JS_ToCString(c, a[1]))) return JS_EXCEPTION;
  m = (NI(n)->tipo == DOM_ELEMENT_NODE || NI(n)->tipo == DOM_DOCUMENT_NODE || NI(n)->tipo == DOM_DOCUMENT_FRAGMENT_NODE)
      ? sel_busca(n, sel, encontrados, uno ? 1 : BUSCA_MAX) : 0;
  if (m < 0) { r = JS_ThrowSyntaxError(c, "SyntaxError: '%s' no es un selector valido", sel); JS_FreeCString(c, sel); return r; }
  JS_FreeCString(c, sel);
  if (uno) return m ? envuelve(encontrados[0]) : JS_NULL;
  r = JS_NewArray(c);
  for (k = 0; k < m; k++) JS_SetPropertyUint32(c, r, k, envuelve(encontrados[k]));
  return r;
}
FN(f_casa) {
  const char *sel;
  int m;
  JSValue r;
  USA; NODO(0, n);
  if (!(sel = JS_ToCString(c, a[1]))) return JS_EXCEPTION;
  m = ES_ELEM(n) ? sel_casa(n, sel, NULL) : 0;
  if (m < 0) { r = JS_ThrowSyntaxError(c, "SyntaxError: '%s' no es un selector valido", sel); JS_FreeCString(c, sel); return r; }
  JS_FreeCString(c, sel);
  return JS_NewBool(c, m);
}
FN(f_por_id) {
  dom_string *s;
  dom_element *e = NULL;
  JSValue r;
  USA;
  if (!(s = ds_de(a[0]))) return JS_EXCEPTION;
  if (dom_string_byte_length(s)) dom_document_get_element_by_id(H->doc, s, &e);
  dom_string_unref(s);
  r = envuelve((dom_node *)e);
  if (e) dom_node_unref(e);
  return r;
}
FN(f_doc) { USA; (void)a; return envuelve((dom_node *)H->doc); }
FN(f_cuerpo) {
  dom_html_element *b = NULL;
  JSValue r;
  USA; (void)a;
  dom_html_document_get_body((dom_html_document *)H->doc, &b);
  r = envuelve((dom_node *)b);
  if (b) dom_node_unref(b);
  return r;
}
FN(f_prototipos) {
  USA;
  H->proto_el = JS_DupValue(c, a[0]); H->proto_tx = JS_DupValue(c, a[1]); H->proto_doc = JS_DupValue(c, a[2]);
  return JS_UNDEFINED;
}
FN(f_escucha) { USA; (void)a; return JS_UNDEFINED; }
/* ¿no se ve? (no tiene caja: él o alguno de arriba con display: none, o
 * fuera del árbol; con las cajas de la última maqueta) */
static void al_dia(void);
FN(f_oculto) {
  USA; NODO(0, n);
  if (!conectado(n)) return JS_TRUE;
  al_dia();
  if (!H->html || !H->html->layout) return JS_FALSE;
  return JS_NewBool(c, box_for_node(n) == NULL);
}
FN(f_abierto) { dom_string *v; USA; NODO(0, n); v = sel_atr(n, "open"); if (v) dom_string_unref(v); return JS_NewBool(c, v != NULL); }
FN(f_pon_abierto) {
  dom_string *o = NULL;
  USA; NODO(0, n);
  dom_string_create((const uint8_t *)"open", 4, &o);
  if (!o) return JS_UNDEFINED;
  if (JS_ToBool(c, a[1])) { dom_string *v = NULL; dom_string_create((const uint8_t *)"", 0, &v); if (v) { dom_element_set_attribute(n, o, v); dom_string_unref(v); } }
  else dom_element_remove_attribute(n, o);
  dom_string_unref(o);
  ensucia();
  return JS_UNDEFINED;
}
/* ---- value, checked, selectedIndex ---- */
static dom_node *select_de(dom_node *o) {
  NodoI *p;
  for (p = NI(o)->padre; p && p->tipo == DOM_ELEMENT_NODE; p = p->padre) {
    if (etq((dom_node *)p, "select")) return (dom_node *)p;
    if (!etq((dom_node *)p, "optgroup")) return NULL;
  }
  return NULL;
}
static dom_string *valor_opcion(dom_node *o) {
  dom_string *v = sel_atr(o, "value"), *t = NULL;
  if (v) return v;
  dom_node_get_text_content(o, &t);
  if (t) {
    /* (sin los blancos de los lados) */
    const char *s = dom_string_data(t);
    size_t l = dom_string_byte_length(t), i = 0;
    while (i < l && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) i++;
    while (l > i && (s[l - 1] == ' ' || s[l - 1] == '\n' || s[l - 1] == '\t' || s[l - 1] == '\r')) l--;
    dom_string_create((const uint8_t *)s + i, l - i, &v);
    dom_string_unref(t);
  }
  return v;
}
static int opciones(dom_node *s, dom_node **o, int max) { return sel_busca(s, "option", o, max); }
static dom_node *opc[512];
static int elegida(dom_node *s) {
  int m = opciones(s, opc, 512), k;
  for (k = 0; k < m; k++) { bool si = false; dom_html_option_element_get_selected((dom_html_option_element *)opc[k], &si); if (si) return k; }
  return m && !sel_atr(s, "multiple") ? 0 : -1;
}
/* el control de NetSurf de una lista, como dicen las <option> del árbol */
static void lista_al_dia(dom_node *s) {
  struct form_control *k = ctl_de(s);
  struct form_option *o;
  if (!k || k->type != GADGET_SELECT) return;
  k->data.select.num_selected = 0;
  for (o = k->data.select.items; o; o = o->next) {
    bool si = false;
    if (o->node) dom_html_option_element_get_selected((dom_html_option_element *)o->node, &si);
    o->selected = si;
    if (si) { k->data.select.num_selected++; k->data.select.current = o; }
  }
  repinta(k);
}
FN(f_valor) {
  struct form_control *k;
  dom_string *v = NULL;
  JSValue r;
  USA; NODO(0, n);
  if (!ES_ELEM(n)) return JS_NewString(c, "");
  if (etq(n, "option")) { v = valor_opcion(n); r = a_js(v); if (v) dom_string_unref(v); return r; }
  if (etq(n, "select")) {
    int i = elegida(n);
    if (i < 0) return JS_NewString(c, "");
    v = valor_opcion(opc[i]); r = a_js(v); if (v) dom_string_unref(v); return r;
  }
  if (etq(n, "input")) {
    dom_string *t = sel_atr(n, "type");
    int caja = t && (!strcasecmp(dom_string_data(t), "checkbox") || !strcasecmp(dom_string_data(t), "radio"));
    if (t) dom_string_unref(t);
    if (caja) { v = sel_atr(n, "value"); r = v ? a_js(v) : JS_NewString(c, "on"); if (v) dom_string_unref(v); return r; }
    if ((k = ctl_de(n))) form_gadget_sync_with_dom(k);
    dom_html_input_element_get_value((dom_html_input_element *)n, &v);
  } else if (etq(n, "textarea")) {
    if ((k = ctl_de(n))) form_gadget_sync_with_dom(k);
    dom_html_text_area_element_get_value((dom_html_text_area_element *)n, &v);
  } else v = sel_atr(n, "value");
  r = a_js(v);
  if (v) dom_string_unref(v);
  return r;
}
FN(f_pon_valor) {
  struct form_control *k;
  dom_string *v;
  USA; NODO(0, n);
  if (!ES_ELEM(n)) return JS_UNDEFINED;
  if (!(v = ds_de(a[1]))) return JS_EXCEPTION;
  if (etq(n, "select")) {
    int m = opciones(n, opc, 512), j, hecho = 0;
    for (j = 0; j < m; j++) {
      dom_string *w = valor_opcion(opc[j]);
      bool si = !hecho && w && dom_string_isequal(w, v);
      if (si) hecho = 1;
      dom_html_option_element_set_selected((dom_html_option_element *)opc[j], si);
      if (w) dom_string_unref(w);
    }
    lista_al_dia(n);
  } else if (etq(n, "input") || etq(n, "textarea")) {
    if (etq(n, "input")) dom_html_input_element_set_value((dom_html_input_element *)n, v);
    else dom_html_text_area_element_set_value((dom_html_text_area_element *)n, v);
    if ((k = ctl_de(n))) { form_gadget_sync_with_dom(k); repinta(k); }
  } else {
    dom_string *nom = NULL;
    dom_string_create((const uint8_t *)"value", 5, &nom);
    if (nom) { dom_element_set_attribute(n, nom, v); dom_string_unref(nom); }
  }
  dom_string_unref(v);
  return JS_UNDEFINED;
}
FN(f_marcado) {
  bool si = false;
  USA; NODO(0, n);
  if (etq(n, "option")) dom_html_option_element_get_selected((dom_html_option_element *)n, &si);
  else if (etq(n, "input")) dom_html_input_element_get_checked((dom_html_input_element *)n, &si);
  return JS_NewBool(c, si);
}
FN(f_pon_marcado) {
  struct form_control *k;
  int si;
  USA; NODO(0, n);
  si = JS_ToBool(c, a[1]);
  if (etq(n, "option")) {
    dom_node *s = select_de(n);
    if (si && s && !sel_atr(s, "multiple")) { int m = opciones(s, opc, 512), j; for (j = 0; j < m; j++) dom_html_option_element_set_selected((dom_html_option_element *)opc[j], false); }
    dom_html_option_element_set_selected((dom_html_option_element *)n, si);
    if (s) lista_al_dia(s);
  } else if (etq(n, "input")) {
    dom_html_input_element_set_checked((dom_html_input_element *)n, si);
    if ((k = ctl_de(n))) {
      if (si && k->type == GADGET_RADIO) form_radio_set(k);
      else k->selected = si;
      repinta(k);
    }
  }
  return JS_UNDEFINED;
}
FN(f_elegida) { USA; NODO(0, n); return JS_NewInt32(c, etq(n, "select") ? elegida(n) : -1); }
FN(f_pon_elegida) {
  int i = 0, m, j;
  USA; NODO(0, n);
  JS_ToInt32(c, &i, a[1]);
  if (!etq(n, "select")) return JS_UNDEFINED;
  m = opciones(n, opc, 512);
  for (j = 0; j < m; j++) dom_html_option_element_set_selected((dom_html_option_element *)opc[j], j == i);
  lista_al_dia(n);
  return JS_UNDEFINED;
}

/* ---- lo de la ventana ---- */
static void reinicia_tope(void) { H->limite = GetTickCount() + (u32)guion_tope_ms; }
static void diferido_tic(void *p);
static void difiere(int que, dom_node *n, dom_node *otro, const char *texto, int k) {
  Diferido *d = calloc(1, sizeof *d), **q;
  if (!d) return;
  d->que = que; d->k = k;
  if (n) d->n = dom_node_ref(n);
  if (otro) d->otro = dom_node_ref(otro);
  if (texto) d->texto = strdup(texto);
  for (q = &H->dif; *q; q = &(*q)->sig) ;
  *q = d;
  framebuffer_schedule(0, diferido_tic, H);
}
FN(f_envia) { USA; NODO(0, f); difiere(D_ENVIA, f, argc > 1 ? nodo_de(a[1]) : NULL, NULL, 0); return JS_UNDEFINED; }
/* elemento.focus(): el cursor a su campo */
FN(f_foco) {
  struct form_control *k;
  dom_node *n = argc > 0 ? nodo_de(a[0]) : NULL;
  USA;
  if (n && (k = ctl_de(n)) && (k->type == GADGET_TEXTBOX || k->type == GADGET_PASSWORD || k->type == GADGET_TEXTAREA)
      && k->data.text.ta && k->box)
    textarea_set_caret(k->data.text.ta, textarea_get_text(k->data.text.ta, NULL, 0));
  if (JS_IsFunction(c, H->fn_foco)) {
    JSValue el = n ? envuelve(n) : JS_NULL;
    JSValue r = JS_Call(c, H->fn_foco, JS_UNDEFINED, 1, (JSValueConst *)&el);
    JS_FreeValue(c, r);
    JS_FreeValue(c, el);
  }
  return JS_UNDEFINED;
}
FN(f_a_la_vista) {
  struct box *b;
  int x, y;
  USA; NODO(0, n);
  if (!H->html || !H->html->layout || !(b = box_for_node(n))) return JS_UNDEFINED;
  box_coords(b, &x, &y);
  surf_ve_a(0, y - 8);
  return JS_UNDEFINED;
}
/* UTF-8 -> 1252, para los cuadros de Hispa Ventanas */
static void a_1252(const char *u, char *b, size_t max) {
  char *r = NULL;
  if (utf8_to_enc(u, "WINDOWS-1252", 0, &r) == NSERROR_OK && r) { snprintf(b, max, "%s", r); free(r); }
  else snprintf(b, max, "%s", u);
}
FN(f_cuadro) {
  int tipo = 0, r;
  const char *t;
  char b[1200];
  USA;
  JS_ToInt32(c, &tipo, a[0]);
  if (!(t = JS_ToCString(c, a[1]))) return JS_EXCEPTION;
  a_1252(t, b, sizeof b);
  JS_FreeCString(c, t);
  reg_vacia();
  r = MessageBoxA(surf_ventana(), b, "La p\341gina dice", tipo ? 0x21 : 0x40);
  reinicia_tope();
  return JS_NewBool(c, tipo ? r == 1 : 1);
}
FN(f_pregunta) {
  const char *t, *d;
  char b[1400], x[1200];
  int r;
  JSValue v;
  USA;
  if (!(t = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  if (!(d = JS_ToCString(c, a[1]))) { JS_FreeCString(c, t); return JS_EXCEPTION; }
  snprintf(x, sizeof x, "%s\n\nRespuesta: %s", t, d);
  a_1252(x, b, sizeof b);
  r = MessageBoxA(surf_ventana(), b, "La p\341gina pregunta", 0x21);
  reinicia_tope();
  v = r == 1 ? JS_NewString(c, d) : JS_NULL;
  JS_FreeCString(c, t); JS_FreeCString(c, d);
  return v;
}
FN(f_titulo) { USA; (void)a; return JS_UNDEFINED; }
static const char *url_pagina(void) {
  if (H->url_js) return H->url_js;
  return H->html ? nsurl_access(content_get_url(&H->html->base)) : "about:blank";
}
FN(f_url) { USA; (void)a; return JS_NewString(c, url_pagina()); }
FN(f_pon_url) {
  const char *t;
  USA;
  if ((t = JS_ToCString(c, a[0]))) {
    free(H->url_js);
    H->url_js = strdup(t);
    surf_pon_dir(t);
    JS_FreeCString(c, t);
  }
  return JS_UNDEFINED;
}
static JSValue une(const char *base, const char *rel) {
  nsurl *b = NULL, *u = NULL;
  JSValue r = JS_NULL;
  if (base) { if (nsurl_create(base, &b) != NSERROR_OK) return JS_NULL; }
  else if (H->html && H->html->base_url) b = nsurl_ref(H->html->base_url);
  else if (nsurl_create(url_pagina(), &b) != NSERROR_OK) return JS_NULL;
  if (H->url_js && !base) { nsurl_unref(b); if (nsurl_create(H->url_js, &b) != NSERROR_OK) return JS_NULL; }
  if (nsurl_join(b, rel, &u) == NSERROR_OK) { r = JS_NewString(cx, nsurl_access(u)); nsurl_unref(u); }
  nsurl_unref(b);
  return r;
}
FN(f_absoluta) {
  const char *t;
  JSValue r;
  USA;
  if (!(t = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  r = une(NULL, t);
  if (JS_IsNull(r)) r = JS_NewString(c, t);
  JS_FreeCString(c, t);
  return r;
}
FN(f_absoluta_de) {
  const char *b, *t;
  JSValue r;
  USA;
  if (!(b = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  if (!(t = JS_ToCString(c, a[1]))) { JS_FreeCString(c, b); return JS_EXCEPTION; }
  r = une(b, t);
  JS_FreeCString(c, b); JS_FreeCString(c, t);
  return r;
}
FN(f_ir) {
  const char *t;
  USA;
  if (!(t = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  if (strncasecmp(t, "javascript:", 11)) difiere(D_IR, NULL, NULL, t, argc > 1 && JS_ToBool(c, a[1]));
  JS_FreeCString(c, t);
  return JS_UNDEFINED;
}
FN(f_historia) { int k = 0; USA; JS_ToInt32(c, &k, a[0]); difiere(D_HISTORIA, NULL, NULL, NULL, k); return JS_UNDEFINED; }
FN(f_galletas) { char *g; JSValue r; USA; (void)a; g = rh_galletas_de(url_pagina()); r = JS_NewString(c, g ? g : ""); free(g); return r; }
FN(f_pon_galleta) { const char *t; USA; if ((t = JS_ToCString(c, a[0]))) { rh_pon_galleta(url_pagina(), t); JS_FreeCString(c, t); } return JS_UNDEFINED; }
static void medidas(int *w, int *h) {
  *w = 784; *h = 500;
  if (H->bw) browser_window_get_dimensions(H->bw, w, h);
}
FN(f_ancho) { int w, h; USA; (void)a; medidas(&w, &h); return JS_NewInt32(c, w); }
FN(f_alto) { int w, h; USA; (void)a; medidas(&w, &h); return JS_NewInt32(c, h); }
FN(f_ahora) { USA; (void)a; return JS_NewFloat64(c, (double)(GetTickCount() - H->t_pagina)); }
FN(f_agente) { USA; (void)a; return JS_NewString(c, AGENTE); }
FN(f_idioma) { USA; (void)a; return JS_NewString(c, "es-ES"); }
FN(f_registro) {
  int niv = 0;
  size_t n;
  const char *t;
  static const char *const pre[] = { "[log] ", "[log] ", "[aviso] ", "[error] " };
  USA;
  JS_ToInt32(c, &niv, a[0]);
  if (!(t = JS_ToCStringLen(c, &n, a[1]))) return JS_EXCEPTION;
  reg(pre[niv & 3], t, n);
  JS_FreeCString(c, t);
  return JS_UNDEFINED;
}

/* las medidas piden las cajas al día: si el árbol ha cambiado en este turno,
 * se rehacen ya (como un navegador de los grandes cuando se lee offsetHeight) */
static void al_dia(void) {
  if (H->sucio && H->html && rehaz_listo(H->html)) { H->sucio = 0; rehaz_cajas(H->html); }
}
/* ---- las medidas de verdad (surf.js): la caja de NetSurf de un elemento,
 * [x, y, ancho, alto] del borde para fuera, en la página ---- */
FN(f_caja) {
  struct box *b;
  int x, y;
  JSValue r;
  USA; NODO(0, n);
  al_dia();
  if (!conectado(n) || !H->html || !H->html->layout || !(b = box_for_node(n))) return JS_NULL;
  box_coords(b, &x, &y);
  r = JS_NewArray(c);
  JS_SetPropertyUint32(c, r, 0, JS_NewInt32(c, x - b->border[LEFT].width));
  JS_SetPropertyUint32(c, r, 1, JS_NewInt32(c, y - b->border[TOP].width));
  JS_SetPropertyUint32(c, r, 2, JS_NewInt32(c, b->width + b->padding[LEFT] + b->padding[RIGHT] + b->border[LEFT].width + b->border[RIGHT].width));
  JS_SetPropertyUint32(c, r, 3, JS_NewInt32(c, b->height + b->padding[TOP] + b->padding[BOTTOM] + b->border[TOP].width + b->border[BOTTOM].width));
  JS_SetPropertyUint32(c, r, 4, JS_NewInt32(c, b->padding[LEFT] + b->width + b->padding[RIGHT]));
  JS_SetPropertyUint32(c, r, 5, JS_NewInt32(c, b->padding[TOP] + b->height + b->padding[BOTTOM]));
  JS_SetPropertyUint32(c, r, 6, JS_NewInt32(c, b->descendant_y1 - b->descendant_y0));
  return r;
}
/* getComputedStyle: lo que dice el estilo de la caja (lo que se pida y se sepa) */
static JSValue px(int v) { char b[24]; sprintf(b, "%dpx", v); return JS_NewString(cx, b); }
static JSValue color(css_color k) { char b[48]; if (!(k >> 24)) return JS_NewString(cx, "rgba(0, 0, 0, 0)"); sprintf(b, "rgb(%u, %u, %u)", (k >> 16) & 255, (k >> 8) & 255, k & 255); return JS_NewString(cx, b); }
FN(f_estilo) {
  struct box *b;
  const char *p;
  const css_computed_style *s;
  JSValue r = JS_NULL;
  USA; NODO(0, n);
  al_dia();
  if (!conectado(n) || !H->html || !H->html->layout || !(b = box_for_node(n)) || !(s = b->style)) return JS_NULL;
  if (!(p = JS_ToCString(c, a[1]))) return JS_EXCEPTION;
  if (!strcmp(p, "display")) {
    static const char *const d[] = { "inherit", "inline", "block", "list-item", "run-in", "inline-block", "table", "inline-table",
      "table-row-group", "table-header-group", "table-footer-group", "table-row", "table-column-group", "table-column", "table-cell",
      "table-caption", "none", "flex", "inline-flex", "grid", "inline-grid" };
    unsigned k = css_computed_display(s, false);
    r = JS_NewString(c, k < sizeof d / sizeof d[0] ? d[k] : "block");
  } else if (!strcmp(p, "visibility")) r = JS_NewString(c, css_computed_visibility(s) == CSS_VISIBILITY_HIDDEN ? "hidden" : css_computed_visibility(s) == CSS_VISIBILITY_COLLAPSE ? "collapse" : "visible");
  else if (!strcmp(p, "position")) {
    unsigned k = css_computed_position(s);
    r = JS_NewString(c, k == CSS_POSITION_RELATIVE ? "relative" : k == CSS_POSITION_ABSOLUTE ? "absolute" : k == CSS_POSITION_FIXED ? "fixed" : "static");
  } else if (!strcmp(p, "float")) { unsigned k = css_computed_float(s); r = JS_NewString(c, k == CSS_FLOAT_LEFT ? "left" : k == CSS_FLOAT_RIGHT ? "right" : "none"); }
  else if (!strcmp(p, "overflow") || !strcmp(p, "overflow-y") || !strcmp(p, "overflow-x")) {
    unsigned k = p[8] == 'x' ? css_computed_overflow_x(s) : css_computed_overflow_y(s);
    r = JS_NewString(c, k == CSS_OVERFLOW_HIDDEN ? "hidden" : k == CSS_OVERFLOW_SCROLL ? "scroll" : k == CSS_OVERFLOW_AUTO ? "auto" : "visible");
  } else if (!strcmp(p, "width")) r = px(b->width);
  else if (!strcmp(p, "height")) r = px(b->height);
  else if (!strncmp(p, "padding-", 8) || !strncmp(p, "margin-", 7) || !strncmp(p, "border-", 7)) {
    const char *l = strchr(p, '-') + 1;
    int lado = !strncmp(l, "top", 3) ? TOP : !strncmp(l, "right", 5) ? RIGHT : !strncmp(l, "bottom", 6) ? BOTTOM : LEFT;
    if (p[0] == 'p') r = px(b->padding[lado]);
    else if (p[0] == 'm') r = px(b->margin[lado]);
    else if (strstr(p, "width")) r = px(b->border[lado].width);
  } else if (!strcmp(p, "color")) { css_color k = 0; css_computed_color(s, &k); r = color(k); }
  else if (!strcmp(p, "background-color")) { css_color k = 0; css_computed_background_color(s, &k); r = color(k); }
  else if (!strcmp(p, "z-index")) { int32_t z = 0; r = css_computed_z_index(s, &z) == CSS_Z_INDEX_SET ? JS_NewInt32(c, FIXTOINT(z)) : JS_NewString(c, "auto"); }
  else if (!strcmp(p, "opacity")) { css_fixed o = 0; css_computed_opacity(s, &o); r = JS_NewFloat64(c, FIXTOFLT(o)); }
  JS_FreeCString(c, p);
  return r;
}
FN(f_desp) {
  int x, y;
  JSValue r = JS_NewArray(c);
  USA; (void)a;
  surf_desp(&x, &y);
  JS_SetPropertyUint32(c, r, 0, JS_NewInt32(c, x));
  JS_SetPropertyUint32(c, r, 1, JS_NewInt32(c, y));
  return r;
}
FN(f_ve_a) { int x = 0, y = 0; USA; JS_ToInt32(c, &x, a[0]); JS_ToInt32(c, &y, a[1]); surf_ve_a(x, y); return JS_UNDEFINED; }
FN(f_alto_pagina) {
  int w = 0, h = 0;
  USA; (void)a;
  al_dia();
  if (H->html && H->html->layout) { w = H->html->layout->descendant_x1; h = H->html->layout->descendant_y1; }
  return JS_NewInt32(c, h > 0 ? h : 0);
  (void)w;
}

/* ---- el almacén: localStorage en NAVEGA2.ALM («L origen datos» por línea);
 * sessionStorage, en la memoria ---- */
static char alm_ruta[300];
#define SES_MAX 8
static struct { char *origen, *datos; } ses[SES_MAX];
static char *alm_lee(const char *origen) {
  char *l = malloc(65536), *r = NULL;
  FILE *f;
  u32 lo = strlen(origen);
  if (!l) return NULL;
  if ((f = fopen(alm_ruta, "rb"))) {
    while (fgets(l, 65536, f))
      if (l[0] == 'L' && l[1] == ' ' && !strncmp(l + 2, origen, lo) && l[2 + lo] == ' ') {
        char *d = l + 3 + lo, *fin = d + strlen(d);
        while (fin > d && (fin[-1] == '\n' || fin[-1] == '\r')) *--fin = 0;
        r = strdup(d);
        break;
      }
    fclose(f);
  }
  free(l);
  return r;
}
static void alm_guarda(const char *origen, const char *datos) {
  char *l = malloc(65536), *todo = NULL;
  u32 n = 0, lo = strlen(origen), cap = 0;
  FILE *f;
  if (!l) return;
  if ((f = fopen(alm_ruta, "rb"))) {
    while (fgets(l, 65536, f)) {
      u32 ll = strlen(l);
      if (l[0] == 'L' && l[1] == ' ' && !strncmp(l + 2, origen, lo) && l[2 + lo] == ' ') continue;
      if (n + ll + 1 > cap) { char *q = realloc(todo, cap = (cap + ll) * 2 + 1024); if (!q) break; todo = q; }
      memcpy(todo + n, l, ll); n += ll;
    }
    fclose(f);
  }
  if ((f = fopen(alm_ruta, "wb"))) {
    if (n) fwrite(todo, 1, n, f);
    if (strcmp(datos, "{}")) { fwrite("L ", 1, 2, f); fwrite(origen, 1, lo, f); fwrite(" ", 1, 1, f); fwrite(datos, 1, strlen(datos), f); fwrite("\r\n", 1, 2, f); }
    fclose(f);
  }
  free(todo); free(l);
}
FN(f_almacen) {
  const char *o;
  char *d = NULL;
  JSValue r;
  int k;
  USA;
  if (!(o = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  if (JS_ToBool(c, a[1])) d = alm_lee(o);
  else for (k = 0; k < SES_MAX; k++) if (ses[k].origen && !strcmp(ses[k].origen, o)) d = strdup(ses[k].datos ? ses[k].datos : "");
  JS_FreeCString(c, o);
  r = JS_NewString(c, d ? d : "");
  free(d);
  return r;
}
FN(f_guarda_almacen) {
  const char *o, *d;
  int k, libre = 0;
  USA;
  if (!(o = JS_ToCString(c, a[0]))) return JS_EXCEPTION;
  if (!(d = JS_ToCString(c, a[2]))) { JS_FreeCString(c, o); return JS_EXCEPTION; }
  if (strlen(d) > 60000) { JS_FreeCString(c, o); JS_FreeCString(c, d); return JS_ThrowRangeError(c, "QuotaExceededError: el almacen esta lleno"); }
  if (JS_ToBool(c, a[1])) alm_guarda(o, d);
  else {
    for (k = 0; k < SES_MAX && !(ses[k].origen && !strcmp(ses[k].origen, o)); k++) if (!ses[k].origen) libre = k;
    if (k == SES_MAX) { k = libre; free(ses[k].origen); free(ses[k].datos); ses[k].origen = strdup(o); ses[k].datos = NULL; }
    free(ses[k].datos);
    ses[k].datos = strdup(d);
  }
  JS_FreeCString(c, o); JS_FreeCString(c, d);
  return JS_UNDEFINED;
}

/* ---- los temporizadores: con el planificador de NetSurf, uno por cada uno ---- */
static void llama_tic(struct jsthread *t, int id);
static void temp_tic(void *p) {
  Temp *x = p, **q;
  struct jsthread *t = x->t;
  int id = x->id;
  for (q = &t->temps; *q; q = &(*q)->sig) if (*q == x) { *q = x->sig; break; }
  free(x);
  if (t->cerrado) return;
  /* (si NetSurf está haciendo las cajas, un poco después) */
  if (t->html && t->html->box_conversion_context) {
    Temp *y = calloc(1, sizeof *y);
    if (y) { y->t = t; y->id = id; y->sig = t->temps; t->temps = y; framebuffer_schedule(50, temp_tic, y); }
    return;
  }
  llama_tic(t, id);
}
FN(f_temporiza) {
  int id = 0;
  double ms = 0;
  Temp *x, *y;
  USA;
  JS_ToInt32(c, &id, a[0]);
  JS_ToFloat64(c, &ms, a[1]);
  if (ms > 86400000.0) ms = 86400000.0;
  if (ms < 0) ms = 0;
  for (x = H->temps; x && x->id != id; x = x->sig) ;
  if (x) framebuffer_schedule(-1, temp_tic, x);
  else {
    int k = 0;
    for (y = H->temps; y; y = y->sig) k++;
    if (k >= 256) return JS_ThrowRangeError(c, "demasiados temporizadores");
    if (!(x = calloc(1, sizeof *x))) return JS_ThrowOutOfMemory(c);
    x->t = H; x->id = id; x->sig = H->temps; H->temps = x;
  }
  framebuffer_schedule((int)ms, temp_tic, x);
  return JS_UNDEFINED;
}
FN(f_des_temporiza) {
  int id = 0;
  Temp **q, *x;
  USA;
  JS_ToInt32(c, &id, a[0]);
  for (q = &H->temps; *q; q = &(*q)->sig)
    if ((*q)->id == id) { x = *q; *q = x->sig; framebuffer_schedule(-1, temp_tic, x); free(x); break; }
  return JS_UNDEFINED;
}

/* ---- la red: fetch y XMLHttpRequest, por red.c; las redirecciones, aquí ---- */
typedef struct Pet { struct Pet *sig; struct jsthread *t; int id, redirs; char *metodo, *url, *cab, *cuerpo; u32 n_cuerpo; } Pet;
static Pet *pets;
static void pet_suelta(Pet *p) {
  Pet **q;
  for (q = &pets; *q; q = &(*q)->sig) if (*q == p) { *q = p->sig; break; }
  free(p->metodo); free(p->url); free(p->cab); free(p->cuerpo); free(p);
}
static void llega(void *ctx, int estado, const char *cab, const unsigned char *datos, unsigned n, const char *error);
static int pet_lanza(Pet *p) {
  return red_js_pide(p->url, p->metodo, p->cuerpo, p->n_cuerpo, p->cab, llega, p);
}
static void llama_llega(struct jsthread *t, int id, int estado, const char *url, const char *cab, const unsigned char *d, unsigned n, const char *err);
static void llega(void *ctx, int estado, const char *cab, const unsigned char *datos, unsigned n, const char *error) {
  Pet *p = ctx;
  struct jsthread *t = p->t;
  if (estado >= 301 && estado <= 308 && estado != 304 && estado != 305 && estado != 306 && p->redirs < 10) {
    /* una redirección: la Location, contra la dirección de ahora */
    const char *l = cab ? strcasestr(cab, "\nLocation:") : NULL;
    if (l) {
      char loc[1024];
      int k;
      nsurl *b = NULL, *u = NULL;
      l += 10;
      while (*l == ' ') l++;
      for (k = 0; l[k] && l[k] != '\r' && l[k] != '\n' && k < 1022; k++) loc[k] = l[k];
      loc[k] = 0;
      if (nsurl_create(p->url, &b) == NSERROR_OK && nsurl_join(b, loc, &u) == NSERROR_OK) {
        free(p->url);
        p->url = strdup(nsurl_access(u));
        p->redirs++;
        if (estado != 307 && estado != 308) { free(p->metodo); p->metodo = strdup("GET"); free(p->cuerpo); p->cuerpo = NULL; p->n_cuerpo = 0; }
        nsurl_unref(u); nsurl_unref(b);
        if (p->url && pet_lanza(p)) return;
      } else if (b) nsurl_unref(b);
    }
  }
  if (!t->cerrado) llama_llega(t, p->id, estado, p->url, cab, datos, n, error);
  pet_suelta(p);
}
FN(f_pide) {
  int id = 0;
  const char *m, *u, *h, *b = NULL;
  size_t nb = 0;
  Pet *p;
  USA;
  JS_ToInt32(c, &id, a[0]);
  m = JS_ToCString(c, a[1]); u = JS_ToCString(c, a[2]); h = JS_ToCString(c, a[3]);
  if (argc > 4 && !JS_IsNull(a[4]) && !JS_IsUndefined(a[4])) b = JS_ToCStringLen(c, &nb, a[4]);
  if (m && u && h && (p = calloc(1, sizeof *p))) {
    p->t = H; p->id = id;
    p->metodo = strdup(m); p->url = strdup(u); p->cab = strdup(h);
    if (b) { p->cuerpo = malloc(nb + 1); if (p->cuerpo) { memcpy(p->cuerpo, b, nb); p->n_cuerpo = nb; } }
    p->sig = pets; pets = p;
    if (!pet_lanza(p)) { pet_suelta(p); difiere(D_EVENTO, NULL, NULL, "red", id); }
  }
  if (m) JS_FreeCString(c, m);
  if (u) JS_FreeCString(c, u);
  if (h) JS_FreeCString(c, h);
  if (b) JS_FreeCString(c, b);
  return JS_UNDEFINED;
}
FN(f_cancela) { USA; (void)a; return JS_UNDEFINED; }

#ifdef PERFIL
static void perfil_dice(void);
static inline unsigned long long rdtsc(void) { unsigned long long x; __asm__ volatile("rdtsc" : "=A"(x)); return x; }
#endif
static const JSCFunctionListEntry funciones_n[] = {
  JS_CFUNC_DEF("es", 1, f_es), JS_CFUNC_DEF("tipo", 1, f_tipo), JS_CFUNC_DEF("padre", 1, f_padre),
  JS_CFUNC_DEF("hijo", 1, f_hijo), JS_CFUNC_DEF("ult", 1, f_ult), JS_CFUNC_DEF("sig", 1, f_sig), JS_CFUNC_DEF("ant", 1, f_ant),
  JS_CFUNC_DEF("conectado", 1, f_conectado), JS_CFUNC_DEF("dentro", 2, f_dentro), JS_CFUNC_DEF("nombre", 1, f_nombre),
  JS_CFUNC_DEF("texto", 1, f_texto), JS_CFUNC_DEF("ponTexto", 2, f_pon_texto), JS_CFUNC_DEF("contenido", 1, f_contenido),
  JS_CFUNC_DEF("ponContenido", 2, f_pon_contenido), JS_CFUNC_DEF("inserta", 3, f_inserta), JS_CFUNC_DEF("quita", 1, f_quita),
  JS_CFUNC_DEF("clona", 2, f_clona), JS_CFUNC_DEF("creaTexto", 1, f_crea_texto), JS_CFUNC_DEF("crea", 1, f_crea),
  JS_CFUNC_DEF("atr", 2, f_atr), JS_CFUNC_DEF("atrs", 1, f_atrs), JS_CFUNC_DEF("ponAtr", 4, f_pon_atr),
  JS_CFUNC_DEF("quitaAtr", 2, f_quita_atr), JS_CFUNC_DEF("html", 2, f_html), JS_CFUNC_DEF("ponHtml", 2, f_pon_html),
  JS_CFUNC_DEF("busca", 3, f_busca), JS_CFUNC_DEF("casa", 2, f_casa), JS_CFUNC_DEF("porId", 1, f_por_id),
  JS_CFUNC_DEF("doc", 0, f_doc), JS_CFUNC_DEF("cuerpo", 0, f_cuerpo), JS_CFUNC_DEF("prototipos", 3, f_prototipos),
  JS_CFUNC_DEF("escucha", 2, f_escucha), JS_CFUNC_DEF("oculto", 1, f_oculto), JS_CFUNC_DEF("abierto", 1, f_abierto),
  JS_CFUNC_DEF("ponAbierto", 2, f_pon_abierto), JS_CFUNC_DEF("valor", 1, f_valor), JS_CFUNC_DEF("ponValor", 2, f_pon_valor),
  JS_CFUNC_DEF("marcado", 1, f_marcado), JS_CFUNC_DEF("ponMarcado", 2, f_pon_marcado), JS_CFUNC_DEF("elegida", 1, f_elegida),
  JS_CFUNC_DEF("ponElegida", 2, f_pon_elegida), JS_CFUNC_DEF("envia", 2, f_envia), JS_CFUNC_DEF("foco", 1, f_foco),
  JS_CFUNC_DEF("aLaVista", 1, f_a_la_vista), JS_CFUNC_DEF("cuadro", 2, f_cuadro), JS_CFUNC_DEF("pregunta", 2, f_pregunta),
  JS_CFUNC_DEF("titulo", 1, f_titulo), JS_CFUNC_DEF("url", 0, f_url), JS_CFUNC_DEF("ponUrl", 1, f_pon_url),
  JS_CFUNC_DEF("absoluta", 1, f_absoluta), JS_CFUNC_DEF("absolutaDe", 2, f_absoluta_de), JS_CFUNC_DEF("ir", 2, f_ir),
  JS_CFUNC_DEF("historia", 1, f_historia), JS_CFUNC_DEF("galletas", 0, f_galletas), JS_CFUNC_DEF("ponGalleta", 1, f_pon_galleta),
  JS_CFUNC_DEF("ancho", 0, f_ancho), JS_CFUNC_DEF("alto", 0, f_alto), JS_CFUNC_DEF("ahora", 0, f_ahora),
  JS_CFUNC_DEF("idioma", 0, f_idioma), JS_CFUNC_DEF("agente", 0, f_agente), JS_CFUNC_DEF("registro", 2, f_registro), JS_CFUNC_DEF("almacen", 2, f_almacen),
  JS_CFUNC_DEF("guardaAlmacen", 3, f_guarda_almacen), JS_CFUNC_DEF("temporiza", 2, f_temporiza),
  JS_CFUNC_DEF("desTemporiza", 1, f_des_temporiza), JS_CFUNC_DEF("pide", 5, f_pide), JS_CFUNC_DEF("cancela", 1, f_cancela),
  /* (los de surf.js) */
  JS_CFUNC_DEF("caja", 1, f_caja), JS_CFUNC_DEF("estilo", 2, f_estilo), JS_CFUNC_DEF("desp", 0, f_desp),
  JS_CFUNC_DEF("veA", 2, f_ve_a), JS_CFUNC_DEF("altoPagina", 0, f_alto_pagina),
};

/* =============================================================================
 *  Los turnos: cada llamada desde fuera; al acabar, las promesas pendientes
 *  y, si el árbol ha cambiado, las cajas otra vez
 * ============================================================================= */
static int corta(JSRuntime *r, void *x) {
  struct jsthread *t = x;
  (void)r;
  return t->nivel && (int)(GetTickCount() - t->limite) > 0;
}
static void excepcion(void) {
  JSValue e = JS_GetException(cx);
  const char *t;
  if (JS_IsNull(e) || JS_IsUndefined(e)) return;
  if (JS_IsError(cx, e) && (t = JS_ToCString(cx, e))) {
    if (strstr(t, "interrupted")) {
      regf("[error] ", "un guion ha tardado mas de %d s y se ha parado", guion_tope_ms / 1000);
      surf_estado("Un guion de la p\303\241gina tardaba demasiado: se ha parado.");
    }
    JS_FreeCString(cx, t);
  }
  if (JS_IsFunction(cx, H->fn_informa)) {
    JSValue r = JS_Call(cx, H->fn_informa, JS_UNDEFINED, 1, (JSValueConst *)&e);
    if (JS_IsException(r)) { JSValue e2 = JS_GetException(cx); JS_FreeValue(cx, e2); }
    JS_FreeValue(cx, r);
  } else if ((t = JS_ToCString(cx, e))) { reg("[error] ", t, strlen(t)); JS_FreeCString(cx, t); }
  JS_FreeValue(cx, e);
}
static void fin_turno(void) {
  JSContext *c2;
  int e, k = 0;
  H->nivel++;
  reinicia_tope();
  while ((e = JS_ExecutePendingJob(H->rt, &c2)) != 0) { if (e < 0) excepcion(); if (++k > 200000) break; }
  H->nivel--;
  if (H->sucio && H->html) { H->sucio = 0; rehaz_pide(H->html); }
  reg_vacia();
}
static JSValue llama(JSValueConst f, int n, JSValueConst *args) {
  JSValue r;
  if (H->cerrado || !JS_IsFunction(cx, f)) return JS_UNDEFINED;
  if (!H->nivel++) reinicia_tope();
  r = JS_Call(cx, f, JS_UNDEFINED, n, args);
  if (JS_IsException(r)) { excepcion(); r = JS_UNDEFINED; }
  if (!--H->nivel) fin_turno();
  return r;
}
static void rechazo(JSContext *c, JSValueConst prom, JSValueConst razon, JS_BOOL atendida, void *x) {
  const char *t;
  (void)prom; (void)x;
  if (atendida) return;
  if ((t = JS_ToCString(c, razon))) { reg("[error] promesa rechazada sin recoger: ", t, strlen(t)); JS_FreeCString(c, t); }
}
static void llama_tic(struct jsthread *t, int id) {
  ENTRA(t);
  {
    JSValue v = JS_NewInt32(cx, id);
    JS_FreeValue(cx, llama(H->fn_tic, 1, &v));
  }
  SALE();
}
static void llama_llega(struct jsthread *t, int id, int estado, const char *url, const char *cab, const unsigned char *d, unsigned n, const char *err) {
  ENTRA(t);
  {
    JSValue args[5];
    int k;
    const char *ca = cab && cab[0] == '\n' ? cab + 1 : cab ? cab : "";
    args[0] = JS_NewInt32(cx, id);
    args[1] = JS_NewInt32(cx, estado);
    args[2] = JS_NewString(cx, url ? url : "");
    args[3] = JS_NewString(cx, ca);
    args[4] = estado ? JS_NewStringLen(cx, d ? (const char *)d : "", d ? n : 0) : JS_NewString(cx, err ? err : "error de la red");
    JS_FreeValue(cx, llama(H->fn_llega, 5, args));
    for (k = 0; k < 5; k++) JS_FreeValue(cx, args[k]);
  }
  SALE();
}

/* lo que no se puede hacer en medio de un guion (ir a otra página, enviar un
 * formulario): después, desde el planificador */
static struct form *forma_de(html_content *h, dom_node *n) {
  struct form *f;
  for (f = h->forms; f; f = f->prev) if (f->node == n) return f;
  return NULL;
}
static void diferido_tic(void *p) {
  struct jsthread *t = p;
  Diferido *d;
  while (!t->cerrado && (d = t->dif)) {
    t->dif = d->sig;
    if (d->que == D_IR && t->bw && d->texto) {
      nsurl *u = NULL;
      if (nsurl_create(d->texto, &u) == NSERROR_OK) {
        browser_window_navigate(t->bw, u, t->html ? content_get_url(&t->html->base) : NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
        nsurl_unref(u);
      }
    } else if (d->que == D_HISTORIA && t->bw) {
      if (d->k < 0) browser_window_history_back(t->bw, false);
      else if (d->k > 0) browser_window_history_forward(t->bw, false);
      else browser_window_reload(t->bw, false);
    } else if (d->que == D_ENVIA && t->html && t->bw) {
      struct form *f = forma_de(t->html, d->n);
      struct form_control *k = NULL;
      if (f && d->otro) for (k = f->controls; k && k->node != d->otro; k = k->next) ;
      if (f) {
        sin_evento_submit = 1;
        form_submit(content_get_url(&t->html->base), t->bw, f, k);
        sin_evento_submit = 0;
      }
    } else if (d->que == D_EVENTO && d->texto && !strcmp(d->texto, "red")) {
      llama_llega(t, d->k, 0, "", "", NULL, 0, "demasiadas peticiones");
    } else if (d->que == D_EVENTO && d->texto && d->n) {
      guion_evento(t, d->n, d->texto, d->otro);
    } else if (d->que == D_FOCO) {
      guion_foco(t, d->n);
    }
    if (d->n) dom_node_unref(d->n);
    if (d->otro) dom_node_unref(d->otro);
    free(d->texto);
    free(d);
    /* (ir a otra página puede haber soltado esta: se mira otra vez) */
    { struct jsthread *x; for (x = hilos; x && x != t; x = x->sig) ; if (!x) return; }
  }
}

/* ---- lo compilado, guardado: los guiones (por su dirección o por su
 * contenido), mientras el Navegador está abierto ---- */
typedef struct { u32 clave, largo, uso; u8 *bc; size_t n_bc; } Comp;
#define COMP_MAX 48
#define COMP_TOPE (600 * 1024)
static Comp comp[COMP_MAX];
static u32 comp_uso, comp_total;
static u32 fnv(const char *t, u32 n, u32 h) { u32 k; for (k = 0; k < n; k++) h = (h ^ (u8)t[k]) * 16777619u; return h; }
static Comp *comp_busca(u32 clave, u32 largo) {
  int k;
  for (k = 0; k < COMP_MAX; k++) if (comp[k].bc && comp[k].clave == clave && comp[k].largo == largo) { comp[k].uso = ++comp_uso; return &comp[k]; }
  return NULL;
}
static void comp_guarda(u32 clave, u32 largo, u8 *bc0, size_t n, JSRuntime *rt) {
  int k, viejo = 0;
  u8 *bc = malloc(n);                            /* (en la memoria del Navegador: la de JavaScript se suelta con cada página) */
  if (bc) memcpy(bc, bc0, n);
  js_free_rt(rt, bc0);
  if (!bc) return;
  if (n > COMP_TOPE / 2) { free(bc); return; }
  while (comp_total + n > COMP_TOPE) {
    int v = -1;
    for (k = 0; k < COMP_MAX; k++) if (comp[k].bc && (v < 0 || comp[k].uso < comp[v].uso)) v = k;
    if (v < 0) break;
    comp_total -= comp[v].n_bc; free(comp[v].bc); comp[v].bc = NULL;
  }
  for (k = 0; k < COMP_MAX && comp[k].bc; k++) if (comp[k].uso < comp[viejo].uso) viejo = k;
  if (k == COMP_MAX) { k = viejo; comp_total -= comp[k].n_bc; free(comp[k].bc); }
  comp[k].clave = clave; comp[k].largo = largo; comp[k].uso = ++comp_uso; comp[k].bc = bc; comp[k].n_bc = n;
  comp_total += n;
}
static void ejecuta(const char *codigo, u32 n, const char *nombre, u32 clave, u32 largo) {
  Comp *k = comp_busca(clave, largo);
  JSValue f;
  u32 t0 = GetTickCount();
  if (k) f = JS_ReadObject(cx, k->bc, k->n_bc, JS_READ_OBJ_BYTECODE);
  else {
    char *z = malloc(n + 1);                     /* (QuickJS quiere el 0 del final) */
    if (!z) return;
    memcpy(z, codigo, n); z[n] = 0;
    f = JS_Eval(cx, z, n, nombre, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    free(z);
    if (!JS_IsException(f) && n > 2000) {
      size_t nb;
      u8 *bc = JS_WriteObject(cx, &nb, f, JS_WRITE_OBJ_BYTECODE);
      if (bc) comp_guarda(clave, largo, bc, nb, H->rt);
    }
  }
  if (JS_IsException(f)) { excepcion(); return; }
  if (!H->nivel++) reinicia_tope();
  f = JS_EvalFunction(cx, f);
  if (JS_IsException(f)) excepcion();
  JS_FreeValue(cx, f);
  if (!--H->nivel) fin_turno();
#ifdef PERFIL
  perfil_dice();
#endif
  if (n > 20000) regf("[tiempo] ", "%.200s: %u KB, %u ms%s", nombre, n / 1024, GetTickCount() - t0, k ? " (ya compilado)" : "");
}

/* =============================================================================
 *  La interfaz de NetSurf (javascript/js.h)
 * ============================================================================= */
static int rt_nuevo(struct jsthread *t) {
  static JSClassDef def = { "Nodo", NULL, NULL, NULL, NULL };
  t->rt = JS_NewRuntime();
  if (!t->rt) return 0;
  n_motores++;
  JS_SetMemoryLimit(t->rt, (u32)guion_memoria_kb * 1024);
  JS_SetMaxStackSize(t->rt, 120 * 1024);
  JS_SetInterruptHandler(t->rt, corta, t);
  JS_SetHostPromiseRejectionTracker(t->rt, rechazo, NULL);
  if (!clase_nodo) JS_NewClassID(&clase_nodo);
  JS_NewClass(t->rt, clase_nodo, &def);
  return 1;
}
static void rt_suelta(JSRuntime *rt) {
  if (!rt) return;
  JS_FreeRuntime(rt);
  /* (sin motores, la memoria de JavaScript vuelve entera al sistema) */
  if (--n_motores == 0) HjReinicia();
}
/* compila una vez un texto de dentro del programa (el preludio, surf.js) */
static u8 *compila_interno(const char *txt, size_t n, const char *nombre, size_t *nb) {
  struct jsthread t;
  JSContext *c;
  JSValue f;
  u8 *r = NULL;
  memset(&t, 0, sizeof t);
  if (!rt_nuevo(&t)) return NULL;
  c = JS_NewContext(t.rt);
  f = c ? JS_Eval(c, txt, n, nombre, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY) : JS_EXCEPTION;
  if (c && JS_IsException(f)) {
    JSValue e = JS_GetException(c);
    const char *s = JS_ToCString(c, e);
    regf("[error] ", "%s: %s", nombre, s ? s : "?");
    if (s) JS_FreeCString(c, s);
    JS_FreeValue(c, e);
  } else if (c) {
    u8 *bc = JS_WriteObject(c, nb, f, JS_WRITE_OBJ_BYTECODE);
    if (bc && (r = malloc(*nb))) memcpy(r, bc, *nb);
    if (bc) js_free_rt(t.rt, bc);
    JS_FreeValue(c, f);
  }
  if (c) JS_FreeContext(c);
  rt_suelta(t.rt);
  return r;
}

void js_initialise(void) {
  FILE *f;
  javascript_init();                             /* (los guiones de fuera son contenidos de NetSurf) */
  snprintf(reg_ruta, sizeof reg_ruta, "%s\\SURFJS.LOG", surf_carpeta);
  snprintf(alm_ruta, sizeof alm_ruta, "%s\\NAVEGA2.ALM", surf_carpeta);
  if ((f = fopen(reg_ruta, "wb"))) fclose(f);    /* el registro, nuevo cada vez */
  preludio_bc = compila_interno(dom_js, sizeof dom_js - 1, "dom.js", &n_preludio_bc);
  surf_bc = compila_interno(surf_js, sizeof surf_js - 1, "surf.js", &n_surf_bc);
  if (!preludio_bc || !surf_bc) {
    reg("[error] ", "sin el preludio: el Navegador va sin JavaScript", 47);
    nsoption_set_bool(enable_javascript, false);
  }
  reg_vacia();
}
void js_finalise(void) {
  int k;
  for (k = 0; k < COMP_MAX; k++) if (comp[k].bc) { free(comp[k].bc); comp[k].bc = NULL; }
  free(preludio_bc); free(surf_bc);
  preludio_bc = surf_bc = NULL;
  sel_fin();
  reg_vacia();
}
nserror js_newheap(int timeout, jsheap **heap) {
  jsheap *h = calloc(1, sizeof *h);
  if (!h) return NSERROR_NOMEM;
  h->tope = timeout;
  *heap = h;
  return NSERROR_OK;
}
void js_destroyheap(jsheap *heap) { free(heap); }

#ifdef PERFIL
#define NF (int)(sizeof funciones_n / sizeof funciones_n[0])
static unsigned long long pf_t[128]; static unsigned pf_n[128];
static JSValue perfil(JSContext *c, JSValueConst este, int argc, JSValueConst *a, int k) {
  unsigned long long t0 = rdtsc();
  JSValue r = funciones_n[k].u.func.cfunc.generic(c, este, argc, a);
  pf_t[k] += rdtsc() - t0; pf_n[k]++;
  return r;
}
static void perfil_pon(JSValue nobj) {
  int k;
  for (k = 0; k < NF; k++)
    JS_SetPropertyStr(cx, nobj, funciones_n[k].name, JS_NewCFunctionMagic(cx, perfil, funciones_n[k].name, funciones_n[k].u.func.length, JS_CFUNC_generic_magic, k));
}
unsigned long long pf_dom, pf_dom2;
static void perfil_dice(void) {
  int k;
  regf("[perfil] ", "libdom: insert_before %u Mciclos, set_attribute %u Mciclos", (unsigned)(pf_dom >> 20), (unsigned)(pf_dom2 >> 20)); pf_dom = pf_dom2 = 0;
  for (k = 0; k < NF; k++) if (pf_n[k]) { regf("[perfil] ", "%s: %u veces, %u Mciclos", funciones_n[k].name, pf_n[k], (unsigned)(pf_t[k] >> 20)); pf_n[k] = 0; pf_t[k] = 0; }
}
#endif
/* el preludio con su N; luego surf.js */
static int prepara(struct jsthread *t) {
  JSValue g, nobj, f, r;
  int ok = 1;
  g = JS_GetGlobalObject(cx);
  nobj = JS_NewObject(cx);
  JS_SetPropertyFunctionList(cx, nobj, funciones_n, sizeof funciones_n / sizeof funciones_n[0]);
#ifdef PERFIL
  perfil_pon(nobj);
#endif
  t->nivel++;
  reinicia_tope();
  f = JS_ReadObject(cx, preludio_bc, n_preludio_bc, JS_READ_OBJ_BYTECODE);
  if (!JS_IsException(f)) f = JS_EvalFunction(cx, f);
  if (!JS_IsException(f)) { r = JS_Call(cx, f, JS_UNDEFINED, 1, (JSValueConst *)&nobj); JS_FreeValue(cx, f); f = r; }
  if (JS_IsException(f)) { excepcion(); ok = 0; }
  JS_FreeValue(cx, f);
  if (ok) {
    f = JS_ReadObject(cx, surf_bc, n_surf_bc, JS_READ_OBJ_BYTECODE);
    if (!JS_IsException(f)) f = JS_EvalFunction(cx, f);
    if (!JS_IsException(f)) { r = JS_Call(cx, f, JS_UNDEFINED, 1, (JSValueConst *)&nobj); JS_FreeValue(cx, f); f = r; }
    if (JS_IsException(f)) { excepcion(); ok = 0; }
    JS_FreeValue(cx, f);
  }
  t->nivel--;
  JS_FreeValue(cx, nobj);
  t->fn_evento = JS_GetPropertyStr(cx, g, "__evento");
  t->fn_tic = JS_GetPropertyStr(cx, g, "__tic");
  t->fn_llega = JS_GetPropertyStr(cx, g, "__llega");
  t->fn_foco = JS_GetPropertyStr(cx, g, "__foco");
  t->fn_cargado = JS_GetPropertyStr(cx, g, "__cargado");
  t->fn_informa = JS_GetPropertyStr(cx, g, "__informa");
  JS_FreeValue(cx, g);
  return ok;
}

nserror js_newthread(jsheap *heap, void *win_priv, void *doc_priv, jsthread **thread) {
  struct jsthread *t;
  html_content *h = doc_priv;
  (void)heap;
  *thread = NULL;
  if (!preludio_bc || !h || !h->document) return NSERROR_NOT_IMPLEMENTED;
  if (!(t = calloc(1, sizeof *t))) return NSERROR_NOMEM;
  t->bw = win_priv; t->html = h; t->doc = h->document;
  dom_node_ref(t->doc);
  t->proto_el = t->proto_tx = t->proto_doc = t->fn_evento = t->fn_tic = t->fn_llega = t->fn_foco = t->fn_cargado = t->fn_informa = JS_UNDEFINED;
  t->t_pagina = GetTickCount();
  if (!rt_nuevo(t) || !(t->cx = JS_NewContext(t->rt))) {
    if (t->rt) rt_suelta(t->rt);
    dom_node_unref(t->doc);
    free(t);
    return NSERROR_NOMEM;
  }
  t->sig = hilos; hilos = t;
  {
    ENTRA(t);
    regf("== ", "%.280s", url_pagina());
    if (!prepara(t)) { SALE(); js_destroythread(t); reg_vacia(); return NSERROR_NOT_IMPLEMENTED; }
    reg_vacia();
    SALE();
  }
  *thread = t;
  return NSERROR_OK;
}

static void cierra(struct jsthread *t) {
  Temp *x;
  Diferido *d;
  Pet *p, *q;
  t->cerrado = 1;
  while ((x = t->temps)) { t->temps = x->sig; framebuffer_schedule(-1, temp_tic, x); free(x); }
  framebuffer_schedule(-1, diferido_tic, t);
  while ((d = t->dif)) { t->dif = d->sig; if (d->n) dom_node_unref(d->n); if (d->otro) dom_node_unref(d->otro); free(d->texto); free(d); }
  for (p = pets; p; p = q) { q = p->sig; if (p->t == t) { red_js_cancela(p); pet_suelta(p); } }
  if (t->html) rehaz_olvida(t->html);
}
nserror js_closethread(jsthread *thread) {
  if (thread) cierra(thread);
  return NSERROR_OK;
}
void js_destroythread(jsthread *thread) {
  struct jsthread *t = thread, **q;
  u32 k;
  if (!t) return;
  cierra(t);
  for (q = &hilos; *q; q = &(*q)->sig) if (*q == t) { *q = t->sig; break; }
  {
    ENTRA(t);
    if (t->cx) {
      u32 uso, max, sis;
      HjMemoria(&uso, &max, &sis);
      regf("[memoria] ", "JavaScript: %u KB en uso (%u KB como mucho), %u KB pedidos al sistema; %u nodos con objeto",
           (uso + 512) / 1024, (max + 512) / 1024, (sis + 512) / 1024, t->n_tabla);
      for (k = 0; k < t->cap_tabla; k++) if (t->tabla[k].n) JS_FreeValue(cx, t->tabla[k].v);
      JS_FreeValue(cx, t->proto_el); JS_FreeValue(cx, t->proto_tx); JS_FreeValue(cx, t->proto_doc);
      JS_FreeValue(cx, t->fn_evento); JS_FreeValue(cx, t->fn_tic); JS_FreeValue(cx, t->fn_llega); JS_FreeValue(cx, t->fn_foco);
      JS_FreeValue(cx, t->fn_cargado); JS_FreeValue(cx, t->fn_informa);
      JS_FreeContext(cx);
    }
    SALE();
  }
  rt_suelta(t->rt);
  for (k = 0; k < t->cap_tabla; k++) if (t->tabla[k].n) dom_node_unref(t->tabla[k].n);
  free(t->tabla);
  dom_node_unref(t->doc);
  free(t->url_js);
  free(t);
  reg_vacia();
}

bool js_exec(jsthread *thread, const uint8_t *txt, size_t txtlen, const char *name) {
  struct jsthread *t = thread;
  char nombre[300];
  u32 clave, largo;
  if (!t || t->cerrado || !txt) return false;
  {
    ENTRA(t);
    if (name && strncmp(name, "?inline", 7)) { snprintf(nombre, sizeof nombre, "%s", name); clave = fnv(name, strlen(name), 2166136261u); largo = 0xFFFFFFFF; }
    else { snprintf(nombre, sizeof nombre, "%.200s (en la pagina)", url_pagina()); clave = fnv((const char *)txt, txtlen, 2166136261u); largo = txtlen; }
    ejecuta((const char *)txt, txtlen, nombre, clave, largo);
    SALE();
  }
  return true;
}

bool js_fire_event(jsthread *thread, const char *type, struct dom_document *doc, struct dom_node *target) {
  struct jsthread *t = thread;
  (void)doc;
  if (!t || t->cerrado) return true;
  if (!target && (!strcmp(type, "DOMContentLoaded") || !strcmp(type, "load"))) {
    int fase = type[0] == 'D' ? 1 : 2;
    ENTRA(t);
    while (t->fase < fase && !t->cerrado) {
      JSValue v = JS_NewInt32(cx, ++t->fase);
      JS_FreeValue(cx, llama(H->fn_cargado, 1, &v));
    }
    if (fase == 2) {
      u32 uso, max, sis;
      HjMemoria(&uso, &max, &sis);
      regf("[memoria] ", "JavaScript: %u KB en uso, %u KB pedidos al sistema; %u ms desde que empezo la pagina",
           (uso + 512) / 1024, (sis + 512) / 1024, GetTickCount() - t->t_pagina);
      reg_vacia();
    }
    SALE();
    return true;
  }
  if (target) return !guion_evento(t, target, type, NULL);
  return true;
}
bool js_dom_event_add_listener(jsthread *thread, struct dom_document *document, struct dom_node *node,
                               struct dom_string *event_type_dom, void *js_funcval) {
  (void)thread; (void)document; (void)node; (void)event_type_dom; (void)js_funcval;
  return true;
}
void js_handle_new_element(jsthread *thread, struct dom_element *node) { (void)thread; (void)node; }
void js_event_cleanup(jsthread *thread, struct dom_event *evt) { (void)thread; (void)evt; }

/* =============================================================================
 *  Lo que llaman los ganchos [HISPA] de NetSurf
 * ============================================================================= */
struct jsthread *guion_de(dom_node *n) {
  struct jsthread *t;
  struct dom_document *d;
  if (!n) return NULL;
  d = NI(n)->tipo == DOM_DOCUMENT_NODE ? (struct dom_document *)n : NI(n)->doc;
  for (t = hilos; t; t = t->sig) if (t->doc == d && !t->cerrado) return t;
  return NULL;
}
bool guion_evento(struct jsthread *t, dom_node *n, const char *tipo, dom_node *otro) {
  bool si = false;
  JSValue args[3], r;
  int k;
  if (!t || t->cerrado || !n) return false;
  if (!strcmp(tipo, "submit") && sin_evento_submit) return false;
  {
    ENTRA(t);
    /* (de un texto, al elemento de arriba) */
    while (n && NI(n)->tipo != DOM_ELEMENT_NODE && NI(n)->tipo != DOM_DOCUMENT_NODE) n = (dom_node *)NI(n)->padre;
    if (n) {
      args[0] = envuelve(n);
      args[1] = JS_NewString(cx, tipo);
      args[2] = otro ? envuelve(otro) : JS_NewInt32(cx, 0);
      r = llama(H->fn_evento, 3, args);
      si = JS_ToBool(cx, r) == 1;
      JS_FreeValue(cx, r);
      for (k = 0; k < 3; k++) JS_FreeValue(cx, args[k]);
    }
    SALE();
  }
  return si;
}
bool guion_tecla(struct jsthread *t, dom_node *n, const char *tipo, unsigned tecla) {
  bool si = false;
  JSValue args[4], r;
  int k, vk = 0, ch = 0;
  if (!t || t->cerrado) return false;
  /* las teclas de NetSurf (netsurf/keypress.h) a las de Windows (dom.js) */
  switch (tecla) {
  case NS_KEY_DELETE_LEFT: vk = 8; break;
  case NS_KEY_TAB: vk = 9; break;
  case NS_KEY_NL: case 13: vk = 13; break;
  case NS_KEY_ESCAPE: vk = 27; break;
  case NS_KEY_LEFT: vk = 37; break;
  case NS_KEY_UP: vk = 38; break;
  case NS_KEY_RIGHT: vk = 39; break;
  case NS_KEY_DOWN: vk = 40; break;
  case NS_KEY_PAGE_UP: vk = 33; break;
  case NS_KEY_PAGE_DOWN: vk = 34; break;
  case NS_KEY_DELETE_RIGHT: vk = 46; break;
  default: if (tecla >= 32 && tecla < 0x10000) ch = tecla; else return false;
  }
  {
    ENTRA(t);
    args[0] = n ? envuelve(n) : JS_NULL;
    args[1] = JS_NewString(cx, tipo);
    args[2] = JS_NewInt32(cx, vk);
    args[3] = JS_NewInt32(cx, ch);
    r = llama(H->fn_evento, 4, args);
    si = JS_ToBool(cx, r) == 1;
    JS_FreeValue(cx, r);
    for (k = 0; k < 4; k++) JS_FreeValue(cx, args[k]);
    SALE();
  }
  return si;
}
void guion_foco(struct jsthread *t, dom_node *n) {
  if (!t || t->cerrado) return;
  {
    ENTRA(t);
    JSValue el = n ? envuelve(n) : JS_NULL;
    JS_FreeValue(cx, llama(H->fn_foco, 1, &el));
    JS_FreeValue(cx, el);
    SALE();
  }
}
void guion_evento_luego(struct jsthread *t, dom_node *n, const char *tipo) {
  if (!t || t->cerrado || !n) return;
  { ENTRA(t); difiere(D_EVENTO, n, NULL, tipo, 0); SALE(); }
}
void guion_foco_luego(struct jsthread *t, dom_node *n) {
  if (!t || t->cerrado) return;
  { ENTRA(t); difiere(D_FOCO, n, NULL, NULL, 0); SALE(); }
}
int guion_ocupado(void) {
  struct jsthread *t;
  if (pets) return 1;
  for (t = hilos; t; t = t->sig) if (t->dif) return 1;
  return 0;
}
