/* =============================================================================
 *  anfitrion_rh.c - la red del Navegador completo en el anfitrión (anfitrion.c):
 *  las funciones de rh.h con el programa curl (de una vez: rh_nueva pide y
 *  rh_avanza dice que ya está). Las galletas, en un archivo de curl
 *  (SURF_GALLETAS, o /tmp/surfppm.galletas) que se guarda entre una vez y
 *  otra: así se puede entrar en la demo de ClicGest y ver otra página después.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rh.h"

typedef struct { char *cab; unsigned char *datos; unsigned n; int estado, cifrada; char err[200]; } RH;

static const char *galletas(void) { return getenv("SURF_GALLETAS") ? getenv("SURF_GALLETAS") : "/tmp/surfppm.galletas"; }

static char *lee_todo(const char *n, unsigned *tam) {
  FILE *f = fopen(n, "rb");
  char *d;
  long l;
  if (!f) { *tam = 0; return NULL; }
  fseek(f, 0, SEEK_END); l = ftell(f); fseek(f, 0, SEEK_SET);
  d = malloc(l + 1);
  if (d && fread(d, 1, l, f) != (size_t)l) l = 0;
  if (d) d[l] = 0;
  fclose(f);
  *tam = (unsigned)l;
  return d;
}

/* entre comillas simples para la orden */
static void comillas(char *d, size_t max, const char *t) {
  size_t n = 0;
  d[n++] = '\'';
  for (; *t && n + 6 < max; t++) {
    if (*t == '\'') { memcpy(d + n, "'\\''", 4); n += 4; }
    else d[n++] = *t;
  }
  d[n++] = '\'';
  d[n] = 0;
}

void *rh_nueva(const char *url, int post, const char *cuerpo, unsigned n_cuerpo, const char *tipo,
               const char *cab, int sub, unsigned max) {
  RH *r = calloc(1, sizeof *r);
  char orden[6000], u[2100], fcab[64], fdat[64], fpost[64], extra[2400] = "";
  unsigned n;
  char *c;
  (void)sub; (void)max;
  if (!r) return NULL;
  snprintf(fcab, sizeof fcab, "/tmp/surfppm.%d.cab", (int)getpid());
  snprintf(fdat, sizeof fdat, "/tmp/surfppm.%d.dat", (int)getpid());
  snprintf(fpost, sizeof fpost, "/tmp/surfppm.%d.post", (int)getpid());
  comillas(u, sizeof u, url);
  if (post) {
    FILE *f = fopen(fpost, "wb");
    char t[300];
    if (f) { if (n_cuerpo) fwrite(cuerpo, 1, n_cuerpo, f); fclose(f); }
    comillas(t, sizeof t, tipo && *tipo ? tipo : "application/x-www-form-urlencoded");
    snprintf(extra, sizeof extra, "--data-binary @%s -H \"Content-Type: \"%s", fpost, t);
  }
  if (cab && *cab) {
    /* «Nombre: valor\r\n»...: cada una con -H */
    const char *p = cab;
    while (*p) {
      const char *e = strstr(p, "\r\n");
      char h[600], hq[700];
      size_t l = e ? (size_t)(e - p) : strlen(p);
      if (l > 0 && l < sizeof h) {
        memcpy(h, p, l); h[l] = 0;
        comillas(hq, sizeof hq, h);
        strncat(extra, " -H ", sizeof extra - strlen(extra) - 1);
        strncat(extra, hq, sizeof extra - strlen(extra) - 1);
      }
      if (!e) break;
      p = e + 2;
    }
  }
  snprintf(orden, sizeof orden,
           "curl -s --compressed -m 30 -A 'HispaSurf/0.1 (NetSurf 3.11; Hispa-DOS 486)' -H 'Accept-Language: es' "
           "-b %s -c %s -D %s -o %s %s %s", galletas(), galletas(), fcab, fdat, extra, u);
  if (system(orden) != 0) { snprintf(r->err, sizeof r->err, "curl no ha podido pedir %.150s", url); }
  c = lee_todo(fcab, &n);
  /* con 100 Continue y demás: la última respuesta */
  if (c) {
    char *ult = c, *p = c;
    while ((p = strstr(p, "\r\n\r\nHTTP/")) != NULL) { ult = p + 4; p += 4; }
    r->cab = malloc(strlen(ult) + 2);
    if (r->cab) { r->cab[0] = '\n'; strcpy(r->cab + 1, ult); }
    sscanf(ult, "HTTP/%*s %d", &r->estado);
    free(c);
  }
  r->datos = (unsigned char *)lee_todo(fdat, &r->n);
  unlink(fcab); unlink(fdat); unlink(fpost);
  r->cifrada = !strncmp(url, "https:", 6);
  return r;
}
int rh_avanza(void *v) { RH *r = v; return r->cab ? 1 : -1; }
int rh_con_cabeceras(void *v) { return ((RH *)v)->cab != NULL; }
int rh_estado(void *v) { return ((RH *)v)->estado; }
const char *rh_cabeceras(void *v) { RH *r = v; return r->cab ? r->cab : ""; }
const unsigned char *rh_datos(void *v, unsigned *n) { RH *r = v; *n = r->n; return r->datos; }
const char *rh_error(void *v) { RH *r = v; return r->err[0] ? r->err : "Error de la red."; }
int rh_cifrada(void *v) { return ((RH *)v)->cifrada; }
void rh_suelta(void *v) { RH *r = v; free(r->cab); free(r->datos); free(r); }
void rh_inicia(const char *idioma) { (void)idioma; }
void rh_fin(void) { }
void rh_galletas_carga(const char *ruta) { (void)ruta; }
int rh_galletas_guarda(const char *ruta) { (void)ruta; return 1; }

/* [JS] lo de fetch y document.cookie (guion.c): en el anfitrión no hay JavaScript */
void rh_metodo(void *r, const char *metodo) { (void)r; (void)metodo; }
char *rh_galletas_de(const char *url) { (void)url; return NULL; }
void rh_pon_galleta(const char *url, const char *linea) { (void)url; (void)linea; }
