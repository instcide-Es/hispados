/* JUEGO.EXE, el «juego de mentira» JUEGO99 (HL): imita la estructura de
 * carpetas de un juego de 1999 (el .EXE en la raíz con su VGUI.DLL; las DLL
 * del mod en MOD\CL_DLLS y MOD\DLLS, importando Winsock por número) e
 * importa lo que pedía Half-Life 1 y Hispa-Pentium no tenía. WIN32
 * COMPRUEBA tiene que decir «está todo». Y al correr: carga las del mod por
 * su ruta relativa (desde otra carpeta de trabajo), y la del cliente
 * encuentra VGUI.DLL en la carpeta del .EXE.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "../hl.h"
int WINAPI VguiPanel(int);
/* (lo que importa sin llamarlo aquí, como un juego: que esté en su tabla) */
void *volatile importa[] = { (void *)ScaleWindowExtEx, (void *)ScaleViewportExtEx, (void *)Escape, (void *)GetBoundsRect,
	(void *)DocumentPropertiesA, (void *)GetLastActivePopup, (void *)ExcludeUpdateRgn, (void *)mmioOpenA, (void *)mmioDescend,
	(void *)mixerGetLineInfoA, (void *)mciGetDeviceIDA, (void *)waveInOpen, (void *)midiOutOpen, (void *)auxGetNumDevs,
	(void *)AdjustTokenPrivileges, (void *)LookupPrivilegeValueA, (void *)GetSecurityDescriptorDacl, (void *)IsValidSid,
	(void *)DdeInitializeA, (void *)UnpackDDElParam, (void *)ReuseDDElParam, (void *)SetEnvironmentVariableW,
	(void *)OpenFileMappingA, (void *)SetVolumeLabelA, (void *)GetProcessVersion, (void *)FatalAppExitA,
	(void *)EnumSystemLocalesA, (void *)_lcreat, (void *)_lwrite, (void *)_llseek, (void *)WaitForInputIdle, (void *)ExitWindowsEx };
int main(void)
{
	WSADATA w;
	OFSTRUCT of;
	HANDLE mx = 0;
	SIZE s;
	HMODULE cl, sv;
	int r;
	r = WSAStartup(0x101, &w);
	printf("juego_wsastartup: %d %04x %08x\n", r, htons(0x1b6c), inet_addr("10.0.2.2"));
	printf("juego_vgui: %d\n", VguiPanel(20));
	printf("juego_openfile: %d\n", OpenFile("NOHAY.CFG", &of, 0x4000) == (HANDLE)-1 ? -1 : 0);
	printf("juego_mixer: %u\n", mixerOpen(&mx, 0, 0, 0, 0));
	mixerClose(mx);
	SetCurrentDirectoryA("C:\\");
	cl = LoadLibraryA("MOD\\CL_DLLS\\CLIENTE.DLL");
	sv = LoadLibraryA("MOD\\DLLS\\SERVIDOR.DLL");
	{
		int (WINAPI *ci)(int) = cl ? (int (WINAPI *)(int))GetProcAddress(cl, "ClienteIniciar") : 0;
		int (WINAPI *si)(void) = sv ? (int (WINAPI *)(void))GetProcAddress(sv, "ServidorIniciar") : 0;
		printf("juego_cliente: %s %d\n", cl ? "cargada" : "no", ci ? ci(5) : -1);
		printf("juego_servidor: %s %d\n", sv ? "cargada" : "no", si ? si() : -1);
	}
	WSACleanup();
	printf("FIN JUEGO\n");
	return 0;
}
