/* =============================================================================
 *  webletra.c - las letras de la web (@font-face) en el Navegador completo
 *  (NAVEGA2.EXE): los iconos de letra (Bootstrap Icons) y las letras de las
 *  páginas (Plus Jakarta Sans, Inter...). docs/486/NAVEGADOR2.md
 *   - webletra_hoja(): lee las reglas @font-face del texto de cada hoja de
 *     estilo (css.c de NetSurf, [HISPA]) y apunta qué familias se usan
 *     (font-family);
 *   - webletra_pide(): antes de hacer las cajas de una página (html.c,
 *     [HISPA]), pide las letras que se usan y no hay todavía; la página
 *     espera a que lleguen (como Chrome con font-display: block), así se
 *     maqueta una vez y con las medidas buenas;
 *   - al llegar: TTF, OTF, WOFF o WOFF2 (woff.c) a letra.c (letra_web()).
 *  Se prefieren TTF/OTF y WOFF a WOFF2 si la hoja da a elegir (en un 486,
 *  Brotli tarda más y gasta más memoria). Con unicode-range, solo las que
 *  tienen las letras de aquí (la «A»).
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "content/llcache.h"
#include "html/private.h"

#include "surf.h"
#include "woff.h"
#include "webletra.h"

#define CARAS 32
#define USADAS 96
#define NOMBRE 48

typedef struct {
  char familia[NOMBRE];       /* en minúsculas */
  int peso, cursiva;
  nsurl *url;
  int estado;                 /* 0 nueva, 1 pidiendo, 2 lista, 3 no se puede */
  llcache_handle *h;
  html_content *html;         /* la página que la espera (o NULL) */
} Cara;
static Cara cara[CARAS];
static int n_caras;
static char usada[USADAS][NOMBRE];
static int n_usadas;

/* ---- leer el CSS ---- */

static int min(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* copia un nombre de familia (sin comillas ni espacios de más, en minúsculas) */
static void nombre(const char *a, const char *b, char *d) {
  int n = 0;
  while (a < b && (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '"' || *a == '\'')) a++;
  while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r' || b[-1] == '"' || b[-1] == '\'')) b--;
  while (a < b && n < NOMBRE - 1) d[n++] = (char)min((unsigned char)*a++);
  d[n] = 0;
}

static bool es_usada(const char *f) {
  int k;
  for (k = 0; k < n_usadas; k++) if (!strcmp(usada[k], f)) return true;
  return false;
}

static void usa(const char *f) {
  if (!*f || es_usada(f) || n_usadas == USADAS) return;
  if (!strcmp(f, "inherit") || !strcmp(f, "sans-serif") || !strcmp(f, "serif") || !strcmp(f, "monospace") ||
      !strcmp(f, "system-ui") || !strcmp(f, "-apple-system") || !strcmp(f, "initial")) return;
  strcpy(usada[n_usadas++], f);
}

/* «font-family: a, "b", c» (y la familia del final de «font: ...»): las apunta */
static void familias_de(const char *a, const char *b, bool atajo) {
  const char *p = a;
  char f[NOMBRE];
  if (atajo) {
    /* «font: 600 1rem/1.5 "Inter", sans-serif»: desde la última cifra o «/» */
    const char *q = a, *ini = a;
    for (; q < b; q++) if ((*q >= '0' && *q <= '9') || *q == '/' || *q == '%') ini = q + 1;
    while (ini < b && *ini != ' ' && *ini != ',') ini++;
    p = ini;
  }
  while (p < b) {
    const char *e = p;
    while (e < b && *e != ',') e++;
    nombre(p, e, f);
    if (strlen(f) > 0 && !strstr(f, "!important")) usa(f);
    else if (strstr(f, "!important")) { *strstr(f, "!important") = 0; nombre(f, f + strlen(f), f); usa(f); }
    p = e + 1;
  }
}

/* el final de una declaración (el «;» de fuera de paréntesis y comillas) */
static const char *fin_decl(const char *p, const char *b) {
  int par = 0;
  char com = 0;
  for (; p < b; p++) {
    if (com) { if (*p == com) com = 0; continue; }
    if (*p == '"' || *p == '\'') com = *p;
    else if (*p == '(') par++;
    else if (*p == ')') { if (par) par--; }
    else if ((*p == ';' || *p == '}') && !par) return p;
  }
  return b;
}

static bool empieza(const char *p, const char *b, const char *t) {
  size_t n = strlen(t);
  return (size_t)(b - p) >= n && !strncasecmp(p, t, n);
}

/* ¿unicode-range tiene la «A»? */
static bool con_la_a(const char *a, const char *b) {
  while (a < b) {
    unsigned long x, y;
    char *e;
    while (a < b && (*a == ' ' || *a == ',' || *a == 'U' || *a == 'u' || *a == '+')) a++;
    if (a >= b) break;
    x = strtoul(a, &e, 16);
    if (e == a) { a++; continue; }
    y = x;
    if (*e == '?') { while (*e == '?') { x <<= 4; y = (y << 4) | 15; e++; } }
    else if (*e == '-') { const char *f = e + 1; y = strtoul(f, &e, 16); }
    if (x <= 0x41 && 0x41 <= y) return true;
    a = e;
  }
  return false;
}

/* una regla @font-face (lo de dentro de las llaves) */
static void cara_de(const char *a, const char *b, const char *base) {
  char fam[NOMBRE] = "", url[1024] = "";
  int peso = 400, cursiva = 0, mejor = 99, k;
  bool rango_ok = true;
  const char *p = a;
  while (p < b) {
    const char *e = fin_decl(p, b), *v;
    while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    v = p;
    while (v < e && *v != ':') v++;
    if (v < e) {
      v++;
      if (empieza(p, e, "font-family")) nombre(v, e, fam);
      else if (empieza(p, e, "font-weight")) {
        while (v < e && *v == ' ') v++;
        if (empieza(v, e, "bold")) peso = 700;
        else if (*v >= '1' && *v <= '9') peso = atoi(v);
      } else if (empieza(p, e, "font-style")) {
        while (v < e && *v == ' ') v++;
        cursiva = empieza(v, e, "italic") || empieza(v, e, "oblique");
      } else if (empieza(p, e, "unicode-range")) {
        rango_ok = con_la_a(v, e);
      } else if (empieza(p, e, "src")) {
        /* las fuentes, separadas por comas: url(...) format(...) */
        const char *q = v;
        while (q < e) {
          const char *f = q, *u0, *u1;
          int par = 0, pref;
          char com = 0;
          while (f < e) {
            if (com) { if (*f == com) com = 0; }
            else if (*f == '"' || *f == '\'') com = *f;
            else if (*f == '(') par++;
            else if (*f == ')') par--;
            else if (*f == ',' && !par) break;
            f++;
          }
          u0 = q;
          while (u0 < f && !empieza(u0, f, "url(")) u0++;
          if (u0 < f) {
            const char *fm;
            u0 += 4;
            u1 = u0;
            while (u1 < f && *u1 != ')') u1++;
            fm = u1;
            while (u0 < u1 && (*u0 == ' ' || *u0 == '"' || *u0 == '\'')) u0++;
            {
              const char *u2 = u1;
              while (u2 > u0 && (u2[-1] == ' ' || u2[-1] == '"' || u2[-1] == '\'')) u2--;
              u1 = u2;
            }
            /* la preferida: TTF/OTF, WOFF, WOFF2 (por el formato o por el nombre) */
            pref = 5;
            while (fm < f && !empieza(fm, f, "format(")) fm++;
            if (fm < f) {
              if (empieza(fm + 7, f, "\"woff2") || empieza(fm + 7, f, "'woff2") || empieza(fm + 7, f, "woff2")) pref = 3;
              else if (empieza(fm + 7, f, "\"woff") || empieza(fm + 7, f, "'woff") || empieza(fm + 7, f, "woff")) pref = 2;
              else if (strstr(fm, "truetype") && strstr(fm, "truetype") < f) pref = 1;
              else if (strstr(fm, "opentype") && strstr(fm, "opentype") < f) pref = 1;
              else if (strstr(fm, "embedded") && strstr(fm, "embedded") < f) pref = 9;
              else if (strstr(fm, "svg") && strstr(fm, "svg") < f) pref = 9;
            } else {
              size_t l = (size_t)(u1 - u0);
              const char *qq = u0;
              while (qq < u1 && *qq != '?' && *qq != '#') qq++;
              l = (size_t)(qq - u0);
              if (l > 6 && !strncasecmp(qq - 6, ".woff2", 6)) pref = 3;
              else if (l > 5 && !strncasecmp(qq - 5, ".woff", 5)) pref = 2;
              else if (l > 4 && (!strncasecmp(qq - 4, ".ttf", 4) || !strncasecmp(qq - 4, ".otf", 4))) pref = 1;
              else if (l > 4 && (!strncasecmp(qq - 4, ".eot", 4) || !strncasecmp(qq - 4, ".svg", 4))) pref = 9;
            }
            if (pref < mejor && u1 > u0 && (size_t)(u1 - u0) < sizeof url) {
              mejor = pref;
              memcpy(url, u0, u1 - u0);
              url[u1 - u0] = 0;
            }
          }
          q = f + 1;
        }
      }
    }
    p = e + 1;
  }
  if (!fam[0] || !url[0] || mejor >= 9 || !rango_ok) return;
  {
    nsurl *b0 = NULL, *u = NULL;
    if (nsurl_create(base, &b0) != NSERROR_OK) return;
    if (nsurl_join(b0, url, &u) != NSERROR_OK) { nsurl_unref(b0); return; }
    nsurl_unref(b0);
    for (k = 0; k < n_caras; k++)
      if (!strcmp(cara[k].familia, fam) && cara[k].peso == peso && cara[k].cursiva == cursiva) {
        nsurl_unref(u);
        return;
      }
    if (n_caras == CARAS) { nsurl_unref(u); return; }
    memset(&cara[n_caras], 0, sizeof cara[0]);
    strcpy(cara[n_caras].familia, fam);
    cara[n_caras].peso = peso;
    cara[n_caras].cursiva = cursiva;
    cara[n_caras].url = u;
    surf_reg("letra de la web: %s %d%s: %s", fam, peso, cursiva ? " cursiva" : "", nsurl_access(u));
    n_caras++;
  }
}

void webletra_hoja(const char *css, size_t n, const char *base) {
  const char *p = css, *b = css + n;
  if (!css || !base) return;
  while (p < b) {
    if (*p == '/' && p + 1 < b && p[1] == '*') {      /* comentario */
      const char *e = p + 2;
      while (e + 1 < b && !(e[0] == '*' && e[1] == '/')) e++;
      p = e + 2;
      continue;
    }
    if (*p == '@' && empieza(p, b, "@font-face")) {
      const char *a = p + 10, *e;
      while (a < b && *a != '{') a++;
      e = a;
      while (e < b && *e != '}') e++;
      if (a < b) cara_de(a + 1, e, base);
      p = e + 1;
      continue;
    }
    if ((*p == 'f' || *p == 'F') && (p == css || p[-1] == '{' || p[-1] == ';' || p[-1] == ' ' || p[-1] == '\n' ||
                                     p[-1] == '\t' || p[-1] == '"')) {
      const char *v = NULL;
      bool atajo = false;
      if (empieza(p, b, "font-family")) v = p + 11;
      else if (empieza(p, b, "font") && p + 4 < b && (p[4] == ':' || p[4] == ' ')) { v = p + 4; atajo = true; }
      if (v) {
        while (v < b && *v == ' ') v++;
        if (v < b && *v == ':') {
          const char *e = fin_decl(v + 1, b);
          familias_de(v + 1, e, atajo);
          p = e;
          continue;
        }
      }
    }
    p++;
  }
}

/* ---- pedirlas ---- */

static void suelta(void *h) { llcache_handle_release((llcache_handle *)h); }

static nserror llega(llcache_handle *h, const llcache_event *ev, void *pw) {
  Cara *c = pw;
  html_content *html;
  if (ev->type != LLCACHE_EVENT_DONE && ev->type != LLCACHE_EVENT_ERROR) return NSERROR_OK;
  if (ev->type == LLCACHE_EVENT_DONE) {
    size_t n = 0, m = 0;
    const uint8_t *d = llcache_handle_get_source_data(h, &n);
    unsigned char *t = d && n ? woff_a_sfnt(d, n, &m) : NULL;
    int k;
    c->estado = 3;
    if (t) {
      /* (las demás caras con la misma dirección, el mismo archivo) */
      if (letra_web(c->familia, c->peso, c->cursiva, t, m)) {
        c->estado = 2;
        for (k = 0; k < n_caras; k++)
          if (&cara[k] != c && cara[k].estado == 0 && nsurl_compare(cara[k].url, c->url, NSURL_COMPLETE)) {
            unsigned char *t2 = malloc(m);
            if (t2) { memcpy(t2, t, m); if (letra_web(cara[k].familia, cara[k].peso, cara[k].cursiva, t2, m)) cara[k].estado = 2; else free(t2); }
          }
      } else free(t);
    }
    surf_reg("letra de la web %s %d: %s (%u octetos)", c->familia, c->peso, c->estado == 2 ? "lista" : "no se puede",
             (unsigned)n);
  } else {
    c->estado = 3;
    surf_reg("letra de la web %s: error", c->familia);
  }
  c->h = NULL;
  framebuffer_schedule(0, suelta, h);
  html = c->html;
  c->html = NULL;
  if (html) {
    html->base.active--;
    if (html_can_begin_conversion(html)) html_begin_conversion(html);
  }
  return NSERROR_OK;
}

bool webletra_pide(struct html_content *html) {
  int k, j;
  bool espera = false;
  for (k = 0; k < n_caras; k++) {
    Cara *c = &cara[k];
    if (c->estado == 1 && c->html == html) { espera = true; continue; }
    if (c->estado != 0 || !es_usada(c->familia)) continue;
    /* ¿otra con la misma dirección ya pedida? (al llegar se copia) */
    for (j = 0; j < n_caras; j++)
      if (j != k && cara[j].estado == 1 && nsurl_compare(cara[j].url, c->url, NSURL_COMPLETE)) break;
    if (j < n_caras) continue;
    if (llcache_handle_retrieve(c->url, 0, NULL, NULL, llega, c, &c->h) != NSERROR_OK) { c->estado = 3; continue; }
    c->estado = 1;
    c->html = html;
    html->base.active++;
    espera = true;
  }
  return espera;
}

void webletra_olvida(struct html_content *html) {
  int k;
  for (k = 0; k < n_caras; k++) if (cara[k].html == html) cara[k].html = NULL;
}
