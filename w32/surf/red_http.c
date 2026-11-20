/* =============================================================================
 *  red_http.c - el puente entre el Navegador completo (NAVEGA2.EXE, NetSurf)
 *  y el HTTP del Navegador propio (w32/navega/http.c: HTTP/1.1 y HTTPS con
 *  WSOCK32.DLL y HSPTLS.DLL, las galletas, la conexión que se guarda). Se
 *  compila con las cabeceras del Navegador (nav.h), no con las de NetSurf:
 *  red.c habla con él solo por estas funciones, con tipos sencillos.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include "../navega/nav.h"
#include "rh.h"

/* lo de html.c que usa http.c */
int min_c(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static const u16 alto1252[32] = {
  0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
  0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };
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
/* ¿de otro sitio? (SameSite): sin saber quién pide, como si no */
int nav_cruzada(const char *de, const char *a) { (void)de; (void)a; return 0; }

typedef struct { Pet p; char err[700]; } RH;

static char *copia(const char *t) {
  char *d;
  if (!t) return NULL;
  d = malloc(strlen(t) + 1);
  if (d) strcpy(d, t);
  return d;
}

void *rh_nueva(const char *url, int post, const char *cuerpo, unsigned n_cuerpo, const char *tipo,
               const char *cab, int sub, unsigned max) {
  RH *r = malloc(sizeof *r);
  if (!r) return NULL;
  memset(r, 0, sizeof *r);
  r->p.url = copia(url);
  r->p.post = post;
  if (post && n_cuerpo) {
    r->p.cuerpo = malloc(n_cuerpo);
    if (r->p.cuerpo) memcpy(r->p.cuerpo, cuerpo, n_cuerpo);
  }
  r->p.n_cuerpo = r->p.cuerpo ? n_cuerpo : 0;
  if (tipo) { int k; for (k = 0; tipo[k] && k < 94; k++) r->p.tipo_cuerpo[k] = tipo[k]; r->p.tipo_cuerpo[k] = 0; }
  r->p.sub = sub;
  r->p.max = max;
  if (!r->p.url) { free(r); return NULL; }
  http_empieza(&r->p);
  r->p.js_cab = copia(cab);
  return r;
}
int rh_avanza(void *v) {
  RH *r = v;
  int k = http_avanza(&r->p);
  if (k < 0 && !r->err[0]) a_utf8(r->p.error, strlen(r->p.error), r->err);
  return k;
}
int rh_con_cabeceras(void *v) { RH *r = v; return r->p.fase == H_CUERPO || r->p.fase == H_HECHO; }
int rh_estado(void *v) { return ((RH *)v)->p.estado; }
const char *rh_cabeceras(void *v) { RH *r = v; return r->p.cab ? r->p.cab : ""; }
const unsigned char *rh_datos(void *v, unsigned *n) { RH *r = v; *n = r->p.n_datos; return r->p.datos; }
const char *rh_error(void *v) { RH *r = v; return r->err[0] ? r->err : "Error de la red."; }
int rh_cifrada(void *v) { RH *r = v; return r->p.u.seguro; }
void rh_suelta(void *v) {
  RH *r = v;
  char *c = (char *)r->p.js_cab, *m = (char *)r->p.js_metodo;
  http_suelta(&r->p);
  free(c);
  free(m);
  free(r);
}
/* [JS] lo de fetch: otro método, y document.cookie */
void rh_metodo(void *v, const char *metodo) { RH *r = v; r->p.js_metodo = copia(metodo); }
char *rh_galletas_de(const char *url) { return http_galletas(url); }
void rh_pon_galleta(const char *url, const char *linea) { http_pon_galleta(url, linea); }
void rh_inicia(const char *idioma) {
  int k;
  for (k = 0; idioma && idioma[k] && k < 30; k++) http_idioma[k] = idioma[k];
  if (idioma) http_idioma[k] = 0;
  http_inicia();
}
void rh_fin(void) { http_fin(); }
void rh_galletas_carga(const char *ruta) { http_galletas_carga(ruta); }
int rh_galletas_guarda(const char *ruta) { return http_galletas_cambio ? http_galletas_guarda(ruta) : 1; }
