/* =============================================================================
 *  anfitrion.c - el Navegador completo (NetSurf con lo de w32/surf) en el
 *  anfitrión (Linux), sin ventana: abre una página, espera a que acabe y
 *  deja la página pintada en un PPM. Para probar el pintado y la maquetación
 *  (CSS, letras, rejillas) en segundos, sin QEMU: tests/run_surf_pintado_tests.sh
 *  y tools/surf_compara.sh. La red, con curl (anfitrion_rh.c).
 *    surfppm [-w ancho] [-h alto] [-entera] [-o salida.ppm] DIRECCION
 *  (-entera: la página entera de alto, no solo lo que se ve)
 *  Las letras y los recursos: SURF_LETRAS (terceros/dejavu) y SURF_RECURSOS.
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "netsurf/netsurf.h"
#include "netsurf/misc.h"
#include "netsurf/window.h"
#include "netsurf/browser_window.h"
#include "netsurf/plotters.h"
#include "netsurf/clipboard.h"
#include "netsurf/fetch.h"
#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "netsurf/layout.h"
#include "utils/file.h"

#include <dom/dom.h>
#include "content/handlers/css/utils.h"
#include "html/box.h"
#include "html/box_inspect.h"
#include "html/html_save.h"

#include "netsurf/browser.h"
#include "surf.h"
#include "rh.h"

struct gui_window { struct browser_window *bw; int cargando, ext_w, ext_h; };
static struct gui_window *ventana;
static int pw = 780, ph = 493, hecha, hubo_carga;
int _commit(int fd) { (void)fd; return 0; }   /* (lo usa utils/log.c [HISPA]) */

void surf_reg(const char *f, ...) {
  va_list ap;
  if (!getenv("SURF_REGISTRO")) return;
  va_start(ap, f);
  vfprintf(stderr, f, ap);
  va_end(ap);
  fputc('\n', stderr);
}
void surf_estado(const char *t) { surf_reg("estado: %s", t); }
void memoria_cuenta(unsigned *b, unsigned *a, unsigned *m) { *b = *a = *m = 0; }

static struct gui_window *v_crea(struct browser_window *bw, struct gui_window *e, gui_window_create_flags f) {
  struct gui_window *g = calloc(1, sizeof *g);
  (void)e; (void)f;
  g->bw = bw;
  ventana = g;
  return g;
}
static void v_destruye(struct gui_window *g) { if (ventana == g) ventana = NULL; free(g); }
static nserror v_invalida(struct gui_window *g, const struct rect *r) { (void)g; (void)r; return NSERROR_OK; }
static bool v_da_desp(struct gui_window *g, int *sx, int *sy) { (void)g; *sx = *sy = 0; return true; }
static nserror v_pon_desp(struct gui_window *g, const struct rect *r) { (void)g; (void)r; return NSERROR_OK; }
static nserror v_medidas(struct gui_window *g, int *w, int *h) { (void)g; *w = pw; *h = ph; return NSERROR_OK; }
static nserror v_suceso(struct gui_window *g, enum gui_window_event e) {
  if (e == GW_EVENT_UPDATE_EXTENT) browser_window_get_extents(g->bw, true, &g->ext_w, &g->ext_h);
  if (e == GW_EVENT_START_THROBBER) { g->cargando = 1; hubo_carga = 1; }
  if (e == GW_EVENT_STOP_THROBBER) { g->cargando = 0; hecha = 1; }
  return NSERROR_OK;
}
static void v_titulo(struct gui_window *g, const char *t) { (void)g; surf_reg("título: %s", t); }
static nserror v_url(struct gui_window *g, struct nsurl *u) { (void)g; surf_reg("dirección: %s", nsurl_access(u)); return NSERROR_OK; }
static void v_estado(struct gui_window *g, const char *t) { (void)g; surf_estado(t); }
static void v_puntero(struct gui_window *g, enum gui_pointer_shape s) { (void)g; (void)s; }
static void v_cursor(struct gui_window *g, int x, int y, int h, const struct rect *c) { (void)g; (void)x; (void)y; (void)h; (void)c; }
static void v_consola(struct gui_window *g, browser_window_console_source src, const char *m, size_t n,
                      browser_window_console_flags f) {
  (void)g; (void)src; (void)f;
  surf_reg("consola: %.*s", (int)n, m);
}
static struct gui_window_table tabla_ventana = {
  .create = v_crea, .destroy = v_destruye, .invalidate = v_invalida, .get_scroll = v_da_desp,
  .set_scroll = v_pon_desp, .get_dimensions = v_medidas, .event = v_suceso,
  .set_title = v_titulo, .set_url = v_url, .set_status = v_estado, .set_pointer = v_puntero,
  .place_caret = v_cursor, .console_log = v_consola,
};
static void m_fin(void) { }
static struct gui_misc_table tabla_varios = { .schedule = framebuffer_schedule, .quit = m_fin };
static void pp_da(char **b, size_t *n) { *b = NULL; *n = 0; }
static void pp_pon(const char *b, size_t n, nsclipboard_styles s[], int ns) { (void)b; (void)n; (void)s; (void)ns; }
static struct gui_clipboard_table tabla_papel = { .get = pp_da, .set = pp_pon };

static nserror opciones(struct nsoption_s *d) {
  (void)d;
  nsoption_set_bool(core_select_menu, true);
  nsoption_set_bool(enable_javascript, false);
  nsoption_set_int(memory_cache_size, 1024 * 1024);
  nsoption_set_int(max_fetchers, 8);
  nsoption_set_int(max_fetchers_per_host, 4);
  nsoption_set_int(max_cached_fetch_handles, 2);
  nsoption_set_int(font_size, 120);
  nsoption_set_int(font_min_size, 80);
  nsoption_set_bool(animate_images, false);
  return NSERROR_OK;
}

static unsigned ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void ram_register_surface(void);

int main(int argc, char **argv) {
  struct netsurf_table tabla = {
    .misc = &tabla_varios, .window = &tabla_ventana, .clipboard = &tabla_papel, .file = NULL,
    .fetch = NULL, .utf8 = NULL, .bitmap = NULL, .layout = NULL,
  };
  const char *sal = "pagina.ppm", *dir = NULL, *letras, *recursos;
  char b[600];
  int k, entera = 0, alto;
  nsurl *u = NULL;
  struct browser_window *bw;
  nsfb_t *lienzo;
  unsigned t0;
  uint8_t *px;
  int paso;
  FILE *f;
  char *nargv[3] = { "surfppm", NULL, NULL };
  int nargc = 1;

  for (k = 1; k < argc; k++) {
    if (!strcmp(argv[k], "-w") && k + 1 < argc) pw = atoi(argv[++k]);
    else if (!strcmp(argv[k], "-h") && k + 1 < argc) ph = atoi(argv[++k]);
    else if (!strcmp(argv[k], "-o") && k + 1 < argc) sal = argv[++k];
    else if (!strcmp(argv[k], "-entera")) entera = 1;
    else dir = argv[k];
  }
  if (!dir) { fprintf(stderr, "uso: surfppm [-w ancho] [-h alto] [-entera] [-o salida.ppm] DIRECCION\n"); return 2; }
  letras = getenv("SURF_LETRAS") ? getenv("SURF_LETRAS") : "terceros/dejavu";
  recursos = getenv("SURF_RECURSOS") ? getenv("SURF_RECURSOS") : "build/anf/RECURSOS";
  {
    char r[PATH_MAX];
    if (!realpath(recursos, r)) { fprintf(stderr, "sin recursos en %s\n", recursos); return 1; }
    snprintf(surf_carpeta, sizeof surf_carpeta, "%s/..", r);
  }

  tabla.file = surf_tabla_archivos;
  tabla.fetch = surf_tabla_pedir;
  tabla.utf8 = surf_tabla_utf8;
  tabla.bitmap = surf_tabla_mapas;
  tabla.layout = surf_tabla_letras;
  if (netsurf_register(&tabla) != NSERROR_OK) { fprintf(stderr, "netsurf_register\n"); return 1; }
  if (getenv("SURF_REGISTRO")) nargv[nargc++] = "-v";
  nslog_init(NULL, &nargc, nargv);
  if (nsoption_init(opciones, &nsoptions, &nsoptions_default) != NSERROR_OK) return 1;
  nsoption_set_charp(accept_language, strdup("es"));
  snprintf(b, sizeof b, "%s/MESSAGES", recursos);
  messages_add_from_file(b);
  if (!letra_inicia(letras)) { fprintf(stderr, "sin letras en %s\n", letras); return 1; }
  /* 96 puntos por pulgada, como Chrome: con los 90 de NetSurf, 16px de CSS
   * eran 15 puntos en las cajas y 17 en las letras (letra.c) */
  browser_set_dpi(96);
  if (netsurf_init(NULL) != NSERROR_OK) { fprintf(stderr, "netsurf_init\n"); return 1; }
  rh_inicia("es");

  if (dir[0] == '/') { snprintf(b, sizeof b, "file://%s", dir); dir = b; }
  else if (!strstr(dir, ":")) {
    char c[400];
    if (!getcwd(c, sizeof c)) c[0] = 0;
    snprintf(b, sizeof b, "file://%s/%s", c, dir); dir = b;
  }
  if (nsurl_create(dir, &u) != NSERROR_OK ||
      browser_window_create(BW_CREATE_HISTORY, u, NULL, NULL, &bw) != NSERROR_OK) {
    fprintf(stderr, "no se puede abrir %s\n", dir);
    return 1;
  }
  nsurl_unref(u);
  t0 = ms();
  /* hasta que acabe de cargar (y se calme el planificador), o 60 s */
  while (ms() - t0 < 60000) {
    int t = schedule_run();
    if (hecha && ventana && !ventana->cargando && (t < 0 || t > 50)) break;
    if (t > 0) usleep((t > 20 ? 20 : t) * 1000);
  }
  surf_reg("hecha en %u ms", ms() - t0);
  if (!ventana) return 1;
  browser_window_get_extents(ventana->bw, true, &ventana->ext_w, &ventana->ext_h);
  /* SURF_CAJAS: el árbol de cajas (con su estilo) por la salida de errores */
  if (getenv("SURF_CAJAS") && browser_window_get_content(ventana->bw)) {
    struct box *caja = html_get_box_tree(browser_window_get_content(ventana->bw));
    if (caja) box_dump(stderr, caja, 0, strcmp(getenv("SURF_CAJAS"), "2") == 0);
  }
  alto = entera ? (ventana->ext_h > ph ? ventana->ext_h : ph) : ph;
  if (alto > 20000) alto = 20000;

  ram_register_surface();
  lienzo = nsfb_new(NSFB_SURFACE_RAM);
  nsfb_set_geometry(lienzo, pw, alto, NSFB_FMT_XRGB8888);
  if (nsfb_init(lienzo) == -1) return 1;
  framebuffer_set_surface(lienzo);
  {
    struct redraw_context ctx = { .interactive = true, .background_images = true, .plot = &fb_plotters };
    struct rect clip = { 0, 0, pw, alto };
    nsfb_bbox_t c = { 0, 0, pw, alto };
    nsfb_plot_set_clip(lienzo, &c);
    nsfb_plot_rectangle_fill(lienzo, &c, 0xFFFFFFFF);
    browser_window_redraw(ventana->bw, 0, 0, &clip, &ctx);
  }
  nsfb_get_buffer(lienzo, &px, &paso);
  f = fopen(sal, "wb");
  if (!f) return 1;
  fprintf(f, "P6\n%d %d\n255\n", pw, alto);
  for (k = 0; k < pw * alto; k++) {
    int y = k / pw, x = k % pw;
    uint32_t c = *(uint32_t *)(px + y * paso + x * 4);
    putc((c >> 16) & 255, f); putc((c >> 8) & 255, f); putc(c & 255, f);
  }
  fclose(f);
  rh_fin();
  return 0;
}
