/* winrueda.c - [RUEDA] prueba de la rueda del ratón en los programas de
 * Windows (Hispa-486, ventanas/win32u.inc): una ventana que cuenta los
 * WM_MOUSEWHEEL que le llegan (cuántos, la suma de los delta, las teclas y el
 * punto de la pantalla), una hija suya que los cuenta y los pasa a
 * DefWindowProcA (que se los da a la madre, como en Windows), una lista
 * (LISTBOX) larga que se desplaza con la rueda y lo que dice
 * SystemParametersInfoA(SPI_GETWHEELSCROLLLINES). La prueba
 * (tests/run_rueda_tests.sh) lo lee en capturas.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "win32min.h"

#define WM_MOUSEWHEEL 0x020A
#define SPI_GETWHEELSCROLLLINES 0x0068
int WINAPI SystemParametersInfoA(u32, u32, void *, u32);

static HANDLE inst, madre, hija;
static int n_madre, suma, teclas, px, py, n_hija, lineas = -1;

static void texto(HANDLE dc, int y, const char *t) { TextOutA(dc, 10, y, t, lstrlenA(t)); }

static int WINAPI proc_hija(HANDLE h, u32 m, u32 w, u32 l) {
  if (m == WM_MOUSEWHEEL) {
    n_hija++;
    InvalidateRect(h, NULL, 1);
    return DefWindowProcA(h, m, w, l);      /* a la madre */
  }
  if (m == WM_PAINT) {
    PAINTSTRUCT ps;
    char b[60];
    HANDLE dc = BeginPaint(h, &ps);
    wsprintfA(b, "Hija: %d", n_hija);
    texto(dc, 10, b);
    EndPaint(h, &ps);
    return 0;
  }
  return DefWindowProcA(h, m, w, l);
}

static int WINAPI proc_madre(HANDLE h, u32 m, u32 w, u32 l) {
  switch (m) {
  case WM_CREATE: {
    int i;
    char b[20];
    HANDLE lista;
    hija = CreateWindowExA(0, "RuedaHija", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 260, 10, 200, 80, h, (HANDLE)1, inst, 0);
    lista = CreateWindowExA(0, "LISTBOX", 0, WS_CHILD | WS_VISIBLE | WS_BORDER, 260, 100, 200, 110, h, (HANDLE)2, inst, 0);
    for (i = 1; i <= 30; i++) {
      wsprintfA(b, "Elemento %d", i);
      SendMessageA(lista, LB_ADDSTRING, 0, (u32)b);
    }
    if (!SystemParametersInfoA(SPI_GETWHEELSCROLLLINES, 0, &lineas, 0)) lineas = -2;
    return 0;
  }
  case WM_MOUSEWHEEL:
    n_madre++;
    suma += (short)(w >> 16);
    teclas = w & 0xFFFF;
    px = (short)(l & 0xFFFF);
    py = (short)(l >> 16);
    InvalidateRect(h, NULL, 1);
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps;
    char b[80];
    HANDLE dc = BeginPaint(h, &ps);
    wsprintfA(b, "Madre: %d, suma %d", n_madre, suma);
    texto(dc, 10, b);
    wsprintfA(b, "Punto: %d,%d", px, py);
    texto(dc, 30, b);
    wsprintfA(b, "Teclas: %d", teclas);
    texto(dc, 50, b);
    wsprintfA(b, "L\xEDneas por paso: %d", lineas);
    texto(dc, 70, b);
    EndPaint(h, &ps);
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(h, m, w, l);
}

void inicio(void) {
  WNDCLASSA c = { 0 };
  MSG m;
  inst = GetModuleHandleA(0);
  c.lpfnWndProc = proc_madre;
  c.hInstance = inst;
  c.hbrBackground = (HANDLE)(5 + 1);    /* COLOR_WINDOW + 1 */
  c.lpszClassName = "RuedaMadre";
  RegisterClassA(&c);
  c.lpfnWndProc = proc_hija;
  c.lpszClassName = "RuedaHija";
  RegisterClassA(&c);
  madre = CreateWindowExA(0, "RuedaMadre", "Rueda en Win32", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 480, 260, 0, 0, inst, 0);
  ShowWindow(madre, 1);
  UpdateWindow(madre);
  while (GetMessageA(&m, 0, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageA(&m);
  }
  ExitProcess(0);
}
