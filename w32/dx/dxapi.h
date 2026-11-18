/* dxapi.h - lo justo de DirectX 7 (sin Direct3D) y WINMM para los programas
 * Hispa32 hechos con gcc -m32 y tools/elf2pe.py (la prueba de la fase 5 y el
 * juego de ejemplo): estructuras, constantes, los GUID y las llamadas a los
 * métodos COM por su número en la tabla. Nombres y números: los documentados.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HX_DXAPI_H
#define HX_DXAPI_H
typedef int HRESULT;
typedef unsigned char u8;
int WINAPI PeekMessageA(MSG *, HANDLE, u32, u32, u32);
typedef struct { u32 d1; u16 d2, d3; u8 d4[8]; } GUID;
/* un método COM: el número en la tabla, y lo demás */
#define COM(o, n, T) ((T)((*(void ***)(o))[n]))
typedef HRESULT WINAPI (*M0)(void *);
typedef HRESULT WINAPI (*M1)(void *, u32);
typedef HRESULT WINAPI (*M2)(void *, u32, u32);
typedef HRESULT WINAPI (*M3)(void *, u32, u32, u32);
typedef HRESULT WINAPI (*M4)(void *, u32, u32, u32, u32);
typedef HRESULT WINAPI (*M5)(void *, u32, u32, u32, u32, u32);
typedef HRESULT WINAPI (*M7)(void *, u32, u32, u32, u32, u32, u32, u32);
#define C0(o, n) COM(o, n, M0)(o)
#define C1(o, n, a) COM(o, n, M1)(o, (u32)(a))
#define C2(o, n, a, b) COM(o, n, M2)(o, (u32)(a), (u32)(b))
#define C3(o, n, a, b, c) COM(o, n, M3)(o, (u32)(a), (u32)(b), (u32)(c))
#define C4(o, n, a, b, c, d) COM(o, n, M4)(o, (u32)(a), (u32)(b), (u32)(c), (u32)(d))
#define C5(o, n, a, b, c, d, e) COM(o, n, M5)(o, (u32)(a), (u32)(b), (u32)(c), (u32)(d), (u32)(e))
#define C7(o, n, a, b, c, d, e, f, g) COM(o, n, M7)(o, (u32)(a), (u32)(b), (u32)(c), (u32)(d), (u32)(e), (u32)(f), (u32)(g))
#define Release(o) C0(o, 2)
/* IDirectDraw */
#define DD_CreatePalette(o, f, e, p) C4(o, 5, f, e, p, 0)
#define DD_CreateSurface(o, d, p) C3(o, 6, d, p, 0)
#define DD_EnumDisplayModes(o, f, d, x, cb) C4(o, 8, f, d, x, cb)
#define DD_RestoreDisplayMode(o) C0(o, 19)
#define DD_SetCooperativeLevel(o, h, f) C2(o, 20, h, f)
#define DD_SetDisplayMode(o, w, h, b) C3(o, 21, w, h, b)
#define DD_WaitForVerticalBlank(o, f) C2(o, 22, f, 0)
/* IDirectDrawSurface */
#define S_Blt(o, dr, s, sr, f, fx) C5(o, 5, dr, s, sr, f, fx)
#define S_BltFast(o, x, y, s, sr, t) C5(o, 7, x, y, s, sr, t)
#define S_Flip(o, s, f) C2(o, 11, s, f)
#define S_GetAttachedSurface(o, c, p) C2(o, 12, c, p)
#define S_GetDC(o, p) C1(o, 17, p)
#define S_GetSurfaceDesc(o, d) C1(o, 22, d)
#define S_IsLost(o) C0(o, 24)
#define S_Lock(o, r, d, f) C4(o, 25, r, d, f, 0)
#define S_ReleaseDC(o, h) C1(o, 26, h)
#define S_Restore(o) C0(o, 27)
#define S_SetColorKey(o, f, k) C2(o, 29, f, k)
#define S_SetPalette(o, p) C1(o, 31, p)
#define S_Unlock(o, p) C1(o, 32, p)
#define P_SetEntries(o, f, a, n, e) C4(o, 6, f, a, n, e)
/* IDirectSound e IDirectSoundBuffer */
#define DS_CreateSoundBuffer(o, d, p) C3(o, 3, d, p, 0)
#define DS_SetCooperativeLevel(o, h, n) C2(o, 6, h, n)
#define B_GetCurrentPosition(o, a, b) C2(o, 4, a, b)
#define B_GetStatus(o, s) C1(o, 9, s)
#define B_Lock(o, off, n, p1, n1, p2, n2, f) C7(o, 11, off, n, p1, n1, p2, n2, f)
#define B_Play(o, f) C3(o, 12, 0, 0, f)
#define B_SetCurrentPosition(o, p) C1(o, 13, p)
#define B_SetFormat(o, f) C1(o, 14, f)
#define B_SetVolume(o, v) C1(o, 15, v)
#define B_SetPan(o, v) C1(o, 16, v)
#define B_SetFrequency(o, v) C1(o, 17, v)
#define B_Stop(o) C0(o, 18)
#define B_Unlock(o, p1, n1, p2, n2) C4(o, 19, p1, n1, p2, n2)
/* IDirectInput e IDirectInputDevice */
#define DI_CreateDevice(o, g, p) C3(o, 3, g, p, 0)
#define DV_SetProperty(o, g, h) C2(o, 6, g, h)
#define DV_Acquire(o) C0(o, 7)
#define DV_Unacquire(o) C0(o, 8)
#define DV_GetDeviceState(o, n, p) C2(o, 9, n, p)
#define DV_GetDeviceData(o, t, d, n, f) C4(o, 10, t, d, n, f)
#define DV_SetDataFormat(o, f) C1(o, 11, f)
#define DV_SetCooperativeLevel(o, h, f) C2(o, 13, h, f)

typedef struct { u32 tam, flags, fourcc, bits, r, g, b, a; } DDPIXELFORMAT;
typedef struct { u32 lo, hi; } DDCOLORKEY;
typedef struct {
  u32 tam, flags, alto, ancho; int paso; u32 traseros, refresco, alfa, res; void *mem;
  DDCOLORKEY ck_dov, ck_dblt, ck_sov, ck_sblt;
  DDPIXELFORMAT pf;
  u32 caps;
} DDSURFACEDESC;
typedef struct { u32 tam, ddfx, rop, ddrop, giro, zop, zlo, zhi, zbase, zbits, zdst, zsbits, zsrc,
  abits, aedge, res, adbits, adst, asbits, asrc, relleno; DDCOLORKEY ck_dst, ck_src; } DDBLTFX;
#define DDSD_CAPS 1
#define DDSD_HEIGHT 2
#define DDSD_WIDTH 4
#define DDSD_BACKBUFFERCOUNT 0x20
#define DDSD_PIXELFORMAT 0x1000
#define DDSCAPS_BACKBUFFER 4
#define DDSCAPS_COMPLEX 8
#define DDSCAPS_FLIP 0x10
#define DDSCAPS_OFFSCREENPLAIN 0x40
#define DDSCAPS_PRIMARYSURFACE 0x200
#define DDSCAPS_SYSTEMMEMORY 0x800
#define DDSCL_FULLSCREEN 1
#define DDSCL_NORMAL 8
#define DDSCL_EXCLUSIVE 0x10
#define DDPCAPS_8BIT 4
#define DDPCAPS_ALLOW256 0x40
#define DDBLT_COLORFILL 0x400
#define DDBLT_KEYSRC 0x8000
#define DDBLT_WAIT 0x1000000
#define DDBLTFAST_SRCCOLORKEY 1
#define DDBLTFAST_WAIT 0x10
#define DDCKEY_SRCBLT 8
#define DDFLIP_WAIT 1
#define DDLOCK_WAIT 1
#define DDWAITVB_BLOCKBEGIN 1
#define DDERR_SURFACELOST ((HRESULT)0x887601C2)

typedef struct { u16 fmt, canales; u32 hz, octps; u16 bloque, bits; u16 extra; } WAVEFORMATEX;
typedef struct { u32 tam, flags, bytes, res; WAVEFORMATEX *fmt; } DSBUFFERDESC;
#define DSSCL_NORMAL 1
#define DSSCL_PRIORITY 2
#define DSBCAPS_PRIMARYBUFFER 1
#define DSBCAPS_STATIC 2
#define DSBCAPS_CTRLFREQUENCY 0x20
#define DSBCAPS_CTRLPAN 0x40
#define DSBCAPS_CTRLVOLUME 0x80
#define DSBPLAY_LOOPING 1
#define DSBSTATUS_PLAYING 1

typedef struct { const GUID *g; u32 ofs, tipo, flags; } DIOBJECTDATAFORMAT;
typedef struct { u32 tam, tam_obj, flags, tam_datos, nobj; DIOBJECTDATAFORMAT *obj; } DIDATAFORMAT;
typedef struct { u32 ofs, dato, hora, seq; } DIDEVICEOBJECTDATA;
typedef struct { u32 tam, cab, obj, como, dato; } DIPROPDWORD;
#define DISCL_EXCLUSIVE 1
#define DISCL_NONEXCLUSIVE 2
#define DISCL_FOREGROUND 4
#define DISCL_BACKGROUND 8
#define DIK_ESCAPE 0x01
#define DIK_SPACE 0x39
#define DIK_RETURN 0x1C
#define DIK_LCONTROL 0x1D
#define DIK_UP 0xC8
#define DIK_DOWN 0xD0
#define DIK_LEFT 0xCB
#define DIK_RIGHT 0xCD

static const GUID GUID_SysKeyboard = { 0x6F1D2B61, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0, 0 } };
static const GUID GUID_SysMouse = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0, 0 } };
static const GUID GUID_Key = { 0x55728220, 0xD33C, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0, 0 } };

HRESULT WINAPI DirectDrawCreate(GUID *, void **, void *);
HRESULT WINAPI DirectSoundCreate(GUID *, void **, void *);
HRESULT WINAPI DirectInputCreateA(HANDLE, u32, void **, void *);
u32 WINAPI timeGetTime(void);
u32 WINAPI timeBeginPeriod(u32);
u32 WINAPI timeEndPeriod(u32);
u32 WINAPI timeSetEvent(u32, u32, void *, u32, u32);
u32 WINAPI timeKillEvent(u32);
int WINAPI PlaySoundA(const char *, HANDLE, u32);
u32 WINAPI waveOutGetNumDevs(void);
u32 WINAPI joyGetNumDevs(void);
u32 WINAPI mciSendStringA(const char *, char *, u32, HANDLE);
u32 WINAPI waveOutOpen(HANDLE *, u32, const WAVEFORMATEX *, u32, u32, u32);
u32 WINAPI waveOutPrepareHeader(HANDLE, void *, u32);
u32 WINAPI waveOutUnprepareHeader(HANDLE, void *, u32);
u32 WINAPI waveOutWrite(HANDLE, void *, u32);
u32 WINAPI waveOutClose(HANDLE);
u32 WINAPI joyGetPos(u32, void *);
#endif
