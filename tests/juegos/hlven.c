/* HLVEN.EXE - (HL) prueba de lo de las ventanas que pedía Half-Life 1:
 * GetLastActivePopup (la ventanita que tiene encima la del juego),
 * ExcludeUpdateRgn (quitar de lo que se pinta lo que está por repintar) y
 * WSAAsyncGetHostByName (el aviso a la ventana, como los buscadores de
 * servidores). En el escritorio; lo que ve, en HLVEN.TXT, en su carpeta.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "hl.h"
HANDLE WINAPI WSAAsyncGetHostByName(HWND, u32, const char *, char *, int);
static FILE *sal;
#define DI(...) do { fprintf(sal, __VA_ARGS__); fflush(sal); } while (0)
static i32 WINAPI proc(HWND h, u32 m, u32 wp, i32 lp) { return DefWindowProcA(h, m, wp, lp); }

int main(void)
{
	WNDCLASSA wc;
	HWND w, p;
	HDC dc;
	RECT r = { 10, 10, 50, 40 }, c;
	MSG msg;
	int k, t;
	static char he[512];
	WSADATA wd;
	sal = fopen("C:\\JUEGOS\\HLVEN.TXT", "w");
	memset(&wc, 0, sizeof wc);
	wc.proc = proc;
	wc.inst = GetModuleHandleA(0);
	wc.clase = "HLVen";
	RegisterClassA(&wc);
	w = CreateWindowExA(0, "HLVen", "Juego", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 400, 300, 0, 0, wc.inst, 0);
	DI("ventana: %s\n", w ? "si" : "no");
	DI("lastactivepopup_sola: %s\n", GetLastActivePopup(w) == w ? "ella" : "MAL");
	p = CreateWindowExA(0, "HLVen", "Aviso", WS_POPUP | 0x00C00000 | WS_VISIBLE, 100, 100, 200, 100, w, 0, wc.inst, 0);
	for (k = 0; k < 10; k++) { while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg); Sleep(20); }
	DI("lastactivepopup: %s\n", GetLastActivePopup(w) == p ? "la de encima" : "MAL");
	DestroyWindow(p);
	ValidateRect(w, 0);
	InvalidateRect(w, &r, 0);
	dc = GetDC(w);
	t = ExcludeUpdateRgn(dc, w);
	GetClipBox(dc, &c);
	DI("excludeupdatergn: %d %s\n", t, t >= 2 ? "sin lo de repintar" : "MAL");
	ReleaseDC(w, dc);
	ValidateRect(w, 0);
	dc = GetDC(w);
	t = ExcludeUpdateRgn(dc, w);
	DI("excludeupdatergn_nada: %d\n", t);
	ReleaseDC(w, dc);
	WSAStartup(0x101, &wd);
	WSAAsyncGetHostByName(w, 0x8001, "10.0.2.15", he, sizeof he);
	for (k = 0, t = 0; k < 50 && !t; k++) {
		while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == 0x8001) {
				HOSTENT *x = (HOSTENT *)he;
				t = 1;
				DI("asyncgethostbyname: %u %u %s %u.%u.%u.%u\n", (u32)msg.lParam >> 16, (u32)msg.lParam & 0xFFFF, x->nombre,
				   (u8)x->dirs[0][0], (u8)x->dirs[0][1], (u8)x->dirs[0][2], (u8)x->dirs[0][3]);
			}
			DispatchMessageA(&msg);
		}
		Sleep(20);
	}
	if (!t)
		DI("asyncgethostbyname: sin aviso\n");
	DestroyWindow(w);
	DI("FIN HLVEN\n");
	fclose(sal);
	return 0;
}
