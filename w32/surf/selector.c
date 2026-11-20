/* =============================================================================
 *  selector.c - los selectores de CSS de querySelector, matches y closest en
 *  el Navegador completo de Hispa-486 (NAVEGA2.EXE), sobre el árbol de libdom.
 *  Es el de w32/navega/dom.c (el Navegador propio) con los nodos de libdom.
 *  Se entienden: etiqueta, *, #id, .clase, [atr], [atr=v], ~= |= ^= $= *= (y
 *  la «i» del final), los combinadores « », >, + y ~, las listas con comas y
 *  las seudoclases :not() :is() :where() :has() :first-child :last-child
 *  :only-child :nth-child() :nth-last-child() :first-of-type :last-of-type
 *  :checked :disabled :enabled :empty :root :scope (y :hover, :focus,
 *  :visited... que no casan). docs/486/NAVEGADOR2.md, «JavaScript».
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dom/dom.h>
#include "guion.h"

typedef dom_node Nodo;
typedef unsigned char u8;
typedef unsigned int u32;

/* ---- los nombres de atributo, ya «internados» en libdom (se buscan rápido) ---- */
#define NOMS 128
static struct { char n[32]; dom_string *d; } noms[NOMS];
static int n_noms;
static dom_string *nom_dom(const char *n) {
  int k;
  dom_string *d;
  for (k = 0; k < n_noms; k++) if (!strcmp(noms[k].n, n)) return noms[k].d;
  if (dom_string_create_interned((const uint8_t *)n, strlen(n), &d) != DOM_NO_ERR) return NULL;
  if (n_noms < NOMS && strlen(n) < 32) { strcpy(noms[n_noms].n, n); noms[n_noms++].d = d; return d; }
  return d;              /* (no cabe: se queda; son pocos, los de la página) */
}
void sel_fin(void) {
  int k;
  for (k = 0; k < n_noms; k++) dom_string_unref(noms[k].d);
  n_noms = 0;
}
dom_string *sel_atr(Nodo *n, const char *nombre) {
  dom_string *v = NULL, *d;
  if (!ES_ELEM(n) || !(d = nom_dom(nombre))) return NULL;
  if (dom_element_get_attribute(n, d, &v) != DOM_NO_ERR) return NULL;
  return v;
}
int sel_etq(Nodo *n, const char *nombre) {
  dom_string *s;
  size_t l;
  if (!ES_ELEM(n) || !(s = NI(n)->nombre)) return 0;
  l = dom_string_byte_length(s);
  return l == strlen(nombre) && !strncasecmp(dom_string_data(s), nombre, l);
}

/* ---- el selector, ya leído ---- */
typedef struct SLista SLista;
typedef struct {
  u8 que;                /* 'i' id, 'c' clase, 'a' atributo, 'p' seudoclase */
  u8 op;                 /* atributo: 0 existe, '=', '~', '|', '^', '$', '*' ; seudo: su código */
  u8 ci;                 /* sin mirar mayúsculas */
  char *n, *v;
  lwc_string *l;         /* la clase, para dom_element_has_class */
  SLista *sub;
  int a, b;              /* :nth-child(an+b) */
} Cond;
#define CONDS 10
typedef struct { char *etq; int nc; u8 comb; Cond c[CONDS]; } Comp;
typedef struct { int n; Comp *k; } Compleja;
struct SLista { int n; Compleja *c; };
#define LWC_MAX 64
typedef struct { char *p, *fin; int malo; lwc_string *lw[LWC_MAX]; int n_lw; } Pool;
static void *pide(Pool *po, u32 n) {
  void *r;
  n = (n + 3) & ~3u;
  if (po->p + n > po->fin) { po->malo = 1; return NULL; }
  r = po->p; po->p += n;
  memset(r, 0, n);
  return r;
}
enum { P_FIRST = 1, P_LAST, P_ONLY, P_NTH, P_NTHL, P_FTYPE, P_LTYPE, P_CHECKED, P_DISABLED, P_ENABLED, P_EMPTY,
       P_ROOT, P_SCOPE, P_NOT, P_IS, P_HAS, P_NUNCA };
static int es_nom(int c) { return ((c | 32) >= 'a' && (c | 32) <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || (u8)c >= 0x80 || c == '\\'; }
static const char *blancos(const char *s) { while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++; return s; }
static char *nombre_de(Pool *po, const char **s) {
  char b[200];
  int n = 0;
  while (es_nom(**s) && n < 199) {
    if (**s == '\\' && (*s)[1]) (*s)++;
    b[n++] = *(*s)++;
  }
  if (!n) { po->malo = 1; return NULL; }
  {
    char *r = pide(po, n + 1);
    if (r) memcpy(r, b, n);
    return r;
  }
}
static SLista *lista_de(Pool *po, const char **s, int hasta);
static void nth(const char *t, int *a, int *b) {
  /* an+b, odd, even, n, -n+3, 5 */
  int signo = 1, v = 0, hay = 0;
  t = blancos(t);
  if (!strncasecmp(t, "odd", 3)) { *a = 2; *b = 1; return; }
  if (!strncasecmp(t, "even", 4)) { *a = 2; *b = 0; return; }
  *a = 0; *b = 0;
  if (*t == '-') { signo = -1; t++; } else if (*t == '+') t++;
  while (*t >= '0' && *t <= '9') { v = v * 10 + *t++ - '0'; hay = 1; }
  if ((*t | 32) == 'n') {
    *a = signo * (hay ? v : 1);
    t = blancos(t + 1);
    signo = 1;
    if (*t == '-') { signo = -1; t++; } else if (*t == '+') t++;
    t = blancos(t);
    v = 0;
    while (*t >= '0' && *t <= '9') v = v * 10 + *t++ - '0';
    *b = signo * v;
  } else *b = signo * v;
}
static int comp_de(Pool *po, const char **s, Comp *k) {
  const char *t = *s;
  int algo = 0;
  k->etq = NULL;
  if (*t == '*') { t++; algo = 1; }
  else if (es_nom(*t)) {
    if (!(k->etq = nombre_de(po, &t))) return 0;
    algo = 1;
  }
  for (;;) {
    Cond *c;
    if (*t != '#' && *t != '.' && *t != '[' && *t != ':') break;
    if (k->nc >= CONDS) { po->malo = 1; return 0; }
    c = &k->c[k->nc++];
    algo = 1;
    if (*t == '#' || *t == '.') {
      c->que = *t == '#' ? 'i' : 'c';
      t++;
      if (!(c->n = nombre_de(po, &t))) return 0;
      if (c->que == 'c') {
        if (po->n_lw >= LWC_MAX || lwc_intern_string(c->n, strlen(c->n), &c->l) != lwc_error_ok) { po->malo = 1; return 0; }
        po->lw[po->n_lw++] = c->l;
      }
    } else if (*t == '[') {
      t = blancos(t + 1);
      c->que = 'a';
      if (!(c->n = nombre_de(po, &t))) return 0;
      { char *q; for (q = c->n; *q; q++) if (*q >= 'A' && *q <= 'Z') *q += 32; }
      t = blancos(t);
      if (*t == '=') { c->op = '='; t++; }
      else if ((*t == '~' || *t == '|' || *t == '^' || *t == '$' || *t == '*') && t[1] == '=') { c->op = *t; t += 2; }
      if (c->op) {
        char b[300];
        int n = 0;
        t = blancos(t);
        if (*t == '"' || *t == '\'') {
          char q = *t++;
          while (*t && *t != q && n < 299) { if (*t == '\\' && t[1]) t++; b[n++] = *t++; }
          if (*t != q) { po->malo = 1; return 0; }
          t++;
        } else while (es_nom(*t) && n < 299) b[n++] = *t++;
        c->v = pide(po, n + 1);
        if (!c->v) return 0;
        memcpy(c->v, b, n);
        t = blancos(t);
        if ((*t | 32) == 'i' && (t[1] == ']' || t[1] == ' ')) { c->ci = 1; t = blancos(t + 1); }
        else if ((*t | 32) == 's' && (t[1] == ']' || t[1] == ' ')) t = blancos(t + 1);
      }
      if (*t != ']') { po->malo = 1; return 0; }
      t++;
    } else {
      static const struct { const char *n; u8 c; } ps[] = {
        { "first-child", P_FIRST }, { "last-child", P_LAST }, { "only-child", P_ONLY }, { "nth-child", P_NTH },
        { "nth-last-child", P_NTHL }, { "first-of-type", P_FTYPE }, { "last-of-type", P_LTYPE },
        { "checked", P_CHECKED }, { "disabled", P_DISABLED }, { "enabled", P_ENABLED }, { "empty", P_EMPTY },
        { "root", P_ROOT }, { "scope", P_SCOPE }, { "not", P_NOT }, { "is", P_IS }, { "where", P_IS },
        { "matches", P_IS }, { "has", P_HAS }
      };
      char *n;
      u32 j;
      t++;
      if (*t == ':') t++;                         /* ::before... : no casa nada */
      if (!(n = nombre_de(po, &t))) return 0;
      c->que = 'p';
      c->op = P_NUNCA;
      for (j = 0; j < sizeof ps / sizeof ps[0]; j++) if (!strcasecmp(ps[j].n, n)) { c->op = ps[j].c; break; }
      if (*t == '(') {
        const char *ini = ++t;
        if (c->op == P_NOT || c->op == P_IS || c->op == P_HAS) {
          c->sub = lista_de(po, &t, ')');
          if (!c->sub) return 0;
        } else {
          int nivel = 1;
          while (*t && nivel) { if (*t == '(') nivel++; else if (*t == ')' && --nivel == 0) break; t++; }
          if (c->op == P_NTH || c->op == P_NTHL) nth(ini, &c->a, &c->b);
        }
        if (*t != ')') { po->malo = 1; return 0; }
        t++;
      } else if (c->op == P_NTH || c->op == P_NTHL || c->op == P_NOT || c->op == P_IS || c->op == P_HAS) { po->malo = 1; return 0; }
    }
  }
  *s = t;
  if (!algo) po->malo = 1;
  return algo;
}
/* una lista de selectores hasta «hasta» (0 = el final) */
static SLista *lista_de(Pool *po, const char **s, int hasta) {
  SLista *l = pide(po, sizeof *l);
  const char *t = *s;
  if (!l) return NULL;
  for (;;) {
    Compleja *x;
    Comp ks[16];
    int nk = 0;
    u8 comb = 0;
    t = blancos(t);
    /* «> a» al principio (en :has): es relativo al de fuera */
    if (*t == '>' || *t == '+' || *t == '~') { comb = *t; t = blancos(t + 1); }
    for (;;) {
      if (nk >= 16) { po->malo = 1; return NULL; }
      memset(&ks[nk], 0, sizeof ks[nk]);
      ks[nk].comb = comb;
      if (!comp_de(po, &t, &ks[nk])) { po->malo = 1; return NULL; }
      nk++;
      {
        const char *u = blancos(t);
        if (*u == '>' || *u == '+' || *u == '~') { comb = *u; t = blancos(u + 1); continue; }
        if (*u == ',' || *u == hasta || !*u) { t = u; break; }
        if (u > t) { comb = ' '; t = u; continue; }
        po->malo = 1;
        return NULL;
      }
    }
    x = pide(po, sizeof(Compleja) * (l->n + 1));
    if (!x) return NULL;
    if (l->n) memcpy(x, l->c, sizeof(Compleja) * l->n);
    x[l->n].n = nk;
    x[l->n].k = pide(po, sizeof(Comp) * nk);
    if (!x[l->n].k) return NULL;
    memcpy(x[l->n].k, ks, sizeof(Comp) * nk);
    l->c = x;
    l->n++;
    if (*t == ',') { t++; continue; }
    break;
  }
  if (hasta && *t != hasta) { po->malo = 1; return NULL; }
  if (!hasta && *t) { po->malo = 1; return NULL; }
  *s = t;
  return l;
}

/* ---- casar ---- */
static int es_elem(const Nodo *n) { return ES_ELEM(n); }
static Nodo *padre_elem(const Nodo *n) { NodoI *p = NI(n)->padre; return es_elem((Nodo *)p) ? (Nodo *)p : NULL; }
static Nodo *ant_elem(const Nodo *n) { NodoI *p; for (p = NI(n)->ant; p && p->tipo != DOM_ELEMENT_NODE; p = p->ant) ; return (Nodo *)p; }
static int sl_casa(Nodo *n, SLista *l, Nodo *ambito);
static int dentro(const Nodo *a, const Nodo *n) { const NodoI *x; for (x = NI(n); x; x = x->padre) if (x == NI(a)) return 1; return 0; }
/* el siguiente en orden del documento, sin salir de raiz (NULL al acabar) */
static Nodo *sig_pre(Nodo *x0, Nodo *raiz) {
  NodoI *x = NI(x0);
  if (x->hijo) return (Nodo *)x->hijo;
  while (x != NI(raiz) && !x->sig) x = x->padre;
  return x == NI(raiz) ? NULL : (Nodo *)x->sig;
}
static int igual_ci(const char *a, const char *b, int ci) { return ci ? !strcasecmp(a, b) : !strcmp(a, b); }
static int empieza_ci(const char *a, const char *b, int ci) { u32 l = strlen(b); return ci ? !strncasecmp(a, b, l) : !strncmp(a, b, l); }
static int palabra(const char *t, const char *p, int ci) {
  u32 l = strlen(p);
  if (!l) return 0;
  while (*t) {
    while (*t == ' ' || *t == '\t' || *t == '\n') t++;
    if ((ci ? !strncasecmp(t, p, l) : !strncmp(t, p, l)) && (t[l] == 0 || t[l] == ' ' || t[l] == '\t' || t[l] == '\n')) return 1;
    while (*t && *t != ' ' && *t != '\t' && *t != '\n') t++;
  }
  return 0;
}
static int tiene_atr(Nodo *n, const char *a) { dom_string *v = sel_atr(n, a); if (v) dom_string_unref(v); return v != NULL; }
static int marcado(Nodo *n) {
  bool b = false;
  if (sel_etq(n, "input")) { dom_html_input_element_get_checked((dom_html_input_element *)n, &b); return b; }
  if (sel_etq(n, "option")) { dom_html_option_element_get_selected((dom_html_option_element *)n, &b); return b; }
  return 0;
}
static int misma_etq(const Nodo *a, const Nodo *b) {
  dom_string *x = NI(a)->nombre, *y = NI(b)->nombre;
  return x && y && dom_string_caseless_isequal(x, y);
}
static int posicion(Nodo *n, int desde_fin, int del_tipo) {   /* 1, 2, 3... entre sus hermanos */
  NodoI *h;
  int k = 0, aqui = 0;
  if (!NI(n)->padre) return 1;
  for (h = NI(n)->padre->hijo; h; h = h->sig) {
    if (h->tipo != DOM_ELEMENT_NODE || (del_tipo && !misma_etq((Nodo *)h, n))) continue;
    k++;
    if (h == NI(n)) aqui = k;
  }
  return desde_fin ? k - aqui + 1 : aqui;
}
static int nth_vale(int pos, int a, int b) {
  if (!a) return pos == b;
  return (pos - b) / a >= 0 && (pos - b) % a == 0;
}
static int casa_atr(Nodo *n, Cond *c) {
  dom_string *ds = sel_atr(n, c->n);
  const char *v;
  int r = 0;
  if (!ds) return 0;
  v = dom_string_data(ds);
  switch (c->op) {
  case 0: r = 1; break;
  case '=': r = igual_ci(v, c->v, c->ci); break;
  case '~': r = palabra(v, c->v, c->ci); break;
  case '|': { u32 l = strlen(c->v); r = igual_ci(v, c->v, c->ci) || (empieza_ci(v, c->v, c->ci) && v[l] == '-'); break; }
  case '^': r = c->v[0] && empieza_ci(v, c->v, c->ci); break;
  case '$': { u32 l = strlen(v), m = strlen(c->v); r = m && l >= m && igual_ci(v + l - m, c->v, c->ci); break; }
  case '*': {
    u32 l = strlen(c->v);
    if (l) for (; *v; v++) if (c->ci ? !strncasecmp(v, c->v, l) : !strncmp(v, c->v, l)) { r = 1; break; }
    break;
  }
  }
  dom_string_unref(ds);
  return r;
}
static int vacio(Nodo *n) {
  NodoI *h;
  for (h = NI(n)->hijo; h; h = h->sig) {
    if (h->tipo == DOM_ELEMENT_NODE) return 0;
    if (h->tipo == DOM_TEXT_NODE && h->valor && dom_string_byte_length(h->valor)) return 0;
  }
  return 1;
}
static int casa_cond(Nodo *n, Cond *c, Nodo *ambito) {
  switch (c->que) {
  case 'i': {
    dom_string *v = sel_atr(n, "id");
    int r = v && !strcmp(dom_string_data(v), c->n);
    if (v) dom_string_unref(v);
    return r;
  }
  case 'c': { bool m = false; dom_element_has_class(n, c->l, &m); return m; }
  case 'a': return casa_atr(n, c);
  case 'p':
    switch (c->op) {
    case P_FIRST: return posicion(n, 0, 0) == 1;
    case P_LAST: return posicion(n, 1, 0) == 1;
    case P_ONLY: return posicion(n, 0, 0) == 1 && posicion(n, 1, 0) == 1;
    case P_NTH: return nth_vale(posicion(n, 0, 0), c->a, c->b);
    case P_NTHL: return nth_vale(posicion(n, 1, 0), c->a, c->b);
    case P_FTYPE: return posicion(n, 0, 1) == 1;
    case P_LTYPE: return posicion(n, 1, 1) == 1;
    case P_CHECKED: return marcado(n);
    case P_DISABLED: case P_ENABLED: {
      int es = sel_etq(n, "input") || sel_etq(n, "button") || sel_etq(n, "select") || sel_etq(n, "textarea")
               || sel_etq(n, "option") || sel_etq(n, "fieldset"), des;
      if (!es) return 0;
      des = tiene_atr(n, "disabled");
      return c->op == P_DISABLED ? des : !des;
    }
    case P_EMPTY: return vacio(n);
    case P_ROOT: return NI(n)->padre && NI(n)->padre->tipo == DOM_DOCUMENT_NODE;
    case P_SCOPE: return ambito ? n == ambito : (NI(n)->padre && NI(n)->padre->tipo == DOM_DOCUMENT_NODE);
    case P_NOT: return !sl_casa(n, c->sub, ambito);
    case P_IS: return sl_casa(n, c->sub, ambito);
    case P_HAS: {
      /* algún descendiente (o hermano, con + y ~) que case, con n de ámbito */
      Nodo *x;
      int k;
      for (k = 0; k < c->sub->n; k++) {
        Compleja *cx = &c->sub->c[k];
        if (cx->k[0].comb == '+' || cx->k[0].comb == '~') {
          for (x = (Nodo *)NI(n)->sig; x; x = (Nodo *)NI(x)->sig) if (es_elem(x) && sl_casa(x, c->sub, n)) return 1;
          return 0;
        }
      }
      for (x = (Nodo *)NI(n)->hijo; x; x = sig_pre(x, n)) if (es_elem(x) && sl_casa(x, c->sub, n)) return 1;
      return 0;
    }
    }
    return 0;
  }
  return 0;
}
static int casa_comp(Nodo *n, Comp *k, Nodo *ambito) {
  int j;
  if (!es_elem(n)) return 0;
  if (k->etq && !sel_etq(n, k->etq)) return 0;
  for (j = 0; j < k->nc; j++) if (!casa_cond(n, &k->c[j], ambito)) return 0;
  return 1;
}
static int casa_desde(Nodo *n, Compleja *x, int i, Nodo *ambito) {
  Nodo *p;
  if (!casa_comp(n, &x->k[i], ambito)) return 0;
  if (i == 0) {
    /* relativo (:has(> a)): el de fuera, el ámbito */
    switch (x->k[0].comb) {
    case '>': return ambito && padre_elem(n) == ambito;
    case ' ': return ambito ? NI(n)->padre && dentro(ambito, (Nodo *)NI(n)->padre) : 1;
    case '+': return ambito && ant_elem(n) == ambito;
    case '~': for (p = ant_elem(n); p; p = ant_elem(p)) if (p == ambito) return 1; return 0;
    }
    return 1;
  }
  switch (x->k[i].comb) {
  case '>': return (p = padre_elem(n)) && casa_desde(p, x, i - 1, ambito);
  case ' ':
    for (p = padre_elem(n); p; p = padre_elem(p)) if (casa_desde(p, x, i - 1, ambito)) return 1;
    return 0;
  case '+':
    p = ant_elem(n);
    return p && casa_desde(p, x, i - 1, ambito);
  case '~':
    for (p = ant_elem(n); p; p = ant_elem(p)) if (casa_desde(p, x, i - 1, ambito)) return 1;
    return 0;
  }
  return 0;
}
static int sl_casa(Nodo *n, SLista *l, Nodo *ambito) {
  int k;
  for (k = 0; k < l->n; k++) if (casa_desde(n, &l->c[k], l->c[k].n - 1, ambito)) return 1;
  return 0;
}
static char pool_mem[16384];
static SLista *compila(const char *sel, Pool *po) {
  const char *s = sel;
  SLista *l;
  po->p = pool_mem; po->fin = pool_mem + sizeof pool_mem; po->malo = 0; po->n_lw = 0;
  l = lista_de(po, &s, 0);
  return po->malo ? NULL : l;
}
static void suelta(Pool *po) { int k; for (k = 0; k < po->n_lw; k++) lwc_string_unref(po->lw[k]); po->n_lw = 0; }
int sel_casa(Nodo *n, const char *sel, Nodo *ambito) {
  Pool po;
  SLista *l = compila(sel, &po);
  int r;
  if (!l) { suelta(&po); return -1; }
  r = sl_casa(n, l, ambito);
  suelta(&po);
  return r;
}
int sel_busca(Nodo *raiz, const char *sel, Nodo **res, int max) {
  Pool po;
  SLista *l = compila(sel, &po);
  Nodo *x, *amb;
  int k = 0;
  if (!l) { suelta(&po); return -1; }
  /* :scope en querySelector: el propio raiz; si raiz es el documento, nada */
  amb = NI(raiz)->tipo == DOM_DOCUMENT_NODE ? NULL : raiz;
  /* (un caso frecuente, «#id» solo: no hace falta mirar más que hasta el primero) */
  for (x = (Nodo *)NI(raiz)->hijo; x; x = sig_pre(x, raiz))
    if (es_elem(x) && sl_casa(x, l, amb)) {
      if (k < max) res[k] = x;
      k++;
      if (k >= max) break;
    }
  suelta(&po);
  return k < max ? k : max;
}
