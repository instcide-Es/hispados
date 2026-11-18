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
#define WM_KEYDOWN 0x0100
#define WM_CHAR 0x0102
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
typedef struct FILE FILE;
FILE *fopen(const char *, const char *);
char *fgets(char *, int, FILE *);
int fclose(FILE *);

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
static int es_texto(const Ctl *c) { return c && (c->tipo == C_TEXTO || c->tipo == C_CLAVE || c->tipo == C_AREA); }
/* el texto de un botón o de la opción elegida de una lista */
static void rotulo(Ctl *c, char *b, int max) {
  int n;
  b[0] = 0;
  if (c->tipo == C_LISTA) { if (c->n_opc) texto_de(c->opc[c->elegida], b, max); }
  else if (c->nodo->etq == E_BUTTON) texto_de(c->nodo, b, max);
  else { n = c->n_valor < max - 1 ? c->n_valor : max - 1; memcpy(b, c->valor, n); b[n] = 0; }
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
  case C_TEXTO: case C_CLAVE: {
    int vis = c->w - 2;
    const char *ph = atr(c->nodo, "placeholder"), *tp = atr(c->nodo, "type");
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
  case C_ARCHIVO:
    txt(dc, x, y + 1, "[no se pueden subir archivos]", c->w, P(P_ARENA_OSC), 0);
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
  if (!p) return;
  lista.activa = 0;
  if (pag) pag_suelta(pag);
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
  repinta();
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
    "<li><b>F6</b> lleva a la direcci\363n, <b>F5</b> recarga y <b>Esc</b> para la carga.</li></ul>"
    "<p><small>Navegador 0.1. Entiende HTML sencillo y formularios; no ejecuta JavaScript ni ense\361a im\341genes. "
    "Las conexiones https van cifradas con TLS 1.2 y se comprueba el certificado del servidor.</small></p>");
  pagina_propia("about:inicio", h);
}
static void ir(const char *url, int post, char *cuerpo, u32 n_cuerpo, int modo) {
  char *u;
  if (cargando) { http_suelta(&pet); cargando = 0; }
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
/* un paso de la carga: se llama desde el bucle de mensajes */
static void carga_avanza(void) {
  static const char *const fases[] = { "Abriendo...", "Buscando el servidor...", "Conectando con el servidor...",
    "Abriendo la conexi\363n segura...", "Enviando la petici\363n...", "Esperando la respuesta...", "Recibiendo la p\341gina..." };
  int r = http_avanza(&pet);
  if (r == 0) {
    char b[120];
    if (pet.fase == H_CUERPO && pet.n_datos) { sprintf(b, "Recibiendo la p\341gina... (%u K)", (pet.n_datos + 512) / 1024); di(b); }
    else if (pet.fase < 7) di(fases[pet.fase]);
    return;
  }
  cargando = 0;
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
      if ((pet.estado == 307 || pet.estado == 308) && pet.post) { post = 1; cuerpo = pet.cuerpo; nc = pet.n_cuerpo; pet.cuerpo = NULL; }
      http_suelta(&pet);
      if (strchr(nueva, '#')) *strchr(nueva, '#') = 0;
      pet.url = nueva; pet.post = post; pet.cuerpo = cuerpo; pet.n_cuerpo = nc;
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
static void foco_pon(int f) {
  foco = f;
  foco_dir = 0;
  if (pag && f >= 0 && f < pag->n_foco) {
    if (pag->foco[f].enlace) { Enlace *e = &pag->enl[pag->foco[f].n]; a_la_vista(e->x, e->y < 0 ? 0 : e->y, 1); }
    else { Ctl *c = &pag->ctl[pag->foco[f].n]; a_la_vista(c->x, c->y, c->h); }
  }
  repinta();
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
static void abre_enlace(int e) {
  Enlace *en = &pag->enl[e];
  char b[300], *u;
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
  if (pag_confirma(en->nodo, b, sizeof b) && MessageBoxA(vent, b, "Navegador", MB_YESNO) != IDYES) return;
  if (en->href[0] == '#') { desplaza(sx, 0); return; }
  u = url_absoluta(pag->doc.base ? pag->doc.base : pag->url, en->href);
  if (!u) { di("Este enlace no se puede abrir aqu\355 (no es una p\341gina web)."); return; }
  ir(u, 0, NULL, 0, 0);
  free(u);
}
static void envia(Ctl *boton) {
  Nodo *forma = boton->forma;
  char b[300], *url, *cuerpo;
  u32 nc;
  int k, post, con = boton->tipo == C_ENVIAR ? (int)(boton - pag->ctl) : -1;
  if (!forma) return;
  if (con < 0)                                   /* Intro en un campo: como pulsar el primer botón de enviar */
    for (k = 0; k < pag->n_ctl; k++)
      if (pag->ctl[k].forma == forma && pag->ctl[k].tipo == C_ENVIAR && !pag->ctl[k].desactivado) { con = k; break; }
  for (k = 0; k < pag->n_ctl; k++) {
    Ctl *c = &pag->ctl[k];
    if (c->forma == forma && c->obligado && !c->desactivado && c->visible && es_texto(c) && !c->n_valor) {
      MessageBoxA(vent, "Falta rellenar un campo obligatorio.", "Navegador", MB_OK);
      foco_pon(foco_de_ctl(k));
      return;
    }
  }
  if ((pag_confirma(forma, b, sizeof b) || (con >= 0 && pag_confirma(pag->ctl[con].nodo, b, sizeof b)))
      && MessageBoxA(vent, b, "Navegador", MB_YESNO) != IDYES) return;
  if (!pag_formulario(pag, forma, con, &url, &cuerpo, &nc, &post)) { di("Este formulario no se puede enviar."); return; }
  ir(url, post, cuerpo, nc, 0);
  free(url);
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
  if (cambia && c->cambia_envia) envia(c);
}
static void pulsa_ctl(int idx) {
  Ctl *c = &pag->ctl[idx];
  int k;
  if (c->desactivado) return;
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
    di("Este bot\363n necesita JavaScript, que el Navegador no tiene.");
    break;
  case C_LISTA:
    repinta();
    lista_abre(idx);
    return;
  }
  repinta();
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
  if (vk == VK_ESCAPE) {
    if (cargando) { http_suelta(&pet); cargando = 0; di("Carga parada."); }
    else if (foco_dir) { foco_dir = 0; if (i_hist >= 0) dir_pon(hist[i_hist].url); repinta(); }
    return;
  }
  if (vk == VK_F5) { recarga(); return; }
  if (vk == VK_F6) { foco_dir = 1; dir_sel = 1; foco = -1; dir.cursor = dir.n_valor; dir.desp = 0; repinta(); return; }
  if (foco_dir) {
    if (vk == VK_RETURN) { foco_dir = 0; ir_dir(dir.valor); repinta(); return; }
    if (vk == VK_TAB) { foco_dir = 0; if (pag && pag->n_foco) foco_pon(0); else repinta(); return; }
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
    foco_pon(foco + 1 < pag->n_foco ? foco + 1 : 0);
    return;
  }
  if (es_texto(c)) {
    if (c->tipo == C_AREA) {
      int lc, total = area_linea(c, -1, 0, NULL);
      if (vk == VK_RETURN) { if (edita(c, 0, '\n')) repinta_ctl(c); return; }
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
    } else if (vk == VK_RETURN) { envia(c); return; }
    if (vk == VK_BACK || vk == VK_DELETE || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_HOME || vk == VK_END) {
      if (edita(c, vk, 0)) repinta_ctl(c);
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
  if (es_texto(c)) { if (edita(c, 0, ch)) repinta_ctl(c); return; }
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
    pulsa_ctl(a->ctl);
    if (c->tipo == C_TEXTO || c->tipo == C_CLAVE) {
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
  if (foco >= 0 || foco_dir) { foco = -1; foco_dir = 0; repinta(); }
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
    if (pag && cols != c0) remaqueta(); else repinta();
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
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(h, m, w, l);
}

/* NAVEGA.INI, junto al programa: Inicio=dirección, Idioma=es, AlContenido=NO,
 * Favorito=nombre|dirección */
static void lee_ini(void) {
  char ruta[200], l[400];
  FILE *f;
  u32 n = GetModuleFileNameA(NULL, carpeta, sizeof carpeta - 1), k;
  carpeta[n] = 0;
  for (k = n; k > 0 && carpeta[k - 1] != '\\' && carpeta[k - 1] != ':'; k--) ;
  carpeta[k] = 0;
  sprintf(ruta, "%sNAVEGA.INI", carpeta);
  if (!(f = fopen(ruta, "r"))) return;
  while (fgets(l, sizeof l, f)) {
    char *fin;
    for (fin = l; *fin && *fin != '\r' && *fin != '\n'; fin++) ;
    *fin = 0;
    if (!_strnicmp(l, "Inicio=", 7) && l[7]) sprintf(inicio, "%.290s", l + 7);
    if (!_strnicmp(l, "Idioma=", 7) && l[7]) sprintf(http_idioma, "%.30s", l + 7);
    if (!_strnicmp(l, "AlContenido=", 12)) al_contenido = !(l[12] == 'N' || l[12] == 'n' || l[12] == '0');
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
  /* la dirección de la línea de órdenes, si la hay; si no, el inicio */
  if (*cmd == '"') { for (cmd++; *cmd && *cmd != '"'; cmd++) ; if (*cmd) cmd++; }
  else while (*cmd && *cmd != ' ') cmd++;
  while (*cmd == ' ') cmd++;
  if (*cmd) ir_dir(cmd); else ir(inicio, 0, NULL, 0, 0);
  for (;;) {
    if (cargando) {
      while (PeekMessageA(&m, NULL, 0, 0, 1)) {
        if (m.message == WM_QUIT) goto fin;
        TranslateMessage(&m);
        DispatchMessageA(&m);
      }
      if (cargando) carga_avanza();
    } else {
      if (!GetMessageA(&m, NULL, 0, 0)) break;
      TranslateMessage(&m);
      DispatchMessageA(&m);
    }
  }
fin:
  if (cargando) http_suelta(&pet);
  http_fin();
  ExitProcess(0);
}
