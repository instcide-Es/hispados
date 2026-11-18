/* dom.c - [JS] el Navegador de Hispa-486: lo que JavaScript hace con el árbol
 * del documento (el de html.c): crear, mover y quitar nodos, atributos,
 * innerHTML (se lee con html.c), los selectores de CSS (querySelector,
 * matches, closest) y sacar el HTML o el texto de un trozo. Lo usa script.c.
 * Escrito desde la descripción pública del DOM y de los selectores de CSS.
 * docs/486/NAVEGADOR.md, «JavaScript».
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dom.h"

Pagina *dom_pag;
static int palabra(const char *t, const char *p, int ci);
int dom_sucio, dom_sucio_ctl;

/* =============================================================================
 *  Nodos
 * ============================================================================= */
const char *dom_nombre(const Nodo *n) { return n->texto ? "#text" : etq_nom[n->etq]; }
int dom_etq(const char *nombre) {
  char b[16];
  int k;
  for (k = 0; nombre[k] && k < 15; k++) b[k] = (char)min_c(nombre[k]);
  if (nombre[k]) return E_OTRO;
  b[k] = 0;
  for (k = 1; k < E_N; k++) if (!strcmp(etq_nom[k], b)) return k;
  return E_OTRO;
}
static Nodo *nodo(void) {
  Nodo *n = ar_pide(&dom_pag->doc.ar, sizeof *n);
  if (n) memset(n, 0, sizeof *n);
  return n;
}
Nodo *dom_crea(const char *nombre) {
  Nodo *n = nodo();
  if (n) n->etq = (u8)dom_etq(nombre);
  return n;
}
Nodo *dom_crea_texto(const char *t, int len) {
  Nodo *n = nodo();
  if (!n) return NULL;
  n->texto = 1;
  n->t = ar_texto(&dom_pag->doc.ar, t, len);
  n->n = strlen(n->t);
  return n;
}
Nodo *dom_ant(const Nodo *n) {
  Nodo *h;
  if (!n->padre || n->padre->hijo == n) return NULL;
  for (h = n->padre->hijo; h && h->sig != n; h = h->sig) ;
  return h;
}
void dom_quita(Nodo *n) {
  Nodo *p = n->padre, *a;
  if (!p) return;
  a = dom_ant(n);
  if (a) a->sig = n->sig; else p->hijo = n->sig;
  if (p->ult == n) p->ult = a;
  n->padre = n->sig = NULL;
  dom_sucio = dom_sucio_ctl = 1;
}
int dom_dentro(const Nodo *a, const Nodo *n) {
  for (; n; n = n->padre) if (n == a) return 1;
  return 0;
}
void dom_inserta(Nodo *padre, Nodo *n, Nodo *antes) {
  if (dom_dentro(n, padre)) return;              /* (no se puede meter un nodo dentro de sí) */
  dom_quita(n);
  if (antes && antes->padre != padre) antes = NULL;
  n->padre = padre;
  if (!antes) {
    if (padre->ult) padre->ult->sig = n; else padre->hijo = n;
    padre->ult = n;
  } else {
    Nodo *a = dom_ant(antes);
    n->sig = antes;
    if (a) a->sig = n; else padre->hijo = n;
  }
  dom_sucio = dom_sucio_ctl = 1;
}

/* ---- atributos (en la arena del documento: lo viejo se queda hasta que se va la página) ---- */
void dom_pon_atr(Nodo *n, const char *nom, const char *val) {
  Arena *a = &dom_pag->doc.ar;
  int k;
  char **na;
  if (n->texto) return;
  dom_sucio = 1;
  for (k = 0; k < n->n_atr; k++)
    if (!strcmp(n->atr[2 * k], nom)) {
      if (strcmp(n->atr[2 * k + 1], val)) n->atr[2 * k + 1] = ar_texto(a, val, -1);
      goto hecho;
    }
  na = ar_pide(a, (n->n_atr + 1) * 2 * sizeof(char *));
  if (!na) return;
  if (n->n_atr) memcpy(na, n->atr, n->n_atr * 2 * sizeof(char *));
  na[2 * n->n_atr] = ar_texto(a, nom, -1);
  na[2 * n->n_atr + 1] = ar_texto(a, val, -1);
  n->atr = na;
  n->n_atr++;
hecho:
  /* un menú lateral plegable (nav, aside) que JavaScript abre con una clase
   * («open», «show»: el cajón de ClicGest): se despliega */
  if (!strcmp(nom, "class") && (n->etq == E_NAV || n->etq == E_ASIDE)) n->abierto = palabra(val, "open", 0) || palabra(val, "show", 0);
}
void dom_quita_atr(Nodo *n, const char *nom) {
  int k;
  for (k = 0; k < n->n_atr; k++)
    if (!strcmp(n->atr[2 * k], nom)) {
      memmove(n->atr + 2 * k, n->atr + 2 * k + 2, (n->n_atr - k - 1) * 2 * sizeof(char *));
      n->n_atr--;
      dom_sucio = 1;
      return;
    }
}
void dom_pon_texto(Nodo *n, const char *t, int len) {
  if (!n->texto) return;
  n->t = ar_texto(&dom_pag->doc.ar, t, len);
  n->n = strlen(n->t);
  dom_sucio = 1;
}
void dom_pon_contenido(Nodo *n, const char *t, int len) {
  Nodo *x;
  if (n->texto) { dom_pon_texto(n, t, len); return; }
  while (n->hijo) dom_quita(n->hijo);
  if (len && (x = dom_crea_texto(t, len))) dom_inserta(n, x, NULL);
  dom_sucio = dom_sucio_ctl = 1;
}

/* ---- innerHTML: se lee el trozo con html.c en un documento aparte y sus
 * nodos pasan a este (con su memoria: los trozos de su arena se cuelgan de
 * la de este documento) ---- */
void dom_html(Nodo *n, const u8 *utf8, u32 len) {
  Doc t;
  Nodo *h, *cont;
  Trozo **q;
  while (n->hijo) dom_quita(n->hijo);
  dom_sucio = dom_sucio_ctl = 1;
  if (!len) return;
  doc_lee(&t, utf8, len, 0);
  if (!t.raiz) return;
  cont = t.body ? t.body : t.raiz;
  /* (un trozo con <html>: lo de dentro de su body; si no, lo de la raíz) */
  if (cont == t.raiz && t.raiz->hijo && t.raiz->hijo->etq == E_HTML && !t.raiz->hijo->sig) cont = t.raiz->hijo;
  while ((h = cont->hijo)) {
    cont->hijo = h->sig;
    h->padre = h->sig = NULL;
    dom_inserta(n, h, NULL);
  }
  for (q = &dom_pag->doc.ar.t; *q; q = &(*q)->sig) ;
  *q = t.ar.t;
}

/* =============================================================================
 *  Sacar el HTML y el texto (en UTF-8)
 * ============================================================================= */
typedef struct { char *b; u32 n, cap; } Buf;
static void b_pon(Buf *b, const char *t, u32 n) {
  if (b->n + n + 1 > b->cap) {
    u32 c = (b->cap ? b->cap * 2 : 256) + n;
    char *q = realloc(b->b, c);
    if (!q) return;
    b->b = q; b->cap = c;
  }
  memcpy(b->b + b->n, t, n);
  b->n += n;
  b->b[b->n] = 0;
}
static void b_1252(Buf *b, const char *t, u32 n, int escapa) {
  char u[256 * 3 + 8];
  u32 k = 0;
  while (k < n) {
    u32 m = 0, j = 0;
    char tmp[256];
    while (k < n && m < 250) {
      char c = t[k++];
      if (escapa && (c == '&' || c == '<' || c == '>' || (escapa == 2 && c == '"'))) {
        const char *e = c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : "&quot;";
        if (m) { j = a_utf8(tmp, m, u); b_pon(b, u, j); m = 0; }
        b_pon(b, e, strlen(e));
        continue;
      }
      tmp[m++] = c;
    }
    if (m) { j = a_utf8(tmp, m, u); b_pon(b, u, j); }
  }
}
static void serializa(Buf *b, const Nodo *n) {
  const Nodo *h;
  int k;
  if (n->texto) {
    if (n->padre && (n->padre->etq == E_SCRIPT)) b_pon(b, n->t, n->n);    /* (el código, tal cual) */
    else b_1252(b, n->t, n->n, n->padre && n->padre->etq == E_STYLE ? 0 : 1);
    return;
  }
  b_pon(b, "<", 1);
  b_pon(b, n->etq == E_OTRO ? "span" : etq_nom[n->etq], strlen(n->etq == E_OTRO ? "span" : etq_nom[n->etq]));
  for (k = 0; k < n->n_atr; k++) {
    b_pon(b, " ", 1);
    b_1252(b, n->atr[2 * k], strlen(n->atr[2 * k]), 0);
    b_pon(b, "=\"", 2);
    b_1252(b, n->atr[2 * k + 1], strlen(n->atr[2 * k + 1]), 2);
    b_pon(b, "\"", 1);
  }
  b_pon(b, ">", 1);
  if (etq_band[n->etq] & EF_VACIO) return;
  for (h = n->hijo; h; h = h->sig) serializa(b, h);
  b_pon(b, "</", 2);
  b_pon(b, n->etq == E_OTRO ? "span" : etq_nom[n->etq], strlen(n->etq == E_OTRO ? "span" : etq_nom[n->etq]));
  b_pon(b, ">", 1);
}
char *dom_serializa(const Nodo *n, int fuera, u32 *len) {
  Buf b = { 0, 0, 0 };
  const Nodo *h;
  b_pon(&b, "", 0);
  if (fuera) serializa(&b, n);
  else for (h = n->hijo; h; h = h->sig) serializa(&b, h);
  *len = b.n;
  return b.b;
}
static void contenido(Buf *b, const Nodo *n) {
  const Nodo *h;
  if (n->texto) {
    if (n->padre && n->padre->etq == E_SCRIPT) b_pon(b, n->t, n->n);
    else b_1252(b, n->t, n->n, 0);
    return;
  }
  for (h = n->hijo; h; h = h->sig) contenido(b, h);
}
char *dom_contenido(const Nodo *n, u32 *len) {
  Buf b = { 0, 0, 0 };
  b_pon(&b, "", 0);
  contenido(&b, n);
  *len = b.n;
  return b.b;
}

/* =============================================================================
 *  Los selectores de CSS. Se entienden: etiqueta, *, #id, .clase,
 *  [atr], [atr=v], ~= |= ^= $= *= (y la «i» del final), los combinadores
 *  « », >, + y ~, las listas con comas y las seudoclases :not() :is()
 *  :where() :has() :first-child :last-child :only-child :nth-child()
 *  :nth-last-child() :first-of-type :last-of-type :checked :disabled
 *  :enabled :empty :root :scope (y :hover, :focus, :visited... que no casan).
 * ============================================================================= */
typedef struct SLista SLista;
typedef struct {
  u8 que;                /* 'i' id, 'c' clase, 'a' atributo, 'p' seudoclase */
  u8 op;                 /* atributo: 0 existe, '=', '~', '|', '^', '$', '*' ; seudo: su código */
  u8 ci;                 /* sin mirar mayúsculas */
  char *n, *v;
  SLista *sub;
  int a, b;              /* :nth-child(an+b) */
} Cond;
#define CONDS 10
typedef struct { int etq; int nc; u8 comb; Cond c[CONDS]; } Comp;
typedef struct { int n; Comp *k; } Compleja;
struct SLista { int n; Compleja *c; };
typedef struct { char *p, *fin; int malo; } Pool;
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
static int es_nom(int c) { return (c | 32) >= 'a' && (c | 32) <= 'z' || (c >= '0' && c <= '9') || c == '-' || c == '_' || (u8)c >= 0x80 || c == '\\'; }
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
  if (!_strnicmp(t, "odd", 3)) { *a = 2; *b = 1; return; }
  if (!_strnicmp(t, "even", 4)) { *a = 2; *b = 0; return; }
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
  k->etq = -1;
  if (*t == '*') { t++; algo = 1; }
  else if (es_nom(*t)) {
    char *n = nombre_de(po, &t);
    if (!n) return 0;
    k->etq = dom_etq(n);
    if (k->etq == E_OTRO) k->etq = -2;           /* (una etiqueta que no se conoce: no casa nada) */
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
    } else if (*t == '[') {
      t = blancos(t + 1);
      c->que = 'a';
      if (!(c->n = nombre_de(po, &t))) return 0;
      { char *q; for (q = c->n; *q; q++) *q = (char)min_c(*q); }
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
      for (j = 0; j < sizeof ps / sizeof ps[0]; j++) if (!_stricmp(ps[j].n, n)) { c->op = ps[j].c; break; }
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

static Nodo *raiz_doc(void) { return dom_pag ? dom_pag->doc.raiz : NULL; }
static int es_elem(const Nodo *n) { return n && !n->texto && n != raiz_doc(); }
static Nodo *padre_elem(const Nodo *n) { Nodo *p = n->padre; return es_elem(p) ? p : NULL; }
static int sl_casa(Nodo *n, SLista *l, Nodo *ambito);
/* el siguiente en orden del documento, sin salir de raiz (NULL al acabar) */
static Nodo *sig_pre(Nodo *x, Nodo *raiz) {
  if (!x->texto && x->hijo) return x->hijo;
  while (x != raiz && !x->sig) x = x->padre;
  return x == raiz ? NULL : x->sig;
}
static int igual_ci(const char *a, const char *b, int ci) { return ci ? !_stricmp(a, b) : !strcmp(a, b); }
static int empieza_ci(const char *a, const char *b, int ci) { u32 l = strlen(b); return ci ? !_strnicmp(a, b, l) : !strncmp(a, b, l); }
static int palabra(const char *t, const char *p, int ci) {
  u32 l = strlen(p);
  if (!l) return 0;
  while (*t) {
    while (*t == ' ' || *t == '\t' || *t == '\n') t++;
    if ((ci ? !_strnicmp(t, p, l) : !strncmp(t, p, l)) && (t[l] == 0 || t[l] == ' ' || t[l] == '\t' || t[l] == '\n')) return 1;
    while (*t && *t != ' ' && *t != '\t' && *t != '\n') t++;
  }
  return 0;
}
static int marcado(const Nodo *n) {
  if (n->ctl && dom_pag && n->ctl <= dom_pag->n_ctl) {
    Ctl *c = &dom_pag->ctl[n->ctl - 1];
    if (c->nodo == n && (c->tipo == C_CASILLA || c->tipo == C_RADIO)) return c->marcado;
  }
  if (n->etq == E_OPTION) {
    Nodo *s;
    for (s = n->padre; s && s->etq != E_SELECT; s = s->padre) ;
    if (s && s->ctl && dom_pag && s->ctl <= dom_pag->n_ctl) {
      Ctl *c = &dom_pag->ctl[s->ctl - 1];
      if (c->nodo == s && c->n_opc && c->elegida < c->n_opc) return c->opc[c->elegida] == n;
    }
    return atr(n, "selected") != NULL;
  }
  return n->etq == E_INPUT && atr(n, "checked");
}
static int posicion(Nodo *n, int desde_fin, int del_tipo) {   /* 1, 2, 3... entre sus hermanos */
  Nodo *h;
  int k = 0, aqui = 0;
  if (!n->padre) return 1;
  for (h = n->padre->hijo; h; h = h->sig) {
    if (h->texto || (del_tipo && h->etq != n->etq)) continue;
    k++;
    if (h == n) aqui = k;
  }
  return desde_fin ? k - aqui + 1 : aqui;
}
static int nth_vale(int pos, int a, int b) {
  if (!a) return pos == b;
  return (pos - b) / a >= 0 && (pos - b) % a == 0;
}
static int casa_cond(Nodo *n, Cond *c, Nodo *ambito) {
  const char *v;
  Nodo *h;
  switch (c->que) {
  case 'i': return (v = atr(n, "id")) && !strcmp(v, c->n);
  case 'c': return tiene_clase(n, c->n);
  case 'a':
    if (!(v = atr(n, c->n))) return 0;
    switch (c->op) {
    case 0: return 1;
    case '=': return igual_ci(v, c->v, c->ci);
    case '~': return palabra(v, c->v, c->ci);
    case '|': { u32 l = strlen(c->v); return igual_ci(v, c->v, c->ci) || (empieza_ci(v, c->v, c->ci) && v[l] == '-'); }
    case '^': return c->v[0] && empieza_ci(v, c->v, c->ci);
    case '$': { u32 l = strlen(v), m = strlen(c->v); return m && l >= m && igual_ci(v + l - m, c->v, c->ci); }
    case '*': {
      u32 l = strlen(c->v);
      if (!l) return 0;
      for (; *v; v++) if (c->ci ? !_strnicmp(v, c->v, l) : !strncmp(v, c->v, l)) return 1;
      return 0;
    }
    }
    return 0;
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
      int des = atr(n, "disabled") != NULL, es = n->etq == E_INPUT || n->etq == E_BUTTON || n->etq == E_SELECT
                || n->etq == E_TEXTAREA || n->etq == E_OPTION || n->etq == E_FIELDSET;
      if (!es) return 0;
      return c->op == P_DISABLED ? des : !des;
    }
    case P_EMPTY: for (h = n->hijo; h; h = h->sig) if (!h->texto || h->n) return 0; return 1;
    case P_ROOT: return n->padre == raiz_doc();
    case P_SCOPE: return ambito ? n == ambito : n->padre == raiz_doc();
    case P_NOT: return !sl_casa(n, c->sub, ambito);
    case P_IS: return sl_casa(n, c->sub, ambito);
    case P_HAS: {
      /* algún descendiente (o hermano, con + y ~) que case, con n de ámbito */
      Nodo *x = n->hijo;
      int k;
      for (k = 0; k < c->sub->n; k++) {
        Compleja *cx = &c->sub->c[k];
        if (cx->k[0].comb == '+' || cx->k[0].comb == '~') {
          for (x = n->sig; x; x = x->sig) if (es_elem(x) && sl_casa(x, c->sub, n)) return 1;
          return 0;
        }
      }
      for (x = n->hijo ? n->hijo : NULL; x; x = sig_pre(x, n)) if (es_elem(x) && sl_casa(x, c->sub, n)) return 1;
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
  if (k->etq == -2 || (k->etq >= 0 && n->etq != k->etq)) return 0;
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
    case ' ': return ambito ? dom_dentro(ambito, n->padre) : 1;
    case '+': return ambito && dom_ant(n) && dom_ant(n) == ambito;
    case '~': for (p = dom_ant(n); p; p = dom_ant(p)) if (p == ambito) return 1; return 0;
    }
    return 1;
  }
  switch (x->k[i].comb) {
  case '>': return (p = padre_elem(n)) && casa_desde(p, x, i - 1, ambito);
  case ' ':
    for (p = padre_elem(n); p; p = padre_elem(p)) if (casa_desde(p, x, i - 1, ambito)) return 1;
    return 0;
  case '+':
    for (p = dom_ant(n); p && p->texto; p = dom_ant(p)) ;
    return p && casa_desde(p, x, i - 1, ambito);
  case '~':
    for (p = dom_ant(n); p; p = dom_ant(p)) if (!p->texto && casa_desde(p, x, i - 1, ambito)) return 1;
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
  po->p = pool_mem; po->fin = pool_mem + sizeof pool_mem; po->malo = 0;
  {
    SLista *l = lista_de(po, &s, 0);
    return po->malo ? NULL : l;
  }
}
int dom_casa(Nodo *n, const char *sel, Nodo *ambito) {
  Pool po;
  SLista *l = compila(sel, &po);
  if (!l) return -1;
  return sl_casa(n, l, ambito);
}
int dom_busca(Nodo *raiz, const char *sel, Nodo **res, int max) {
  Pool po;
  SLista *l = compila(sel, &po);
  Nodo *x;
  int k = 0;
  if (!l) return -1;
  /* :scope en querySelector: el propio raiz; si raiz es el documento, nada */
  for (x = raiz->hijo; x; x = sig_pre(x, raiz))
    if (!x->texto && sl_casa(x, l, raiz == raiz_doc() ? NULL : raiz)) {
      if (k < max) res[k] = x;
      k++;
      if (max == 1) return 1;
    }
  return k < max ? k : max;
}
Nodo *dom_por_id(Nodo *raiz, const char *id) {
  Nodo *x;
  const char *v;
  for (x = raiz->hijo; x; x = sig_pre(x, raiz)) if (!x->texto && (v = atr(x, "id")) && !strcmp(v, id)) return x;
  return NULL;
}
int dom_oculto(const Nodo *n) {
  for (; n; n = n->padre) {
    if (n->texto) continue;
    if (n->oculto || (etq_band[n->etq] & EF_FUERA)) return 1;
    if (atr(n, "hidden")) return 1;
  }
  return 0;
}
void dom_prepara(Doc *d) {
  Nodo *r = d->raiz, *h, *html = NULL, *body, *head = NULL;
  if (!r) return;
  for (h = r->hijo; h; h = h->sig) if (!h->texto && h->etq == E_HTML) html = h;
  if (html && d->body != r) {
    /* html.c no hace <head>: lo de antes del <body> (title, meta, link...) va dentro de uno */
    for (h = html->hijo; h; h = h->sig) if (!h->texto && h->etq == E_HEAD) return;
    if (!(head = dom_crea("head"))) return;
    for (h = html->hijo; h && h != d->body; ) {
      Nodo *s = h->sig;
      if (!dom_dentro(h, d->body)) { dom_quita(h); dom_inserta(head, h, NULL); }
      h = s;
    }
    dom_inserta(html, head, html->hijo);
    return;
  }
  /* sin <html> o sin <body>: se ponen, con lo que hubiera dentro */
  if (!html) {
    html = dom_crea("html");
    body = dom_crea("body");
    if (!html || !body) return;
    while ((h = r->hijo)) { dom_quita(h); dom_inserta(body, h, NULL); }
    dom_inserta(html, body, NULL);
    if ((head = dom_crea("head"))) dom_inserta(html, head, body);
    dom_inserta(r, html, NULL);
  } else {
    body = dom_crea("body");
    if (!body) return;
    for (h = html->hijo; h; ) {
      Nodo *s = h->sig;
      if (h->texto || h->etq != E_HEAD) { dom_quita(h); dom_inserta(body, h, NULL); }
      h = s;
    }
    dom_inserta(html, body, NULL);
    if ((head = dom_crea("head"))) dom_inserta(html, head, body);
  }
  d->body = body;
}
