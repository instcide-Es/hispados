/* =============================================================================
 *  archivo.c - las rutas de Hispa-DOS para NetSurf (NAVEGA2.EXE): C:\CARPETA\
 *  PAGINA.HTM <-> file:///C:/CARPETA/PAGINA.HTM, el tipo de un archivo por su
 *  extensión y dónde están los recursos del Navegador (las hojas de estilo de
 *  NetSurf, sus textos) junto al programa. docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "utils/errors.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "utils/url.h"
#include "utils/string.h"
#include "utils/corestrings.h"
#include "netsurf/fetch.h"

#include "surf.h"

char surf_carpeta[260];

static nserror a_mkpath(char **str, size_t *size, size_t nelm, va_list ap) {
  return vsnstrjoin(str, size, '\\', nelm, ap);
}

static nserror a_basename(const char *path, char **str, size_t *size) {
  const char *h;
  if (!path) return NSERROR_BAD_PARAMETER;
  h = path + strlen(path);
  while (h > path && h[-1] != '\\' && h[-1] != '/' && h[-1] != ':') h--;
  *str = strdup(h);
  if (!*str) return NSERROR_NOMEM;
  if (size) *size = strlen(*str);
  return NSERROR_OK;
}

static nserror a_nsurl_a_ruta(struct nsurl *url, char **sal) {
  lwc_string *esq, *ruta;
  bool es;
  char *p, *q;
  nserror r;
  if (!url || !sal) return NSERROR_BAD_PARAMETER;
  esq = nsurl_get_component(url, NSURL_SCHEME);
  if (lwc_string_caseless_isequal(esq, corestring_lwc_file, &es) != lwc_error_ok) return NSERROR_BAD_PARAMETER;
  lwc_string_unref(esq);
  if (!es) return NSERROR_BAD_PARAMETER;
  ruta = nsurl_get_component(url, NSURL_PATH);
  if (!ruta) return NSERROR_BAD_PARAMETER;
  r = url_unescape(lwc_string_data(ruta), lwc_string_length(ruta), NULL, &p);
  lwc_string_unref(ruta);
  if (r != NSERROR_OK) return r;
  /* «/C:/A/B» -> «C:\A\B»; «/A/B» (sin unidad) -> «\A\B» */
  q = p;
  if (q[0] == '/' && q[1] && q[2] == ':') q++;
  memmove(p, q, strlen(q) + 1);
  for (q = p; *q; q++) if (*q == '/') *q = '\\';
  *sal = p;
  return NSERROR_OK;
}

static nserror a_ruta_a_nsurl(const char *path, struct nsurl **url) {
  char *b, *e, *u, *q;
  size_t l;
  nserror r;
  if (!path || !url || !*path) return NSERROR_BAD_PARAMETER;
  l = strlen(path);
  b = malloc(l + 2);
  if (!b) return NSERROR_NOMEM;
  strcpy(b, path);
  for (q = b; *q; q++) if (*q == '\\') *q = '/';
  r = url_escape(b, false, "/:", &e);
  free(b);
  if (r != NSERROR_OK) return r;
  q = e;
  while (*q == '/') q++;
  u = malloc(strlen(q) + 10);
  if (!u) { free(e); return NSERROR_NOMEM; }
  sprintf(u, "file:///%s", q);
  free(e);
  r = nsurl_create(u, url);
  free(u);
  return r;
}

static nserror a_mkdir_todo(const char *fname) {
  char *d = strdup(fname), *s;
  struct stat sb;
  if (!d) return NSERROR_NOMEM;
  s = strrchr(d, '\\');
  if (!s) { free(d); return NSERROR_OK; }
  *s = 0;
  if (stat(d, &sb) == 0) { free(d); return S_ISDIR(sb.st_mode) ? NSERROR_OK : NSERROR_NOT_DIRECTORY; }
  *s = '\\';
  s = d;
  if (s[0] && s[1] == ':') s += 2;
  while (*s == '\\') s++;
  while ((s = strchr(s, '\\')) != NULL) {
    *s = 0;
    if (stat(d, &sb) != 0 && mkdir(d, 0700) != 0) { free(d); return NSERROR_NOT_FOUND; }
    *s = '\\';
    while (*s == '\\') s++;
  }
  free(d);
  return NSERROR_OK;
}

static struct gui_file_table tabla_archivos = {
  .mkpath = a_mkpath, .basename = a_basename, .nsurl_to_path = a_nsurl_a_ruta,
  .path_to_nsurl = a_ruta_a_nsurl, .mkdir_all = a_mkdir_todo,
};
struct gui_file_table *surf_tabla_archivos = &tabla_archivos;

/* los recursos de NetSurf (default.css...) van en la carpeta RECURSOS junto
 * al programa; los nombres largos, en 8+3 */
char *surf_recurso(const char *n) {
  static const char *const largo[][2] = { { "welcome.html", "inicio.htm" }, { "credits.html", "creditos.htm" },
                                          { "licence.html", "licencia.htm" }, { "maps.html", "mapas.htm" } };
  char b[300];
  struct stat sb;
  unsigned k;
  for (k = 0; k < sizeof largo / sizeof largo[0]; k++) if (!strcmp(n, largo[k][0])) n = largo[k][1];
  snprintf(b, sizeof b, "%s\\RECURSOS\\%s", surf_carpeta, n);
  if (stat(b, &sb) != 0) return NULL;
  return strdup(b);
}

static struct nsurl *p_recurso(const char *path) {
  char *r;
  struct nsurl *u = NULL;
  if (!strcmp(path, "favicon.ico")) path = "favicon.png";
  r = surf_recurso(path);
  if (!r) return NULL;
  a_ruta_a_nsurl(r, &u);
  free(r);
  return u;
}

/* el tipo de un archivo del disco, por su extensión */
static const char *p_tipo(const char *ruta) {
  static const char *const t[][2] = {
    { "css", "text/css" }, { "htm", "text/html" }, { "html", "text/html" }, { "txt", "text/plain" },
    { "png", "image/png" }, { "jpg", "image/jpeg" }, { "jpeg", "image/jpeg" }, { "gif", "image/gif" },
    { "bmp", "image/bmp" }, { "ico", "image/x-icon" }, { "svg", "image/svg+xml" }, { "js", "application/javascript" },
    { "ini", "text/plain" }, { "bat", "text/plain" }, { "md", "text/plain" }, { "c", "text/plain" }, { "h", "text/plain" },
    { "asm", "text/plain" }, { "inc", "text/plain" } };
  const char *e = strrchr(ruta, '.');
  unsigned k;
  if (e && !strchr(e, '\\') && !strchr(e, '/'))
    for (k = 0; k < sizeof t / sizeof t[0]; k++) if (!strcasecmp(e + 1, t[k][0])) return t[k][1];
  return "text/html";
}

static struct gui_fetch_table tabla_pedir = { .filetype = p_tipo, .get_resource_url = p_recurso };
struct gui_fetch_table *surf_tabla_pedir = &tabla_pedir;
