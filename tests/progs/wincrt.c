/* wincrt.c - prueba de la fase 3c de Hispa-486 (docs/486/HISPA-486.md), parte
 * 1: un programa de ventanas hecho en C con MSVCRT.DLL dentro de Hispa
 * Ventanas. KERNEL32 es el de HW32 (el montón, VirtualAlloc, los archivos y
 * las búsquedas, la hora, el entorno, TLS, GetProcAddress) y las DLL se
 * cargan: MSVCRT.DLL (de la carpeta de HW32) y DLLPRUE.DLL (de la del
 * programa). Cada cosa escribe su línea en la ventana: la prueba
 * (tests/run_win32crt_tests.sh) las lee en una captura. Un clic deja un
 * archivo abierto adrede (al cerrar, HW32 lo cierra); la «s» hace Sleep, y
 * las demás tareas siguen. Con -DSINDLL importa de una DLL que no existe: no
 * se carga y se dice cuál falta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "win32min.h"

/* MSVCRT */
int sprintf(char *, const char *, ...);
void *malloc(u32);
void *realloc(void *, u32);
void free(void *);
void *memset(void *, int, u32);
u32 strlen(const char *);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
char *strcat(char *, const char *);
double strtod(const char *, char **);
int atoi(const char *);
void qsort(void *, u32, u32, int (*)(const void *, const void *));
typedef struct FILE FILE;
FILE *fopen(const char *, const char *);
int fprintf(FILE *, const char *, ...);
char *fgets(char *, int, FILE *);
int fclose(FILE *);
int remove(const char *);
int printf(const char *, ...);
u32 time(u32 *);
char *getenv(const char *);
void exit(int);
/* DLLPRUE.DLL */
int WINAPI Doble(int);
extern int *__imp_contador;
#ifdef SINDLL
int WINAPI NoEsta(int);
#endif

#define NL 12
static char linea[NL][64];
static int nl, clics;
static HANDLE abierto;

static void pon(const char *s) { if (nl < NL) strcpy(linea[nl++], s); }
static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static void pruebas(void) {
  char b[128], c[64];
  u32 n, e;
  /* el montón y VirtualAlloc */
  HANDLE h = GetProcessHeap();
  char *p = HeapAlloc(h, 8, 100), *q;
  int bien = p && p[0] == 0 && p[99] == 0;
  if (p) { strcpy(p, "monton"); p = HeapReAlloc(h, 0, p, 70000); bien = bien && p && !strcmp(p, "monton") && HeapSize(h, 0, p) == 70000; }
  q = VirtualAlloc(0, 200000, 0x3000, 4);
  bien = bien && q && q[199999] == 0;
  if (q) { q[199999] = 7; bien = bien && VirtualFree(q, 0, 0x8000); }
  bien = bien && HeapFree(h, 0, p);
  pon(bien ? "Memoria: bien" : "Memoria: MAL");
  /* archivos y búsquedas */
  h = CreateFileA("A:\\WINCRT.TMP", GENERIC_READ | GENERIC_WRITE, 0, 0, 2, 0x80, 0);
  bien = h != (HANDLE)-1 && WriteFile(h, "Hispa-486 escribe", 17, &n, 0) && n == 17;
  bien = bien && SetFilePointer(h, 6, 0, 0) == 6 && ReadFile(h, b, 20, &n, 0) && n == 11 && GetFileSize(h, 0) == 17;
  b[n] = 0;
  CloseHandle(h);
  { WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA("A:\\WINCRT.*", &fd);
    bien = bien && f != (HANDLE)-1 && !strcmp(fd.cFileName, "WINCRT.TMP") && fd.nFileSizeLow == 17 && !FindNextFileA(f, &fd);
    FindClose(f); }
  bien = bien && DeleteFileA("A:\\WINCRT.TMP") && CreateFileA("A:\\WINCRT.TMP", GENERIC_READ, 0, 0, 3, 0, 0) == (HANDLE)-1 && GetLastError() == 2;
  sprintf(c, "Archivos: %s (%s)", bien ? "bien" : "MAL", b);
  pon(c);
  /* la hora, el entorno y TLS */
  { SYSTEMTIME st; u32 t0 = GetTickCount(), t;
    GetLocalTime(&st); t = TlsAlloc();
    bien = st.wYear >= 2024 && st.wMonth >= 1 && st.wMonth <= 12 && TlsSetValue(t, (void *)0x486) && TlsGetValue(t) == (void *)0x486 && TlsFree(t);
    n = GetEnvironmentVariableA("COMSPEC", b, 100);
    bien = bien && n > 4 && n == strlen(b) && GetTickCount() >= t0 && time(0) > 1700000000u;
    sprintf(c, "Hora, entorno y TLS: %s", bien ? "bien" : "MAL");
    pon(c); }
  /* módulos y GetProcAddress */
  { HANDLE k = GetModuleHandleA("KERNEL32"), u = GetModuleHandleA("user32.dll"), m = LoadLibraryA("MSVCRT.DLL");
    u32 WINAPI (*tc)(void) = GetProcAddress(k, "GetTickCount");
    int WINAPI (*ll)(const char *) = GetProcAddress(k, "lstrlenA");
    u32 (*sl)(const char *) = GetProcAddress(m, "strlen");
    int WINAPI (*gsm)(int) = GetProcAddress(u, "GetSystemMetrics");
    bien = k && u && m && tc && ll && sl && gsm && ll("cuatro") == 6 && sl("cinco") == 5 && gsm(0) >= 640 && tc() > 0;
    bien = bien && !GetProcAddress(u, "NoHayTalFuncion") && !GetProcAddress(k, "NiEsta") && !GetModuleHandleA("NOESTA.DLL");
    bien = bien && GetProcAddress(u, "GetSystemMetrics") == (void *)gsm && GetModuleHandleA(0) == (HANDLE)0x400000;
    sprintf(c, "GetProcAddress: %s", bien ? "bien" : "MAL");
    pon(c); }
  /* MSVCRT: sprintf con coma, strtod, malloc, qsort, archivos con búfer */
  { int v[6] = { 5, 3, 9, 1, 4, 2 }; FILE *f; char *m = malloc(40);
    qsort(v, 6, sizeof(int), cmp_int);
    sprintf(m, "%d%d%d%d%d%d %5.2f %s", v[0], v[1], v[2], v[3], v[4], v[5], strtod("3.14159", 0) * 2, "x");
    sprintf(c, "MSVCRT: %s", m);
    pon(c);
    free(m);
    f = fopen("A:\\WINCRT.TXT", "w");
    bien = f != 0;
    if (f) { fprintf(f, "linea %d\notra\n", 1); fclose(f); f = fopen("A:\\WINCRT.TXT", "r"); }
    bien = bien && f && fgets(b, 100, f) && !strcmp(b, "linea 1\n") && fgets(b, 100, f) && !strcmp(b, "otra\n") && !fgets(b, 100, f);
    if (f) fclose(f);
    bien = bien && !remove("A:\\WINCRT.TXT") && printf("a la consola, que aqui no hay\n") > 0;
    sprintf(c, "MSVCRT, archivos: %s", bien ? "bien" : "MAL");
    pon(c); }
  /* la DLL de al lado del programa: su función, su dato y su DllMain */
  sprintf(c, "DLLPRUE: Doble(20) = %d, contador = %d", Doble(20), *__imp_contador);
  pon(c);
  GetModuleFileNameA(0, b, 100);
  sprintf(c, "%s|%s", b, GetCommandLineA());
  pon(c);
}

static int WINAPI proc(HANDLE v, u32 m, u32 w, u32 l) {
  switch (m) {
  case WM_CREATE:
    pruebas();
    return 0;
  case WM_PAINT: {
    PAINTSTRUCT ps; int i; char b[48];
    HANDLE dc = BeginPaint(v, &ps);
    for (i = 0; i < nl; i++) TextOutA(dc, 10, 8 + 18 * i, linea[i], strlen(linea[i]));
    sprintf(b, "Clics: %d  ", clics);
    TextOutA(dc, 10, 8 + 18 * NL, b, strlen(b));
    EndPaint(v, &ps);
    return 0; }
  case WM_LBUTTONDOWN:
    clics++;
    if (!abierto) abierto = CreateFileA("A:\\WINCRT.ABI", GENERIC_WRITE, 0, 0, 2, 0x80, 0);  /* (se queda abierto) */
    InvalidateRect(v, 0, 0);
    return 0;
  case WM_CHAR:
    if (w == 's') { u32 t = GetTickCount(); Sleep(1500); clics += (GetTickCount() - t >= 1400) ? 100 : 500; InvalidateRect(v, 0, 0); }
    if (w == 'x') exit(3);                       /* (por MSVCRT: ExitProcess) */
    if (w == 'n') *(char *)HeapAlloc(GetProcessHeap(), 0, 3000000) = 1;   /* (memoria que no se suelta) */
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
  HANDLE v;
#ifdef SINDLL
  NoEsta(1);
#endif
  memset(&c, 0, sizeof c);
  c.lpfnWndProc = proc;
  c.hInstance = GetModuleHandleA(0);
  c.hbrBackground = (HANDLE)(COLOR_WINDOW + 1);
  c.lpszClassName = "HispaCrt";
  RegisterClassA(&c);
  v = CreateWindowExA(0, "HispaCrt", "Win32 con MSVCRT", WS_OVERLAPPEDWINDOW, 40, 40, 520, 290, 0, 0, c.hInstance, 0);
  ShowWindow(v, SW_SHOW);
  while (GetMessageA(&m, 0, 0, 0)) { TranslateMessage(&m); DispatchMessageA(&m); }
  ExitProcess(m.wParam);
}
