/* JUEGOV.EXE - prueba de las ventanas y la entrada de un juego (JUEGOS):
 * GetVersionEx, la hora fina (QueryPerformanceCounter, timeGetTime), la DLL
 * del juego por su ruta relativa a la carpeta del .EXE (LoadLibrary,
 * GetProcAddress, GetModuleFileName), una ventana WS_POPUP a pantalla
 * completa (DirectDraw: SetCooperativeLevel y SetDisplayMode a 640 x 480),
 * GetSystemMetrics y GetClientRect en el modo del juego, SetCursorPos y
 * GetCursorPos, ShowCursor, ClipCursor, el ratón relativo como lo hace el
 * motor de un juego (mirar con el ratón volviendo al centro en cada vuelta:
 * más de lo que mide la pantalla) y el de DirectInput (lo movido de verdad).
 * Lo que ve lo escribe en JUEGOV.TXT, en su carpeta (la prueba lo mira con
 * mtools). Hay que moverle el ratón (QEMU: mouse_move) en las dos tandas.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
typedef struct { u32 d1; u16 d2, d3; u8 d4[8]; } GUID;
typedef long HRESULT;
typedef struct { void **vt; } COM;
#define LLAMA(o, n, ...) ((HRESULT(WINAPI *)())((COM *)(o))->vt[n])((o), ##__VA_ARGS__)
HRESULT WINAPI DirectDrawCreate(void *, void **, void *);
HRESULT WINAPI DirectInputCreateA(HINSTANCE, u32, void **, void *);
static const GUID GUID_SysMouse = { 0x6F1D2B60, 0xD5A0, 0x11CF, { 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00 } };
typedef struct { const GUID *g; u32 ofs, tipo, banderas; } DIOBJ;
typedef struct { u32 tam, tam_obj, banderas, tam_datos, nobj; const DIOBJ *obj; } DIFMT;
static const DIOBJ objs_raton[] = {
	{ 0, 0, 0x00FFFF03, 0 }, { 0, 4, 0x00FFFF03, 0 }, { 0, 8, 0x80FFFF03, 0 },
	{ 0, 12, 0x00FFFF0C, 0 }, { 0, 13, 0x00FFFF0C, 0 }, { 0, 14, 0x80FFFF0C, 0 }, { 0, 15, 0x80FFFF0C, 0 },
};
static const DIFMT fmt_raton = { 24, 16, 2, 16, 7, objs_raton };   /* (como c_dfDIMouse: DIDF_RELAXIS) */

static FILE *sal;
#define DI(...) do { fprintf(sal, __VA_ARGS__); fflush(sal); } while (0)

static i32 WINAPI proc(HWND h, u32 m, u32 w, i32 l)
{
	if (m == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(h, m, w, l);
}
static void mensajes(void)
{
	MSG m;
	while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) {
		TranslateMessage(&m);
		DispatchMessageA(&m);
	}
}

int main(void)
{
	char b[260], exe[260];
	OSVERSIONINFOA v;
	u64 f = 0, c1 = 0, c2 = 0;
	u32 t1, t2;
	HMODULE dll;
	WNDCLASSA wc;
	HWND w;
	void *dd = 0, *di = 0, *raton = 0;
	RECT r;
	POINT p;
	GetModuleFileNameA(0, exe, sizeof exe);
	strcpy(b, exe);
	strcpy(strstr(b, "JUEGOV.EXE"), "JUEGOV.TXT");
	sal = fopen(b, "w");
	if (!sal)
		return 1;
	DI("exe: %s\n", strstr(exe, "\\JUEGOS\\JUEGOV.EXE") ? "C:\\JUEGOS\\JUEGOV.EXE" : exe);
	v.tam = sizeof v;
	GetVersionExA(&v);
	DI("version: %u.%u %u\n", v.may, v.men, v.plat);
	QueryPerformanceFrequency(&f);
	DI("timebeginperiod: %u\n", timeBeginPeriod(1));
	QueryPerformanceCounter(&c1);
	t1 = timeGetTime();
	Sleep(100);
	QueryPerformanceCounter(&c2);
	t2 = timeGetTime();
	DI("qpc: %s %s\n", f >= 1000000 ? "mhz" : "lenta", c2 > c1 ? "sube" : "MAL");
	{
		u32 ms = (u32)(c2 - c1) / ((u32)f / 1000);   /* (sin divisiones de 64 bits) */
		DI("sleep100: %s %s\n", ms >= 90 && ms < 200 ? "bien" : "mal", t2 - t1 >= 90 && t2 - t1 < 200 ? "bien" : "mal");
		DI("medidas: %u ms (qpc) %u ms (timeGetTime) a %u kHz\n", ms, t2 - t1, (u32)f / 1000);
	}
	/* la DLL del juego, desde otra carpeta de trabajo */
	SetCurrentDirectoryA("C:\\");
	dll = LoadLibraryA("SUB\\JDLL.DLL");
	{
		int (*suma)(int, int) = dll ? (int (*)(int, int))GetProcAddress(dll, "Suma") : 0;
		GetModuleFileNameA(dll, b, sizeof b);
		DI("loadlibrary_relativa: %s %d %s\n", dll ? "si" : "no", suma ? suma(2, 3) : -1, strstr(b, "JUEGOS\\SUB\\JDLL.DLL") ? "su ruta" : b);
	}
	DI("getprocaddress_nohay: %s\n", GetProcAddress(dll, "NoEsta") ? "MAL" : "nulo");
	/* la ventana del juego, a pantalla completa */
	memset(&wc, 0, sizeof wc);
	wc.proc = proc;
	wc.inst = GetModuleHandleA(0);
	wc.clase = "JuegoV";
	RegisterClassA(&wc);
	w = CreateWindowExA(0, "JuegoV", "Juego", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, 0, 0, wc.inst, 0);
	ShowWindow(w, 1);
	UpdateWindow(w);
	DirectDrawCreate(0, &dd, 0);
	{
		HRESULT e1 = dd ? LLAMA(dd, 20, w, 0x11) : -1, e2;
		e2 = dd ? LLAMA(dd, 21, 640, 480, 8) : -1;
		DI("ddraw: %s %ld %ld\n", dd ? "si" : "no", e1, e2);
	}
	mensajes();
	DI("pantalla: %d %d\n", GetSystemMetrics(0), GetSystemMetrics(1));
	GetClientRect(w, &r);
	DI("cliente: %d %d %d %d\n", r.left, r.top, r.right, r.bottom);
	SetCursorPos(320, 240);
	GetCursorPos(&p);
	DI("cursor_centro: %d %d\n", p.x, p.y);
	{
		int a = ShowCursor(0), b2 = ShowCursor(0);
		DI("showcursor: %d %d\n", a, b2);
	}
	ShowCursor(1);
	{
		RECT c = { 100, 100, 200, 200 };
		ClipCursor(&c);
		SetCursorPos(10, 10);
		GetCursorPos(&p);
		DI("clipcursor: %d %d\n", p.x, p.y);
		ClipCursor(0);
	}
	DI("asynckey: %d\n", GetAsyncKeyState(0x41) & 0x8000 ? 1 : 0);
	DI("LISTO tanda 1\n");
	/* tanda 1: el ratón como lo mira un motor de juegos */
	{
		i32 total = 0;
		u32 fin = GetTickCount() + 7000;
		SetCursorPos(320, 240);
		while (GetTickCount() < fin) {
			mensajes();
			GetCursorPos(&p);
			total += p.x - 320;
			SetCursorPos(320, 240);
			Sleep(10);
		}
		DI("raton_al_centro: %s %d\n", total > 640 ? "mas que la pantalla" : total > 0 ? "poco" : "nada", total);
	}
	/* tanda 2: DirectInput */
	DirectInputCreateA(wc.inst, 0x0500, &di, 0);
	if (di && LLAMA(di, 3, &GUID_SysMouse, &raton, 0) == 0 && raton) {
		i32 total = 0;
		u32 fin;
		HRESULT e1, e2, e3;
		e1 = LLAMA(raton, 11, &fmt_raton);
		e2 = LLAMA(raton, 13, w, 5);
		e3 = LLAMA(raton, 7);
		DI("dinput_raton: %ld %ld %ld\n", e1, e2, e3);
		DI("LISTO tanda 2\n");
		fin = GetTickCount() + 7000;
		while (GetTickCount() < fin) {
			i32 est[4];
			mensajes();
			memset(est, 0, sizeof est);
			if (LLAMA(raton, 9, 16, est) == 0)
				total += est[0];
			Sleep(10);
		}
		DI("dinput_relativo: %s %d\n", total > 800 ? "mas que la pantalla" : total > 0 ? "poco" : "nada", total);
	} else
		DI("dinput_raton: no\n");
	if (dd)
		LLAMA(dd, 19);
	DestroyWindow(w);
	DI("FIN JUEGOV\n");
	fclose(sal);
	return 0;
}
