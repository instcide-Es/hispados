/* winhola.c - prueba de la fase 3b de Hispa-486 (docs/486/HISPA-486.md): el
 * «Hola, mundo» de ventana de Win32, dentro de Hispa Ventanas. Registra una
 * clase, crea la ventana «Hola desde Win32» (con CW_USEDEFAULT) y, en
 * WM_PAINT, escribe un saludo, cuántos clics lleva (WM_LBUTTONDOWN ->
 * InvalidateRect), lo que mide su zona (WM_SIZE), si ha vencido tres veces su
 * temporizador (SetTimer, WM_TIMER, KillTimer), lo que devolvió un
 * SendMessageA hecho desde WM_CREATE y la última tecla (WM_CHAR, WM_KEYDOWN);
 * dibuja un rectángulo rojo con su borde y una raya (Rectangle, LineTo) y
 * otro azul con un texto centrado (FillRect, DrawTextA). El botón derecho
 * abre un MessageBoxA cuya dueña es la ventana; la «T» cambia el título
 * (SetWindowTextA, GetWindowTextA). Al cerrarla (WM_CLOSE -> DefWindowProcA
 * -> DestroyWindow -> WM_DESTROY) llama a PostQuitMessage, GetMessageA
 * devuelve 0 y acaba con ExitProcess.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
typedef unsigned int u32;
typedef void *HANDLE;
#define WINAPI __attribute__((stdcall))
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;
typedef struct { HANDLE hwnd; u32 message, wParam, lParam, time; POINT pt; } MSG;
typedef int WINAPI (*WNDPROC)(HANDLE, u32, u32, u32);
typedef struct {
  u32 style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HANDLE hInstance, hIcon, hCursor, hbrBackground;
  const char *lpszMenuName, *lpszClassName;
} WNDCLASSA;
typedef struct { HANDLE hdc; int fErase; RECT rcPaint; int fRestore, fIncUpdate; char rgbReserved[32]; } PAINTSTRUCT;
typedef struct { void *lpCreateParams; HANDLE hInstance, hMenu, hwndParent; int cy, cx, y, x; u32 style;
  const char *lpszName, *lpszClass; u32 dwExStyle; } CREATESTRUCTA;

void WINAPI ExitProcess(u32);
void *WINAPI GetModuleHandleA(const char *);
int WINAPI lstrlenA(const char *);
char *WINAPI lstrcpyA(char *, const char *);
char *WINAPI lstrcatA(char *, const char *);
u32 WINAPI RegisterClassA(const WNDCLASSA *);
HANDLE WINAPI CreateWindowExA(u32, const char *, const char *, u32, int, int, int, int, HANDLE, HANDLE, HANDLE, void *);
int WINAPI ShowWindow(HANDLE, int);
int WINAPI UpdateWindow(HANDLE);
int WINAPI DestroyWindow(HANDLE);
int WINAPI DefWindowProcA(HANDLE, u32, u32, u32);
int WINAPI GetMessageA(MSG *, HANDLE, u32, u32);
int WINAPI TranslateMessage(const MSG *);
int WINAPI DispatchMessageA(const MSG *);
void WINAPI PostQuitMessage(int);
int WINAPI PostMessageA(HANDLE, u32, u32, u32);
int WINAPI SendMessageA(HANDLE, u32, u32, u32);
int WINAPI GetClientRect(HANDLE, RECT *);
int WINAPI GetWindowRect(HANDLE, RECT *);
int WINAPI InvalidateRect(HANDLE, const RECT *, int);
int WINAPI SetWindowTextA(HANDLE, const char *);
int WINAPI GetWindowTextA(HANDLE, char *, int);
u32 WINAPI SetTimer(HANDLE, u32, u32, void *);
int WINAPI KillTimer(HANDLE, u32);
int WINAPI MessageBoxA(HANDLE, const char *, const char *, u32);
HANDLE WINAPI LoadCursorA(HANDLE, const char *);
HANDLE WINAPI LoadIconA(HANDLE, const char *);
int WINAPI GetSystemMetrics(int);
HANDLE WINAPI GetDC(HANDLE);
int WINAPI ReleaseDC(HANDLE, HANDLE);
HANDLE WINAPI BeginPaint(HANDLE, PAINTSTRUCT *);
int WINAPI EndPaint(HANDLE, const PAINTSTRUCT *);
int WINAPI FillRect(HANDLE, const RECT *, HANDLE);
int WINAPI DrawTextA(HANDLE, const char *, int, RECT *, u32);
int wsprintfA(char *, const char *, ...);
int WINAPI TextOutA(HANDLE, int, int, const char *, int);
u32 WINAPI SetTextColor(HANDLE, u32);
u32 WINAPI SetBkColor(HANDLE, u32);
int WINAPI SetBkMode(HANDLE, int);
HANDLE WINAPI GetStockObject(int);
HANDLE WINAPI CreateSolidBrush(u32);
HANDLE WINAPI CreatePen(int, int, u32);
HANDLE WINAPI SelectObject(HANDLE, HANDLE);
int WINAPI DeleteObject(HANDLE);
int WINAPI Rectangle(HANDLE, int, int, int, int);
int WINAPI MoveToEx(HANDLE, int, int, POINT *);
int WINAPI LineTo(HANDLE, int, int);
int WINAPI GetTextExtentPoint32A(HANDLE, const char *, int, POINT *);

#define WM_CREATE 1
#define WM_DESTROY 2
#define WM_SIZE 5
#define WM_PAINT 15
#define WM_CLOSE 16
#define WM_KEYDOWN 0x100
#define WM_KEYUP 0x101
#define WM_CHAR 0x102
#define WM_TIMER 0x113
#define WM_LBUTTONDOWN 0x201
#define WM_RBUTTONDOWN 0x204
#define WM_USER 0x400
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define CW_USEDEFAULT ((int)0x80000000)
#define RGB(r, g, b) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16))
#define IDC_ARROW ((const char *)32512)
#define IDI_APPLICATION ((const char *)32512)

static int clics, ancho, alto, vencidos, listo, enviado, creada, soltadas;
static char tecla[16] = "-";
static char linea[120];
static const char clase[] = "HolaWin32";

static void escribe(HANDLE dc, int x, int y, const char *t) { TextOutA(dc, x, y, t, lstrlenA(t)); }

static void pinta(HANDLE v) {
  PAINTSTRUCT ps;
  RECT r;
  POINT m;
  HANDLE dc = BeginPaint(v, &ps), rojo, azul, pluma, antes, pantes;
  escribe(dc, 10, 10, "\xA1Hola, mundo de Win32!");
  wsprintfA(linea, "Clics: %d", clics);
  escribe(dc, 10, 30, linea);
  wsprintfA(linea, "Zona: %dx%d", ancho, alto);
  escribe(dc, 10, 50, linea);
  escribe(dc, 10, 70, listo ? "Temporizador: bien" : "Temporizador: esperando");
  wsprintfA(linea, "Enviado: %d  Tecla: %s", enviado, tecla);
  escribe(dc, 10, 90, linea);
  GetTextExtentPoint32A(dc, "cinco", 5, &m);
  GetClientRect(v, &r);
  wsprintfA(linea, "%s %04X %c%u", creada == 1 && m.x == 40 && m.y == 16 && r.right == ancho && r.bottom == alto
            && r.left == 0 ? "Todo bien." : "ALGO MAL.", 0xBEEF, 'z', 4000000000u);
  SetTextColor(dc, RGB(0, 102, 0));
  escribe(dc, 10, 110, linea);
  /* un rectángulo rojo con el borde negro y una raya blanca */
  rojo = CreateSolidBrush(RGB(255, 0, 0));
  antes = SelectObject(dc, rojo);
  Rectangle(dc, 250, 10, 380, 60);
  pluma = CreatePen(0, 1, RGB(255, 255, 255));
  pantes = SelectObject(dc, pluma);
  MoveToEx(dc, 260, 35, 0);
  LineTo(dc, 370, 35);
  SelectObject(dc, pantes);
  SelectObject(dc, antes);
  DeleteObject(pluma);
  DeleteObject(rojo);
  /* y otro azul, con un texto blanco centrado y sin fondo */
  azul = CreateSolidBrush(RGB(0, 0, 255));
  r.left = 250; r.top = 70; r.right = 380; r.bottom = 110;
  FillRect(dc, &r, azul);
  DeleteObject(azul);
  SetTextColor(dc, RGB(255, 255, 255));
  SetBkMode(dc, 1);
  DrawTextA(dc, "Centro", -1, &r, 1 | 4 | 0x20);
  EndPaint(v, &ps);
}

static int WINAPI proc(HANDLE v, u32 m, u32 w, u32 l) {
  switch (m) {
  case WM_CREATE:
    if (((CREATESTRUCTA *)l)->lpCreateParams == (void *)0x1234 && ((CREATESTRUCTA *)l)->cx == 400) creada++;
    enviado = SendMessageA(v, WM_USER + 1, 5, 7);       /* (anidado: vuelve a entrar aquí) */
    SetTimer(v, 1, 200, 0);
    return 0;
  case WM_USER + 1:
    return w + l;
  case WM_SIZE:
    ancho = l & 0xFFFF; alto = l >> 16;
    return 0;
  case WM_TIMER:
    if (w == 1 && ++vencidos == 3) { KillTimer(v, 1); listo = 1; InvalidateRect(v, 0, 1); }
    return 0;
  case WM_LBUTTONDOWN:
    clics++;
    InvalidateRect(v, 0, 1);
    return 0;
  case WM_RBUTTONDOWN:
    wsprintfA(linea, "Lleva %d clics.\nPunto: %d, %d", clics, l & 0xFFFF, l >> 16);
    MessageBoxA(v, linea, "Hola desde Win32", 0);
    return 0;
  case WM_KEYUP:
    soltadas++;
    return 0;
  case WM_KEYDOWN:
    if (w == 0x70) { lstrcpyA(tecla, "F1"); InvalidateRect(v, 0, 1); }      /* VK_F1 */
    return 0;
  case WM_CHAR:
    if (w == 't') {             /* el título: se lee, se cambia y se vuelve a leer */
      GetWindowTextA(v, linea, sizeof linea);
      lstrcatA(linea, " (t\xEDtulo)");
      SetWindowTextA(v, linea);
    }
    wsprintfA(tecla, "%c (%d)", w, soltadas);
    InvalidateRect(v, 0, 1);
    return 0;
  case WM_PAINT:
    pinta(v);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(7);
    return 0;
  }
  return DefWindowProcA(v, m, w, l);
}

void inicio(void) {
  WNDCLASSA c;
  MSG m;
  HANDLE v, mod = GetModuleHandleA(0);
  c.style = 3; c.lpfnWndProc = proc; c.cbClsExtra = 0; c.cbWndExtra = 0; c.hInstance = mod;
  c.hIcon = LoadIconA(0, IDI_APPLICATION); c.hCursor = LoadCursorA(0, IDC_ARROW);
  c.hbrBackground = GetStockObject(0);          /* WHITE_BRUSH */
  c.lpszMenuName = 0; c.lpszClassName = clase;
  if (!RegisterClassA(&c) || RegisterClassA(&c) || !c.hIcon || !c.hCursor || GetSystemMetrics(0) < 640) {
    MessageBoxA(0, "No se ha podido registrar la clase.", "Hola desde Win32", 0);
    ExitProcess(1);
  }
  v = CreateWindowExA(0, clase, "Hola desde Win32", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 240,
                      0, 0, mod, (void *)0x1234);
  if (!v || CreateWindowExA(0, "NoHay", "x", 0, 0, 0, 10, 10, 0, 0, mod, 0)) {
    MessageBoxA(0, "No se ha podido crear la ventana.", "Hola desde Win32", 0);
    ExitProcess(2);
  }
  ShowWindow(v, 1);
  UpdateWindow(v);
  while (GetMessageA(&m, 0, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageA(&m);
  }
  ExitProcess(m.wParam);
}
