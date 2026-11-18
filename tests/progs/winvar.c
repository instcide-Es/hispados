/* winvar.c - prueba de la fase 3d de Hispa-486 (docs/486/HISPA-486.md): lo de
 * USER32 y COMDLG32 que usan los programas de Windows 95/98 además de pintar.
 * Una ventana con barra de desplazamiento (WS_VSCROLL, SetScrollInfo,
 * WM_VSCROLL) que enseña 50 líneas de 10 en 10; el icono de los recursos
 * (en su clase: sale en su botón de la barra de tareas) dibujado con DrawIcon y
 * DrawIconEx, y el de serie; el cursor de los recursos (el de su clase); un
 * mapa de los recursos (LoadBitmapA); un caret detrás de lo que se escribe;
 * los aceleradores de los recursos (Ctrl+O, F5, Mayús+F5, Ctrl+L, Ctrl+C,
 * Ctrl+V): abrir (GetOpenFileNameA), contar, un menú emergente
 * (TrackPopupMenu con TPM_RETURNCMD), copiar y pegar (el portapapeles); y
 * del menú, elegir un color (ChooseColorA) y una letra (ChooseFontA).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui --res (ver el Makefile). */
#include "win32min.h"
#include "win32gdi.h"
void *memset(void *, int, u32);

typedef struct { u32 cbSize, fMask; int nMin, nMax; u32 nPage; int nPos, nTrackPos; } SCROLLINFO;
typedef struct { u32 lStructSize; HANDLE hwndOwner, hInstance; const char *lpstrFilter; char *lpstrCustomFilter;
  u32 nMaxCustFilter, nFilterIndex; char *lpstrFile; u32 nMaxFile; char *lpstrFileTitle; u32 nMaxFileTitle;
  const char *lpstrInitialDir, *lpstrTitle; u32 Flags; u16 nFileOffset, nFileExtension; const char *lpstrDefExt;
  u32 lCustData; void *lpfnHook; const char *lpTemplateName; } OPENFILENAMEA;
typedef struct { u32 lStructSize; HANDLE hwndOwner, hInstance; u32 rgbResult; u32 *lpCustColors; u32 Flags;
  u32 lCustData; void *lpfnHook; const char *lpTemplateName; } CHOOSECOLORA;
typedef struct { u32 lStructSize; HANDLE hwndOwner, hDC; LOGFONTA *lpLogFont; int iPointSize; u32 Flags, rgbColors;
  u32 lCustData; void *lpfnHook; const char *lpTemplateName; HANDLE hInstance; char *lpszStyle; u16 nFontType, pad;
  int nSizeMin, nSizeMax; } CHOOSEFONTA;

HANDLE WINAPI LoadAcceleratorsA(HANDLE, const char *);
int WINAPI TranslateAcceleratorA(HANDLE, HANDLE, MSG *);
HANDLE WINAPI LoadBitmapA(HANDLE, const char *);
int WINAPI DrawIcon(HANDLE, int, int, HANDLE);
int WINAPI DrawIconEx(HANDLE, int, int, HANDLE, int, int, u32, HANDLE, u32);
int WINAPI SetScrollInfo(HANDLE, int, const SCROLLINFO *, int);
int WINAPI GetScrollInfo(HANDLE, int, SCROLLINFO *);
int WINAPI CreateCaret(HANDLE, HANDLE, int, int);
int WINAPI ShowCaret(HANDLE);
int WINAPI HideCaret(HANDLE);
int WINAPI SetCaretPos(int, int);
int WINAPI DestroyCaret(void);
short WINAPI GetKeyState(int);
int WINAPI OpenClipboard(HANDLE);
int WINAPI CloseClipboard(void);
int WINAPI EmptyClipboard(void);
HANDLE WINAPI SetClipboardData(u32, HANDLE);
HANDLE WINAPI GetClipboardData(u32);
HANDLE WINAPI GlobalAlloc(u32, u32);
void *WINAPI GlobalLock(HANDLE);
int WINAPI GlobalUnlock(HANDLE);
int WINAPI TrackPopupMenu(HANDLE, u32, int, int, int, HANDLE, const RECT *);
HANDLE WINAPI GetSubMenu(HANDLE, int);
int WINAPI ClientToScreen(HANDLE, POINT *);
int WINAPI GetOpenFileNameA(OPENFILENAMEA *);
int WINAPI GetSaveFileNameA(OPENFILENAMEA *);
int WINAPI ChooseColorA(CHOOSECOLORA *);
int WINAPI ChooseFontA(CHOOSEFONTA *);

#define SB_VERT 1
#define SIF_RANGE 1
#define SIF_PAGE 2
#define SIF_POS 4
#define SIF_ALL 0x17
#define WM_VSCROLL 0x0115
#define WM_MOUSEMOVE 0x0200

static HANDLE inst, icono, mapa, letra;
static int arriba, f5, mf5, menu_el, nlet, otra;
static char texto[64], pegado[64], archivo[128] = "(ninguno)";
static u32 color = RGB(255, 255, 255);
static int puntos;

static void barra(HANDLE w)
{
  SCROLLINFO si;
  si.cbSize = sizeof si; si.fMask = SIF_ALL; si.nMin = 0; si.nMax = 49; si.nPage = 10; si.nPos = arriba;
  SetScrollInfo(w, SB_VERT, &si, 1);
}

static void caret(HANDLE w)
{
  SetCaretPos(8 + (7 + nlet) * 8, 200);
}

static void pinta(HANDLE w)
{
  PAINTSTRUCT ps; char t[160]; int i; HANDLE dc = BeginPaint(w, &ps), m, v, b, vf;
  for (i = 0; i < 10; i++) {
    wsprintfA(t, "L\xednea %d", arriba + i + 1);
    TextOutA(dc, 8, 4 + i * 16, t, lstrlenA(t));
  }
  DrawIcon(dc, 300, 8, icono);
  DrawIcon(dc, 340, 8, LoadIconA(NULL, (const char *)32512));
  DrawIconEx(dc, 380, 8, icono, 64, 64, 0, NULL, 3);
  m = CreateCompatibleDC(dc); v = SelectObject(m, mapa);
  BitBlt(dc, 300, 80, 32, 16, m, 0, 0, SRCCOPY);
  SelectObject(m, v); DeleteDC(m);
  b = CreateSolidBrush(color);
  { RECT r = { 300, 120, 340, 150 }; FillRect(dc, &r, b); FrameRect(dc, &r, GetStockObject(BLACK_BRUSH)); }
  DeleteObject(b);
  wsprintfA(t, "Texto: %s", texto);
  TextOutA(dc, 8, 200, t, lstrlenA(t));
  wsprintfA(t, "F5: %d  May\xfas+F5: %d  Men\xfa: %d  Pegado: %s", f5, mf5, menu_el, pegado);
  TextOutA(dc, 8, 224, t, lstrlenA(t));
  wsprintfA(t, "Archivo: %s  (%d)", archivo, otra);
  TextOutA(dc, 8, 248, t, lstrlenA(t));
  if (letra) {
    vf = SelectObject(dc, letra);
    wsprintfA(t, "Letra de %d puntos", puntos);
    TextOutA(dc, 8, 272, t, lstrlenA(t));
    SelectObject(dc, vf);
  }
  EndPaint(w, &ps);
}

static void abrir(HANDLE w, int guardar)
{
  OPENFILENAMEA o; char f[128] = "";
  memset(&o, 0, sizeof o);
  o.lStructSize = sizeof o; o.hwndOwner = w; o.lpstrFile = f; o.nMaxFile = sizeof f;
  o.lpstrFilter = "Programas (*.exe)\0*.EXE\0Todos\0*.*\0"; o.nFilterIndex = 1;
  if (guardar ? GetSaveFileNameA(&o) : GetOpenFileNameA(&o)) {
    lstrcpyA(archivo, f);
    lstrcatA(archivo, guardar ? " (guardar)" : "");
  } else lstrcpyA(archivo, "(cancelado)");
  InvalidateRect(w, NULL, 1);
}

static int WINAPI proc(HANDLE w, u32 m, u32 wp, u32 lp)
{
  switch (m) {
  case WM_CREATE:
    barra(w);
    CreateCaret(w, NULL, 2, 16); caret(w); ShowCaret(w);
    return 0;
  case WM_PAINT:
    pinta(w);
    return 0;
  case WM_VSCROLL: {
    SCROLLINFO si; int n = arriba;
    si.cbSize = sizeof si; si.fMask = SIF_ALL; GetScrollInfo(w, SB_VERT, &si);
    switch (LOWORD(wp)) {
    case 0: n--; break;                       /* SB_LINEUP */
    case 1: n++; break;                       /* SB_LINEDOWN */
    case 2: n -= 10; break;                   /* SB_PAGEUP */
    case 3: n += 10; break;                   /* SB_PAGEDOWN */
    case 4: case 5: n = HIWORD(wp); break;    /* SB_THUMBPOSITION, SB_THUMBTRACK */
    }
    if (n < 0) n = 0;
    if (n > 40) n = 40;
    if (n != arriba) { arriba = n; barra(w); InvalidateRect(w, NULL, 1); }
    return 0;
  }
  case WM_CHAR:
    if (wp >= 32 && nlet < 40) {
      texto[nlet++] = (char)wp; texto[nlet] = 0;
      HideCaret(w); caret(w); ShowCaret(w);
      InvalidateRect(w, NULL, 1);
    }
    return 0;
  case WM_COMMAND:
    switch (LOWORD(wp)) {
    case 101: abrir(w, 0); break;
    case 102: abrir(w, 1); break;
    case 103: f5++; break;
    case 104: mf5++; break;
    case 105: {
      POINT p = { 200, 100 };
      ClientToScreen(w, &p);
      menu_el = TrackPopupMenu(GetSubMenu(LoadMenuA(inst, (const char *)2), 0) ? GetSubMenu(LoadMenuA(inst, (const char *)2), 0)
                               : LoadMenuA(inst, (const char *)2), 0x100, p.x, p.y, 0, w, NULL);
      break;
    }
    case 106: {
      HANDLE h = GlobalAlloc(2, 64); char *p = GlobalLock(h);
      lstrcpyA(p, texto[0] ? texto : "nada"); GlobalUnlock(h);
      if (OpenClipboard(w)) { EmptyClipboard(); SetClipboardData(1, h); CloseClipboard(); }
      break;
    }
    case 107:
      if (OpenClipboard(w)) {
        HANDLE h = GetClipboardData(1);
        if (h) { char *p = GlobalLock(h); lstrcpyA(pegado, p); GlobalUnlock(h); }
        CloseClipboard();
      }
      break;
    case 109: DestroyWindow(w); return 0;
    default: otra = LOWORD(wp); break;
    case 201: {
      CHOOSECOLORA c; u32 cust[16];
      memset(&c, 0, sizeof c); memset(cust, 0, sizeof cust);
      c.lStructSize = sizeof c; c.hwndOwner = w; c.rgbResult = color; c.lpCustColors = cust; c.Flags = 1;
      if (ChooseColorA(&c)) color = c.rgbResult;
      break;
    }
    case 202: {
      CHOOSEFONTA c; LOGFONTA lf;
      memset(&c, 0, sizeof c); memset(&lf, 0, sizeof lf);
      c.lStructSize = sizeof c; c.hwndOwner = w; c.lpLogFont = &lf; c.Flags = 1;
      if (ChooseFontA(&c)) {
        if (letra) DeleteObject(letra);
        letra = CreateFontIndirectA(&lf);
        puntos = c.iPointSize / 10;
      }
      break;
    }
    }
    InvalidateRect(w, NULL, 1);
    return 0;
  case WM_DESTROY:
    DestroyCaret();
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(w, m, wp, lp);
}

void inicio(void)
{
  WNDCLASSA c; MSG msg; HANDLE w, acel;
  memset(&c, 0, sizeof c);
  inst = GetModuleHandleA(NULL);
  icono = LoadIconA(inst, (const char *)1);
  mapa = LoadBitmapA(inst, (const char *)3);
  c.lpfnWndProc = proc; c.hInstance = inst; c.hIcon = icono;
  c.hCursor = LoadCursorA(inst, (const char *)2);
  c.hbrBackground = GetStockObject(WHITE_BRUSH);
  c.lpszMenuName = (const char *)1;
  c.lpszClassName = "PruebaVarios";
  RegisterClassA(&c);
  w = CreateWindowExA(0, "PruebaVarios", "Varios de USER32", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_VISIBLE,
                      20, 40, 600, 400, NULL, NULL, inst, NULL);
  acel = LoadAcceleratorsA(inst, (const char *)1);
  while (GetMessageA(&msg, NULL, 0, 0) > 0)
    if (!TranslateAcceleratorA(w, acel, &msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
  ExitProcess(0);
}
