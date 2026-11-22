/* SINDLL.EXE - prueba del diagnóstico (JUEGOS): necesita NOHAY.DLL, que no
 * está: WIN32 tiene que decirlo (y COMPRUEBA, también).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#include "j.h"
int WINAPI FuncionDeNoHay(int);
int main(void)
{
	printf("MAL: ha arrancado %d\n", FuncionDeNoHay(1));
	return 0;
}
