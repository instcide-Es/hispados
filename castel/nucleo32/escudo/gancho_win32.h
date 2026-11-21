/* gancho_win32.h - (ESCUDO) el gancho de WIN32.BIN (castel/nucleo32/win32,
 * rama pentium-win32): antes de cargar un .EXE de Windows, que lo mire Escudo.
 *
 * Se pone en win32/principal.c, en w32_principal(), justo antes de
 * «if (!cargar_exe(ruta_exe, &falta))», con la ruta tal como llega en los
 * argumentos (en la página 437, la del disco: «r»; o ruta_exe, que es la
 * misma pasada a la 1252: para Escudo vale si no lleva letras raras):
 *
 *     #include "../escudo/gancho_win32.h"
 *     ...
 *     if (escudo_antes(r))          // (ESCUDO)
 *             sys_salir(255);
 *
 * Lanza «ESCUDO /ANTES RUTA» y lo espera: Escudo mira el .EXE y, si
 * encuentra algo, avisa en su cuadro («A cuarentena», «No abrirlo», «Abrirlo
 * igual») y acaba con 1 si no se debe abrir. Sin Escudo instalado, con la
 * opción «Revisar los programas de Windows» quitada o si algo falla al
 * lanzarlo, da 0 y el programa se abre como siempre (Escudo no estorba si
 * no está).
 * Copyright (C) 2026 Carlos Sanz y contribuidores. GPL-2.0 o posterior. */
#ifndef ESCUDO_GANCHO_WIN32_H
#define ESCUDO_GANCHO_WIN32_H

static inline unsigned escudo_llamada(unsigned n, unsigned a, unsigned b, unsigned c)
{
	unsigned r;
	__asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
	return r;
}

/* 1: no abrirlo (Escudo ha encontrado algo y no se ha pedido abrirlo igual) */
static int escudo_antes(const char *ruta)
{
	static const char *const sitios[2] = { "\\HISPA\\ESCUDO.BIN", "\\ESCUDO.BIN" };
	char prog[32], args[300];
	unsigned u = escudo_llamada(42, 0, 0, 0), t, m, i, k, j;	/* 42: la unidad de trabajo */
	for (k = 0; k < 2; k++) {
		const char *s = sitios[k];
		prog[0] = (char)(u >= 'a' && u <= 'z' ? u - 32 : u >= 'A' && u <= 'Z' ? u : 'C');
		prog[1] = ':';
		for (i = 0; s[i] && i + 3 < sizeof prog; i++)
			prog[2 + i] = s[i];
		prog[2 + i] = 0;
		m = escudo_llamada(7, (unsigned)prog, 0, 0);		/* 7: abrir */
		if (m >= 0xFFFFFFF0u)
			continue;
		escudo_llamada(9, m, 0, 0);				/* 9: cerrar */
		j = 0;
		for (s = "/ANTES "; *s; s++)
			args[j++] = *s;
		for (s = ruta; *s && j + 1 < sizeof args; s++)
			args[j++] = *s;
		args[j] = 0;
		t = escudo_llamada(10, (unsigned)prog, (unsigned)args, 0);	/* 10: lanzar */
		if (t >= 0xFFFFFFF0u)
			return 0;
		return escudo_llamada(11, t, 0, 0) == 1;		/* 11: esperar: su código */
	}
	return 0;
}
#endif
