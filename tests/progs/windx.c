/* windx.c - prueba de la fase 5 de Hispa-486 (docs/486/HISPA-486.md):
 * DirectDraw, DirectSound, DirectInput y WINMM (w32/dx/) dentro de Hispa
 * Ventanas. La maneja tests/run_dx_tests.sh:
 *  A. pantalla completa en 320×240 a 8 bits con paleta propia y un búfer
 *     trasero: fondo azul, un cuarto rojo (Blt con color), un dibujo de 32×32
 *     con clave de color (BltFast) y el mismo estirado al doble (Blt con
 *     DDBLT_KEYSRC); suena un la (440 Hz) en bucle por DirectSound. Sigue al
 *     pulsar el espacio (DirectInput por búfer);
 *  B. 640×480 a 16 bits (5-6-5): rojo arriba a la izquierda, verde abajo a la
 *     derecha, un degradado con Lock y un texto con GetDC; el sonido sigue
 *     con otro tono (SetFrequency). Espacio otra vez;
 *  C. se devuelve la pantalla (RestoreDisplayMode) y la ventana dice cuántas
 *     comprobaciones de valores han salido bien. Esc cierra.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior.
 * Se compila con gcc -m32 y tools/elf2pe.py --gui (ver el Makefile). */
#include "win32min.h"
#include "win32gdi.h"
#include "../../w32/dx/dxapi.h"
void *memset(void *, int, u32);
void *memcpy(void *, const void *, u32);
double sin(double);

static int bien, hechas;
static char fallos[96];
static void comprueba(int n, int ok) {
  hechas++;
  if (ok) { bien++; return; }
  if (lstrlenA(fallos) < 80) { char t[8]; wsprintfA(t, " %d", n); lstrcatA(fallos, t); }
}

static HANDLE ven;
static void *dd, *prim, *atras, *spr, *pal, *ds, *sbuf, *di, *tec;
static int fase;                    /* 0 A, 1 B, 2 C */
static int tics;
static void WINAPI al_tic(u32 id, u32 m, u32 dato, u32 a, u32 b) { (void)id; (void)m; (void)dato; (void)a; (void)b; tics++; }
static int hechos;
static void WINAPI al_wave(HANDLE h, u32 m, u32 inst, u32 a, u32 b) { (void)h; (void)inst; (void)a; (void)b; if (m == 0x3BD) hechos++; }

static void bombea(u32 ms) {        /* los mensajes durante un rato */
  MSG msg;
  u32 t = timeGetTime();
  while (timeGetTime() - t < ms) {
    while (PeekMessageA(&msg, NULL, 0, 0, 1)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    Sleep(10);
  }
}

static DIOBJECTDATAFORMAT obj_tec[256];
static DIDATAFORMAT df_tec = { 24, 16, 2, 256, 256, obj_tec };
static int espacio(void) {          /* ¿se ha pulsado el espacio? (por el búfer) */
  DIDEVICEOBJECTDATA e[8];
  u32 n = 8, k;
  int si = 0;
  if (!tec) return 0;
  if (DV_GetDeviceData(tec, sizeof(DIDEVICEOBJECTDATA), e, &n, 0) < 0) return 0;
  for (k = 0; k < n; k++) if (e[k].ofs == DIK_SPACE && (e[k].dato & 0x80)) si = 1;
  return si;
}

static void sonido(void) {
  WAVEFORMATEX f = { 1, 1, 22050, 44100, 2, 16, 0 };
  DSBUFFERDESC d = { 20, DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLPAN, 22050 * 2, 0, &f };
  short *p1, *p2; u32 n1, n2, k, a, b;
  comprueba(12, DirectSoundCreate(NULL, &ds, NULL) == 0);
  if (!ds) return;
  DS_SetCooperativeLevel(ds, ven, DSSCL_NORMAL);
  comprueba(13, DS_CreateSoundBuffer(ds, &d, &sbuf) == 0);
  if (!sbuf) return;
  if (B_Lock(sbuf, 0, 22050 * 2, &p1, &n1, &p2, &n2, 0) == 0) {
    for (k = 0; k < n1 / 2; k++) p1[k] = (short)(12000 * sin(k * 2 * 3.14159265358979 * 440 / 22050));
    B_Unlock(sbuf, p1, n1, p2, n2);
  }
  B_Play(sbuf, DSBPLAY_LOOPING);
  B_GetCurrentPosition(sbuf, &a, NULL);
  bombea(300);
  B_GetCurrentPosition(sbuf, &b, NULL);
  { u32 s = 0; B_GetStatus(sbuf, &s); comprueba(14, a != b && (s & DSBSTATUS_PLAYING)); }
}

static void entrada(void) {
  u32 k;
  DIPROPDWORD p = { 20, 16, 0, 0, 32 };
  for (k = 0; k < 256; k++) { obj_tec[k].g = &GUID_Key; obj_tec[k].ofs = k; obj_tec[k].tipo = 0x8000000C | (k << 8); }
  comprueba(15, DirectInputCreateA(GetModuleHandleA(NULL), 0x0700, &di, NULL) == 0 &&
                DI_CreateDevice(di, &GUID_SysKeyboard, &tec) == 0 &&
                DV_SetDataFormat(tec, &df_tec) == 0 &&
                DV_SetCooperativeLevel(tec, ven, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND) == 0 &&
                DV_SetProperty(tec, 1, &p) == 0 &&
                DV_Acquire(tec) == 0);
}

/* fase A: 320×240×8 */
static u8 paleta[256][4];
static void fase_a(void) {
  DDSURFACEDESC d;
  u32 caps[4] = { DDSCAPS_BACKBUFFER, 0, 0, 0 };
  DDBLTFX fx;
  RECT r;
  DDCOLORKEY k = { 6, 6 };
  int x, y;
  comprueba(4, DirectDrawCreate(NULL, &dd, NULL) == 0);
  if (!dd) return;
  comprueba(5, DD_SetCooperativeLevel(dd, ven, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE) == 0);
  comprueba(6, DD_SetDisplayMode(dd, 320, 240, 8) == 0);
  memset(&d, 0, sizeof d);
  d.tam = sizeof d; d.flags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT; d.traseros = 1;
  d.caps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
  comprueba(7, DD_CreateSurface(dd, &d, &prim) == 0);
  if (!prim) return;
  comprueba(8, S_GetAttachedSurface(prim, caps, &atras) == 0);
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  comprueba(9, atras && S_GetSurfaceDesc(atras, &d) == 0 && d.ancho == 320 && d.alto == 240 && d.pf.bits == 8);
  memset(paleta, 0, sizeof paleta);
  paleta[1][0] = 255;                                   /* 1 rojo */
  paleta[2][1] = 255;                                   /* 2 verde */
  paleta[3][2] = 255;                                   /* 3 azul */
  paleta[4][0] = paleta[4][1] = 255;                    /* 4 amarillo */
  paleta[5][0] = paleta[5][1] = paleta[5][2] = 255;     /* 5 blanco */
  paleta[6][0] = paleta[6][2] = 255;                    /* 6 magenta (la clave) */
  DD_CreatePalette(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, paleta, &pal);
  S_SetPalette(prim, pal);
  memset(&d, 0, sizeof d);
  d.tam = sizeof d; d.flags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; d.ancho = 32; d.alto = 32;
  d.caps = DDSCAPS_OFFSCREENPLAIN;
  DD_CreateSurface(dd, &d, &spr);
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  comprueba(10, spr && S_Lock(spr, NULL, &d, DDLOCK_WAIT) == 0 && d.paso >= 32);
  if (d.mem) {
    for (y = 0; y < 32; y++) for (x = 0; x < 32; x++)
      ((u8 *)d.mem)[y * d.paso + x] = (x >= 8 && x < 24 && y >= 8 && y < 24) ? 4 : 6;
    S_Unlock(spr, NULL);
  }
  S_SetColorKey(spr, DDCKEY_SRCBLT, &k);
  comprueba(11, GetSystemMetrics(0) == 320 && GetSystemMetrics(1) == 240);
  memset(&fx, 0, sizeof fx); fx.tam = sizeof fx;
  fx.relleno = 3;
  S_Blt(atras, NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
  r.left = 0; r.top = 0; r.right = 160; r.bottom = 120;
  fx.relleno = 1;
  S_Blt(atras, &r, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
  S_BltFast(atras, 200, 100, spr, NULL, DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT);
  r.left = 240; r.top = 160; r.right = 304; r.bottom = 224;
  S_Blt(atras, &r, spr, NULL, DDBLT_KEYSRC | DDBLT_WAIT, NULL);
  S_Flip(prim, NULL, DDFLIP_WAIT);
}

/* fase B: 640×480×16 */
static void fase_b(void) {
  DDSURFACEDESC d;
  u32 caps[4] = { DDSCAPS_BACKBUFFER, 0, 0, 0 };
  DDBLTFX fx;
  RECT r;
  HANDLE dc;
  int x, y, blancos = 0;
  if (!dd) return;
  if (spr) { Release(spr); spr = NULL; }
  if (prim) { Release(prim); prim = atras = NULL; }
  comprueba(17, DD_SetDisplayMode(dd, 640, 480, 16) == 0);
  memset(&d, 0, sizeof d);
  d.tam = sizeof d; d.flags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT; d.traseros = 1;
  d.caps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
  if (DD_CreateSurface(dd, &d, &prim) || S_GetAttachedSurface(prim, caps, &atras)) { comprueba(18, 0); return; }
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  S_GetSurfaceDesc(prim, &d);
  comprueba(18, d.pf.bits == 16 && d.pf.r == 0xF800 && d.pf.g == 0x07E0 && d.pf.b == 0x1F);
  memset(&fx, 0, sizeof fx); fx.tam = sizeof fx;
  fx.relleno = 0;
  S_Blt(atras, NULL, NULL, NULL, DDBLT_COLORFILL, &fx);
  r.left = 0; r.top = 0; r.right = 320; r.bottom = 240; fx.relleno = 0xF800;
  S_Blt(atras, &r, NULL, NULL, DDBLT_COLORFILL, &fx);
  r.left = 320; r.top = 240; r.right = 640; r.bottom = 480; fx.relleno = 0x07E0;
  S_Blt(atras, &r, NULL, NULL, DDBLT_COLORFILL, &fx);
  memset(&d, 0, sizeof d); d.tam = sizeof d;
  if (S_Lock(atras, NULL, &d, DDLOCK_WAIT) == 0) {           /* un degradado azul en la fila 300-319 */
    for (y = 300; y < 320; y++) for (x = 0; x < 320; x++) ((u16 *)((u8 *)d.mem + y * d.paso))[x] = x * 32 / 320;
    S_Unlock(atras, NULL);
  }
  if (S_GetDC(atras, &dc) == 0) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    TextOutA(dc, 16, 16, "Hispa-486", 9);
    S_ReleaseDC(atras, dc);
    memset(&d, 0, sizeof d); d.tam = sizeof d;
    if (S_Lock(atras, NULL, &d, DDLOCK_WAIT) == 0) {
      for (y = 16; y < 32; y++) for (x = 16; x < 88; x++) if (((u16 *)((u8 *)d.mem + y * d.paso))[x] == 0xFFFF) blancos++;
      S_Unlock(atras, NULL);
    }
    comprueba(19, blancos > 50);
  } else comprueba(19, 0);
  S_Flip(prim, NULL, DDFLIP_WAIT);
  if (sbuf) B_SetFrequency(sbuf, 33075);                  /* la quinta: 660 Hz */
}

static void fase_c(void) {
  static u8 wav[44 + 4000];
  HANDLE wo;
  WAVEFORMATEX f = { 1, 1, 8000, 8000, 1, 8, 0 };
  struct { u8 *d; u32 n, r, u, fl, vu; void *sig; u32 res; } h;
  u32 k;
  if (sbuf) { B_Stop(sbuf); Release(sbuf); sbuf = NULL; }
  if (prim) { Release(prim); prim = atras = NULL; }
  if (dd) { DD_RestoreDisplayMode(dd); Release(dd); dd = NULL; }
  comprueba(20, GetSystemMetrics(0) != 320 && GetSystemMetrics(0) != 640);
  bombea(1000);                     /* (un silencio: en la grabación, cada cosa aparte) */
  /* un WAV en memoria con PlaySound y otro trozo por waveOut */
  memcpy(wav, "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x01\0\x40\x1f\0\0\x40\x1f\0\0\x01\0\x08\0data", 40);
  *(u32 *)(wav + 4) = 36 + 4000; *(u32 *)(wav + 40) = 4000;
  for (k = 0; k < 4000; k++) wav[44 + k] = (k / 8) & 1 ? 0xA0 : 0x60;
  comprueba(21, PlaySoundA((const char *)wav, NULL, 4 | 1) == 1);   /* SND_MEMORY | SND_ASYNC */
  bombea(1200);
  if (waveOutOpen(&wo, 0xFFFFFFFF, &f, (u32)al_wave, 0, 0x30000) == 0) {
    memset(&h, 0, sizeof h); h.d = wav + 44; h.n = 1600;
    waveOutPrepareHeader(wo, &h, sizeof h);
    waveOutWrite(wo, &h, sizeof h);
    bombea(900);
    comprueba(22, hechos == 1 && (h.fl & 1));
    waveOutUnprepareHeader(wo, &h, sizeof h);
    waveOutClose(wo);
  } else comprueba(22, 0);
  { u32 j[4]; u32 r = joyGetPos(0, (void *)j); comprueba(23, r == 0 || r == 167); }
}

static int WINAPI proc(HANDLE w, u32 m, u32 wp, u32 lp) {
  if (m == WM_PAINT) {
    PAINTSTRUCT ps; char t[160];
    HANDLE dc = BeginPaint(w, &ps);
    if (fase == 2) {
      wsprintfA(t, "Comprobaciones: %d de %d bien.%s%s", bien, hechas, fallos[0] ? " Mal:" : "", fallos);
      SetTextColor(dc, RGB(0, 0, 0));
      TextOutA(dc, 20, 20, t, lstrlenA(t));
    }
    EndPaint(w, &ps);
    return 0;
  }
  if (m == WM_KEYDOWN && wp == 27 && fase == 2) { DestroyWindow(w); return 0; }
  if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcA(w, m, wp, lp);
}

void inicio(void) {
  WNDCLASSA c;
  MSG msg;
  u32 t0, t1, ev;
  memset(&c, 0, sizeof c);
  c.lpfnWndProc = proc;
  c.hInstance = GetModuleHandleA(NULL);
  c.hbrBackground = GetStockObject(WHITE_BRUSH);
  c.lpszClassName = "PruebaDX";
  RegisterClassA(&c);
  ven = CreateWindowExA(0, "PruebaDX", "DirectX de Hispa-486", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                        10, 30, 780, 300, NULL, NULL, c.hInstance, NULL);
  /* WINMM */
  t0 = timeGetTime(); Sleep(300); t1 = timeGetTime();
  comprueba(1, t1 - t0 >= 250 && t1 - t0 <= 700);
  ev = timeSetEvent(50, 1, al_tic, 0, 1);
  bombea(500);
  timeKillEvent(ev);
  comprueba(2, tics >= 4 && tics <= 14);
  comprueba(3, waveOutGetNumDevs() == 1);
  entrada();
  fase_a();
  sonido();
  for (;;) {                        /* las fases A y B: hasta el espacio */
    while (PeekMessageA(&msg, NULL, 0, 0, 1)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    if (espacio()) {
      comprueba(fase == 0 ? 16 : 24, 1);
      if (fase == 0) { fase = 1; fase_b(); }
      else break;
    }
    Sleep(20);
  }
  fase = 2;
  fase_c();
  InvalidateRect(ven, NULL, 1);
  while (GetMessageA(&msg, NULL, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageA(&msg); }
  ExitProcess(0);
}
