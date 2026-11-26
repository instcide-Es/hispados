/* script.c - [JS] el Navegador de Hispa-486: JavaScript en las páginas.
 * Une el motor (HSPJS.DLL, que es QuickJS) con el árbol (html.c, dom.c), la
 * página (pagina.c), la red (http.c) y la ventana (navega.c, por JsNav):
 *  - por cada página con <script> (o con onclick="..."), un contexto nuevo
 *    en el que se ejecuta el preludio (dom.js: document, window, eventos,
 *    fetch...) y luego sus guiones, en orden (los de fuera se piden a la red,
 *    sin parar la ventana), los «defer», DOMContentLoaded y load;
 *  - los eventos de la persona (clics, teclas, lo escrito, el foco) llegan
 *    desde navega.c; tras cada guion o evento, si el árbol ha cambiado, la
 *    página se vuelve a maquetar;
 *  - setTimeout (con un temporizador de la ventana), fetch y
 *    XMLHttpRequest (de uno en uno, por http.c, con las galletas),
 *    localStorage (NAVEGA.ALM, junto al programa) y la consola (NAVEGA.LOG);
 *  - un tope de tiempo por guion o evento (js_tope_ms): si una página se
 *    queda dando vueltas, se para ese guion y el Navegador sigue.
 * Lo compilado se guarda (en la memoria, mientras el Navegador está abierto):
 * el preludio, una vez; los guiones de fuera, por su dirección; los de
 * dentro, por su contenido. Así las páginas siguientes de ClicGest no vuelven
 * a pedir ni a compilar lo mismo.
 * docs/486/NAVEGADOR.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include <stdio.h>                              /* (el de w32/js/inc, antes que nav.h) */
#include "dom.h"
#include "quickjs.h"
#include "nav_domjs.h"
#include "cors.h"

#define MFILE FILE
u32 WINAPI GetTickCount(void);
u32 WINAPI SetTimer(HANDLE, u32, u32, void *);
int WINAPI KillTimer(HANDLE, u32);
typedef struct { int left, top, right, bottom; } RECTJ;
int WINAPI GetClientRect(HANDLE, RECTJ *);
void HjMemoria(u32 *uso, u32 *maximo, u32 *sistema);   /* (HSPJS.DLL) */
void HjReinicia(void);

int js_activo = 1;
int js_tope_ms = 15000;
int js_memoria_kb = 7168;
static JSRuntime *rt;
static JSContext *cx;
static JSClassID clase_nodo;
static JSValue proto_el, proto_tx, proto_doc, fn_evento, fn_tic, fn_llega, fn_foco, fn_cargado, fn_guion, fn_informa, fn_ev;
static Pagina *pg;
static const JsNav *nav;
static HANDLE vent;
static char carpeta[160];
static u8 *preludio_bc;
static size_t n_preludio_bc;

/* =============================================================================
 *  El registro (NAVEGA.LOG): la consola de las páginas y sus errores
 * ============================================================================= */
static char reg_buf[8192];
static u32 n_reg;
static void reg_vacia(void) {
  char ruta[200];
  MFILE *f;
  if (!n_reg) return;
  sprintf(ruta, "%sNAVEGA.LOG", carpeta);
  if ((f = fopen(ruta, "ab"))) { fwrite(reg_buf, 1, n_reg, f); fclose(f); }
  n_reg = 0;
}
static void reg(const char *pre, const char *t, u32 n) {
  if (n_reg + n + 32 > sizeof reg_buf) reg_vacia();
  if (n + 32 > sizeof reg_buf) n = sizeof reg_buf - 32;
  memcpy(reg_buf + n_reg, pre, strlen(pre)); n_reg += strlen(pre);
  memcpy(reg_buf + n_reg, t, n); n_reg += n;
  reg_buf[n_reg++] = '\r'; reg_buf[n_reg++] = '\n';
}

/* =============================================================================
 *  Los nodos en JavaScript: un objeto por nodo (siempre el mismo), en una
 *  tabla Nodo* -> objeto que los tiene vivos hasta que se va la página
 * ============================================================================= */
typedef struct { Nodo *n; JSValue v; u8 escucha, hecho; } Env;
static Env *tabla;
static u32 n_tabla, cap_tabla;
static u32 hash_p(const void *p) { return ((u32)p >> 2) * 2654435761u; }
static Env *env_de(const Nodo *n) {
  u32 k;
  if (!cap_tabla) return NULL;
  for (k = hash_p(n) & (cap_tabla - 1); tabla[k].n; k = (k + 1) & (cap_tabla - 1)) if (tabla[k].n == n) return &tabla[k];
  return NULL;
}
static Env *env_pon(Nodo *n) {
  u32 k;
  if (n_tabla * 2 >= cap_tabla) {
    Env *viejo = tabla;
    u32 c = cap_tabla, j;
    cap_tabla = cap_tabla ? cap_tabla * 2 : 1024;
    tabla = malloc(cap_tabla * sizeof(Env));
    if (!tabla) { tabla = viejo; cap_tabla = c; return NULL; }
    memset(tabla, 0, cap_tabla * sizeof(Env));
    for (j = 0; j < c; j++)
      if (viejo[j].n) {
        for (k = hash_p(viejo[j].n) & (cap_tabla - 1); tabla[k].n; k = (k + 1) & (cap_tabla - 1)) ;
        tabla[k] = viejo[j];
      }
    free(viejo);
  }
  for (k = hash_p(n) & (cap_tabla - 1); tabla[k].n; k = (k + 1) & (cap_tabla - 1)) ;
  tabla[k].n = n; tabla[k].v = JS_UNDEFINED; tabla[k].escucha = tabla[k].hecho = 0;
  n_tabla++;
  return &tabla[k];
}
static JSValue envuelve(Nodo *n) {
  Env *e;
  JSValue o;
  if (!n) return JS_NULL;
  if ((e = env_de(n)) && !JS_IsUndefined(e->v)) return JS_DupValue(cx, e->v);
  o = JS_NewObjectProtoClass(cx, n->texto ? proto_tx : n == pg->doc.raiz ? proto_doc : proto_el, clase_nodo);
  if (JS_IsException(o)) return o;
  JS_SetOpaque(o, n);
  if (!e) e = env_pon(n);
  if (e) e->v = JS_DupValue(cx, o);
  return o;
}
static Nodo *nodo_de(JSValueConst v) { return JS_GetOpaque(v, clase_nodo); }

/* ---- textos: la página 1252 de dentro <-> UTF-8 de JavaScript ---- */
static JSValue a_js(const char *t, int n) {
  char pila[600], *b;
  int m;
  JSValue r;
  if (n < 0) n = strlen(t);
  b = n * 3 < (int)sizeof pila ? pila : malloc(n * 3 + 1);
  if (!b) return JS_ThrowOutOfMemory(cx);
  m = a_utf8(t, n, b);
  r = JS_NewStringLen(cx, b, m);
  if (b != pila) free(b);
  return r;
}
static char *de_js(JSValueConst v, int *len) {
  size_t n;
  const char *u = JS_ToCStringLen(cx, &n, v);
  char *b;
  int m;
  if (!u) return NULL;
  b = malloc(n + 1);
  if (!b) { JS_FreeCString(cx, u); return NULL; }
  m = utf8_a_1252((const u8 *)u, n, b);
  b[m] = 0;
  JS_FreeCString(cx, u);
  if (len) *len = m;
  return b;
}
static int es_guion(const Nodo *n) { return n && !n->texto && n->etq == E_SCRIPT; }

/* ---- los controles de formulario (en pagina.c) ---- */
static Ctl *ctl_de(Nodo *n) {
  if (dom_sucio_ctl) { pag_ctl_rehaz(pg, 1); dom_sucio_ctl = 0; dom_sucio = 1; }
  if (n->ctl && n->ctl <= pg->n_ctl && pg->ctl[n->ctl - 1].nodo == n) return &pg->ctl[n->ctl - 1];
  return NULL;
}
static Nodo *select_de(Nodo *o) {
  for (o = o->padre; o && o->etq != E_SELECT; o = o->padre) if (o->etq != E_OPTGROUP) return NULL;
  return o;
}
static void texto_opcion(Nodo *o, char *b, int max) {
  const char *v = atr(o, "value");
  int k;
  if (v) { for (k = 0; v[k] && k < max - 1; k++) b[k] = v[k]; b[k] = 0; return; }
  b[0] = 0;
  texto_de(o, b, max);
  for (k = strlen(b); k && b[k - 1] == ' '; ) b[--k] = 0;
  if (b[0] == ' ') memmove(b, b + 1, strlen(b));
}

/* =============================================================================
 *  Lo que da script.c a dom.js (el objeto N)
 * ============================================================================= */
#define FN(nombre) static JSValue nombre(JSContext *c, JSValueConst este, int argc, JSValueConst *a)
#define NODO(i, n) Nodo *n = argc > i ? nodo_de(a[i]) : NULL; if (!n) return JS_ThrowTypeError(c, "no es un nodo")
#define USA (void)c; (void)este; (void)argc
FN(f_es) { USA; return JS_NewBool(c, argc > 0 && nodo_de(a[0]) != NULL); }
FN(f_tipo) { USA; NODO(0, n); return JS_NewInt32(c, n->texto ? 3 : n == pg->doc.raiz ? 9 : 1); }
FN(f_padre) { USA; NODO(0, n); return envuelve(n->padre); }
FN(f_hijo) { USA; NODO(0, n); return envuelve(n->texto ? NULL : n->hijo); }
FN(f_ult) { USA; NODO(0, n); return envuelve(n->texto ? NULL : n->ult); }
FN(f_sig) { USA; NODO(0, n); return envuelve(n->sig); }
FN(f_ant) { USA; NODO(0, n); return envuelve(dom_ant(n)); }
FN(f_conectado) { USA; NODO(0, n); return JS_NewBool(c, dom_dentro(pg->doc.raiz, n)); }
FN(f_dentro) { USA; NODO(0, p); NODO(1, n); return JS_NewBool(c, dom_dentro(p, n)); }
FN(f_nombre) { USA; NODO(0, n); return JS_NewString(c, n->texto ? "#text" : n->etq == E_OTRO ? "" : etq_nom[n->etq]); }
FN(f_texto) {
  USA; NODO(0, n);
  if (!n->texto) return JS_NewString(c, "");
  return es_guion(n->padre) ? JS_NewStringLen(c, n->t, n->n) : a_js(n->t, n->n);
}
FN(f_pon_texto) {
  char *t;
  int l;
  USA; NODO(0, n);
  if (es_guion(n->padre)) {
    size_t m;
    const char *u = JS_ToCStringLen(c, &m, a[1]);
    if (u) { n->t = ar_texto(&pg->doc.ar, u, m); n->n = m; JS_FreeCString(c, u); }
    return JS_UNDEFINED;
  }
  if (!(t = de_js(a[1], &l))) return JS_EXCEPTION;
  dom_pon_texto(n, t, l);
  free(t);
  return JS_UNDEFINED;
}
FN(f_contenido) {
  u32 l;
  char *t;
  JSValue r;
  USA; NODO(0, n);
  t = dom_contenido(n, &l);
  r = JS_NewStringLen(c, t ? t : "", t ? l : 0);
  free(t);
  return r;
}
FN(f_pon_contenido) {
  char *t;
  int l;
  USA; NODO(0, n);
  if (es_guion(n)) {
    size_t m;
    const char *u = JS_ToCStringLen(c, &m, a[1]);
    Nodo *x;
    while (n->hijo) dom_quita(n->hijo);
    if (u && m && (x = dom_crea_texto("", 0))) { dom_inserta(n, x, NULL); x->t = ar_texto(&pg->doc.ar, u, m); x->n = m; }
    if (u) JS_FreeCString(c, u);
    return JS_UNDEFINED;
  }
  if (!(t = de_js(a[1], &l))) return JS_EXCEPTION;
  dom_pon_contenido(n, t, l);
  free(t);
  return JS_UNDEFINED;
}
static void guion_dinamico(Nodo *n);
static void mira_guiones(Nodo *n) {
  Nodo *h;
  if (n->texto) return;
  if (n->etq == E_SCRIPT) { guion_dinamico(n); return; }
  for (h = n->hijo; h; h = h->sig) mira_guiones(h);
}
FN(f_inserta) {
  Nodo *ref;
  USA; NODO(0, p); NODO(1, n);
  ref = argc > 2 ? nodo_de(a[2]) : NULL;
  if (p->texto) return JS_ThrowTypeError(c, "un texto no puede tener hijos");
  if (dom_dentro(n, p)) return JS_ThrowTypeError(c, "HierarchyRequestError: un nodo no puede ir dentro de sí mismo");
  dom_inserta(p, n, ref);
  if (dom_dentro(pg->doc.raiz, n)) mira_guiones(n);
  return JS_UNDEFINED;
}
FN(f_quita) { USA; NODO(0, n); dom_quita(n); return JS_UNDEFINED; }
static Nodo *clona(Nodo *n, int hondo) {
  Nodo *x, *h;
  if (n->texto) {
    x = dom_crea_texto("", 0);
    if (x) { x->t = n->t; x->n = n->n; }
    return x;
  }
  x = dom_crea(etq_nom[n->etq]);
  if (!x) return NULL;
  x->etq = n->etq;
  if (n->n_atr) {
    x->atr = ar_pide(&pg->doc.ar, n->n_atr * 2 * sizeof(char *));
    if (x->atr) { memcpy(x->atr, n->atr, n->n_atr * 2 * sizeof(char *)); x->n_atr = n->n_atr; }
  }
  x->abierto = n->abierto;
  if (hondo) for (h = n->hijo; h; h = h->sig) { Nodo *y = clona(h, 1); if (y) dom_inserta(x, y, NULL); }
  return x;
}
FN(f_clona) { USA; NODO(0, n); return envuelve(clona(n, argc > 1 && JS_ToBool(c, a[1]))); }
FN(f_crea_texto) {
  char *t;
  int l;
  JSValue r;
  USA;
  if (!(t = de_js(a[0], &l))) return JS_EXCEPTION;
  r = envuelve(dom_crea_texto(t, l));
  free(t);
  return r;
}
FN(f_crea) {
  char *t;
  JSValue r;
  USA;
  if (!(t = de_js(a[0], NULL))) return JS_EXCEPTION;
  r = envuelve(dom_crea(t));
  free(t);
  return r;
}
FN(f_atr) {
  char *t;
  const char *v;
  USA; NODO(0, n);
  if (n->texto || !(t = de_js(a[1], NULL))) return JS_NULL;
  v = atr(n, t);
  free(t);
  return v ? a_js(v, -1) : JS_NULL;
}
FN(f_atrs) {
  JSValue r = JS_NewArray(c);
  int k;
  USA; NODO(0, n);
  for (k = 0; k < n->n_atr; k++) JS_SetPropertyUint32(c, r, k, a_js(n->atr[2 * k], -1));
  return r;
}
/* lo que no cambia cómo se ve la página: no hace falta volver a maquetar */
static int sin_maqueta(const char *nom) {
  return !strncmp(nom, "aria-", 5) || !strncmp(nom, "data-", 5) || !strcmp(nom, "role") || !strcmp(nom, "tabindex")
      || !strcmp(nom, "autocomplete") || !strcmp(nom, "spellcheck") || !strncmp(nom, "on", 2);
}
static void sincroniza(Nodo *n, const char *nom, const char *val) {
  Ctl *k;
  if (!strcmp(nom, "open") && n->etq == E_DETAILS) n->abierto = val != NULL;
  if (n->etq != E_INPUT && n->etq != E_SELECT && n->etq != E_TEXTAREA && n->etq != E_BUTTON && n->etq != E_OPTION) return;
  if (!strcmp(nom, "type") || !strcmp(nom, "name") || (!strcmp(nom, "value") && n->etq == E_OPTION)) { dom_sucio_ctl = 1; return; }
  if (n->etq == E_OPTION) {
    Nodo *s;
    if (!strcmp(nom, "selected") && val && (s = select_de(n)) && (k = ctl_de(s))) {
      int j;
      for (j = 0; j < k->n_opc; j++) if (k->opc[j] == n) k->elegida = j;
    }
    return;
  }
  if (!(k = ctl_de(n))) return;
  if (!strcmp(nom, "disabled")) k->desactivado = val != NULL;
  else if (!strcmp(nom, "readonly")) k->sololee = val != NULL;
  else if (!strcmp(nom, "required")) k->obligado = val != NULL;
  else if (!strcmp(nom, "checked") && (k->tipo == C_CASILLA || k->tipo == C_RADIO)) k->marcado = val != NULL;
  else if (!strcmp(nom, "value") && (k->tipo == C_TEXTO || k->tipo == C_CLAVE || k->tipo == C_OCULTO || k->tipo == C_ENVIAR
                                     || k->tipo == C_BOTON || k->tipo == C_REINICIO || k->tipo == C_CASILLA || k->tipo == C_RADIO)) {
    ctl_pon(k, val ? val : "", -1);
    k->cursor = k->n_valor;
  }
}
FN(f_pon_atr) {
  char *nom, *val;
  int antes = dom_sucio, quieto;
  USA; NODO(0, n);
  if (n->texto) return JS_UNDEFINED;
  if (!(nom = de_js(a[1], NULL))) return JS_EXCEPTION;
  if (!(val = de_js(a[2], NULL))) { free(nom); return JS_EXCEPTION; }
  quieto = (argc > 3 && JS_ToBool(c, a[3])) || sin_maqueta(nom);
  dom_pon_atr(n, nom, val);
  sincroniza(n, nom, val);
  /* otra imagen en el mismo <img>: ficha nueva al maquetar (n->ctl es la de la imagen) */
  if (n->etq == E_IMG && !_stricmp(nom, "src")) { n->ctl = 0; dom_sucio = 1; quieto = 0; }
  if (quieto) dom_sucio = antes;
  free(nom); free(val);
  return JS_UNDEFINED;
}
FN(f_quita_atr) {
  char *nom;
  int antes = dom_sucio;
  USA; NODO(0, n);
  if (!(nom = de_js(a[1], NULL))) return JS_EXCEPTION;
  if (atr(n, nom)) {
    dom_quita_atr(n, nom);
    if (!strcmp(nom, "class") && (n->etq == E_NAV || n->etq == E_ASIDE)) n->abierto = 0;
    sincroniza(n, nom, NULL);
    if (sin_maqueta(nom)) dom_sucio = antes;
  }
  free(nom);
  return JS_UNDEFINED;
}
FN(f_html) {
  u32 l;
  char *t;
  JSValue r;
  USA; NODO(0, n);
  t = dom_serializa(n, argc > 1 && JS_ToBool(c, a[1]), &l);
  r = JS_NewStringLen(c, t ? t : "", t ? l : 0);
  free(t);
  return r;
}
FN(f_pon_html) {
  size_t m;
  const char *u;
  USA; NODO(0, n);
  if (n->texto) return JS_UNDEFINED;
  if (!(u = JS_ToCStringLen(c, &m, a[1]))) return JS_EXCEPTION;
  if (es_guion(n)) { JS_FreeCString(c, u); return f_pon_contenido(c, este, argc, a); }
  dom_html(n, (const u8 *)u, m);
  JS_FreeCString(c, u);
  return JS_UNDEFINED;
}
#define BUSCA_MAX 8192
static Nodo *encontrados[BUSCA_MAX];
FN(f_busca) {
  char *sel;
  int k, m, uno;
  JSValue r;
  USA; NODO(0, n);
  uno = argc > 2 && JS_ToBool(c, a[2]);
  if (!(sel = de_js(a[1], NULL))) return JS_EXCEPTION;
  m = n->texto ? 0 : dom_busca(n, sel, encontrados, uno ? 1 : BUSCA_MAX);
  if (m < 0) { r = JS_ThrowSyntaxError(c, "SyntaxError: '%s' no es un selector valido", sel); free(sel); return r; }
  free(sel);
  if (uno) return m ? envuelve(encontrados[0]) : JS_NULL;
  r = JS_NewArray(c);
  for (k = 0; k < m; k++) JS_SetPropertyUint32(c, r, k, envuelve(encontrados[k]));
  return r;
}
FN(f_casa) {
  char *sel;
  int m;
  JSValue r;
  USA; NODO(0, n);
  if (!(sel = de_js(a[1], NULL))) return JS_EXCEPTION;
  m = dom_casa(n, sel, NULL);
  if (m < 0) { r = JS_ThrowSyntaxError(c, "SyntaxError: '%s' no es un selector valido", sel); free(sel); return r; }
  free(sel);
  return JS_NewBool(c, m);
}
FN(f_por_id) {
  char *t;
  JSValue r;
  USA;
  if (!(t = de_js(a[0], NULL))) return JS_EXCEPTION;
  r = envuelve(t[0] ? dom_por_id(pg->doc.raiz, t) : NULL);
  free(t);
  return r;
}
FN(f_doc) { USA; (void)a; return envuelve(pg->doc.raiz); }
FN(f_cuerpo) { USA; (void)a; return envuelve(pg->doc.body && pg->doc.body != pg->doc.raiz ? pg->doc.body : NULL); }
FN(f_prototipos) {
  USA;
  proto_el = JS_DupValue(c, a[0]); proto_tx = JS_DupValue(c, a[1]); proto_doc = JS_DupValue(c, a[2]);
  return JS_UNDEFINED;
}
FN(f_escucha) {
  Env *e;
  USA; NODO(0, n);
  if ((e = env_de(n)) && !e->escucha) { e->escucha = 1; dom_sucio = 1; }
  return JS_UNDEFINED;
}
FN(f_oculto) { USA; NODO(0, n); return JS_NewBool(c, dom_oculto(n) || !dom_dentro(pg->doc.raiz, n)); }
FN(f_abierto) { USA; NODO(0, n); return JS_NewBool(c, n->etq == E_DETAILS ? n->abierto : atr(n, "open") != NULL); }
FN(f_pon_abierto) {
  int si;
  USA; NODO(0, n);
  si = JS_ToBool(c, a[1]);
  if (si && !atr(n, "open")) dom_pon_atr(n, "open", "");
  if (!si && atr(n, "open")) dom_quita_atr(n, "open");
  if (n->etq == E_DETAILS && n->abierto != si) { n->abierto = (u8)si; dom_sucio = 1; }
  return JS_UNDEFINED;
}
/* value, checked, selectedIndex */
FN(f_valor) {
  Ctl *k;
  char b[400];
  const char *v;
  USA; NODO(0, n);
  if (n->texto) return JS_NewString(c, "");
  if (n->etq == E_OPTION) { texto_opcion(n, b, sizeof b); return a_js(b, -1); }
  if ((k = ctl_de(n))) {
    if (k->tipo == C_LISTA) {
      if (!k->n_opc || k->elegida >= k->n_opc) return JS_NewString(c, "");
      texto_opcion(k->opc[k->elegida], b, sizeof b);
      return a_js(b, -1);
    }
    if (k->tipo == C_CASILLA || k->tipo == C_RADIO) return a_js((v = atr(n, "value")) ? v : "on", -1);
    if (k->tipo == C_ARCHIVO) return JS_NewString(c, "");
    return a_js(k->valor ? k->valor : "", k->valor ? k->n_valor : 0);
  }
  if (n->etq == E_TEXTAREA) { u32 l; char *t = dom_contenido(n, &l); JSValue r = JS_NewStringLen(c, t ? t : "", t ? l : 0); free(t); return r; }
  if (n->etq == E_SELECT) {
    Nodo *x[1];
    if (dom_busca(n, "option[selected]", x, 1) == 1 || dom_busca(n, "option", x, 1) == 1) { texto_opcion(x[0], b, sizeof b); return a_js(b, -1); }
    return JS_NewString(c, "");
  }
  v = atr(n, "value");
  if (n->etq == E_INPUT && !v && (v = atr(n, "type")) && (!_stricmp(v, "checkbox") || !_stricmp(v, "radio"))) return JS_NewString(c, "on");
  return a_js(atr(n, "value") ? atr(n, "value") : "", -1);
}
FN(f_pon_valor) {
  Ctl *k;
  char *t, b[400];
  int l, j;
  USA; NODO(0, n);
  if (!(t = de_js(a[1], &l))) return JS_EXCEPTION;
  if ((k = ctl_de(n))) {
    if (k->tipo == C_LISTA) {
      k->elegida = k->n_opc ? k->elegida : 0;
      for (j = 0; j < k->n_opc; j++) { texto_opcion(k->opc[j], b, sizeof b); if (!strcmp(b, t)) { k->elegida = j; break; } }
    } else if (k->tipo == C_CASILLA || k->tipo == C_RADIO) dom_pon_atr(n, "value", t);
    else { ctl_pon(k, t, l); k->cursor = k->n_valor; k->desp = 0; }
    dom_sucio = 1;
  } else if (n->etq == E_TEXTAREA) dom_pon_contenido(n, t, l);
  else if (n->etq != E_SELECT) dom_pon_atr(n, "value", t);
  free(t);
  return JS_UNDEFINED;
}
FN(f_marcado) {
  Ctl *k;
  USA; NODO(0, n);
  if (n->etq == E_OPTION) {
    Nodo *s = select_de(n);
    if (s && (k = ctl_de(s)) && k->n_opc && k->elegida < k->n_opc) return JS_NewBool(c, k->opc[k->elegida] == n);
    return JS_NewBool(c, atr(n, "selected") != NULL);
  }
  if ((k = ctl_de(n)) && (k->tipo == C_CASILLA || k->tipo == C_RADIO)) return JS_NewBool(c, k->marcado);
  return JS_NewBool(c, atr(n, "checked") != NULL);
}
FN(f_pon_marcado) {
  Ctl *k;
  int si, j;
  USA; NODO(0, n);
  si = JS_ToBool(c, a[1]);
  if (n->etq == E_OPTION) {
    Nodo *s = select_de(n);
    if (s && (k = ctl_de(s))) { for (j = 0; j < k->n_opc; j++) if (k->opc[j] == n && si) k->elegida = j; dom_sucio = 1; }
    else if (si) dom_pon_atr(n, "selected", ""); else dom_quita_atr(n, "selected");
    return JS_UNDEFINED;
  }
  if ((k = ctl_de(n)) && (k->tipo == C_CASILLA || k->tipo == C_RADIO)) {
    if (si && k->tipo == C_RADIO)
      for (j = 0; j < pg->n_ctl; j++) {
        Ctl *o = &pg->ctl[j];
        if (o->tipo == C_RADIO && o->forma == k->forma && o->nombre && k->nombre && !strcmp(o->nombre, k->nombre)) o->marcado = 0;
      }
    k->marcado = (u8)si;
    dom_sucio = 1;
  } else if (si) dom_pon_atr(n, "checked", ""); else dom_quita_atr(n, "checked");
  return JS_UNDEFINED;
}
FN(f_elegida) {
  Ctl *k;
  USA; NODO(0, n);
  if ((k = ctl_de(n)) && k->tipo == C_LISTA) return JS_NewInt32(c, k->n_opc ? k->elegida : -1);
  return JS_NewInt32(c, -1);
}
FN(f_pon_elegida) {
  Ctl *k;
  int i = 0;
  USA; NODO(0, n);
  JS_ToInt32(c, &i, a[1]);
  if ((k = ctl_de(n)) && k->tipo == C_LISTA && i >= 0 && i < k->n_opc) { k->elegida = i; dom_sucio = 1; }
  return JS_UNDEFINED;
}
/* ---- lo de la ventana ---- */
static u32 limite;
static void reinicia_tope(void) { limite = GetTickCount() + (u32)js_tope_ms; }
FN(f_envia) { USA; NODO(0, f); nav->envia(f, argc > 1 ? nodo_de(a[1]) : NULL); return JS_UNDEFINED; }
FN(f_foco) { USA; nav->foco(argc > 0 ? nodo_de(a[0]) : NULL); return JS_UNDEFINED; }
static Nodo *a_ver;                              /* scrollIntoView: tras volver a maquetar */
FN(f_a_la_vista) { USA; NODO(0, n); a_ver = n; return JS_UNDEFINED; }
FN(f_cuadro) {
  int tipo = 0, r;
  char *t;
  USA;
  JS_ToInt32(c, &tipo, a[0]);
  if (!(t = de_js(a[1], NULL))) return JS_EXCEPTION;
  reg_vacia();
  r = nav->cuadro(t, tipo);
  reinicia_tope();
  free(t);
  return JS_NewBool(c, r);
}
FN(f_pregunta) {
  char *t, *d, resp[400];
  int r;
  USA;
  if (!(t = de_js(a[0], NULL))) return JS_EXCEPTION;
  if (!(d = de_js(a[1], NULL))) { free(t); return JS_EXCEPTION; }
  r = nav->pregunta(t, d, resp, sizeof resp);
  reinicia_tope();
  free(t); free(d);
  return r ? a_js(resp, -1) : JS_NULL;
}
FN(f_titulo) { char *t; USA; if ((t = de_js(a[0], NULL))) { pg->doc.titulo = ar_texto(&pg->doc.ar, t, -1); nav->titulo(t); free(t); } return JS_UNDEFINED; }
FN(f_url) { USA; (void)a; return a_js(pg->url, -1); }
FN(f_pon_url) {
  char *t;
  USA;
  if ((t = de_js(a[0], NULL))) {
    /* (history.pushState y location.hash: solo dentro del mismo origen; con
     * otro, la página pasaría por otro sitio: sus galletas y su almacén) */
    if (!cors_mismo(pg->url, t)) { free(t); return JS_ThrowTypeError(c, "SecurityError: pushState a otro origen"); }
    pg->url = ar_texto(&pg->doc.ar, t, -1); nav->pon_url(t); free(t);
  }
  return JS_UNDEFINED;
}
FN(f_absoluta) {
  char *t, *u;
  JSValue r;
  USA;
  if (!(t = de_js(a[0], NULL))) return JS_EXCEPTION;
  u = url_absoluta(pg->doc.base ? pg->doc.base : pg->url, t);
  r = a_js(u ? u : t, -1);
  free(u); free(t);
  return r;
}
FN(f_absoluta_de) {
  char *b, *t, *u;
  JSValue r;
  USA;
  if (!(b = de_js(a[0], NULL))) return JS_EXCEPTION;
  if (!(t = de_js(a[1], NULL))) { free(b); return JS_EXCEPTION; }
  u = url_absoluta(b, t);
  r = u ? a_js(u, -1) : JS_NULL;
  free(u); free(t); free(b);
  return r;
}
FN(f_ir) {
  char *t;
  USA;
  if (!(t = de_js(a[0], NULL))) return JS_EXCEPTION;
  if (!_strnicmp(t, "javascript:", 11)) { free(t); return JS_UNDEFINED; }
  nav->ir(t, 0, NULL, 0);
  free(t);
  return JS_UNDEFINED;
}
FN(f_historia) { int k = 0; USA; JS_ToInt32(c, &k, a[0]); nav->historia(k); return JS_UNDEFINED; }
FN(f_galletas) { char *g; JSValue r; USA; (void)a; g = http_galletas(pg->url); r = a_js(g ? g : "", -1); free(g); return r; }
FN(f_pon_galleta) { char *t; USA; if ((t = de_js(a[0], NULL))) { http_pon_galleta(pg->url, t); free(t); } return JS_UNDEFINED; }
static int px_ancho(int alto) {
  RECTJ r;
  GetClientRect(vent, &r);
  return alto ? r.bottom - r.top - 74 : r.right - r.left - 16;
}
FN(f_ancho) { USA; (void)a; return JS_NewInt32(c, px_ancho(0)); }
FN(f_alto) { USA; (void)a; return JS_NewInt32(c, px_ancho(1)); }
FN(f_ahora) { USA; (void)a; return JS_NewFloat64(c, (double)GetTickCount()); }
FN(f_agente) { USA; (void)a; return JS_NewString(c, HTTP_AGENTE); }
FN(f_idioma) { USA; (void)a; return JS_NewString(c, !_strnicmp(http_idioma, "ca", 2) ? "ca-ES" : !_strnicmp(http_idioma, "en", 2) ? "en-GB" : "es-ES"); }
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

/* ---- el almacén: localStorage en NAVEGA.ALM («L origen datos» por línea);
 * sessionStorage, en la memoria ---- */
#define SES_MAX 8
static struct { char *origen, *datos; } ses[SES_MAX];
static char *alm_lee(const char *origen) {
  char ruta[200], *l = malloc(65536), *r = NULL;
  MFILE *f;
  u32 lo = strlen(origen);
  if (!l) return NULL;
  sprintf(ruta, "%sNAVEGA.ALM", carpeta);
  if ((f = fopen(ruta, "rb"))) {
    while (fgets(l, 65536, f))
      if (l[0] == 'L' && l[1] == ' ' && !strncmp(l + 2, origen, lo) && l[2 + lo] == ' ') {
        char *d = l + 3 + lo, *fin = d + strlen(d);
        while (fin > d && (fin[-1] == '\n' || fin[-1] == '\r')) *--fin = 0;
        r = malloc(strlen(d) + 1);
        if (r) strcpy(r, d);
        break;
      }
    fclose(f);
  }
  free(l);
  return r;
}
static void alm_guarda(const char *origen, const char *datos) {
  char ruta[200], *l = malloc(65536), *todo = NULL;
  u32 n = 0, lo = strlen(origen), cap = 0;
  MFILE *f;
  if (!l) return;
  sprintf(ruta, "%sNAVEGA.ALM", carpeta);
  if ((f = fopen(ruta, "rb"))) {
    while (fgets(l, 65536, f)) {
      u32 ll = strlen(l);
      if (l[0] == 'L' && l[1] == ' ' && !strncmp(l + 2, origen, lo) && l[2 + lo] == ' ') continue;
      if (n + ll + 1 > cap) { char *q = realloc(todo, cap = (cap + ll) * 2 + 1024); if (!q) break; todo = q; }
      memcpy(todo + n, l, ll); n += ll;
    }
    fclose(f);
  }
  if ((f = fopen(ruta, "wb"))) {
    if (n) fwrite(todo, 1, n, f);
    if (strcmp(datos, "{}")) { fwrite("L ", 1, 2, f); fwrite(origen, 1, lo, f); fwrite(" ", 1, 1, f); fwrite(datos, 1, strlen(datos), f); fwrite("\r\n", 1, 2, f); }
    fclose(f);
  }
  free(todo); free(l);
}
/* el origen del almacén: el de la dirección de la página, que sabe C (no el
 * que diga el JavaScript de la página, que puede cambiar URL.prototype);
 * 0 si no tiene (about:, file:...: sin almacén en el disco) */
static int alm_origen(char *o) { return pg && cors_origen(pg->url, o, 160); }
FN(f_almacen) {
  char o[160];
  char *d = NULL;
  JSValue r;
  int k;
  USA;
  if (!alm_origen(o)) return JS_NewString(c, "");
  if (JS_ToBool(c, a[0])) d = alm_lee(o);
  else for (k = 0; k < SES_MAX; k++) if (ses[k].origen && !strcmp(ses[k].origen, o)) { d = malloc(strlen(ses[k].datos) + 1); if (d) strcpy(d, ses[k].datos); }
  r = JS_NewString(c, d ? d : "");
  free(d);
  return r;
}
FN(f_guarda_almacen) {
  const char *d;
  char o[160];
  int k, libre = 0;
  USA;
  if (!alm_origen(o)) return JS_UNDEFINED;
  if (!(d = JS_ToCString(c, a[1]))) return JS_EXCEPTION;
  if (strlen(d) > 60000) { JS_FreeCString(c, d); return JS_ThrowRangeError(c, "QuotaExceededError: el almacen esta lleno"); }
  if (strchr(d, '\n') || strchr(d, '\r')) { JS_FreeCString(c, d); return JS_UNDEFINED; }   /* (JSON.stringify no los deja; una línea por origen) */
  if (JS_ToBool(c, a[0])) alm_guarda(o, d);
  else {
    for (k = 0; k < SES_MAX && !(ses[k].origen && !strcmp(ses[k].origen, o)); k++) if (!ses[k].origen) libre = k;
    if (k == SES_MAX) { k = libre; free(ses[k].origen); free(ses[k].datos); ses[k].origen = malloc(strlen(o) + 1); if (ses[k].origen) strcpy(ses[k].origen, o); ses[k].datos = NULL; }
    free(ses[k].datos);
    ses[k].datos = malloc(strlen(d) + 1);
    if (ses[k].datos) strcpy(ses[k].datos, d);
  }
  JS_FreeCString(c, d);
  return JS_UNDEFINED;
}

/* ---- los temporizadores: uno de la ventana (JS_TIMER) mientras haya alguno ---- */
#define TEMP_MAX 256
static struct { int id; u32 cuando; } temps[TEMP_MAX];
static int n_temps, temp_puesto;
static void temp_ajusta(void) {
  if (n_temps && !temp_puesto) { SetTimer(vent, JS_TIMER, 55, NULL); temp_puesto = 1; }
  if (!n_temps && temp_puesto) { KillTimer(vent, JS_TIMER); temp_puesto = 0; }
}
FN(f_temporiza) {
  int id = 0, k;
  double ms = 0;
  USA;
  JS_ToInt32(c, &id, a[0]);
  JS_ToFloat64(c, &ms, a[1]);
  if (ms > 86400000.0) ms = 86400000.0;
  for (k = 0; k < n_temps && temps[k].id != id; k++) ;
  if (k == n_temps) { if (n_temps == TEMP_MAX) return JS_ThrowRangeError(c, "demasiados temporizadores"); n_temps++; }
  temps[k].id = id;
  temps[k].cuando = GetTickCount() + (u32)ms;
  return JS_UNDEFINED;
}
FN(f_des_temporiza) {
  int id = 0, k;
  USA;
  JS_ToInt32(c, &id, a[0]);
  for (k = 0; k < n_temps; k++) if (temps[k].id == id) { temps[k] = temps[--n_temps]; break; }
  return JS_UNDEFINED;
}

/* ---- la red: una cola de peticiones (fetch, XMLHttpRequest y los guiones
 * de fuera); una en marcha, por http.c, cuando el Navegador no está cargando
 * una página ---- */
/* cred: las galletas que pide la página (0 «omit», 1 «same-origin», 2
 * «include» o withCredentials); otro: ha ido (o ido a parar) a otro origen,
 * y entonces la respuesta solo la lee si el servidor lo deja (CORS) */
typedef struct { int id, guion, redirs, post, cred, otro; char *metodo, *url, *cab, *cuerpo; u32 n_cuerpo; } PetJs;
#define COLA_MAX 32
static PetJs cola[COLA_MAX];
static int n_cola;
static Pet jp;
static PetJs en_marcha;
static int jp_activa;
static char *copia(const char *t, int n) {
  char *p;
  if (!t) return NULL;
  if (n < 0) n = strlen(t);
  p = malloc(n + 1);
  if (p) { memcpy(p, t, n); p[n] = 0; }
  return p;
}
static void pet_suelta(PetJs *p) { free(p->metodo); free(p->url); free(p->cab); free(p->cuerpo); memset(p, 0, sizeof *p); }
static int encola(int id, int guion, const char *metodo, const char *url, const char *cab, const char *cuerpo, u32 n_cuerpo, int cred) {
  PetJs *p;
  if (n_cola == COLA_MAX) return 0;
  p = &cola[n_cola++];
  memset(p, 0, sizeof *p);
  p->id = id; p->guion = guion; p->cred = cred;
  p->metodo = copia(metodo ? metodo : "GET", -1);
  p->url = copia(url, -1);
  p->cab = copia(cab ? cab : "", -1);
  p->post = cuerpo != NULL;
  p->cuerpo = cuerpo ? copia(cuerpo, n_cuerpo) : NULL;
  p->n_cuerpo = n_cuerpo;
  return 1;
}
FN(f_pide) {
  int id = 0, cred = 1;
  const char *m, *u, *h, *b = NULL;
  size_t nb = 0;
  USA;
  JS_ToInt32(c, &id, a[0]);
  m = JS_ToCString(c, a[1]); u = JS_ToCString(c, a[2]); h = JS_ToCString(c, a[3]);
  if (argc > 4 && !JS_IsNull(a[4]) && !JS_IsUndefined(a[4])) b = JS_ToCStringLen(c, &nb, a[4]);
  if (argc > 5) JS_ToInt32(c, &cred, a[5]);
  if (m && u && h) {
    char *u1252 = malloc(strlen(u) + 1);
    if (u1252) { u1252[utf8_a_1252((const u8 *)u, strlen(u), u1252)] = 0; }
    if (!u1252 || !encola(id, -1, m, u1252, h, b, nb, cred)) {
      JSValue args[5] = { JS_NewInt32(c, id), JS_NewInt32(c, 0), JS_NewString(c, ""), JS_NewString(c, ""), JS_NewString(c, "demasiadas peticiones") };
      JS_FreeValue(c, JS_Call(c, fn_llega, JS_UNDEFINED, 5, args));
      { int k; for (k = 0; k < 5; k++) JS_FreeValue(c, args[k]); }
    }
    free(u1252);
  }
  if (m) JS_FreeCString(c, m);
  if (u) JS_FreeCString(c, u);
  if (h) JS_FreeCString(c, h);
  if (b) JS_FreeCString(c, b);
  return JS_UNDEFINED;
}
FN(f_cancela) {
  int id = 0, k;
  USA;
  JS_ToInt32(c, &id, a[0]);
  for (k = 0; k < n_cola; k++) if (cola[k].id == id && cola[k].guion < 0) { pet_suelta(&cola[k]); memmove(cola + k, cola + k + 1, (n_cola - k - 1) * sizeof cola[0]); n_cola--; break; }
  return JS_UNDEFINED;
}

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
  JS_CFUNC_DEF("idioma", 0, f_idioma), JS_CFUNC_DEF("agente", 0, f_agente), JS_CFUNC_DEF("registro", 2, f_registro), JS_CFUNC_DEF("almacen", 1, f_almacen),
  JS_CFUNC_DEF("guardaAlmacen", 2, f_guarda_almacen), JS_CFUNC_DEF("temporiza", 2, f_temporiza),
  JS_CFUNC_DEF("desTemporiza", 1, f_des_temporiza), JS_CFUNC_DEF("pide", 6, f_pide), JS_CFUNC_DEF("cancela", 1, f_cancela),
};

/* =============================================================================
 *  Ejecutar: cada llamada desde fuera es un «turno»; al acabar, las
 *  promesas pendientes y, si el árbol ha cambiado, la maqueta
 * ============================================================================= */
static int nivel;
static int corta(JSRuntime *r, void *x) {
  (void)r; (void)x;
  return nivel && (int)(GetTickCount() - limite) > 0;
}
static void excepcion(void) {
  JSValue e = JS_GetException(cx);
  const char *t;
  if (JS_IsNull(e) || JS_IsUndefined(e)) return;
  if (JS_IsError(cx, e) && (t = JS_ToCString(cx, e))) {
    if (strstr(t, "interrupted")) {
      char b[120];
      sprintf(b, "un guion ha tardado mas de %d s y se ha parado", js_tope_ms / 1000);
      reg("[error] ", b, strlen(b));
      nav->estado("Un guion de la p\341gina tardaba demasiado: se ha parado.");
    }
    JS_FreeCString(cx, t);
  }
  if (JS_IsFunction(cx, fn_informa)) {
    JSValue r = JS_Call(cx, fn_informa, JS_UNDEFINED, 1, &e);
    if (JS_IsException(r)) { JSValue e2 = JS_GetException(cx); JS_FreeValue(cx, e2); }
    JS_FreeValue(cx, r);
  }
  JS_FreeValue(cx, e);
}
static void fin_turno(void) {
  JSContext *c2;
  int e, k = 0;
  nivel++;
  reinicia_tope();
  while (cx && (e = JS_ExecutePendingJob(rt, &c2)) != 0) { if (e < 0) excepcion(); if (++k > 200000) break; }
  nivel--;
  if (!cx) return;
  if (dom_sucio_ctl) { pag_ctl_rehaz(pg, 1); dom_sucio_ctl = 0; dom_sucio = 1; }
  if (dom_sucio) { dom_sucio = 0; nav->remaqueta(); }
  if (a_ver) { Nodo *n = a_ver; a_ver = NULL; if (dom_dentro(pg->doc.raiz, n)) nav->a_la_vista(n); }
  temp_ajusta();
  reg_vacia();
}
static JSValue llama(JSValueConst f, int n, JSValueConst *args) {
  JSValue r;
  if (!cx) return JS_UNDEFINED;
  if (!nivel++) reinicia_tope();
  r = JS_Call(cx, f, JS_UNDEFINED, n, args);
  if (JS_IsException(r)) { excepcion(); r = JS_UNDEFINED; }
  if (!--nivel) fin_turno();
  return r;
}
static void rechazo(JSContext *c, JSValueConst prom, JSValueConst razon, JS_BOOL atendida, void *x) {
  const char *t;
  (void)prom; (void)x;
  if (atendida) return;
  if ((t = JS_ToCString(c, razon))) { reg("[error] promesa rechazada sin recoger: ", t, strlen(t)); JS_FreeCString(c, t); }
}

/* ---- lo compilado, guardado: el preludio y los guiones (por su dirección o
 * por su contenido), mientras el Navegador está abierto ---- */
typedef struct { u32 clave, largo, uso; u8 *bc; size_t n_bc; } Comp;
#define COMP_MAX 48
#define COMP_TOPE (450 * 1024)
static Comp comp[COMP_MAX];
static u32 comp_uso, comp_total;
static u32 fnv(const char *t, u32 n, u32 h) { u32 k; for (k = 0; k < n; k++) h = (h ^ (u8)t[k]) * 16777619u; return h; }
static Comp *comp_busca(u32 clave, u32 largo) {
  int k;
  for (k = 0; k < COMP_MAX; k++) if (comp[k].bc && comp[k].clave == clave && comp[k].largo == largo) { comp[k].uso = ++comp_uso; return &comp[k]; }
  return NULL;
}
static void comp_guarda(u32 clave, u32 largo, u8 *bc0, size_t n) {
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
/* compila (o saca de lo guardado) y ejecuta un guion */
static void ejecuta(const char *codigo, u32 n, const char *nombre, u32 clave, u32 largo) {
  Comp *k = comp_busca(clave, largo);
  JSValue f;
  if (k) f = JS_ReadObject(cx, k->bc, k->n_bc, JS_READ_OBJ_BYTECODE);
  else {
    char *z = malloc(n + 1);                     /* (QuickJS quiere el 0 del final) */
    if (!z) return;
    memcpy(z, codigo, n); z[n] = 0;
    f = JS_Eval(cx, z, n, nombre, JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY);
    free(z);
    if (!JS_IsException(f)) {
      size_t nb;
      u8 *bc = JS_WriteObject(cx, &nb, f, JS_WRITE_OBJ_BYTECODE);
      if (bc) comp_guarda(clave, largo, bc, nb);
    }
  }
  if (JS_IsException(f)) { excepcion(); return; }
  if (!nivel++) reinicia_tope();
  f = JS_EvalFunction(cx, f);
  if (JS_IsException(f)) excepcion();
  JS_FreeValue(cx, f);
  if (!--nivel) fin_turno();
}

/* =============================================================================
 *  Los guiones de la página, en orden: primero los normales (los de fuera se
 *  esperan), luego los «defer» (y «async»); después, DOMContentLoaded y load
 * ============================================================================= */
typedef struct { Nodo *n; char *url; char *codigo; u32 n_codigo; u8 estado; } Guion;   /* estado: 0 en la red, 1 listo, 2 hecho */
static Guion *gs;
static int n_gs, cap_gs, i_gs, fase_carga;
static u32 t_pagina;
static int es_js(const Nodo *n) {
  const char *t = atr(n, "type");
  if (!t || !t[0]) return 1;
  return !_stricmp(t, "text/javascript") || !_stricmp(t, "application/javascript") || !_stricmp(t, "module")
      || !_stricmp(t, "text/ecmascript") || !_stricmp(t, "application/ecmascript");
}
static u32 clave_url(const char *u);
static Guion *guion_nuevo(Nodo *n) {
  Guion *g;
  const char *src = atr(n, "src");
  Env *e = env_pon(n);
  if (n_gs == cap_gs) {
    Guion *q = realloc(gs, (cap_gs + 16) * sizeof(Guion));
    if (!q) return NULL;
    gs = q; cap_gs += 16;
  }
  if (e) e->hecho = 1;
  g = &gs[n_gs++];
  memset(g, 0, sizeof *g);
  g->n = n;
  if (src && src[0]) {
    g->url = url_absoluta(pg->doc.base ? pg->doc.base : pg->url, src);
    if (!g->url) { g->estado = 2; return g; }
    {
      /* ¿ya compilado? entonces no hace falta pedirlo */
      if (comp_busca(clave_url(g->url), 0xFFFFFFFF)) g->estado = 1;
      else if (!encola(-1, n_gs - 1, "GET", g->url, "Accept: */*\r\n", NULL, 0, 2)) g->estado = 2;
    }
  } else {
    if (n->hijo && n->hijo->texto) { g->codigo = n->hijo->t; g->n_codigo = n->hijo->n; }
    g->estado = 1;
  }
  return g;
}
static void corre_guion(Guion *g) {
  JSValue el = envuelve(g->n);
  char nombre[300];
  u32 clave, largo;
  JS_FreeValue(cx, llama(fn_guion, 1, (JSValueConst *)&el));
  if (g->url) { sprintf(nombre, "%.290s", g->url); clave = fnv(g->url, strlen(g->url), 2166136261u); largo = 0xFFFFFFFF; }
  else { sprintf(nombre, "%.200s (en la pagina)", pg->url); clave = fnv(g->codigo ? g->codigo : "", g->n_codigo, 2166136261u); largo = g->n_codigo; }
  if (g->url || g->n_codigo) ejecuta(g->codigo ? g->codigo : "", g->n_codigo, nombre, clave, largo);
  if (cx) {
    JSValue nul = JS_NULL;
    JS_FreeValue(cx, llama(fn_guion, 1, &nul));
    if (g->url && cx) {
      /* el evento load del <script> */
      JSValue args[2] = { el, JS_NewString(cx, "load") };
      JS_FreeValue(cx, llama(fn_ev, 2, args));
      JS_FreeValue(cx, args[1]);
    }
  }
  JS_FreeValue(cx, el);
  g->estado = 2;
  if (g->url) { free(g->codigo); g->codigo = NULL; }
}
static u32 clave_url(const char *u) { return fnv(u, strlen(u), 2166136261u); }
/* ¿se puede ejecutar ya? (uno de fuera que estaba compilado y se ha soltado: a pedirlo otra vez) */
static int listo(Guion *g) {
  if (g->estado != 1) return 0;
  if (g->url && !g->codigo && !comp_busca(clave_url(g->url), 0xFFFFFFFF)) {
    g->estado = encola(-1, (int)(g - gs), "GET", g->url, "Accept: */*\r\n", NULL, 0, 2) ? 0 : 2;
    return 0;
  }
  return 1;
}
static void avanza_guiones(void) {
  int k;
  while (cx && i_gs < n_gs && fase_carga == 0) {
    Guion *g = &gs[i_gs];
    if (g->estado == 0) return;
    if (listo(g)) corre_guion(g);
    if (g->estado != 2) return;
    if (!cx) return;
    i_gs++;
  }
  /* los dinámicos (los que pone un guion), cuando lleguen */
  for (k = i_gs; cx && fase_carga && k < n_gs; k++) if (listo(&gs[k])) corre_guion(&gs[k]);
  if (cx && i_gs >= n_gs && fase_carga == 0) {
    JSValue uno = JS_NewInt32(cx, 1);
    fase_carga = 1;
    JS_FreeValue(cx, llama(fn_cargado, 1, &uno));
  }
  if (cx && fase_carga == 1 && !jp_activa && !n_cola) {
    JSValue dos = JS_NewInt32(cx, 2);
    u32 uso, max, sis;
    char b[160];
    fase_carga = 2;
    JS_FreeValue(cx, llama(fn_cargado, 1, &dos));
    HjMemoria(&uso, &max, &sis);
    sprintf(b, "JavaScript: %u KB en uso (%u KB como mucho), %u KB pedidos al sistema; %u ms desde que llego la pagina",
            (uso + 512) / 1024, (max + 512) / 1024, (sis + 512) / 1024, GetTickCount() - t_pagina);
    reg("[memoria] ", b, strlen(b));
    reg_vacia();
  }
}
/* un <script> que un guion mete en la página */
static void guion_dinamico(Nodo *n) {
  Env *e = env_de(n);
  Guion *g;
  if ((e && e->hecho) || !es_js(n)) return;
  if (!atr(n, "src") && !(n->hijo && n->hijo->texto)) return;    /* (vacío: no se mira más) */
  if (!(g = guion_nuevo(n))) return;
  if (fase_carga && g->estado == 1 && !g->url) corre_guion(g);    /* uno de dentro: ya */
}

/* =============================================================================
 *  Lo que llama navega.c
 * ============================================================================= */
/* un motor nuevo (uno por página: al irse, se suelta entero y su memoria
 * vuelve al sistema con HjReinicia) */
static int rt_nuevo(void) {
  static JSClassDef def = { "Nodo", NULL, NULL, NULL, NULL };
  rt = JS_NewRuntime();
  if (!rt) return 0;
  JS_SetMemoryLimit(rt, (u32)js_memoria_kb * 1024);
  JS_SetMaxStackSize(rt, 120 * 1024);
  JS_SetInterruptHandler(rt, corta, NULL);
  JS_SetHostPromiseRejectionTracker(rt, rechazo, NULL);
  if (!clase_nodo) JS_NewClassID(&clase_nodo);
  JS_NewClass(rt, clase_nodo, &def);
  return 1;
}
static void rt_suelta(void) {
  if (!rt) return;
  JS_FreeRuntime(rt);
  rt = NULL;
  HjReinicia();
}
void js_inicia(HANDLE v, const JsNav *n, const char *carp) {
  JSContext *c;
  JSValue f;
  vent = v; nav = n;
  sprintf(carpeta, "%.150s", carp);
  {
    char ruta[200];
    MFILE *fl;
    sprintf(ruta, "%sNAVEGA.LOG", carpeta);
    if ((fl = fopen(ruta, "wb"))) fclose(fl);    /* el registro, nuevo cada vez */
  }
  if (!js_activo) return;
  if (!rt_nuevo()) { js_activo = 0; return; }
  /* el preludio, compilado una vez (y guardado en la memoria del Navegador) */
  c = JS_NewContext(rt);
  f = c ? JS_Eval(c, dom_js, sizeof dom_js - 1, "dom.js", JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_COMPILE_ONLY) : JS_EXCEPTION;
  if (!c) js_activo = 0;
  else if (JS_IsException(f)) {
    JSValue e = JS_GetException(c);
    const char *t = JS_ToCString(c, e);
    reg("[error] dom.js: ", t ? t : "?", t ? strlen(t) : 1);
    if (t) JS_FreeCString(c, t);
    JS_FreeValue(c, e);
    js_activo = 0;
  } else {
    size_t nb;
    u8 *bc = JS_WriteObject(c, &nb, f, JS_WRITE_OBJ_BYTECODE);
    if (bc && (preludio_bc = malloc(nb))) { memcpy(preludio_bc, bc, nb); n_preludio_bc = nb; }
    if (bc) js_free_rt(rt, bc);
    JS_FreeValue(c, f);
  }
  if (c) JS_FreeContext(c);
  rt_suelta();
  reg_vacia();
}
void js_fin(void) {
  int k;
  js_suelta();
  for (k = 0; k < COMP_MAX; k++) if (comp[k].bc) { free(comp[k].bc); comp[k].bc = NULL; }
  free(preludio_bc);
  preludio_bc = NULL;
  reg_vacia();
}
/* ¿hace falta JavaScript en esta página? (algún <script> u onalgo="...") */
static int pide_js(const Nodo *n) {
  const Nodo *h;
  int k;
  if (n->texto) return 0;
  if (n->etq == E_SCRIPT && es_js(n)) return 1;
  for (k = 0; k < n->n_atr; k++) if (n->atr[2 * k][0] == 'o' && n->atr[2 * k][1] == 'n') return 1;
  for (h = n->hijo; h; h = h->sig) if (pide_js(h)) return 1;
  return 0;
}
static void recoge(Nodo *n, int diferidos) {
  Nodo *h;
  if (n->texto) return;
  /* con JavaScript, <noscript> es como si fuera texto: lo de dentro ni se ve
   * ni existe (sus <input> no van en el formulario: ClicGest pone ahí
   * «_cg_sin_js» para preguntar él cuando no hay confirm()) */
  if (n->etq == E_NOSCRIPT && !diferidos) {
    dom_pon_atr(n, "hidden", "");
    while (n->hijo) dom_quita(n->hijo);
    return;
  }
  if (n->etq == E_SCRIPT && es_js(n)) {
    int dif = atr(n, "src") && (atr(n, "defer") || atr(n, "async") || ((atr(n, "type") && !_stricmp(atr(n, "type"), "module"))));
    if (dif == diferidos) guion_nuevo(n);
    return;
  }
  for (h = n->hijo; h; h = h->sig) recoge(h, diferidos);
}
static int clicable(Nodo *n);
void js_pagina(Pagina *p) {
  JSValue g, nobj, f, r;
  js_suelta();
  if (!js_activo || !preludio_bc || !p) return;
  t_pagina = GetTickCount();
  if (!pide_js(p->doc.raiz)) return;
  pg = p; dom_pag = p;
  dom_prepara(&p->doc);
  if (!rt_nuevo()) return;
  cx = JS_NewContext(rt);
  if (!cx) { rt_suelta(); return; }
  g = JS_GetGlobalObject(cx);
  nobj = JS_NewObject(cx);
  JS_SetPropertyFunctionList(cx, nobj, funciones_n, sizeof funciones_n / sizeof funciones_n[0]);
  f = JS_ReadObject(cx, preludio_bc, n_preludio_bc, JS_READ_OBJ_BYTECODE);
  nivel++;
  reinicia_tope();
  if (!JS_IsException(f)) f = JS_EvalFunction(cx, f);
  if (!JS_IsException(f)) { r = JS_Call(cx, f, JS_UNDEFINED, 1, (JSValueConst *)&nobj); JS_FreeValue(cx, f); f = r; }
  nivel--;
  JS_FreeValue(cx, nobj);
  if (JS_IsException(f)) {
    JSValue e = JS_GetException(cx);
    const char *t = JS_ToCString(cx, e);
    reg("[error] dom.js: ", t ? t : "?", t ? strlen(t) : 1);
    if (t) JS_FreeCString(cx, t);
    JS_FreeValue(cx, e);
    JS_FreeValue(cx, g);
    js_suelta();
    return;
  }
  JS_FreeValue(cx, f);
  fn_evento = JS_GetPropertyStr(cx, g, "__evento");
  fn_tic = JS_GetPropertyStr(cx, g, "__tic");
  fn_llega = JS_GetPropertyStr(cx, g, "__llega");
  fn_foco = JS_GetPropertyStr(cx, g, "__foco");
  fn_cargado = JS_GetPropertyStr(cx, g, "__cargado");
  fn_guion = JS_GetPropertyStr(cx, g, "__guion");
  fn_informa = JS_GetPropertyStr(cx, g, "__informa");
  fn_ev = JS_GetPropertyStr(cx, g, "__ev");
  JS_FreeValue(cx, g);
  pag_clicable = clicable;
  {
    char b[300];
    sprintf(b, "%.280s", p->url);
    reg("== ", b, strlen(b));
  }
  recoge(p->doc.raiz, 0);
  recoge(p->doc.raiz, 1);
  pag_ctl_rehaz(p, 1);
  dom_sucio = 1;
  i_gs = 0; fase_carga = 0;
  nivel++;
  avanza_guiones();
  nivel--;
  if (cx) fin_turno();
}
void js_suelta(void) {
  u32 k;
  int j;
  if (jp_activa) { http_suelta(&jp); pet_suelta(&en_marcha); jp_activa = 0; }
  for (j = 0; j < n_cola; j++) pet_suelta(&cola[j]);
  n_cola = 0;
  n_temps = 0;
  if (vent) temp_ajusta();
  for (j = 0; j < n_gs; j++) if (gs[j].url) { free(gs[j].url); free(gs[j].codigo); }
  free(gs); gs = NULL; n_gs = cap_gs = i_gs = 0;
  pag_clicable = NULL;
  a_ver = NULL;
  if (!cx) { free(tabla); tabla = NULL; n_tabla = cap_tabla = 0; return; }
  for (k = 0; k < cap_tabla; k++) if (tabla[k].n) JS_FreeValue(cx, tabla[k].v);
  free(tabla); tabla = NULL; n_tabla = cap_tabla = 0;
  JS_FreeValue(cx, proto_el); JS_FreeValue(cx, proto_tx); JS_FreeValue(cx, proto_doc);
  JS_FreeValue(cx, fn_evento); JS_FreeValue(cx, fn_tic); JS_FreeValue(cx, fn_llega); JS_FreeValue(cx, fn_foco);
  JS_FreeValue(cx, fn_cargado); JS_FreeValue(cx, fn_guion); JS_FreeValue(cx, fn_informa); JS_FreeValue(cx, fn_ev);
  proto_el = proto_tx = proto_doc = fn_evento = fn_tic = fn_llega = fn_foco = fn_cargado = fn_guion = fn_informa = fn_ev = JS_UNDEFINED;
  JS_FreeContext(cx);
  cx = NULL;
  rt_suelta();
  reg_vacia();
  pg = NULL;
}
void js_para_red(void) {
  int j;
  if (jp_activa) { http_suelta(&jp); pet_suelta(&en_marcha); jp_activa = 0; }
  for (j = 0; j < n_cola; j++) pet_suelta(&cola[j]);
  n_cola = 0;
}
int js_ocupado(void) {
  if (!cx) return 0;
  {
    int k;
    if (jp_activa || n_cola || fase_carga == 1) return 1;
    for (k = i_gs; k < n_gs; k++) if (gs[k].estado == 1) return 1;
  }
  return 0;
}
/* la respuesta de una petición: a su guion o a fetch */
static void entrega(int ok) {
  PetJs *p = &en_marcha;
  if (p->guion >= 0) {
    if (p->guion < n_gs) {
      Guion *g = &gs[p->guion];
      if (ok && jp.estado == 200) {
        g->codigo = (char *)jp.datos; g->n_codigo = jp.n_datos;
        jp.datos = NULL;
        g->estado = 1;
      } else {
        char b[400];
        sprintf(b, "no se ha podido cargar %.300s (%d %.60s)", g->url, ok ? jp.estado : 0, ok ? "" : jp.error);
        reg("[error] ", b, strlen(b));
        g->estado = 2;
      }
    }
  } else if (cx) {
    JSValue args[5];
    int k;
    char *vis;
    args[0] = JS_NewInt32(cx, p->id);
    args[1] = JS_NewInt32(cx, ok ? jp.estado : 0);
    args[2] = a_js(p->url, -1);
    /* (las cabeceras, sin Set-Cookie; de otro origen, solo las que deja CORS) */
    vis = ok && jp.cab ? cors_visibles(jp.cab, p->otro, !jp.sin_galletas) : NULL;
    args[3] = vis ? JS_NewString(cx, vis + (vis[0] == '\n')) : JS_NewString(cx, "");
    free(vis);
    args[4] = ok ? (jp.datos ? JS_NewStringLen(cx, (char *)jp.datos, jp.n_datos) : JS_NewString(cx, "")) : a_js(jp.error, -1);
    JS_FreeValue(cx, llama(fn_llega, 5, args));
    for (k = 0; k < 5; k++) JS_FreeValue(cx, args[k]);
  }
}
/* [JS] lo que no se deja pedir o leer (CORS): a fetch, como un error de la
 * red, y al registro (la consola), con por qué */
static void js_rechaza(const char *por) {
  char b[400];
  sprintf(b, "%.200s: %.190s", en_marcha.url ? en_marcha.url : "", por);
  reg("[error] ", b, strlen(b));
  memset(&jp, 0, sizeof jp);
  sprintf(jp.error, "%.190s", por);
  entrega(0);
  pet_suelta(&en_marcha);
}
/* el origen de la página (0: no tiene, «null») */
static int js_origen(char *o) { return pg && cors_origen(pg->url, o, 160); }
void js_avanza(void) {
  int r;
  char o[160];
  if (!cx) return;
  if (!jp_activa && n_cola) {
    en_marcha = cola[0];
    memmove(cola, cola + 1, (n_cola - 1) * sizeof cola[0]);
    n_cola--;
  empieza:
    memset(&jp, 0, sizeof jp);
    if (en_marcha.guion < 0) {
      /* fetch y XMLHttpRequest: a otro origen (o tras pasar por otro), sin las
       * cabeceras que no sean «simples» ni otro método que GET, HEAD o POST
       * (no se pregunta antes con OPTIONS); y nunca las prohibidas (Cookie,
       * Host, Origin...). docs/486/NAVEGADOR.md, «Lo que puede pedir una página» */
      char *cab = NULL;
      const char *por;
      int hay = js_origen(o);
      if (!pg || !cors_mismo(pg->url, en_marcha.url)) en_marcha.otro = 1;
      por = cors_pide(en_marcha.metodo, en_marcha.cab, en_marcha.otro, hay ? o : NULL, &cab);
      if (por) { js_rechaza(por); return; }
      free(en_marcha.cab);
      en_marcha.cab = cab;
    }
    jp.url = copia(en_marcha.url, -1);
    jp.post = en_marcha.post;
    jp.cuerpo = en_marcha.post ? copia(en_marcha.cuerpo ? en_marcha.cuerpo : "", en_marcha.n_cuerpo) : NULL;
    jp.n_cuerpo = en_marcha.post ? en_marcha.n_cuerpo : 0;
    /* (SameSite como el resto: lo que pide la página no es ir a otra; si es de
     * otro sitio, solo van las galletas SameSite=None) */
    jp.sub = 1;
    jp.cruzada = pg ? nav_cruzada(pg->url, jp.url) : 1;
    http_empieza(&jp);
    /* las galletas: con «include» (o withCredentials), las que toquen
     * (SameSite, arriba); con «same-origin», solo al mismo origen; con
     * «omit», nunca. Sin ellas, tampoco se guardan las que mande */
    jp.sin_galletas = en_marcha.guion < 0 && !(en_marcha.cred == 2 || (en_marcha.cred == 1 && !en_marcha.otro));
    jp.js_cab = en_marcha.cab;
    jp.js_metodo = strcmp(en_marcha.metodo, "GET") && strcmp(en_marcha.metodo, "POST") ? en_marcha.metodo : NULL;
    jp_activa = 1;
    return;
  }
  if (jp_activa) {
    r = http_avanza(&jp);
    if (r == 0) return;
    jp_activa = 0;
    /* de otro origen: la respuesta (también la de una redirección) solo si
     * el servidor la deja leer (Access-Control-Allow-Origin...) */
    if (r > 0 && en_marcha.guion < 0 && en_marcha.otro && !cors_deja(jp.cab, js_origen(o) ? o : NULL, !jp.sin_galletas)) {
      http_suelta(&jp);
      js_rechaza("CORS: el servidor de otro origen no deja leer la respuesta (Access-Control-Allow-Origin)");
      return;
    }
    if (r > 0 && (jp.estado == 301 || jp.estado == 302 || jp.estado == 303 || jp.estado == 307 || jp.estado == 308)
        && http_cabecera(&jp, "Location") && en_marcha.redirs < 10) {
      const char *l = http_cabecera(&jp, "Location");
      char loc[1024], *nueva;
      int k;
      for (k = 0; l[k] && l[k] != '\r' && l[k] != '\n' && k < 1022; k++) loc[k] = l[k];
      loc[k] = 0;
      nueva = url_absoluta(en_marcha.url, loc);
      k = jp.estado;
      http_suelta(&jp);
      if (nueva) {
        free(en_marcha.url);
        en_marcha.url = nueva;
        en_marcha.redirs++;
        if (k != 307 && k != 308) {          /* (el estado, de antes de soltar jp) */
          en_marcha.post = 0;
          free(en_marcha.metodo); en_marcha.metodo = copia("GET", -1);
        }
        goto empieza;
      }
      pet_suelta(&en_marcha);
      return;
    }
    entrega(r > 0);
    http_suelta(&jp);
    pet_suelta(&en_marcha);
  }
  if (cx) avanza_guiones();
}
void js_tic(void) {
  u32 ahora = GetTickCount();
  int k, ids[TEMP_MAX], n = 0;
  if (!cx || nivel) return;
  for (k = 0; k < n_temps; ) {
    if ((int)(ahora - temps[k].cuando) >= 0) { ids[n++] = temps[k].id; temps[k] = temps[--n_temps]; }
    else k++;
  }
  /* (de menor a mayor: el orden en que se pidieron) */
  for (k = 1; k < n; k++) { int x = ids[k], j = k; while (j > 0 && ids[j - 1] > x) { ids[j] = ids[j - 1]; j--; } ids[j] = x; }
  for (k = 0; k < n && cx; k++) {
    JSValue id = JS_NewInt32(cx, ids[k]);
    JS_FreeValue(cx, llama(fn_tic, 1, &id));
  }
  if (cx) temp_ajusta();
}
int js_evento(Nodo *n, const char *tipo, Nodo *otro) {
  JSValue args[3], r;
  int k, si;
  if (!cx || !n) return 0;
  args[0] = envuelve(n);
  args[1] = JS_NewString(cx, tipo);
  args[2] = otro ? envuelve(otro) : JS_NewInt32(cx, 0);
  r = llama(fn_evento, 3, args);
  si = cx ? JS_ToBool(cx, r) == 1 : 0;
  if (cx) { JS_FreeValue(cx, r); for (k = 0; k < 3; k++) JS_FreeValue(cx, args[k]); }
  return si;
}
int js_tecla(Nodo *n, int vk, int ch) {
  JSValue args[4], r;
  int k, si;
  if (!cx) return 0;
  args[0] = n ? envuelve(n) : JS_NULL;
  args[1] = JS_NewString(cx, "keydown");
  args[2] = JS_NewInt32(cx, vk);
  args[3] = JS_NewInt32(cx, ch);
  r = llama(fn_evento, 4, args);
  si = cx ? JS_ToBool(cx, r) == 1 : 0;
  if (cx) { JS_FreeValue(cx, r); for (k = 0; k < 4; k++) JS_FreeValue(cx, args[k]); }
  return si;
}
void js_foco(Nodo *n) {
  JSValue el;
  if (!cx) return;
  el = n ? envuelve(n) : JS_NULL;
  JS_FreeValue(cx, llama(fn_foco, 1, &el));
  if (cx) JS_FreeValue(cx, el);
}
int js_enlace(const char *href) {
  char *t;
  int k, j = 0;
  JSValue r;
  if (!cx || _strnicmp(href, "javascript:", 11)) return 0;
  t = malloc(strlen(href) * 3 + 1);
  if (!t) return 1;
  /* (%XX de la dirección, y a UTF-8) */
  {
    char *d = malloc(strlen(href) + 1);
    if (!d) { free(t); return 1; }
    for (k = 11; href[k]; k++) {
      if (href[k] == '%' && href[k + 1] && href[k + 2]) {
        int a = href[k + 1], b = href[k + 2];
        a = a <= '9' ? a - '0' : (a | 32) - 'a' + 10; b = b <= '9' ? b - '0' : (b | 32) - 'a' + 10;
        d[j++] = (char)(a * 16 + b); k += 2;
      } else d[j++] = href[k];
    }
    d[j] = 0;
    j = a_utf8(d, j, t);
    t[j] = 0;
    free(d);
  }
  if (!nivel++) reinicia_tope();
  r = JS_Eval(cx, t, j, "javascript:", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(r)) excepcion();
  JS_FreeValue(cx, r);
  if (!--nivel) fin_turno();
  free(t);
  return 1;
}
int js_en_pagina(void) { return cx != NULL; }
void js_redimensiona(void) {
  JSValue tres;
  if (!cx) return;
  tres = JS_NewInt32(cx, 3);
  JS_FreeValue(cx, llama(fn_cargado, 1, &tres));
}
/* ¿se puede pulsar este elemento? (tiene un clic de JavaScript, o es una
 * fila o un elemento de una lista cuyo contenedor lo tiene) */
static int clicable(Nodo *n) {
  Env *e;
  Nodo *p;
  const char *r;
  if (!cx || n->texto) return 0;
  if (n->etq == E_INPUT || n->etq == E_SELECT || n->etq == E_TEXTAREA || n->etq == E_BUTTON || n->etq == E_A
      || n->etq == E_LABEL || n->etq == E_BODY || n->etq == E_HTML || n->etq == E_SUMMARY || n->etq == E_DETAILS || n == pg->doc.raiz) return 0;
  if (atr(n, "onclick") || atr(n, "onmousedown")) return 1;
  if ((e = env_de(n)) && e->escucha) return 1;
  r = atr(n, "role");
  if (n->etq == E_LI || n->etq == E_TR || (r && (!strcmp(r, "option") || !strcmp(r, "button") || !strcmp(r, "tab") || !strcmp(r, "menuitem"))))
    for (p = n->padre; p && p != pg->doc.body && p->etq != E_BODY && p != pg->doc.raiz; p = p->padre)
      if ((e = env_de(p)) && e->escucha) return 1;
  return 0;
}
