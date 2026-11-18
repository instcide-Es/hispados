/* =============================================================================
 *  NAVEGA.EXE - el Navegador de Hispa-486: un navegador web sencillo para
 *  Hispa Ventanas (programa Hispa32 con ventana). docs/486/NAVEGADOR.md
 *  Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 *
 *  Aquí, la ventana: la barra (Atrás, Adelante, Recargar, Inicio, la
 *  dirección), la página (pagina.c la deja en una rejilla de letras; aquí se
 *  pinta y se maneja con el ratón y el teclado), los campos de formulario,
 *  la barra de desplazamiento, el historial y la carga (http.c) sin dejar
 *  parada la ventana. Todo se dibuja aquí con lo básico de GDI (rectángulos y
 *  texto), con los colores de Hispa Ventanas.
 *  Los textos, en la página 1252 (la de los programas Hispa32).
 * ============================================================================= */
#include "nav.h"
#include "dom.h"                 /* [JS] */

/* ---- lo justo de USER32, GDI32 y KERNEL32 ---- */
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { HANDLE hwnd; u32 message, wParam, lParam, time; POINT pt; } MSG;
typedef int WINAPI (*WNDPROC)(HANDLE, u32, u32, u32);
typedef struct {
  u32 style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance, hIcon, hCursor, hbrBackground;
  const char *lpszMenuName, *lpszClassName;
} WNDCLASSA;
typedef struct { HANDLE hdc; int fErase; RECT rcPaint; int fRestore, fIncUpdate; char rgbReserved[32]; } PAINTSTRUCT;
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ERASEBKGND 0x0014
#define WM_TIMER 0x0113
#define WM_KEYDOWN 0x0100
#define WM_CHAR 0x0102
#define WM_TIMER 0x0113
#define WM_MOUSEMOVE 0x0200
#define WM_LBUTTONDOWN 0x0201
#define WM_LBUTTONUP 0x0202
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define VK_BACK 8
#define VK_TAB 9
#define VK_RETURN 13
#define VK_ESCAPE 27
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
#define MB_YESNO 4
#define MB_OK 0
#define MB_ICONQUESTION 0x20
#define IDYES 6
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
void WINAPI TlsAzar(const void *, int);
u32 WINAPI GetTickCount(void);
u32 WINAPI SetTimer(HANDLE, u32, u32, void *);
int WINAPI KillTimer(HANDLE, u32);
short WINAPI GetKeyState(int);
u32 WINAPI WinExec(const char *, u32);
typedef struct { u32 biSize; int biWidth, biHeight; u16 biPlanes, biBitCount; u32 biCompression, biSizeImage;
                 int biXPelsPerMeter, biYPelsPerMeter; u32 biClrUsed, biClrImportant; u16 colores[256]; } BMI;
int WINAPI SetDIBitsToDevice(HANDLE, int, int, u32, u32, int, int, u32, u32, const void *, const BMI *, u32);
typedef struct {
  u32 lStructSize; HANDLE hwndOwner, hInstance; const char *lpstrFilter; char *lpstrCustomFilter; u32 nMaxCustFilter, nFilterIndex;
  char *lpstrFile; u32 nMaxFile; char *lpstrFileTitle; u32 nMaxFileTitle; const char *lpstrInitialDir, *lpstrTitle;
  u32 Flags; u16 nFileOffset, nFileExtension; const char *lpstrDefExt; u32 lCustData; void *lpfnHook; const char *lpTemplateName;
} OPENFILENAMEA;
int WINAPI GetOpenFileNameA(OPENFILENAMEA *);
int WINAPI GetSaveFileNameA(OPENFILENAMEA *);

/* ---- medidas (puntos) y colores (la paleta de Hispa Ventanas) ---- */
#define LW 8                   /* una letra */
#define LH 18                  /* una fila de la página */
#define BARRA_H 54             /* las dos filas de arriba: botones y dirección */
#define ESTADO_H 20
#define DESP_W 16              /* la barra de desplazamiento */
#define P(n) (0x01000000u | (n))
enum { P_TINTA, P_MARRON, P_PETROLEO, P_PETROLEO_CL, P_TERRACOTA, P_TERRACOTA_CL, P_ARENA, P_ARENA_OSC, P_CREMA,
       P_BLANCO, P_OCRE, P_OLIVA, P_ANIL, P_GRANATE, P_TURQUESA, P_PIEDRA };

static HANDLE vent;
static int an, al;             /* la zona de la ventana */
static int cols, filas_vis;    /* letras y filas de página que caben */
static Pagina *pag;
static int sx, sy;             /* desde dónde se ve la página (letras, filas) */
static int foco = -1;          /* en pag->foco; -1 nadie */
static int foco_dir;           /* 1: se escribe en la dirección */
static int dir_sel;            /* la dirección está elegida entera: lo que se escriba la sustituye */
static Ctl dir;                /* el campo de la dirección */
static char estado[200], sobre[200];   /* la línea de estado y el enlace de debajo del ratón */
static int sobre_enl = -1;
static Pet pet;
static int cargando, redirs, modo_hist, sy_tras;
static char inicio[300] = "about:inicio";
static int al_contenido = 1;
static char carpeta[150];
/* el historial */
#define HIST 40
static struct { char *url; int sy; } hist[HIST];
static int n_hist, i_hist = -1;
/* la lista desplegada de un <select> */
static struct { int activa, ctl, sel, top, x, y, w, vis; } lista;
static int arrastra;           /* se arrastra el cursor de la barra de desplazamiento */
/* pinceles: uno por color */
static HANDLE pinceles[80];
static u32 pincel_color[80];
static int n_pinceles;
/* las imágenes: la que se está bajando, la petición y lo ya visto (para no
 * bajar otra vez el logotipo en cada página) */
static Pet ipet;
static int icarga = -1, iredirs, i_remaq, i_hechas;
static u32 i_remaq_t;
#define CACHE_N 16
#define CACHE_MAX (320u << 10)
static struct { char *url; int pw, ph, w, h; u8 *px; u32 uso; } cache[CACHE_N];
static u32 cache_uso;
/* el refresco de <meta http-equiv=refresh>, y lo de las descargas */
static char *refresco;
static char dir_descargas[150], dir_hispados[150];
#define T_REFRESCO 1
static void img_cancela(void);
static void remaqueta(void);
static void guarda_galletas(void);

static HANDLE pincel(u32 c) {
  int k;
  for (k = 0; k < n_pinceles; k++) if (pincel_color[k] == c) return pinceles[k];
  if (n_pinceles == 80) { for (k = 16; k < 80; k++) DeleteObject(pinceles[k]); n_pinceles = 16; }
  pincel_color[n_pinceles] = c;
  pinceles[n_pinceles] = CreateSolidBrush(c);
  return pinceles[n_pinceles++];
}
static void rect(HANDLE dc, int x, int y, int w, int h, u32 c) {
  RECT r;
  if (w <= 0 || h <= 0) return;
  r.left = x; r.top = y; r.right = x + w; r.bottom = y + h;
  FillRect(dc, &r, pincel(c));
}
static void marco(HANDLE dc, int x, int y, int w, int h, u32 c) {
  rect(dc, x, y, w, 1, c); rect(dc, x, y + h - 1, w, 1, c);
  rect(dc, x, y, 1, h, c); rect(dc, x + w - 1, y, 1, h, c);
}
static void txt(HANDLE dc, int x, int y, const char *t, int n, u32 c, int negrita) {
  if (n < 0) n = strlen(t);
  if (n <= 0) return;
  SetTextColor(dc, c);
  TextOutA(dc, x, y, t, n);
  if (negrita) TextOutA(dc, x + 1, y, t, n);
}

/* =============================================================================
 *  La barra de arriba y la de estado
 * ============================================================================= */
enum { B_ATRAS, B_ADELANTE, B_RECARGA, B_INICIO, B_IR, B_N };
static const char *const b_texto[B_N] = { "< Atr\341s", "Adelante >", "Recargar", "Inicio", "Ir" };
static RECT b_rect[B_N];
static RECT dir_rect;
static void coloca(void) {
  RECT r;
  int k, x = 6;
  GetClientRect(vent, &r);
  an = r.right; al = r.bottom;
  cols = (an - DESP_W) / LW;
  filas_vis = (al - BARRA_H - ESTADO_H) / LH;
  if (cols < 20) cols = 20;
  if (filas_vis < 1) filas_vis = 1;
  for (k = 0; k < 4; k++) {
    int w = (strlen(b_texto[k]) + 2) * LW;
    b_rect[k].left = x; b_rect[k].top = 4; b_rect[k].right = x + w; b_rect[k].bottom = 26;
    x += w + 6;
  }
  b_rect[B_IR].right = an - 6; b_rect[B_IR].left = an - 6 - 4 * LW; b_rect[B_IR].top = 29; b_rect[B_IR].bottom = 51;
  dir_rect.left = 6 + 11 * LW; dir_rect.top = 29; dir_rect.right = b_rect[B_IR].left - 6; dir_rect.bottom = 51;
}
static int b_activo(int k) {
  if (k == B_ATRAS) return i_hist > 0;
  if (k == B_ADELANTE) return i_hist >= 0 && i_hist < n_hist - 1;
  return 1;
}
static int dir_vis(void) { return (dir_rect.right - dir_rect.left) / LW - 2; }
static void pinta_dir(HANDLE dc) {
  int vis = dir_vis(), n;
  rect(dc, dir_rect.left, dir_rect.top, dir_rect.right - dir_rect.left, 22, P(P_BLANCO));
  marco(dc, dir_rect.left, dir_rect.top, dir_rect.right - dir_rect.left, 22, P(foco_dir ? P_TERRACOTA : P_TINTA));
  if (dir.cursor < dir.desp) dir.desp = dir.cursor;
  if (dir.cursor > dir.desp + vis - 1) dir.desp = dir.cursor - vis + 1;
  if (dir.desp < 0) dir.desp = 0;
  n = dir.n_valor - dir.desp;
  if (n > vis) n = vis;
  if (foco_dir && dir_sel && n > 0) rect(dc, dir_rect.left + LW, dir_rect.top + 3, n * LW, 16, P(P_ANIL));
  txt(dc, dir_rect.left + LW, dir_rect.top + 3, dir.valor + dir.desp, n, P(foco_dir && dir_sel ? P_BLANCO : P_TINTA), 0);
  if (foco_dir && !dir_sel) rect(dc, dir_rect.left + LW + (dir.cursor - dir.desp) * LW, dir_rect.top + 3, 2, 16, P(P_TERRACOTA));
}
static void pinta_barra(HANDLE dc) {
  int k;
  rect(dc, 0, 0, an, BARRA_H - 1, P(P_PIEDRA));
  rect(dc, 0, BARRA_H - 1, an, 1, P(P_ARENA_OSC));
  SetBkMode(dc, 1);
  for (k = 0; k < B_N; k++) {
    RECT *r = &b_rect[k];
    rect(dc, r->left, r->top, r->right - r->left, r->bottom - r->top, P(P_ARENA));
    marco(dc, r->left, r->top, r->right - r->left, r->bottom - r->top, P(P_TINTA));
    txt(dc, r->left + LW, r->top + 3, b_texto[k], -1, P(b_activo(k) ? P_TINTA : P_ARENA_OSC), 0);
  }
  txt(dc, 6, 32, "Direcci\363n:", -1, P(P_TINTA), 0);
  pinta_dir(dc);
}
static void pinta_estado(HANDLE dc) {
  int y = al - ESTADO_H, n, seguro = pag && !_strnicmp(pag->url, "https:", 6);
  const char *t = sobre[0] ? sobre : estado, *d = "";
  rect(dc, 0, y, an, ESTADO_H, P(P_PIEDRA));
  rect(dc, 0, y, an, 1, P(P_ARENA_OSC));
  SetBkMode(dc, 1);
  if (pag && !cargando && !_strnicmp(pag->url, "http", 4)) d = seguro ? "Conexi\363n cifrada" : "Sin cifrar";
  n = strlen(t);
  if (n > (an - 12) / LW - (int)strlen(d) - 2) n = (an - 12) / LW - strlen(d) - 2;
  txt(dc, 6, y + 2, t, n, P(P_TINTA), 0);
  txt(dc, an - 6 - strlen(d) * LW, y + 2, d, -1, P(seguro ? P_OLIVA : P_GRANATE), 0);
}
static void di(const char *t) {
  HANDLE dc;
  if (!strcmp(estado, t)) return;
  { int k; for (k = 0; t[k] && k < 198; k++) estado[k] = t[k]; estado[k] = 0; }
  if (!vent) return;
  dc = GetDC(vent);
  pinta_estado(dc);
  ReleaseDC(vent, dc);
}

/* =============================================================================
 *  Pintar la página
 * ============================================================================= */
static u32 color_de(int k) { return pag->color[k ? k : K_TINTA]; }
static Ctl *ctl_foco(void) {
  if (!pag || foco < 0 || foco >= pag->n_foco || pag->foco[foco].enlace) return NULL;
  return &pag->ctl[pag->foco[foco].n];
}
static int enl_foco(void) {
  if (!pag || foco < 0 || foco >= pag->n_foco || !pag->foco[foco].enlace) return -1;
  return pag->foco[foco].n;
}
static int es_texto(const Ctl *c) { return c && (c->tipo == C_TEXTO || c->tipo == C_CLAVE || c->tipo == C_AREA || c->tipo == C_ARCHIVO); }
#define ARCH_BOTON 12                /* «Examinar...» a la derecha del campo de subir un archivo */
/* el texto de un botón o de la opción elegida de una lista */
static void rotulo(Ctl *c, char *b, int max) {
  int n;
  b[0] = 0;
  if (c->tipo == C_LISTA) { if (c->n_opc) texto_de(c->opc[c->elegida], b, max); }
  else if (c->nodo->etq == E_BUTTON) texto_de(c->nodo, b, max);
  else { n = c->n_valor < max - 1 ? c->n_valor : max - 1; memcpy(b, c->valor, n); b[n] = 0; }
  if (c->tipo != C_LISTA) pag_rotulo_vacio(c, b, max);   /* [JS] */
  n = strlen(b);
  while (n && b[n - 1] == ' ') b[--n] = 0;
  if (b[0] == ' ') memmove(b, b + 1, n);
}
/* las líneas de un área de texto (se parten a su ancho y en los saltos).
 * cual >= 0: dónde empieza esa línea; cual == -1: cuántas hay. Con lin_pos,
 * además, en qué línea cae la posición pos */
static int area_linea(Ctl *c, int cual, int pos, int *lin_pos) {
  int an_l = c->w - 2, k, l = 0, col = 0;
  if (an_l < 1) an_l = 1;
  if (lin_pos) *lin_pos = 0;
  if (cual == 0 && !lin_pos) return 0;
  for (k = 0; k < c->n_valor; k++) {
    if (col == an_l) { l++; col = 0; if (l == cual) return k; }
    if (lin_pos && k == pos) *lin_pos = l;
    if (c->valor[k] == '\n') { l++; col = 0; if (l == cual) return k + 1; }
    else col++;
  }
  if (lin_pos && pos >= c->n_valor) *lin_pos = l;
  return cual == -1 ? l + 1 : c->n_valor;
}
static void pinta_ctl(HANDLE dc, Ctl *c) {
  int x = (c->x - sx) * LW, y = BARRA_H + (c->y - sy) * LH, w = c->w * LW, h = c->h * LH, n, k;
  int tiene = ctl_foco() == c;
  char b[300];
  u32 borde = P(tiene ? P_TERRACOTA : P_TINTA);
  if (y + h <= BARRA_H || y >= al - ESTADO_H || x + w <= 0 || x >= an - DESP_W) return;
  SetBkMode(dc, 1);
  switch (c->tipo) {
  case C_TEXTO: case C_CLAVE: case C_ARCHIVO: {
    int vis = c->w - 2;
    const char *ph = atr(c->nodo, "placeholder"), *tp = atr(c->nodo, "type");
    if (c->tipo == C_ARCHIVO) {                /* el campo de la ruta y el botón de elegirla */
      int bw = ARCH_BOTON * LW;
      w -= bw + LW;
      vis = c->w - ARCH_BOTON - 3;
      rect(dc, x + w + LW, y, bw, h, P(P_ARENA));
      marco(dc, x + w + LW, y, bw, h, P(P_TINTA));
      txt(dc, x + w + LW + LW, y + 1, "Examinar...", 11, P(c->desactivado ? P_ARENA_OSC : P_TINTA), 0);
      ph = "(ning\372n archivo)";
    }
    if (!ph && tp && !_stricmp(tp, "date")) ph = "aaaa-mm-dd";
    rect(dc, x, y, w, h, P(c->desactivado || c->sololee ? P_CREMA : P_BLANCO));
    marco(dc, x, y, w, h, borde);
    if (tiene) marco(dc, x - 1, y - 1, w + 2, h + 2, borde);
    if (c->cursor < c->desp) c->desp = c->cursor;
    if (c->cursor > c->desp + vis - 1) c->desp = c->cursor - vis + 1;
    if (c->desp < 0) c->desp = 0;
    n = c->n_valor - c->desp;
    if (n > vis) n = vis;
    if (!c->n_valor && ph) { n = strlen(ph); txt(dc, x + LW, y + 1, ph, n > vis ? vis : n, P(P_ARENA_OSC), 0); }
    else if (c->tipo == C_CLAVE) { for (k = 0; k < n && k < 299; k++) b[k] = '*'; txt(dc, x + LW, y + 1, b, n, P(P_TINTA), 0); }
    else txt(dc, x + LW, y + 1, c->valor + c->desp, n, P(c->desactivado ? P_ARENA_OSC : P_TINTA), 0);
    if (tiene) rect(dc, x + LW + (c->cursor - c->desp) * LW, y + 1, 2, 16, P(P_TERRACOTA));
    break;
  }
  case C_AREA: {
    int lc, total = area_linea(c, -1, 0, NULL), l;
    rect(dc, x, y, w, h, P(P_BLANCO));
    marco(dc, x, y, w, h, borde);
    if (tiene) marco(dc, x - 1, y - 1, w + 2, h + 2, borde);
    area_linea(c, -2, c->cursor, &lc);
    if (lc < c->desp) c->desp = lc;
    if (lc > c->desp + c->h - 1) c->desp = lc - c->h + 1;
    for (l = c->desp; l < c->desp + c->h && l < total; l++) {
      int ini = area_linea(c, l, 0, NULL), fin = l + 1 < total ? area_linea(c, l + 1, 0, NULL) : c->n_valor;
      if (fin > ini && c->valor[fin - 1] == '\n') fin--;
      txt(dc, x + LW, y + (l - c->desp) * LH + 1, c->valor + ini, fin - ini, P(P_TINTA), 0);
      if (tiene && l == lc) rect(dc, x + LW + (c->cursor - ini) * LW, y + (l - c->desp) * LH + 1, 2, 16, P(P_TERRACOTA));
    }
    break;
  }
  case C_CASILLA: case C_RADIO:
    rect(dc, x + 5, y + 2, 14, 14, P(P_BLANCO));
    marco(dc, x + 5, y + 2, 14, 14, borde);
    if (tiene) marco(dc, x + 4, y + 1, 16, 16, borde);
    if (c->marcado) {
      if (c->tipo == C_RADIO) rect(dc, x + 9, y + 6, 6, 6, P(P_TINTA));
      else for (k = 0; k < 8; k++) { rect(dc, x + 8 + k, y + 5 + k, 2, 1, P(P_TINTA)); rect(dc, x + 15 - k, y + 5 + k, 2, 1, P(P_TINTA)); }
    }
    break;
  case C_ENVIAR: case C_BOTON: case C_REINICIO:
    rotulo(c, b, sizeof b);
    rect(dc, x, y, w, h, P(P_ARENA));
    marco(dc, x, y, w, h, P(P_TINTA));
    if (tiene) marco(dc, x + 2, y + 2, w - 4, h - 4, P(P_TERRACOTA));
    n = strlen(b);
    if (n > c->w - 2) n = c->w - 2;
    txt(dc, x + (w - n * LW) / 2, y + 1, b, n, P(c->desactivado || c->solo_js ? P_ARENA_OSC : P_TINTA), 0);
    break;
  case C_LISTA:
    rotulo(c, b, sizeof b);
    rect(dc, x, y, w, h, P(c->desactivado ? P_CREMA : P_BLANCO));
    rect(dc, x + w - 2 * LW, y, 2 * LW, h, P(P_ARENA));
    marco(dc, x, y, w, h, borde);
    if (tiene) marco(dc, x - 1, y - 1, w + 2, h + 2, borde);
    n = strlen(b);
    if (n > c->w - 4) n = c->w - 4;
    txt(dc, x + LW, y + 1, b, n, P(c->desactivado ? P_ARENA_OSC : P_TINTA), 0);
    txt(dc, x + w - 2 * LW + 4, y + 1, "v", 1, P(P_TINTA), 0);
    break;
  }
}
static void pinta_fila(HANDLE dc, int f) {
  int y = BARRA_H + (f - sy) * LH, x0 = sx, x1 = sx + cols, k, ef = enl_foco();
  Fila *fl = f < pag->n_filas ? &pag->filas[f] : NULL;
  rect(dc, 0, y, an - DESP_W, LH, P(P_BLANCO));
  if (!fl || !fl->n) return;
  if (x1 > fl->n) x1 = fl->n;
  SetBkMode(dc, 1);
  for (k = x0; k < x1; ) {
    int ini = k, a = fl->a[k], x, w, blanco = 1, j;
    Atr *at = &pag->atrs[a];
    u32 fg, bg;
    while (k < x1 && fl->a[k] == a) { if (fl->c[k] != ' ') blanco = 0; k++; }
    if (at->ctl >= 0) continue;                 /* (un control: se pinta aparte) */
    x = (ini - sx) * LW; w = (k - ini) * LW;
    if (at->img >= 0) {                         /* (una imagen: se pinta aparte; aquí, su fondo) */
      bg = at->enlace >= 0 && at->enlace == ef ? P(P_ANIL) : at->bg ? pag->color[at->bg] : P(P_BLANCO);
      if (bg != P(P_BLANCO)) rect(dc, x, y, w, LH, bg);
      continue;
    }
    fg = color_de(at->fg);
    bg = at->bg ? pag->color[at->bg] : P(P_BLANCO);
    if (at->enlace >= 0 && at->enlace == ef) { bg = P(P_ANIL); fg = P(P_BLANCO); }   /* el enlace con el foco */
    else if (at->est & A_BOTON) bg = P(P_CREMA);
    if (bg != P(P_BLANCO)) rect(dc, x, y, w, LH, bg);
    if (at->est & A_RAYA) { rect(dc, x, y + LH / 2, w, 1, fg); continue; }
    if (!blanco) {
      /* (sin los espacios de las puntas: menos que pintar) */
      j = k;
      while (j > ini && fl->c[j - 1] == ' ') j--;
      txt(dc, x, y + 1, (const char *)fl->c + ini, j - ini, fg, at->est & A_NEGRITA);
    }
    if (at->est & A_SUBRAYA) rect(dc, x, y + 17, w, 1, fg);    /* (debajo de la letra, que ocupa de y+1 a y+16) */
    if (at->est & A_TACHA) rect(dc, x, y + 9, w, 1, fg);
  }
}
static void pinta_desp(HANDLE dc) {
  int x = an - DESP_W, y0 = BARRA_H, h = al - BARRA_H - ESTADO_H, total = pag ? pag->n_filas : 0, ph, py;
  rect(dc, x, y0, DESP_W, h, P(P_CREMA));
  rect(dc, x, y0, 1, h, P(P_ARENA_OSC));
  if (total <= filas_vis || h < 40) return;
  ph = h * filas_vis / total;
  if (ph < 16) ph = 16;
  py = y0 + (h - ph) * sy / (total - filas_vis);
  rect(dc, x + 2, py, DESP_W - 3, ph, P(P_ARENA));
  marco(dc, x + 2, py, DESP_W - 3, ph, P(P_TINTA));
}
static void pinta_lista(HANDLE dc) {
  Ctl *c = &pag->ctl[lista.ctl];
  int k;
  char b[300];
  rect(dc, lista.x, lista.y, lista.w, lista.vis * LH + 2, P(P_BLANCO));
  marco(dc, lista.x, lista.y, lista.w, lista.vis * LH + 2, P(P_TINTA));
  SetBkMode(dc, 1);
  for (k = 0; k < lista.vis && lista.top + k < c->n_opc; k++) {
    int o = lista.top + k, n;
    b[0] = 0;
    texto_de(c->opc[o], b, sizeof b);
    n = strlen(b);
    while (n && b[n - 1] == ' ') n--;
    if (n > lista.w / LW - 2) n = lista.w / LW - 2;
    if (o == lista.sel) rect(dc, lista.x + 1, lista.y + 1 + k * LH, lista.w - 2, LH, P(P_ANIL));
    txt(dc, lista.x + LW, lista.y + 2 + k * LH, b[0] == ' ' ? b + 1 : b, n, P(o == lista.sel ? P_BLANCO : P_TINTA), 0);
  }
}
/* las imágenes que se ven; las que aún no han llegado (con su sitio
 * guardado), un marco */
static void pinta_imgs(HANDLE dc, int y0, int y1) {
  static BMI bmi;
  int k, ab = al - ESTADO_H, de = an - DESP_W;
  if (!bmi.biSize) { bmi.biSize = 40; bmi.biPlanes = 1; bmi.biBitCount = 8; for (k = 0; k < 256; k++) bmi.colores[k] = (u16)k; }
  for (k = 0; k < pag->n_img; k++) {
    Img *im = &pag->img[k];
    int px, py, xs, ys, w, h;
    if (im->y < 0 || !im->cw || im->y + im->ch <= sy || im->y >= sy + filas_vis + 1) continue;
    px = (im->x - sx) * LW;
    py = BARRA_H + (im->y - sy) * LH;
    if (py >= y1 || py + im->ch * LH <= y0) continue;
    if (im->estado != IM_LISTA || !im->px) {
      if (im->estado != IM_FALLO && px < de && py + 2 < ab) {
        int ww = im->cw * LW, hh = im->ch * LH - 2;
        if (px + ww > de) ww = de - px;
        if (py < BARRA_H) { hh -= BARRA_H - py; py = BARRA_H; }
        if (py + hh > ab) hh = ab - py;
        marco(dc, px, py + 1, ww, hh, P(P_ARENA));
      }
      continue;
    }
    /* (dentro de su bloque: si la columna es más estrecha que la imagen, se corta) */
    w = im->w < im->cw * LW ? im->w : im->cw * LW;
    h = im->h < im->ch * LH ? im->h : im->ch * LH;
    py += (im->ch * LH - h) / 2;
    xs = px < 0 ? -px : 0;
    ys = py < BARRA_H ? BARRA_H - py : 0;
    w -= xs; h -= ys;
    if (px + xs + w > de) w = de - px - xs;
    if (py + ys + h > ab) h = ab - py - ys;
    if (w <= 0 || h <= 0) continue;
    bmi.biWidth = im->w; bmi.biHeight = -im->h;
    SetDIBitsToDevice(dc, px + xs, py + ys, w, h, xs, ys, 0, im->h, im->px, &bmi, 1);
  }
}
static void pinta_pagina(HANDLE dc, int y0, int y1) {
  int f, k;
  if (!pag) { rect(dc, 0, BARRA_H, an - DESP_W, al - BARRA_H - ESTADO_H, P(P_BLANCO)); pinta_desp(dc); return; }
  for (f = sy; f < sy + filas_vis; f++) {
    int y = BARRA_H + (f - sy) * LH;
    if (y + LH <= y0 || y >= y1) continue;
    pinta_fila(dc, f);
  }
  /* lo que sobra por abajo (menos de una fila) */
  k = BARRA_H + filas_vis * LH;
  if (k < al - ESTADO_H) rect(dc, 0, k, an - DESP_W, al - ESTADO_H - k, P(P_BLANCO));
  pinta_imgs(dc, y0, y1);
  for (k = 0; k < pag->n_ctl; k++) {
    Ctl *c = &pag->ctl[k];
    if (!c->visible || c->y + c->h <= sy || c->y >= sy + filas_vis) continue;
    pinta_ctl(dc, c);
  }
  pinta_desp(dc);
  if (lista.activa) pinta_lista(dc);
}
static void repinta(void) { InvalidateRect(vent, NULL, 0); }
static void repinta_ctl(Ctl *c) {
  HANDLE dc = GetDC(vent);
  pinta_ctl(dc, c);
  ReleaseDC(vent, dc);
}
static void repinta_pagina(void) {
  RECT r;
  r.left = 0; r.top = BARRA_H; r.right = an; r.bottom = al - ESTADO_H;
  InvalidateRect(vent, &r, 0);
}
static void desplaza(int nx, int ny) {
  int my = pag ? pag->n_filas - filas_vis : 0, mx = pag ? pag->ancho - cols : 0;
  if (ny > my) ny = my;
  if (ny < 0) ny = 0;
  if (nx > mx) nx = mx;
  if (nx < 0) nx = 0;
  if (nx == sx && ny == sy) return;
  sx = nx; sy = ny;
  repinta_pagina();
}

/* =============================================================================
 *  Cargar páginas
 * ============================================================================= */
static void dir_pon(const char *u) {
  ctl_pon(&dir, u, -1);
  dir.cursor = dir.n_valor; dir.desp = 0;
}
static void muestra(Pagina *p, const char *url_hist) {
  char t[260];
  int seg;
  if (!p) return;
  lista.activa = 0;
  img_cancela();
  if (pag) { js_suelta(); pag_suelta(pag); }   /* [JS] lo de JavaScript de la página de antes, fuera */
  pag = p;
  pag_maqueta(pag, cols);
  sx = 0; sy = 0; foco = -1; foco_dir = 0; sobre[0] = 0; sobre_enl = -1;
  if (url_hist) {
    if (modo_hist == 0) {                       /* una entrada nueva, tras la de ahora */
      int k;
      for (k = i_hist + 1; k < n_hist; k++) free(hist[k].url);
      n_hist = i_hist + 1;
      if (n_hist == HIST) { free(hist[0].url); memmove(hist, hist + 1, (HIST - 1) * sizeof hist[0]); n_hist--; }
      hist[n_hist].url = malloc(strlen(url_hist) + 1);
      if (hist[n_hist].url) { strcpy(hist[n_hist].url, url_hist); hist[n_hist].sy = 0; i_hist = n_hist++; }
      /* se empieza por lo principal de la página (<main>), si lo dice: lo de
       * antes (menús, avisos) queda arriba, a un paso */
      if (al_contenido && pag->y_cont > 3) desplaza(0, pag->y_cont);
    } else if (i_hist >= 0) {
      char *q = malloc(strlen(url_hist) + 1);
      if (q) { strcpy(q, url_hist); free(hist[i_hist].url); hist[i_hist].url = q; }
      desplaza(0, sy_tras);
    }
    dir_pon(url_hist);
  }
  sprintf(t, "%.200s - Navegador", pag->doc.titulo && pag->doc.titulo[0] ? pag->doc.titulo : url_hist ? url_hist : "Navegador");
  SetWindowTextA(vent, t);
  estado[0] = 0;
  js_pagina(pag);                               /* [JS] sus <script> */
  repinta();
  /* <meta http-equiv="refresh">: a la dirección que diga, pasado el tiempo */
  free(refresco); refresco = NULL;
  KillTimer(vent, T_REFRESCO);
  if (url_hist && (seg = pag_refresco(pag, &refresco)) >= 0) {
    if (!refresco) { refresco = malloc(strlen(pag->url) + 1); if (refresco) strcpy(refresco, pag->url); }
    if (refresco) SetTimer(vent, T_REFRESCO, seg ? seg * 1000 : 50, NULL);
  }
}
/* una página hecha aquí: el inicio, los avisos */
static void pagina_propia(const char *url, const char *html) {
  muestra(pag_nueva(url, (const u8 *)html, strlen(html), 1, 0), url);
}
static void pagina_error(const char *url, const char *porque) {
  static char h[1400];
  sprintf(h, "<title>No se ha podido abrir</title><h1>No se ha podido abrir la p\341gina</h1><p>%.600s</p>"
             "<p><b>Direcci\363n:</b> %.400s</p><p>Puede probar otra vez con <b>Recargar</b>.</p>", porque, url);
  pagina_propia(url, h);
}
static void pagina_inicio(void) {
  static char h[6000];
  char l[400], ruta[200];
  FILE *f;
  int n;
  n = sprintf(h, "<title>Inicio</title><h1>Navegador de Hispa-486</h1>"
    "<p>Escriba una direcci\363n arriba y pulse Intro, o elija una de estas:</p><ul>");
  sprintf(ruta, "%sNAVEGA.INI", carpeta);
  if ((f = fopen(ruta, "r"))) {
    while (fgets(l, sizeof l, f) && n < 4500) {
      char *b = strchr(l, '|'), *fin;
      if (_strnicmp(l, "Favorito=", 9) || !b) continue;
      *b++ = 0;
      for (fin = b; *fin && *fin != '\r' && *fin != '\n'; fin++) ;
      *fin = 0;
      n += sprintf(h + n, "<li><a href=\"%.200s\">%.100s</a></li>", b, l + 9);
    }
    fclose(f);
  }
  n += sprintf(h + n, "<li><a href=\"https://clicgest.es/login\">ClicGest: entrar</a></li>"
    "<li><a href=\"https://clicgest.es/demo\">ClicGest: probar la demostraci\363n</a></li></ul>"
    "<h3>C\363mo se maneja</h3><ul>"
    "<li><b>Tab</b> pasa al enlace o al campo siguiente; <b>Intro</b> lo abre o env\355a el formulario.</li>"
    "<li>Las <b>flechas</b>, <b>Re P\341g</b> y <b>Av P\341g</b> mueven la p\341gina; <b>Retroceso</b> vuelve atr\341s.</li>"
    "<li><b>F6</b> lleva a la direcci\363n, <b>F5</b> recarga y <b>Esc</b> para la carga.</li>"
    "<li><b>May\372s+Tab</b> vuelve al anterior. Los archivos que no son p\341ginas (un PDF) se guardan en el disco.</li></ul>"
    "<p><small>Navegador " NAV_VERSION ". Entiende HTML sencillo, formularios (tambi\351n los de subir archivos), im\341genes PNG, GIF y JPEG "
    "y JavaScript (se apaga con JavaScript=NO en NAVEGA.INI). Las conexiones https van cifradas con TLS 1.2 y se comprueba el certificado del servidor. "
    "Las galletas que lo piden se guardan al cerrar, para no tener que entrar cada vez.</small></p>");
  pagina_propia("about:inicio", h);
}
/* ¿son del mismo sitio? (para SameSite: lo mismo de los dos últimos
 * trozos del nombre del servidor, «clicgest.es») */
static const char *sitio_de(const char *u, int *n) {
  const char *a = strstr(u, "://"), *f, *p1 = NULL, *p2 = NULL, *q;
  *n = 0;
  if (!a) return NULL;
  a += 3;
  for (f = a; *f && *f != '/' && *f != ':' && *f != '?' && *f != '#'; f++) ;
  for (q = a; q < f; q++) if (*q == '.') { p2 = p1; p1 = q; }
  if (p2) a = p2 + 1;
  *n = f - a;
  return a;
}
int nav_cruzada(const char *de, const char *a) {
  int n1, n2;
  const char *s1, *s2;
  if (!de || _strnicmp(de, "http", 4)) return 0;
  s1 = sitio_de(de, &n1); s2 = sitio_de(a, &n2);
  return !s1 || !s2 || n1 != n2 || _strnicmp(s1, s2, n1);
}
static void ir2(const char *url, int post, char *cuerpo, u32 n_cuerpo, int modo, const char *tipo, const char *origen);
static void ir(const char *url, int post, char *cuerpo, u32 n_cuerpo, int modo) { ir2(url, post, cuerpo, n_cuerpo, modo, NULL, NULL); }
static void ir2(const char *url, int post, char *cuerpo, u32 n_cuerpo, int modo, const char *tipo, const char *origen) {
  char *u;
  if (cargando) { http_suelta(&pet); cargando = 0; }
  img_cancela();
  KillTimer(vent, T_REFRESCO);
  js_para_red();                                /* [JS] (la conexión es una: para la página) */
  lista.activa = 0;
  modo_hist = modo;
  if (!_strnicmp(url, "about:", 6)) {
    free(cuerpo);
    if (!_stricmp(url, "about:blank")) pagina_propia("about:blank", "<title>En blanco</title>");
    else pagina_inicio();
    return;
  }
  u = malloc(strlen(url) + 1);
  if (!u) { free(cuerpo); return; }
  strcpy(u, url);
  if (strchr(u, '#')) *strchr(u, '#') = 0;
  memset(&pet, 0, sizeof pet);
  pet.url = u; pet.post = post; pet.cuerpo = cuerpo; pet.n_cuerpo = n_cuerpo;
  if (tipo) sprintf(pet.tipo_cuerpo, "%.94s", tipo);
  pet.cruzada = nav_cruzada(origen, u);
  http_empieza(&pet);
  cargando = 1;
  redirs = 0;
  sobre[0] = 0; sobre_enl = -1;
  di("Abriendo...");
}
static void ir_dir(const char *t) {
  char b[1200];
  while (*t == ' ') t++;
  if (!*t) return;
  if (strstr(t, "://") || !_strnicmp(t, "about:", 6)) sprintf(b, "%.1100s", t);
  else sprintf(b, "http://%.1100s", t);
  ir(b, 0, NULL, 0, 0);
}
static void atras(void) {
  if (i_hist <= 0) return;
  if (pag) hist[i_hist].sy = sy;
  i_hist--;
  sy_tras = hist[i_hist].sy;
  ir(hist[i_hist].url, 0, NULL, 0, 1);
}
static void adelante(void) {
  if (i_hist < 0 || i_hist >= n_hist - 1) return;
  if (pag) hist[i_hist].sy = sy;
  i_hist++;
  sy_tras = hist[i_hist].sy;
  ir(hist[i_hist].url, 0, NULL, 0, 1);
}
static void recarga(void) {
  if (i_hist < 0) return;
  sy_tras = sy;
  ir(hist[i_hist].url, 0, NULL, 0, 1);
}
static int es_latin1(const Pet *p) {
  const char *c = strstr(p->tipo, "charset=");
  char b[1025];
  u32 n = p->n_datos < 1024 ? p->n_datos : 1024, k;
  if (c) return !strncmp(c + 8, "iso-8859-1", 10) || !strncmp(c + 8, "windows-1252", 12) || !strncmp(c + 8, "latin1", 6);
  for (k = 0; k < n; k++) b[k] = (char)min_c(p->datos[k]);
  b[n] = 0;
  c = strstr(b, "charset=");
  if (!c) return 0;
  c += 8;
  if (*c == '"' || *c == '\'') c++;
  return !strncmp(c, "iso-8859-1", 10) || !strncmp(c, "windows-1252", 12) || !strncmp(c, "latin1", 6);
}
/* =============================================================================
 *  Las descargas: lo que no es una página (un PDF, un recibo) se guarda en
 *  el disco y se ofrece abrirlo con su programa (el Lector, para los PDF)
 * ============================================================================= */
static int existe(const char *ruta) {
  FILE *f = fopen(ruta, "rb");
  if (f) fclose(f);
  return f != NULL;
}
/* el programa que abre los archivos de la extensión ext (de \HISPADOS\ASOCIA.INI,
 * como el Administrador de archivos; o los de serie), en prog; 0 si no hay */
static int visor_de(const char *ext, char *prog, int max) {
  char ruta[200], l[200], v[160];
  FILE *f;
  int k, n = strlen(ext);
  v[0] = 0;
  if (!n || n > 4) return 0;
  sprintf(ruta, "%sASOCIA.INI", dir_hispados);
  if ((f = fopen(ruta, "r"))) {
    while (fgets(l, sizeof l, f)) {
      for (k = 0; l[k] && l[k] != '\r' && l[k] != '\n'; k++) ;
      l[k] = 0;
      if (!_strnicmp(l, ext, n) && l[n] == '=') { sprintf(v, "%.150s", l + n + 1); break; }
    }
    fclose(f);
  } else if (!_stricmp(ext, "PDF")) strcpy(v, "PROGS\\LECTOR.HVP");
  else if (!_stricmp(ext, "PNG") || !_stricmp(ext, "JPG") || !_stricmp(ext, "GIF") || !_stricmp(ext, "BMP")) strcpy(v, "PROGS\\MIRADOR.HVP");
  if (!v[0] || v[0] == '-' || v[0] == ':') return 0;
  if (v[1] == ':' || v[0] == '\\') sprintf(prog, "%.*s", max - 1, v);
  else sprintf(prog, "%.*s%.*s", 100, dir_hispados, max - 101, v);
  return existe(prog);
}
/* el nombre que propone el servidor (Content-Disposition) o el de la
 * dirección, en 8+3 y en mayúsculas */
static void nombre_descarga(const Pet *p, char *d) {
  const char *cd = http_cabecera(p, "Content-Disposition"), *t = NULL, *fin;
  char b[200], ext[4];
  int n = 0, k, e = 0, punto = -1;
  if (cd) {
    const char *f = cd;
    while ((f = strstr(f, "filename"))) {
      const char *v = f + 8;
      if (*v == '*') { v++; while (*v == ' ') v++; if (*v == '=') { v++; for (fin = v; *fin && *fin != ';' && *fin != '\r'; fin++) ; for (; v < fin && !(v[0] == '\'' && v[1] == '\''); v++) ; if (v < fin) { t = v + 2; break; } } }
      while (*v == ' ') v++;
      if (*v == '=') { v++; while (*v == ' ') v++; if (*v == '"') v++; t = v; }
      f = v;
    }
  }
  if (!t) {                                      /* el último trozo de la ruta */
    const char *q = p->url, *a = strstr(q, "://");
    if (a) q = a + 3;
    for (t = q; *q && *q != '?' && *q != '#'; q++) if (*q == '/') t = q + 1;
    for (fin = t; *fin && *fin != '?' && *fin != '#'; fin++) ;
  } else for (fin = t; *fin && *fin != '"' && *fin != ';' && *fin != '\r' && *fin != '\n'; fin++) ;
  for (k = 0; t + k < fin && n < 190; k++) b[n++] = t[k];
  b[n] = 0;
  for (k = 0; k < n; k++) if (b[k] == '.') punto = k;
  d[0] = 0;
  for (k = 0, n = 0; b[k] && (punto < 0 || k < punto) && n < 8; k++) {
    int c = (u8)b[k];
    if (c == '%' && b[k + 1] && b[k + 2]) { k += 2; continue; }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') d[n++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
  }
  if (!n) { strcpy(d, "DESCARGA"); n = 8; }
  if (punto >= 0) for (k = punto + 1; b[k] && e < 3; k++) {
    int c = (u8)b[k];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ext[e++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
  }
  if (!e) {                                      /* sin extensión: la del tipo */
    if (strstr(p->tipo, "pdf")) { memcpy(ext, "PDF", 3); e = 3; }
    else if (strstr(p->tipo, "png")) { memcpy(ext, "PNG", 3); e = 3; }
    else if (strstr(p->tipo, "jpeg")) { memcpy(ext, "JPG", 3); e = 3; }
    else if (strstr(p->tipo, "gif")) { memcpy(ext, "GIF", 3); e = 3; }
    else if (strstr(p->tipo, "csv")) { memcpy(ext, "CSV", 3); e = 3; }
    else if (strstr(p->tipo, "text/")) { memcpy(ext, "TXT", 3); e = 3; }
  }
  d[n] = 0;
  if (e) { d[n] = '.'; memcpy(d + n + 1, ext, e); d[n + 1 + e] = 0; }
}
/* ¿lo que llega es para guardar y no para ver? (un adjunto, o lo que no es
 * una página ni un texto) */
static int es_descarga(const Pet *p) {
  const char *cd = http_cabecera(p, "Content-Disposition");
  int html = !p->tipo[0] || strstr(p->tipo, "html") != NULL, plano = !strncmp(p->tipo, "text/", 5) && !html;
  if (p->estado < 200 || p->estado >= 300 || p->estado == 204) return 0;
  return (cd && !_strnicmp(cd, "attachment", 10)) || (!html && !plano);
}
/* las cabeceras de una descarga: dónde se guarda (el diálogo de Guardar
 * como); lo que llegue desde ahora va derecho al archivo. 0 si no se guarda */
static char desc_ruta[260];
static int descarga_empieza(Pet *p) {
  OPENFILENAMEA o;
  char m[400];
  FILE *f;
  int k;
  nombre_descarga(p, desc_ruta);
  if (p->largo > 0) sprintf(m, "Descarga: %.60s (%u K). Elija d\363nde guardarla.", desc_ruta, ((u32)p->largo + 1023) / 1024);
  else sprintf(m, "Descarga: %.60s. Elija d\363nde guardarla.", desc_ruta);
  di(m);
  memset(&o, 0, sizeof o);
  o.lStructSize = sizeof o; o.hwndOwner = vent; o.lpstrFile = desc_ruta; o.nMaxFile = sizeof desc_ruta;
  o.lpstrInitialDir = dir_descargas[0] ? dir_descargas : NULL; o.lpstrTitle = "Guardar la descarga";
  if (!GetSaveFileNameA(&o) || !desc_ruta[0]) { di("No se ha guardado la descarga."); return 0; }
  if (existe(desc_ruta)) {
    sprintf(m, "Ya hay un archivo %.200s.\n\n\277Sustituirlo por el que se est\341 descargando?", desc_ruta);
    if (MessageBoxA(vent, m, "Navegador", MB_YESNO | MB_ICONQUESTION) != IDYES) { di("No se ha guardado la descarga."); return 0; }
  }
  f = fopen(desc_ruta, "wb");
  if (!f || (p->n_datos && fwrite(p->datos, 1, p->n_datos, f) != p->n_datos)) {
    if (f) { fclose(f); remove(desc_ruta); }
    sprintf(m, "No se puede escribir %.200s.\n\nPuede que el disco est\351 lleno o protegido contra escritura.", desc_ruta);
    MessageBoxA(vent, m, "Navegador", MB_OK);
    di("No se ha guardado la descarga.");
    return 0;
  }
  free(p->datos); p->datos = NULL; p->cap_datos = 0;
  p->disco = f;
  p->max = 64u << 20;
  /* la carpeta, para la siguiente */
  for (k = strlen(desc_ruta); k > 0 && desc_ruta[k - 1] != '\\'; k--) ;
  if (k > 0 && k < (int)sizeof dir_descargas) { memcpy(dir_descargas, desc_ruta, k); dir_descargas[k] = 0; if (k > 3) dir_descargas[k - 1] = 0; }
  return 1;
}
/* la descarga ha acabado (bien, si ok): se cierra el archivo y se ofrece
 * abrirlo con su programa (el Lector, para los PDF; desde él se imprime) */
static void descarga_acaba(Pet *p, int ok) {
  char m[700], prog[200], *ext;
  u32 n = p->n_datos;
  int mal = fclose(p->disco);
  p->disco = NULL;
  if (!ok || mal) {
    remove(desc_ruta);
    sprintf(m, "La descarga no se ha completado: %.200s\n\nNo se ha guardado nada.", ok ? "no se puede escribir en el disco." : p->error);
    MessageBoxA(vent, m, "Navegador", MB_OK);
    di("No se ha guardado la descarga.");
    return;
  }
  sprintf(m, "Guardado en %.200s", desc_ruta);
  di(m);
  ext = strrchr(desc_ruta, '.');
  if (ext && !strchr(ext, '\\') && visor_de(ext + 1, prog, sizeof prog)) {
    const char *quien = strstr(prog, "LECTOR") ? "el Lector" : strstr(prog, "MIRADOR") ? "Mirador" : "su programa";
    sprintf(m, "Guardado en %.200s (%u K).\n\n\277Abrirlo ahora con %s?%s", desc_ruta, (n + 1023) / 1024, quien,
            strstr(prog, "LECTOR") ? "\nDesde el Lector tambi\351n se puede imprimir." : "");
    if (MessageBoxA(vent, m, "Navegador", MB_YESNO | MB_ICONQUESTION) == IDYES) {
      char l[480];
      sprintf(l, "%.200s %.250s", prog, desc_ruta);
      WinExec(l, 1);
    }
  } else {
    sprintf(m, "Guardado en %.200s (%u K).", desc_ruta, (n + 1023) / 1024);
    MessageBoxA(vent, m, "Navegador", MB_OK);
  }
}

/* =============================================================================
 *  Las imágenes: después de la página, una a una, con la misma conexión
 * ============================================================================= */
static void img_cancela(void) {
  if (icarga >= 0) {
    http_suelta(&ipet);
    if (pag && icarga < pag->n_img && pag->img[icarga].estado == IM_BAJANDO) pag->img[icarga].estado = IM_PENDIENTE;
  }
  icarga = -1;
  i_remaq = 0; i_hechas = 0;
}
static int img_pendiente(void) {
  int k;
  if (!pag) return -1;
  for (k = 0; k < pag->n_img; k++) if (pag->img[k].visto && pag->img[k].estado == IM_PENDIENTE) return k;
  return -1;
}
static int img_trabajo(void) { return icarga >= 0 || i_remaq || i_hechas || img_pendiente() >= 0; }
static int img_max_w(void) { int m = (cols - 4) * LW; return m < IMG_MAX_W ? m : IMG_MAX_W; }
/* la imagen k ya está (o no se ha podido ver): si cabe en el sitio que
 * tenía guardado, basta con pintarla; si no, hay que maquetar otra vez la
 * página (no a cada una: ver img_avanza) */
static void img_lista(int k) {
  Img *im = &pag->img[k];
  i_hechas++;
  if (im->estado == IM_LISTA && im->y >= 0 && im->cw == (im->w + LW - 1) / LW && im->ch == (im->h + LH - 1) / LH) {
    RECT r;
    r.left = (im->x - sx) * LW; r.top = BARRA_H + (im->y - sy) * LH;
    r.right = r.left + im->cw * LW; r.bottom = r.top + im->ch * LH;
    if (r.top < BARRA_H) r.top = BARRA_H;
    if (r.bottom > al - ESTADO_H) r.bottom = al - ESTADO_H;
    if (r.bottom > r.top) InvalidateRect(vent, &r, 0);
  } else if (im->estado == IM_LISTA || (im->y >= 0 && im->cw)) {
    if (!i_remaq) i_remaq_t = GetTickCount();
    i_remaq = 1;
  }
}
static int base64(const char *t, u8 *d) {
  int n = 0, b = 0, q = 0;
  for (; *t; t++) {
    int c = *t, v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
          : c == '+' || c == '-' ? 62 : c == '/' || c == '_' ? 63 : -1;
    if (v < 0) continue;
    b = b << 6 | v; q += 6;
    if (q >= 8) { q -= 8; d[n++] = (u8)(b >> q); }
  }
  return n;
}
/* de los datos a los puntos de la imagen k */
static void img_hace(int k, const u8 *d, u32 n) {
  Img *im = &pag->img[k];
  char e[80];
  int w, h, c;
  u32 tam = 0;
  im->px = img_descodifica(d, n, im->pw, im->ph, img_max_w(), IMG_MAX_H, &w, &h, e);
  if (!im->px) { im->estado = IM_FALLO; img_lista(k); return; }
  im->w = w; im->h = h; im->estado = IM_LISTA;
  /* a lo ya visto (sale una copia; se tira la más vieja si no cabe) */
  for (c = 0; c < CACHE_N; c++) if (cache[c].px) tam += ((cache[c].w + 3) & ~3) * cache[c].h;
  for (c = 0; c < CACHE_N; c++) if (!cache[c].url) break;
  while (c == CACHE_N || tam + ((w + 3) & ~3) * h > CACHE_MAX) {
    int j, v = -1;
    for (j = 0; j < CACHE_N; j++) if (cache[j].url && (v < 0 || cache[j].uso < cache[v].uso)) v = j;
    if (v < 0) break;
    tam -= ((cache[v].w + 3) & ~3) * cache[v].h;
    free(cache[v].url); free(cache[v].px); memset(&cache[v], 0, sizeof cache[v]);
    c = v;
  }
  if (c < CACHE_N && ((w + 3) & ~3) * h <= CACHE_MAX / 2) {
    cache[c].url = malloc(strlen(im->src) + 1);
    cache[c].px = malloc(((w + 3) & ~3) * h);
    if (cache[c].url && cache[c].px) {
      strcpy(cache[c].url, im->src); memcpy(cache[c].px, im->px, ((w + 3) & ~3) * h);
      cache[c].pw = im->pw; cache[c].ph = im->ph; cache[c].w = w; cache[c].h = h; cache[c].uso = ++cache_uso;
    } else { free(cache[c].url); free(cache[c].px); memset(&cache[c], 0, sizeof cache[c]); }
  }
  img_lista(k);
}
static void img_avanza(void) {
  int k, r;
  Img *im;
  if (icarga < 0) {
    k = img_pendiente();
    if (k < 0) {
      if (i_remaq) { i_remaq = 0; remaqueta(); }
      if (i_hechas) { i_hechas = 0; if (!cargando) di(""); }
      return;
    }
    /* de vez en cuando, la página maquetada otra vez con las que ya han llegado */
    if (i_remaq && GetTickCount() - i_remaq_t > 2500u) { i_remaq = 0; remaqueta(); }
    im = &pag->img[k];
    {                                            /* ¿ya vista? */
      int c;
      for (c = 0; c < CACHE_N; c++)
        if (cache[c].url && !strcmp(cache[c].url, im->src) && cache[c].pw == im->pw && cache[c].ph == im->ph && cache[c].w <= img_max_w()) {
          im->px = malloc(((cache[c].w + 3) & ~3) * cache[c].h);
          if (!im->px) break;
          memcpy(im->px, cache[c].px, ((cache[c].w + 3) & ~3) * cache[c].h);
          im->w = cache[c].w; im->h = cache[c].h; im->estado = IM_LISTA;
          cache[c].uso = ++cache_uso;
          img_lista(k);
          return;
        }
    }
    if (!_strnicmp(im->src, "data:", 5)) {      /* data:image/png;base64,... */
      const char *c = strchr(im->src, ',');
      u8 *d;
      if (!c || !strstr(im->src, ";base64") || strstr(im->src, ";base64") > c || !(d = malloc(strlen(c)))) { im->estado = IM_FALLO; img_lista(k); return; }
      img_hace(k, d, base64(c + 1, d));
      free(d);
      return;
    }
    memset(&ipet, 0, sizeof ipet);
    ipet.url = malloc(strlen(im->src) + 1);
    if (!ipet.url) { im->estado = IM_FALLO; return; }
    strcpy(ipet.url, im->src);
    ipet.sub = 1; ipet.max = 1u << 20;
    ipet.cruzada = nav_cruzada(pag->url, ipet.url);
    http_empieza(&ipet);
    im->estado = IM_BAJANDO;
    icarga = k; iredirs = 0;
    return;
  }
  r = http_avanza(&ipet);
  if (r == 0) {
    char b[80];
    int j, total = 0, hechas = 0;
    for (j = 0; j < pag->n_img; j++) if (pag->img[j].visto && pag->img[j].src) { total++; if (pag->img[j].estado >= IM_LISTA) hechas++; }
    sprintf(b, "Bajando las im\341genes (%d de %d)...", hechas + 1, total);
    if (!sobre[0]) di(b);
    return;
  }
  im = &pag->img[icarga];
  k = icarga;
  icarga = -1;
  if (r > 0 && (ipet.estado == 301 || ipet.estado == 302 || ipet.estado == 303 || ipet.estado == 307 || ipet.estado == 308)
      && http_cabecera(&ipet, "Location") && ++iredirs <= 5) {
    const char *l = http_cabecera(&ipet, "Location");
    char loc[1024], *nueva;
    int j;
    for (j = 0; l[j] && l[j] != '\r' && l[j] != '\n' && j < 1022; j++) loc[j] = l[j];
    loc[j] = 0;
    nueva = url_absoluta(ipet.url, loc);
    http_suelta(&ipet);
    if (nueva) {
      ipet.url = nueva; ipet.sub = 1; ipet.max = 1u << 20; ipet.cruzada = nav_cruzada(pag->url, nueva);
      http_empieza(&ipet);
      icarga = k;
      return;
    }
    im->estado = IM_FALLO;
    img_lista(k);
    return;
  }
  if (r > 0 && ipet.estado == 200 && ipet.n_datos) img_hace(k, ipet.datos, ipet.n_datos);
  else { im->estado = IM_FALLO; img_lista(k); }
  http_suelta(&ipet);
}

/* un paso de la carga: se llama desde el bucle de mensajes */
static void carga_avanza(void) {
  static const char *const fases[] = { "Abriendo...", "Buscando el servidor...", "Conectando con el servidor...",
    "Abriendo la conexi\363n segura...", "Enviando la petici\363n...", "Esperando la respuesta...", "Recibiendo la p\341gina..." };
  int r = http_avanza(&pet);
  if (r >= 0 && !pet.decidido && pet.fase >= H_CUERPO) {     /* las cabeceras: ¿es para guardar? */
    pet.decidido = 1;
    if (es_descarga(&pet) && !descarga_empieza(&pet)) { cargando = 0; http_suelta(&pet); return; }
  }
  if (r == 0) {
    char b[160];
    if (pet.disco) {
      if (pet.largo > 0) sprintf(b, "Guardando %.60s... (%u K de %u K)", desc_ruta, (pet.n_datos + 512) / 1024, ((u32)pet.largo + 512) / 1024);
      else sprintf(b, "Guardando %.60s... (%u K)", desc_ruta, (pet.n_datos + 512) / 1024);
      di(b);
    }
    else if (pet.fase == H_CUERPO && pet.n_datos) { sprintf(b, "Recibiendo la p\341gina... (%u K)", (pet.n_datos + 512) / 1024); di(b); }
    else if (pet.fase < 7) di(fases[pet.fase]);
    return;
  }
  cargando = 0;
  if (pet.disco) { descarga_acaba(&pet, r > 0); http_suelta(&pet); return; }
  if (r < 0) {
    char *u = pet.url;
    pet.url = NULL;
    pagina_error(u, pet.error);
    free(u);
    http_suelta(&pet);
    return;
  }
  /* una redirección: a la dirección nueva (con GET, salvo 307 y 308) */
  if ((pet.estado == 301 || pet.estado == 302 || pet.estado == 303 || pet.estado == 307 || pet.estado == 308) && http_cabecera(&pet, "Location")) {
    const char *l = http_cabecera(&pet, "Location");
    char loc[1024], *nueva, *cuerpo = NULL;
    u32 nc = 0;
    int k, post = 0;
    for (k = 0; l[k] && l[k] != '\r' && l[k] != '\n' && k < 1022; k++) loc[k] = l[k];
    loc[k] = 0;
    nueva = url_absoluta(pet.url, loc);
    if (nueva && ++redirs <= 10) {
      char tc[sizeof pet.tipo_cuerpo];
      tc[0] = 0;
      if ((pet.estado == 307 || pet.estado == 308) && pet.post) { post = 1; cuerpo = pet.cuerpo; nc = pet.n_cuerpo; pet.cuerpo = NULL; memcpy(tc, pet.tipo_cuerpo, sizeof tc); }
      http_suelta(&pet);
      if (strchr(nueva, '#')) *strchr(nueva, '#') = 0;
      pet.url = nueva; pet.post = post; pet.cuerpo = cuerpo; pet.n_cuerpo = nc;
      memcpy(pet.tipo_cuerpo, tc, sizeof tc);
      http_empieza(&pet);
      cargando = 1;
      return;
    }
    free(nueva);
    {
      char *u = pet.url;
      pet.url = NULL;
      pagina_error(u, redirs > 10 ? "El servidor redirige una y otra vez." : "El servidor redirige a una direcci\363n que no se entiende.");
      free(u);
    }
    http_suelta(&pet);
    return;
  }
  {
    int html = !pet.tipo[0] || strstr(pet.tipo, "html") != NULL, plano = !strncmp(pet.tipo, "text/", 5) && !html;
    Pagina *p = NULL;
    if ((html || plano) && pet.n_datos) p = pag_nueva(pet.url, pet.datos, pet.n_datos, es_latin1(&pet), plano);
    if (p) muestra(p, pet.url);
    else {
      static char h[900];
      if (pet.estado >= 400)
        sprintf(h, "<title>Error %d</title><h1>El servidor responde con el error %d</h1><p><b>Direcci\363n:</b> %.400s</p>", pet.estado, pet.estado, pet.url);
      else if (!pet.n_datos)
        sprintf(h, "<title>Sin contenido</title><p>El servidor no ha enviado nada (respuesta %d).</p>", pet.estado);
      else
        sprintf(h, "<title>No se puede mostrar</title><h1>No se puede mostrar</h1><p>Esta direcci\363n no es una p\341gina: es un archivo "
                   "de tipo <b>%.70s</b> (%u octetos). Para bajarlo, use la orden HTTPS de Hispa-DOS.</p>", pet.tipo, pet.n_datos);
      { char *u = pet.url; pet.url = NULL; pagina_propia(u, h); free(u); }
    }
  }
  http_suelta(&pet);
}

/* =============================================================================
 *  El foco, los enlaces y los formularios
 * ============================================================================= */
static void a_la_vista(int x, int y, int h) {
  int ny = sy;
  (void)x;
  if (y < sy) ny = y - 2;
  else if (y + h > sy + filas_vis) ny = y + h - filas_vis + 2;
  desplaza(sx, ny);
}
static Nodo *nodo_foco(void);
static void foco_pon(int f) {
  foco = f;
  foco_dir = 0;
  if (pag && f >= 0 && f < pag->n_foco) {
    if (pag->foco[f].enlace) { Enlace *e = &pag->enl[pag->foco[f].n]; a_la_vista(e->x, e->y < 0 ? 0 : e->y, 1); }
    else { Ctl *c = &pag->ctl[pag->foco[f].n]; a_la_vista(c->x, c->y, c->h); }
  }
  repinta();
  js_foco(nodo_foco());                         /* [JS] focus y blur */
}
static int foco_de_ctl(int idx) {
  int k;
  for (k = 0; k < pag->n_foco; k++) if (!pag->foco[k].enlace && pag->foco[k].n == idx) return k;
  return -1;
}
static void remaqueta(void) {
  int f = foco;
  if (!pag) return;
  pag_maqueta(pag, cols);
  foco = f < pag->n_foco ? f : -1;
  desplaza(sx, sy);
  if (sy > pag->n_filas - filas_vis) { sy = pag->n_filas - filas_vis; if (sy < 0) sy = 0; }
  repinta();
}
static void pulsa_ctl(int idx);
static int js_clic_enlace(int *e);
static void abre_enlace(int e) {
  Enlace *en;
  char b[300], *u;
  if (js_clic_enlace(&e)) return;              /* [JS] primero, el clic de la página */
  en = &pag->enl[e];
  if (!en->href && en->nodo->etq == E_LABEL) {   /* el texto de una etiqueta: su control */
    int k = pag_ctl_de_label(pag, en->nodo);
    if (k >= 0 && pag->ctl[k].visible) pulsa_ctl(k);
    return;
  }
  if (!en->href) {                               /* algo que se pliega */
    en->nodo->abierto = !en->nodo->abierto;
    remaqueta();
    return;
  }
  if (!js_en_pagina() && pag_confirma(en->nodo, b, sizeof b) && MessageBoxA(vent, b, "Navegador", MB_YESNO) != IDYES) return;
  if (en->href[0] == '#') { desplaza(sx, 0); return; }
  u = url_absoluta(pag->doc.base ? pag->doc.base : pag->url, en->href);
  if (!u) { di("Este enlace no se puede abrir aqu\355 (no es una p\341gina web)."); return; }
  ir2(u, 0, NULL, 0, 0, NULL, pag->url);
  free(u);
}
/* el nombre de un campo para los avisos: su <label>, su placeholder o su name */
static void nombre_campo(Ctl *c, char *b, int max) {
  const char *id = atr(c->nodo, "id"), *t;
  int k, n;
  b[0] = 0;
  if (id && id[0])
    for (k = 0; k < pag->n_enl; k++) {
      Nodo *l = pag->enl[k].nodo;
      if (!pag->enl[k].href && l->etq == E_LABEL && (t = atr(l, "for")) && !strcmp(t, id)) { texto_de(l, b, max); break; }
    }
  if (!b[0] && (t = atr(c->nodo, "aria-label"))) sprintf(b, "%.*s", max - 1, t);
  if (!b[0] && (t = atr(c->nodo, "placeholder"))) sprintf(b, "%.*s", max - 1, t);
  if (!b[0] && c->nombre) sprintf(b, "%.*s", max - 1, c->nombre);
  n = strlen(b);
  while (n && (b[n - 1] == ' ' || b[n - 1] == ':' || b[n - 1] == '*')) b[--n] = 0;
  while (b[0] == ' ') memmove(b, b + 1, n--);
}
/* lo que pide el formulario antes de enviarse (required, pattern, type=email,
 * los archivos que hay que subir): 1 si se puede enviar */
static int valida(Nodo *forma) {
  char m[400], nom[120];
  int k;
  for (k = 0; k < pag->n_ctl; k++) {
    Ctl *c = &pag->ctl[k];
    const char *t, *falta = NULL;
    if (c->forma != forma || c->desactivado || !c->visible) continue;
    if (c->obligado) {
      if (es_texto(c) && !c->n_valor) falta = c->tipo == C_ARCHIVO ? "Falta elegir el archivo de" : "Falta rellenar";
      else if (c->tipo == C_CASILLA && !c->marcado) falta = "Hay que marcar";
      else if (c->tipo == C_LISTA && c->n_opc && (!(t = atr(c->opc[c->elegida], "value")) ? 0 : !t[0])) falta = "Falta elegir";
      else if (c->tipo == C_RADIO) {
        int j, alguno = 0;
        for (j = 0; j < pag->n_ctl; j++) {
          Ctl *o = &pag->ctl[j];
          if (o->tipo == C_RADIO && o->forma == forma && o->nombre && c->nombre && !strcmp(o->nombre, c->nombre) && o->marcado) alguno = 1;
        }
        if (!alguno) falta = "Falta elegir";
      }
    }
    if (!falta && (c->tipo == C_TEXTO || c->tipo == C_CLAVE) && c->n_valor) {
      if ((t = atr(c->nodo, "type")) && !_stricmp(t, "email") && (!strchr(c->valor, '@') || c->valor[0] == '@' || c->valor[c->n_valor - 1] == '@'))
        falta = "No es una direcci\363n de correo";
      else if ((t = atr(c->nodo, "pattern")) && t[0] && !pag_patron(t, c->valor, c->n_valor)) falta = "No tiene la forma que se pide";
      else if ((t = atr(c->nodo, "minlength")) && atoi(t) > c->n_valor) falta = "Es demasiado corto";
    }
    if (!falta && c->tipo == C_ARCHIVO && c->n_valor) {
      long l = pag_archivo_mide(c->valor);
      if (l == -1) falta = "No se puede leer el archivo de";
      else if (l == -2) falta = "Es demasiado grande (m\341s de 2 MB) el archivo de";
    }
    if (falta) {
      nombre_campo(c, nom, sizeof nom);
      t = atr(c->nodo, "title");
      sprintf(m, "%s%s%.100s%s%s%.150s", falta, nom[0] ? " \253" : " un campo obligatorio", nom, nom[0] ? "\273." : ".",
              t && t[0] && strstr(falta, "forma") ? "\n\n" : "", t && strstr(falta, "forma") ? t : "");
      MessageBoxA(vent, m, "Navegador", MB_OK);
      foco_pon(foco_de_ctl(k));
      return 0;
    }
  }
  return 1;
}
static int js_envia(Ctl **boton);
static void envia(Ctl *boton) {
  Nodo *forma = boton->forma;
  char b[300], *url, *cuerpo, tipo[96];
  u32 nc;
  int k, j, post, con;
  if (!forma) return;
  for (j = 0; j < 2; j++) {
    con = boton->tipo == C_ENVIAR ? (int)(boton - pag->ctl) : -1;
    if (con < 0)                                 /* Intro en un campo: como pulsar el primer botón de enviar */
      for (k = 0; k < pag->n_ctl; k++)
        if (pag->ctl[k].forma == forma && pag->ctl[k].tipo == C_ENVIAR && !pag->ctl[k].desactivado) { con = k; break; }
    if (j) break;
    /* lo que pide el formulario (required, pattern...) y luego [JS] el evento submit */
    if (!(con >= 0 && atr(pag->ctl[con].nodo, "formnovalidate")) && !atr(forma, "novalidate") && !valida(forma)) return;
    if (js_envia(&boton)) return;
    forma = boton->forma;
  }
  if (!js_en_pagina() && (pag_confirma(forma, b, sizeof b) || (con >= 0 && pag_confirma(pag->ctl[con].nodo, b, sizeof b)))
      && MessageBoxA(vent, b, "Navegador", MB_YESNO) != IDYES) return;
  if (!pag_formulario(pag, forma, con, &url, &cuerpo, &nc, &post, tipo)) { di("Este formulario no se puede enviar."); return; }
  ir2(url, post, cuerpo, nc, 0, tipo[0] ? tipo : NULL, pag->url);
  free(url);
}
/* el diálogo de abrir, para el campo de subir un archivo */
static void elige_archivo(Ctl *c) {
  static char dir[150];
  OPENFILENAMEA o;
  char ruta[260];
  int k;
  sprintf(ruta, "%.250s", c->valor);
  for (k = strlen(ruta); k > 0 && ruta[k - 1] != '\\'; k--) ;
  if (k > 0) { sprintf(dir, "%.*s", k > 3 ? k - 1 : k, ruta); memmove(ruta, ruta + k, strlen(ruta + k) + 1); }
  memset(&o, 0, sizeof o);
  o.lStructSize = sizeof o; o.hwndOwner = vent; o.lpstrFile = ruta; o.nMaxFile = sizeof ruta;
  o.lpstrInitialDir = dir[0] ? dir : dir_descargas[0] ? dir_descargas : NULL; o.lpstrTitle = "Elegir el archivo que se sube";
  if (GetOpenFileNameA(&o) && ruta[0]) {
    ctl_pon(c, ruta, -1);
    c->cursor = c->n_valor;
    for (k = strlen(ruta); k > 0 && ruta[k - 1] != '\\'; k--) ;
    if (k > 0) sprintf(dir, "%.*s", k > 3 ? k - 1 : k, ruta);
  }
  repinta();
}
static void lista_abre(int idx) {
  Ctl *c = &pag->ctl[idx];
  int y;
  if (!c->n_opc) return;
  lista.activa = 1; lista.ctl = idx; lista.sel = c->elegida;
  lista.vis = c->n_opc < 10 ? c->n_opc : 10;
  if (lista.vis > filas_vis - 1) lista.vis = filas_vis - 1;
  if (lista.vis < 1) lista.vis = 1;
  lista.w = c->w * LW;
  lista.x = (c->x - sx) * LW;
  y = BARRA_H + (c->y - sy + 1) * LH;
  if (y + lista.vis * LH + 2 > al - ESTADO_H) y = BARRA_H + (c->y - sy) * LH - lista.vis * LH - 2;
  if (y < BARRA_H) y = BARRA_H;
  lista.y = y;
  lista.top = lista.sel - lista.vis / 2;
  if (lista.top > c->n_opc - lista.vis) lista.top = c->n_opc - lista.vis;
  if (lista.top < 0) lista.top = 0;
  repinta_pagina();
}
static void lista_cierra(int elige) {
  Ctl *c = &pag->ctl[lista.ctl];
  int cambia = elige && lista.sel != c->elegida;
  lista.activa = 0;
  if (cambia) c->elegida = lista.sel;
  repinta_pagina();
  if (cambia && js_en_pagina()) {               /* [JS] input y change (y lo de onchange, en la página) */
    Nodo *n = c->nodo;
    js_evento(n, "input", NULL);
    js_evento(n, "change", NULL);
    return;
  }
  if (cambia && c->cambia_envia) envia(c);
}
static int js_pulsa(int *idx);
static void pulsa_ctl(int idx) {
  Ctl *c = &pag->ctl[idx];
  int k;
  if (c->desactivado) return;
  if (js_pulsa(&idx)) return;                   /* [JS] el clic de la página (y su foco) */
  c = &pag->ctl[idx];
  foco = foco_de_ctl(idx);
  foco_dir = 0;
  switch (c->tipo) {
  case C_CASILLA:
    c->marcado = !c->marcado;
    break;
  case C_RADIO:
    for (k = 0; k < pag->n_ctl; k++) {
      Ctl *o = &pag->ctl[k];
      if (o->tipo == C_RADIO && o->forma == c->forma && o->nombre && c->nombre && !strcmp(o->nombre, c->nombre)) o->marcado = 0;
    }
    c->marcado = 1;
    break;
  case C_ENVIAR:
    repinta();
    envia(c);
    return;
  case C_REINICIO:
    for (k = 0; k < pag->n_ctl; k++) {
      Ctl *o = &pag->ctl[k];
      if (o->forma != c->forma) continue;
      if (es_texto(o)) ctl_pon(o, o->valor0, -1);
      o->marcado = o->marcado0; o->elegida = o->elegida0;
    }
    break;
  case C_BOTON:
    if (!js_en_pagina()) di(js_activo ? "Este bot\363n no hace nada." : "Este bot\363n necesita JavaScript, que est\341 apagado (NAVEGA.INI).");
    break;
  case C_LISTA:
    repinta();
    lista_abre(idx);
    return;
  }
  repinta();
}
static void pulsa_ctl_en(int idx, int col) {
  Ctl *c = &pag->ctl[idx];
  if (c->tipo == C_ARCHIVO && !c->desactivado && col >= c->x + c->w - ARCH_BOTON) {
    foco = foco_de_ctl(idx); foco_dir = 0;
    repinta();
    elige_archivo(c);
    return;
  }
  pulsa_ctl(idx);
}
/* escribir en un campo de una línea: 1 si ha cambiado algo que pintar */
static int edita(Ctl *c, int vk, int ch) {
  if (c == &dir && dir_sel) {                    /* la dirección, elegida entera */
    dir_sel = 0;
    if (ch >= 32 || vk == VK_BACK || vk == VK_DELETE) { ctl_pon(c, "", 0); c->cursor = c->desp = 0; if (ch < 32) return 1; }
    else return 1;
  }
  if (ch >= 32 || ch == '\n') {
    if (c->sololee || c->n_valor >= c->max) return 0;
    if (c->n_valor + 2 > c->cap_valor) {
      char *v = realloc(c->valor, c->n_valor + 128);
      if (!v) return 0;
      c->valor = v; c->cap_valor = c->n_valor + 128;
    }
    memmove(c->valor + c->cursor + 1, c->valor + c->cursor, c->n_valor - c->cursor + 1);
    c->valor[c->cursor++] = (char)ch;
    c->n_valor++;
    return 1;
  }
  switch (vk) {
  case VK_BACK:
    if (!c->cursor || c->sololee) return 0;
    memmove(c->valor + c->cursor - 1, c->valor + c->cursor, c->n_valor - c->cursor + 1);
    c->cursor--; c->n_valor--;
    return 1;
  case VK_DELETE:
    if (c->cursor >= c->n_valor || c->sololee) return 0;
    memmove(c->valor + c->cursor, c->valor + c->cursor + 1, c->n_valor - c->cursor);
    c->n_valor--;
    return 1;
  case VK_LEFT: if (c->cursor) { c->cursor--; return 1; } return 0;
  case VK_RIGHT: if (c->cursor < c->n_valor) { c->cursor++; return 1; } return 0;
  case VK_HOME: c->cursor = 0; return 1;
  case VK_END: c->cursor = c->n_valor; return 1;
  }
  return 0;
}
static void tecla(int vk) {
  Ctl *c = ctl_foco();
  int e;
  if (lista.activa) {
    Ctl *l = &pag->ctl[lista.ctl];
    switch (vk) {
    case VK_UP: if (lista.sel) lista.sel--; break;
    case VK_DOWN: if (lista.sel < l->n_opc - 1) lista.sel++; break;
    case VK_PRIOR: lista.sel -= lista.vis; break;
    case VK_NEXT: lista.sel += lista.vis; break;
    case VK_HOME: lista.sel = 0; break;
    case VK_END: lista.sel = l->n_opc - 1; break;
    case VK_RETURN: lista_cierra(1); return;
    case VK_ESCAPE: case VK_TAB: lista_cierra(0); return;
    default: return;
    }
    if (lista.sel < 0) lista.sel = 0;
    if (lista.sel > l->n_opc - 1) lista.sel = l->n_opc - 1;
    if (lista.sel < lista.top) lista.top = lista.sel;
    if (lista.sel >= lista.top + lista.vis) lista.top = lista.sel - lista.vis + 1;
    repinta_pagina();
    return;
  }
  if (!foco_dir && pag && js_en_pagina() && vk != VK_F5 && vk != VK_F6) {   /* [JS] keydown */
    Pagina *p0 = pag;
    if (js_tecla(nodo_foco(), vk, 0) || pag != p0 || cargando) return;
    c = ctl_foco();
  }
  if (vk == VK_ESCAPE) {
    if (cargando) { http_suelta(&pet); cargando = 0; di("Carga parada."); }
    else if (icarga >= 0 || img_pendiente() >= 0) {      /* (y las imágenes que faltan, sin ellas) */
      int k;
      img_cancela();
      for (k = 0; pag && k < pag->n_img; k++) if (pag->img[k].estado == IM_PENDIENTE) pag->img[k].estado = IM_FALLO;
      remaqueta();
      di("Carga parada.");
    }
    else if (foco_dir) { foco_dir = 0; if (i_hist >= 0) dir_pon(hist[i_hist].url); repinta(); }
    return;
  }
  if (vk == VK_F5) { recarga(); return; }
  if (vk == VK_F6) { foco_dir = 1; dir_sel = 1; foco = -1; dir.cursor = dir.n_valor; dir.desp = 0; repinta(); return; }
  if (foco_dir) {
    if (vk == VK_RETURN) { foco_dir = 0; ir_dir(dir.valor); repinta(); return; }
    if (vk == VK_TAB) { foco_dir = 0; if (pag && pag->n_foco) foco_pon((GetKeyState(0x10) & 0x8000) ? pag->n_foco - 1 : 0); else repinta(); return; }
    if ((vk == VK_BACK || vk == VK_DELETE || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END) && edita(&dir, vk, 0)) {
      HANDLE dc = GetDC(vent);
      pinta_dir(dc);
      ReleaseDC(vent, dc);
    }
    return;
  }
  if (!pag) return;
  if (vk == VK_TAB) {
    if (!pag->n_foco) return;
    if (GetKeyState(0x10) & 0x8000) foco_pon(foco > 0 ? foco - 1 : pag->n_foco - 1);   /* Mayús+Tab: hacia atrás */
    else foco_pon(foco + 1 < pag->n_foco ? foco + 1 : 0);
    return;
  }
  if (es_texto(c)) {
    if (c->tipo == C_AREA) {
      int lc, total = area_linea(c, -1, 0, NULL);
      if (vk == VK_RETURN) { if (edita(c, 0, '\n')) { Nodo *n = c->nodo; repinta_ctl(c); js_evento(n, "input", NULL); } return; }
      if (vk == VK_UP || vk == VK_DOWN) {
        int ini, col;
        area_linea(c, -2, c->cursor, &lc);
        ini = area_linea(c, lc, 0, NULL);
        col = c->cursor - ini;
        lc += vk == VK_UP ? -1 : 1;
        if (lc < 0 || lc >= total) return;
        ini = area_linea(c, lc, 0, NULL);
        e = lc + 1 < total ? area_linea(c, lc + 1, 0, NULL) : c->n_valor;
        if (e > ini && c->valor[e - 1] == '\n') e--;
        c->cursor = ini + col > e ? e : ini + col;
        repinta_ctl(c);
        return;
      }
    } else if (vk == VK_RETURN) { if (c->tipo == C_ARCHIVO) elige_archivo(c); else envia(c); return; }
    if (vk == VK_BACK || vk == VK_DELETE || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END) {
      int n0 = c->n_valor;
      if (edita(c, vk, 0)) { Nodo *n = c->nodo; repinta_ctl(c); if (c->n_valor != n0) js_evento(n, "input", NULL); }   /* [JS] */
      return;
    }
  }
  switch (vk) {
  case VK_RETURN:
    if ((e = enl_foco()) >= 0) abre_enlace(e);
    else if (c) pulsa_ctl((int)(c - pag->ctl));
    return;
  case VK_UP: desplaza(sx, sy - 1); return;
  case VK_DOWN: desplaza(sx, sy + 1); return;
  case VK_PRIOR: desplaza(sx, sy - (filas_vis - 2)); return;
  case VK_NEXT: desplaza(sx, sy + (filas_vis - 2)); return;
  case VK_HOME: desplaza(0, 0); return;
  case VK_END: desplaza(sx, pag->n_filas); return;
  case VK_LEFT: desplaza(sx - 8, sy); return;
  case VK_RIGHT: desplaza(sx + 8, sy); return;
  case VK_BACK: atras(); return;
  }
}
static void letra(int ch) {
  Ctl *c = ctl_foco();
  if (ch == 12) { tecla(VK_F6); return; }       /* Ctrl+L: a la dirección */
  if (ch == 18) { recarga(); return; }          /* Ctrl+R */
  if (ch < 32) return;
  if (lista.activa) {                           /* la opción siguiente que empieza por esa letra */
    Ctl *l = &pag->ctl[lista.ctl];
    int k;
    char b[80];
    for (k = 1; k <= l->n_opc; k++) {
      int o = (lista.sel + k) % l->n_opc;
      b[0] = 0;
      texto_de(l->opc[o], b, sizeof b);
      if (min_c(b[b[0] == ' ']) == min_c(ch)) { lista.sel = o; break; }
    }
    if (lista.sel < lista.top) lista.top = lista.sel;
    if (lista.sel >= lista.top + lista.vis) lista.top = lista.sel - lista.vis + 1;
    repinta_pagina();
    return;
  }
  if (foco_dir) {
    if (edita(&dir, 0, ch)) { HANDLE dc = GetDC(vent); pinta_dir(dc); ReleaseDC(vent, dc); }
    return;
  }
  if (c && js_en_pagina() && ch != ' ' ) {     /* [JS] keydown de una letra */
    Pagina *p0 = pag;
    if (js_tecla(c->nodo, 0, ch) || pag != p0 || cargando) return;
    c = ctl_foco();
  }
  if (es_texto(c)) { if (edita(c, 0, ch)) { Nodo *n = c->nodo; repinta_ctl(c); js_evento(n, "input", NULL); } return; }   /* [JS] input */
  if (ch == ' ') {
    if (c && (c->tipo == C_CASILLA || c->tipo == C_RADIO || c->tipo == C_LISTA)) pulsa_ctl((int)(c - pag->ctl));
    else if (pag) desplaza(sx, sy + (filas_vis - 2));
  }
}
/* qué hay en un punto de la página: el atributo, o NULL */
static Atr *en_punto(int px, int py, int *col, int *fil) {
  int c, f;
  if (!pag || py < BARRA_H || py >= BARRA_H + filas_vis * LH || px < 0 || px >= an - DESP_W) return NULL;
  c = sx + px / LW; f = sy + (py - BARRA_H) / LH;
  if (col) *col = c;
  if (fil) *fil = f;
  if (f >= pag->n_filas || c >= pag->filas[f].n) return NULL;
  return &pag->atrs[pag->filas[f].a[c]];
}
static int en_rect(const RECT *r, int x, int y) { return x >= r->left && x < r->right && y >= r->top && y < r->bottom; }
static void arrastre(int py) {
  int h = al - BARRA_H - ESTADO_H, total = pag ? pag->n_filas : 0;
  if (total <= filas_vis || h < 40) return;
  desplaza(sx, (py - BARRA_H) * total / h - filas_vis / 2);
}
static void clic(int px, int py) {
  Atr *a;
  int col, fil, k;
  if (lista.activa) {
    Ctl *l = &pag->ctl[lista.ctl];
    if (px >= lista.x && px < lista.x + lista.w && py >= lista.y && py < lista.y + lista.vis * LH + 2) {
      k = lista.top + (py - lista.y - 1) / LH;
      if (k >= 0 && k < l->n_opc) { lista.sel = k; lista_cierra(1); }
    } else lista_cierra(0);
    return;
  }
  if (py < BARRA_H) {
    for (k = 0; k < B_N; k++)
      if (en_rect(&b_rect[k], px, py)) {
        foco_dir = 0;
        if (k == B_ATRAS) atras();
        else if (k == B_ADELANTE) adelante();
        else if (k == B_RECARGA) recarga();
        else if (k == B_INICIO) ir(inicio, 0, NULL, 0, 0);
        else ir_dir(dir.valor);
        repinta();
        return;
      }
    if (en_rect(&dir_rect, px, py)) {
      dir_sel = !foco_dir;                         /* el primer clic la elige entera; otro pone el cursor */
      foco_dir = 1; foco = -1;
      dir.cursor = dir.desp + (px - dir_rect.left - LW + LW / 2) / LW;
      if (dir.cursor > dir.n_valor) dir.cursor = dir.n_valor;
      if (dir.cursor < 0) dir.cursor = 0;
      repinta();
    }
    return;
  }
  if (px >= an - DESP_W && py < al - ESTADO_H) { arrastra = 1; arrastre(py); return; }
  a = en_punto(px, py, &col, &fil);
  if (a && a->ctl >= 0) {
    Ctl *c = &pag->ctl[a->ctl];
    pulsa_ctl_en(a->ctl, col);
    if (c->tipo == C_TEXTO || c->tipo == C_CLAVE || (c->tipo == C_ARCHIVO && col < c->x + c->w - ARCH_BOTON)) {
      c->cursor = c->desp + col - c->x - 1;
      if (c->cursor > c->n_valor) c->cursor = c->n_valor;
      if (c->cursor < 0) c->cursor = 0;
      repinta_ctl(c);
    } else if (c->tipo == C_AREA) {
      int total = area_linea(c, -1, 0, NULL), l = c->desp + fil - c->y, ini, fin;
      if (l >= total) l = total - 1;
      ini = area_linea(c, l, 0, NULL);
      fin = l + 1 < total ? area_linea(c, l + 1, 0, NULL) : c->n_valor;
      if (fin > ini && c->valor[fin - 1] == '\n') fin--;
      c->cursor = ini + col - c->x - 1;
      if (c->cursor > fin) c->cursor = fin;
      if (c->cursor < ini) c->cursor = ini;
      repinta_ctl(c);
    }
    return;
  }
  if (a && a->enlace >= 0) {
    for (k = 0; k < pag->n_foco; k++) if (pag->foco[k].enlace && pag->foco[k].n == a->enlace) foco = k;
    foco_dir = 0;
    abre_enlace(a->enlace);
    return;
  }
  if (foco >= 0 || foco_dir) { foco = -1; foco_dir = 0; repinta(); js_foco(NULL); }
  if (js_en_pagina() && pag->doc.body) js_evento(pag->doc.body, "click", NULL);   /* [JS] un clic fuera de todo */
}
static void raton(int px, int py, int boton) {
  Atr *a;
  HANDLE dc;
  if (arrastra) {
    if (boton) arrastre(py); else arrastra = 0;
    return;
  }
  a = lista.activa ? NULL : en_punto(px, py, NULL, NULL);
  if ((a && a->enlace >= 0 ? a->enlace : -1) == sobre_enl) return;
  sobre_enl = a && a->enlace >= 0 ? a->enlace : -1;
  sobre[0] = 0;
  if (sobre_enl >= 0 && pag->enl[sobre_enl].href) {
    char *u = url_absoluta(pag->doc.base ? pag->doc.base : pag->url, pag->enl[sobre_enl].href);
    if (u) { sprintf(sobre, "%.190s", u); free(u); }
  }
  dc = GetDC(vent);
  pinta_estado(dc);
  ReleaseDC(vent, dc);
}

// [RUEDA] la rueda del ratón (WM_MOUSEWHEEL: 120 por paso, positivo hacia
// arriba): 3 filas por paso; con una lista desplegada abierta, 3 opciones
static void rueda(int delta) {
  static int resto;
  int pasos;
  resto += delta;
  pasos = resto / 120;
  resto -= pasos * 120;
  if (!pasos || !pag) return;
  if (lista.activa) {
    Ctl *c = &pag->ctl[lista.ctl];
    int t = lista.top - 3 * pasos;
    if (t > c->n_opc - lista.vis) t = c->n_opc - lista.vis;
    if (t < 0) t = 0;
    if (t != lista.top) { lista.top = t; repinta_pagina(); }
    return;
  }
  desplaza(sx, sy - 3 * pasos);
}
/* =============================================================================
 *  [JS] JavaScript: lo que la página hace al pulsar, escribir y enfocar, y lo
 *  que JavaScript pide a la ventana (script.c, docs/486/NAVEGADOR.md)
 * ============================================================================= */
/* el nodo del foco: el de su control o su enlace (NULL: nadie) */
static Nodo *nodo_foco(void) {
  if (!pag || foco < 0 || foco >= pag->n_foco) return NULL;
  return pag->foco[foco].enlace ? pag->enl[pag->foco[foco].n].nodo : pag->ctl[pag->foco[foco].n].nodo;
}
static int foco_de_nodo(Nodo *n) {
  int k;
  if (!pag || !n) return -1;
  for (k = 0; k < pag->n_foco; k++)
    if ((pag->foco[k].enlace ? pag->enl[pag->foco[k].n].nodo : pag->ctl[pag->foco[k].n].nodo) == n) return k;
  return -1;
}
static int ctl_de_nodo(Nodo *n) {
  int k;
  for (k = 0; pag && k < pag->n_ctl; k++) if (pag->ctl[k].nodo == n) return k;
  return -1;
}
/* el clic en un enlace (o en algo que se pulsa con JavaScript): 1 si ya está
 * todo hecho (la página lo ha impedido, o ha cambiado de página, o no hay
 * nada más que hacer); si no, *e es el enlace (otra vez, por si la página se
 * ha vuelto a maquetar) */
static int js_clic_enlace(int *e) {
  Enlace *en = &pag->enl[*e];
  Nodo *n = en->nodo;
  const char *h = en->href;
  Pagina *p0 = pag;
  int k;
  if (!js_en_pagina()) return 0;
  if (js_evento(n, "mousedown", NULL) || pag != p0 || cargando) return 1;
  if (js_evento(n, "click", NULL) || pag != p0 || cargando) return 1;
  for (k = 0; k < pag->n_enl && pag->enl[k].nodo != n; k++) ;
  if (k == pag->n_enl) return 1;
  *e = k;
  h = pag->enl[k].href;
  if (h && !_strnicmp(h, "javascript:", 11)) { js_enlace(h); return 1; }
  if (!h && pag_clicable && pag_clicable(n)) return 1;   /* (solo tenía el clic de la página) */
  return 0;
}
/* pulsar un control: el foco, mousedown, click (y en las casillas, input y
 * change). 1 si ya está todo hecho; si no, *idx es el control otra vez */
static int js_pulsa(int *idx) {
  Ctl *c = &pag->ctl[*idx];
  Nodo *n = c->nodo;
  Pagina *p0 = pag;
  int tipo = c->tipo, antes = c->marcado, k;
  if (!js_en_pagina()) return 0;
  k = foco_de_ctl(*idx);
  if (k >= 0 && k != foco) { foco = k; foco_dir = 0; repinta(); js_foco(n); if (pag != p0 || cargando) return 1; }
  if (js_evento(n, "mousedown", NULL) || pag != p0 || cargando) return 1;
  if ((k = ctl_de_nodo(n)) < 0) return 1;
  c = &pag->ctl[k];
  if (tipo == C_CASILLA || tipo == C_RADIO) {
    int j;
    if (tipo == C_RADIO)
      for (j = 0; j < pag->n_ctl; j++) {
        Ctl *o = &pag->ctl[j];
        if (o->tipo == C_RADIO && o->forma == c->forma && o->nombre && c->nombre && !strcmp(o->nombre, c->nombre)) o->marcado = 0;
      }
    c->marcado = tipo == C_RADIO ? 1 : !antes;
    repinta();
    if (js_evento(n, "click", NULL)) {           /* impedido: como estaba */
      if ((k = ctl_de_nodo(n)) >= 0) pag->ctl[k].marcado = (u8)antes;
      repinta();
      return 1;
    }
    if (pag != p0 || cargando) return 1;
    if ((k = ctl_de_nodo(n)) >= 0 && pag->ctl[k].marcado != antes) { js_evento(n, "input", NULL); js_evento(n, "change", NULL); }
    return 1;
  }
  if (js_evento(n, "click", NULL) || pag != p0 || cargando) return 1;
  if ((k = ctl_de_nodo(n)) < 0) return 1;
  *idx = k;
  if (tipo == C_BOTON) return 1;
  return 0;
}
/* antes de enviar un formulario: el evento submit. 1 si no
 * hay que enviarlo; si no, *boton es el control otra vez */
static int js_envia(Ctl **boton) {
  Nodo *bn = (*boton)->nodo, *forma = (*boton)->forma, *sub = (*boton)->tipo == C_ENVIAR ? bn : NULL;
  Pagina *p0 = pag;
  int k;
  if (!js_en_pagina()) return 0;              /* (lo obligatorio ya lo ha mirado envia: valida) */
  if (js_evento(forma, "submit", sub) || pag != p0 || cargando) return 1;
  if ((k = ctl_de_nodo(bn)) < 0) return 1;
  *boton = &pag->ctl[k];
  return 0;
}
/* lo que JavaScript pide a la ventana */
static void jn_remaqueta(void) {
  Nodo *n = nodo_foco();
  lista.activa = 0;
  remaqueta();
  foco = foco_de_nodo(n);
}
static void jn_ir(const char *url, int post, char *cuerpo, u32 nc) { ir2(url, post, cuerpo, nc, 0, NULL, pag ? pag->url : NULL); }
static void jn_envia(Nodo *forma, Nodo *boton) {   /* form.submit(): sin validar, como los demás */
  char *url, *cuerpo, tipo[96];
  u32 nc;
  int post;
  if (!pag_formulario(pag, forma, boton ? ctl_de_nodo(boton) : -1, &url, &cuerpo, &nc, &post, tipo)) { di("Este formulario no se puede enviar."); return; }
  ir2(url, post, cuerpo, nc, 0, tipo[0] ? tipo : NULL, pag->url);
  free(url);
}
static void jn_pon_url(const char *u) {
  char *q = malloc(strlen(u) + 1);
  dir_pon(u);
  if (q && i_hist >= 0) { strcpy(q, u); free(hist[i_hist].url); hist[i_hist].url = q; } else free(q);
  repinta();
}
static void jn_historia(int k) { if (k < 0) atras(); else if (k > 0) adelante(); else recarga(); }
static void jn_foco(Nodo *n) {
  int f = foco_de_nodo(n);
  if (!n) { if (foco >= 0) { foco = -1; repinta(); js_foco(NULL); } return; }
  if (f >= 0) foco_pon(f);
}
static int jn_cuadro(const char *t, int tipo) { return MessageBoxA(vent, t, "Navegador", tipo ? MB_YESNO : MB_OK) == IDYES; }
/* prompt(): Hispa Ventanas no tiene un cuadro para escribir; se pregunta si
 * vale la respuesta que trae la página */
static int jn_pregunta(const char *t, const char *def, char *resp, int max) {
  static char b[700];
  sprintf(b, "%.400s\n\nRespuesta: %.200s\n\n(\277Vale esta respuesta?)", t, def[0] ? def : "(nada)");
  if (MessageBoxA(vent, b, "Navegador", MB_YESNO) != IDYES) return 0;
  sprintf(resp, "%.*s", max - 1, def);
  return 1;
}
static void jn_titulo(const char *t) {
  char b[260];
  sprintf(b, "%.200s - Navegador", t[0] ? t : "Navegador");
  SetWindowTextA(vent, b);
}
static void jn_a_la_vista(Nodo *n) {
  int k;
  for (k = 0; pag && k < pag->n_foco; k++) {
    Nodo *x = pag->foco[k].enlace ? pag->enl[pag->foco[k].n].nodo : pag->ctl[pag->foco[k].n].nodo;
    if (dom_dentro(n, x)) {
      if (pag->foco[k].enlace) { Enlace *e = &pag->enl[pag->foco[k].n]; a_la_vista(e->x, e->y < 0 ? 0 : e->y, 1); }
      else { Ctl *c = &pag->ctl[pag->foco[k].n]; a_la_vista(c->x, c->y, c->h); }
      return;
    }
  }
  for (k = 0; pag && k < pag->n_enl; k++)          /* (lo que se pulsa sin foco: las filas de una lista) */
    if (pag->enl[k].y >= 0 && dom_dentro(n, pag->enl[k].nodo)) { a_la_vista(pag->enl[k].x, pag->enl[k].y, 1); return; }
}
static const JsNav js_nav = { jn_remaqueta, jn_ir, jn_envia, jn_pon_url, jn_historia, jn_foco, jn_cuadro, jn_pregunta,
                              jn_titulo, di, jn_a_la_vista };

static int WINAPI proc(HANDLE h, u32 m, u32 w, u32 l) {
  switch (m) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HANDLE dc = BeginPaint(h, &ps);
    if (ps.rcPaint.top < BARRA_H) pinta_barra(dc);
    if (ps.rcPaint.bottom > BARRA_H) pinta_pagina(dc, ps.rcPaint.top, ps.rcPaint.bottom);
    if (ps.rcPaint.bottom > al - ESTADO_H) pinta_estado(dc);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_ERASEBKGND:
    return 1;
  case WM_SIZE: {
    int c0 = cols;
    coloca();
    if (pag && cols != c0) { remaqueta(); js_redimensiona(); } else repinta();
    return 0;
  }
  case WM_KEYDOWN:
    TlsAzar(&l, 4);
    tecla((int)w);
    return 0;
  case WM_CHAR:
    letra((int)w);
    return 0;
  case WM_LBUTTONDOWN:
    TlsAzar(&l, 4);
    clic((short)(l & 0xFFFF), (short)(l >> 16));
    return 0;
  case WM_LBUTTONUP:
    arrastra = 0;
    return 0;
  case WM_MOUSEMOVE:
    raton((short)(l & 0xFFFF), (short)(l >> 16), w & 1);
    return 0;
  case WM_TIMER:
    if (w == JS_TIMER) js_tic();                /* [JS] setTimeout */
    if (w == T_REFRESCO) {
      KillTimer(h, T_REFRESCO);
      if (refresco && !cargando) { char *u = refresco; refresco = NULL; ir2(u, 0, NULL, 0, 0, NULL, u); free(u); }
    }
    return 0;
  case 0x020A:                 // [RUEDA] WM_MOUSEWHEEL
    rueda((short)(w >> 16));
    return 0;
  case WM_DESTROY:
    guarda_galletas();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(h, m, w, l);
}

/* NAVEGA.INI, junto al programa: Inicio=dirección, Idioma=es, AlContenido=NO,
 * Favorito=nombre|dirección */
static int guardar_galletas = 1;
/* las galletas que caducan, al disco (al cerrar, y cuando cambian) */
static void guarda_galletas(void) {
  char ruta[200];
  if (!http_galletas_cambio) return;
  sprintf(ruta, "%sGALLETAS.TXT", carpeta);
  if (!guardar_galletas) { http_galletas_cambio = 0; return; }   /* (Galletas=NO en NAVEGA.INI) */
  http_galletas_guarda(ruta);
}
static void lee_ini(void) {
  char ruta[200], l[400];
  FILE *f;
  u32 n = GetModuleFileNameA(NULL, carpeta, sizeof carpeta - 1), k;
  carpeta[n] = 0;
  for (k = n; k > 0 && carpeta[k - 1] != '\\' && carpeta[k - 1] != ':'; k--) ;
  carpeta[k] = 0;
  /* la carpeta de Hispa-DOS (\HISPADOS\, la de encima de INTERNET\), la de
   * las descargas y las galletas guardadas */
  if (k >= 9 && !_stricmp(carpeta + k - 9, "INTERNET\\")) sprintf(dir_hispados, "%.*s", k - 9, carpeta);
  else strcpy(dir_hispados, "C:\\HISPADOS\\");
  if ((carpeta[0] | 32) == 'c') strcpy(dir_descargas, "C:\\");
  sprintf(ruta, "%sGALLETAS.TXT", carpeta);
  http_galletas_carga(ruta);
  sprintf(ruta, "%sNAVEGA.INI", carpeta);
  if (!(f = fopen(ruta, "r"))) return;
  while (fgets(l, sizeof l, f)) {
    char *fin;
    for (fin = l; *fin && *fin != '\r' && *fin != '\n'; fin++) ;
    *fin = 0;
    if (!_strnicmp(l, "Inicio=", 7) && l[7]) sprintf(inicio, "%.290s", l + 7);
    if (!_strnicmp(l, "Idioma=", 7) && l[7]) sprintf(http_idioma, "%.30s", l + 7);
    if (!_strnicmp(l, "AlContenido=", 12)) al_contenido = !(l[12] == 'N' || l[12] == 'n' || l[12] == '0');
    if (!_strnicmp(l, "Descargas=", 10) && l[10]) sprintf(dir_descargas, "%.140s", l + 10);
    if (!_strnicmp(l, "Galletas=", 9) && (l[9] == 'N' || l[9] == 'n' || l[9] == '0')) guardar_galletas = 0;
    if (!_strnicmp(l, "JavaScript=", 11)) js_activo = !(l[11] == 'N' || l[11] == 'n' || l[11] == '0');   /* [JS] */
    if (!_strnicmp(l, "TiempoJS=", 9) && atoi(l + 9) > 0) js_tope_ms = atoi(l + 9) * 1000;
    if (!_strnicmp(l, "MemoriaJS=", 10) && atoi(l + 10) >= 512) js_memoria_kb = atoi(l + 10);
  }
  fclose(f);
}
void inicio_prog(void) {
  WNDCLASSA wc;
  MSG m;
  HANDLE inst = GetModuleHandleA(NULL);
  const char *cmd = GetCommandLineA();
  int w = GetSystemMetrics(0), h = GetSystemMetrics(17);
  lee_ini();
  memset(&wc, 0, sizeof wc);
  wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = "HispaNavegador";
  RegisterClassA(&wc);
  ctl_pon(&dir, "", 0);
  dir.max = 1000;
  if (w > 1024) w = 1024;
  if (h > 720) h = 720;
  vent = CreateWindowExA(0, "HispaNavegador", "Navegador", WS_OVERLAPPEDWINDOW, 8, 30, w - 16, h - 14, NULL, NULL, inst, NULL);
  if (!vent) ExitProcess(1);
  ShowWindow(vent, 3);                           /* a toda la pantalla: la página necesita sitio */
  coloca();
  UpdateWindow(vent);
  js_inicia(vent, &js_nav, carpeta);             /* [JS] */
  /* la dirección de la línea de órdenes, si la hay; si no, el inicio */
  if (*cmd == '"') { for (cmd++; *cmd && *cmd != '"'; cmd++) ; if (*cmd) cmd++; }
  else while (*cmd && *cmd != ' ') cmd++;
  while (*cmd == ' ') cmd++;
  if (*cmd) ir_dir(cmd); else ir(inicio, 0, NULL, 0, 0);
  for (;;) {
    if (cargando || js_ocupado() || img_trabajo()) {
      while (PeekMessageA(&m, NULL, 0, 0, 1)) {
        if (m.message == WM_QUIT) goto fin;
        TranslateMessage(&m);
        DispatchMessageA(&m);
      }
      if (cargando) {
        carga_avanza();
        if (!cargando) guarda_galletas();
      } else if (icarga < 0 && js_ocupado()) js_avanza();   /* [JS] guiones que llegan, fetch (antes que las imágenes; la conexión es una) */
      else if (img_trabajo()) img_avanza();
    } else {
      if (!GetMessageA(&m, NULL, 0, 0)) break;
      TranslateMessage(&m);
      DispatchMessageA(&m);
    }
  }
fin:
  if (cargando) http_suelta(&pet);
  img_cancela();
  js_fin();                                     /* [JS] */
  guarda_galletas();
  http_fin();
  ExitProcess(0);
}
