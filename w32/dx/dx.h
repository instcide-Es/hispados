/* dx.h - lo común de las DLL de DirectX de Hispa-486 (fase 5,
 * docs/486/HISPA-486.md): WINMM.DLL, DDRAW.DLL, DSOUND.DLL y DINPUT.DLL.
 * Tipos, lo justo del API Win32, los GUID y los códigos de error; los
 * nombres y los números son los documentados del API (sala limpia).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HX_DX_H
#define HX_DX_H
typedef unsigned int u32;
typedef int i32;
typedef unsigned short u16;
typedef short i16;
typedef unsigned char u8;
typedef signed char i8;
typedef unsigned long long u64;
typedef long long i64;
typedef void *HANDLE;
typedef HANDLE HWND;
typedef i32 HRESULT;
#define WINAPI __attribute__((stdcall))
#define NULL ((void *)0)
typedef struct { u32 d1; u16 d2, d3; u8 d4[8]; } GUID;
typedef struct { int left, top, right, bottom; } RECT;
typedef struct { int x, y; } POINT;

/* KERNEL32 (HW32) */
HANDLE WINAPI GetProcessHeap(void);
void *WINAPI HeapAlloc(HANDLE, u32, u32);
void *WINAPI HeapReAlloc(HANDLE, u32, void *, u32);
int WINAPI HeapFree(HANDLE, u32, void *);
u32 WINAPI GetTickCount(void);
void WINAPI Sleep(u32);
HANDLE WINAPI CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
int WINAPI ReadFile(HANDLE, void *, u32, u32 *, void *);
int WINAPI CloseHandle(HANDLE);
u32 WINAPI GetFileSize(HANDLE, u32 *);
HANDLE WINAPI FindResourceA(HANDLE, const char *, const char *);
HANDLE WINAPI LoadResource(HANDLE, HANDLE);
void *WINAPI LockResource(HANDLE);
u32 WINAPI SizeofResource(HANDLE, HANDLE);
HANDLE WINAPI GetModuleHandleA(const char *);
int WINAPI SetEvent(HANDLE);
/* USER32 y GDI32 (Hispa Ventanas) */
int WINAPI PostMessageA(HWND, u32, u32, u32);
int WINAPI GetClientRect(HWND, RECT *);
int WINAPI ClientToScreen(HWND, POINT *);
int WINAPI GetCursorPos(POINT *);
int WINAPI SetCursorPos(int, int);
HANDLE WINAPI GetDC(HWND);
int WINAPI ReleaseDC(HWND, HANDLE);
HANDLE WINAPI CreateCompatibleDC(HANDLE);
int WINAPI DeleteDC(HANDLE);
HANDLE WINAPI SelectObject(HANDLE, HANDLE);
int WINAPI DeleteObject(HANDLE);
HANDLE WINAPI CreateDIBSection(HANDLE, const void *, u32, void **, HANDLE, u32);
int WINAPI StretchDIBits(HANDLE, int, int, int, int, int, int, int, int, const void *, const void *, u32, u32);
int WINAPI SetDIBitsToDevice(HANDLE, int, int, u32, u32, int, int, u32, u32, const void *, const void *, u32);
HWND WINAPI GetActiveWindow(void);
/* las propias de Hispa Ventanas (ventanas/win32dx.inc) */
int WINAPI HispaDXPantalla(int, u32, u32, u32);
int WINAPI HispaDXSonido(int, u32, u32, u32);
int WINAPI HispaDXEntrada(int, u32, u32);
int WINAPI HispaDXTic(void *);
typedef struct { u32 ancho, alto, paso, bits, octetos; u8 *lfb; u8 masc[8]; } HXPANT;

/* lo de WINMM que usan las otras (exportado por WINMM.DLL) */
typedef struct HxVoz {
  const u8 *datos;      /* las muestras */
  u32 tramas;           /* cuántas tramas hay */
  int bits, canales;    /* 8 o 16; 1 o 2 */
  u32 pos, frac;        /* por dónde va: trama y fracción (16 bits) */
  u32 paso;             /* lo que avanza por trama de la mezcla (16.16) */
  int bucle;            /* 1: al acabar vuelve al principio */
  int vol_i, vol_d;     /* ganancia de cada lado: 0..4096 */
  int activa;
  void (*fin)(struct HxVoz *);  /* al acabar (sin bucle): puede dar otros datos */
  void *dueno;
  struct HxVoz *sig;
} HxVoz;
void WINAPI HispaMezPon(HxVoz *);
void WINAPI HispaMezQuita(HxVoz *);
void WINAPI HispaMezBomba(void);
u32 WINAPI HispaMezHz(void);
u32 WINAPI HispaMezPaso(u32 hz);
int WINAPI HispaMezGanancia(int mb);
u32 WINAPI timeGetTime(void);

/* los resultados */
#define S_OK 0
#define S_FALSE 1
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_POINTER ((HRESULT)0x80004003)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define CLASS_E_NOAGGREGATION ((HRESULT)0x80040110)

/* pequeñas ayudas (cada DLL lleva las suyas) */
static inline void *hx_pedir(u32 n) { return HeapAlloc(GetProcessHeap(), 8 /* HEAP_ZERO_MEMORY */, n ? n : 1); }
static inline void hx_soltar(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
static inline void hx_cero(void *d, u32 n) { u8 *p = d; while (n--) *p++ = 0; }
static inline void hx_copia(void *d, const void *s, u32 n) {
  u8 *a = d; const u8 *b = s;
  if (!((u32)a & 3) && !((u32)b & 3)) { while (n >= 4) { *(u32 *)a = *(const u32 *)b; a += 4; b += 4; n -= 4; } }
  while (n--) *a++ = *b++;
}
/* 64 entre 32 bits sin libgcc (el cociente ha de caber en 32 bits... o no:
 * se hace en dos pasos) */
static inline u64 hx_div64(u64 n, u32 d) {
  u32 hi = (u32)(n >> 32), lo = (u32)n, qh, ql, r;
  qh = hi / d; r = hi % d;
  __asm__("divl %4" : "=a"(ql), "=d"(r) : "a"(lo), "d"(r), "rm"(d));
  return ((u64)qh << 32) | ql;
}
/* la traza: con SET HISPADX=TRAZA, cada llamada importante sale por el
 * puerto serie COM1 (en QEMU, por la consola de las pruebas) */
u32 WINAPI GetEnvironmentVariableA(const char *, char *, u32);
static int hx_traza_si = -1;
static inline void hx_serie(const char *s) {
  while (*s) { __asm__ volatile("outb %0, %1" :: "a"(*s), "Nd"((u16)0x3F8)); s++; }
}
static void hx_t(const char *s, u32 v) {
  char b[12]; int k;
  if (hx_traza_si < 0) { char e[16]; e[0] = 0; GetEnvironmentVariableA("HISPADX", e, sizeof e); hx_traza_si = e[0] == 'T' || e[0] == 't'; }
  if (!hx_traza_si) return;
  hx_serie("[dx] "); hx_serie(s); hx_serie(" ");
  for (k = 0; k < 8; k++) b[k] = "0123456789ABCDEF"[(v >> (28 - k * 4)) & 15];
  b[8] = '\r'; b[9] = '\n'; b[10] = 0;
  hx_serie(b);
}
static inline int hx_guid(const GUID *g, u32 d1) { return g && g->d1 == d1; }
#endif
