/* comctl32.c - COMCTL32.DLL de Hispa-486 (fase 3d, docs/486/HISPA-486.md):
 * los controles comunes de Windows 95 que usan los programas sencillos de la
 * época, hechos sobre el USER32 y el GDI32 de Hispa Ventanas:
 *  - la barra de estado (msctls_statusbar32, CreateStatusWindowA): partes
 *    (SB_SETPARTS), textos (SB_SETTEXTA), modo sencillo (SB_SIMPLE); se pega
 *    abajo de su madre cuando esta cambia de tamaño;
 *  - las pestañas (SysTabControl32): TCM_INSERTITEMA, TCM_SETITEMA,
 *    TCM_GETITEMA (con los octetos de más: TCM_SETITEMEXTRA), TCM_DELETEITEM,
 *    TCM_GETCURSEL, TCM_SETCURSEL, TCM_ADJUSTRECT, TCM_GETITEMRECT,
 *    TCM_HITTEST; con un clic, TCN_SELCHANGING y TCN_SELCHANGE (WM_NOTIFY);
 *    arriba o abajo (TCS_BOTTOM);
 *  - la barra de progreso (msctls_progress32): PBM_SETRANGE, PBM_SETPOS,
 *    PBM_DELTAPOS, PBM_SETSTEP, PBM_STEPIT;
 *  - las listas de imágenes (ImageList_Create, _Add, _Draw...), lo justo;
 *  - InitCommonControls (el número 17, por el que la importan muchos) e
 *    InitCommonControlsEx.
 * Con el aspecto de Hispa Ventanas (los colores del sistema de su USER32).
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --dll (ver el Makefile). */
typedef unsigned int u32;
typedef unsigned short u16;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { HANDLE hdc; int fErase; RECT rcPaint; int fRestore, fIncUpdate; char rgbReserved[32]; } PAINTSTRUCT;
typedef int WINAPI (*WNDPROC)(HANDLE, u32, u32, u32);
typedef struct {
  u32 style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance, hIcon, hCursor, hbrBackground;
  const char *lpszMenuName, *lpszClassName;
} WNDCLASSA;
typedef struct { HANDLE hwndFrom; u32 idFrom; int code; } NMHDR;
typedef struct { void *lpCreateParams; HANDLE hInstance, hMenu, hwndParent; int cy, cx, y, x; u32 style;
  const char *lpszName, *lpszClass; u32 dwExStyle; } CREATESTRUCTA;
typedef struct { u32 mask, dwState, dwStateMask; char *pszText; int cchTextMax, iImage; u32 lParam; } TCITEMA;
typedef struct { int bmType, bmWidth, bmHeight, bmWidthBytes; u16 bmPlanes, bmBitsPixel; void *bmBits; } BITMAP;

HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
void *WINAPI HeapReAlloc(HANDLE, u32, void *, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
u32 WINAPI RegisterClassA(const WNDCLASSA *);
HANDLE WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
int WINAPI DefWindowProcA(HANDLE, u32, u32, u32);
int WINAPI SendMessageA(HANDLE, u32, u32, u32);
int WINAPI GetClientRect(HANDLE, RECT *);
int WINAPI InvalidateRect(HANDLE, const RECT *, int);
int WINAPI MoveWindow(HANDLE, int, int, int, int, int);
HANDLE WINAPI GetParent(HANDLE);
int WINAPI GetWindowLongA(HANDLE, int);
int WINAPI SetWindowLongA(HANDLE, int, int);
int WINAPI GetDlgCtrlID(HANDLE);
HANDLE WINAPI BeginPaint(HANDLE, PAINTSTRUCT *);
int WINAPI EndPaint(HANDLE, const PAINTSTRUCT *);
int WINAPI FillRect(HANDLE, const RECT *, HANDLE);
int WINAPI DrawEdge(HANDLE, RECT *, u32, u32);
int WINAPI DrawTextA(HANDLE, const char *, int, RECT *, u32);
HANDLE WINAPI GetSysColorBrush(int);
u32 WINAPI GetSysColor(int);
int WINAPI SetBkMode(HANDLE, int);
u32 WINAPI SetTextColor(HANDLE, u32);
HANDLE WINAPI SelectObject(HANDLE, HANDLE);
int WINAPI GetObjectA(HANDLE, int, void *);
HANDLE WINAPI CreateCompatibleDC(HANDLE);
int WINAPI DeleteDC(HANDLE);
HANDLE WINAPI CreateCompatibleBitmap(HANDLE, int, int);
int WINAPI DeleteObject(HANDLE);
int WINAPI BitBlt(HANDLE, int, int, int, int, HANDLE, int, int, u32);
HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);

#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_SIZE 0x0005
#define WM_SETTEXT 0x000C
#define WM_PAINT 0x000F
#define WM_NOTIFY 0x004E
#define WM_LBUTTONDOWN 0x0201
#define WM_USER 0x0400
#define GWL_USERDATA (-21)
#define COLOR_BTNFACE 15
#define COLOR_BTNTEXT 18
#define COLOR_WINDOW 5
#define COLOR_HIGHLIGHT 13
#define BDR_SUNKENOUTER 2
#define BDR_RAISEDINNER 4
#define EDGE_RAISED 5
#define BF_RECT 15
#define BF_LEFT 1
#define BF_TOP 2
#define BF_RIGHT 4
#define BF_BOTTOM 8
#define DT_LEFT 0
#define DT_CENTER 1
#define DT_RIGHT 2
#define DT_VCENTER 4
#define DT_SINGLELINE 0x20
#define DT_NOPREFIX 0x800
#define DT_END_ELLIPSIS 0x8000
#define TRANSPARENT 1
#define CCS_TOP 1
#define CCS_NOPARENTALIGN 8
#define CCS_NORESIZE 4

static HANDLE monton;
static void *pide(u32 n)
{
  if (!monton) monton = GetProcessHeap();
  return HeapAlloc(monton, 8, n);       /* HEAP_ZERO_MEMORY */
}
static void suelta(void *p) { if (p) HeapFree(monton, 0, p); }
static int largo(const char *s) { int n = 0; if (s) while (s[n]) n++; return n; }
static char *copia(const char *s)
{
  int n = largo(s), i;
  char *d = pide(n + 1);
  if (d) { for (i = 0; i < n; i++) d[i] = s[i]; d[n] = 0; }
  return d;
}
static void *datos(HANDLE w) { return (void *)GetWindowLongA(w, GWL_USERDATA); }

/* ---- la barra de estado ---- */
#define SB_SETTEXTA (WM_USER + 1)
#define SB_GETTEXTA (WM_USER + 2)
#define SB_GETTEXTLENGTHA (WM_USER + 3)
#define SB_SETPARTS (WM_USER + 4)
#define SB_GETPARTS (WM_USER + 6)
#define SB_GETBORDERS (WM_USER + 7)
#define SB_SETMINHEIGHT (WM_USER + 8)
#define SB_SIMPLE (WM_USER + 9)
#define SB_GETRECT (WM_USER + 10)
#define SB_ISSIMPLE (WM_USER + 14)
#define SB_PARTES 16
#define SB_ALTO 24
typedef struct { int n, simple; int bordes[SB_PARTES]; char *texto[SB_PARTES + 1]; } ESTADO;

static void sb_colocar(HANDLE w)
{
  RECT r; HANDLE m = GetParent(w);
  u32 st = GetWindowLongA(w, -16);
  if (!m || (st & (CCS_NOPARENTALIGN | CCS_NORESIZE))) return;
  GetClientRect(m, &r);
  if (st & CCS_TOP) MoveWindow(w, 0, 0, r.right, SB_ALTO, 1);
  else MoveWindow(w, 0, r.bottom - SB_ALTO, r.right, SB_ALTO, 1);
}
static void sb_rect(HANDLE w, ESTADO *e, int i, RECT *r)
{
  RECT c; GetClientRect(w, &c);
  r->top = 2; r->bottom = c.bottom - 1;
  r->left = i == 0 ? 2 : e->bordes[i - 1] + 2;
  r->right = (e->simple || e->bordes[i] < 0 || e->bordes[i] > c.right) ? c.right - 2 : e->bordes[i];
}
static int WINAPI sb_proc(HANDLE w, u32 m, u32 wp, u32 lp)
{
  ESTADO *e = datos(w);
  switch (m) {
  case WM_CREATE: {
    CREATESTRUCTA *cs = (CREATESTRUCTA *)lp;
    e = pide(sizeof *e);
    if (!e) return -1;
    e->n = 1; e->bordes[0] = -1;
    SetWindowLongA(w, GWL_USERDATA, (int)e);
    if (cs->lpszName) e->texto[0] = copia(cs->lpszName);
    sb_colocar(w);
    return 0;
  }
  case WM_DESTROY: {
    int i;
    if (e) { for (i = 0; i <= SB_PARTES; i++) suelta(e->texto[i]); suelta(e); }
    SetWindowLongA(w, GWL_USERDATA, 0);
    return 0;
  }
  case WM_SIZE:
    sb_colocar(w);
    return 0;
  case SB_SETPARTS: {
    int i, *p = (int *)lp;
    if (!e || wp < 1 || wp > SB_PARTES) return 0;
    e->n = wp;
    for (i = 0; i < (int)wp; i++) e->bordes[i] = p[i];
    InvalidateRect(w, NULL, 1);
    return 1;
  }
  case SB_GETPARTS: {
    int i, *p = (int *)lp;
    if (!e) return 0;
    if (p) for (i = 0; i < e->n && i < (int)wp; i++) p[i] = e->bordes[i];
    return e->n;
  }
  case WM_SETTEXT:
    wp = 0; /* (sigue) */
  case SB_SETTEXTA: {
    int i = wp & 0xFF;
    if (i == 0xFF) i = SB_PARTES;
    if (!e || i > SB_PARTES) return 0;
    suelta(e->texto[i]);
    e->texto[i] = copia((const char *)lp);
    InvalidateRect(w, NULL, 1);
    return 1;
  }
  case SB_GETTEXTA:
  case SB_GETTEXTLENGTHA: {
    int i = wp & 0xFF, n;
    if (!e || i > SB_PARTES) return 0;
    n = largo(e->texto[i]);
    if (m == SB_GETTEXTA && lp) { int k; for (k = 0; k <= n; k++) ((char *)lp)[k] = k < n ? e->texto[i][k] : 0; }
    return n;
  }
  case SB_SIMPLE:
    if (e) { e->simple = wp != 0; InvalidateRect(w, NULL, 1); }
    return 1;
  case SB_ISSIMPLE:
    return e ? e->simple : 0;
  case SB_GETRECT:
    if (!e || (int)wp >= e->n) return 0;
    sb_rect(w, e, wp, (RECT *)lp);
    return 1;
  case SB_GETBORDERS: {
    int *p = (int *)lp;
    p[0] = 0; p[1] = 2; p[2] = 2;
    return 1;
  }
  case SB_SETMINHEIGHT:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps; RECT c, r; int i, n;
    HANDLE dc = BeginPaint(w, &ps);
    GetClientRect(w, &c);
    FillRect(dc, &c, GetSysColorBrush(COLOR_BTNFACE));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    n = e ? (e->simple ? 1 : e->n) : 0;
    for (i = 0; i < n; i++) {
      sb_rect(w, e, i, &r);
      DrawEdge(dc, &r, BDR_SUNKENOUTER, BF_RECT);
      r.left += 4; r.right -= 2;
      {
        const char *t = e->simple ? e->texto[SB_PARTES] : e->texto[i];
        u32 al = DT_LEFT;               /* un tabulador delante: centrado; dos: a la derecha */
        if (t && *t == '\t') { t++; al = DT_CENTER; if (*t == '\t') { t++; al = DT_RIGHT; } }
        if (t) DrawTextA(dc, t, -1, &r, al | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
      }
    }
    EndPaint(w, &ps);
    return 0;
  }
  }
  return DefWindowProcA(w, m, wp, lp);
}

HANDLE WINAPI CreateStatusWindowA(int estilo, const char *texto, HANDLE madre, u32 id)
{
  return CreateWindowExA(0, "msctls_statusbar32", texto, estilo, 0, 0, 0, 0, madre, (HANDLE)id, NULL, NULL);
}
void WINAPI DrawStatusTextA(HANDLE dc, RECT *r, const char *t, u32 f)
{
  RECT q = *r;
  DrawEdge(dc, &q, BDR_SUNKENOUTER, BF_RECT);
  q.left += 4;
  SetBkMode(dc, TRANSPARENT);
  if (t) DrawTextA(dc, t, -1, &q, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* ---- las listas de imágenes: un mapa con todas, una detrás de otra ---- */
typedef struct { int cx, cy, n, cap; HANDLE mapa; } LISTA;
#define ILD_NORMAL 0
#define SRCCOPY 0x00CC0020

HANDLE WINAPI ImageList_Create(int cx, int cy, u32 flags, int n, int crece)
{
  LISTA *l = pide(sizeof *l);
  if (!l) return NULL;
  l->cx = cx; l->cy = cy; l->n = 0;
  return l;
}
static int il_cabe(LISTA *l, int mas)
{
  HANDLE dc, a, b, va, vb, nuevo;
  int cap = l->n + mas;
  if (cap <= l->cap) return 1;
  cap += 4;
  dc = GetDC(NULL);
  nuevo = CreateCompatibleBitmap(dc, l->cx * cap, l->cy);
  if (!nuevo) { ReleaseDC(NULL, dc); return 0; }
  if (l->mapa) {
    a = CreateCompatibleDC(dc); b = CreateCompatibleDC(dc);
    va = SelectObject(a, l->mapa); vb = SelectObject(b, nuevo);
    BitBlt(b, 0, 0, l->cx * l->n, l->cy, a, 0, 0, SRCCOPY);
    SelectObject(a, va); SelectObject(b, vb); DeleteDC(a); DeleteDC(b);
    DeleteObject(l->mapa);
  }
  ReleaseDC(NULL, dc);
  l->mapa = nuevo; l->cap = cap;
  return 1;
}
int WINAPI ImageList_Add(HANDLE h, HANDLE mapa, HANDLE mascara)
{
  LISTA *l = h; BITMAP bm; HANDLE dc, a, b, va, vb; int k, i;
  if (!l || !mapa || !GetObjectA(mapa, sizeof bm, &bm)) return -1;
  k = bm.bmWidth / (l->cx ? l->cx : 1);
  if (k < 1) k = 1;
  if (!il_cabe(l, k)) return -1;
  dc = GetDC(NULL);
  a = CreateCompatibleDC(dc); b = CreateCompatibleDC(dc);
  va = SelectObject(a, mapa); vb = SelectObject(b, l->mapa);
  BitBlt(b, l->cx * l->n, 0, l->cx * k, l->cy, a, 0, 0, SRCCOPY);
  SelectObject(a, va); SelectObject(b, vb); DeleteDC(a); DeleteDC(b);
  ReleaseDC(NULL, dc);
  i = l->n; l->n += k;
  return i;
}
int WINAPI ImageList_AddMasked(HANDLE h, HANDLE mapa, u32 color) { return ImageList_Add(h, mapa, NULL); }
int WINAPI ImageList_Draw(HANDLE h, int i, HANDLE dc, int x, int y, u32 estilo)
{
  LISTA *l = h; HANDLE a, va;
  if (!l || i < 0 || i >= l->n) return 0;
  a = CreateCompatibleDC(dc); va = SelectObject(a, l->mapa);
  BitBlt(dc, x, y, l->cx, l->cy, a, l->cx * i, 0, SRCCOPY);
  SelectObject(a, va); DeleteDC(a);
  return 1;
}
int WINAPI ImageList_Destroy(HANDLE h)
{
  LISTA *l = h;
  if (!l) return 0;
  if (l->mapa) DeleteObject(l->mapa);
  suelta(l);
  return 1;
}
int WINAPI ImageList_GetImageCount(HANDLE h) { return h ? ((LISTA *)h)->n : 0; }
int WINAPI ImageList_GetIconSize(HANDLE h, int *cx, int *cy)
{
  if (!h) return 0;
  *cx = ((LISTA *)h)->cx; *cy = ((LISTA *)h)->cy;
  return 1;
}
int WINAPI ImageList_ReplaceIcon(HANDLE h, int i, HANDLE icono) { return -1; }
int WINAPI ImageList_Remove(HANDLE h, int i)
{
  if (!h) return 0;
  if (i < 0) ((LISTA *)h)->n = 0;
  return 1;
}
u32 WINAPI ImageList_SetBkColor(HANDLE h, u32 c) { return 0xFFFFFFFF; }

/* ---- las pestañas ---- */
#define TCM_FIRST 0x1300
#define TCM_GETIMAGELIST (TCM_FIRST + 2)
#define TCM_SETIMAGELIST (TCM_FIRST + 3)
#define TCM_GETITEMCOUNT (TCM_FIRST + 4)
#define TCM_GETITEMA (TCM_FIRST + 5)
#define TCM_SETITEMA (TCM_FIRST + 6)
#define TCM_INSERTITEMA (TCM_FIRST + 7)
#define TCM_DELETEITEM (TCM_FIRST + 8)
#define TCM_DELETEALLITEMS (TCM_FIRST + 9)
#define TCM_GETITEMRECT (TCM_FIRST + 10)
#define TCM_GETCURSEL (TCM_FIRST + 11)
#define TCM_SETCURSEL (TCM_FIRST + 12)
#define TCM_HITTEST (TCM_FIRST + 13)
#define TCM_SETITEMEXTRA (TCM_FIRST + 14)
#define TCM_ADJUSTRECT (TCM_FIRST + 40)
#define TCM_SETITEMSIZE (TCM_FIRST + 41)
#define TCM_GETROWCOUNT (TCM_FIRST + 44)
#define TCM_GETCURFOCUS (TCM_FIRST + 47)
#define TCM_SETCURFOCUS (TCM_FIRST + 48)
#define TCIF_TEXT 1
#define TCIF_IMAGE 2
#define TCIF_PARAM 8
#define TCS_BOTTOM 2
#define TCN_SELCHANGE (-551)
#define TCN_SELCHANGING (-552)
#define TAB_ALTO 24
#define TAB_MAX 64
typedef struct { char *texto; int imagen; u32 extra[8]; } PESTANA;
typedef struct { int n, sel, nextra; LISTA *imagenes; PESTANA p[TAB_MAX]; } PESTANAS;

static int tab_ancho(PESTANAS *t, int i)
{
  int w = largo(t->p[i].texto) * 8 + 16;
  if (t->imagenes && t->p[i].imagen >= 0) w += t->imagenes->cx + 4;
  return w;
}
static void tab_rect(HANDLE w, PESTANAS *t, int i, RECT *r)
{
  RECT c; int k, x = 2;
  GetClientRect(w, &c);
  for (k = 0; k < i; k++) x += tab_ancho(t, k);
  r->left = x; r->right = x + tab_ancho(t, i);
  if (GetWindowLongA(w, -16) & TCS_BOTTOM) { r->top = c.bottom - TAB_ALTO; r->bottom = c.bottom; }
  else { r->top = 0; r->bottom = TAB_ALTO; }
}
static void tab_avisa(HANDLE w, int codigo, int *r)
{
  NMHDR n;
  n.hwndFrom = w; n.idFrom = GetDlgCtrlID(w); n.code = codigo;
  *r = SendMessageA(GetParent(w), WM_NOTIFY, n.idFrom, (u32)&n);
}
static void tab_dar(PESTANAS *t, int i, TCITEMA *it)
{
  if (it->mask & TCIF_TEXT && it->pszText && it->cchTextMax > 0) {
    int k, n = largo(t->p[i].texto);
    if (n >= it->cchTextMax) n = it->cchTextMax - 1;
    for (k = 0; k < n; k++) it->pszText[k] = t->p[i].texto[k];
    it->pszText[n] = 0;
  }
  if (it->mask & TCIF_IMAGE) it->iImage = t->p[i].imagen;
  if (it->mask & TCIF_PARAM) {
    int k; u32 *d = &it->lParam;
    if (t->nextra <= 4) d[0] = t->p[i].extra[0];
    else for (k = 0; k < t->nextra / 4 && k < 8; k++) d[k] = t->p[i].extra[k];
  }
}
static void tab_poner(PESTANAS *t, int i, TCITEMA *it)
{
  if (it->mask & TCIF_TEXT) { suelta(t->p[i].texto); t->p[i].texto = copia(it->pszText); }
  if (it->mask & TCIF_IMAGE) t->p[i].imagen = it->iImage;
  if (it->mask & TCIF_PARAM) {
    int k; u32 *d = &it->lParam;
    if (t->nextra <= 4) t->p[i].extra[0] = d[0];
    else for (k = 0; k < t->nextra / 4 && k < 8; k++) t->p[i].extra[k] = d[k];
  }
}
static int WINAPI tab_proc(HANDLE w, u32 m, u32 wp, u32 lp)
{
  PESTANAS *t = datos(w);
  switch (m) {
  case WM_CREATE:
    t = pide(sizeof *t);
    if (!t) return -1;
    t->sel = -1; t->nextra = 4;
    SetWindowLongA(w, GWL_USERDATA, (int)t);
    return 0;
  case WM_DESTROY: {
    int i;
    if (t) { for (i = 0; i < t->n; i++) suelta(t->p[i].texto); suelta(t); }
    SetWindowLongA(w, GWL_USERDATA, 0);
    return 0;
  }
  case TCM_SETITEMEXTRA:
    if (!t || t->n || wp > 32) return 0;
    t->nextra = wp;
    return 1;
  case TCM_GETITEMCOUNT:
    return t ? t->n : 0;
  case TCM_INSERTITEMA: {
    int i = wp, k;
    if (!t || t->n >= TAB_MAX) return -1;
    if (i < 0 || i > t->n) i = t->n;
    for (k = t->n; k > i; k--) t->p[k] = t->p[k - 1];
    t->p[i].texto = NULL; t->p[i].imagen = -1;
    for (k = 0; k < 8; k++) t->p[i].extra[k] = 0;
    t->n++;
    tab_poner(t, i, (TCITEMA *)lp);
    if (t->sel >= i && t->sel >= 0 && t->n > 1) t->sel++;
    if (t->sel < 0) t->sel = 0;
    InvalidateRect(w, NULL, 1);
    return i;
  }
  case TCM_SETITEMA:
    if (!t || (int)wp < 0 || (int)wp >= t->n) return 0;
    tab_poner(t, wp, (TCITEMA *)lp);
    InvalidateRect(w, NULL, 1);
    return 1;
  case TCM_GETITEMA:
    if (!t || (int)wp < 0 || (int)wp >= t->n) return 0;
    tab_dar(t, wp, (TCITEMA *)lp);
    return 1;
  case TCM_DELETEITEM: {
    int k;
    if (!t || (int)wp < 0 || (int)wp >= t->n) return 0;
    suelta(t->p[wp].texto);
    for (k = wp; k < t->n - 1; k++) t->p[k] = t->p[k + 1];
    t->n--;
    if (t->sel >= t->n) t->sel = t->n - 1;
    InvalidateRect(w, NULL, 1);
    return 1;
  }
  case TCM_DELETEALLITEMS: {
    int k;
    if (!t) return 0;
    for (k = 0; k < t->n; k++) suelta(t->p[k].texto);
    t->n = 0; t->sel = -1;
    InvalidateRect(w, NULL, 1);
    return 1;
  }
  case TCM_GETCURSEL:
  case TCM_GETCURFOCUS:
    return t ? t->sel : -1;
  case TCM_SETCURSEL:
  case TCM_SETCURFOCUS: {
    int antes;
    if (!t || (int)wp >= t->n) return -1;
    antes = t->sel; t->sel = wp;
    InvalidateRect(w, NULL, 1);
    return antes;
  }
  case TCM_GETITEMRECT:
    if (!t || (int)wp < 0 || (int)wp >= t->n) return 0;
    tab_rect(w, t, wp, (RECT *)lp);
    return 1;
  case TCM_HITTEST: {
    POINT *p = (POINT *)lp; int i; RECT r;
    if (!t) return -1;
    for (i = 0; i < t->n; i++) {
      tab_rect(w, t, i, &r);
      if (p->x >= r.left && p->x < r.right && p->y >= r.top && p->y < r.bottom) return i;
    }
    return -1;
  }
  case TCM_ADJUSTRECT: {
    RECT *r = (RECT *)lp; int abajo = GetWindowLongA(w, -16) & TCS_BOTTOM;
    if (wp) {                   /* de lo de dentro a todo */
      if (abajo) r->bottom += TAB_ALTO; else r->top -= TAB_ALTO;
      r->left -= 2; r->right += 2;
    } else {
      if (abajo) r->bottom -= TAB_ALTO; else r->top += TAB_ALTO;
      r->left += 2; r->right -= 2;
    }
    return 0;
  }
  case TCM_SETIMAGELIST: {
    LISTA *a = t ? t->imagenes : NULL;
    if (t) t->imagenes = (LISTA *)lp;
    return (int)a;
  }
  case TCM_GETIMAGELIST:
    return t ? (int)t->imagenes : 0;
  case TCM_GETROWCOUNT:
    return 1;
  case TCM_SETITEMSIZE:
    return 0;
  case WM_LBUTTONDOWN: {
    POINT p; int i, r;
    p.x = (short)(lp & 0xFFFF); p.y = (short)(lp >> 16);
    i = tab_proc(w, TCM_HITTEST, 0, (u32)&p);
    if (i >= 0 && t && i != t->sel) {
      tab_avisa(w, TCN_SELCHANGING, &r);
      if (r) return 0;
      t->sel = i;
      InvalidateRect(w, NULL, 1);
      tab_avisa(w, TCN_SELCHANGE, &r);
    }
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps; RECT c, r; int i, abajo = GetWindowLongA(w, -16) & TCS_BOTTOM;
    HANDLE dc = BeginPaint(w, &ps);
    GetClientRect(w, &c);
    r = c;
    if (abajo) r.bottom -= TAB_ALTO - 1; else r.top += TAB_ALTO - 1;
    FillRect(dc, &c, GetSysColorBrush(COLOR_BTNFACE));
    DrawEdge(dc, &r, EDGE_RAISED, BF_RECT);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    for (i = 0; t && i < t->n; i++) {
      tab_rect(w, t, i, &r);
      if (i != t->sel) { if (abajo) r.bottom -= 2; else r.top += 2; }
      else FillRect(dc, &r, GetSysColorBrush(COLOR_BTNFACE));
      DrawEdge(dc, &r, EDGE_RAISED, abajo ? BF_LEFT | BF_RIGHT | BF_BOTTOM : BF_LEFT | BF_RIGHT | BF_TOP);
      r.left += 6;
      if (t->imagenes && t->p[i].imagen >= 0) {
        ImageList_Draw(t->imagenes, t->p[i].imagen, dc, r.left, r.top + (r.bottom - r.top - t->imagenes->cy) / 2, 0);
        r.left += t->imagenes->cx + 4;
      }
      if (t->p[i].texto) DrawTextA(dc, t->p[i].texto, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    EndPaint(w, &ps);
    return 0;
  }
  }
  return DefWindowProcA(w, m, wp, lp);
}

/* ---- la barra de progreso ---- */
#define PBM_SETRANGE (WM_USER + 1)
#define PBM_SETPOS (WM_USER + 2)
#define PBM_DELTAPOS (WM_USER + 3)
#define PBM_SETSTEP (WM_USER + 4)
#define PBM_STEPIT (WM_USER + 5)
#define PBM_SETRANGE32 (WM_USER + 6)
#define PBM_GETRANGE (WM_USER + 7)
#define PBM_GETPOS (WM_USER + 8)
typedef struct { int min, max, pos, paso; } PROGRESO;
static int WINAPI pb_proc(HANDLE w, u32 m, u32 wp, u32 lp)
{
  PROGRESO *p = datos(w);
  int antes;
  switch (m) {
  case WM_CREATE:
    p = pide(sizeof *p);
    if (!p) return -1;
    p->max = 100; p->paso = 10;
    SetWindowLongA(w, GWL_USERDATA, (int)p);
    return 0;
  case WM_DESTROY:
    suelta(p);
    SetWindowLongA(w, GWL_USERDATA, 0);
    return 0;
  }
  if (!p) return DefWindowProcA(w, m, wp, lp);
  antes = p->pos;
  switch (m) {
  case PBM_SETRANGE:
    antes = p->min | (p->max << 16);
    p->min = lp & 0xFFFF; p->max = lp >> 16;
    break;
  case PBM_SETRANGE32:
    antes = p->min | (p->max << 16);
    p->min = wp; p->max = lp;
    break;
  case PBM_GETRANGE:
    return wp ? p->min : p->max;
  case PBM_SETPOS: p->pos = wp; break;
  case PBM_DELTAPOS: p->pos += wp; break;
  case PBM_SETSTEP: antes = p->paso; p->paso = wp; break;
  case PBM_STEPIT: p->pos += p->paso; if (p->pos > p->max) p->pos = p->min; break;
  case PBM_GETPOS: return p->pos;
  case WM_PAINT: {
    PAINTSTRUCT ps; RECT c, r;
    HANDLE dc = BeginPaint(w, &ps);
    GetClientRect(w, &c);
    FillRect(dc, &c, GetSysColorBrush(COLOR_BTNFACE));
    DrawEdge(dc, &c, BDR_SUNKENOUTER, BF_RECT);
    r = c; r.left += 2; r.top += 2; r.bottom -= 2;
    if (p->max > p->min) {
      int x = p->pos < p->min ? p->min : p->pos > p->max ? p->max : p->pos;
      r.right = r.left + (c.right - 4) * (x - p->min) / (p->max - p->min);
      FillRect(dc, &r, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    EndPaint(w, &ps);
    return 0;
  }
  default:
    return DefWindowProcA(w, m, wp, lp);
  }
  if (p->pos < p->min) p->pos = p->min;
  if (p->pos > p->max) p->pos = p->max;
  InvalidateRect(w, NULL, 0);
  return antes;
}

/* ---- inicio ---- */
static HANDLE yo;
static int hechas;
static void registra(const char *nombre, WNDPROC f)
{
  WNDCLASSA c;
  int i; char *p = (char *)&c;
  for (i = 0; i < (int)sizeof c; i++) p[i] = 0;
  c.style = 8;                          /* CS_DBLCLKS */
  c.lpfnWndProc = f; c.hInstance = yo;
  c.hbrBackground = (HANDLE)(COLOR_BTNFACE + 1);
  c.lpszClassName = nombre;
  RegisterClassA(&c);
}
static void registra_todas(void)
{
  if (hechas) return;
  hechas = 1;
  registra("msctls_statusbar32", sb_proc);
  registra("SysTabControl32", tab_proc);
  registra("msctls_progress32", pb_proc);
}
void WINAPI InitCommonControls(void) { registra_todas(); }
int WINAPI InitCommonControlsEx(const void *p) { registra_todas(); return 1; }
int WINAPI DllMain(HANDLE inst, u32 por, void *r)
{
  if (por == 1) { yo = inst; registra_todas(); }
  return 1;
}
