/* tests/gl/terceros/windows.h - lo justo de <windows.h> (y de las cabeceras de
 * C) para compilar con gcc -m32 y tools/elf2pe.py programas de OpenGL de
 * otros escritos para Windows (wglgears.c, de los demos de Mesa: ver
 * tests/thirdparty/fetch_gl.sh), contra OPENGL32.DLL de HispaGL y la
 * MSVCRT.DLL de Hispa-486. Nombres y números: los documentados del API.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef HGL_TERCEROS_WINDOWS_H
#define HGL_TERCEROS_WINDOWS_H
#define WINAPI __attribute__((stdcall))
#define APIENTRY WINAPI
#define CALLBACK WINAPI
#define WINVER 0x0400
#include "GL/gl.h"

typedef unsigned int DWORD, UINT;
typedef int BOOL, LONG;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned int WPARAM;
typedef long LPARAM, LRESULT;
typedef void *HANDLE, *HWND, *HDC, *HGLRC, *HINSTANCE, *HICON, *HCURSOR, *HBRUSH, *HMENU, *HMODULE;
typedef const char *LPCSTR;
typedef char *LPSTR;
#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL ((void *)0)
#endif
#define LOWORD(l) ((WORD)((DWORD)(l) & 0xFFFF))
#define HIWORD(l) ((WORD)((DWORD)(l) >> 16))
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG x, y; } POINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; } MSG;
typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef struct { UINT style; WNDPROC lpfnWndProc; int cbClsExtra, cbWndExtra; HINSTANCE hInstance; HICON hIcon; HCURSOR hCursor;
	HBRUSH hbrBackground; LPCSTR lpszMenuName, lpszClassName; } WNDCLASS;
typedef struct { WORD nSize, nVersion; DWORD dwFlags; BYTE iPixelType, cColorBits, cRedBits, cRedShift, cGreenBits, cGreenShift,
	cBlueBits, cBlueShift, cAlphaBits, cAlphaShift, cAccumBits, cAccumRedBits, cAccumGreenBits, cAccumBlueBits, cAccumAlphaBits,
	cDepthBits, cStencilBits, cAuxBuffers, iLayerType, bReserved; DWORD dwLayerMask, dwVisibleMask, dwDamageMask; } PIXELFORMATDESCRIPTOR;
typedef struct { UINT wPeriodMin, wPeriodMax; } TIMECAPS;
typedef struct { char dmDeviceName[32]; WORD dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; DWORD dmFields;
	char otros[60]; WORD dmLogPixels; DWORD dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
	DWORD resto[8]; } DEVMODE;                  /* (156 octetos: el DEVMODEA de Windows 95) */
#define DM_BITSPERPEL 0x00040000
#define DM_PELSWIDTH 0x00080000
#define DM_PELSHEIGHT 0x00100000
#define CDS_FULLSCREEN 4
LONG WINAPI ChangeDisplaySettingsA(DEVMODE *, DWORD);
#define ChangeDisplaySettings ChangeDisplaySettingsA
HWND WINAPI SetFocus(HWND);
BOOL WINAPI SetForegroundWindow(HWND);
void *memset(void *, int, unsigned int);

#define CS_VREDRAW 1
#define CS_HREDRAW 2
#define CS_OWNDC 0x20
#define WS_OVERLAPPEDWINDOW 0x00CF0000
#define WS_POPUP 0x80000000
#define WS_CLIPSIBLINGS 0x04000000
#define WS_CLIPCHILDREN 0x02000000
#define WS_EX_APPWINDOW 0x00040000
#define WS_EX_WINDOWEDGE 0x00000100
#define CW_USEDEFAULT ((int)0x80000000)
#define SW_SHOW 5
#define WM_SIZE 0x0005
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_KEYDOWN 0x0100
#define WM_NCCREATE 0x0081
#define VK_ESCAPE 0x1B
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define PM_REMOVE 1
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define IDI_WINLOGO ((LPCSTR)32517)
#define IDC_ARROW ((LPCSTR)32512)
#define PFD_DOUBLEBUFFER 1
#define PFD_DRAW_TO_WINDOW 4
#define PFD_SUPPORT_OPENGL 0x20
#define PFD_TYPE_RGBA 0
#define PFD_MAIN_PLANE 0

HMODULE WINAPI GetModuleHandleA(LPCSTR);
HICON WINAPI LoadIconA(HINSTANCE, LPCSTR);
HCURSOR WINAPI LoadCursorA(HINSTANCE, LPCSTR);
WORD WINAPI RegisterClassA(const WNDCLASS *);
BOOL WINAPI AdjustWindowRectEx(RECT *, DWORD, BOOL, DWORD);
HWND WINAPI CreateWindowExA(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, void *);
BOOL WINAPI ShowWindow(HWND, int);
HDC WINAPI GetDC(HWND);
int WINAPI ReleaseDC(HWND, HDC);
BOOL WINAPI GetClientRect(HWND, RECT *);
int WINAPI GetSystemMetrics(int);
BOOL WINAPI PeekMessageA(MSG *, HWND, UINT, UINT, UINT);
BOOL WINAPI TranslateMessage(const MSG *);
LRESULT WINAPI DispatchMessageA(const MSG *);
LRESULT WINAPI DefWindowProcA(HWND, UINT, WPARAM, LPARAM);
void WINAPI PostQuitMessage(int);
int WINAPI ChoosePixelFormat(HDC, const PIXELFORMATDESCRIPTOR *);
BOOL WINAPI SetPixelFormat(HDC, int, const PIXELFORMATDESCRIPTOR *);
BOOL WINAPI SwapBuffers(HDC);
HGLRC WINAPI wglCreateContext(HDC);
BOOL WINAPI wglMakeCurrent(HDC, HGLRC);
BOOL WINAPI wglDeleteContext(HGLRC);
HGLRC WINAPI wglGetCurrentContext(void);
DWORD WINAPI timeGetTime(void);
UINT WINAPI timeGetDevCaps(TIMECAPS *, UINT);
UINT WINAPI timeBeginPeriod(UINT);
UINT WINAPI timeEndPeriod(UINT);
#define GetModuleHandle GetModuleHandleA
#define LoadIcon LoadIconA
#define LoadCursor LoadCursorA
#define RegisterClass RegisterClassA
#define CreateWindowEx CreateWindowExA
#define PeekMessage PeekMessageA
#define DispatchMessage DispatchMessageA
#define DefWindowProc DefWindowProcA

/* lo de la biblioteca de C (MSVCRT.DLL) */
#define LONG_MAX 2147483647L
#define INT_MAX 2147483647
#define stdout ((void *)0)          /* (fflush(NULL): todos) */
int fflush(void *);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define assert(x) ((void)0)
int printf(const char *, ...);
void exit(int);
int strcmp(const char *, const char *);
long strtol(const char *, char **, int);
double strtod(const char *, char **);
int isdigit(int);
int tolower(int);
double sin(double);
double cos(double);
double sqrt(double);
double pow(double, double);
static inline float powf(float a, float b) { return (float)pow(a, b); }   /* (no está en MSVCRT: la de C++) */
#endif
