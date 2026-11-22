/* FALTA.EXE - prueba del diagnóstico de WIN32.BIN (JUEGOS): un programa que
 * importa funciones que Hispa-Pentium no tiene (KERNEL32!FuncionQueNoHay,
 * WINMM.DLL!mixerNoHay), pide otra por GetProcAddress y, al final, llama a
 * la que falta: WIN32 tiene que decir cuál, apuntarlo y acabar.
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
void WINAPI FuncionQueNoHay(int);
u32 WINAPI mixerNoHay(void);
int main(int argc, char **argv)
{
	void *p = GetProcAddress(GetModuleHandleA("KERNEL32.DLL"), "OtraQueNoHay");
	printf("getprocaddress_nohay: %s\n", p ? "MAL" : "nulo");
	if (argc > 1 && argv[1][0] == 'M')
		mixerNoHay();
	printf("antes de llamar a la que falta\n");
	fflush(stdout);
	FuncionQueNoHay(5);
	printf("MAL: ha vuelto\n");
	return 0;
}
