/* surf.h - lo común del Navegador completo de Hispa-486 (NAVEGA2.EXE: NetSurf
 * con la ventana de Hispa Ventanas; w32/surf, docs/486/NAVEGADOR2.md)
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef SURF_H
#define SURF_H
#include <stdbool.h>
#include <libnsfb.h>
#include "netsurf/plotters.h"
#include "netsurf/plot_style.h"

/* cómo se presenta a los servidores: NO «HispaNavegador» (a ese, ClicGest le
 * da la web ligera), sino uno propio: así le dan la web normal */
#define SURF_VERSION "0.1"
/* el separador de las rutas (en el anfitrión, para las pruebas: anfitrion.c, «/») */
#ifdef SURF_ANFITRION
#define SURF_SEP "/"
#else
#define SURF_SEP "\\"
#endif
#define SURF_AGENTE "HispaSurf/" SURF_VERSION " (Hispa-486; NetSurf 3.11)"

/* ns/plot.c (los dibujos de NetSurf framebuffer, GPL-2.0) */
extern const struct plotter_table fb_plotters;
nsfb_t *framebuffer_set_surface(nsfb_t *new_nsfb);
/* ns/bitmap.c */
extern struct gui_bitmap_table *surf_tabla_mapas;
bool surf_bitmap_opaca(void *bitmap);
/* ns/schedule.c */
nserror framebuffer_schedule(int tival, void (*callback)(void *p), void *p);
int schedule_run(void);

/* letra.c */
bool letra_inicia(const char *carpeta);
void letra_fin(void);
void letra_pinta(nsfb_t *fb, const plot_font_style_t *s, int x, int y, const char *t, size_t n);
extern struct gui_layout_table *surf_tabla_letras;
extern struct gui_utf8_table *surf_tabla_utf8;

/* archivo.c: rutas de DOS (C:\...) <-> file:///C:/... */
extern struct gui_file_table *surf_tabla_archivos;
extern struct gui_fetch_table *surf_tabla_pedir;
extern char surf_carpeta[260];        /* la del programa (sin \ al final) */
char *surf_recurso(const char *nombre);   /* la ruta de un recurso (malloc) o NULL */

/* red.c: http y https con WSOCK32 y HSPTLS (el http.c del Navegador) */
nserror fetch_hispa_register(void);
int red_ocupada(void);
void red_fin(void);
extern int surf_cifrada;              /* la última página llegó por https */
/* [JS] fetch y XMLHttpRequest (guion.c): la respuesta entera (estado 0: error) */
typedef void (*red_js_cb)(void *ctx, int estado, const char *cab, const unsigned char *datos, unsigned n, const char *error);
int red_js_pide(const char *url, const char *metodo, const char *cuerpo, unsigned n_cuerpo, const char *cab,
                red_js_cb cb, void *ctx);
void red_js_cancela(void *ctx);

/* surf.c */
void surf_estado(const char *t);
void surf_reg(const char *f, ...);
#endif
