/* =============================================================================
 *  red.c - http: y https: para NetSurf en Hispa-486 (NAVEGA2.EXE): un
 *  «fetcher» de NetSurf (content/fetchers.h) sobre el HTTP del Navegador
 *  propio (red_http.c -> w32/navega/http.c, con WSOCK32.DLL y HSPTLS.DLL),
 *  en vez de libcurl y OpenSSL. Una petición cada vez, por la conexión que se
 *  guarda abierta (el apretón de manos de TLS es lo caro en un 486); las
 *  demás esperan en la cola. docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils/errors.h"
#include "utils/nsurl.h"
#include "utils/messages.h"
#include "utils/corestrings.h"
#include "content/fetch.h"
#include "content/fetchers.h"

#include "surf.h"
#include "rh.h"

typedef struct Pedido {
  struct Pedido *sig;
  struct fetch *padre;
  nsurl *url;
  char *cuerpo; unsigned n_cuerpo;
  char tipo[96];
  char *cab;
  int post, solo_2xx;
  void *r;                  /* la petición de http.c, cuando le toca */
  unsigned enviado;         /* lo del cuerpo que ya ha ido a NetSurf */
  int con_cab;              /* ya se le han dado las cabeceras */
  int abortado, en_uso;
  int de_golpe;             /* HTML: el cuerpo entero de una vez (ver cabeceras) */
  red_js_cb js;             /* [JS] una petición de fetch o XMLHttpRequest (sin padre de NetSurf) */
  void *js_ctx;
  char *metodo;
} Pedido;

static Pedido *cola, *activo;
int surf_cifrada;

int red_ocupada(void) { return activo != NULL || cola != NULL; }

static bool r_inicia(lwc_string *e) { (void)e; return true; }
static void r_termina(lwc_string *e) { (void)e; }
static bool r_vale(const nsurl *u) { (void)u; return true; }

/* el cuerpo de multipart/form-data (RFC 7578): los campos y los archivos */
static char *multiparte(const struct fetch_multipart_data *m, unsigned *n, char *tipo) {
  static const char frontera[] = "----HispaSurf7d3f19a2b4c";
  size_t cap = 4096, l = 0;
  char *b = malloc(cap);
  for (; m && b; m = m->next) {
    size_t need;
    char *dato = NULL;
    long tam = 0;
    const char *nombre = "";
    if (m->file && m->rawfile) {
      FILE *f = fopen(m->rawfile, "rb");
      const char *p = m->value;
      if (f) {
        fseek(f, 0, SEEK_END); tam = ftell(f); fseek(f, 0, SEEK_SET);
        if (tam > 0 && tam < (2L << 20) && (dato = malloc(tam))) { if (fread(dato, 1, tam, f) != (size_t)tam) tam = 0; }
        else tam = 0;
        fclose(f);
      }
      nombre = p ? p : "";
      { const char *q = strrchr(nombre, '\\'); if (q) nombre = q + 1; }
    } else {
      tam = m->value ? strlen(m->value) : 0;
    }
    need = l + 400 + strlen(m->name) + strlen(nombre) + tam;
    if (need > cap) { char *q; while (cap < need) cap *= 2; q = realloc(b, cap); if (!q) { free(b); b = NULL; free(dato); break; } b = q; }
    if (m->file)
      l += sprintf(b + l, "--%s\r\nContent-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
                   "Content-Type: application/octet-stream\r\n\r\n", frontera, m->name, nombre);
    else
      l += sprintf(b + l, "--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n", frontera, m->name);
    if (tam) memcpy(b + l, m->file ? dato : m->value, tam);
    l += tam;
    memcpy(b + l, "\r\n", 2); l += 2;
    free(dato);
  }
  if (!b) return NULL;
  if (l + 64 > cap) { char *q = realloc(b, l + 64); if (!q) { free(b); return NULL; } b = q; }
  l += sprintf(b + l, "--%s--\r\n", frontera);
  sprintf(tipo, "multipart/form-data; boundary=%s", frontera);
  *n = l;
  return b;
}

static void *r_prepara(struct fetch *padre, nsurl *url, bool solo_2xx, bool downgrade_tls,
                       const char *post_urlenc, const struct fetch_multipart_data *post_multipart,
                       const char **headers) {
  Pedido *p = calloc(1, sizeof *p);
  size_t lc = 0;
  int k;
  (void)downgrade_tls;
  if (!p) return NULL;
  p->padre = padre;
  p->url = nsurl_ref(url);
  p->solo_2xx = solo_2xx;
  if (post_urlenc) {
    p->post = 1;
    p->cuerpo = strdup(post_urlenc);
    p->n_cuerpo = p->cuerpo ? strlen(p->cuerpo) : 0;
  } else if (post_multipart) {
    p->post = 1;
    p->cuerpo = multiparte(post_multipart, &p->n_cuerpo, p->tipo);
  }
  /* las cabeceras de NetSurf (If-None-Match...) y un Accept para todo */
  for (k = 0; headers && headers[k]; k++) lc += strlen(headers[k]) + 2;
  p->cab = malloc(lc + 120);
  if (p->cab) {
    char *c = p->cab;
    c += sprintf(c, "Accept: text/html,application/xhtml+xml,text/css,image/png,image/jpeg,image/gif,*/*;q=0.5\r\n");
    for (k = 0; headers && headers[k]; k++) c += sprintf(c, "%s\r\n", headers[k]);
  }
  return p;
}

static bool r_empieza(void *v) {
  Pedido *p = v, **q = &cola;
  while (*q) q = &(*q)->sig;
  *q = p;
  p->en_uso = 1;
  return true;
}

static void quita_de_cola(Pedido *p) {
  Pedido **q;
  for (q = &cola; *q; q = &(*q)->sig) if (*q == p) { *q = p->sig; break; }
  p->sig = NULL;
}

/* NetSurf lo deja: solo se marca (puede llegar desde dentro de un aviso
 * nuestro); r_sondea lo suelta luego */
static void r_aborta(void *v) { ((Pedido *)v)->abortado = 1; }

static void r_libera(void *v) {
  Pedido *p = v;
  if (p == activo) activo = NULL;
  quita_de_cola(p);
  if (p->r) rh_suelta(p->r);
  if (p->url) nsurl_unref(p->url);
  free(p->cuerpo);
  free(p->cab);
  free(p->metodo);
  free(p);
}

/* (a una de JavaScript no se le manda nada de NetSurf: solo el final) */
static void manda(Pedido *p, fetch_msg *m) { if (!p->js) fetch_send_callback(m, p->padre); }

/* da por acabado el pedido activo (NetSurf llama a r_libera) */
static void acaba(Pedido *p) {
  activo = NULL;
  if (p->r) { rh_suelta(p->r); p->r = NULL; }
  if (p->js) { r_libera(p); return; }
  fetch_remove_from_queues(p->padre);
  fetch_free(p->padre);
}

/* las cabeceras, a NetSurf: el código y una a una; true si ya no sigue */
static bool cabeceras(Pedido *p) {
  const char *c = rh_cabeceras(p->r), *l, *loc = NULL;
  int est = rh_estado(p->r);
  fetch_msg m;
  char linea[2048];
  fetch_set_http_code(p->padre, est);
  surf_cifrada = rh_cifrada(p->r);
  /* la primera línea es la del estado: se salta */
  l = strchr(c + 1, '\n');
  while (l && *++l) {
    const char *e = l;
    size_t n;
    while (*e && *e != '\r' && *e != '\n') e++;
    n = e - l;
    if (!n) break;
    if (n >= sizeof linea) n = sizeof linea - 1;
    memcpy(linea, l, n); linea[n] = 0;
    if (!strncasecmp(linea, "Location:", 9)) { loc = l + 9; while (*loc == ' ') loc++; }
    /* una página HTML se le da a NetSurf entera, no a trozos: con los trozos
     * de la red, un <style> partido entre dos se perdía (la hoja no se leía) */
    if (!strncasecmp(linea, "Content-Type:", 13) && strstr(linea, "html")) p->de_golpe = 1;
    m.type = FETCH_HEADER;
    m.data.header_or_data.buf = (const uint8_t *)linea;
    m.data.header_or_data.len = n;
    manda(p, &m);
    if (p->abortado) return true;
    l = strchr(e, '\n');
  }
  if (est == 304 && !p->post) { m.type = FETCH_NOTMODIFIED; manda(p, &m); return true; }
  if (est >= 300 && est < 400 && loc) {
    char d[2048];
    size_t n = 0;
    while (loc[n] && loc[n] != '\r' && loc[n] != '\n' && n < sizeof d - 1) { d[n] = loc[n]; n++; }
    d[n] = 0;
    m.type = FETCH_REDIRECT;
    m.data.redirect = d;
    manda(p, &m);
    return true;
  }
  if (est == 401) { m.type = FETCH_AUTH; m.data.auth.realm = ""; manda(p, &m); return true; }
  if (p->solo_2xx && (est < 200 || est > 299)) {
    m.type = FETCH_ERROR;
    m.data.error = messages_get("Not2xx");
    manda(p, &m);
    return true;
  }
  return false;
}

static int dentro;
static void r_sondea2(void);
static void r_sondea(lwc_string *e) {
  (void)e;
  if (dentro) return;
  dentro = 1;
  r_sondea2();
  dentro = 0;
}
static void r_sondea2(void) {
  Pedido *p, *q;
  int k, vueltas;
  /* los de la cola que NetSurf ya no quiere */
  for (p = cola; p; p = q) {
    q = p->sig;
    if (p->abortado && p->js) { r_libera(p); continue; }
    if (p->abortado) { quita_de_cola(p); fetch_remove_from_queues(p->padre); fetch_free(p->padre); }
  }
  if (activo && activo->abortado) acaba(activo);
  if (!activo && cola) {                /* le toca al primero de la cola */
    char *u;
    p = cola;
    cola = p->sig;
    p->sig = NULL;
    activo = p;
    u = strdup(nsurl_access(p->url));
    if (u) { char *h = strchr(u, '#'); if (h) *h = 0; }
    surf_reg("pide %s", u ? u : "?");
    p->r = u ? rh_nueva(u, p->post, p->cuerpo, p->n_cuerpo, p->tipo[0] ? p->tipo : NULL, p->cab, p->js != NULL, 8u << 20) : NULL;
    if (p->r && p->metodo) rh_metodo(p->r, p->metodo);
    free(u);
    if (!p->r) {
      fetch_msg m;
      m.type = FETCH_ERROR;
      m.data.error = "No hay memoria.";
      manda(p, &m);
      acaba(p);
      return;
    }
    {
      fetch_msg m;
      m.type = FETCH_PROGRESS;
      m.data.progress = "Pidiendo...";
      manda(p, &m);
      if (p->abortado) { acaba(p); return; }
    }
  }
  p = activo;
  if (!p) return;
  for (vueltas = 0; vueltas < 4 && activo == p; vueltas++) {
    k = rh_avanza(p->r);
    if (p->js) {                        /* [JS] entera, y luego a quien la pidió */
      if (k == 0) continue;
      if (k > 0) {
        unsigned n;
        const unsigned char *d = rh_datos(p->r, &n);
        const char *c = rh_cabeceras(p->r);
        p->js(p->js_ctx, rh_estado(p->r), c, d, n, NULL);
      } else p->js(p->js_ctx, 0, "", NULL, 0, rh_error(p->r));
      if (activo == p) acaba(p);
      return;
    }
    if (k < 0) {
      fetch_msg m;
      m.type = FETCH_ERROR;
      m.data.error = rh_error(p->r);
      manda(p, &m);
      if (activo == p) acaba(p);
      return;
    }
    if (!p->con_cab && rh_con_cabeceras(p->r)) {
      p->con_cab = 1;
      if (cabeceras(p)) { if (activo == p) acaba(p); return; }
    }
    if (p->con_cab) {
      unsigned n;
      const unsigned char *d = rh_datos(p->r, &n);
      if (n > p->enviado && (!p->de_golpe || k == 1)) {
        fetch_msg m;
        m.type = FETCH_DATA;
        m.data.header_or_data.buf = d + p->enviado;
        m.data.header_or_data.len = n - p->enviado;
        p->enviado = n;
        manda(p, &m);
        if (p->abortado) { acaba(p); return; }
      }
    }
    if (k == 1) {
      fetch_msg m;
      m.type = FETCH_FINISHED;
      manda(p, &m);
      if (activo == p) acaba(p);
      return;
    }
  }
}

/* [JS] fetch y XMLHttpRequest: a la misma cola que lo de NetSurf (una
 * petición cada vez, por la misma conexión); se sondea con el planificador
 * mientras haya alguna */
static int js_sondeando;
static void js_sondea(void *x) {
  Pedido *p;
  int hay = 0;
  (void)x;
  r_sondea(NULL);
  for (p = cola; p; p = p->sig) if (p->js) hay = 1;
  if (activo && activo->js) hay = 1;
  if (hay) framebuffer_schedule(10, js_sondea, NULL);
  else js_sondeando = 0;
}
int red_js_pide(const char *url, const char *metodo, const char *cuerpo, unsigned n_cuerpo, const char *cab,
                red_js_cb cb, void *ctx) {
  Pedido *p = calloc(1, sizeof *p), **q;
  if (!p) return 0;
  if (nsurl_create(url, &p->url) != NSERROR_OK) { free(p); return 0; }
  p->js = cb; p->js_ctx = ctx;
  if (metodo && strcmp(metodo, "GET") && strcmp(metodo, "POST")) p->metodo = strdup(metodo);
  if (cuerpo) {
    p->post = 1;
    p->cuerpo = malloc(n_cuerpo + 1);
    if (p->cuerpo) { memcpy(p->cuerpo, cuerpo, n_cuerpo); p->n_cuerpo = n_cuerpo; }
  }
  p->cab = strdup(cab ? cab : "");
  for (q = &cola; *q; q = &(*q)->sig) ;
  *q = p;
  p->en_uso = 1;
  if (!js_sondeando) { js_sondeando = 1; framebuffer_schedule(0, js_sondea, NULL); }
  return 1;
}
/* las de quien se va (la página): sin avisarle */
void red_js_cancela(void *ctx) {
  Pedido *p;
  for (p = cola; p; p = p->sig) if (p->js && p->js_ctx == ctx) p->abortado = 1;
  if (activo && activo->js && activo->js_ctx == ctx) { activo->abortado = 1; }
}

static int r_fdset(lwc_string *e, fd_set *a, fd_set *b, fd_set *c) { (void)e; (void)a; (void)b; (void)c; return -1; }

nserror fetch_hispa_register(void) {
  const struct fetcher_operation_table ops = {
    .initialise = r_inicia, .acceptable = r_vale, .setup = r_prepara, .start = r_empieza,
    .abort = r_aborta, .free = r_libera, .poll = r_sondea, .fdset = r_fdset, .finalise = r_termina,
  };
  lwc_string *s;
  nserror r;
  if (lwc_intern_string("http", 4, &s) != lwc_error_ok) return NSERROR_NOMEM;
  r = fetcher_add(s, &ops);
  if (r != NSERROR_OK) return r;
  if (lwc_intern_string("https", 5, &s) != lwc_error_ok) return NSERROR_NOMEM;
  return fetcher_add(s, &ops);
}

void red_fin(void) { rh_fin(); }
