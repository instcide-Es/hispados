/* winmenu.c - prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md),
 * partes 3 y 4: los menús y los recursos. La ventana lleva una barra de
 * menús hecha con CreateMenu, CreatePopupMenu y AppendMenuA (con una orden
 * que se marca, otra en gris que se activa desde otra orden y una orden
 * suelta en la barra); desde ella se cambia por el menú de los recursos
 * (LoadMenuA) y se abren los diálogos de plantilla: uno modal
 * (DialogBoxParamA: caja de texto, casilla, Aceptar y Cancelar) y otro que no
 * lo es (CreateDialogParamA). LoadStringA, LoadIconA y FindResourceA leen lo
 * demás de winmenu.rc. Lo que pasa se escribe en la ventana: la prueba
 * (tests/run_win32menu_tests.sh) lo lee en capturas.
 * Los textos van en la página 1252, la de Windows.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "win32min.h"

HANDLE WINAPI FindResourceA(HANDLE, const char *, const char *);
HANDLE WINAPI LoadResource(HANDLE, HANDLE);
void *WINAPI LockResource(HANDLE);
u32 WINAPI SizeofResource(HANDLE, HANDLE);
HANDLE WINAPI GetSubMenu(HANDLE, int);
int WINAPI GetMenuItemCount(HANDLE);

enum { O_CONTAR = 101, O_MARCA, O_GRIS, O_SALIR, O_ACTIVAR, O_RECURSOS, O_SINMENU, O_ACERCA,
       R_MODAL = 201, R_APARTE, R_TEXTO, R_ANTES };

static HANDLE inst, ventana, barra, aparte;
static int orden, cuenta, marca, sumas, res_dlg = -1;
static char texto[80] = "-", nombre[40] = "Hispa";

static HANDLE hacer_barra(void) {
  HANDLE b = CreateMenu(), a = CreatePopupMenu(), v = CreatePopupMenu();
  AppendMenuA(a, MF_STRING, O_CONTAR, "&Contar\tCtrl+C");
  AppendMenuA(a, MF_SEPARATOR, 0, 0);
  AppendMenuA(a, MF_STRING, O_MARCA, "&Marcada");
  AppendMenuA(a, MF_STRING | MF_GRAYED, O_GRIS, "&Gris");
  AppendMenuA(a, MF_STRING, O_SALIR, "&Salir");
  AppendMenuA(v, MF_STRING, O_ACTIVAR, "&Activar la gris");
  AppendMenuA(v, MF_STRING, O_RECURSOS, "Men\xFA de los &recursos");
  AppendMenuA(v, MF_STRING, O_SINMENU, "&Sin men\xFA");
  AppendMenuA(b, MF_POPUP, (u32)a, "&Archivo");
  AppendMenuA(b, MF_POPUP, (u32)v, "&Ver");
  AppendMenuA(b, MF_STRING, O_ACERCA, "A&cerca");
  return b;
}

/* el diálogo modal: una caja de texto y una casilla */
static int WINAPI dlg_datos(HANDLE d, u32 m, u32 w, u32 l) {
  switch (m) {
  case WM_INITDIALOG:
    SetDlgItemTextA(d, 301, (const char *)l);
    CheckDlgButton(d, 302, 1);
    return 1;
  case WM_COMMAND:
    if (LOWORD(w) == IDOK) { GetDlgItemTextA(d, 301, nombre, sizeof nombre); EndDialog(d, 1 + 10 * IsDlgButtonChecked(d, 302)); return 1; }
    if (LOWORD(w) == IDCANCEL) { EndDialog(d, 0); return 1; }
    break;
  }
  return 0;
}
/* el «Acerca de»: su texto sale de los recursos */
static int WINAPI dlg_acerca(HANDLE d, u32 m, u32 w, u32 l) {
  char b[80];
  if (m == WM_INITDIALOG) { LoadStringA(inst, 2, b, sizeof b); SetDlgItemTextA(d, 300, b); return 1; }
  if (m == WM_COMMAND && (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL)) { EndDialog(d, LOWORD(w)); return 1; }
  return 0;
}
/* el que no es modal: suma en la ventana principal */
static int WINAPI dlg_aparte(HANDLE d, u32 m, u32 w, u32 l) {
  if (m == WM_INITDIALOG) return 1;
  if (m == WM_COMMAND && LOWORD(w) == 310) { sumas += l ? 1 : 100; InvalidateRect(ventana, 0, 1); return 1; }
  if (m == WM_COMMAND && LOWORD(w) == IDCANCEL) { DestroyWindow(d); aparte = 0; return 1; }
  return 0;
}

static int WINAPI proc(HANDLE v, u32 m, u32 w, u32 l) {
  char b[128];
  switch (m) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HANDLE dc = BeginPaint(v, &ps), r = FindResourceA(inst, MAKEINTRESOURCEA(1), MAKEINTRESOURCEA(4));
    HANDLE a = GetSubMenu(GetMenu(v), 0);
    u32 est = GetMenuState(GetMenu(v), O_MARCA, MF_BYCOMMAND), gris = GetMenuState(GetMenu(v), O_GRIS, MF_BYCOMMAND);
    RECT z;
    GetClientRect(v, &z);
    wsprintfA(b, "Orden: %d  Cuenta: %d  Sumas: %d   ", orden, cuenta, sumas);
    TextOutA(dc, 10, 8, b, lstrlenA(b));
    wsprintfA(b, "Marca: %d (estado %d)  Gris: %d  Archivo: %d   ", marca, est, gris, a ? GetMenuItemCount(a) : -1);
    TextOutA(dc, 10, 26, b, lstrlenA(b));
    wsprintfA(b, "Texto: %s   ", texto);
    TextOutA(dc, 10, 44, b, lstrlenA(b));
    wsprintfA(b, "Di\xE1logo: %d \xAB%s\xBB   ", res_dlg, nombre);
    TextOutA(dc, 10, 62, b, lstrlenA(b));
    wsprintfA(b, "Icono: %d %d  Men\xFA 1: %d octetos, %d  Zona: %d   ", LoadIconA(inst, MAKEINTRESOURCEA(1)) != 0,
              LoadIconA(inst, MAKEINTRESOURCEA(99)) != 0, r ? SizeofResource(inst, r) : 0,
              r ? *(unsigned short *)LockResource(LoadResource(inst, r)) : -1, z.bottom);
    TextOutA(dc, 10, 80, b, lstrlenA(b));
    EndPaint(v, &ps);
    return 0; }
  case WM_COMMAND:
    orden = LOWORD(w);
    switch (orden) {
    case O_CONTAR: cuenta++; break;
    case O_MARCA: marca = !marca; CheckMenuItem(GetMenu(v), O_MARCA, MF_BYCOMMAND | (marca ? MF_CHECKED : MF_UNCHECKED)); break;
    case O_GRIS: cuenta += 1000; break;
    case O_ACTIVAR: EnableMenuItem(barra, O_GRIS, MF_BYCOMMAND | MF_ENABLED); break;
    case O_SALIR: DestroyWindow(v); return 0;
    case O_RECURSOS: SetMenu(v, LoadMenuA(inst, "DEMAS")); break;
    case O_SINMENU: SetMenu(v, 0); break;
    case O_ACERCA: res_dlg = 100 + DialogBoxParamA(inst, MAKEINTRESOURCEA(1), v, dlg_acerca, 0); break;
    case R_MODAL: res_dlg = DialogBoxParamA(inst, MAKEINTRESOURCEA(2), v, dlg_datos, (u32)nombre); break;
    case R_APARTE: if (!aparte) aparte = CreateDialogParamA(inst, "APARTE", v, dlg_aparte, 0); break;
    case R_TEXTO: LoadStringA(inst, cuenta & 1 ? 17 : 1, texto, sizeof texto); cuenta++; break;
    case R_ANTES: SetMenu(v, barra); break;
    }
    InvalidateRect(v, 0, 1);
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
  inst = GetModuleHandleA(0);
  c.lpfnWndProc = proc;
  c.hInstance = inst;
  c.hbrBackground = (HANDLE)(COLOR_WINDOW + 1);
  c.lpszClassName = "HispaMenu";
  RegisterClassA(&c);
  barra = hacer_barra();
  ventana = CreateWindowExA(0, "HispaMenu", "Men\xFAs y recursos", WS_OVERLAPPEDWINDOW, 40, 40, 500, 170, 0, barra, inst, 0);
  ShowWindow(ventana, SW_SHOW);
  while (GetMessageA(&m, 0, 0, 0)) { TranslateMessage(&m); DispatchMessageA(&m); }
  ExitProcess(m.wParam);
}
