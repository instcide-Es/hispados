/* =============================================================================
 *  cssvar.c - las variables de CSS (custom properties: «--color: #fa0» y
 *  «color: var(--color)») para NetSurf, que no las entiende (libcss 0.9.2).
 *  Las webs de hoy (Bootstrap 5, la de Hispa-DOS) ponen casi todos sus
 *  colores, letras y medidas así: sin esto salen en blanco y negro.
 *
 *  Se hace con el texto de la hoja de estilo antes de que libcss la lea
 *  (NAVEGA2.EXE, terceros/netsurf/netsurf/content/handlers/css/css.c y
 *  select.c, marcado [HISPA]):
 *   - las variables de :root, html, body, :host y * valen para todo (también
 *     para las hojas que vengan después en la misma página y para los
 *     atributos style);
 *   - un var() se cambia por el valor: el de la misma regla, si lo pone; si
 *     no, el de todo; si no, el que trae el var() de reserva;
 *   - si otra regla (.btn-primary) pone la variable que usa una regla
 *     (.btn { background: var(--bs-btn-bg) }), se añade al final de su
 *     bloque una regla para los que llevan las dos (.btn.btn-primary) o
 *     están dentro (.btn-primary .btn) con el valor de esa regla. No es la
 *     cascada de verdad, pero da casi siempre lo mismo.
 *  docs/486/NAVEGADOR2.md, «Las variables de CSS»
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>

#include "cssvar.h"

/* ---- un texto que crece ---- */
typedef struct { char *t; size_t n, cap; int mal; } Txt;
static void tx_pon(Txt *x, const char *s, size_t n) {
  if (x->mal) return;
  if (x->n + n + 1 > x->cap) {
    size_t c = x->cap ? x->cap * 2 : 4096;
    char *q;
    while (c < x->n + n + 1) c *= 2;
    q = realloc(x->t, c);
    if (!q) { x->mal = 1; return; }
    x->t = q; x->cap = c;
  }
  memcpy(x->t + x->n, s, n);
  x->n += n;
  x->t[x->n] = 0;
}
static void tx_cad(Txt *x, const char *s) { tx_pon(x, s, strlen(s)); }

/* ---- las variables de todo (de :root...) ---- */
typedef struct Var { struct Var *sig; char *nombre, *valor; } Var;
static Var *globales;
void cssvar_olvida(void) {
  while (globales) { Var *v = globales; globales = v->sig; free(v->nombre); free(v->valor); free(v); }
}
static int n_globales;
static void var_pon(Var **l, const char *n, size_t ln, const char *v, size_t lv) {
  Var *x;
  /* (las de todo se guardan de una página a otra: las hojas de dentro de la
   * página se leen antes que las de fuera, que traen las variables; si son
   * demasiadas, se empieza de nuevo) */
  if (l == &globales && n_globales > 3000) { cssvar_olvida(); n_globales = 0; }
  for (x = *l; x; x = x->sig)
    if (strlen(x->nombre) == ln && !memcmp(x->nombre, n, ln)) {
      char *q = malloc(lv + 1);
      if (!q) return;
      memcpy(q, v, lv); q[lv] = 0;
      free(x->valor); x->valor = q;
      return;
    }
  x = calloc(1, sizeof *x);
  if (!x) return;
  x->nombre = malloc(ln + 1); x->valor = malloc(lv + 1);
  if (!x->nombre || !x->valor) { free(x->nombre); free(x->valor); free(x); return; }
  memcpy(x->nombre, n, ln); x->nombre[ln] = 0;
  memcpy(x->valor, v, lv); x->valor[lv] = 0;
  x->sig = *l; *l = x;
  if (l == &globales) n_globales++;
}
static const char *var_de(Var *l, const char *n, size_t ln) {
  for (; l; l = l->sig) if (strlen(l->nombre) == ln && !memcmp(l->nombre, n, ln)) return l->valor;
  return NULL;
}

/* ---- la hoja, en árbol: reglas, grupos (@media...) y texto tal cual ---- */
typedef struct Decl { const char *n, *v; size_t ln, lv; } Decl;
enum { I_TAL, I_REGLA, I_GRUPO };
typedef struct Item {
  struct Item *sig;
  int tipo;
  const char *a; size_t la;        /* el selector, el preludio del grupo o el texto */
  Decl *d; int nd;                 /* las declaraciones de la regla */
  Var *loc;                        /* las variables que pone la regla */
  struct Item *hijos;              /* lo del grupo */
} Item;

static const char *salta_com(const char *p, const char *f) {
  for (;;) {
    while (p < f && isspace((unsigned char)*p)) p++;
    if (p + 1 < f && p[0] == '/' && p[1] == '*') {
      const char *e = p + 2;
      while (e + 1 < f && !(e[0] == '*' && e[1] == '/')) e++;
      p = e + 1 < f ? e + 2 : f;
      continue;
    }
    return p;
  }
}
/* hasta uno de «stop» fuera de cadenas, paréntesis y corchetes */
static const char *hasta(const char *p, const char *f, const char *stop) {
  int par = 0;
  while (p < f) {
    char c = *p;
    if (c == '"' || c == '\'') { p++; while (p < f && *p != c) { if (*p == '\\') p++; p++; } p++; continue; }
    if (c == '/' && p + 1 < f && p[1] == '*') { p += 2; while (p + 1 < f && !(p[0] == '*' && p[1] == '/')) p++; p += 2; continue; }
    if (c == '\\') { p += 2; continue; }
    if (c == '(' || c == '[') par++;
    else if ((c == ')' || c == ']') && par) par--;
    else if (!par && strchr(stop, c)) return p;
    p++;
  }
  return f;
}
static const char *recorta(const char *a, size_t *l) {
  while (*l && isspace((unsigned char)*a)) { a++; (*l)--; }
  while (*l && isspace((unsigned char)a[*l - 1])) (*l)--;
  return a;
}
static void suelta(Item *i) {
  while (i) {
    Item *s = i->sig;
    while (i->loc) { Var *v = i->loc; i->loc = v->sig; free(v->nombre); free(v->valor); free(v); }
    free(i->d);
    suelta(i->hijos);
    free(i);
    i = s;
  }
}
static int es_global(const char *s, size_t l) {
  size_t k = 0;
  /* «:root», «html», «:root, [data-bs-theme=light]», «*»... cualquiera de la lista */
  while (k < l) {
    const char *a = s + k;
    size_t m = 0;
    while (k + m < l && s[k + m] != ',') m++;
    k += m + 1;
    a = recorta(a, &m);
    if ((m == 5 && !memcmp(a, ":root", 5)) || (m == 4 && !memcmp(a, "html", 4)) || (m == 4 && !memcmp(a, "body", 4)) ||
        (m == 5 && !memcmp(a, ":host", 5)) || (m == 1 && a[0] == '*') || (m > 5 && !memcmp(a, ":root[", 6)) ||
        (m > 5 && !memcmp(a, ":root:", 6)))
      return 1;
  }
  return 0;
}
/* lee las reglas de [p, f): hasta el «}» de cierre (o el final) */
static Item *lee(const char **pp, const char *f, int prof) {
  Item *prim = NULL, **ult = &prim;
  const char *p = *pp;
  for (;;) {
    Item *it;
    const char *q;
    p = salta_com(p, f);
    if (p >= f) break;
    if (*p == '}') { p++; break; }
    it = calloc(1, sizeof *it);
    if (!it) break;
    *ult = it; ult = &it->sig;
    if (*p == '@') {
      q = hasta(p, f, ";{");
      if (q >= f || *q == ';') { it->tipo = I_TAL; it->a = p; it->la = (q < f ? q + 1 : f) - p; p = q < f ? q + 1 : f; continue; }
      if (prof < 4 && (!strncmp(p, "@media", 6) || !strncmp(p, "@supports", 9) || !strncmp(p, "@layer", 6) ||
                       !strncmp(p, "@container", 10))) {
        it->tipo = I_GRUPO; it->a = p; it->la = q - p;
        p = q + 1;
        it->hijos = lee(&p, f, prof + 1);
        continue;
      }
      /* @font-face, @keyframes, @page...: tal cual, con sus llaves */
      {
        int nivel = 0;
        const char *e = q;
        while (e < f) {
          e = hasta(e, f, "{}");
          if (e >= f) break;
          if (*e == '{') nivel++; else if (--nivel == 0) { e++; break; }
          e++;
        }
        it->tipo = I_TAL; it->a = p; it->la = e - p; p = e;
      }
      continue;
    }
    q = hasta(p, f, "{}");
    if (q >= f || *q == '}') { it->tipo = I_TAL; it->a = p; it->la = q - p; p = q; continue; }
    it->tipo = I_REGLA;
    it->la = q - p;
    it->a = recorta(p, &it->la);
    p = q + 1;
    /* las declaraciones, hasta «}» */
    for (;;) {
      const char *dn, *dv, *e;
      size_t ln, lv;
      p = salta_com(p, f);
      if (p >= f) break;
      if (*p == '}') { p++; break; }
      if (*p == ';') { p++; continue; }
      e = hasta(p, f, ";}");
      dn = p;
      q = hasta(p, e, ":");
      if (q >= e) { p = e; continue; }
      ln = q - dn; dn = recorta(dn, &ln);
      dv = q + 1; lv = e - dv; dv = recorta(dv, &lv);
      p = e;
      if (!ln) continue;
      {
        Decl *nd = realloc(it->d, (it->nd + 1) * sizeof *nd);
        if (!nd) continue;
        it->d = nd;
        it->d[it->nd].n = dn; it->d[it->nd].ln = ln; it->d[it->nd].v = dv; it->d[it->nd].lv = lv;
        it->nd++;
      }
      if (ln > 2 && dn[0] == '-' && dn[1] == '-') {
        var_pon(&it->loc, dn, ln, dv, lv);
        if (es_global(it->a, it->la)) var_pon(&globales, dn, ln, dv, lv);
      }
    }
  }
  *pp = p;
  return prim;
}

/* ---- cambiar los var() de un valor ---- */
static int tiene_var(const char *v, size_t l) {
  size_t k;
  for (k = 0; k + 4 <= l; k++) if (!memcmp(v + k, "var(", 4)) return 1;
  return 0;
}
/* escribe v (l octetos) en x cambiando los var(): primero las de loc, luego
 * las de extra, luego las de todo; devuelve 0 si queda alguno sin valor */
static int expande(Txt *x, const char *v, size_t l, Var *loc, Var *extra, int prof) {
  size_t k = 0, ini = 0;
  int bien = 1;
  while (k + 4 <= l) {
    if (memcmp(v + k, "var(", 4) || (k && (isalnum((unsigned char)v[k - 1]) || v[k - 1] == '-'))) { k++; continue; }
    {
      const char *a = v + k + 4, *f = v + l, *e = hasta(a, f, ")"), *c = hasta(a, e, ",");
      size_t ln = c - a;
      const char *n = recorta(a, &ln), *val;
      tx_pon(x, v + ini, k - ini);
      val = var_de(loc, n, ln);
      if (!val) val = var_de(extra, n, ln);
      if (!val) val = var_de(globales, n, ln);
      if (val && prof < 8) { if (!expande(x, val, strlen(val), loc, extra, prof + 1)) bien = 0; }
      else if (c < e) {                         /* el de reserva: var(--x, 10px) */
        size_t lr = e - c - 1;
        const char *r = recorta(c + 1, &lr);
        if (!expande(x, r, lr, loc, extra, prof + 1)) bien = 0;
      } else bien = 0;
      k = (e < f ? e + 1 : f) - v;
      ini = k;
    }
  }
  tx_pon(x, v + ini, l - ini);
  return bien;
}

/* ---- los degradados: libcss no los entiende y tira la declaración entera
 * (y el fondo oscuro de la página se queda blanco). «background:
 * radial-gradient(...), #0b0a07» -> «background: #0b0a07»; si solo hay
 * degradado, su primer color ---- */
static int es_fondo(const char *n, size_t l) {
  return (l == 10 && !memcmp(n, "background", 10)) || (l == 16 && !memcmp(n, "background-image", 16));
}
static const char *busca_grad(const char *v, size_t l) {
  size_t k;
  for (k = 0; k + 9 <= l; k++) if (!memcmp(v + k, "gradient(", 9)) return v + k;
  return NULL;
}
/* la regla tiene «background-clip: text» (el texto pintado con el degradado:
 * «todo en uno» de clicgest.es): el degradado, al color del texto */
static int fondo_texto;
/* ¿es un color casi transparente? (rgba(..., .15), #rrggbbaa, transparent):
 * los brillos de los degradados; NetSurf pinta sin transparencia y salían
 * manchas de color */
static int casi_transparente(const char *c, size_t l) {
  if (l >= 11 && !strncmp(c, "transparent", 11)) return 1;
  if (l == 9 && c[0] == '#') { int a = (int)strtol(c + 7, NULL, 16); return a < 0x80; }
  if (l >= 4 && !strncmp(c, "rgba", 4)) {
    const char *u = c + l;
    while (u > c && u[-1] != ',' && u[-1] != '/') u--;
    if (u > c) { double a = strtod(u, NULL); if (strchr(u, '%')) a /= 100; return a < 0.5; }
  }
  if (l >= 4 && !strncmp(c, "rgb(", 4) && memchr(c, '/', l)) {
    const char *u = memchr(c, '/', l) + 1;
    double a = strtod(u, NULL);
    if (memchr(u, '%', c + l - u)) a /= 100;
    return a < 0.5;
  }
  return 0;
}
/* escribe en x el valor sin degradados; 0 si no queda nada que valga */
static int sin_degradado(Txt *x, const char *n, size_t ln, const char *v, size_t l) {
  Txt r = { 0 };
  const char *p = v, *f = v + l, *g, *color = NULL;
  size_t lc = 0;
  while ((g = busca_grad(p, f - p))) {
    const char *a = g, *e;
    while (a > p && (isalnum((unsigned char)a[-1]) || a[-1] == '-')) a--;
    tx_pon(&r, p, a - p);
    e = hasta(g + 9, f, ")");
    if (!color) {                               /* su primer color, por si no queda otro */
      const char *c = g + 9;
      while (c < e) {
        if (*c == '#') { const char *q = c + 1; while (q < e && isxdigit((unsigned char)*q)) q++; color = c; lc = q - c; break; }
        if (!strncmp(c, "rgb", 3)) { const char *q = hasta(c, e, ")"); color = c; lc = q - c + 1; break; }
        c++;
      }
    }
    p = e < f ? e + 1 : f;
    while (p < f && (isspace((unsigned char)*p) || *p == ',')) p++;
  }
  tx_pon(&r, p, f - p);
  {
    size_t m = r.n;
    const char *t = r.t ? recorta(r.t, &m) : "";
    while (m && (t[m - 1] == ',' || isspace((unsigned char)t[m - 1]))) m--;
    if (m && !(ln == 16)) { tx_pon(x, n, ln); tx_cad(x, ":"); tx_pon(x, t, m); tx_cad(x, ";"); free(r.t); return 1; }
  }
  free(r.t);
  if (color && fondo_texto) { tx_cad(x, "color:"); tx_pon(x, color, lc); tx_cad(x, ";"); return 1; }
  if (color && !casi_transparente(color, lc)) { tx_cad(x, "background-color:"); tx_pon(x, color, lc); tx_cad(x, ";"); return 1; }
  return 0;
}
static int strstr_n(const char *v, size_t l, const char *b) {
  size_t m = strlen(b), k;
  for (k = 0; k + m <= l; k++) if (!memcmp(v + k, b, m)) return 1;
  return 0;
}
/* ---- clamp(), min() y max() (libcss 0.9.2 no los entiende y tira la
 * declaración: los títulos de hispados.es, «font-size: clamp(44px, 8vw,
 * 92px)», salían con la letra de siempre): si lo de dentro son longitudes
 * que se pueden pasar a puntos (px, rem, em y vw/vh con la medida de la
 * ventana de ahora: cssvar_ancho, cssvar_alto), se cambian por lo que dan ---- */
int cssvar_ancho = 780, cssvar_alto = 493;
static int long_px(const char *a, size_t l, double *px) {
  char b[40], *e;
  double v;
  a = recorta(a, &l);
  if (!l || l >= sizeof b) return 0;
  memcpy(b, a, l); b[l] = 0;
  v = strtod(b, &e);
  if (e == b) return 0;
  if (!*e) { if (v != 0) return 0; *px = 0; return 1; }
  if (!strcmp(e, "px")) *px = v;
  else if (!strcmp(e, "rem") || !strcmp(e, "em")) *px = v * 16;
  else if (!strcmp(e, "vw")) *px = v * cssvar_ancho / 100;
  else if (!strcmp(e, "vh")) *px = v * cssvar_alto / 100;
  else if (!strcmp(e, "pt")) *px = v * 96 / 72;
  else return 0;
  return 1;
}
/* la función (clamp, min o max) que empieza en v[k] (su nombre) y acaba en el
 * «)» de v + *fin: lo que da, en puntos; 0 si no se sabe */
static int una_mates(const char *v, size_t l, size_t k, size_t *fin, double *res) {
  int que = v[k] == 'c' ? 0 : v[k + 1] == 'i' ? 1 : 2, n = 0, prof = 0;
  size_t i = k + (que ? 4 : 6), ini = i;
  double a[3];
  for (; i < l; i++) {
    if (v[i] == '(') { prof++; continue; }
    if (v[i] == ')' && prof) { prof--; continue; }
    if (prof) continue;
    if (v[i] == ',' || v[i] == ')') {
      if (n == 3 || !long_px(v + ini, i - ini, &a[n])) return 0;
      n++;
      ini = i + 1;
      if (v[i] == ')') break;
    }
  }
  if (i >= l || (que == 0 && n != 3) || (que && n < 2)) return 0;
  *fin = i;
  if (que == 0) *res = a[1] < a[0] ? a[0] : a[1] > a[2] ? a[2] : a[1];
  else { int j; *res = a[0]; for (j = 1; j < n; j++) if (que == 1 ? a[j] < *res : a[j] > *res) *res = a[j]; }
  return 1;
}
static int es_mates(const char *v, size_t l, size_t k) {
  if (k && ((v[k - 1] >= 'a' && v[k - 1] <= 'z') || v[k - 1] == '-')) return 0;   /* (minmax(), -webkit-...) */
  return (k + 6 <= l && !memcmp(v + k, "clamp(", 6)) || (k + 4 <= l && (!memcmp(v + k, "min(", 4) || !memcmp(v + k, "max(", 4)));
}
/* env(nombre, reserva): la reserva (sin ella, 0px): aquí no hay muescas ni
 * esquinas redondas en la pantalla («padding: max(20px,
 * env(safe-area-inset-left, 0px))» de hispados.es) */
static size_t sin_env(const char *v, size_t l, char *d, size_t cabe) {
  size_t k = 0, n = 0;
  while (k < l && n + 8 < cabe) {
    if (v[k] == 'e' && k + 4 <= l && !memcmp(v + k, "env(", 4) && (!k || !((v[k - 1] >= 'a' && v[k - 1] <= 'z') || v[k - 1] == '-'))) {
      size_t i = k + 4, coma = 0;
      int prof = 0;
      for (; i < l; i++) {
        if (v[i] == '(') prof++;
        else if (v[i] == ')') { if (!prof) break; prof--; }
        else if (v[i] == ',' && !prof && !coma) coma = i;
      }
      if (i < l) {
        const char *r = coma ? v + coma + 1 : "0px";
        size_t lr = coma ? i - coma - 1 : 3;
        if (n + lr + 1 >= cabe) break;
        memcpy(d + n, r, lr); n += lr;
        k = i + 1;
        continue;
      }
    }
    d[n++] = v[k++];
  }
  if (k < l) return 0;                          /* (no cabe: tal cual) */
  return n;
}
static void pon_valor(Txt *x, const char *v, size_t l) {
  size_t k, desde = 0;
  char b0[512];
  if (l < sizeof b0 && memchr(v, '(', l)) {
    size_t m = 0;
    const char *e = v;
    while (e + 4 <= v + l && memcmp(e, "env(", 4)) e++;
    if (e + 4 <= v + l && (m = sin_env(v, l, b0, sizeof b0))) { v = b0; l = m; }
  }
  for (k = 0; k < l; k++) {
    size_t fin;
    double r;
    if ((v[k] == 'c' || v[k] == 'm') && es_mates(v, l, k) && una_mates(v, l, k, &fin, &r)) {
      char b[32];
      tx_pon(x, v + desde, k - desde);
      snprintf(b, sizeof b, "%dpx", (int)(r + 0.5));
      tx_cad(x, b);
      k = fin;
      desde = fin + 1;
    }
  }
  tx_pon(x, v + desde, l - desde);
}

/* inset (top, right, bottom y left de una vez, como margin), que libcss no
 * conoce: «inset: auto 0 0 0» (la hoja «Más» del modo teléfono de ClicGest,
 * pegada abajo) o «inset: 0» (los fondos oscuros de detrás de una ventana) */
static int pon_inset(Txt *x, const char *v, size_t l) {
  static const char *const lado[4] = { "top", "right", "bottom", "left" };
  const char *pa[4];
  size_t pl[4], k = 0;
  int n = 0, i, prof;
  char imp[16] = "";
  /* (con !important, a los cuatro) */
  {
    const char *e = v + l;
    while (e > v && isspace((unsigned char)e[-1])) e--;
    if (e - v > 10 && !strncasecmp(e - 10, "!important", 10)) {
      strcpy(imp, " !important");
      l = (e - 10) - v;
      while (l && isspace((unsigned char)v[l - 1])) l--;
    }
  }
  while (k < l) {
    while (k < l && isspace((unsigned char)v[k])) k++;
    if (k >= l) break;
    if (n == 4) return 0;                      /* (más de cuatro: no es inset) */
    pa[n] = v + k;
    for (prof = 0; k < l && (prof || !isspace((unsigned char)v[k])); k++) {
      if (v[k] == '(') prof++;
      else if (v[k] == ')' && prof) prof--;
    }
    pl[n] = v + k - pa[n];
    n++;
  }
  if (!n) return 0;
  for (i = 0; i < 4; i++) {
    /* 1: los cuatro; 2: arriba y abajo, los lados; 3: arriba, lados, abajo */
    int j = n == 1 ? 0 : n == 2 ? i % 2 : n == 3 ? (i == 3 ? 1 : i) : i;
    tx_cad(x, lado[i]); tx_cad(x, ":"); pon_valor(x, pa[j], pl[j]); tx_cad(x, imp); tx_cad(x, ";");
  }
  return 1;
}

/* una declaración ya sin var(): a la salida (quitando los degradados) */
static void pon_decl(Txt *x, const char *n, size_t ln, const char *v, size_t lv) {
  v = recorta(v, &lv);
  if (!lv) return;                              /* (una variable vacía: «--bs-btn-font-family: ;») */
  /* con el degradado en el texto, el color transparente no (ver fondo_texto) */
  if (fondo_texto && ((ln == 5 && !memcmp(n, "color", 5)) || (ln == 23 && !memcmp(n, "-webkit-text-fill-color", 23))) &&
      lv >= 11 && !memcmp(v, "transparent", 11)) return;
  if (es_fondo(n, ln) && busca_grad(v, lv)) { sin_degradado(x, n, ln, v, lv); return; }
  if (ln == 5 && !memcmp(n, "inset", 5) && pon_inset(x, v, lv)) return;
  tx_pon(x, n, ln); tx_cad(x, ":"); pon_valor(x, v, lv); tx_cad(x, ";");
  /* lo que se esconde fuera de la pantalla moviéndolo (el menú lateral
   * «off-canvas»: transform: translateX(-100%)): NetSurf no mueve, así que
   * se quedaba encima de la página; se esconde */
  /* un brillo de adorno (filter: blur() grande: los «blobs» de colores de
   * clicgest.es): sin desenfoque salía un círculo de color; se esconde */
  if (ln == 6 && !memcmp(n, "filter", 6) && lv > 5 && strstr_n(v, lv, "blur(")) {
    const char *b = v;
    while (b + 5 <= v + lv && memcmp(b, "blur(", 5)) b++;
    if (atoi(b + 5) >= 16) tx_cad(x, "visibility:hidden;");
  }
  if (ln == 9 && !memcmp(n, "transform", 9) &&
      (strstr_n(v, lv, "translateX(-100%") || strstr_n(v, lv, "translate(-100%") || strstr_n(v, lv, "translate3d(-100%")))
    tx_cad(x, "visibility:hidden;");
}

/* ---- la salida ---- */
static int partes(const char *s, size_t l, const char **pa, size_t *pl, int max) {
  int n = 0;
  size_t k = 0;
  while (k < l && n < max) {
    const char *e = hasta(s + k, s + l, ",");
    size_t m = e - (s + k);
    pa[n] = recorta(s + k, &m);
    pl[n] = m;
    if (m) n++;
    k = e - s + 1;
  }
  return n;
}
static int compuesto(const char *s, size_t l) {
  size_t k;
  for (k = 0; k < l; k++) if (isspace((unsigned char)s[k]) || s[k] == '>' || s[k] == '+' || s[k] == '~') return 0;
  return 1;
}
static int pegable(char c) { return c == '.' || c == '#' || c == ':' || c == '['; }
/* el selector de la regla añadida: los que llevan las dos (U.D) y los que
 * están dentro de D (D U) */
static void selector_de(Txt *x, const Item *u, const Item *d) {
  const char *pu[16], *pd[16];
  size_t lu[16], ld[16];
  int nu = partes(u->a, u->la, pu, lu, 16), nd = partes(d->a, d->la, pd, ld, 16), i, j, prim = 1;
  for (i = 0; i < nu; i++)
    for (j = 0; j < nd; j++) {
      if (compuesto(pu[i], lu[i]) && compuesto(pd[j], ld[j])) {
        if (pegable(pd[j][0])) { tx_cad(x, prim ? "" : ","); tx_pon(x, pu[i], lu[i]); tx_pon(x, pd[j], ld[j]); prim = 0; }
        else if (pegable(pu[i][0])) { tx_cad(x, prim ? "" : ","); tx_pon(x, pd[j], ld[j]); tx_pon(x, pu[i], lu[i]); prim = 0; }
      }
      tx_cad(x, prim ? "" : ",");
      tx_pon(x, pd[j], ld[j]); tx_cad(x, " "); tx_pon(x, pu[i], lu[i]);
      prim = 0;
    }
}
/* ¿pone d alguna de las variables que usa la declaración? */
static int pone_alguna(const Item *d, const char *v, size_t l) {
  size_t k;
  for (k = 0; k + 4 <= l; k++) {
    if (memcmp(v + k, "var(", 4)) continue;
    {
      const char *a = v + k + 4, *e = hasta(a, v + l, ",)");
      size_t ln = e - a;
      const char *n = recorta(a, &ln);
      if (var_de(d->loc, n, ln)) return 1;
    }
  }
  return 0;
}
#define MAX_DEF 24          /* las reglas añadidas por cada regla que usa variables */
/* las reglas de un estado (:hover, :focus, :active, :disabled...) que ponen
 * variables no dan reglas añadidas: en un 486 la memoria vale más que el
 * color al pasar el ratón */
static int de_estado(const char *s, size_t l) {
  size_t k;
  for (k = 0; k + 1 < l; k++) if (s[k] == ':' && s[k + 1] != ':' && strncmp(s + k, ":root", 5) && strncmp(s + k, ":not(", 5)) return 1;
  return 0;
}
static void escribe(Txt *x, Item *lista, Item *todas) {
  Item *i, *d;
  for (i = lista; i; i = i->sig) {
    int k;
    if (i->tipo == I_TAL) { tx_pon(x, i->a, i->la); tx_cad(x, "\n"); continue; }
    if (i->tipo == I_GRUPO) { tx_pon(x, i->a, i->la); tx_cad(x, "{\n"); escribe(x, i->hijos, todas); tx_cad(x, "}\n"); continue; }
    tx_pon(x, i->a, i->la);
    tx_cad(x, "{");
    fondo_texto = 0;
    for (k = 0; k < i->nd; k++) {
      Decl *e = &i->d[k];
      size_t lv = e->lv;
      const char *v = recorta(e->v, &lv);
      if (((e->ln == 15 && !memcmp(e->n, "background-clip", 15)) || (e->ln == 23 && !memcmp(e->n, "-webkit-background-clip", 23))) &&
          lv >= 4 && !memcmp(v, "text", 4)) fondo_texto = 1;
    }
    for (k = 0; k < i->nd; k++) {
      Decl *e = &i->d[k];
      if (e->ln > 2 && e->n[0] == '-' && e->n[1] == '-') continue;   /* (las variables, libcss no las quiere) */
      if (!tiene_var(e->v, e->lv)) { pon_decl(x, e->n, e->ln, e->v, e->lv); continue; }
      {
        Txt v = { 0 };
        if (expande(&v, e->v, e->lv, i->loc, NULL, 0) && v.t) pon_decl(x, e->n, e->ln, v.t, v.n);
        free(v.t);
      }
    }
    tx_cad(x, "}\n");
    fondo_texto = 0;
  }
  /* las reglas añadidas: las que usan variables que pone otra regla */
  for (i = lista; i; i = i->sig) {
    int k, n_def = 0;
    if (i->tipo != I_REGLA) continue;
    for (k = 0; k < i->nd; k++) if (tiene_var(i->d[k].v, i->d[k].lv) && !(i->d[k].ln > 2 && i->d[k].n[0] == '-')) break;
    if (k == i->nd) continue;
    for (d = todas; d && n_def < MAX_DEF; d = d->sig) {
      Txt s = { 0 }, cuerpo = { 0 };
      if (d == i || d->tipo != I_REGLA || !d->loc || es_global(d->a, d->la) || de_estado(d->a, d->la)) continue;
      if (d->la == i->la && !memcmp(d->a, i->a, i->la)) continue;
      for (k = 0; k < i->nd; k++) {
        Decl *e = &i->d[k];
        Txt v = { 0 };
        if (e->ln > 2 && e->n[0] == '-' && e->n[1] == '-') continue;
        if (!tiene_var(e->v, e->lv) || !pone_alguna(d, e->v, e->lv)) continue;
        if (expande(&v, e->v, e->lv, d->loc, i->loc, 0) && v.t) pon_decl(&cuerpo, e->n, e->ln, v.t, v.n);
        free(v.t);
      }
      if (cuerpo.t) {
        selector_de(&s, i, d);
        if (s.t) { tx_cad(x, s.t); tx_cad(x, "{"); tx_cad(x, cuerpo.t); tx_cad(x, "}\n"); n_def++; }
      }
      free(s.t); free(cuerpo.t);
    }
  }
}

char *cssvar_expande(const char *css, size_t n, size_t *sale) {
  const char *p = css, *f = css + n;
  Item *arbol;
  Txt x = { 0 };
  /* nada que hacer si no hay «var(» ni «--» */
  {
    size_t k;
    int hay = 0;
    for (k = 0; k + 9 <= n && !hay; k++)
      if ((css[k] == 'v' && !memcmp(css + k, "var(", 4)) || (css[k] == '-' && css[k + 1] == '-') || (css[k] == 'g' && !memcmp(css + k, "gradient(", 9)) ||
          (css[k] == 't' && !memcmp(css + k, "translate", 9)) || (css[k] == 'c' && !memcmp(css + k, "clamp(", 6)) ||
          (css[k] == 'e' && !memcmp(css + k, "env(", 4)) || (css[k] == 'i' && !memcmp(css + k, "inset", 5)) ||
          ((css[k] == 'm') && es_mates(css, n, k))) hay = 1;
    if (!hay) return NULL;
  }
  arbol = lee(&p, f, 0);
  /* (las reglas que ponen variables se buscan en el bloque de fuera) */
  escribe(&x, arbol, arbol);
  suelta(arbol);
  if (x.mal) { free(x.t); return NULL; }
  *sale = x.n;
  return x.t;
}

/* un atributo style («color: var(--tinta); margin: 0»): solo declaraciones */
char *cssvar_estilo(const char *t, size_t n, size_t *sale) {
  char *b, *r;
  size_t m;
  if (n < 5 || !memchr(t, '(', n)) return NULL;   /* (sin var() ni degradados) */
  b = malloc(n + 8);
  if (!b) return NULL;
  memcpy(b, "x{", 2); memcpy(b + 2, t, n); memcpy(b + 2 + n, "}", 1);
  r = cssvar_expande(b, n + 3, &m);
  free(b);
  if (!r) return NULL;
  /* «x{...}\n» -> «...» */
  if (m >= 4 && r[0] == 'x' && r[1] == '{') {
    char *e = strchr(r, '}');
    m = e ? (size_t)(e - r - 2) : m - 2;
    memmove(r, r + 2, m);
    r[m] = 0;
    *sale = m;
    return r;
  }
  free(r);
  return NULL;
}
