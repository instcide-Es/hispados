/* html.c - el Navegador de Hispa-486: del HTML al árbol del documento.
 * Lector tolerante (etiquetas sin cerrar, mal anidadas, atributos sin
 * comillas), entidades, y UTF-8 a la página 1252 (la de los programas
 * Hispa32; Hispa Ventanas la pasa luego a su letra). Escrito desde la
 * descripción pública de HTML; no sigue el algoritmo de la norma al pie de la
 * letra: hace lo que hace falta para leer páginas corrientes.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "nav.h"

/* ---- memoria a trozos: todo lo de un documento se suelta de una vez ---- */
void *ar_pide(Arena *a, u32 n) {
  Trozo *t = a->t;
  void *p;
  n = (n + 3) & ~3u;
  if (!t || t->usado + n > t->tam) {
    u32 tam = n + sizeof(Trozo) > 32752 ? n + sizeof(Trozo) : 32752;
    t = malloc(tam);
    if (!t) return NULL;
    t->sig = a->t; t->usado = sizeof(Trozo); t->tam = tam;
    a->t = t;
  }
  p = (char *)t + t->usado;
  t->usado += n;
  return p;
}
char *ar_texto(Arena *a, const char *t, int n) {
  char *p;
  if (n < 0) n = strlen(t);
  p = ar_pide(a, n + 1);
  if (!p) return "";
  memcpy(p, t, n);
  p[n] = 0;
  return p;
}
void ar_suelta(Arena *a) {
  while (a->t) { Trozo *s = a->t->sig; free(a->t); a->t = s; }
}
int min_c(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* ---- letras: Unicode <-> página 1252 ---- */
static const u16 alto1252[32] = {
  0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
  0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178
};
/* una letra de Unicode a la 1252; 0 = no se pone nada; lo que no hay, lo más
 * parecido o «?» */
static int uni_1252(u32 c) {
  int k;
  if (c < 0x80 || (c >= 0xA0 && c <= 0xFF)) return (int)c;
  for (k = 0; k < 32; k++) if (alto1252[k] == c) return 0x80 + k;
  if (c == 0x200B || c == 0x200C || c == 0x200D || c == 0xFEFF || c == 0xFE0F || c == 0x00AD) return 0;
  if (c >= 0x2000 && c <= 0x200A) return ' ';
  if (c == 0x2010 || c == 0x2011 || c == 0x2012 || c == 0x2212 || c == 0x2015) return '-';
  if (c == 0x2192 || c == 0x25B6 || c == 0x25B8 || c == 0x203A) return '>';
  if (c == 0x2190 || c == 0x25C0) return '<';
  if (c == 0x2191) return '^';
  if (c == 0x2193 || c == 0x25BE || c == 0x25BC) return 'v';
  if (c == 0x2713 || c == 0x2714 || c == 0x2705) return 'v';
  if (c == 0x2717 || c == 0x2718 || c == 0x274C || c == 0x2715) return 'x';
  if (c == 0x2032) return '\'';
  if (c == 0x2033) return '"';
  if (c == 0x25CF || c == 0x25CB || c == 0x25A0 || c == 0x25AA || c == 0x2023 || c == 0x2043) return 0xB7;
  if (c == 0x2605 || c == 0x2606) return '*';
  if (c == 0x2264) return '<';
  if (c == 0x2265) return '>';
  if (c == 0x2260) return '#';
  if (c == 0x0131) return 'i';
  if (c == 0x2116) return 'N';
  if (c >= 0x1F000 || (c >= 0x2600 && c <= 0x27BF) || (c >= 0x2B00 && c <= 0x2BFF) || (c >= 0x2300 && c <= 0x23FF))
    return 0;                                /* dibujos (emoji): fuera */
  if (c >= 0x0300 && c <= 0x036F) return 0;  /* acentos sueltos */
  return '?';
}
static u32 lee_utf8(const u8 *o, u32 n, u32 *usa) {
  u32 c = o[0];
  int mas = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0, k;
  *usa = 1;
  if (c < 0x80) return c;
  if (c < 0xC0 || (u32)mas >= n) return '?';
  c &= 0x3F >> mas;
  for (k = 1; k <= mas; k++) {
    if ((o[k] & 0xC0) != 0x80) return '?';
    c = c << 6 | (o[k] & 0x3F);
  }
  *usa = mas + 1;
  return c;
}
int utf8_a_1252(const u8 *o, u32 n, char *d) {
  u32 k = 0, usa;
  char *p = d;
  while (k < n) {
    int c = uni_1252(lee_utf8(o + k, n - k, &usa));
    if (c) *p++ = (char)c;
    k += usa;
  }
  return p - d;
}
int a_utf8(const char *o, int n, char *d) {
  char *p = d;
  int k;
  for (k = 0; k < n; k++) {
    u32 c = (u8)o[k];
    if (c >= 0x80 && c < 0xA0 && alto1252[c - 0x80]) c = alto1252[c - 0x80];
    if (c < 0x80) *p++ = (char)c;
    else if (c < 0x800) { *p++ = (char)(0xC0 | c >> 6); *p++ = (char)(0x80 | (c & 0x3F)); }
    else { *p++ = (char)(0xE0 | c >> 12); *p++ = (char)(0x80 | (c >> 6 & 0x3F)); *p++ = (char)(0x80 | (c & 0x3F)); }
  }
  return p - d;
}

/* ---- entidades ---- */
static const struct { const char *n; u16 c; } ents[] = {
  { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' }, { "nbsp", 0xA0 },
  { "copy", 0xA9 }, { "reg", 0xAE }, { "laquo", 0xAB }, { "raquo", 0xBB }, { "middot", 0xB7 },
  { "hellip", 0x2026 }, { "mdash", 0x2014 }, { "ndash", 0x2013 }, { "euro", 0x20AC }, { "iexcl", 0xA1 },
  { "iquest", 0xBF }, { "ordf", 0xAA }, { "ordm", 0xBA }, { "deg", 0xB0 }, { "plusmn", 0xB1 },
  { "times", 0xD7 }, { "divide", 0xF7 }, { "ntilde", 0xF1 }, { "Ntilde", 0xD1 }, { "ccedil", 0xE7 },
  { "Ccedil", 0xC7 }, { "szlig", 0xDF }, { "bull", 0x2022 }, { "trade", 0x2122 }, { "lsquo", 0x2018 },
  { "rsquo", 0x2019 }, { "ldquo", 0x201C }, { "rdquo", 0x201D }, { "sbquo", 0x201A }, { "bdquo", 0x201E },
  { "larr", 0x2190 }, { "rarr", 0x2192 }, { "uarr", 0x2191 }, { "darr", 0x2193 }, { "sect", 0xA7 },
  { "para", 0xB6 }, { "pound", 0xA3 }, { "yen", 0xA5 }, { "cent", 0xA2 }, { "frac12", 0xBD },
  { "frac14", 0xBC }, { "frac34", 0xBE }, { "sup2", 0xB2 }, { "sup3", 0xB3 }, { "micro", 0xB5 },
  { "shy", 0xAD }, { "ensp", ' ' }, { "emsp", ' ' }, { "thinsp", ' ' }, { "zwnj", 0x200C }, { "zwj", 0x200D },
  { "lsaquo", 0x2039 }, { "rsaquo", 0x203A }, { "check", 0x2713 }, { "cross", 0x2717 }, { "minus", 0x2212 },
  { "prime", 0x2032 }, { "le", 0x2264 }, { "ge", 0x2265 }, { "ne", 0x2260 }, { "aring", 0xE5 }, { "Aring", 0xC5 },
  { "oslash", 0xF8 }, { "Oslash", 0xD8 }, { "aelig", 0xE6 }, { "AElig", 0xC6 }
};
/* «&...;» en o (n letras, o[0] = '&') -> la letra de Unicode y lo que ocupa;
 * 0 si no es una entidad */
static u32 entidad(const u8 *o, u32 n, u32 *usa) {
  u32 k = 1, c = 0, j;
  if (n < 3) return 0;
  if (o[1] == '#') {
    k = 2;
    if (k < n && (o[k] == 'x' || o[k] == 'X')) {
      for (k++; k < n && k < 10; k++) {
        int h = o[k];
        if (h >= '0' && h <= '9') c = c * 16 + h - '0';
        else if ((h | 32) >= 'a' && (h | 32) <= 'f') c = c * 16 + (h | 32) - 'a' + 10;
        else break;
      }
    } else for (; k < n && k < 10 && o[k] >= '0' && o[k] <= '9'; k++) c = c * 10 + o[k] - '0';
    if (k <= 2 || !c) return 0;
    if (k < n && o[k] == ';') k++;
    *usa = k;
    if (c >= 0x80 && c < 0xA0 && alto1252[c - 0x80]) c = alto1252[c - 0x80];  /* (&#150; y parecidas) */
    return c;
  }
  for (k = 1; k < n && k < 10 && ((o[k] | 32) >= 'a' && (o[k] | 32) <= 'z' || (o[k] >= '0' && o[k] <= '9')); k++) ;
  if (k < 3 || k >= n || o[k] != ';') return 0;
  for (j = 0; j < sizeof ents / sizeof ents[0]; j++)
    if (strlen(ents[j].n) == k - 1 && !memcmp(ents[j].n, o + 1, k - 1)) { *usa = k + 1; return ents[j].c; }
  /* letra con acento: aacute, Egrave, icirc, ouml, atilde */
  {
    static const char *ac[] = { "grave", "acute", "circ", "tilde", "uml" };
    static const char bases[] = "AEIOUaeiou";
    static const u8 cod[10] = { 0xC0, 0xC8, 0xCC, 0xD2, 0xD9, 0xE0, 0xE8, 0xEC, 0xF2, 0xF9 };
    const char *b = strchr(bases, o[1]);
    if (b && o[1])
      for (j = 0; j < 5; j++)
        if (strlen(ac[j]) == k - 2 && !memcmp(ac[j], o + 2, k - 2)) {
          int e = b - bases;
          if (j == 3 && (e % 5 == 1 || e % 5 == 2 || e % 5 == 4)) return 0;  /* (no hay e, i, u con tilde) */
          *usa = k + 1;
          return cod[e] + (j == 4 && e % 5 == 0 ? 4 : j == 4 ? 3 : j) + (j == 4 && (e % 5 == 3) ? 1 : 0);
        }
    if ((o[1] == 'y' || o[1] == 'Y') && k == 7 && !memcmp(o + 2, "acute", 5)) { *usa = 8; return o[1] == 'y' ? 0xFD : 0xDD; }
  }
  return 0;
}
/* texto de la página (UTF-8 o 1252, con entidades si ent) a la 1252 */
static int decodifica(const u8 *o, u32 n, char *d, int latin1, int ent) {
  u32 k = 0, usa;
  char *p = d;
  while (k < n) {
    u32 c;
    int x;
    if (o[k] == '&' && ent && (c = entidad(o + k, n - k, &usa))) k += usa;
    else if (latin1 || o[k] < 0x80) { *p++ = (char)o[k++]; continue; }
    else { c = lee_utf8(o + k, n - k, &usa); k += usa; }
    x = uni_1252(c);
    if (x) *p++ = (char)x;
  }
  return p - d;
}

/* ---- las etiquetas ---- */
const char *const etq_nom[E_N] = {   /* [JS] (no estática: la usa dom.c) */
  "", "html", "head", "body", "title", "meta", "link", "base", "script", "style", "noscript", "template",
  "div", "p", "h1", "h2", "h3", "h4", "h5", "h6", "ul", "ol", "li", "dl", "dt", "dd", "pre", "blockquote",
  "hr", "br", "table", "caption", "thead", "tbody", "tfoot", "tr", "td", "th", "form", "fieldset", "legend",
  "input", "select", "optgroup", "option", "textarea", "button", "label", "a", "img", "b", "strong", "i", "em",
  "u", "s", "small", "code", "span", "font", "center", "section", "article", "header", "footer", "main", "nav",
  "aside", "details", "summary", "dialog", "svg", "iframe", "address", "figure", "figcaption", "mark", "del",
  "ins", "sub", "sup", "kbd", "var", "samp", "cite", "q", "abbr", "time", "col", "colgroup", "object", "canvas",
  "video", "audio", "datalist", "wbr", "area", "map", "source", "embed", "track", "picture", "big", "tt"
};
#define B EF_BLOQUE
#define V EF_VACIO
#define F EF_FUERA
const u8 etq_band[E_N] = {
  0, B, B | F, B, EF_CRUDO_ENT | F, V | F, V | F, V | F, EF_CRUDO | F, EF_CRUDO | F, 0, F,
  B, B, B, B, B, B, B, B, B, B, B, B, B, B, B, B,
  B | V, V, B, B, B, B, B, B, B, B, B, B, B,
  V, 0, 0, 0, EF_CRUDO_ENT, 0, 0, 0, V, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, B, B, B, B, B, B, B,
  B, B, B, B, F, F, B, B, B, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, V | F, F, F, F,
  F, F, F, V, V | F, F, V | F, V | F, V | F, 0, 0, 0
};
#undef B
#undef V
#undef F
static int etq_de(const u8 *t, u32 n) {
  char b[12];
  u32 k;
  if (n > 10) return E_OTRO;
  for (k = 0; k < n; k++) b[k] = (char)min_c(t[k]);
  b[n] = 0;
  for (k = 1; k < E_N; k++) if (!strcmp(etq_nom[k], b)) return (int)k;
  return E_OTRO;
}
const char *atr(const Nodo *n, const char *nombre) {
  int k;
  for (k = 0; k < n->n_atr; k++) if (!strcmp(n->atr[2 * k], nombre)) return n->atr[2 * k + 1];
  return NULL;
}
int tiene_clase(const Nodo *n, const char *c) {
  const char *t = atr(n, "class");
  u32 l = strlen(c);
  while (t && *t) {
    while (*t == ' ' || *t == '\t' || *t == '\n') t++;
    if (!strncmp(t, c, l) && (t[l] == 0 || t[l] == ' ' || t[l] == '\t' || t[l] == '\n')) return 1;
    while (*t && *t != ' ' && *t != '\t' && *t != '\n') t++;
  }
  return 0;
}
void texto_de(const Nodo *n, char *b, int max) {
  int k = strlen(b);
  const Nodo *h;
  if (n->texto) {
    u32 j;
    for (j = 0; j < n->n && k < max - 1; j++) {
      int c = (u8)n->t[j];
      if (c == '\n' || c == '\r' || c == '\t' || c == 0xA0) c = ' ';
      if (c == ' ' && (k == 0 || b[k - 1] == ' ')) continue;
      b[k++] = (char)c;
    }
    b[k] = 0;
    return;
  }
  if (etq_band[n->etq] & EF_FUERA) return;
  if (n->etq == E_IMG) {
    const char *a = atr(n, "alt");
    if (a) { int l = strlen(a); if (l > max - 1 - k) l = max - 1 - k; memcpy(b + k, a, l); b[k + l] = 0; }
    return;
  }
  for (h = n->hijo; h; h = h->sig) texto_de(h, b, max);
}

/* ---- el árbol ---- */
#define PILA 96
typedef struct {
  Doc *d;
  Nodo *pila[PILA];
  int np, latin1;
} Lector;
static Nodo *nodo_nuevo(Lector *l, int etq) {
  Nodo *n = ar_pide(&l->d->ar, sizeof *n), *p = l->pila[l->np - 1];
  if (!n) return NULL;
  memset(n, 0, sizeof *n);
  n->etq = (u8)etq;
  n->padre = p;
  if (p->ult) p->ult->sig = n; else p->hijo = n;
  p->ult = n;
  return n;
}
static void pon_texto(Lector *l, const u8 *t, u32 n, int ent) {
  Nodo *x;
  char *b;
  if (!n) return;
  x = nodo_nuevo(l, 0);
  b = ar_pide(&l->d->ar, n + 1);
  if (!x || !b) return;
  x->texto = 1;
  x->n = decodifica(t, n, b, l->latin1, ent);
  b[x->n] = 0;
  x->t = b;
}
/* ¿hay abierta una etq? Se busca hasta una de las «paredes» (0 acaba) */
static int abierta(Lector *l, int etq, const u8 *paredes) {
  int k, j;
  for (k = l->np - 1; k > 0; k--) {
    if (l->pila[k]->etq == etq) return k;
    for (j = 0; paredes[j]; j++) if (l->pila[k]->etq == paredes[j]) return 0;
  }
  return 0;
}
static const u8 p_celda[] = { E_TD, E_TH, E_TABLE, E_CAPTION, 0 };
static const u8 p_tabla[] = { E_TABLE, 0 };
static const u8 p_lista[] = { E_UL, E_OL, E_TD, E_TH, E_TABLE, 0 };
static const u8 p_dl[] = { E_DL, E_TD, E_TH, E_TABLE, 0 };
static const u8 p_sel[] = { E_SELECT, E_DATALIST, 0 };
static const u8 p_nada[] = { 0 };
static void cierra_hasta(Lector *l, int k) { if (k > 0) l->np = k; }

static void abre(Lector *l, int etq, char **atrs, int n_atr, int sola) {
  Nodo *n;
  int k;
  /* lo que se cierra solo al llegar otra */
  switch (etq) {
  case E_LI: cierra_hasta(l, abierta(l, E_LI, p_lista)); break;
  case E_DT: case E_DD:
    cierra_hasta(l, abierta(l, E_DT, p_dl));
    cierra_hasta(l, abierta(l, E_DD, p_dl));
    break;
  case E_OPTION: cierra_hasta(l, abierta(l, E_OPTION, p_sel)); break;
  case E_OPTGROUP:
    cierra_hasta(l, abierta(l, E_OPTION, p_sel));
    cierra_hasta(l, abierta(l, E_OPTGROUP, p_sel));
    break;
  case E_TR:
    cierra_hasta(l, abierta(l, E_TR, p_tabla));
    break;
  case E_TD: case E_TH:
    cierra_hasta(l, abierta(l, E_TD, p_tabla));
    cierra_hasta(l, abierta(l, E_TH, p_tabla));
    break;
  case E_THEAD: case E_TBODY: case E_TFOOT:
    cierra_hasta(l, abierta(l, E_THEAD, p_tabla));
    cierra_hasta(l, abierta(l, E_TBODY, p_tabla));
    cierra_hasta(l, abierta(l, E_TFOOT, p_tabla));
    break;
  case E_HTML: case E_HEAD:
    if (l->np > 1) return;                    /* repetidas: nada */
    break;
  case E_BODY:
    if (l->d->body) return;
    l->np = (l->np > 1 && l->pila[1]->etq == E_HTML) ? 2 : 1;   /* (cierra head) */
    break;
  case E_A: cierra_hasta(l, abierta(l, E_A, p_celda)); break;
  case E_BUTTON: cierra_hasta(l, abierta(l, E_BUTTON, p_celda)); break;
  }
  if ((etq_band[etq] & EF_BLOQUE) && etq != E_HTML && etq != E_BODY)
    cierra_hasta(l, abierta(l, E_P, p_celda));
  n = nodo_nuevo(l, etq);
  if (!n) return;
  n->atr = atrs; n->n_atr = (short)n_atr;
  if (etq == E_BODY) l->d->body = n;
  if (etq == E_BASE && !l->d->base && atr(n, "href")) l->d->base = (char *)atr(n, "href");
  if (etq == E_DETAILS && atr(n, "open")) n->abierto = 1;
  if (!sola && !(etq_band[etq] & EF_VACIO) && l->np < PILA) l->pila[l->np++] = n;
}
static void cierra(Lector *l, int etq) {
  int k;
  if (etq == E_OTRO || etq == E_HTML || etq == E_BODY || (etq_band[etq] & EF_VACIO)) {
    if (etq == E_BR) abre(l, E_BR, NULL, 0, 1);  /* «</br>»: un salto */
    return;
  }
  if (etq == E_P) {
    k = abierta(l, E_P, p_celda);
    if (!k) { abre(l, E_P, NULL, 0, 1); return; }
    cierra_hasta(l, k);
    return;
  }
  if (etq == E_TABLE || etq == E_TD || etq == E_TH || etq == E_TR || etq == E_THEAD || etq == E_TBODY || etq == E_TFOOT
      || etq == E_CAPTION)
    k = abierta(l, etq, etq == E_TABLE ? p_nada : p_tabla);
  else k = abierta(l, etq, p_celda);
  cierra_hasta(l, k);
}

void doc_lee(Doc *d, const u8 *f, u32 n, int latin1) {
  static Lector l;
  u32 k = 0, ini = 0;
  memset(&l, 0, sizeof l);
  memset(d, 0, sizeof *d);
  l.d = d; l.latin1 = latin1;
  d->raiz = ar_pide(&d->ar, sizeof(Nodo));
  if (!d->raiz) return;
  memset(d->raiz, 0, sizeof(Nodo));
  d->raiz->etq = E_HTML;
  l.pila[0] = d->raiz; l.np = 1;
  if (n >= 3 && f[0] == 0xEF && f[1] == 0xBB && f[2] == 0xBF) k = ini = 3;
  while (k < n) {
    u32 p, fin_nom;
    int cierre = 0, etq, n_atr = 0, sola = 0;
    char *atrs[64];
    if (f[k] != '<') { k++; continue; }
    if (k + 1 >= n) break;
    if (f[k + 1] == '!' || f[k + 1] == '?') {
      pon_texto(&l, f + ini, k - ini, 1);
      if (k + 3 < n && f[k + 2] == '-' && f[k + 3] == '-') {
        for (p = k + 4; p + 2 < n && !(f[p] == '-' && f[p + 1] == '-' && f[p + 2] == '>'); p++) ;
        k = p + 2 < n ? p + 3 : n;
      } else {
        for (p = k + 2; p < n && f[p] != '>'; p++) ;
        k = p < n ? p + 1 : n;
      }
      ini = k;
      continue;
    }
    p = k + 1;
    if (f[p] == '/') { cierre = 1; p++; }
    if (p >= n || !((f[p] | 32) >= 'a' && (f[p] | 32) <= 'z')) { k++; continue; }   /* un «<» suelto */
    pon_texto(&l, f + ini, k - ini, 1);
    for (fin_nom = p; fin_nom < n && f[fin_nom] != '>' && f[fin_nom] != '/' && f[fin_nom] > ' '; fin_nom++) ;
    etq = etq_de(f + p, fin_nom - p);
    p = fin_nom;
    /* los atributos */
    for (;;) {
      u32 a0, a1, v0 = 0, v1 = 0;
      while (p < n && (f[p] <= ' ' || f[p] == '/')) { if (f[p] == '/' && p + 1 < n && f[p + 1] == '>') sola = 1; p++; }
      if (p >= n || f[p] == '>') break;
      a0 = p;
      while (p < n && f[p] > ' ' && f[p] != '=' && f[p] != '>' && !(f[p] == '/' && p + 1 < n && f[p + 1] == '>')) p++;
      a1 = p;
      while (p < n && f[p] <= ' ') p++;
      if (p < n && f[p] == '=') {
        p++;
        while (p < n && f[p] <= ' ') p++;
        if (p < n && (f[p] == '"' || f[p] == '\'')) {
          u8 q = f[p++];
          v0 = p;
          while (p < n && f[p] != q) p++;
          v1 = p;
          if (p < n) p++;
        } else {
          v0 = p;
          while (p < n && f[p] > ' ' && f[p] != '>') p++;
          v1 = p;
        }
      }
      if (!cierre && n_atr < 32 && a1 > a0) {
        char *nom = ar_pide(&d->ar, a1 - a0 + 1), *val = ar_pide(&d->ar, v1 - v0 + 1);
        u32 j;
        if (!nom || !val) break;
        for (j = 0; j < a1 - a0; j++) nom[j] = (char)min_c(f[a0 + j]);
        nom[j] = 0;
        val[decodifica(f + v0, v1 - v0, val, latin1, 1)] = 0;
        atrs[2 * n_atr] = nom; atrs[2 * n_atr + 1] = val;
        n_atr++;
      }
    }
    k = p < n ? p + 1 : n;
    ini = k;
    if (cierre) { cierra(&l, etq); continue; }
    {
      char **a = NULL;
      if (n_atr) {
        a = ar_pide(&d->ar, 2 * n_atr * sizeof(char *));
        if (a) memcpy(a, atrs, 2 * n_atr * sizeof(char *)); else n_atr = 0;
      }
      abre(&l, etq, a, n_atr, sola);
    }
    if ((etq_band[etq] & (EF_CRUDO | EF_CRUDO_ENT)) && !sola) {
      /* lo de dentro, tal cual hasta su cierre */
      const char *nom = etq_nom[etq];
      u32 ln = strlen(nom), j;
      for (p = k; p < n; p++) {
        if (f[p] != '<' || p + 1 >= n || f[p + 1] != '/' || p + 2 + ln > n) continue;
        for (j = 0; j < ln && min_c(f[p + 2 + j]) == nom[j]; j++) ;
        if (j == ln) break;
      }
      if (etq == E_STYLE) {
        char *c = ar_pide(&d->ar, d->n_css + (p - k) + 2);
        if (c) {
          if (d->n_css) memcpy(c, d->css, d->n_css);
          memcpy(c + d->n_css, f + k, p - k);
          d->n_css += p - k;
          c[d->n_css++] = '\n'; c[d->n_css] = 0;
          d->css = c;
        }
      } else if (etq == E_SCRIPT) {
        /* [JS] el código, tal cual, para script.c (en UTF-8: si la página
         * es de la 1252, se pasa) */
        Nodo *x = p > k ? nodo_nuevo(&l, 0) : NULL;
        char *b = x ? ar_pide(&d->ar, latin1 ? (p - k) * 3 + 1 : p - k + 1) : NULL;
        if (b) {
          x->texto = 1; x->t = b;
          if (latin1) x->n = a_utf8((const char *)f + k, p - k, b); else { memcpy(b, f + k, p - k); x->n = p - k; }
          b[x->n] = 0;
        }
      } else {
        Nodo *e = l.pila[l.np - 1];
        pon_texto(&l, f + k, p - k, 1);
        if (etq == E_TITLE && !d->titulo && e->etq == E_TITLE && e->hijo) {
          char b[200];
          b[0] = 0;
          texto_de(e->hijo, b, sizeof b);
          d->titulo = ar_texto(&d->ar, b, -1);
        }
      }
      if (l.np > 1 && l.pila[l.np - 1]->etq == etq) l.np--;
      while (p < n && f[p] != '>') p++;
      k = p < n ? p + 1 : n;
      ini = k;
    }
  }
  pon_texto(&l, f + ini, n - ini, 1);
  if (!d->body) d->body = d->raiz;
}
void doc_suelta(Doc *d) { ar_suelta(&d->ar); memset(d, 0, sizeof *d); }
