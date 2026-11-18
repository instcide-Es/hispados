/* =============================================================================
 *  NAVEGA2.EXE - el Navegador completo de Hispa-486: NetSurf (terceros/netsurf,
 *  GPL-2.0) dentro de una ventana de Hispa Ventanas, como programa Hispa32.
 *  docs/486/NAVEGADOR2.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Aquí, «el frontal» de NetSurf hecho nuestro: la ventana, la barra (Atrás,
 *  Adelante, Recargar/Parar, Inicio, la dirección), la línea de estado y la
 *  barra de desplazamiento, con el aspecto y los colores de Hispa Ventanas y
 *  con lo básico de GDI; la página la pinta NetSurf en un búfer de puntos de
 *  32 bits (libnsfb, ns/plot.c) que se copia a la ventana con
 *  SetDIBitsToDevice (Hispa Ventanas lo pasa a su paleta). El ratón, el
 *  teclado (Mayús, la rueda) y el planificador de NetSurf, en el bucle de
 *  mensajes. Los textos de la ventana, en la página 1252.
 * ============================================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utf8.h"
#include "netsurf/netsurf.h"
#include "netsurf/misc.h"
#include "netsurf/window.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "netsurf/mouse.h"
#include "netsurf/plotters.h"
#include "netsurf/clipboard.h"
#include "netsurf/search.h"
#include "netsurf/fetch.h"
#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "netsurf/layout.h"
#include "utils/file.h"
#include "desktop/browser_history.h"

#include "surf.h"
#include "rh.h"

/* ---- lo justo de USER32, GDI32 y KERNEL32 ---- */
#define WINAPI __attribute__((stdcall))
typedef void *HANDLE;
typedef uint32_t u32;
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { HANDLE hwnd; u32 message, wParam, lParam, time; POINT pt; } MSG;
typedef int WINAPI (*WNDPROC)(HANDLE, u32, u32, u32);
typedef struct {
  u32 style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance, hIcon, hCursor, hbrBackground;
  const char *lpszMenuName, *lpszClassName;
} WNDCLASSA;
typedef struct { HANDLE hdc; int fErase; RECT rcPaint; int fRestore, fIncUpdate; char rgbReserved[32]; } PAINTSTRUCT;
typedef struct { u32 biSize; int biWidth, biHeight; uint16_t biPlanes, biBitCount; u32 biCompression, biSizeImage;
                 int biXPelsPerMeter, biYPelsPerMeter; u32 biClrUsed, biClrImportant; } BMIH;
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_SETFOCUS 0x0007
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WM_RBUTTONDOWN 0x0204
#define WM_MOUSEWHEEL 0x020A
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_ESCAPE 27
#define VK_SPACE 32
#define VK_PRIOR 33
#define VK_NEXT 34
#define VK_END 35
#define VK_HOME 36
#define VK_LEFT 37
#define VK_UP 38
#define VK_RIGHT 39
#define VK_DOWN 40
#define VK_DELETE 46
#define VK_F5 0x74
#define VK_F6 0x75
HANDLE WINAPI GetModuleHandleA(const char *);
u32 WINAPI GetModuleFileNameA(HANDLE, char *, u32);
char *WINAPI GetCommandLineA(void);
void WINAPI ExitProcess(u32);
u32 WINAPI RegisterClassA(const WNDCLASSA *);
HANDLE WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
int WINAPI ShowWindow(HANDLE, int);
int WINAPI UpdateWindow(HANDLE);
int WINAPI DestroyWindow(HANDLE);
int WINAPI DefWindowProcA(HANDLE, u32, u32, u32);
int WINAPI GetMessageA(MSG *, HANDLE, u32, u32);
int WINAPI PeekMessageA(MSG *, HANDLE, u32, u32, u32);
int WINAPI TranslateMessage(const MSG *);
int WINAPI DispatchMessageA(const MSG *);
void WINAPI PostQuitMessage(int);
int WINAPI GetClientRect(HANDLE, RECT *);
int WINAPI InvalidateRect(HANDLE, const RECT *, int);
int WINAPI SetWindowTextA(HANDLE, const char *);
int WINAPI MessageBoxA(HANDLE, const char *, const char *, u32);
int WINAPI GetSystemMetrics(int);
HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);
HANDLE WINAPI BeginPaint(HANDLE, PAINTSTRUCT *);
int WINAPI EndPaint(HANDLE, const PAINTSTRUCT *);
int WINAPI FillRect(HANDLE, const RECT *, HANDLE);
int WINAPI TextOutA(HANDLE, int, int, const char *, int);
u32 WINAPI SetTextColor(HANDLE, u32);
int WINAPI SetBkMode(HANDLE, int);
HANDLE WINAPI CreateSolidBrush(u32);
int WINAPI DeleteObject(HANDLE);
u32 WINAPI GetTickCount(void);
u32 WINAPI SetTimer(HANDLE, u32, u32, void *);
int WINAPI KillTimer(HANDLE, u32);
short WINAPI GetKeyState(int);
HANDLE WINAPI LoadCursorA(HANDLE, u32);
HANDLE WINAPI SetCursor(HANDLE);
HANDLE WINAPI SetCapture(HANDLE);
int WINAPI ReleaseCapture(void);
int WINAPI SetDIBitsToDevice(HANDLE, int, int, u32, u32, int, int, u32, u32, const void *, const void *, u32);

/* ---- medidas (puntos) y colores (la paleta de Hispa Ventanas) ---- */
#define LW 8                   /* una letra de la ventana */
#define BARRA_H 32             /* la barra de arriba: botones y dirección */
#define ESTADO_H 20
#define DESP_W 16              /* la barra de desplazamiento */
#define P(n) (0x01000000u | (n))
enum { P_TINTA, P_MARRON, P_PETROLEO, P_PETROLEO_CL, P_TERRACOTA, P_TERRACOTA_CL, P_ARENA, P_ARENA_OSC, P_CREMA,
       P_BLANCO, P_OCRE, P_OLIVA, P_ANIL, P_GRANATE, P_TURQUESA, P_PIEDRA };

struct gui_window {
  struct browser_window *bw;
  struct gui_window *sig;
  int sx, sy;                  /* desde dónde se ve la página (puntos) */
  int ext_w, ext_h;            /* lo que mide la página */
  int caret_x, caret_y, caret_h;   /* el cursor de un campo (caret_h 0: no hay) */
  int cargando;
};

static HANDLE vent;
static int an, al;             /* la zona de la ventana */
static int pw, ph;             /* la zona de la página */
static nsfb_t *lienzo;         /* el búfer de la página (pw x ph, 32 bits) */
static struct gui_window *ventanas, *actual;
static struct { int x0, y0, x1, y1; } sucio;   /* lo que hay que repintar de la página (en la zona) */
static int salir;
static char estado[300];
static char inicio[400] = "resource:inicio.htm";
static char idioma[32] = "es";
static char galletas[300];
/* el registro de lo que pasa (si existe SURF.LOG junto al programa) */
static char *registro;
void surf_reg(const char *f, ...) {
  va_list ap;
  FILE *r;
  if (!registro || !(r = fopen(registro, "a"))) return;
  fprintf(r, "%u ", (unsigned)GetTickCount());
  va_start(ap, f);
  vfprintf(r, f, ap);
  va_end(ap);
  fputc('\n', r);
  fclose(r);    /* (cerrado: DOS apunta el tamaño en el directorio) */
}
/* la dirección: un campo de texto propio (la página 1252) */
static char dir[1024];
static int n_dir, dir_cur, dir_desp, foco_dir, dir_sel;
/* la barra de desplazamiento: arrastrar el pulgar */
static int arrastra = -1, arr_y0, arr_sy0;
static int boton_dentro;       /* se pulsó dentro de la página (el botón izquierdo) */

/* ---- pinceles ---- */
static HANDLE pinceles[64];
static u32 pincel_color[64];
static int n_pinceles;
static HANDLE pincel(u32 c) {
  int k;
  for (k = 0; k < n_pinceles; k++) if (pincel_color[k] == c) return pinceles[k];
  if (n_pinceles == 64) { for (k = 16; k < 64; k++) DeleteObject(pinceles[k]); n_pinceles = 16; }
  pincel_color[n_pinceles] = c;
  pinceles[n_pinceles] = CreateSolidBrush(c);
  return pinceles[n_pinceles++];
}
static void caja(HANDLE dc, int x, int y, int w, int h, u32 c) {
  RECT r;
  if (w <= 0 || h <= 0) return;
  r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
  FillRect(dc, &r, pincel(c));
}
static void marco(HANDLE dc, int x, int y, int w, int h, u32 c) {
  caja(dc, x, y, w, 1, c); caja(dc, x, y + h - 1, w, 1, c);
  caja(dc, x, y, 1, h, c); caja(dc, x + w - 1, y, 1, h, c);
}
static void txt(HANDLE dc, int x, int y, const char *t, int n, u32 c) {
  if (n < 0) n = strlen(t);
  if (n <= 0) return;
  SetTextColor(dc, c);
  TextOutA(dc, x, y, t, n);
}
/* UTF-8 -> 1252 (para la ventana), en b */
static void a_1252(const char *u, char *b, size_t max) {
  char *r = NULL;
  if (utf8_to_enc(u, "WINDOWS-1252", 0, &r) == NSERROR_OK && r) { snprintf(b, max, "%s", r); free(r); }
  else snprintf(b, max, "%s", u);
}

/* =============================================================================
 *  La barra de arriba, la de estado y la de desplazamiento
 * ============================================================================= */
enum { B_ATRAS, B_ADELANTE, B_RECARGA, B_INICIO, B_IR, B_N };
static const char *const b_texto[B_N] = { "<", ">", "Recargar", "Inicio", "Ir" };
static RECT b_rect[B_N], dir_rect;
static int cargando(void) { return actual && actual->cargando; }
static const char *b_rotulo(int k) { return k == B_RECARGA && cargando() ? "Parar" : b_texto[k]; }
static int b_activo(int k) {
  if (!actual) return 0;
  if (k == B_ATRAS) return browser_window_back_available(actual->bw);
  if (k == B_ADELANTE) return browser_window_forward_available(actual->bw);
  return 1;
}
static void coloca(void) {
  RECT r;
  int k, x = 4;
  GetClientRect(vent, &r);
  an = r.right; al = r.bottom;
  pw = an - DESP_W; ph = al - BARRA_H - ESTADO_H;
  if (pw < 64) pw = 64;
  if (ph < 32) ph = 32;
  for (k = 0; k < B_IR; k++) {
    int w = (strlen(k == B_RECARGA ? "Recargar" : b_texto[k]) + 2) * LW;
    b_rect[k].left = x; b_rect[k].top = 4; b_rect[k].right = x + w; b_rect[k].bottom = 27;
    x += w + 4;
  }
  b_rect[B_IR].right = an - 4; b_rect[B_IR].left = an - 4 - 4 * LW; b_rect[B_IR].top = 4; b_rect[B_IR].bottom = 27;
  dir_rect.left = x + 4; dir_rect.top = 4; dir_rect.right = b_rect[B_IR].left - 4; dir_rect.bottom = 27;
}
static int dir_vis(void) { return (dir_rect.right - dir_rect.left) / LW - 2; }
static void pinta_dir(HANDLE dc) {
  int vis = dir_vis(), n;
  caja(dc, dir_rect.left, dir_rect.top, dir_rect.right - dir_rect.left, dir_rect.bottom - dir_rect.top, P(P_BLANCO));
  marco(dc, dir_rect.left, dir_rect.top, dir_rect.right - dir_rect.left, dir_rect.bottom - dir_rect.top, P(foco_dir ? P_TERRACOTA : P_TINTA));
  if (!foco_dir) dir_desp = 0;
  else if (dir_cur < dir_desp) dir_desp = dir_cur;
  if (foco_dir && dir_cur > dir_desp + vis - 1) dir_desp = dir_cur - vis + 1;
  if (dir_desp < 0) dir_desp = 0;
  n = n_dir - dir_desp;
  if (n > vis) n = vis;
  SetBkMode(dc, 1);
  if (foco_dir && dir_sel && n > 0) caja(dc, dir_rect.left + LW, dir_rect.top + 4, n * LW, 16, P(P_ANIL));
  txt(dc, dir_rect.left + LW, dir_rect.top + 4, dir + dir_desp, n, P(foco_dir && dir_sel ? P_BLANCO : P_TINTA));
  if (foco_dir && !dir_sel) caja(dc, dir_rect.left + LW + (dir_cur - dir_desp) * LW, dir_rect.top + 4, 2, 16, P(P_TERRACOTA));
}
static void pinta_barra(HANDLE dc) {
  int k;
  caja(dc, 0, 0, an, BARRA_H - 1, P(P_PIEDRA));
  caja(dc, 0, BARRA_H - 1, an, 1, P(P_ARENA_OSC));
  SetBkMode(dc, 1);
  for (k = 0; k < B_N; k++) {
    RECT *r = &b_rect[k];
    const char *t = b_rotulo(k);
    int w = r->right - r->left;
    caja(dc, r->left, r->top, w, r->bottom - r->top, P(k == B_RECARGA && cargando() ? P_OCRE : P_ARENA));
    marco(dc, r->left, r->top, w, r->bottom - r->top, P(P_TINTA));
    txt(dc, r->left + (w - (int)strlen(t) * LW) / 2, r->top + 4, t, -1, P(b_activo(k) ? P_TINTA : P_ARENA_OSC));
  }
  pinta_dir(dc);
}
static void pinta_estado(HANDLE dc) {
  int y = al - ESTADO_H, n, max;
  int carga = cargando();
  const char *d = carga ? "Espere, por favor" : actual && surf_cifrada ? "Conexi\363n cifrada" : "";
  caja(dc, 0, y, an, ESTADO_H, P(carga ? P_OCRE : P_PIEDRA));
  caja(dc, 0, y, an, 1, P(P_ARENA_OSC));
  SetBkMode(dc, 1);
  max = (an - 12) / LW - (int)strlen(d) - 2;
  n = strlen(estado);
  if (n > max) n = max;
  txt(dc, 6, y + 2, estado, n, P(P_TINTA));
  txt(dc, an - 6 - (int)strlen(d) * LW, y + 2, d, -1, P(carga ? P_TINTA : P_OLIVA));
}
/* la barra de desplazamiento: el pulgar */
static void pulgar(int *y, int *h) {
  int alto = ph, tot = actual && actual->ext_h > ph ? actual->ext_h : ph;
  *h = alto * ph / tot;
  if (*h < 16) *h = 16;
  if (*h > alto) *h = alto;
  *y = tot > ph && actual ? (int)((int64_t)actual->sy * (alto - *h) / (tot - ph)) : 0;
}
static void pinta_desp(HANDLE dc) {
  int y, h;
  caja(dc, pw, BARRA_H, DESP_W, ph, P(P_ARENA));
  caja(dc, pw, BARRA_H, 1, ph, P(P_ARENA_OSC));
  pulgar(&y, &h);
  if (actual && actual->ext_h > ph) {
    caja(dc, pw + 2, BARRA_H + y, DESP_W - 3, h, P(P_PETROLEO));
    marco(dc, pw + 2, BARRA_H + y, DESP_W - 3, h, P(P_TINTA));
  }
}
void surf_estado(const char *t) {
  HANDLE dc;
  char b[300];
  a_1252(t ? t : "", b, sizeof b);
  if (!strcmp(b, estado)) return;
  strcpy(estado, b);
  if (!vent) return;
  dc = GetDC(vent);
  pinta_estado(dc);
  ReleaseDC(vent, dc);
}
static void repinta_barras(void) {
  HANDLE dc;
  if (!vent) return;
  dc = GetDC(vent);
  pinta_barra(dc);
  pinta_estado(dc);
  pinta_desp(dc);
  ReleaseDC(vent, dc);
}

/* =============================================================================
 *  La página: el búfer, lo sucio y copiarlo a la ventana
 * ============================================================================= */
static void ensucia(int x0, int y0, int x1, int y1) {
  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > pw) x1 = pw;
  if (y1 > ph) y1 = ph;
  if (x0 >= x1 || y0 >= y1) return;
  if (sucio.x0 >= sucio.x1) { sucio.x0 = x0; sucio.y0 = y0; sucio.x1 = x1; sucio.y1 = y1; return; }
  if (x0 < sucio.x0) sucio.x0 = x0;
  if (y0 < sucio.y0) sucio.y0 = y0;
  if (x1 > sucio.x1) sucio.x1 = x1;
  if (y1 > sucio.y1) sucio.y1 = y1;
}
static void ensucia_todo(void) { ensucia(0, 0, pw, ph); }
static void copia(HANDLE dc, int x0, int y0, int x1, int y1) {
  BMIH b;
  uint8_t *px;
  int paso;
  if (!lienzo || x0 >= x1 || y0 >= y1) return;
  nsfb_get_buffer(lienzo, &px, &paso);
  memset(&b, 0, sizeof b);
  b.biSize = 40; b.biWidth = paso / 4; b.biHeight = -ph; b.biPlanes = 1; b.biBitCount = 32;
  SetDIBitsToDevice(dc, x0, BARRA_H + y0, x1 - x0, y1 - y0, x0, y0, 0, ph, px, &b, 0);
}
/* pinta lo sucio en el búfer y lo copia a la ventana */
static void pinta_sucio(void) {
  struct redraw_context ctx = { .interactive = true, .background_images = true, .plot = &fb_plotters };
  struct rect clip;
  HANDLE dc;
  if (sucio.x0 >= sucio.x1 || !actual || !lienzo) return;
  clip.x0 = sucio.x0; clip.y0 = sucio.y0; clip.x1 = sucio.x1; clip.y1 = sucio.y1;
  sucio.x0 = sucio.x1 = 0;
  {
    nsfb_bbox_t c = { clip.x0, clip.y0, clip.x1, clip.y1 };
    nsfb_plot_set_clip(lienzo, &c);
    nsfb_plot_rectangle_fill(lienzo, &c, 0xFFFFFFFF);
  }
  {
    u32 t0 = GetTickCount();
    browser_window_redraw(actual->bw, -actual->sx, -actual->sy, &clip, &ctx);
    surf_reg("pinta %d,%d-%d,%d: %u ms", clip.x0, clip.y0, clip.x1, clip.y1, (unsigned)(GetTickCount() - t0));
  }
  if (actual->caret_h) {
    nsfb_bbox_t l;
    nsfb_plot_pen_t pen;
    l.x0 = l.x1 = actual->caret_x - actual->sx;
    l.y0 = actual->caret_y - actual->sy;
    l.y1 = l.y0 + actual->caret_h;
    pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
    pen.stroke_width = 1;
    pen.stroke_colour = 0xFF000000;
    nsfb_plot_line(lienzo, &l, &pen);
  }
  dc = GetDC(vent);
  copia(dc, clip.x0, clip.y0, clip.x1, clip.y1);
  ReleaseDC(vent, dc);
}
void ram_register_surface(void);   /* libnsfb (sin constructores en Hispa-486) */
static void nuevo_lienzo(void) {
  static int hecho;
  if (!hecho) { ram_register_surface(); hecho = 1; }
  if (lienzo) nsfb_free(lienzo);
  lienzo = nsfb_new(NSFB_SURFACE_RAM);
  if (!lienzo) return;
  nsfb_set_geometry(lienzo, pw, ph, NSFB_FMT_XRGB8888);
  if (nsfb_init(lienzo) == -1) { surf_reg("lienzo: no"); nsfb_free(lienzo); lienzo = NULL; return; }
  surf_reg("lienzo %dx%d", pw, ph);
  framebuffer_set_surface(lienzo);
  ensucia_todo();
}

/* el desplazamiento: hasta dónde se puede */
static void desplaza_a(int x, int y) {
  int mx, my;
  if (!actual) return;
  mx = actual->ext_w - pw; my = actual->ext_h - ph;
  if (x > mx) x = mx;
  if (y > my) y = my;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x == actual->sx && y == actual->sy) return;
  actual->sx = x; actual->sy = y;
  ensucia_todo();
  {
    HANDLE dc = GetDC(vent);
    pinta_desp(dc);
    ReleaseDC(vent, dc);
  }
}
static void desplaza(int dx, int dy) { if (actual) desplaza_a(actual->sx + dx, actual->sy + dy); }

/* =============================================================================
 *  Las ventanas de NetSurf (gui_window_table): aquí hay una, la de arriba
 * ============================================================================= */
static struct gui_window *v_crea(struct browser_window *bw, struct gui_window *existe, gui_window_create_flags f) {
  struct gui_window *g = calloc(1, sizeof *g);
  (void)existe; (void)f;
  if (!g) return NULL;
  g->bw = bw;
  g->sig = ventanas;
  ventanas = g;
  actual = g;
  ensucia_todo();
  return g;
}
static void v_destruye(struct gui_window *g) {
  struct gui_window **q;
  for (q = &ventanas; *q; q = &(*q)->sig) if (*q == g) { *q = g->sig; break; }
  if (actual == g) actual = ventanas;
  free(g);
  if (!ventanas) salir = 1;
  ensucia_todo();
}
static nserror v_invalida(struct gui_window *g, const struct rect *r) {
  if (g != actual) return NSERROR_OK;
  if (!r) ensucia_todo();
  else ensucia(r->x0 - g->sx, r->y0 - g->sy, r->x1 - g->sx, r->y1 - g->sy);
  return NSERROR_OK;
}
static bool v_da_desp(struct gui_window *g, int *sx, int *sy) { *sx = g->sx; *sy = g->sy; return true; }
static nserror v_pon_desp(struct gui_window *g, const struct rect *r) {
  if (g != actual) { g->sx = r->x0; g->sy = r->y0; return NSERROR_OK; }
  desplaza_a(r->x0, r->y0);
  return NSERROR_OK;
}
static nserror v_medidas(struct gui_window *g, int *w, int *h) { (void)g; *w = pw; *h = ph; return NSERROR_OK; }
static void v_extension(struct gui_window *g) {
  int w, h;
  if (browser_window_get_extents(g->bw, true, &w, &h) == NSERROR_OK) { g->ext_w = w; g->ext_h = h; }
  if (g == actual) { desplaza_a(g->sx, g->sy); repinta_barras(); }
}
static nserror v_suceso(struct gui_window *g, enum gui_window_event e) {
  switch (e) {
  case GW_EVENT_UPDATE_EXTENT: v_extension(g); break;
  case GW_EVENT_REMOVE_CARET:
    if (g->caret_h) { v_invalida(g, &(struct rect){ g->caret_x - 1, g->caret_y, g->caret_x + 2, g->caret_y + g->caret_h }); g->caret_h = 0; }
    break;
  case GW_EVENT_START_THROBBER: g->cargando = 1; repinta_barras(); SetCursor(LoadCursorA(NULL, 32514)); break;
  case GW_EVENT_STOP_THROBBER: g->cargando = 0; repinta_barras(); SetCursor(LoadCursorA(NULL, 32512)); break;
  default: break;
  }
  return NSERROR_OK;
}
static void v_titulo(struct gui_window *g, const char *t) {
  char b[200], c[260];
  (void)g;
  a_1252(t, b, 150);
  snprintf(c, sizeof c, "%s - Navegador completo", b);
  SetWindowTextA(vent, c);
}
static nserror v_url(struct gui_window *g, struct nsurl *u) {
  if (g != actual || foco_dir) return NSERROR_OK;
  a_1252(nsurl_access(u), dir, sizeof dir);
  n_dir = strlen(dir); dir_cur = n_dir; dir_desp = 0;
  repinta_barras();
  return NSERROR_OK;
}
static void v_estado(struct gui_window *g, const char *t) { surf_reg("estado: %s", t); if (g == actual) surf_estado(t); }
static void v_puntero(struct gui_window *g, enum gui_pointer_shape s) {
  (void)g;
  SetCursor(LoadCursorA(NULL, s == GUI_POINTER_CARET ? 32513 : s == GUI_POINTER_WAIT || s == GUI_POINTER_PROGRESS ? 32514
                                : s == GUI_POINTER_CROSS ? 32515 : 32512));
}
static void v_cursor(struct gui_window *g, int x, int y, int h, const struct rect *clip) {
  (void)clip;
  if (g->caret_h) v_invalida(g, &(struct rect){ g->caret_x - 1, g->caret_y, g->caret_x + 2, g->caret_y + g->caret_h });
  g->caret_x = x; g->caret_y = y; g->caret_h = h;
  v_invalida(g, &(struct rect){ x - 1, y, x + 2, y + h });
}
static void v_consola(struct gui_window *g, browser_window_console_source src, const char *m, size_t n,
                      browser_window_console_flags f) {
  (void)g; (void)src; (void)m; (void)n; (void)f;
}

static struct gui_window_table tabla_ventana = {
  .create = v_crea, .destroy = v_destruye, .invalidate = v_invalida, .get_scroll = v_da_desp,
  .set_scroll = v_pon_desp, .get_dimensions = v_medidas, .event = v_suceso,
  .set_title = v_titulo, .set_url = v_url, .set_status = v_estado, .set_pointer = v_puntero,
  .place_caret = v_cursor, .console_log = v_consola,
};

static void m_fin(void) { }
static nserror m_avisa(const char *a, const char *b) {
  char t[600], x[600];
  snprintf(t, sizeof t, "%s%s%s", a ? a : "", b ? "\n" : "", b ? b : "");
  a_1252(t, x, sizeof x);
  MessageBoxA(vent, x, "Navegador completo", 0x30);
  return NSERROR_OK;
}
static struct gui_misc_table tabla_varios = { .schedule = framebuffer_schedule, .quit = m_fin };

/* el portapapeles: uno propio, dentro del programa */
static char *papel;
static void pp_da(char **b, size_t *n) {
  *b = papel ? strdup(papel) : NULL;
  *n = *b ? strlen(*b) : 0;
}
static void pp_pon(const char *b, size_t n, nsclipboard_styles s[], int ns) {
  (void)s; (void)ns;
  free(papel);
  papel = malloc(n + 1);
  if (papel) { memcpy(papel, b, n); papel[n] = 0; }
}
static struct gui_clipboard_table tabla_papel = { .get = pp_da, .set = pp_pon };

/* =============================================================================
 *  Ir a una dirección
 * ============================================================================= */
static void ir(const char *u8) {
  nsurl *u;
  nserror e;
  if (!actual) return;
  e = nsurl_create(u8, &u);
  if (e != NSERROR_OK) {
    /* sin esquema: «hispados.es» -> http://hispados.es */
    char b[1100];
    snprintf(b, sizeof b, "http://%s", u8);
    e = nsurl_create(b, &u);
  }
  if (e != NSERROR_OK) { m_avisa("La direcci\303\263n no vale.", u8); return; }
  browser_window_navigate(actual->bw, u, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
  nsurl_unref(u);
}
static void ir_dir(void) {
  char *u8 = NULL;
  char b[1100];
  int k = 0;
  while (k < n_dir && dir[k] == ' ') k++;
  snprintf(b, sizeof b, "%.*s", n_dir - k, dir + k);
  if (!b[0]) return;
  if (!strstr(b, "://") && strncasecmp(b, "about:", 6) && strncasecmp(b, "resource:", 9) && strncasecmp(b, "data:", 5) &&
      strncasecmp(b, "file:", 5)) {
    char c[1100];
    if (b[1] == ':' && (b[2] == '\\' || b[2] == '/')) {      /* C:\CARPETA\PAGINA.HTM */
      nsurl *u;
      if (netsurf_path_to_nsurl(b, &u) == NSERROR_OK) {
        foco_dir = 0;
        browser_window_navigate(actual->bw, u, NULL, BW_NAVIGATE_HISTORY, NULL, NULL, NULL);
        nsurl_unref(u);
        repinta_barras();
        return;
      }
    }
    snprintf(c, sizeof c, "http://%s", b);
    strcpy(b, c);
  }
  utf8_from_enc(b, "WINDOWS-1252", 0, &u8, NULL);
  foco_dir = 0;
  ir(u8 ? u8 : b);
  free(u8);
  repinta_barras();
}

/* =============================================================================
 *  El ratón y el teclado
 * ============================================================================= */
static int mayus(void) { return (GetKeyState(VK_SHIFT) & 0x8000) != 0; }
static int ctrl(void) { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
static int en(RECT *r, int x, int y) { return x >= r->left && x < r->right && y >= r->top && y < r->bottom; }
static browser_mouse_state mods(void) {
  return (mayus() ? BROWSER_MOUSE_MOD_1 : 0) | (ctrl() ? BROWSER_MOUSE_MOD_2 : 0);
}
static void boton(int k) {
  if (!actual) return;
  switch (k) {
  case B_ATRAS: if (browser_window_back_available(actual->bw)) browser_window_history_back(actual->bw, false); break;
  case B_ADELANTE: if (browser_window_forward_available(actual->bw)) browser_window_history_forward(actual->bw, false); break;
  case B_RECARGA: if (cargando()) browser_window_stop(actual->bw); else browser_window_reload(actual->bw, true); break;
  case B_INICIO: ir(inicio); break;
  case B_IR: ir_dir(); break;
  }
  repinta_barras();
}
static void foco_a_dir(int todo) {
  foco_dir = 1; dir_sel = todo; dir_cur = n_dir;
  repinta_barras();
}
static void clic(int x, int y) {
  int k;
  if (y < BARRA_H) {
    for (k = 0; k < B_N; k++) if (en(&b_rect[k], x, y)) { foco_dir = 0; boton(k); return; }
    if (en(&dir_rect, x, y)) {
      int c = (x - dir_rect.left - LW) / LW + dir_desp;
      if (!foco_dir) { foco_a_dir(1); return; }
      dir_sel = 0;
      dir_cur = c < 0 ? 0 : c > n_dir ? n_dir : c;
      repinta_barras();
    }
    return;
  }
  if (foco_dir) { foco_dir = 0; repinta_barras(); }
  if (x >= pw && y < BARRA_H + ph) {                 /* la barra de desplazamiento */
    int py, h;
    pulgar(&py, &h);
    y -= BARRA_H;
    if (y < py) desplaza(0, -(ph - 40));
    else if (y >= py + h) desplaza(0, ph - 40);
    else { arrastra = 1; arr_y0 = y; arr_sy0 = actual ? actual->sy : 0; SetCapture(vent); }
    return;
  }
  if (y >= BARRA_H && y < BARRA_H + ph && actual) {
    boton_dentro = 1;
    SetCapture(vent);
    browser_window_mouse_click(actual->bw, BROWSER_MOUSE_PRESS_1 | mods(), x + actual->sx, y - BARRA_H + actual->sy);
  }
}
static void suelta(int x, int y) {
  if (arrastra >= 0) { arrastra = -1; ReleaseCapture(); return; }
  if (boton_dentro && actual) {
    boton_dentro = 0;
    ReleaseCapture();
    browser_window_mouse_click(actual->bw, BROWSER_MOUSE_CLICK_1 | mods(), x + actual->sx, y - BARRA_H + actual->sy);
  }
}
static void mueve(int x, int y, int botones) {
  if (arrastra >= 0 && actual) {
    int py, h, tot = actual->ext_h;
    pulgar(&py, &h);
    if (ph > h && tot > ph) desplaza_a(actual->sx, arr_sy0 + (int)((int64_t)(y - BARRA_H - arr_y0) * (tot - ph) / (ph - h)));
    return;
  }
  if (!actual) return;
  if (y >= BARRA_H && y < BARRA_H + ph && x < pw)
    browser_window_mouse_track(actual->bw, ((botones & 1) ? BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON : 0) | mods(),
                               x + actual->sx, y - BARRA_H + actual->sy);
  else surf_estado("");
}
static void rueda(int pasos, int x, int y) {
  int dy = -pasos * 48;
  if (!actual) return;
  if (mayus()) { desplaza(dy, 0); return; }
  if (x < pw && y >= BARRA_H && y < BARRA_H + ph &&
      browser_window_scroll_at_point(actual->bw, x + actual->sx, y - BARRA_H + actual->sy, 0, dy)) return;
  desplaza(0, dy);
}

/* una tecla en el campo de la dirección */
static int tecla_dir(u32 v, int letra) {
  if (letra) {
    if (letra == 13) { ir_dir(); return 1; }
    if (letra == 27) { foco_dir = 0; if (actual) v_url(actual, browser_window_access_url(actual->bw)); repinta_barras(); return 1; }
    if (letra == 8) {
      if (dir_sel) { n_dir = dir_cur = 0; dir_sel = 0; }
      else if (dir_cur > 0) { memmove(dir + dir_cur - 1, dir + dir_cur, n_dir - dir_cur); n_dir--; dir_cur--; }
    } else if ((unsigned char)letra >= 32 && n_dir < (int)sizeof dir - 1) {
      if (dir_sel) { n_dir = dir_cur = 0; dir_sel = 0; }
      memmove(dir + dir_cur + 1, dir + dir_cur, n_dir - dir_cur);
      dir[dir_cur++] = (char)letra; n_dir++;
    }
    repinta_barras();
    return 1;
  }
  switch (v) {
  case VK_LEFT: if (dir_cur > 0) dir_cur--; dir_sel = 0; break;
  case VK_RIGHT: if (dir_cur < n_dir) dir_cur++; dir_sel = 0; break;
  case VK_HOME: dir_cur = 0; dir_sel = 0; break;
  case VK_END: dir_cur = n_dir; dir_sel = 0; break;
  case VK_DELETE:
    if (dir_sel) { n_dir = dir_cur = 0; dir_sel = 0; }
    else if (dir_cur < n_dir) { memmove(dir + dir_cur, dir + dir_cur + 1, n_dir - dir_cur - 1); n_dir--; }
    break;
  default: return 0;
  }
  repinta_barras();
  return 1;
}
/* las teclas especiales, a la página (NetSurf) o al desplazamiento */
static void tecla(u32 v) {
  uint32_t k = 0;
  if (v == VK_F6 || (ctrl() && v == 'L')) { foco_a_dir(1); return; }
  if (v == VK_F5 || (ctrl() && v == 'R')) { boton(B_RECARGA); return; }
  if (v == VK_ESCAPE && cargando()) { browser_window_stop(actual->bw); return; }
  if (foco_dir) { tecla_dir(v, 0); return; }
  if (!actual) return;
  switch (v) {
  case VK_LEFT: k = NS_KEY_LEFT; break;
  case VK_RIGHT: k = NS_KEY_RIGHT; break;
  case VK_UP: k = NS_KEY_UP; break;
  case VK_DOWN: k = NS_KEY_DOWN; break;
  case VK_PRIOR: k = NS_KEY_PAGE_UP; break;
  case VK_NEXT: k = NS_KEY_PAGE_DOWN; break;
  case VK_HOME: k = ctrl() ? NS_KEY_TEXT_START : NS_KEY_LINE_START; break;
  case VK_END: k = ctrl() ? NS_KEY_TEXT_END : NS_KEY_LINE_END; break;
  case VK_DELETE: k = NS_KEY_DELETE_RIGHT; break;
  default: return;
  }
  if (browser_window_key_press(actual->bw, k)) return;
  /* si no era para un campo: mover la página */
  switch (v) {
  case VK_UP: desplaza(0, -40); break;
  case VK_DOWN: desplaza(0, 40); break;
  case VK_LEFT: desplaza(-40, 0); break;
  case VK_RIGHT: desplaza(40, 0); break;
  case VK_PRIOR: desplaza(0, -(ph - 40)); break;
  case VK_NEXT: desplaza(0, ph - 40); break;
  case VK_HOME: desplaza_a(0, 0); break;
  case VK_END: if (actual) desplaza_a(0, actual->ext_h); break;
  }
}
static const uint16_t alto1252[32] = {
  0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
  0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };
static void letra(u32 c) {
  uint32_t u = c & 255;
  if (foco_dir) { tecla_dir(0, (int)u); return; }
  if (!actual) return;
  if (u >= 0x80 && u < 0xA0 && alto1252[u - 0x80]) u = alto1252[u - 0x80];
  if (u == 8) u = NS_KEY_DELETE_LEFT;
  else if (u == 9) u = mayus() ? NS_KEY_SHIFT_TAB : NS_KEY_TAB;
  else if (u == 13) u = NS_KEY_NL;
  else if (u == 27) u = NS_KEY_ESCAPE;
  else if (u == 1 && ctrl()) u = NS_KEY_SELECT_ALL;
  else if (u == 3 && ctrl()) u = NS_KEY_COPY_SELECTION;
  else if (u == 22 && ctrl()) u = NS_KEY_PASTE;
  else if (u == 24 && ctrl()) u = NS_KEY_CUT_SELECTION;
  else if (u < 32) return;
  if (browser_window_key_press(actual->bw, u)) return;
  if (u == ' ') desplaza(0, mayus() ? -(ph - 40) : ph - 40);
  else if (u == NS_KEY_DELETE_LEFT) boton(B_ATRAS);
}

static int WINAPI proc(HANDLE h, u32 m, u32 w, u32 l) {
  int x = (short)(l & 0xFFFF), y = (short)(l >> 16);
  switch (m) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HANDLE dc = BeginPaint(h, &ps);
    pinta_barra(dc);
    pinta_estado(dc);
    pinta_desp(dc);
    copia(dc, 0, 0, pw, ph);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_ERASEBKGND: return 1;
  case WM_SIZE: {
    int pw0 = pw, ph0 = ph;
    coloca();
    if (pw != pw0 || ph != ph0) {
      nuevo_lienzo();
      if (actual) browser_window_schedule_reformat(actual->bw);
    }
    InvalidateRect(h, NULL, 0);
    return 0;
  }
  case WM_LBUTTONDOWN: clic(x, y); return 0;
  case WM_LBUTTONUP: suelta(x, y); return 0;
  case WM_MOUSEMOVE: mueve(x, y, w); return 0;
  case WM_MOUSEWHEEL: {
    POINT p = { x, y };
    (void)p;
    rueda((short)(w >> 16) / 120, x, y);
    return 0;
  }
  case WM_KEYDOWN: tecla(w); return 0;
  case WM_CHAR: letra(w); return 0;
  case WM_CLOSE: salir = 1; return 0;
  case WM_DESTROY: salir = 1; return 0;
  }
  return DefWindowProcA(h, m, w, l);
}

/* =============================================================================
 *  NAVEGA2.INI, las opciones de NetSurf y el arranque
 * ============================================================================= */
static nserror opciones(struct nsoption_s *d) {
  (void)d;
  nsoption_set_bool(core_select_menu, true);
  nsoption_set_bool(enable_javascript, false);
  nsoption_set_int(memory_cache_size, 2 * 1024 * 1024);
  nsoption_set_int(max_fetchers, 8);
  nsoption_set_int(max_fetchers_per_host, 4);
  nsoption_set_int(max_cached_fetch_handles, 2);
  nsoption_set_int(font_size, 120);
  nsoption_set_int(font_min_size, 80);
  nsoption_set_bool(animate_images, false);
  return NSERROR_OK;
}
static void lee_ini(void) {
  char n[300], l[500];
  FILE *f;
  snprintf(n, sizeof n, "%s\\NAVEGA2.INI", surf_carpeta);
  f = fopen(n, "r");
  if (!f) return;
  while (fgets(l, sizeof l, f)) {
    char *e = l + strlen(l);
    while (e > l && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
    if (!strncasecmp(l, "Inicio=", 7) && l[7]) snprintf(inicio, sizeof inicio, "%s", l + 7);
    if (!strncasecmp(l, "Idioma=", 7) && l[7]) snprintf(idioma, sizeof idioma, "%s", l + 7);
  }
  fclose(f);
}
static void muere(const char *t) {
  MessageBoxA(NULL, t, "Navegador completo", 0x10);
  ExitProcess(1);
}

void inicio_prog(void) {
  WNDCLASSA wc;
  MSG m;
  HANDLE inst = GetModuleHandleA(NULL);
  const char *cmd = GetCommandLineA();
  char b[400], *argv[4];
  static char regi[300], regns[300];
  int w = GetSystemMetrics(0), h = GetSystemMetrics(1), argc = 1;
  struct netsurf_table tabla = {
    .misc = &tabla_varios, .window = &tabla_ventana, .clipboard = &tabla_papel, .file = NULL,
    .fetch = NULL, .utf8 = NULL, .bitmap = NULL, .layout = NULL,
  };
  nserror e;
  char *p;

  tabla.file = surf_tabla_archivos;
  tabla.fetch = surf_tabla_pedir;
  tabla.utf8 = surf_tabla_utf8;
  tabla.bitmap = surf_tabla_mapas;
  tabla.layout = surf_tabla_letras;
  GetModuleFileNameA(NULL, surf_carpeta, sizeof surf_carpeta);
  p = strrchr(surf_carpeta, '\\');
  if (p) *p = 0; else strcpy(surf_carpeta, ".");
  lee_ini();
  if (netsurf_register(&tabla) != NSERROR_OK) muere("No se ha podido preparar NetSurf.");
  /* el registro (para ver qué pasa): con SURF.LOG junto al programa */
  argv[0] = "navega2";
  snprintf(regi, sizeof regi, "%s\\SURF.LOG", surf_carpeta);
  {
    FILE *f = fopen(regi, "r");
    if (f) { fclose(f); registro = regi; argv[1] = "-V"; snprintf(regns, sizeof regns, "%s\\NS.LOG", surf_carpeta); argv[2] = regns; argc = 3; }
  }
  argv[argc] = NULL;
  nslog_init(NULL, &argc, argv);
  if (nsoption_init(opciones, &nsoptions, &nsoptions_default) != NSERROR_OK) muere("Faltan las opciones.");
  nsoption_set_charp(accept_language, strdup(idioma));
  snprintf(b, sizeof b, "%s\\RECURSOS\\MESSAGES", surf_carpeta);
  messages_add_from_file(b);
  snprintf(b, sizeof b, "%s\\FUENTES", surf_carpeta);
  if (!letra_inicia(b)) muere("No se encuentran las letras (FUENTES\\SANS.TTF junto al programa).");
  e = netsurf_init(NULL);
  if (e != NSERROR_OK) {
    snprintf(b, sizeof b, "NetSurf no ha podido empezar: %s", messages_get_errorcode(e));
    muere(b);
  }
  rh_inicia(idioma);
  snprintf(galletas, sizeof galletas, "%s\\SURFGAL.TXT", surf_carpeta);
  rh_galletas_carga(galletas);

  memset(&wc, 0, sizeof wc);
  wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = "HispaSurf";
  RegisterClassA(&wc);
  if (w > 1024) w = 1024;
  if (h > 768) h = 768;
  vent = CreateWindowExA(0, "HispaSurf", "Navegador completo", WS_OVERLAPPEDWINDOW, 8, 30, w - 16, h - 44, NULL, NULL, inst, NULL);
  if (!vent) muere("No se puede abrir la ventana.");
  ShowWindow(vent, 3);
  coloca();
  nuevo_lienzo();
  UpdateWindow(vent);

  /* la dirección de la línea de órdenes, si la hay; si no, el inicio */
  if (*cmd == '"') { for (cmd++; *cmd && *cmd != '"'; cmd++) ; if (*cmd) cmd++; }
  else while (*cmd && *cmd != ' ') cmd++;
  while (*cmd == ' ') cmd++;
  {
    nsurl *u = NULL;
    struct browser_window *bw;
    char d[1100];
    if (*cmd) {
      snprintf(dir, sizeof dir, "%s", cmd);
      n_dir = strlen(dir);
      if (dir[1] == ':') netsurf_path_to_nsurl(dir, &u);
      else {
        char *u8 = NULL;
        utf8_from_enc(dir, "WINDOWS-1252", 0, &u8, NULL);
        snprintf(d, sizeof d, "%s", u8 ? u8 : dir);
        free(u8);
        if (nsurl_create(d, &u) != NSERROR_OK) { snprintf(b, sizeof b, "http://%s", d); nsurl_create(b, &u); }
      }
    } else nsurl_create(inicio, &u);
    if (!u || browser_window_create(BW_CREATE_HISTORY, u, NULL, NULL, &bw) != NSERROR_OK)
      muere("No se puede abrir la primera p\341gina.");
    nsurl_unref(u);
  }

  while (!salir) {
    int t;
    while (PeekMessageA(&m, NULL, 0, 0, 1)) {
      if (m.message == WM_QUIT) { salir = 1; break; }
      TranslateMessage(&m);
      DispatchMessageA(&m);
    }
    if (salir) break;
    t = schedule_run();
    pinta_sucio();
    if (salir) break;
    if (t == 0) continue;
    if (t < 0 || t > 500) t = 500;
    SetTimer(vent, 1, t, NULL);
    if (!GetMessageA(&m, NULL, 0, 0)) break;
    KillTimer(vent, 1);
    if (m.message != WM_TIMER) { TranslateMessage(&m); DispatchMessageA(&m); }
  }
  rh_galletas_guarda(galletas);
  /* (netsurf_exit libera todo; en un 486 tarda: al cerrar, se sale ya) */
  red_fin();
  ExitProcess(0);
}
