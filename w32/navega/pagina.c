/* pagina.c - el Navegador de Hispa-486: del documento a la página.
 * La letra de Hispa Ventanas es fija (8×16), así que la página es una rejilla
 * de letras, como en un terminal: cada fila, sus letras y el atributo de cada
 * una (negrita, color, enlace, control). Aquí están el estilo (lo poco de CSS
 * que se entiende y lo que se sabe de memoria de las hojas de estilo más
 * corrientes), la maqueta (párrafos, listas, tablas, formularios) y los
 * formularios (qué se envía).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "nav.h"

#define LMAX 320               /* letras de una línea como mucho */
#define PALETA(n) (0x01000000u | (n))

/* =============================================================================
 *  El estilo
 * ============================================================================= */
typedef struct {
  u8 etq, oculto, negrita, alin, subraya, hay_color, hay_fondo, del_atr_hidden;
  const char *clase, *clase2, *id;
  u32 color, fondo;
} Regla;
static const struct { const char *n; u32 c; } nombres_color[] = {
  { "black", 0x000000 }, { "white", 0xFFFFFF }, { "red", 0x0000FF }, { "green", 0x008000 }, { "blue", 0xFF0000 },
  { "gray", 0x808080 }, { "grey", 0x808080 }, { "silver", 0xC0C0C0 }, { "maroon", 0x000080 }, { "navy", 0x800000 },
  { "purple", 0x800080 }, { "teal", 0x808000 }, { "olive", 0x008080 }, { "orange", 0x00A5FF }, { "yellow", 0x00FFFF },
  { "lime", 0x00FF00 }, { "aqua", 0xFFFF00 }, { "fuchsia", 0xFF00FF }, { "brown", 0x2A2AA5 }, { "darkred", 0x00008B },
  { "darkgreen", 0x006400 }, { "darkblue", 0x8B0000 }, { "lightgray", 0xD3D3D3 }, { "lightgrey", 0xD3D3D3 },
  { "darkgray", 0xA9A9A9 }, { "crimson", 0x3C14DC }, { "gold", 0x00D7FF }, { "pink", 0xCBC0FF }
};
static int hex1(int c) { return c >= '0' && c <= '9' ? c - '0' : (c | 32) >= 'a' && (c | 32) <= 'f' ? (c | 32) - 'a' + 10 : -1; }
/* un color de CSS -> COLORREF (00BBGGRR); 0 si no se entiende */
static int color_css(const char *t, u32 *c) {
  int k, n = 0, v[6];
  u32 j;
  while (*t == ' ') t++;
  if (*t == '#') {
    for (k = 1; k <= 6 && hex1(t[k]) >= 0; k++) v[n++] = hex1(t[k]);
    if (n == 3) { *c = (u32)(v[0] * 17) | (u32)(v[1] * 17) << 8 | (u32)(v[2] * 17) << 16; return 1; }
    if (n == 6) { *c = (u32)(v[0] * 16 + v[1]) | (u32)(v[2] * 16 + v[3]) << 8 | (u32)(v[4] * 16 + v[5]) << 16; return 1; }
    return 0;
  }
  if (!_strnicmp(t, "rgb", 3)) {
    const char *p = strchr(t, '(');
    if (!p) return 0;
    for (k = 0, p++; k < 3; k++) {
      while (*p == ' ' || *p == ',') p++;
      if (*p < '0' || *p > '9') return 0;
      v[k] = atoi(p);
      if (v[k] > 255) v[k] = 255;
      while (*p >= '0' && *p <= '9') p++;
    }
    *c = (u32)v[0] | (u32)v[1] << 8 | (u32)v[2] << 16;
    return 1;
  }
  for (j = 0; j < sizeof nombres_color / sizeof nombres_color[0]; j++) {
    u32 l = strlen(nombres_color[j].n);
    if (!_strnicmp(t, nombres_color[j].n, l) && !((t[l] | 32) >= 'a' && (t[l] | 32) <= 'z')) { *c = nombres_color[j].c; return 1; }
  }
  return 0;
}
static int luz(u32 c) { return (int)((c & 255) * 3 + (c >> 8 & 255) * 6 + (c >> 16 & 255)) / 10; }
/* el índice de un color en la página (se añade si no está) */
static int color_pag(Pagina *p, u32 c) {
  int k;
  for (k = K_N; k < p->n_color; k++) if (p->color[k] == c) return k;
  if (p->n_color >= 64) return 0;
  p->color[p->n_color] = c;
  return p->n_color++;
}
/* las declaraciones «prop: valor; ...» (hasta fin o «}») a la regla */
static void declaraciones(const char *t, const char *fin, Regla *r) {
  while (t < fin) {
    const char *dp, *pc, *v;
    char prop[24];
    int n = 0;
    while (t < fin && (*t <= ' ' || *t == ';')) t++;
    for (dp = t; dp < fin && *dp != ':' && *dp != ';'; dp++) ;
    for (pc = dp; pc < fin && *pc != ';'; pc++) ;
    if (dp < fin && *dp == ':') {
      for (v = t; v < dp && n < 23; v++) if (*v > ' ') prop[n++] = (char)min_c(*v);
      prop[n] = 0;
      v = dp + 1;
      while (v < pc && *v <= ' ') v++;
      if (!strcmp(prop, "display")) { if (!_strnicmp(v, "none", 4)) r->oculto = 1; else r->oculto = 2; }
      else if (!strcmp(prop, "visibility")) { if (!_strnicmp(v, "hidden", 6)) r->oculto = 1; }
      else if (!strcmp(prop, "color")) { if (color_css(v, &r->color)) r->hay_color = 1; }
      else if (!strcmp(prop, "background-color") || !strcmp(prop, "background")) { if (color_css(v, &r->fondo)) r->hay_fondo = 1; }
      else if (!strcmp(prop, "font-weight")) { if (!_strnicmp(v, "bold", 4) || (*v >= '6' && *v <= '9')) r->negrita = 1; }
      else if (!strcmp(prop, "text-align")) {
        if (!_strnicmp(v, "center", 6)) r->alin = 1;
        else if (!_strnicmp(v, "right", 5) || !_strnicmp(v, "end", 3)) r->alin = 2;
      } else if (!strcmp(prop, "text-decoration")) { if (!_strnicmp(v, "underline", 9)) r->subraya = 1; }
    }
    t = pc + 1;
  }
}
/* un selector sencillo: [etiqueta][.clase][.clase][#id] o [hidden]; 0 si
 * lleva algo más (descendientes, seudoclases...): esas reglas no se usan */
static int selector(Pagina *p, const char *t, const char *fin, Regla *r) {
  char b[64];
  int n;
  while (t < fin && *t <= ' ') t++;
  while (fin > t && fin[-1] <= ' ') fin--;
  if (t >= fin) return 0;
  if (fin - t == 8 && !memcmp(t, "[hidden]", 8)) { r->del_atr_hidden = 1; return 1; }
  if (*t == '*' ) return 0;
  for (n = 0; t < fin && ((*t | 32) >= 'a' && (*t | 32) <= 'z' || (*t >= '0' && *t <= '9')) && n < 20; t++) b[n++] = (char)min_c(*t);
  b[n] = 0;
  if (n) {
    static const char *const nom[] = { "div", "p", "span", "a", "table", "td", "th", "tr", "ul", "li", "h1", "h2", "h3",
      "nav", "aside", "form", "label", "small", "section", "header", "footer", "button", "input", "details", "summary" };
    static const u8 cod[] = { E_DIV, E_P, E_SPAN, E_A, E_TABLE, E_TD, E_TH, E_TR, E_UL, E_LI, E_H1, E_H2, E_H3,
      E_NAV, E_ASIDE, E_FORM, E_LABEL, E_SMALL, E_SECTION, E_HEADER, E_FOOTER, E_BUTTON, E_INPUT, E_DETAILS, E_SUMMARY };
    u32 k;
    for (k = 0; k < sizeof cod; k++) if (!strcmp(nom[k], b)) { r->etq = cod[k]; break; }
    if (k == sizeof cod) return 0;
  }
  while (t < fin) {
    int que = *t++;
    if (que != '.' && que != '#') return 0;
    for (n = 0; t < fin && (((*t | 32) >= 'a' && (*t | 32) <= 'z') || (*t >= '0' && *t <= '9') || *t == '-' || *t == '_') && n < 62; t++) b[n++] = *t;
    if (!n) return 0;
    if (que == '#') { if (r->id) return 0; r->id = ar_texto(&p->doc.ar, b, n); }
    else if (!r->clase) r->clase = ar_texto(&p->doc.ar, b, n);
    else if (!r->clase2) r->clase2 = ar_texto(&p->doc.ar, b, n);
    else return 0;
  }
  return r->etq || r->clase || r->id;
}
#define REGLAS_MAX 400
/* las reglas de los <style>: de css[i..fin); px = el ancho de la ventana */
static void css_lee(Pagina *p, const char *c, const char *fin, int px) {
  Regla *rs = p->reglas;
  while (c < fin) {
    const char *llave, *cierre, *s;
    int nivel;
    while (c < fin && *c <= ' ') c++;
    if (c + 1 < fin && c[0] == '/' && c[1] == '*') {
      for (c += 2; c + 1 < fin && !(c[0] == '*' && c[1] == '/'); c++) ;
      c += 2;
      continue;
    }
    for (llave = c; llave < fin && *llave != '{' && *llave != ';' && *llave != '}'; llave++) ;
    if (llave >= fin) return;
    if (*llave != '{') { c = llave + 1; continue; }
    for (cierre = llave + 1, nivel = 1; cierre < fin; cierre++) {
      if (*cierre == '{') nivel++;
      else if (*cierre == '}' && --nivel == 0) break;
    }
    if (*c == '@') {
      /* @media: solo lo que vale para una pantalla de este ancho */
      if (!_strnicmp(c, "@media", 6)) {
        int vale = 1;
        const char *m;
        char cond[200];
        int n = llave - c < 199 ? llave - c : 199;
        memcpy(cond, c, n); cond[n] = 0;
        if (strstr(cond, "print") && !strstr(cond, "screen")) vale = 0;
        if ((m = strstr(cond, "max-width")) && strchr(m, ':') && atoi(strchr(m, ':') + 1) < px) vale = 0;
        if ((m = strstr(cond, "min-width")) && strchr(m, ':') && atoi(strchr(m, ':') + 1) > px) vale = 0;
        if (strstr(cond, "prefers-") || strstr(cond, "hover")) vale = 0;
        if (vale) css_lee(p, llave + 1, cierre, px);
      }
      c = cierre + 1;
      continue;
    }
    for (s = c; s < llave; ) {
      const char *coma = s;
      Regla r;
      while (coma < llave && *coma != ',') coma++;
      memset(&r, 0, sizeof r);
      if (p->n_reglas < REGLAS_MAX && selector(p, s, coma, &r)) {
        declaraciones(llave + 1, cierre < fin ? cierre : fin, &r);
        if (r.oculto || r.negrita || r.alin || r.subraya || r.hay_color || r.hay_fondo) rs[p->n_reglas++] = r;
      }
      s = coma + 1;
    }
    c = cierre + 1;
  }
}
static int clase_emp(const Nodo *n, const char *pre) {    /* ¿alguna clase empieza así? */
  const char *t = atr(n, "class");
  u32 l = strlen(pre);
  while (t && *t) {
    while (*t == ' ') t++;
    if (!strncmp(t, pre, l)) return 1;
    while (*t && *t != ' ') t++;
  }
  return 0;
}
static int cuenta_enlaces(const Nodo *n) {
  int c = !n->texto && n->etq == E_A;
  const Nodo *h;
  for (h = n->hijo; h; h = h->sig) if (!h->texto) c += cuenta_enlaces(h);
  return c;
}
/* el estilo de un elemento: se calcula una vez */
static void estilo(Pagina *p, Nodo *n) {
  Regla *rs = p->reglas, r;
  const char *cl = atr(n, "class"), *id = atr(n, "id"), *st = atr(n, "style"), *t;
  int k, oculto = 0;
  n->est |= EN_VISTO;
  memset(&r, 0, sizeof r);
  if (atr(n, "hidden")) oculto = 1;
  if (n->etq == E_DIALOG && !atr(n, "open")) oculto = 1;
  if (n->etq == E_INPUT && (t = atr(n, "type")) && !_stricmp(t, "hidden")) oculto = 1;
  if (cl) {
    /* lo que se sabe de memoria de Bootstrap (las hojas de estilo de fuera
     * no se bajan): lo que esas clases esconden en una pantalla de 800 */
    if (tiene_clase(n, "d-none") && !clase_emp(n, "d-sm-") && !clase_emp(n, "d-md-")) oculto = 1;
    if (tiene_clase(n, "d-sm-none") || tiene_clase(n, "d-md-none")) oculto = 1;
    if (tiene_clase(n, "collapse") && !tiene_clase(n, "show")) oculto = 1;
    if (tiene_clase(n, "dropdown-menu") && !tiene_clase(n, "show")) oculto = 1;
    if (tiene_clase(n, "tab-pane") && !tiene_clase(n, "active")) oculto = 1;
    if (tiene_clase(n, "toast") && !tiene_clase(n, "show")) oculto = 1;
    if (tiene_clase(n, "modal") || tiene_clase(n, "offcanvas") || tiene_clase(n, "visually-hidden-focusable")) oculto = 1;
    if ((tiene_clase(n, "invalid-feedback") || tiene_clase(n, "valid-feedback")) && !tiene_clase(n, "d-block")) oculto = 1;
    if (tiene_clase(n, "fw-bold") || tiene_clase(n, "fw-semibold") || tiene_clase(n, "fw-bolder")) r.negrita = 1;
    if (tiene_clase(n, "text-center")) r.alin = 1;
    if (tiene_clase(n, "text-end") || tiene_clase(n, "text-right")) r.alin = 2;
    if (tiene_clase(n, "text-danger") || tiene_clase(n, "alert-danger")) n->fg = K_ROJO;
    if (tiene_clase(n, "text-success") || tiene_clase(n, "alert-success")) n->fg = K_VERDE;
    if (tiene_clase(n, "text-muted") || tiene_clase(n, "text-secondary") || tiene_clase(n, "text-body-secondary")) n->fg = K_GRIS;
    if (tiene_clase(n, "alert")) n->bg = K_AVISO;
    if (tiene_clase(n, "badge")) n->bg = K_CABEZA;
    if (n->etq == E_A && tiene_clase(n, "btn")) n->est |= EN_BOTON;
  }
  for (k = 0; k < p->n_reglas; k++) {
    Regla *q = &rs[k];
    if (q->del_atr_hidden) continue;
    if (q->etq && q->etq != n->etq) continue;
    if (q->id && (!id || strcmp(id, q->id))) continue;
    if (q->clase && (!cl || !tiene_clase(n, q->clase))) continue;
    if (q->clase2 && !tiene_clase(n, q->clase2)) continue;
    if (q->oculto == 1) oculto = 1;
    if (q->oculto == 2 && !atr(n, "hidden")) oculto = 0;
    if (q->negrita) r.negrita = 1;
    if (q->alin) r.alin = q->alin;
    if (q->subraya) r.subraya = 1;
    if (q->hay_color) { r.hay_color = 1; r.color = q->color; }
    if (q->hay_fondo) { r.hay_fondo = 1; r.fondo = q->fondo; }
  }
  if (st) {
    Regla q;
    memset(&q, 0, sizeof q);
    declaraciones(st, st + strlen(st), &q);
    if (q.oculto == 1) oculto = 1;
    if (q.oculto == 2 && !atr(n, "hidden")) oculto = 0;
    if (q.negrita) r.negrita = 1;
    if (q.alin) r.alin = q.alin;
    if (q.subraya) r.subraya = 1;
    if (q.hay_color) { r.hay_color = 1; r.color = q.color; }
    if (q.hay_fondo) { r.hay_fondo = 1; r.fondo = q.fondo; }
  }
  if (n->etq == E_FONT && (t = atr(n, "color")) && color_css(t, &r.color)) r.hay_color = 1;
  if ((t = atr(n, "align"))) { if (!_stricmp(t, "center")) r.alin = 1; else if (!_stricmp(t, "right")) r.alin = 2; }
  n->oculto = (u8)oculto;
  if (r.negrita) n->est |= EN_NEGRITA;
  if (r.subraya) n->est |= EN_SUBRAYA;
  if (r.alin == 1) n->est |= EN_CENTRO;
  if (r.alin == 2) n->est |= EN_DERECHA;
  /* los colores de la página, solo si se van a poder leer: el fondo tiene
   * que ser claro (la letra por omisión es oscura) y la letra, oscura */
  if (r.hay_fondo && luz(r.fondo) >= 150 && luz(r.fondo) < 250) n->bg = (u8)color_pag(p, r.fondo);
  if (r.hay_color && luz(r.color) < 140) n->fg = (u8)color_pag(p, r.color);
  if ((n->etq == E_NAV || n->etq == E_ASIDE) && cuenta_enlaces(n) > 8) {
    const Nodo *a;
    for (a = n->padre; a && !(a->est & EN_PLEGABLE); a = a->padre) ;
    if (!a) n->est |= EN_PLEGABLE;             /* (uno dentro de otro: solo el de fuera) */
  }
}

/* =============================================================================
 *  Los controles de formulario
 * ============================================================================= */
void ctl_pon(Ctl *c, const char *t, int n) {
  if (n < 0) n = strlen(t);
  if (n + 1 > c->cap_valor) {
    char *v = realloc(c->valor, n + 64);
    if (!v) return;
    c->valor = v; c->cap_valor = n + 64;
  }
  memcpy(c->valor, t, n);
  c->valor[n] = 0;
  c->n_valor = n;
  if (c->cursor > n) c->cursor = n;
}
static void opciones(Ctl *c, Nodo *n, Arena *a, int cuenta) {
  Nodo *h;
  for (h = n->hijo; h; h = h->sig) {
    if (h->texto) continue;
    if (h->etq == E_OPTION) {
      if (!cuenta) {
        c->opc[c->n_opc] = h;
        if (atr(h, "selected")) c->elegida = c->n_opc;
      }
      c->n_opc++;
    } else opciones(c, h, a, cuenta);
  }
}
static void ctl_crea(Pagina *p, Nodo *n) {
  Ctl *c;
  const char *t = atr(n, "type"), *v = atr(n, "value");
  Nodo *f;
  char b[600];
  if (p->n_ctl == p->cap_ctl) {
    Ctl *q = realloc(p->ctl, (p->cap_ctl + 32) * sizeof(Ctl));
    if (!q) return;
    p->ctl = q; p->cap_ctl += 32;
  }
  c = &p->ctl[p->n_ctl];
  memset(c, 0, sizeof *c);
  c->nodo = n;
  c->nombre = atr(n, "name");
  c->desactivado = atr(n, "disabled") != NULL;
  c->sololee = atr(n, "readonly") != NULL;
  c->obligado = atr(n, "required") != NULL;
  c->max = 4000;
  for (f = n->padre; f && f->etq != E_FORM; f = f->padre) ;
  c->forma = f;
  if (n->etq == E_INPUT) {
    if (!t) t = "text";
    if (!_stricmp(t, "hidden")) c->tipo = C_OCULTO;
    else if (!_stricmp(t, "password")) c->tipo = C_CLAVE;
    else if (!_stricmp(t, "checkbox")) c->tipo = C_CASILLA;
    else if (!_stricmp(t, "radio")) c->tipo = C_RADIO;
    else if (!_stricmp(t, "submit") || !_stricmp(t, "image")) c->tipo = C_ENVIAR;
    else if (!_stricmp(t, "button")) c->tipo = C_BOTON;
    else if (!_stricmp(t, "reset")) c->tipo = C_REINICIO;
    else if (!_stricmp(t, "file")) c->tipo = C_ARCHIVO;
    else c->tipo = C_TEXTO;
    if (c->tipo == C_CASILLA || c->tipo == C_RADIO) {
      c->marcado = c->marcado0 = atr(n, "checked") != NULL;
      ctl_pon(c, v ? v : "on", -1);
    } else if (c->tipo == C_ENVIAR) ctl_pon(c, v ? v : "Enviar", -1);
    else if (c->tipo == C_REINICIO) ctl_pon(c, v ? v : "Restablecer", -1);
    else ctl_pon(c, v ? v : "", -1);
    if (atr(n, "maxlength") && atoi(atr(n, "maxlength")) > 0) c->max = atoi(atr(n, "maxlength"));
  } else if (n->etq == E_BUTTON) {
    c->tipo = !t || !_stricmp(t, "submit") ? C_ENVIAR : !_stricmp(t, "reset") ? C_REINICIO : C_BOTON;
    ctl_pon(c, v ? v : "", -1);
  } else if (n->etq == E_TEXTAREA) {
    c->tipo = C_AREA;
    c->max = 20000;
    b[0] = 0;
    if (n->hijo && n->hijo->texto) {
      const char *x = n->hijo->t;
      u32 l = n->hijo->n;
      if (l && x[0] == '\r') { x++; l--; }
      if (l && x[0] == '\n') { x++; l--; }
      ctl_pon(c, x, (int)l);
    } else ctl_pon(c, "", 0);
    if (atr(n, "maxlength") && atoi(atr(n, "maxlength")) > 0) c->max = atoi(atr(n, "maxlength"));
  } else {
    c->tipo = C_LISTA;
    opciones(c, n, &p->doc.ar, 1);
    c->opc = ar_pide(&p->doc.ar, (c->n_opc + 1) * sizeof(Nodo *));
    c->n_opc = 0;
    if (c->opc) opciones(c, n, &p->doc.ar, 0);
    c->elegida0 = c->elegida;
    t = atr(n, "onchange");
    c->cambia_envia = t && strstr(t, "submit") != NULL;
    ctl_pon(c, "", 0);
  }
  c->valor0 = ar_texto(&p->doc.ar, c->valor ? c->valor : "", -1);
  c->cursor = c->n_valor;
  n->ctl = (short)(p->n_ctl + 1);
  p->n_ctl++;
}
static void ctl_todos(Pagina *p, Nodo *n) {
  Nodo *h;
  if (n->texto) return;
  if (n->etq == E_INPUT || n->etq == E_SELECT || n->etq == E_TEXTAREA || n->etq == E_BUTTON) {
    ctl_crea(p, n);
    return;
  }
  for (h = n->hijo; h; h = h->sig) ctl_todos(p, h);
}
/* el texto de «confirm('...')» de onsubmit u onclick: lo que la página
 * preguntaría con JavaScript antes de seguir */
const char *pag_confirma(Nodo *n, char *b, int max) {
  const char *t = atr(n, "onsubmit"), *c;
  int k = 0, q;
  if (!t || !strstr(t, "confirm(")) t = atr(n, "onclick");
  if (!t || !(c = strstr(t, "confirm("))) return NULL;
  c += 8;
  while (*c == ' ') c++;
  q = *c;
  if (q != '\'' && q != '"') return NULL;
  for (c++; *c && *c != q && k < max - 1; c++) {
    if (*c == '\\' && c[1]) {
      c++;
      if (*c == 'n') { b[k++] = '\n'; continue; }
      if (*c == 'u' && hex1(c[1]) >= 0 && hex1(c[2]) >= 0 && hex1(c[3]) >= 0 && hex1(c[4]) >= 0) {
        int u = hex1(c[1]) << 12 | hex1(c[2]) << 8 | hex1(c[3]) << 4 | hex1(c[4]);
        b[k++] = (char)(u < 256 ? u : '?');
        c += 4;
        continue;
      }
    }
    b[k++] = *c;
  }
  b[k] = 0;
  return b;
}
static void campo(const char *nom, const char *val, int n, char **d, u32 *nd, u32 *cap) {
  if (*nd) url_codifica("&", -2, d, nd, cap);
  url_codifica(nom, strlen(nom), d, nd, cap);
  url_codifica("=", -2, d, nd, cap);
  url_codifica(val, n, d, nd, cap);
}
int pag_formulario(Pagina *p, Nodo *forma, int con, char **url, char **cuerpo, u32 *n_cuerpo, int *post) {
  const char *accion, *metodo, *t;
  char *d = NULL, *abs;
  u32 nd = 0, cap = 0;
  int k;
  if (!forma) return 0;
  accion = atr(forma, "action"); metodo = atr(forma, "method");
  if (con >= 0) {
    if ((t = atr(p->ctl[con].nodo, "formaction"))) accion = t;
    if ((t = atr(p->ctl[con].nodo, "formmethod"))) metodo = t;
  }
  *post = metodo && !_stricmp(metodo, "post");
  for (k = 0; k < p->n_ctl; k++) {
    Ctl *c = &p->ctl[k];
    if (c->forma != forma || c->desactivado || !c->nombre || !c->nombre[0]) continue;
    switch (c->tipo) {
    case C_TEXTO: case C_CLAVE: case C_AREA: case C_OCULTO:
      campo(c->nombre, c->valor, c->n_valor, &d, &nd, &cap);
      break;
    case C_CASILLA: case C_RADIO:
      if (c->marcado) campo(c->nombre, c->valor, c->n_valor, &d, &nd, &cap);
      break;
    case C_LISTA:
      if (c->n_opc && c->elegida < c->n_opc) {
        char b[300];
        t = atr(c->opc[c->elegida], "value");
        if (!t) { b[0] = 0; texto_de(c->opc[c->elegida], b, sizeof b); t = b; while (*t == ' ') t++; }
        campo(c->nombre, t, strlen(t), &d, &nd, &cap);
      }
      break;
    case C_ENVIAR:
      if (k == con) campo(c->nombre, c->valor, c->n_valor, &d, &nd, &cap);
      break;
    }
  }
  if (!d) { d = malloc(1); if (!d) return 0; }
  d[nd] = 0;
  abs = url_absoluta(p->doc.base ? p->doc.base : p->url, accion && accion[0] ? accion : p->url);
  if (!abs) { free(d); return 0; }
  if (strchr(abs, '#')) *strchr(abs, '#') = 0;
  if (*post) { *url = abs; *cuerpo = d; *n_cuerpo = nd; return 1; }
  /* GET: los campos van en la dirección, en lugar de la consulta que hubiera */
  if (strchr(abs, '?')) *strchr(abs, '?') = 0;
  *url = malloc(strlen(abs) + nd + 2);
  if (!*url) { free(abs); free(d); return 0; }
  strcpy(*url, abs);
  if (nd) { strcat(*url, "?"); strcat(*url, d); }
  free(abs); free(d);
  *cuerpo = NULL; *n_cuerpo = 0;
  return 1;
}

/* =============================================================================
 *  La rejilla
 * ============================================================================= */
static int atr_id(Pagina *p, int est, int fg, int bg, int enlace, int ctl) {
  int k;
  for (k = p->n_atrs - 1; k >= 0; k--) {
    Atr *a = &p->atrs[k];
    if (a->est == est && a->fg == fg && a->bg == bg && a->enlace == enlace && a->ctl == ctl) return k;
  }
  if (p->n_atrs == p->cap_atrs) {
    Atr *q = realloc(p->atrs, (p->cap_atrs + 64) * sizeof(Atr));
    if (!q) return 0;
    p->atrs = q; p->cap_atrs += 64;
  }
  if (p->n_atrs >= 65000) return 0;
  p->atrs[p->n_atrs].est = (u8)est; p->atrs[p->n_atrs].fg = (u8)fg; p->atrs[p->n_atrs].bg = (u8)bg;
  p->atrs[p->n_atrs].enlace = (short)enlace; p->atrs[p->n_atrs].ctl = (short)ctl;
  return p->n_atrs++;
}
static Fila *fila(Pagina *p, int y, int hasta) {
  Fila *f;
  if (y < 0 || y > 30000) return NULL;
  if (y >= p->cap_filas) {
    int cap = y + 64;
    Fila *q = realloc(p->filas, cap * sizeof(Fila));
    if (!q) return NULL;
    memset(q + p->cap_filas, 0, (cap - p->cap_filas) * sizeof(Fila));
    p->filas = q; p->cap_filas = cap;
  }
  if (y >= p->n_filas) p->n_filas = y + 1;
  f = &p->filas[y];
  if (hasta > f->n) {
    u8 *c = realloc(f->c, hasta);
    u16 *a;
    if (!c) return NULL;
    f->c = c;
    a = realloc(f->a, hasta * sizeof(u16));
    if (!a) return NULL;
    f->a = a;
    memset(c + f->n, ' ', hasta - f->n);
    memset(a + f->n, 0, (hasta - f->n) * sizeof(u16));
    f->n = (u16)hasta;
    if (hasta > p->ancho) p->ancho = hasta;
  }
  return f;
}
static void fila_pon(Pagina *p, int y, int x, const u8 *c, const u16 *a, int n) {
  Fila *f = fila(p, y, x + n);
  if (!f) return;
  memcpy(f->c + x, c, n);
  memcpy(f->a + x, a, n * sizeof(u16));
}
/* un fondo para las letras [x, x+n) de la fila y que no tengan otro */
static void fila_fondo(Pagina *p, int y, int x, int n, int bg) {
  Fila *f = fila(p, y, x + n);
  int k;
  if (!f) return;
  for (k = x; k < x + n; k++) {
    Atr a = p->atrs[f->a[k]];
    if (!a.bg) f->a[k] = (u16)atr_id(p, a.est, a.fg, bg, a.enlace, a.ctl);
  }
}

/* =============================================================================
 *  La maqueta
 * ============================================================================= */
typedef struct { u8 est, fg, bg; short enlace; } Est;
typedef struct {
  Pagina *p;
  int x0, ancho, y, col, espacio, margen, vacio, alin, pre, medir, max_an, min_an;
  char pref[8];                 /* lo que va delante de la primera línea (la viñeta) */
  u8 lc[LMAX];
  u16 la[LMAX];
} Ctx;
static void caja(Ctx *c, Nodo *n, Est e);
static void hijos(Ctx *c, Nodo *n, Est e) {
  Nodo *h;
  for (h = n->hijo; h; h = h->sig) caja(c, h, e);
}
static void ctx_nuevo(Ctx *c, Pagina *p, int x0, int ancho, int y, int medir) {
  c->p = p; c->x0 = x0; c->ancho = ancho < 1 ? 1 : ancho > LMAX - 1 ? LMAX - 1 : ancho; c->y = y;
  c->col = c->espacio = c->margen = c->alin = c->pre = c->max_an = c->min_an = 0;
  c->vacio = 1; c->medir = medir; c->pref[0] = 0;
}
/* la línea que se estaba llenando, a la página */
static void salta(Ctx *c) {
  Pagina *p = c->p;
  int k, desp = 0;
  if (!c->col) return;
  while (c->col > 0 && c->lc[c->col - 1] == ' ' && p->atrs[c->la[c->col - 1]].ctl < 0 && !(p->atrs[c->la[c->col - 1]].est & A_RAYA)) c->col--;
  if (c->col > c->max_an) c->max_an = c->col;
  if (!c->medir && c->col) {
    if (c->alin == 1) desp = (c->ancho - c->col) / 2;
    else if (c->alin == 2) desp = c->ancho - c->col;
    if (desp < 0) desp = 0;
    fila_pon(p, c->y, c->x0 + desp, c->lc, c->la, c->col);
    for (k = 0; k < c->col; k++) {
      Atr *a = &p->atrs[c->la[k]];
      if (k && c->la[k] == c->la[k - 1]) continue;
      if (a->ctl >= 0 && (!k || p->atrs[c->la[k - 1]].ctl != a->ctl)) { p->ctl[a->ctl].x = c->x0 + desp + k; p->ctl[a->ctl].y = c->y; }
      if (a->enlace >= 0 && p->enl[a->enlace].y < 0) { p->enl[a->enlace].x = c->x0 + desp + k; p->enl[a->enlace].y = c->y; }
    }
  }
  c->y++;
  c->col = 0; c->espacio = 0; c->vacio = 0; c->margen = 0;
}
static void margen(Ctx *c) { salta(c); c->margen = 1; }
/* empieza una línea: el margen pendiente y la viñeta */
static void empieza(Ctx *c) {
  if (c->margen && !c->vacio) c->y++;
  c->margen = 0; c->espacio = 0;
  if (c->pref[0]) {
    int l = strlen(c->pref), k;
    u16 a[8];
    for (k = 0; k < l; k++) a[k] = 0;
    if (!c->medir && c->x0 >= l) fila_pon(c->p, c->y, c->x0 - l, (const u8 *)c->pref, a, l);
    c->pref[0] = 0;
  }
}
/* una palabra (no se parte si cabe en una línea) con su atributo */
static void pon(Ctx *c, const u8 *t, int n, int a) {
  if (n > c->min_an) c->min_an = n > c->ancho && !c->medir ? c->ancho : n;
  if (c->col && !c->pre && c->col + c->espacio + n > c->ancho) salta(c);
  if (!c->col) empieza(c);
  if (c->espacio && c->col < LMAX - 1) {
    c->lc[c->col] = ' ';
    c->la[c->col] = (u16)(c->la[c->col - 1] == a ? a : atr_id(c->p, 0, c->p->atrs[a].fg, c->p->atrs[a].bg, -1, -1));
    c->col++;
  }
  c->espacio = 0;
  while (n > 0) {
    int sitio = (c->pre ? LMAX - 1 : c->ancho) - c->col, q = n < sitio ? n : sitio, k;
    if (q <= 0) { if (c->pre) return; salta(c); empieza(c); continue; }
    memcpy(c->lc + c->col, t, q);
    for (k = 0; k < q; k++) c->la[c->col + k] = (u16)a;
    c->col += q; t += q; n -= q;
    if (n > 0) { if (c->pre) return; salta(c); empieza(c); }
  }
}
static int atr_de(Ctx *c, Est e) { return atr_id(c->p, e.est, e.fg, e.bg, e.enlace, -1); }
static void texto(Ctx *c, const Nodo *n, Est e) {
  const u8 *t = (const u8 *)n->t;
  u32 k = 0, ini;
  int a = atr_de(c, e);
  if (c->pre) {
    while (k < n->n) {
      if (t[k] == '\n') { if (c->col) salta(c); else { empieza(c); c->y++; c->vacio = 0; } k++; continue; }
      if (t[k] == '\r') { k++; continue; }
      if (t[k] == '\t') { int s = 8 - (c->col & 7); pon(c, (const u8 *)"        ", s, a); k++; continue; }
      for (ini = k; k < n->n && t[k] != '\n' && t[k] != '\r' && t[k] != '\t'; k++) ;
      pon(c, t + ini, k - ini, a);
    }
    return;
  }
  while (k < n->n) {
    if (t[k] == ' ' || t[k] == '\n' || t[k] == '\r' || t[k] == '\t' || t[k] == '\f') {
      if (c->col) c->espacio = 1;
      k++;
      continue;
    }
    for (ini = k; k < n->n && !(t[k] == ' ' || t[k] == '\n' || t[k] == '\r' || t[k] == '\t' || t[k] == '\f'); k++) ;
    {
      /* (el espacio duro se escribe como espacio, pero no parte la línea) */
      u8 b[LMAX];
      u32 l = k - ini, j;
      if (l > LMAX - 1) { pon(c, t + ini, l, a); continue; }
      for (j = 0; j < l; j++) b[j] = t[ini + j] == 0xA0 ? ' ' : t[ini + j];
      pon(c, b, l, a);
    }
  }
}
static void literal(Ctx *c, const char *t, int a) { pon(c, (const u8 *)t, strlen(t), a); }
static void foco_pon(Pagina *p, int enlace, int n) {
  if (p->n_foco == p->cap_foco) {
    Foco *q = realloc(p->foco, (p->cap_foco + 64) * sizeof(Foco));
    if (!q) return;
    p->foco = q; p->cap_foco += 64;
  }
  p->foco[p->n_foco].enlace = (u8)enlace; p->foco[p->n_foco].n = (short)n;
  p->n_foco++;
}
static int enlace_nuevo(Ctx *c, const char *href, Nodo *n) {
  Pagina *p = c->p;
  if (c->medir) return -1;
  if (p->n_enl == p->cap_enl) {
    Enlace *q = realloc(p->enl, (p->cap_enl + 64) * sizeof(Enlace));
    if (!q) return -1;
    p->enl = q; p->cap_enl += 64;
  }
  if (p->n_enl >= 32000) return -1;
  p->enl[p->n_enl].href = href; p->enl[p->n_enl].nodo = n; p->enl[p->n_enl].x = 0; p->enl[p->n_enl].y = -1;
  foco_pon(p, 1, p->n_enl);
  return p->n_enl++;
}
/* el ancho, en letras, de un control */
static int ctl_ancho(Ctx *c, Ctl *k) {
  const char *t;
  char b[300];
  int w = 0, j;
  switch (k->tipo) {
  case C_CASILLA: case C_RADIO: return 3;
  case C_ENVIAR: case C_BOTON: case C_REINICIO:
    b[0] = 0;
    if (k->nodo->etq == E_BUTTON) texto_de(k->nodo, b, sizeof b);
    else { j = k->n_valor < 290 ? k->n_valor : 290; memcpy(b, k->valor, j); b[j] = 0; }
    w = strlen(b);
    while (w && b[w - 1] == ' ') w--;
    return w + 4;
  case C_LISTA:
    for (j = 0; j < k->n_opc; j++) {
      int l;
      b[0] = 0;
      texto_de(k->opc[j], b, sizeof b);
      l = strlen(b);
      while (l && b[l - 1] == ' ') l--;
      if (l > w) w = l;
    }
    w += 4;
    return w < 8 ? 8 : w > 44 ? 44 : w;
  case C_ARCHIVO: return 22;
  case C_AREA:
    w = (t = atr(k->nodo, "cols")) ? atoi(t) : 60;
    if (w < 20) w = 20;
    return w > c->ancho ? c->ancho : w;
  default:
    t = atr(k->nodo, "size");
    w = t && atoi(t) > 0 ? atoi(t) + 2 : 0;
    t = atr(k->nodo, "type");
    if (!w) {
      if (t && (!_stricmp(t, "number") || !_stricmp(t, "date") || !_stricmp(t, "time"))) w = 14;
      else if (t && (!_stricmp(t, "search") || !_stricmp(t, "email") || !_stricmp(t, "url"))) w = 32;
      else w = k->tipo == C_CLAVE ? 22 : 28;
      if (k->max + 2 < w) w = k->max + 2;
    }
    if (w < 4) w = 4;
    return w > c->ancho ? c->ancho : w > 60 ? 60 : w;
  }
}
static void control(Ctx *c, Nodo *n, Est e) {
  Pagina *p = c->p;
  Ctl *k;
  u8 b[LMAX];
  int a, w, idx = n->ctl - 1;
  if (idx < 0) return;
  k = &p->ctl[idx];
  if (k->tipo == C_OCULTO) return;
  w = ctl_ancho(c, k);
  if (w > LMAX - 2) w = LMAX - 2;
  a = atr_id(p, 0, e.fg, e.bg, -1, idx);
  if (k->tipo == C_AREA) {
    const char *t = atr(n, "rows");
    int h = t ? atoi(t) : 3, j;
    if (h < 2) h = 2;
    if (h > 10) h = 10;
    salta(c);
    empieza(c);
    if (w > c->max_an) c->max_an = w;
    if (20 > c->min_an) c->min_an = 20;
    if (!c->medir) {
      memset(b, ' ', w);
      for (j = 0; j < h; j++) { u16 at[LMAX]; int q; for (q = 0; q < w; q++) at[q] = (u16)a; fila_pon(p, c->y + j, c->x0, b, at, w); }
      k->x = c->x0; k->y = c->y; k->w = w; k->h = h; k->visible = 1;
      foco_pon(p, 0, idx);
    }
    c->y += h;
    c->vacio = 0;
    return;
  }
  memset(b, ' ', w);
  pon(c, b, w, a);
  if (!c->medir) {
    k->w = w; k->h = 1; k->visible = 1;
    if (!k->desactivado) foco_pon(p, 0, idx);
  }
}
static void raya(Ctx *c, int n) {
  u8 b[LMAX];
  int a = atr_id(c->p, A_RAYA, K_GRIS, 0, -1, -1);
  salta(c);
  if (n > c->ancho) n = c->ancho;
  memset(b, ' ', n);
  empieza(c);
  if (!c->medir) { u16 at[LMAX]; int q; for (q = 0; q < n; q++) at[q] = (u16)a; fila_pon(c->p, c->y, c->x0, b, at, n); }
  c->y++;
  c->vacio = 0;
}

/* ---- las tablas ---- */
#define COLS 32
typedef struct { Nodo *n; int col, cols; } Celda;
static int filas_de(Nodo *t, Nodo **f, int max, int n) {
  Nodo *h;
  for (h = t->hijo; h; h = h->sig) {
    if (h->texto) continue;
    if (h->etq == E_TR) { if (n < max) f[n] = h; n++; }
    else if (h->etq == E_THEAD || h->etq == E_TBODY || h->etq == E_TFOOT) n = filas_de(h, f, max, n);
  }
  return n;
}
static void tabla(Ctx *c, Nodo *t, Est e) {
  Pagina *p = c->p;
  Nodo **fs, *h;
  int nf, k, j, nc = 0, nat[COLS], mn[COLS], w[COLS], off[COLS], total, sitio, cebra, y0;
  Ctx *s;
  nf = filas_de(t, NULL, 0, 0);
  salta(c);
  for (h = t->hijo; h; h = h->sig)
    if (!h->texto && h->etq == E_CAPTION) { Est e2 = e; e2.est |= A_NEGRITA; hijos(c, h, e2); salta(c); }
  if (!nf) return;
  fs = malloc(nf * sizeof(Nodo *));
  s = malloc(sizeof(Ctx));
  if (!fs || !s) { free(fs); free(s); return; }
  filas_de(t, fs, nf, 0);
  for (k = 0; k < COLS; k++) nat[k] = mn[k] = 0;
  /* lo que pide cada columna: lo más ancho y lo que no se puede partir */
  for (k = 0; k < nf; k++) {
    j = 0;
    for (h = fs[k]->hijo; h; h = h->sig) {
      const char *cs;
      int span;
      if (h->texto || (h->etq != E_TD && h->etq != E_TH)) continue;
      if (!(h->est & EN_VISTO)) estilo(p, h);
      if (h->oculto) continue;
      span = (cs = atr(h, "colspan")) ? atoi(cs) : 1;
      if (span < 1) span = 1;
      if (j >= COLS) break;
      if (span == 1) {
        Est e2 = e;
        if (h->etq == E_TH) e2.est |= A_NEGRITA;
        ctx_nuevo(s, p, 0, LMAX - 1, 0, 1);
        hijos(s, h, e2);
        salta(s);
        if (s->max_an > nat[j]) nat[j] = s->max_an;
        if (s->min_an > mn[j]) mn[j] = s->min_an;
      }
      j += span;
      if (j > COLS) j = COLS;
    }
    if (j > nc) nc = j;
  }
  if (!nc) { free(fs); free(s); return; }
  for (k = 0, total = 0; k < nc; k++) { if (nat[k] < 1) nat[k] = 1; if (mn[k] < 1) mn[k] = 1; w[k] = nat[k]; total += w[k]; }
  sitio = c->ancho - 2 * (nc - 1);
  if (c->medir) {
    int tm = 0;
    for (k = 0; k < nc; k++) tm += mn[k];
    if (total + 2 * (nc - 1) > c->max_an) c->max_an = total + 2 * (nc - 1);
    if (tm + 2 * (nc - 1) > c->min_an) c->min_an = tm + 2 * (nc - 1);
    free(fs); free(s);
    c->vacio = 0;
    return;
  }
  while (total > sitio) {                      /* no cabe: se estrecha la más ancha */
    int m = -1;
    for (k = 0; k < nc; k++) if (w[k] > mn[k] && w[k] > 6 && (m < 0 || w[k] > w[m])) m = k;
    if (m < 0) {
      for (k = 0; k < nc; k++) if (w[k] > 6 && (m < 0 || w[k] > w[m])) m = k;
      if (m < 0) break;
    }
    w[m]--; total--;
  }
  for (k = 0, j = 0; k < nc; k++) { off[k] = j; j += w[k] + 2; }
  total += 2 * (nc - 1);
  cebra = nf >= 3;
  empieza(c);
  y0 = c->y;
  for (k = 0; k < nf; k++) {
    int alto = 1, cab = 0, celdas = 0;
    if (!(fs[k]->est & EN_VISTO)) estilo(p, fs[k]);
    if (fs[k]->oculto) continue;
    j = 0;
    for (h = fs[k]->hijo; h; h = h->sig) {
      const char *cs;
      int span, an, q;
      Est e2 = e;
      if (h->texto || (h->etq != E_TD && h->etq != E_TH) || h->oculto) continue;
      if (j >= nc) break;
      span = (cs = atr(h, "colspan")) ? atoi(cs) : 1;
      if (span < 1) span = 1;
      if (j + span > nc) span = nc - j;
      for (an = 0, q = j; q < j + span; q++) an += w[q] + (q > j ? 2 : 0);
      if (h->etq == E_TH) { e2.est |= A_NEGRITA; cab++; }
      if (h->fg) e2.fg = h->fg;
      if (h->est & EN_NEGRITA) e2.est |= A_NEGRITA;
      ctx_nuevo(s, p, c->x0 + off[j], an, y0, 0);
      if (h->est & EN_CENTRO) s->alin = 1;
      if (h->est & EN_DERECHA) s->alin = 2;
      hijos(s, h, e2);
      salta(s);
      if (s->y - y0 > alto) alto = s->y - y0;
      j += span;
      celdas++;
    }
    if (!celdas) continue;
    if (cab == celdas) for (j = 0; j < alto; j++) fila_fondo(p, y0 + j, c->x0, total, K_CABEZA);
    else if (cebra && (k & 1)) for (j = 0; j < alto; j++) fila_fondo(p, y0 + j, c->x0, total, K_ZEBRA);
    y0 += alto;
  }
  c->y = y0;
  c->vacio = 0;
  free(fs); free(s);
}

/* la línea de algo que se pliega: «[+] título» */
static void plegable(Ctx *c, Nodo *n, Est e, const char *titulo, Nodo *resumen) {
  Est e2 = e;
  int a;
  salta(c);
  e2.enlace = (short)enlace_nuevo(c, NULL, n);
  e2.est |= A_NEGRITA;
  e2.fg = K_ENLACE;
  a = atr_de(c, e2);
  literal(c, n->abierto ? "[-]" : "[+]", a);
  c->espacio = 1;
  if (resumen) hijos(c, resumen, e2);
  else literal(c, titulo, a);
  salta(c);
}

static void caja(Ctx *c, Nodo *n, Est e) {
  Pagina *p = c->p;
  int band, alin0, x0, an0, k, a;
  const char *t;
  char b[300];
  Nodo *h;
  if (n->texto) { texto(c, n, e); return; }
  band = etq_band[n->etq];
  if (band & EF_FUERA) return;
  if (!(n->est & EN_VISTO)) estilo(p, n);
  if (n->oculto) return;
  if (n->fg) e.fg = n->fg;
  if (n->bg) e.bg = n->bg;
  if (n->est & EN_NEGRITA) e.est |= A_NEGRITA;
  if (n->est & EN_SUBRAYA) e.est |= A_SUBRAYA;
  alin0 = c->alin;
  if ((band & EF_BLOQUE) && (n->est & (EN_CENTRO | EN_DERECHA))) { salta(c); c->alin = n->est & EN_CENTRO ? 1 : 2; }
  switch (n->etq) {
  case E_BR:
    if (c->col) salta(c);
    else { empieza(c); c->y++; c->vacio = 0; }
    return;
  case E_HR:
    raya(c, c->ancho);
    return;
  case E_IMG:
    t = atr(n, "alt");
    if (t && t[0]) {
      Est e2 = e;
      if (e.enlace < 0) e2.fg = K_GRIS;
      a = atr_de(c, e2);
      k = strlen(t);
      if (k > 60) k = 60;
      b[0] = '[';
      memcpy(b + 1, t, k);
      b[k + 1] = ']';
      for (x0 = 0; x0 < k + 2; x0++) if ((u8)b[x0] < ' ') b[x0] = ' ';
      pon(c, (const u8 *)b, k + 2, a);
    }
    return;
  case E_INPUT: case E_SELECT: case E_TEXTAREA: case E_BUTTON:
    control(c, n, e);
    return;
  case E_A:
    t = atr(n, "href");
    if (t) {
      e.enlace = (short)enlace_nuevo(c, t, n);
      e.fg = K_ENLACE;
      if (n->est & EN_BOTON) {
        e.est |= A_BOTON | A_NEGRITA;
        a = atr_de(c, e);
        literal(c, "[", a);
        hijos(c, n, e);
        c->espacio = 0;
        literal(c, "]", a);
        return;
      }
      e.est |= A_SUBRAYA;
    }
    hijos(c, n, e);
    return;
  case E_B: case E_STRONG:
    e.est |= A_NEGRITA;
    break;
  case E_U: case E_INS:
    e.est |= A_SUBRAYA;
    break;
  case E_S: case E_DEL:
    e.est |= A_TACHA;
    break;
  case E_MARK:
    e.bg = K_MARCA;
    break;
  case E_H1: case E_H2: case E_H3: case E_H4: case E_H5: case E_H6:
    margen(c);
    e.est |= A_NEGRITA;
    if (n->etq <= E_H3 && !n->fg) e.fg = K_TITULO;
    hijos(c, n, e);
    if (n->etq == E_H1 && c->col) { k = c->col; salta(c); c->alin = 0; raya(c, k); }
    margen(c);
    c->alin = alin0;
    return;
  case E_P: case E_BLOCKQUOTE: case E_FIELDSET: case E_FIGURE: case E_DL:
    margen(c);
    if (n->etq == E_BLOCKQUOTE && c->ancho > 12) {
      c->x0 += 4; c->ancho -= 4;
      hijos(c, n, e);
      salta(c);
      c->x0 -= 4; c->ancho += 4;
    } else hijos(c, n, e);
    margen(c);
    c->alin = alin0;
    return;
  case E_LEGEND:
    salta(c);
    e.est |= A_NEGRITA;
    hijos(c, n, e);
    salta(c);
    return;
  case E_UL: case E_OL:
    for (h = n->padre; h && h->etq != E_LI; h = h->padre) ;
    if (h) salta(c); else margen(c);
    if (c->ancho > 10) {
      int num = (t = atr(n, "start")) ? atoi(t) : 1;
      int sin = tiene_clase(n, "list-unstyled") || tiene_clase(n, "nav") || tiene_clase(n, "navbar-nav");
      int ind = sin ? 0 : n->etq == E_OL ? 4 : 2;
      c->x0 += ind; c->ancho -= ind;
      for (h = n->hijo; h; h = h->sig) {
        if (!h->texto && h->etq == E_LI && !sin) {
          if (!(h->est & EN_VISTO)) estilo(p, h);
          if (h->oculto) continue;
          salta(c);
          if (n->etq == E_OL) sprintf(c->pref, "%2d. ", num++ % 100);
          else strcpy(c->pref, "\267 ");
          caja(c, h, e);
          salta(c);
          c->pref[0] = 0;
        } else caja(c, h, e);
      }
      salta(c);
      c->x0 -= ind; c->ancho += ind;
    } else hijos(c, n, e);
    for (h = n->padre; h && h->etq != E_LI; h = h->padre) ;
    if (h) salta(c); else margen(c);
    c->alin = alin0;
    return;
  case E_DT:
    salta(c);
    e.est |= A_NEGRITA;
    hijos(c, n, e);
    salta(c);
    return;
  case E_DD:
    salta(c);
    if (c->ancho > 12) { c->x0 += 4; c->ancho -= 4; hijos(c, n, e); salta(c); c->x0 -= 4; c->ancho += 4; }
    else hijos(c, n, e);
    return;
  case E_PRE:
    margen(c);
    c->pre++;
    hijos(c, n, e);
    salta(c);
    c->pre--;
    margen(c);
    return;
  case E_TABLE:
    if (tiene_clase(n, "table")) margen(c);
    tabla(c, n, e);
    salta(c);
    c->alin = alin0;
    return;
  case E_DETAILS:
    for (h = n->hijo; h && (h->texto || h->etq != E_SUMMARY); h = h->sig) ;
    plegable(c, n, e, "Detalles", h);
    if (n->abierto && c->ancho > 10) {
      Nodo *q;
      c->x0 += 2; c->ancho -= 2;
      for (q = n->hijo; q; q = q->sig) if (q != h) caja(c, q, e);
      salta(c);
      c->x0 -= 2; c->ancho += 2;
    }
    return;
  case E_NAV: case E_ASIDE:
    if (n->est & EN_PLEGABLE) {
      t = atr(n, "aria-label");
      if (!t) for (h = n->hijo; h && !t; h = h->sig) if (!h->texto) t = atr(h, "aria-label");
      sprintf(b, "%.60s (%d enlaces)", t && t[0] ? t : "Men\372", cuenta_enlaces(n));
      plegable(c, n, e, b, NULL);
      if (n->abierto && c->ancho > 10) {
        c->x0 += 2; c->ancho -= 2;
        hijos(c, n, e);
        salta(c);
        c->x0 -= 2; c->ancho += 2;
      }
      margen(c);
      return;
    }
    break;
  case E_Q:
    literal(c, "\253", atr_de(c, e));
    hijos(c, n, e);
    c->espacio = 0;
    literal(c, "\273", atr_de(c, e));
    return;
  case E_OPTION: case E_OPTGROUP:
    return;
  }
  if (band & EF_BLOQUE) {
    salta(c);
    if (n->bg && c->ancho > 4 && !c->medir) {
      /* un bloque con fondo (un aviso): con su margen y el fondo en todo el ancho */
      int y0;
      margen(c);
      empieza(c);
      y0 = c->y;
      c->vacio = 1;
      c->x0 += 1; c->ancho -= 2;
      hijos(c, n, e);
      salta(c);
      c->x0 -= 1; c->ancho += 2;
      for (k = y0; k < c->y; k++) fila_fondo(p, k, c->x0, c->ancho, n->bg);
      c->vacio = 0;
      margen(c);
    } else {
      hijos(c, n, e);
      salta(c);
    }
    c->alin = alin0;
    return;
  }
  hijos(c, n, e);
  (void)x0; (void)an0;
}

/* =============================================================================
 *  La página
 * ============================================================================= */
static const u32 colores_fijos[K_N] = {
  0, PALETA(0), PALETA(9), PALETA(12), PALETA(7), PALETA(13), PALETA(11), PALETA(4), PALETA(8), PALETA(10), PALETA(6), PALETA(8)
};
Pagina *pag_nueva(const char *url, const u8 *fuente, u32 n, int latin1, int texto_plano) {
  Pagina *p = malloc(sizeof *p);
  int k;
  if (!p) return NULL;
  memset(p, 0, sizeof *p);
  if (texto_plano) {
    /* un texto sin más: como si fuera <pre> */
    Nodo *pre, *tx;
    char *b;
    p->doc.raiz = ar_pide(&p->doc.ar, sizeof(Nodo));
    pre = ar_pide(&p->doc.ar, sizeof(Nodo));
    tx = ar_pide(&p->doc.ar, sizeof(Nodo));
    b = ar_pide(&p->doc.ar, n + 1);
    if (!p->doc.raiz || !pre || !tx || !b) { ar_suelta(&p->doc.ar); free(p); return NULL; }
    memset(p->doc.raiz, 0, sizeof(Nodo)); memset(pre, 0, sizeof(Nodo)); memset(tx, 0, sizeof(Nodo));
    p->doc.raiz->etq = E_BODY; p->doc.raiz->hijo = p->doc.raiz->ult = pre;
    pre->etq = E_PRE; pre->padre = p->doc.raiz; pre->hijo = pre->ult = tx;
    tx->texto = 1; tx->padre = pre; tx->t = b;
    if (latin1) { memcpy(b, fuente, n); tx->n = n; } else tx->n = utf8_a_1252(fuente, n, b);
    p->doc.body = p->doc.raiz;
  } else doc_lee(&p->doc, fuente, n, latin1);
  p->url = ar_texto(&p->doc.ar, url, -1);
  for (k = 0; k < K_N; k++) p->color[k] = colores_fijos[k];
  p->n_color = K_N;
  p->reglas = ar_pide(&p->doc.ar, REGLAS_MAX * sizeof(Regla));
  if (!p->reglas || !p->doc.raiz) { ar_suelta(&p->doc.ar); free(p); return NULL; }
  ctl_todos(p, p->doc.raiz);
  return p;
}
static void estilo_fuera(Nodo *n) {
  Nodo *h;
  if (n->texto) return;
  n->est &= ~(EN_VISTO | EN_NEGRITA | EN_CENTRO | EN_DERECHA | EN_PLEGABLE | EN_BOTON | EN_SUBRAYA);
  n->fg = n->bg = 0;
  for (h = n->hijo; h; h = h->sig) estilo_fuera(h);
}
void pag_maqueta(Pagina *p, int columnas) {
  static Ctx c;
  Est e = { 0, K_TINTA, 0, -1 };
  int k;
  for (k = 0; k < p->n_filas; k++) { free(p->filas[k].c); free(p->filas[k].a); }
  if (p->filas) memset(p->filas, 0, p->cap_filas * sizeof(Fila));
  p->n_filas = 0; p->ancho = 0; p->n_atrs = 0; p->n_enl = 0; p->n_foco = 0;
  for (k = 0; k < p->n_ctl; k++) p->ctl[k].visible = 0;
  if (columnas < 20) columnas = 20;
  if (columnas > LMAX - 2) columnas = LMAX - 2;
  /* las reglas dependen del ancho (@media): se leen otra vez */
  p->n_reglas = 0;
  p->n_color = K_N;
  estilo_fuera(p->doc.raiz);
  if (p->doc.css) css_lee(p, p->doc.css, p->doc.css + p->doc.n_css, columnas * 8);
  atr_id(p, 0, K_TINTA, 0, -1, -1);            /* el atributo 0: el de por omisión */
  ctx_nuevo(&c, p, 1, columnas - 2, 0, 0);
  caja(&c, p->doc.body, e);
  salta(&c);
  fila(p, c.y, 0);
}
void pag_suelta(Pagina *p) {
  int k;
  if (!p) return;
  for (k = 0; k < p->n_filas; k++) { free(p->filas[k].c); free(p->filas[k].a); }
  for (k = 0; k < p->n_ctl; k++) free(p->ctl[k].valor);
  free(p->filas); free(p->atrs); free(p->enl); free(p->ctl); free(p->foco);
  ar_suelta(&p->doc.ar);
  free(p);
}
