/* ole32.c - OLE32.DLL mínima de Hispa-486 (fase 5, docs/486/HISPA-486.md):
 * lo que piden de COM los programas de la época que no usan objetos de
 * otros (los juegos de DirectX la importan por CoInitialize): CoInitialize,
 * CoInitializeEx, CoUninitialize, CoTaskMemAlloc/Realloc/Free y
 * CoCreateInstance, que dice que esa clase no está registrada (no hay
 * registro). DirectDraw, DirectSound y DirectInput se crean con sus
 * funciones (DirectDrawCreate...), que es lo que hacen casi todos.
 * Sala limpia: hecho a partir de la documentación pública del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "dx.h"
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154)
static int iniciado;
HRESULT WINAPI CoInitialize(void *r) { (void)r; return iniciado++ ? S_FALSE : S_OK; }
HRESULT WINAPI CoInitializeEx(void *r, u32 f) { (void)f; return CoInitialize(r); }
HRESULT WINAPI OleInitialize(void *r) { return CoInitialize(r); }
void WINAPI CoUninitialize(void) { if (iniciado) iniciado--; }
void WINAPI OleUninitialize(void) { CoUninitialize(); }
void *WINAPI CoTaskMemAlloc(u32 n) { return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
void *WINAPI CoTaskMemRealloc(void *p, u32 n) { return p ? HeapReAlloc(GetProcessHeap(), 0, p, n ? n : 1) : CoTaskMemAlloc(n); }
void WINAPI CoTaskMemFree(void *p) { if (p) HeapFree(GetProcessHeap(), 0, p); }
HRESULT WINAPI CoCreateInstance(const GUID *c, void *u, u32 ctx, const GUID *i, void **p) {
  (void)c; (void)u; (void)ctx; (void)i;
  if (p) *p = NULL;
  return REGDB_E_CLASSNOTREG;
}
HRESULT WINAPI CoGetClassObject(const GUID *c, u32 ctx, void *s, const GUID *i, void **p) { (void)c; (void)ctx; (void)s; (void)i; if (p) *p = NULL; return REGDB_E_CLASSNOTREG; }
HRESULT WINAPI CoCreateGuid(GUID *g) {
  static u32 n = 0x48495350;
  if (!g) return E_INVALIDARG;
  hx_cero(g, sizeof *g);
  g->d1 = n++ ^ GetTickCount(); g->d2 = 0x4853; g->d3 = 0x4100;
  return S_OK;
}
