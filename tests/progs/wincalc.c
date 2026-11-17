/* wincalc.c - la prueba final de la fase 3c de Hispa-486
 * (docs/486/HISPA-486.md): una calculadora sencilla de Windows, en C y con
 * MSVCRT.DLL, que se maneja con el ratón y con el teclado dentro de Hispa
 * Ventanas. Tiene de todo lo de la fase: el menú sale de los recursos (el de
 * la clase: lpszMenuName), los botones y la caja de texto son controles de
 * serie (BUTTON y EDIT), «Dos decimales» se marca con CheckMenuItem, «Acerca
 * de» es un diálogo de plantilla y los textos se leen con LoadStringA; las
 * cuentas se hacen con la coma flotante de MSVCRT (strtod, sprintf, sqrt).
 *  - Con el ratón: los botones; también se puede escribir en la caja.
 *  - Con el teclado: las cifras y los signos (suben a la ventana desde el
 *    botón que tiene el foco), Intro (el foco está en «=») y Esc (borra).
 * La prueba (tests/run_win32calc_tests.sh) mira los resultados en capturas.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "win32min.h"

int sprintf(char *, const char *, ...);
double strtod(const char *, char **);
double sqrt(double);
u32 strlen(const char *);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
void *memset(void *, int, u32);

enum { ID_CAJA = 100, ID_IGUAL = 200, ID_BORRAR, ID_RAIZ, ID_DECIMALES = 300, ID_ACERCA, ID_SALIR };

static HANDLE inst, ventana, caja;
static double acumulado;
static int pendiente, nuevo = 1, decimales, cuentas, propio;

/* a la caja (lo que se pone desde aquí no es «escribir en la caja») */
static void pon(const char *t) { propio = 1; SetWindowTextA(caja, t); propio = 0; }
static void muestra(double v) {
  char b[64];
  sprintf(b, decimales ? "%.2f" : "%.10g", v);
  pon(b);
}
static double valor(void) {
  char b[64];
  GetWindowTextA(caja, b, sizeof b);
  return strtod(b, 0);
}
/* una cifra o la coma: a la caja (si acaba de salir un resultado, de nuevas) */
static void cifra(int c) {
  char b[64];
  int n;
  if (nuevo) b[0] = 0; else GetWindowTextA(caja, b, sizeof b - 2);
  n = strlen(b);
  if (c == '.' && n == 0) b[n++] = '0';
  b[n] = c; b[n + 1] = 0;
  pon(b);
  nuevo = 0;
}
/* una operación: se hace la que estaba pendiente y esta queda para luego */
static void opera(int op) {
  double v = valor();
  char t[80];
  if (pendiente && !nuevo) {
    if (pendiente == '/' && v == 0) {
      LoadStringA(inst, 2, t, sizeof t);
      MessageBoxA(ventana, t, 0, MB_OK);
      pon("Error");
      pendiente = 0; nuevo = 1;
      return;
    }
    acumulado = pendiente == '+' ? acumulado + v : pendiente == '-' ? acumulado - v
              : pendiente == '*' ? acumulado * v : acumulado / v;
    cuentas++;
  } else if (!pendiente || !nuevo)
    acumulado = v;
  muestra(acumulado);
  pendiente = op == '=' ? 0 : op;
  nuevo = 1;
}
static void tecla(int c) {
  if ((c >= '0' && c <= '9') || c == '.') cifra(c);
  else if (c == ',') cifra('.');
  else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '=') opera(c);
}
static void borra(void) { acumulado = 0; pendiente = 0; nuevo = 1; pon("0"); }

static int WINAPI acerca(HANDLE d, u32 m, u32 w, u32 l) {
  char b[120], c[80];
  if (m == WM_INITDIALOG) {
    LoadStringA(inst, 1, c, sizeof c);
    sprintf(b, c, cuentas, (double)l / 2);     /* (con su coma flotante) */
    SetDlgItemTextA(d, 10, b);
    return 1;
  }
  if (m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) { EndDialog(d, 1); return 1; }
  return 0;
}

static int WINAPI proc(HANDLE v, u32 m, u32 w, u32 l) {
  static const char *teclas = "789/456*123-0.=+";
  int i;
  switch (m) {
  case WM_CREATE:
    caja = CreateWindowExA(0, "EDIT", "0", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP, 10, 8, 180, 24, v, (HANDLE)ID_CAJA, inst, 0);
    CreateWindowExA(0, "BUTTON", "C", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 196, 8, 36, 24, v, (HANDLE)ID_BORRAR, inst, 0);
    for (i = 0; i < 16; i++) {
      char t[2] = { teclas[i], 0 };
      CreateWindowExA(0, "BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (teclas[i] == '=' ? BS_DEFPUSHBUTTON : 0),
                      10 + 57 * (i % 4), 40 + 34 * (i / 4), 51, 28, v, (HANDLE)(teclas[i] == '=' ? ID_IGUAL : teclas[i]), inst, 0);
    }
    SetFocus(GetDlgItem(v, ID_IGUAL));
    return 0;
  case WM_COMMAND:
    if (LOWORD(w) == ID_CAJA) {                           /* se ha escrito en la caja: es un número nuevo */
      if (HIWORD(w) == EN_CHANGE && !propio) nuevo = 0;
      return 0;
    }
    switch (LOWORD(w)) {
    case ID_IGUAL: opera('='); break;
    case ID_BORRAR: borra(); break;
    case ID_RAIZ: acumulado = sqrt(valor()); muestra(acumulado); nuevo = 1; break;
    case ID_DECIMALES:
      decimales = !decimales;
      CheckMenuItem(GetMenu(v), ID_DECIMALES, MF_BYCOMMAND | (decimales ? MF_CHECKED : MF_UNCHECKED));
      muestra(valor());
      break;
    case ID_ACERCA: DialogBoxParamA(inst, "ACERCA", v, acerca, 7); break;
    case ID_SALIR: DestroyWindow(v); break;
    default: if (l) tecla(LOWORD(w));                      /* un botón: su número es su letra */
    }
    if (l && LOWORD(w) != ID_SALIR) SetFocus(GetDlgItem(v, ID_IGUAL));
    return 0;
  case WM_CHAR:                 /* lo que no quiere el botón con el foco */
    tecla(w);
    return 0;
  case WM_KEYDOWN:
    if (w == 0x1B) borra();     /* Esc */
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(v, m, w, l);
}

void inicio(void) {
  WNDCLASSA c;
  MSG m;
  char t[40];
  memset(&c, 0, sizeof c);
  inst = GetModuleHandleA(0);
  c.lpfnWndProc = proc;
  c.hInstance = inst;
  c.hIcon = LoadIconA(inst, MAKEINTRESOURCEA(1));
  c.hbrBackground = (HANDLE)(COLOR_BTNFACE + 1);
  c.lpszMenuName = "CALC";
  c.lpszClassName = "HispaCalc";
  RegisterClassA(&c);
  LoadStringA(inst, 3, t, sizeof t);
  ventana = CreateWindowExA(0, "HispaCalc", t, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            60, 50, 246, 226, 0, 0, inst, 0);
  ShowWindow(ventana, SW_SHOW);
  while (GetMessageA(&m, 0, 0, 0)) { TranslateMessage(&m); DispatchMessageA(&m); }
  ExitProcess(m.wParam);
}
