/* MOD\CL_DLLS\CLIENTE.DLL del «juego de mentira» JUEGO99 (HL): la parte
 * del cliente de un mod, en su subcarpeta. Importa VGUI.DLL (que está junto
 * al .EXE), WSOCK32 por número y WININET y WINMM por nombre, como la
 * client.dll de un juego de 1999.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../hl.h"
int WINAPI VguiPanel(int);
void *volatile importa_cl[] = { (void *)InternetOpenA, (void *)InternetOpenUrlA, (void *)InternetReadFile, (void *)InternetCloseHandle, (void *)HttpQueryInfoA };
int WINAPI ClienteIniciar(int x)
{
	u32 fl = 0;
	if (x < 0)
		return InternetGetConnectedState(&fl, 0) + (int)mixerGetNumDevs();
	return VguiPanel(x) + ntohs(htons(1000));
}
int WINAPI DllMain(void *h, unsigned r, void *x) { (void)h; (void)r; (void)x; return 1; }
