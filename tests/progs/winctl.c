/* winctl.c - prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md), parte
 * 2: los controles de serie de Windows como ventanas hijas, sobre los de
 * Hispa Ventanas. Una ventana con un rótulo (STATIC), una caja de texto
 * (EDIT), botones (BUTTON: pulsador, casilla y dos opciones), una lista
 * (LISTBOX), una lista desplegable (COMBOBOX) y una caja de varias líneas.
 * Cada aviso (WM_COMMAND con BN_CLICKED, EN_CHANGE, LBN_SELCHANGE,
 * CBN_SELCHANGE) escribe lo que ha pasado en un rótulo: la prueba
 * (tests/run_win32ctl_tests.sh) lo lee en capturas.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "win32min.h"

enum { ID_NOMBRE = 101, ID_SALUDAR, ID_MAYUS, ID_UNO, ID_DOS, ID_LISTA, ID_COMBO, ID_VARIAS, ID_DESACT,
       ID_SALUDO, ID_ESTADO, ID_NUM, ID_DOBLE, ID_CAMBIOS };

static HANDLE inst;
static int cambios, teclas;

static HANDLE ctl(HANDLE v, const char *clase, const char *texto, u32 estilo, int x, int y, int an, int al, int id) {
  return CreateWindowExA(0, clase, texto, WS_CHILD | WS_VISIBLE | estilo, x, y, an, al, v, (HANDLE)id, inst, 0);
}

static void crear(HANDLE v) {
  static const char *colores[] = { "Rojo", "Verde", "Azul", "Amarillo" };
  static const char *tallas[] = { "Peque\xF1o", "Mediano", "Grande" };
  int i;
  ctl(v, "STATIC", "Nombre:", 0, 10, 12, 64, 20, -1);
  ctl(v, "EDIT", "Hispa", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 80, 10, 160, 24, ID_NOMBRE);
  ctl(v, "BUTTON", "Saludar", WS_TABSTOP | BS_DEFPUSHBUTTON, 250, 9, 90, 26, ID_SALUDAR);
  ctl(v, "button", "Desactivar", WS_TABSTOP, 350, 9, 110, 26, ID_DESACT);
  ctl(v, "BUTTON", "May\xFAsculas", WS_TABSTOP | BS_AUTOCHECKBOX, 10, 44, 120, 20, ID_MAYUS);
  ctl(v, "BUTTON", "Uno", WS_TABSTOP | BS_AUTORADIOBUTTON, 140, 44, 60, 20, ID_UNO);
  ctl(v, "BUTTON", "Dos", BS_AUTORADIOBUTTON, 210, 44, 60, 20, ID_DOS);
  ctl(v, "EDIT", "", WS_TABSTOP | WS_BORDER, 290, 42, 60, 24, ID_NUM);
  ctl(v, "BUTTON", "Doble", WS_TABSTOP, 360, 41, 70, 26, ID_DOBLE);
  ctl(v, "LISTBOX", 0, WS_TABSTOP | WS_BORDER | LBS_NOTIFY, 10, 76, 140, 70, ID_LISTA);
  ctl(v, "COMBOBOX", 0, WS_TABSTOP | CBS_DROPDOWNLIST, 160, 76, 140, 120, ID_COMBO);
  ctl(v, "EDIT", "una\r\ndos", WS_TABSTOP | WS_BORDER | ES_MULTILINE, 310, 76, 160, 70, ID_VARIAS);
  ctl(v, "STATIC", "", 0, 10, 156, 460, 18, ID_SALUDO);
  ctl(v, "STATIC", "Listo.", 0, 10, 178, 460, 18, ID_ESTADO);
  ctl(v, "STATIC", "", 0, 10, 200, 460, 18, ID_CAMBIOS);
  for (i = 0; i < 4; i++) SendDlgItemMessageA(v, ID_LISTA, LB_ADDSTRING, 0, (u32)colores[i]);
  for (i = 0; i < 3; i++) SendMessageA(GetDlgItem(v, ID_COMBO), CB_ADDSTRING, 0, (u32)tallas[i]);
  SendDlgItemMessageA(v, ID_COMBO, CB_SETCURSEL, 1, 0);
  CheckRadioButton(v, ID_UNO, ID_DOS, ID_UNO);
  SetDlgItemInt(v, ID_NUM, 21, 0);
  cambios = 0;                                  /* (los de crear no cuentan) */
  SetFocus(GetDlgItem(v, ID_NOMBRE));
}

static void estado(HANDLE v, const char *t) { SetDlgItemTextA(v, ID_ESTADO, t); }

static void saludar(HANDLE v) {
  char n[64], b[128];
  int i, l = GetDlgItemTextA(v, ID_NOMBRE, n, sizeof n);
  if (IsDlgButtonChecked(v, ID_MAYUS))
    for (i = 0; n[i]; i++) if (n[i] >= 'a' && n[i] <= 'z') n[i] -= 32;
  wsprintfA(b, "Hola, %s (%d letras, opci\xF3n %d)", n, l, IsDlgButtonChecked(v, ID_UNO) ? 1 : IsDlgButtonChecked(v, ID_DOS) ? 2 : 0);
  SetWindowTextA(GetDlgItem(v, ID_SALUDO), b);
}

static int WINAPI proc(HANDLE v, u32 m, u32 w, u32 l) {
  char b[128], c[64];
  int i;
  switch (m) {
  case WM_CREATE:
    crear(v);
    return 0;
  case WM_COMMAND:
    switch (LOWORD(w)) {
    case ID_SALUDAR:
      if (HIWORD(w) == BN_CLICKED && (HANDLE)l == GetDlgItem(v, ID_SALUDAR)) saludar(v);
      break;
    case ID_DESACT: {
      HANDLE s = GetDlgItem(v, ID_SALUDAR);
      int act = IsWindowEnabled(s);
      EnableWindow(s, !act);
      SetWindowTextA((HANDLE)l, act ? "Activar" : "Desactivar");
      wsprintfA(b, "Saludar: %s (bot\xF3n %d)", IsWindowEnabled(s) ? "activo" : "desactivado", GetDlgCtrlID((HANDLE)l));
      estado(v, b);
      break; }
    case ID_MAYUS: case ID_UNO: case ID_DOS:
      wsprintfA(b, "Casilla %d, opciones %d y %d", IsDlgButtonChecked(v, ID_MAYUS), IsDlgButtonChecked(v, ID_UNO), IsDlgButtonChecked(v, ID_DOS));
      estado(v, b);
      break;
    case ID_DOBLE: {
      int bien, n = GetDlgItemInt(v, ID_NUM, &bien, 1);
      if (bien) SetDlgItemInt(v, ID_NUM, n * 2, 1); else estado(v, "Eso no es un n\xFAmero.");
      break; }
    case ID_LISTA:
      i = SendDlgItemMessageA(v, ID_LISTA, LB_GETCURSEL, 0, 0);
      c[0] = 0;
      SendDlgItemMessageA(v, ID_LISTA, LB_GETTEXT, i, (u32)c);
      wsprintfA(b, "Lista: %s (%d de %d)%s", c, i, SendDlgItemMessageA(v, ID_LISTA, LB_GETCOUNT, 0, 0), HIWORD(w) == LBN_DBLCLK ? " doble" : "");
      if (HIWORD(w) == LBN_SELCHANGE || HIWORD(w) == LBN_DBLCLK) estado(v, b);
      break;
    case ID_COMBO:
      if (HIWORD(w) != CBN_SELCHANGE) break;
      i = SendMessageA((HANDLE)l, CB_GETCURSEL, 0, 0);
      SendMessageA((HANDLE)l, CB_GETLBTEXT, i, (u32)c);
      GetWindowTextA((HANDLE)l, b + 100, 20);
      wsprintfA(b, "Combo: %s (%d de %d) %s", c, i, SendMessageA((HANDLE)l, CB_GETCOUNT, 0, 0), b + 100);
      estado(v, b);
      break;
    case ID_NOMBRE: case ID_VARIAS: case ID_NUM:
      if (HIWORD(w) != EN_CHANGE) break;
      cambios++;
      wsprintfA(b, "Cambios: %d; la caja %d: %d letras, %d l\xEDneas", cambios, LOWORD(w), GetWindowTextLengthA((HANDLE)l),
                SendMessageA((HANDLE)l, EM_GETLINECOUNT, 0, 0));
      SetDlgItemTextA(v, ID_CAMBIOS, b);
      break;
    }
    return 0;
  case WM_CHAR:                 /* lo que no quiere el control con el foco */
    if (w == '!') { GetDlgItemTextA(v, ID_VARIAS, c, sizeof c); for (i = 0; c[i]; i++) if (c[i] < ' ') c[i] = c[i] == '\r' ? '<' : '>';
                    wsprintfA(b, "Varias: [%s] foco %d", c, GetDlgCtrlID(GetFocus())); estado(v, b); }
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(v, m, w, l);
}

void inicio(void) {
  WNDCLASSA c = { 0 };
  MSG m;
  HANDLE v;
  inst = GetModuleHandleA(0);
  c.lpfnWndProc = proc;
  c.hInstance = inst;
  c.hbrBackground = (HANDLE)(COLOR_BTNFACE + 1);
  c.lpszClassName = "HispaCtl";
  RegisterClassA(&c);
  v = CreateWindowExA(0, "HispaCtl", "Controles de Win32", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                      40, 40, 486, 252, 0, 0, inst, 0);
  ShowWindow(v, SW_SHOW);
  while (GetMessageA(&m, 0, 0, 0)) {
    if (IsDialogMessageA(v, &m)) continue;
    TranslateMessage(&m);
    DispatchMessageA(&m);
  }
  ExitProcess(m.wParam);
}
